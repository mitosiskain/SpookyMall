#include "Voice/MallWhisperBackend.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"

THIRD_PARTY_INCLUDES_START
#include "whisper.h"
#include "ggml-backend.h"
THIRD_PARTY_INCLUDES_END

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

namespace
{
// GetDllHandle may return a borrowed Windows loader handle. Retain all native
// modules for the process lifetime, including any native thread-local teardown.
FCriticalSection GMallWhisperNativeLifecycleMutex;
struct FMallWhisperProcessLibraries
{
	FString Directory;
	TArray<void*> PinnedHandles;
	TArray<void*> PinnedCpuHandles;
	bool bCpuRegistered = false;
};
FMallWhisperProcessLibraries GMallWhisperProcessLibraries;

#if PLATFORM_WINDOWS
bool IsWhisperModuleAtPath(HMODULE Module, const FString& ExpectedPath)
{
	TArray<TCHAR> Buffer;
	Buffer.SetNumUninitialized(32768);
	const DWORD Length = ::GetModuleFileNameW(Module, Buffer.GetData(), Buffer.Num());
	return Length > 0 && Length < static_cast<DWORD>(Buffer.Num())
		&& FPaths::IsSamePath(FString(static_cast<int32>(Length), Buffer.GetData()), ExpectedPath);
}
#endif

/** Caller holds GMallWhisperNativeLifecycleMutex, including the DLL directory stack. */
bool GetPinnedWhisperLibrary(const FString& Directory, void*& OutLibrary, FString& OutError)
{
#if PLATFORM_WINDOWS
	if (!GMallWhisperProcessLibraries.Directory.IsEmpty()
		&& !FPaths::IsSamePath(GMallWhisperProcessLibraries.Directory, Directory))
	{
		OutError = TEXT("Whisper libraries are already loaded from another directory; restart the application to change them.");
		return false;
	}
	GMallWhisperProcessLibraries.Directory = Directory;
	const TArray<FString> Names = {TEXT("ggml-base.dll"), TEXT("ggml.dll"), TEXT("whisper.dll")};
	FPlatformProcess::PushDllDirectory(*Directory);
	bool bSuccess = true;
	for (int32 Index = GMallWhisperProcessLibraries.PinnedHandles.Num(); Index < Names.Num(); ++Index)
	{
		const FString ExpectedPath = FPaths::Combine(Directory, Names[Index]);
		HMODULE ExistingHandle = ::GetModuleHandleW(*Names[Index]);
		if (ExistingHandle && !IsWhisperModuleAtPath(ExistingHandle, ExpectedPath))
		{
			OutError = FString::Printf(TEXT("Another plugin loaded a conflicting %s; restart without that plugin before using Whisper."),
				*Names[Index]);
			bSuccess = false;
			break;
		}
		void* Handle = FPlatformProcess::GetDllHandle(*ExpectedPath);
		if (!Handle)
		{
			OutError = FString::Printf(TEXT("Whisper speech library could not load: %s"), *Names[Index]);
			bSuccess = false;
			break;
		}
		if (!IsWhisperModuleAtPath(reinterpret_cast<HMODULE>(Handle), ExpectedPath))
		{
			OutError = FString::Printf(TEXT("Whisper loaded %s from an unexpected location."), *Names[Index]);
			bSuccess = false;
			break;
		}
		HMODULE PinnedHandle = nullptr;
		if (!::GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
			reinterpret_cast<LPCWSTR>(Handle), &PinnedHandle))
		{
			OutError = FString::Printf(TEXT("Whisper speech library could not be retained safely: %s"), *Names[Index]);
			bSuccess = false;
			break;
		}
		GMallWhisperProcessLibraries.PinnedHandles.Add(PinnedHandle);
	}
	if (bSuccess && !GMallWhisperProcessLibraries.bCpuRegistered)
	{
		void* GgmlLibrary = GMallWhisperProcessLibraries.PinnedHandles[1];
		const auto LoadBackends = reinterpret_cast<decltype(&ggml_backend_load_all_from_path)>(
			FPlatformProcess::GetDllExport(GgmlLibrary, TEXT("ggml_backend_load_all_from_path")));
		const auto FindBackend = reinterpret_cast<decltype(&ggml_backend_reg_by_name)>(
			FPlatformProcess::GetDllExport(GgmlLibrary, TEXT("ggml_backend_reg_by_name")));
		if (!LoadBackends || !FindBackend)
		{
			OutError = TEXT("Whisper CPU dispatch library has an incompatible API.");
			bSuccess = false;
		}
		else
		{
			// Upstream probes instruction support and selects the best compatible
			// CPU variant. Never directly execute an arbitrary AVX variant.
			LoadBackends(TCHAR_TO_UTF8(*Directory));
			TArray<FString> CpuNames;
			IFileManager::Get().FindFiles(CpuNames, *FPaths::Combine(Directory, TEXT("ggml-cpu-*.dll")), true, false);
			for (const FString& Name : CpuNames)
			{
				const FString FullPath = FPaths::Combine(Directory, Name);
				HMODULE LoadedHandle = ::GetModuleHandleW(*FullPath);
				if (!LoadedHandle)
				{
					continue; // Rejected CPU probes were unloaded by upstream.
				}
				if (!IsWhisperModuleAtPath(LoadedHandle, FullPath))
				{
					OutError = TEXT("Whisper CPU backend was loaded from an unexpected location.");
					bSuccess = false;
					break;
				}
				HMODULE PinnedHandle = nullptr;
				if (!::GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
					reinterpret_cast<LPCWSTR>(LoadedHandle), &PinnedHandle))
				{
					OutError = TEXT("Whisper CPU backend could not be retained safely.");
					bSuccess = false;
					break;
				}
				GMallWhisperProcessLibraries.PinnedCpuHandles.Add(PinnedHandle);
			}
			if (bSuccess && (!FindBackend("CPU") || GMallWhisperProcessLibraries.PinnedCpuHandles.IsEmpty()))
			{
				OutError = TEXT("No compatible offline Whisper CPU backend is available.");
				bSuccess = false;
			}
			GMallWhisperProcessLibraries.bCpuRegistered = bSuccess;
		}
	}
	FPlatformProcess::PopDllDirectory(*Directory);
	if (bSuccess)
	{
		OutLibrary = GMallWhisperProcessLibraries.PinnedHandles.Last();
	}
	return bSuccess;
#else
	OutError = TEXT("The prototype Whisper speech backend currently supports Windows only.");
	return false;
#endif
}

bool AbortWhisper(void* UserData)
{
	return static_cast<const TAtomic<bool>*>(UserData)->Load();
}

bool BeginWhisperEncoder(whisper_context*, whisper_state*, void* UserData)
{
	return !AbortWhisper(UserData);
}

/** Reject silence/DC and isolated clicks without imposing a normal speech loudness gate. */
bool HasAudibleContent(TConstArrayView<int16> Samples)
{
	constexpr int32 FrameSamples = 320; // 20 ms at the required 16 kHz.
	constexpr double MinimumAcMeanSquare = 0.0001 * 0.0001; // About -80 dBFS.
	int32 ActiveFrames = 0;
	for (int32 Offset = 0; Offset + FrameSamples <= Samples.Num(); Offset += FrameSamples)
	{
		double Sum = 0.0;
		double SumSquared = 0.0;
		for (int32 Index = Offset; Index < Offset + FrameSamples; ++Index)
		{
			const double Sample = static_cast<double>(Samples[Index]) / 32768.0;
			Sum += Sample;
			SumSquared += Sample * Sample;
		}
		const double Mean = Sum / FrameSamples;
		const double AcMeanSquare = SumSquared / FrameSamples - Mean * Mean;
		if (AcMeanSquare >= MinimumAcMeanSquare && ++ActiveFrames >= 4)
		{
			return true;
		}
	}
	return false;
}
}

class FMallWhisperBackend::FImpl
{
public:
	bool Initialize(const FString& DllDirectory, const FString& ModelFile,
		const TAtomic<bool>& bStopping, FString& OutError)
	{
		FScopeLock NativeLock(&GMallWhisperNativeLifecycleMutex);
		if (bStopping.Load())
		{
			OutError = TEXT("Whisper speech initialization was cancelled.");
			return false;
		}
		void* Library = nullptr;
		if (!GetPinnedWhisperLibrary(DllDirectory, Library, OutError))
		{
			return false;
		}
		Version = reinterpret_cast<decltype(&whisper_version)>(FPlatformProcess::GetDllExport(Library, TEXT("whisper_version")));
		// The public structs are versioned by the pinned upstream headers. Check
		// before calling any function that passes one of those structs by value.
		const char* RuntimeVersion = Version ? Version() : nullptr;
		if (!RuntimeVersion || FCStringAnsi::Strcmp(RuntimeVersion, "1.9.5") != 0)
		{
			OutError = TEXT("Whisper speech libraries do not match the bundled 1.9.5 headers.");
			return false;
		}
		ContextDefaults = reinterpret_cast<decltype(&whisper_context_default_params)>(
			FPlatformProcess::GetDllExport(Library, TEXT("whisper_context_default_params")));
		ModelNew = reinterpret_cast<decltype(&whisper_init_from_file_with_params)>(
			FPlatformProcess::GetDllExport(Library, TEXT("whisper_init_from_file_with_params")));
		ModelFree = reinterpret_cast<decltype(&whisper_free)>(FPlatformProcess::GetDllExport(Library, TEXT("whisper_free")));
		FullDefaults = reinterpret_cast<decltype(&whisper_full_default_params)>(
			FPlatformProcess::GetDllExport(Library, TEXT("whisper_full_default_params")));
		Full = reinterpret_cast<decltype(&whisper_full)>(FPlatformProcess::GetDllExport(Library, TEXT("whisper_full")));
		SegmentCount = reinterpret_cast<decltype(&whisper_full_n_segments)>(
			FPlatformProcess::GetDllExport(Library, TEXT("whisper_full_n_segments")));
		SegmentText = reinterpret_cast<decltype(&whisper_full_get_segment_text)>(
			FPlatformProcess::GetDllExport(Library, TEXT("whisper_full_get_segment_text")));
		SegmentNoSpeech = reinterpret_cast<decltype(&whisper_full_get_segment_no_speech_prob)>(
			FPlatformProcess::GetDllExport(Library, TEXT("whisper_full_get_segment_no_speech_prob")));
		TokenCount = reinterpret_cast<decltype(&whisper_full_n_tokens)>(
			FPlatformProcess::GetDllExport(Library, TEXT("whisper_full_n_tokens")));
		TokenData = reinterpret_cast<decltype(&whisper_full_get_token_data)>(
			FPlatformProcess::GetDllExport(Library, TEXT("whisper_full_get_token_data")));
		EndOfTextToken = reinterpret_cast<decltype(&whisper_token_eot)>(
			FPlatformProcess::GetDllExport(Library, TEXT("whisper_token_eot")));
		if (!ContextDefaults || !ModelNew || !ModelFree || !FullDefaults || !Full || !SegmentCount
			|| !SegmentText || !SegmentNoSpeech || !TokenCount || !TokenData || !EndOfTextToken)
		{
			OutError = TEXT("Whisper speech library has an incompatible API.");
			return false;
		}
		if (!FPaths::FileExists(ModelFile))
		{
			OutError = TEXT("The offline English Whisper model is missing.");
			return false;
		}
		whisper_context_params ContextParams = ContextDefaults();
		ContextParams.use_gpu = false;
		ContextParams.flash_attn = false;
		ContextParams.dtw_token_timestamps = false;
		Context = ModelNew(TCHAR_TO_UTF8(*ModelFile), ContextParams);
		if (!Context)
		{
			OutError = TEXT("The offline English Whisper model could not be initialized.");
			return false;
		}
		return true;
	}

	FMallWhisperTranscript Recognize(TConstArrayView<int16> Samples, const TAtomic<bool>& bStopping)
	{
		FMallWhisperTranscript Transcript;
		if (!Context || bStopping.Load())
		{
			Transcript.bValid = false;
			return Transcript;
		}
		if (!HasAudibleContent(Samples))
		{
			return Transcript;
		}
		TArray<float> Audio;
		Audio.SetNumUninitialized(Samples.Num());
		for (int32 Index = 0; Index < Samples.Num(); ++Index)
		{
			Audio[Index] = static_cast<float>(Samples[Index]) / 32768.0f;
		}

		whisper_full_params Params = FullDefaults(WHISPER_SAMPLING_GREEDY);
		Params.n_threads = FMath::Clamp(FPlatformMisc::NumberOfCores() - 1, 1, 4);
		Params.language = "en";
		Params.detect_language = false;
		Params.translate = false;
		Params.no_context = true;
		Params.n_max_text_ctx = 0;
		Params.no_timestamps = true;
		Params.single_segment = true;
		Params.print_special = false;
		Params.print_progress = false;
		Params.print_realtime = false;
		Params.print_timestamps = false;
		Params.token_timestamps = false;
		Params.initial_prompt = nullptr;
		Params.carry_initial_prompt = false;
		Params.prompt_tokens = nullptr;
		Params.prompt_n_tokens = 0;
		Params.grammar_rules = nullptr;
		Params.n_grammar_rules = 0;
		Params.vad = false; // One complete push-to-talk utterance; no separate VAD model.
		Params.temperature = 0.0f;
		Params.temperature_inc = 0.0f;
		Params.greedy.best_of = 1;
		Params.suppress_blank = true;
		Params.suppress_nst = true;
		Params.encoder_begin_callback = &BeginWhisperEncoder;
		Params.encoder_begin_callback_user_data = const_cast<TAtomic<bool>*>(&bStopping);
		Params.abort_callback = &AbortWhisper;
		Params.abort_callback_user_data = const_cast<TAtomic<bool>*>(&bStopping);

		if (Full(Context, Params, Audio.GetData(), Audio.Num()) != 0 || bStopping.Load())
		{
			Transcript.bValid = false;
			return Transcript;
		}
		const int32 Count = SegmentCount(Context);
		if (Count < 0 || Count > 128)
		{
			Transcript.bValid = false;
			return Transcript;
		}
		const whisper_token Eot = EndOfTextToken(Context);
		double ProbabilitySum = 0.0;
		int32 OrdinaryTokens = 0;
		for (int32 Segment = 0; Segment < Count; ++Segment)
		{
			const float NoSpeechProbability = SegmentNoSpeech(Context, Segment);
			if (!FMath::IsFinite(NoSpeechProbability))
			{
				Transcript.bValid = false;
				return Transcript;
			}
			// Keep the upstream no-speech/log-probability filter and reject its
			// very strongest no-speech results even when they contain text.
			if (NoSpeechProbability >= 0.95f)
			{
				// Reject the whole turn rather than dropping a segment that
				// could contain a negation of a later recognized command.
				Transcript.bValid = false;
				return Transcript;
			}
			const char* Text = SegmentText(Context, Segment);
			const int32 Tokens = TokenCount(Context, Segment);
			if (!Text || Tokens < 0 || Tokens > 512)
			{
				Transcript.bValid = false;
				return Transcript;
			}
			if (!Transcript.Text.IsEmpty())
			{
				Transcript.Text += TEXT(" ");
			}
			Transcript.Text += UTF8_TO_TCHAR(Text);
			for (int32 Token = 0; Token < Tokens; ++Token)
			{
				const whisper_token_data Data = TokenData(Context, Segment, Token);
				if (Data.id >= 0 && Data.id < Eot)
				{
					if (!FMath::IsFinite(Data.p) || Data.p < 0.0f || Data.p > 1.0f)
					{
						Transcript.bValid = false;
						return Transcript;
					}
					ProbabilitySum += Data.p;
					++OrdinaryTokens;
				}
			}
		}
		Transcript.Confidence = OrdinaryTokens > 0
			? static_cast<float>(ProbabilitySum / OrdinaryTokens) : 0.0f;
		if (!Transcript.Text.IsEmpty() && OrdinaryTokens == 0)
		{
			Transcript.bValid = false;
		}
		return Transcript;
	}

	void Shutdown()
	{
		if (Context && ModelFree)
		{
			FScopeLock NativeLock(&GMallWhisperNativeLifecycleMutex);
			ModelFree(Context);
			Context = nullptr;
		}
		// Process-pinned DLLs are deliberately never freed by an individual world.
	}

private:
	whisper_context* Context = nullptr;
	decltype(&whisper_version) Version = nullptr;
	decltype(&whisper_context_default_params) ContextDefaults = nullptr;
	decltype(&whisper_init_from_file_with_params) ModelNew = nullptr;
	decltype(&whisper_free) ModelFree = nullptr;
	decltype(&whisper_full_default_params) FullDefaults = nullptr;
	decltype(&whisper_full) Full = nullptr;
	decltype(&whisper_full_n_segments) SegmentCount = nullptr;
	decltype(&whisper_full_get_segment_text) SegmentText = nullptr;
	decltype(&whisper_full_get_segment_no_speech_prob) SegmentNoSpeech = nullptr;
	decltype(&whisper_full_n_tokens) TokenCount = nullptr;
	decltype(&whisper_full_get_token_data) TokenData = nullptr;
	decltype(&whisper_token_eot) EndOfTextToken = nullptr;
};

FMallWhisperBackend::FMallWhisperBackend() : Impl(MakeUnique<FImpl>()) {}
FMallWhisperBackend::~FMallWhisperBackend() { Shutdown(); }

bool FMallWhisperBackend::Initialize(const FString& DllDirectory, const FString& ModelFile,
	const TAtomic<bool>& bStopping, FString& OutError)
{
	return Impl->Initialize(DllDirectory, ModelFile, bStopping, OutError);
}

FMallWhisperTranscript FMallWhisperBackend::Recognize(TConstArrayView<int16> Samples, const TAtomic<bool>& bStopping)
{
	return Impl->Recognize(Samples, bStopping);
}

void FMallWhisperBackend::Shutdown() { Impl->Shutdown(); }
