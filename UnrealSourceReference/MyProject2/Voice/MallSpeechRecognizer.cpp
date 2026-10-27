#include "Voice/MallSpeechRecognizer.h"

#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "HAL/Event.h"
#include "HAL/PlatformProcess.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Voice/MallVoicePhraseMatcher.h"
#include "Voice/MallWhisperBackend.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

namespace
{
// UE's GetDllHandle can return GetModuleHandle without increasing the Windows
// loader reference count. Such a borrowed handle must not be freed by each PIE
// world. MinGW also installs TLS callbacks that can run after a worker's Run()
// returns, so retaining DLLs merely until model destruction is insufficient.
// Pin all four native libraries for the process lifetime; models and workers
// retain their normal, bounded per-recognizer lifetimes.
FCriticalSection GMallVoskNativeLifecycleMutex;
struct FMallVoskProcessLibraries
{
	FString Directory;
	TArray<void*> PinnedHandles;
};
FMallVoskProcessLibraries GMallVoskProcessLibraries;

/** Caller holds GMallVoskNativeLifecycleMutex, including the DLL directory stack. */
bool GetPinnedVoskLibrary(const FString& Directory, void*& OutLibrary, FString& OutError)
{
#if PLATFORM_WINDOWS
	if (!GMallVoskProcessLibraries.Directory.IsEmpty()
		&& !FPaths::IsSamePath(GMallVoskProcessLibraries.Directory, Directory))
	{
		OutError = TEXT("Offline speech libraries are already loaded from another directory; restart the application to change them.");
		return false;
	}
	GMallVoskProcessLibraries.Directory = Directory;
	const TArray<FString> Names = {TEXT("libgcc_s_seh-1.dll"), TEXT("libstdc++-6.dll"),
		TEXT("libwinpthread-1.dll"), TEXT("libvosk.dll")};
	FPlatformProcess::PushDllDirectory(*Directory);
	bool bSuccess = true;
	for (int32 Index = GMallVoskProcessLibraries.PinnedHandles.Num(); Index < Names.Num(); ++Index)
	{
		void* Handle = FPlatformProcess::GetDllHandle(*FPaths::Combine(Directory, Names[Index]));
		if (!Handle)
		{
			OutError = FString::Printf(TEXT("Offline speech library could not load: %s"), *Names[Index]);
			bSuccess = false;
			break;
		}
		HMODULE PinnedHandle = nullptr;
		if (!::GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
			reinterpret_cast<LPCWSTR>(Handle), &PinnedHandle))
		{
			OutError = FString::Printf(TEXT("Offline speech library could not be retained safely: %s"), *Names[Index]);
			bSuccess = false;
			break;
		}
		GMallVoskProcessLibraries.PinnedHandles.Add(PinnedHandle);
	}
	FPlatformProcess::PopDllDirectory(*Directory);
	if (bSuccess)
	{
		OutLibrary = GMallVoskProcessLibraries.PinnedHandles.Last();
	}
	return bSuccess;
#else
	OutError = TEXT("The prototype offline speech backend currently supports Windows only.");
	return false;
#endif
}

struct FMallRecognitionLifetime
{
	TAtomic<bool> bAlive{true};
};

struct FMallQueuedUtterance
{
	TArray<int16> Samples;
	FMallSpeechRecognizer::FOnRecognized Callback;
};

struct FMallTranscript
{
	FString Text;
	double ConfidenceSum = 0.0;
	int32 WordCount = 0;
	bool bValid = true;

	void Append(const char* Json)
	{
		if (!Json)
		{
			bValid = false;
			return;
		}
		TSharedPtr<FJsonObject> Object;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(UTF8_TO_TCHAR(Json));
		if (!FJsonSerializer::Deserialize(Reader, Object) || !Object.IsValid())
		{
			bValid = false;
			return;
		}
		FString Segment;
		if (!Object->TryGetStringField(TEXT("text"), Segment))
		{
			bValid = false;
			return;
		}
		if (!Segment.IsEmpty())
		{
			if (!Text.IsEmpty())
			{
				Text += TEXT(" ");
			}
			Text += Segment;
		}
		const TArray<TSharedPtr<FJsonValue>>* Words = nullptr;
		if (Object->TryGetArrayField(TEXT("result"), Words))
		{
			for (const TSharedPtr<FJsonValue>& Word : *Words)
			{
				const TSharedPtr<FJsonObject>* WordObject = nullptr;
				double Confidence = 0.0;
				if (!Word.IsValid() || !Word->TryGetObject(WordObject) || !WordObject->IsValid()
					|| !(*WordObject)->TryGetNumberField(TEXT("conf"), Confidence)
					|| !FMath::IsFinite(Confidence) || Confidence < 0.0 || Confidence > 1.0)
				{
					bValid = false;
					continue;
				}
				ConfidenceSum += Confidence;
				++WordCount;
			}
		}
		else if (!Segment.IsEmpty())
		{
			bValid = false;
		}
	}
};
}

class FMallSpeechRecognizer::FImpl final : public FRunnable
{
public:
	FImpl(const FString& InDllDirectory, const FString& InModelDirectory,
		const TMap<FName, TArray<FString>>& InAliases, FOnInitialized InCallback, float InMinimumConfidence,
		bool bInUseWhisper = false)
		: DllDirectory(FPaths::ConvertRelativePathToFull(InDllDirectory))
		, ModelDirectory(FPaths::ConvertRelativePathToFull(InModelDirectory))
		, Aliases(InAliases)
		, InitializationCallback(MoveTemp(InCallback))
		, MinimumConfidence(FMath::Clamp(InMinimumConfidence, 0.0f, 1.0f))
		, bUseWhisper(bInUseWhisper)
		, Lifetime(MakeShared<FMallRecognitionLifetime, ESPMode::ThreadSafe>())
	{
		Wake = FPlatformProcess::GetSynchEventFromPool(false);
	}

	~FImpl() override
	{
		Shutdown();
		FPlatformProcess::ReturnSynchEventToPool(Wake);
	}

	void Start()
	{
		Thread.Reset(FRunnableThread::Create(this, TEXT("MallOfflineSpeech"), 0, TPri_BelowNormal));
		if (!Thread)
		{
			SetStatus(TEXT("Could not start the offline speech worker."));
			DispatchInitialized(false);
		}
	}

	void Shutdown()
	{
		Lifetime->bAlive.Store(false);
		bStopping.Store(true);
		bReady.Store(false);
		Wake->Trigger();
		if (Thread)
		{
			Thread->WaitForCompletion();
			Thread.Reset();
		}
	}

	bool Submit(TArray<int16> Samples, FOnRecognized Callback)
	{
		if (!bReady.Load() || bStopping.Load() || Samples.Num() < 4000 || Samples.Num() > 128000 || !Callback)
		{
			return false;
		}
		FScopeLock Lock(&QueueMutex);
		if (Queued.IsSet())
		{
			return false;
		}
		Queued.Emplace(FMallQueuedUtterance{MoveTemp(Samples), MoveTemp(Callback)});
		Wake->Trigger();
		return true;
	}

	uint32 Run() override
	{
		if (!LoadBackend())
		{
			DispatchInitialized(false);
			ReleaseBackend();
			return 0;
		}
		bReady.Store(!bStopping.Load());
		SetStatus(bUseWhisper ? TEXT("Offline English voice recognition ready (Whisper).")
			: TEXT("Offline English voice recognition ready."));
		DispatchInitialized(true);
		while (!bStopping.Load())
		{
			TOptional<FMallQueuedUtterance> Work;
			{
				FScopeLock Lock(&QueueMutex);
				if (Queued.IsSet())
				{
					Work = MoveTemp(Queued);
					Queued.Reset();
				}
			}
			if (Work.IsSet())
			{
				Recognize(Work.GetValue());
			}
			else
			{
				Wake->Wait(100);
			}
		}
		ReleaseBackend();
		return 0;
	}

	bool IsReady() const { return bReady.Load(); }
	FString GetStatus() const
	{
		FScopeLock Lock(&StatusMutex);
		return Status;
	}

private:
	using FModelNew = void* (*)(const char*);
	using FModelFree = void (*)(void*);
	using FRecognizerNew = void* (*)(void*, float);
	using FRecognizerFree = void (*)(void*);
	using FSetWords = void (*)(void*, int);
	using FAcceptWaveform = int (*)(void*, const short*, int);
	using FResult = const char* (*)(void*);
	using FSetLogLevel = void (*)(int);

	bool LoadBackend()
	{
		if (bUseWhisper)
		{
			Whisper = MakeUnique<FMallWhisperBackend>();
			FString Error;
			if (!Whisper->Initialize(DllDirectory, ModelDirectory, bStopping, Error))
			{
				SetStatus(MoveTemp(Error));
				return false;
			}
			return true;
		}
#if PLATFORM_WINDOWS
		// Serialize native setup and model lifetime changes across simultaneous PIE
		// worlds. The native API/loader state is process-wide, not world-specific.
		FScopeLock NativeLock(&GMallVoskNativeLifecycleMutex);
		void* Library = nullptr;
		FString LibraryError;
		if (!GetPinnedVoskLibrary(DllDirectory, Library, LibraryError))
		{
			SetStatus(MoveTemp(LibraryError));
			return false;
		}
		ModelNew = reinterpret_cast<FModelNew>(FPlatformProcess::GetDllExport(Library, TEXT("vosk_model_new")));
		ModelFree = reinterpret_cast<FModelFree>(FPlatformProcess::GetDllExport(Library, TEXT("vosk_model_free")));
		RecognizerNew = reinterpret_cast<FRecognizerNew>(FPlatformProcess::GetDllExport(Library, TEXT("vosk_recognizer_new")));
		RecognizerFree = reinterpret_cast<FRecognizerFree>(FPlatformProcess::GetDllExport(Library, TEXT("vosk_recognizer_free")));
		SetWords = reinterpret_cast<FSetWords>(FPlatformProcess::GetDllExport(Library, TEXT("vosk_recognizer_set_words")));
		AcceptWaveform = reinterpret_cast<FAcceptWaveform>(FPlatformProcess::GetDllExport(Library, TEXT("vosk_recognizer_accept_waveform_s")));
		Result = reinterpret_cast<FResult>(FPlatformProcess::GetDllExport(Library, TEXT("vosk_recognizer_result")));
		FinalResult = reinterpret_cast<FResult>(FPlatformProcess::GetDllExport(Library, TEXT("vosk_recognizer_final_result")));
		if (!ModelNew || !ModelFree || !RecognizerNew || !RecognizerFree || !SetWords || !AcceptWaveform || !Result || !FinalResult)
		{
			SetStatus(TEXT("Offline speech library has an incompatible API."));
			return false;
		}
		if (FSetLogLevel SetLogLevel = reinterpret_cast<FSetLogLevel>(FPlatformProcess::GetDllExport(Library, TEXT("vosk_set_log_level"))))
		{
			SetLogLevel(-1);
		}
		if (!FPaths::DirectoryExists(ModelDirectory))
		{
			SetStatus(TEXT("The offline English speech model is missing."));
			return false;
		}
		Model = ModelNew(TCHAR_TO_UTF8(*ModelDirectory));
		if (!Model)
		{
			SetStatus(TEXT("The offline English speech model could not be initialized."));
			return false;
		}
		return true;
#else
		SetStatus(TEXT("The prototype offline speech backend currently supports Windows only."));
		return false;
#endif
	}

	void ReleaseBackend()
	{
		bReady.Store(false);
		Whisper.Reset();
		if (Model && ModelFree)
		{
			FScopeLock NativeLock(&GMallVoskNativeLifecycleMutex);
			ModelFree(Model);
			Model = nullptr;
		}
		// Never unload the process-pinned DLLs here: another world may still own a
		// model, and this worker has not yet completed native TLS thread teardown.
	}

	void Recognize(FMallQueuedUtterance& Work)
	{
		if (Whisper)
		{
			FMallWhisperTranscript Transcript = Whisper->Recognize(Work.Samples, bStopping);
			DispatchRecognized(Work, MoveTemp(Transcript.Text), Transcript.Confidence, Transcript.bValid);
			return;
		}
		FMallTranscript Transcript;
		void* Recognizer = RecognizerNew(Model, 16000.0f);
		if (Recognizer)
		{
			SetWords(Recognizer, 1);
			// Decode unrestricted speech, then apply whole-phrase matching. A tiny command
			// grammar can force unrelated speech into a permitted command.
			for (int32 Offset = 0; Offset < Work.Samples.Num() && !bStopping.Load(); Offset += 3200)
			{
				const int32 Count = FMath::Min(3200, Work.Samples.Num() - Offset);
				const int Accepted = AcceptWaveform(Recognizer, Work.Samples.GetData() + Offset, Count);
				if (Accepted == 1)
				{
					Transcript.Append(Result(Recognizer));
				}
				else if (Accepted < 0)
				{
					Transcript.bValid = false;
					break;
				}
			}
			if (!bStopping.Load())
			{
				Transcript.Append(FinalResult(Recognizer));
			}
			RecognizerFree(Recognizer);
		}
		else
		{
			Transcript.bValid = false;
		}
		const float Confidence = Transcript.bValid && Transcript.WordCount > 0
			? static_cast<float>(Transcript.ConfidenceSum / Transcript.WordCount) : 0.0f;
		DispatchRecognized(Work, MoveTemp(Transcript.Text), Confidence, Transcript.bValid);
	}

	void DispatchRecognized(FMallQueuedUtterance& Work, FString Text, float Confidence, bool bValid)
	{
		if (bStopping.Load())
		{
			return;
		}
		FString Normalized = MallVoicePhraseMatcher::Normalize(Text);
		const FName PhraseId = bValid
			? MallVoicePhraseMatcher::Match(Normalized, Confidence, MinimumConfidence, Aliases) : NAME_None;
		const TSharedRef<FMallRecognitionLifetime, ESPMode::ThreadSafe> Guard = Lifetime;
		AsyncTask(ENamedThreads::GameThread,
			[Guard, Callback = MoveTemp(Work.Callback), PhraseId, Confidence, Normalized = MoveTemp(Normalized)]() mutable
			{
				if (Guard->bAlive.Load() && Callback)
				{
					Callback(PhraseId, Confidence, MoveTemp(Normalized));
				}
			});
	}

	void SetStatus(FString Value)
	{
		FScopeLock Lock(&StatusMutex);
		Status = MoveTemp(Value);
	}

	void DispatchInitialized(bool bSuccess)
	{
		const TSharedRef<FMallRecognitionLifetime, ESPMode::ThreadSafe> Guard = Lifetime;
		AsyncTask(ENamedThreads::GameThread,
			[Guard, Callback = MoveTemp(InitializationCallback), bSuccess, Message = GetStatus()]() mutable
			{
				if (Guard->bAlive.Load() && Callback)
				{
					Callback(bSuccess, MoveTemp(Message));
				}
			});
	}

	FString DllDirectory;
	FString ModelDirectory;
	TMap<FName, TArray<FString>> Aliases;
	FOnInitialized InitializationCallback;
	float MinimumConfidence;
	bool bUseWhisper = false;
	TUniquePtr<FMallWhisperBackend> Whisper;
	TSharedRef<FMallRecognitionLifetime, ESPMode::ThreadSafe> Lifetime;
	TUniquePtr<FRunnableThread> Thread;
	FEvent* Wake = nullptr;
	TAtomic<bool> bStopping{false};
	TAtomic<bool> bReady{false};
	FCriticalSection QueueMutex;
	TOptional<FMallQueuedUtterance> Queued;
	mutable FCriticalSection StatusMutex;
	FString Status = TEXT("Starting offline speech recognition.");
	void* Model = nullptr;
	FModelNew ModelNew = nullptr;
	FModelFree ModelFree = nullptr;
	FRecognizerNew RecognizerNew = nullptr;
	FRecognizerFree RecognizerFree = nullptr;
	FSetWords SetWords = nullptr;
	FAcceptWaveform AcceptWaveform = nullptr;
	FResult Result = nullptr;
	FResult FinalResult = nullptr;
};

FMallSpeechRecognizer::FMallSpeechRecognizer() = default;
FMallSpeechRecognizer::~FMallSpeechRecognizer() { Shutdown(); }

void FMallSpeechRecognizer::Initialize(const FString& DllDirectory, const FString& ModelDirectory,
	const TMap<FName, TArray<FString>>& PhraseAliases, FOnInitialized OnInitialized, float MinimumConfidence)
{
	Shutdown();
	Impl = MakeUnique<FImpl>(DllDirectory, ModelDirectory, PhraseAliases, MoveTemp(OnInitialized), MinimumConfidence);
	Impl->Start();
}

void FMallSpeechRecognizer::InitializeWhisper(const FString& DllDirectory, const FString& ModelFile,
	const TMap<FName, TArray<FString>>& PhraseAliases, FOnInitialized OnInitialized, float MinimumConfidence)
{
	Shutdown();
	Impl = MakeUnique<FImpl>(DllDirectory, ModelFile, PhraseAliases, MoveTemp(OnInitialized), MinimumConfidence, true);
	Impl->Start();
}

bool FMallSpeechRecognizer::SubmitUtterance(TArray<int16> Samples, FOnRecognized OnRecognized)
{
	return Impl && Impl->Submit(MoveTemp(Samples), MoveTemp(OnRecognized));
}

void FMallSpeechRecognizer::Shutdown() { Impl.Reset(); }
bool FMallSpeechRecognizer::IsReady() const { return Impl && Impl->IsReady(); }
FString FMallSpeechRecognizer::GetStatus() const
{
	return Impl ? Impl->GetStatus() : TEXT("Offline speech recognition is stopped.");
}
