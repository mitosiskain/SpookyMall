#include "Voice/MallSpeechRecognizer.h"
#include "Voice/MallMicrophoneProcessor.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

namespace MallSpeechRecognitionTests
{
struct FCase
{
	FString File;
	FName ExpectedId;
	bool bQuietMicrophone = false;
};

/** Test fixture reader: accept only uncompressed PCM16 mono 16 kHz RIFF/WAVE. */
bool LoadPCMFixture(const FString& Path, TArray<int16>& Samples)
{
	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *Path) || Bytes.Num() < 12
		|| FMemory::Memcmp(Bytes.GetData(), "RIFF", 4) != 0
		|| FMemory::Memcmp(Bytes.GetData() + 8, "WAVE", 4) != 0)
	{
		return false;
	}
	const auto Read16 = [&Bytes](int64 Position)
	{
		uint16 Value = 0;
		FMemory::Memcpy(&Value, Bytes.GetData() + Position, sizeof(Value));
		return Value;
	};
	const auto Read32 = [&Bytes](int64 Position)
	{
		uint32 Value = 0;
		FMemory::Memcpy(&Value, Bytes.GetData() + Position, sizeof(Value));
		return Value;
	};
	bool bCorrectFormat = false;
	for (int64 Position = 12; Position + 8 <= Bytes.Num();)
	{
		const uint32 Size = Read32(Position + 4);
		const int64 DataStart = Position + 8;
		if (Size > static_cast<uint64>(Bytes.Num() - DataStart))
		{
			return false;
		}
		if (FMemory::Memcmp(Bytes.GetData() + Position, "fmt ", 4) == 0 && Size >= 16)
		{
			bCorrectFormat = Read16(DataStart) == 1 && Read16(DataStart + 2) == 1
				&& Read32(DataStart + 4) == 16000 && Read16(DataStart + 14) == 16;
		}
		else if (FMemory::Memcmp(Bytes.GetData() + Position, "data", 4) == 0)
		{
			if ((Size & 1) != 0 || Size < 8000 || Size > 256000)
			{
				return false;
			}
			Samples.SetNumUninitialized(Size / sizeof(int16));
			FMemory::Memcpy(Samples.GetData(), Bytes.GetData() + DataStart, Size);
		}
		Position = DataStart + Size + (Size & 1);
	}
	return bCorrectFormat && !Samples.IsEmpty();
}

struct FState
{
	TUniquePtr<FMallSpeechRecognizer> Recognizer = MakeUnique<FMallSpeechRecognizer>();
	TArray<FCase> Cases;
	FString SamplesDirectory;
	FString InitializationStatus;
	bool bInitialized = false;
	bool bInitializationSucceeded = false;
	bool bCallbacksOnGameThread = true;
	bool bWaitingForResult = false;
	bool bCancellationPhase = false;
	int32 CanceledCallbacks = 0;
	int32 CaseIndex = 0;
	FName ResultId;
	float ResultConfidence = 0.0f;
	FString ResultText;
	double Deadline = 0.0;
};

class FRunBackend final : public IAutomationLatentCommand
{
public:
	FRunBackend(FAutomationTestBase* InTest, TSharedRef<FState, ESPMode::ThreadSafe> InState)
		: Test(InTest), State(MoveTemp(InState)) {}

	bool Update() override
	{
		const double Now = FPlatformTime::Seconds();
		if (State->bCancellationPhase)
		{
			if (Now < State->Deadline)
			{
				return false;
			}
			Test->TestEqual(TEXT("Shutdown suppresses outstanding recognition callbacks"), State->CanceledCallbacks, 0);
			Test->TestTrue(TEXT("Initialization and recognition callbacks run on the game thread"), State->bCallbacksOnGameThread);
			return true;
		}
		if (Now > State->Deadline)
		{
			Test->AddError(TEXT("Timed out waiting for the actual offline speech backend."));
			State->Recognizer->Shutdown();
			return true;
		}
		if (!State->bInitialized)
		{
			return false;
		}
		if (!State->bInitializationSucceeded)
		{
			Test->AddError(TEXT("Actual offline speech initialization failed: ") + State->InitializationStatus);
			State->Recognizer->Shutdown();
			return true;
		}
		if (State->bWaitingForResult)
		{
			return false;
		}
		if (State->CaseIndex > 0)
		{
			const FCase& Previous = State->Cases[State->CaseIndex - 1];
			const FString CaseName = Previous.File + (Previous.bQuietMicrophone ? TEXT(" [quiet microphone, cold AGC]") : TEXT(""));
			Test->TestTrue(*FString::Printf(TEXT("%s produces the expected complete-phrase decision"), *CaseName),
				State->ResultId == Previous.ExpectedId);
			Test->TestTrue(TEXT("Backend confidence stays finite and bounded"), FMath::IsFinite(State->ResultConfidence)
				&& State->ResultConfidence >= 0.0f && State->ResultConfidence <= 1.0f);
			Test->AddInfo(FString::Printf(TEXT("Fixture %s: phrase=%s confidence=%.3f heard=%s"),
				*CaseName, *State->ResultId.ToString(), State->ResultConfidence, *State->ResultText));
		}
		if (State->CaseIndex >= State->Cases.Num())
		{
			TArray<int16> CancellationAudio;
			CancellationAudio.SetNumZeroed(16000);
			const TWeakPtr<FState, ESPMode::ThreadSafe> Weak = State;
			Test->TestTrue(TEXT("Cancellation test queues a valid utterance"), State->Recognizer->SubmitUtterance(MoveTemp(CancellationAudio),
				[Weak](FName, float, FString)
				{
					if (TSharedPtr<FState, ESPMode::ThreadSafe> Current = Weak.Pin()) { ++Current->CanceledCallbacks; }
				}));
			State->Recognizer->Shutdown();
			Test->TestFalse(TEXT("Recognizer is not ready after shutdown"), State->Recognizer->IsReady());
			TArray<int16> AfterShutdown;
			AfterShutdown.SetNumZeroed(16000);
			Test->TestFalse(TEXT("Shutdown rejects new work"), State->Recognizer->SubmitUtterance(MoveTemp(AfterShutdown), [](FName, float, FString) {}));
			State->bCancellationPhase = true;
			State->Deadline = FPlatformTime::Seconds() + 0.25;
			return false;
		}
		TArray<int16> Samples;
		const FCase& CurrentCase = State->Cases[State->CaseIndex];
		if (!LoadPCMFixture(FPaths::Combine(State->SamplesDirectory, CurrentCase.File), Samples))
		{
			Test->AddError(TEXT("Invalid PCM16 mono 16 kHz WAV fixture: ") + CurrentCase.File);
			State->Recognizer->Shutdown();
			return true;
		}
		if (CurrentCase.bQuietMicrophone)
		{
			const auto RMS = [](const TArray<int16>& Audio)
			{
				double Power = 0.0;
				for (const int16 Sample : Audio)
				{
					const double Value = static_cast<double>(Sample) / 32768.0;
					Power += Value * Value;
				}
				return Audio.IsEmpty() ? 0.0 : FMath::Sqrt(Power / Audio.Num());
			};
			const double OriginalRMS = RMS(Samples);
			if (!Test->TestTrue(TEXT("Quiet-microphone source fixture has nonzero audio"), OriginalRMS > 0.0))
			{
				State->Recognizer->Shutdown();
				return true;
			}
			const double Scale = 0.0004 / OriginalRMS;
			for (int16& Sample : Samples)
			{
				Sample = static_cast<int16>(FMath::Clamp(FMath::RoundToInt(Sample * Scale), -32768, 32767));
			}
			const double QuietRMS = RMS(Samples);
			Test->TestTrue(TEXT("Backend regression starts with actual speech scaled to 0.0004 RMS"),
				FMath::IsNearlyEqual(QuietRMS, 0.0004, 0.00005));
			const int32 OriginalCount = Samples.Num();
			// A newly constructed processor must acquire this utterance itself. No
			// warmup tone, prior speech, microphone device or saved user profile is used.
			FMallMicrophoneProcessor Processor;
			const FMallMicrophoneOptions Options;
			const FMallMicrophoneLevels Levels = Processor.Process(Samples, Options);
			Test->TestEqual(TEXT("Cold microphone conditioning preserves every ASR sample"), Samples.Num(), OriginalCount);
			Test->TestTrue(TEXT("Cold AGC raises real speech above the recognizer activity threshold"), Levels.ProcessedRMS > 0.006f);
			Test->TestTrue(TEXT("Cold AGC reports the actual quiet input RMS"),
				FMath::IsNearlyEqual(static_cast<double>(Levels.RawRMS), QuietRMS, 0.0000001));
		}
		if (State->CaseIndex == 0)
		{
			Test->TestTrue(TEXT("Backend is ready after asynchronous initialization"), State->Recognizer->IsReady());
			TArray<int16> TooShort;
			TooShort.SetNumZeroed(3999);
			TArray<int16> TooLong;
			TooLong.SetNumZeroed(128001);
			Test->TestFalse(TEXT("Utterances below the minimum are rejected"), State->Recognizer->SubmitUtterance(MoveTemp(TooShort), [](FName, float, FString) {}));
			Test->TestFalse(TEXT("Utterances above the bounded maximum are rejected"), State->Recognizer->SubmitUtterance(MoveTemp(TooLong), [](FName, float, FString) {}));
		}
		State->bWaitingForResult = true;
		State->Deadline = Now + 20.0;
		const TWeakPtr<FState, ESPMode::ThreadSafe> Weak = State;
		const bool bSubmitted = State->Recognizer->SubmitUtterance(MoveTemp(Samples),
			[Weak](FName PhraseId, float Confidence, FString Text)
			{
				if (TSharedPtr<FState, ESPMode::ThreadSafe> Current = Weak.Pin())
				{
					Current->bCallbacksOnGameThread &= IsInGameThread();
					Current->ResultId = PhraseId;
					Current->ResultConfidence = Confidence;
					Current->ResultText = MoveTemp(Text);
					Current->bWaitingForResult = false;
				}
			});
		if (!Test->TestTrue(TEXT("Backend accepts the next PCM fixture"), bSubmitted))
		{
			State->Recognizer->Shutdown();
			return true;
		}
		++State->CaseIndex;
		return false;
	}

private:
	FAutomationTestBase* Test;
	TSharedRef<FState, ESPMode::ThreadSafe> State;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMallOfflineSpeechBackendTest, "MyProject2.Voice.OfflineSpeechBackend",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMallOfflineSpeechBackendTest::RunTest(const FString& Parameters)
{
#if !PLATFORM_WINDOWS
	AddInfo(TEXT("SKIP: the prototype offline speech backend currently supports Windows only."));
	return true;
#else
	using namespace MallSpeechRecognitionTests;
	const TSharedRef<FState, ESPMode::ThreadSafe> State = MakeShared<FState, ESPMode::ThreadSafe>();
	State->SamplesDirectory = FPaths::Combine(FPaths::ProjectDir(), TEXT("Automation/Voice/Fixtures"));
	FParse::Value(FCommandLine::Get(), TEXT("MallVoiceTestSamples="), State->SamplesDirectory);
	State->Cases = {
		{TEXT("where_are_you.wav"), TEXT("where_are_you")},
		{TEXT("give_me_a_sign.wav"), TEXT("give_me_a_sign")},
		{TEXT("come_here.wav"), TEXT("come_here")},
		{TEXT("leave_me_alone.wav"), TEXT("leave_me_alone")},
		{TEXT("unrelated.wav"), NAME_None},
		{TEXT("negation.wav"), NAME_None},
		{TEXT("longer.wav"), NAME_None},
		{TEXT("come_here.wav"), TEXT("come_here"), true}
	};
	for (const FCase& TestCase : State->Cases)
	{
		if (!FPaths::FileExists(FPaths::Combine(State->SamplesDirectory, TestCase.File)))
		{
			AddInfo(TEXT("SKIP: synthetic voice fixtures are missing; provide -MallVoiceTestSamples=<directory> containing the WAV fixtures for all eight 16 kHz PCM cases. Missing: ") + TestCase.File);
			return true;
		}
	}
	TMap<FName, TArray<FString>> Aliases;
	Aliases.Add(TEXT("where_are_you"), {TEXT("where are you")});
	Aliases.Add(TEXT("give_me_a_sign"), {TEXT("give me a sign")});
	Aliases.Add(TEXT("come_here"), {TEXT("come here")});
	Aliases.Add(TEXT("leave_me_alone"), {TEXT("leave me alone")});
	State->Deadline = FPlatformTime::Seconds() + 30.0;
	const TWeakPtr<FState, ESPMode::ThreadSafe> Weak = State;
	State->Recognizer->Initialize(FPaths::Combine(FPaths::ProjectDir(), TEXT("ThirdParty/Vosk/Win64")),
		FPaths::Combine(FPaths::ProjectContentDir(), TEXT("VoiceModels/vosk-model-small-en-us-0.15")), Aliases,
		[Weak](bool bSuccess, FString Status)
		{
			if (TSharedPtr<FState, ESPMode::ThreadSafe> Current = Weak.Pin())
			{
				Current->bCallbacksOnGameThread &= IsInGameThread();
				Current->bInitializationSucceeded = bSuccess;
				Current->InitializationStatus = MoveTemp(Status);
				Current->bInitialized = true;
			}
		});
	ADD_LATENT_AUTOMATION_COMMAND(FRunBackend(this, State));
	return true;
#endif
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMallWhisperSpeechBackendTest, "MyProject2.Voice.WhisperSpeechBackend",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMallWhisperSpeechBackendTest::RunTest(const FString& Parameters)
{
#if !PLATFORM_WINDOWS
	AddInfo(TEXT("SKIP: the offline Whisper runtime currently supports Windows only."));
	return true;
#else
	using namespace MallSpeechRecognitionTests;
	const TSharedRef<FState, ESPMode::ThreadSafe> State = MakeShared<FState, ESPMode::ThreadSafe>();
	State->SamplesDirectory = FPaths::Combine(FPaths::ProjectDir(), TEXT("Automation/Voice/Fixtures/Whisper"));
	State->Cases = {
		{TEXT("where_0.wav"), TEXT("where_are_you")},
		{TEXT("who_0.wav"), TEXT("who_are_you")},
		{TEXT("age_0.wav"), TEXT("how_old_are_you")},
		{TEXT("book_0.wav"), TEXT("write_in_book")},
		{TEXT("where_0.wav"), TEXT("where_are_you"), true},
		{TEXT("where_noisy.wav"), TEXT("where_are_you")},
		{TEXT("negated_0.wav"), NAME_None},
		{TEXT("unrelated_0.wav"), NAME_None},
		{TEXT("similar_noise_words_0.wav"), NAME_None},
		{TEXT("silence.wav"), NAME_None},
		{TEXT("noise.wav"), NAME_None},
		{TEXT("pop.wav"), NAME_None}
	};
	for (const FCase& TestCase : State->Cases)
	{
		if (!TestTrue(TEXT("Required Whisper regression fixture exists"),
			FPaths::FileExists(FPaths::Combine(State->SamplesDirectory, TestCase.File))))
		{
			return false;
		}
	}
	TMap<FName, TArray<FString>> Aliases;
	Aliases.Add(TEXT("where_are_you"), {TEXT("where are you")});
	Aliases.Add(TEXT("who_are_you"), {TEXT("who are you")});
	Aliases.Add(TEXT("how_old_are_you"), {TEXT("how old are you")});
	Aliases.Add(TEXT("write_in_book"), {TEXT("write in the book")});
	State->Deadline = FPlatformTime::Seconds() + 30.0;
	const TWeakPtr<FState, ESPMode::ThreadSafe> Weak = State;
	State->Recognizer->InitializeWhisper(
		FPaths::Combine(FPaths::ProjectDir(), TEXT("ThirdParty/Whisper/Win64")),
		FPaths::Combine(FPaths::ProjectDir(), TEXT("ThirdParty/Whisper/Models/ggml-small.en.bin")), Aliases,
		[Weak](bool bSuccess, FString Status)
		{
			if (TSharedPtr<FState, ESPMode::ThreadSafe> Current = Weak.Pin())
			{
				Current->bCallbacksOnGameThread &= IsInGameThread();
				Current->bInitializationSucceeded = bSuccess;
				Current->InitializationStatus = MoveTemp(Status);
				Current->bInitialized = true;
			}
		});
	ADD_LATENT_AUTOMATION_COMMAND(FRunBackend(this, State));
	return true;
#endif
}

namespace MallSpeechRecognitionTests
{
enum class EMultiWorkerPhase : uint8
{
	Initializing,
	FirstSurvivorResult,
	SecondSurvivorResult,
	Finished
};

struct FMultiWorkerState
{
	TUniquePtr<FMallSpeechRecognizer> Workers[2] = {
		MakeUnique<FMallSpeechRecognizer>(), MakeUnique<FMallSpeechRecognizer>()};
	TUniquePtr<FMallSpeechRecognizer> CancelDuringInitialization = MakeUnique<FMallSpeechRecognizer>();
	TArray<int16> Samples;
	TMap<FName, TArray<FString>> Aliases;
	bool bInitialized[2] = {false, false};
	bool bInitializationSucceeded[2] = {false, false};
	FString InitializationStatus[2];
	bool bCallbacksOnGameThread = true;
	bool bResultReady = false;
	FName ResultId;
	float ResultConfidence = 0.0f;
	int32 CanceledRecognitionCallbacks = 0;
	int32 CanceledInitializationCallbacks = 0;
	int32 Round = 0;
	int32 Survivor = 1;
	bool bWhisper = false;
	int32 RoundsToRun = 3;
	FName ExpectedPhraseId = TEXT("come_here");
	double Deadline = 0.0;
	EMultiWorkerPhase Phase = EMultiWorkerPhase::Initializing;

	void InitializeWorker(FMallSpeechRecognizer& Worker, FMallSpeechRecognizer::FOnInitialized Callback) const
	{
		if (bWhisper)
		{
			Worker.InitializeWhisper(
				FPaths::Combine(FPaths::ProjectDir(), TEXT("ThirdParty/Whisper/Win64")),
				FPaths::Combine(FPaths::ProjectDir(), TEXT("ThirdParty/Whisper/Models/ggml-small.en.bin")),
				Aliases, MoveTemp(Callback));
		}
		else
		{
			Worker.Initialize(
				FPaths::Combine(FPaths::ProjectDir(), TEXT("ThirdParty/Vosk/Win64")),
				FPaths::Combine(FPaths::ProjectContentDir(), TEXT("VoiceModels/vosk-model-small-en-us-0.15")),
				Aliases, MoveTemp(Callback));
		}
	}

	void ShutdownAll()
	{
		Workers[0]->Shutdown();
		Workers[1]->Shutdown();
		CancelDuringInitialization->Shutdown();
	}
};

void BeginMultiWorkerRound(const TSharedRef<FMultiWorkerState, ESPMode::ThreadSafe>& State)
{
	State->Phase = EMultiWorkerPhase::Initializing;
	State->Deadline = FPlatformTime::Seconds() + 30.0;
	const TWeakPtr<FMultiWorkerState, ESPMode::ThreadSafe> Weak = State;
	for (int32 Index = 0; Index < 2; ++Index)
	{
		State->bInitialized[Index] = false;
		State->bInitializationSucceeded[Index] = false;
		State->InitializeWorker(*State->Workers[Index],
			[Weak, Index](bool bSuccess, FString Status)
			{
				if (TSharedPtr<FMultiWorkerState, ESPMode::ThreadSafe> Current = Weak.Pin())
				{
					Current->bCallbacksOnGameThread &= IsInGameThread();
					Current->bInitialized[Index] = true;
					Current->bInitializationSucceeded[Index] = bSuccess;
					Current->InitializationStatus[Index] = MoveTemp(Status);
				}
			});
	}
}

class FRunMultiWorkerTeardown final : public IAutomationLatentCommand
{
public:
	FRunMultiWorkerTeardown(FAutomationTestBase* InTest, TSharedRef<FMultiWorkerState, ESPMode::ThreadSafe> InState)
		: Test(InTest), State(MoveTemp(InState)) {}

	bool Update() override
	{
		const double Now = FPlatformTime::Seconds();
		if (State->Phase == EMultiWorkerPhase::Finished)
		{
			if (Now < State->Deadline) { return false; }
			Test->TestEqual(TEXT("Canceled recognition never calls into a destroyed world"), State->CanceledRecognitionCallbacks, 0);
			Test->TestEqual(TEXT("Canceled initialization never calls into a destroyed world"), State->CanceledInitializationCallbacks, 0);
			Test->TestTrue(TEXT("All concurrent worker callbacks run on the game thread"), State->bCallbacksOnGameThread);
			return true;
		}
		if (Now > State->Deadline)
		{
			Test->AddError(TEXT("Timed out during simultaneous offline speech worker teardown/restart regression."));
			State->ShutdownAll();
			return true;
		}
		if (State->Phase == EMultiWorkerPhase::Initializing)
		{
			if (!State->bInitialized[0] || !State->bInitialized[1]) { return false; }
			for (int32 Index = 0; Index < 2; ++Index)
			{
				if (!State->bInitializationSucceeded[Index])
				{
					Test->AddError(TEXT("Concurrent native speech initialization failed: ") + State->InitializationStatus[Index]);
					State->ShutdownAll();
					return true;
				}
				Test->TestTrue(TEXT("Both simultaneous recognizers are ready"), State->Workers[Index]->IsReady());
			}
			const int32 StoppedWorker = State->Round % 2;
			State->Survivor = 1 - StoppedWorker;
			const TWeakPtr<FMultiWorkerState, ESPMode::ThreadSafe> Weak = State;
			if (!Test->TestTrue(TEXT("Worker scheduled for cancellation accepts actual PCM"),
				State->Workers[StoppedWorker]->SubmitUtterance(State->Samples, [Weak](FName, float, FString)
				{
					if (TSharedPtr<FMultiWorkerState, ESPMode::ThreadSafe> Current = Weak.Pin())
					{
						Current->bCallbacksOnGameThread &= IsInGameThread();
						++Current->CanceledRecognitionCallbacks;
					}
				})))
			{
				State->ShutdownAll();
				return true;
			}
			if (!SubmitSurvivor()) { State->ShutdownAll(); return true; }
			// Both workers have a model and queued PCM. Destroy one while the other
			// remains live; the old per-world FreeDllHandle invalidated its native API.
			State->Workers[StoppedWorker]->Shutdown();
			Test->TestFalse(TEXT("Stopped worker is no longer ready"), State->Workers[StoppedWorker]->IsReady());
			Test->TestTrue(TEXT("Other world remains ready during sibling teardown"), State->Workers[State->Survivor]->IsReady());
			// Also end a new world's lifetime before its native initialization finishes.
			State->InitializeWorker(*State->CancelDuringInitialization,
				[Weak](bool, FString)
				{
					if (TSharedPtr<FMultiWorkerState, ESPMode::ThreadSafe> Current = Weak.Pin())
					{
						Current->bCallbacksOnGameThread &= IsInGameThread();
						++Current->CanceledInitializationCallbacks;
					}
				});
			State->CancelDuringInitialization->Shutdown();
			State->Phase = EMultiWorkerPhase::FirstSurvivorResult;
			State->Deadline = FPlatformTime::Seconds() + 20.0;
			return false;
		}
		if (!State->bResultReady) { return false; }
		Test->TestTrue(TEXT("Surviving world recognizes actual PCM after sibling shutdown"), State->ResultId == State->ExpectedPhraseId);
		Test->TestTrue(TEXT("Survivor confidence remains valid"), FMath::IsFinite(State->ResultConfidence)
			&& State->ResultConfidence >= (State->bWhisper ? 0.0f : 0.70f) && State->ResultConfidence <= 1.0f);
		if (State->Phase == EMultiWorkerPhase::FirstSurvivorResult)
		{
			// Fresh native recognizer construction must also work after sibling TLS exit.
			if (!SubmitSurvivor()) { State->ShutdownAll(); return true; }
			State->Phase = EMultiWorkerPhase::SecondSurvivorResult;
			State->Deadline = Now + 20.0;
			return false;
		}
		State->ShutdownAll();
		Test->AddInfo(FString::Printf(TEXT("Concurrent speech teardown/restart round %d passed."), State->Round + 1));
		if (++State->Round < State->RoundsToRun)
		{
			BeginMultiWorkerRound(State);
		}
		else
		{
			State->Phase = EMultiWorkerPhase::Finished;
			State->Deadline = FPlatformTime::Seconds() + 0.25;
		}
		return false;
	}

private:
	bool SubmitSurvivor()
	{
		State->bResultReady = false;
		const TWeakPtr<FMultiWorkerState, ESPMode::ThreadSafe> Weak = State;
		return Test->TestTrue(TEXT("Surviving recognizer accepts actual PCM"),
			State->Workers[State->Survivor]->SubmitUtterance(State->Samples,
				[Weak](FName Id, float Confidence, FString)
				{
					if (TSharedPtr<FMultiWorkerState, ESPMode::ThreadSafe> Current = Weak.Pin())
					{
						Current->bCallbacksOnGameThread &= IsInGameThread();
						Current->ResultId = Id;
						Current->ResultConfidence = Confidence;
						Current->bResultReady = true;
					}
				}));
	}

	FAutomationTestBase* Test;
	TSharedRef<FMultiWorkerState, ESPMode::ThreadSafe> State;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMallSpeechConcurrentTeardownTest, "MyProject2.Voice.ConcurrentSpeechTeardown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMallSpeechConcurrentTeardownTest::RunTest(const FString& Parameters)
{
#if !PLATFORM_WINDOWS
	AddInfo(TEXT("SKIP: the prototype offline speech backend currently supports Windows only."));
	return true;
#else
	using namespace MallSpeechRecognitionTests;
	const TSharedRef<FMultiWorkerState, ESPMode::ThreadSafe> State = MakeShared<FMultiWorkerState, ESPMode::ThreadSafe>();
	FString SamplesDirectory = FPaths::Combine(FPaths::ProjectDir(), TEXT("Automation/Voice/Fixtures"));
	FParse::Value(FCommandLine::Get(), TEXT("MallVoiceTestSamples="), SamplesDirectory);
	const FString Fixture = FPaths::Combine(SamplesDirectory, TEXT("come_here.wav"));
	if (!FPaths::FileExists(Fixture))
	{
		AddInfo(TEXT("SKIP: provide -MallVoiceTestSamples=<directory> containing come_here.wav for concurrent native teardown coverage."));
		return true;
	}
	if (!TestTrue(TEXT("Concurrent teardown fixture is valid PCM16 mono 16 kHz"), LoadPCMFixture(Fixture, State->Samples)))
	{
		return false;
	}
	State->Aliases.Add(TEXT("come_here"), {TEXT("come here")});
	BeginMultiWorkerRound(State);
	ADD_LATENT_AUTOMATION_COMMAND(FRunMultiWorkerTeardown(this, State));
	return true;
#endif
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMallWhisperConcurrentTeardownTest, "MyProject2.Voice.ConcurrentWhisperTeardown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMallWhisperConcurrentTeardownTest::RunTest(const FString& Parameters)
{
#if !PLATFORM_WINDOWS
	AddInfo(TEXT("SKIP: the offline Whisper runtime currently supports Windows only."));
	return true;
#else
	using namespace MallSpeechRecognitionTests;
	const TSharedRef<FMultiWorkerState, ESPMode::ThreadSafe> State = MakeShared<FMultiWorkerState, ESPMode::ThreadSafe>();
	State->bWhisper = true;
	State->RoundsToRun = 1;
	State->ExpectedPhraseId = TEXT("where_are_you");
	const FString Fixture = FPaths::Combine(FPaths::ProjectDir(), TEXT("Automation/Voice/Fixtures/Whisper/where_0.wav"));
	if (!TestTrue(TEXT("Whisper concurrent teardown fixture is valid PCM16 mono 16 kHz"),
		LoadPCMFixture(Fixture, State->Samples)))
	{
		return false;
	}
	State->Aliases.Add(State->ExpectedPhraseId, {TEXT("where are you")});
	BeginMultiWorkerRound(State);
	ADD_LATENT_AUTOMATION_COMMAND(FRunMultiWorkerTeardown(this, State));
	return true;
#endif
}
#endif
