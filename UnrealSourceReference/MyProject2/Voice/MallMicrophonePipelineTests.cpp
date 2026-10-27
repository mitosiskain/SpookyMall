#include "Voice/MallVoiceComponent.h"
#include "Voice/MallMicrophoneSettings.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMallMicrophonePipelineTest, "MyProject2.Voice.MicrophonePipeline",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMallMicrophonePipelineTest::RunTest(const FString& Parameters)
{
	// Change only in-memory user preferences and restore them without writing a profile.
	struct FRestoreMicrophoneSettings
	{
		UMallMicrophoneSettings* Settings = GetMutableDefault<UMallMicrophoneSettings>();
		float GainDb = Settings->GainDb;
		bool bAutoGain = Settings->bAutoGain;
		bool bNoiseFilter = Settings->bNoiseFilter;
		float MaxAutoGainDb = Settings->MaxAutoGainDb;
		~FRestoreMicrophoneSettings()
		{
			Settings->GainDb = GainDb;
			Settings->bAutoGain = bAutoGain;
			Settings->bNoiseFilter = bNoiseFilter;
			Settings->MaxAutoGainDb = MaxAutoGainDb;
		}
	} Restore;
	Restore.Settings->GainDb = 20.0f;
	Restore.Settings->bAutoGain = false;
	Restore.Settings->bNoiseFilter = false;

	UMallVoiceComponent* Voice = NewObject<UMallVoiceComponent>();
	if (!TestNotNull(TEXT("Ownerless test component can be created without microphone access"), Voice)) { return false; }
	TArray<int16> RecognizerPCM;
	TArray<int16> ProcessedInput;
	TArray<int16> RawInput;
	int32 CallbackCount = 0;
	bool bUnexpectedUtteranceEnd = false;
	const FDelegateHandle PCMHandle = Voice->OnLocalPCM.AddLambda([&](const TArray<int16>& Samples, bool bEnd)
	{
		++CallbackCount;
		bUnexpectedUtteranceEnd |= bEnd;
		RecognizerPCM.Append(Samples);
	});

	constexpr int32 SamplesPerChunk = 160;
	constexpr int32 ChunkCount = 100;
	for (int32 ChunkIndex = 0; ChunkIndex < ChunkCount; ++ChunkIndex)
	{
		TArray<int16> Samples;
		Samples.SetNumUninitialized(SamplesPerChunk);
		for (int32 Index = 0; Index < SamplesPerChunk; ++Index)
		{
			const int32 SampleIndex = ChunkIndex * SamplesPerChunk + Index;
			Samples[Index] = static_cast<int16>(FMath::RoundToInt(64.0 * FMath::Sin(2.0 * PI * 440.0 * SampleIndex / 16000.0)));
		}
		RawInput.Append(Samples);
		// This is the actual shared processing/dispatch path called by PollCapture.
		Voice->FeedCapturedSamples(Samples);
		ProcessedInput.Append(Samples);
	}
	const auto RMS = [](const TArray<int16>& Samples)
	{
		double Energy = 0.0;
		for (const int16 Sample : Samples)
		{
			const double Normalized = static_cast<double>(Sample) / 32768.0;
			Energy += Normalized * Normalized;
		}
		return Samples.IsEmpty() ? 0.0 : FMath::Sqrt(Energy / Samples.Num());
	};
	TestEqual(TEXT("Every captured chunk reaches the recognition delegate"), CallbackCount, ChunkCount);
	TestFalse(TEXT("Capture processing does not invent an utterance end"), bUnexpectedUtteranceEnd);
	TestEqual(TEXT("Processed sample count is unchanged"), ProcessedInput.Num(), SamplesPerChunk * ChunkCount);
	TestTrue(TEXT("Recognition delegate receives the exact processed samples"), RecognizerPCM == ProcessedInput);
	TestEqual(TEXT("Network queue retains every processed PCM16 byte"), Voice->PendingPCM.Num(), ProcessedInput.Num() * static_cast<int32>(sizeof(int16)));
	if (Voice->PendingPCM.Num() == ProcessedInput.Num() * sizeof(int16))
	{
		TestTrue(TEXT("Network queue contains the same boosted PCM received by recognition"),
			FMemory::Memcmp(Voice->PendingPCM.GetData(), ProcessedInput.GetData(), Voice->PendingPCM.Num()) == 0);
	}
	TestTrue(TEXT("Quiet source PCM is below the local recognition activity threshold"), RMS(RawInput) < 0.006);
	TestTrue(TEXT("Actual recognition PCM has been boosted above the activity threshold"), RMS(RecognizerPCM) > 0.006);
	TestTrue(TEXT("Raw meter remains distinct from processed level"), Voice->RawMicrophoneRMS < 0.006f && Voice->MicrophoneRMS > 0.006f);
	TestTrue(TEXT("Applied gain reports the selected constant boost"), FMath::IsNearlyEqual(Voice->AppliedMicrophoneGainDb, 20.0f, 0.01f));
	TestFalse(TEXT("Pipeline test never created a capture object"), Voice->Capture.IsValid());
	TestFalse(TEXT("Pipeline test never created an encoder"), Voice->Encoder.IsValid());
	Voice->OnLocalPCM.Remove(PCMHandle);

	TArray<int16> LoudSamples;
	LoudSamples.Init(30000, SamplesPerChunk);
	Voice->FeedCapturedSamples(LoudSamples);
	TestTrue(TEXT("Limiter engagement raises the actual component clipping indicator"), Voice->bMicrophoneClipping);
	TestTrue(TEXT("Clipping has a positive display hold time"), Voice->MicrophoneClipRemaining > 0.0f);
	Voice->UpdateMicrophoneMeter(0.4f);
	TestTrue(TEXT("Brief clipping remains visible during its hold"), Voice->bMicrophoneClipping);
	Voice->UpdateMicrophoneMeter(0.61f);
	TestFalse(TEXT("Clipping indicator expires by elapsed time"), Voice->bMicrophoneClipping);
	TestEqual(TEXT("Expired clipping timer reaches zero"), Voice->MicrophoneClipRemaining, 0.0f);

	Voice->ResetMicrophoneMeter();
	Voice->MicrophoneRMS = 0.2f;
	Voice->UpdateMicrophoneMeter(0.1f);
	TestTrue(TEXT("Meter holds its level long enough for the 10 Hz HUD"), FMath::IsNearlyEqual(Voice->MicrophoneRMS, 0.2f, 0.000001f));
	Voice->UpdateMicrophoneMeter(0.04f);
	TestTrue(TEXT("Meter begins decaying after the hold"), Voice->MicrophoneRMS > 0.0f && Voice->MicrophoneRMS < 0.2f);

	Voice->ResetMicrophoneMeter();
	Voice->MicrophoneRMS = 0.2f;
	for (int32 Frame = 0; Frame < 30; ++Frame) { Voice->UpdateMicrophoneMeter(1.0f / 30.0f); }
	const float At30FPS = Voice->MicrophoneRMS;
	Voice->ResetMicrophoneMeter();
	Voice->MicrophoneRMS = 0.2f;
	for (int32 Frame = 0; Frame < 120; ++Frame) { Voice->UpdateMicrophoneMeter(1.0f / 120.0f); }
	const float At120FPS = Voice->MicrophoneRMS;
	TestTrue(TEXT("Equal elapsed time produces equivalent meter decay at 30 and 120 FPS"), FMath::IsNearlyEqual(At30FPS, At120FPS, 0.00001f));
	TestTrue(TEXT("One-second level has decayed rather than remaining frozen"), At30FPS > 0.0f && At30FPS < 0.02f);

	// A new PTT press follows a loud prior turn without receiving any samples
	// while the microphone is closed. It must acquire quiet speech as a fresh turn.
	Restore.Settings->GainDb = 19.705271f;
	Restore.Settings->bAutoGain = true;
	Restore.Settings->bNoiseFilter = true;
	TArray<int16> PriorLoudTurn;
	PriorLoudTurn.SetNumUninitialized(8000);
	for (int32 Index = 0; Index < PriorLoudTurn.Num(); ++Index)
	{
		PriorLoudTurn[Index] = static_cast<int16>(24000.0 * FMath::Sin(2.0 * PI * 240.0 * Index / 16000.0));
	}
	Voice->FeedCapturedSamples(PriorLoudTurn);
	Voice->BeginMicrophoneTurn();
	TestTrue(TEXT("Starting a new turn discards previous chat queue"), Voice->PendingPCM.IsEmpty());
	TArray<int16> NextQuietTurn;
	NextQuietTurn.SetNumUninitialized(4800);
	for (int32 Index = 0; Index < NextQuietTurn.Num(); ++Index)
	{
		NextQuietTurn[Index] = static_cast<int16>(18.0 * FMath::Sin(2.0 * PI * 320.0 * Index / 16000.0));
	}
	TArray<int16> ExpectedQuietTurn = NextQuietTurn;
	FMallMicrophoneProcessor FreshProcessor;
	FreshProcessor.Process(ExpectedQuietTurn, Restore.Settings->GetOptions());
	Voice->FeedCapturedSamples(NextQuietTurn);
	TestTrue(TEXT("Quiet turn after a loud PTT matches fresh processing sample for sample"), NextQuietTurn == ExpectedQuietTurn);
	TestTrue(TEXT("Quiet turn crosses activity threshold without stale gain history"), RMS(NextQuietTurn) > .006);
	TestFalse(TEXT("Turn reset regression never opens microphone hardware"), Voice->Capture.IsValid());

	// Disabled cleanup is exercised without a controller/RPC because PTT was never opened.
	Voice->RawMicrophoneRMS = 0.01f;
	Voice->AppliedMicrophoneGainDb = 20.0f;
	Voice->MicrophoneClipRemaining = 1.0f;
	Voice->bMicrophoneClipping = true;
	Voice->StopLocalVoiceForDisable();
	TestTrue(TEXT("Disabling discards queued network PCM"), Voice->PendingPCM.IsEmpty());
	TestEqual(TEXT("Disabling clears processed meter"), Voice->MicrophoneRMS, 0.0f);
	TestEqual(TEXT("Disabling clears raw meter"), Voice->RawMicrophoneRMS, 0.0f);
	TestEqual(TEXT("Disabling clears applied gain display"), Voice->AppliedMicrophoneGainDb, 0.0f);
	TestFalse(TEXT("Disabling clears clipping indicator"), Voice->bMicrophoneClipping);
	TestEqual(TEXT("Disabling clears meter hold age"), Voice->MicrophonePeakAge, 0.0f);
	TestEqual(TEXT("Disabling clears clipping hold"), Voice->MicrophoneClipRemaining, 0.0f);
	TestFalse(TEXT("Disabling leaves PTT inactive"), Voice->IsPushToTalkActive());
	return true;
}

#endif
