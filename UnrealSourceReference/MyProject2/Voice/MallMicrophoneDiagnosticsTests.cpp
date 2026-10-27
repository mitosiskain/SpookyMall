#include "Voice/MallMicrophoneDiagnostics.h"
#include "Voice/MallVoiceComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMallMicrophoneDiagnosticsTest, "MyProject2.Voice.MicrophoneDiagnostics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMallMicrophoneDiagnosticsTest::RunTest(const FString& Parameters)
{
	// Synthetic arrays only: no controller, device, playback or speech decoder is created.
	UMallVoiceComponent* Voice = NewObject<UMallVoiceComponent>();
	if (!TestNotNull(TEXT("Ownerless diagnostic test component exists"), Voice)) { return false; }
	TArray<FString> SavedDirectories;
	const auto CleanSavedFiles = [&SavedDirectories]()
	{
		const FString Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("VoiceDiagnostics"))) + TEXT("/");
		for (FString Directory : SavedDirectories)
		{
			FPaths::NormalizeDirectoryName(Directory);
			if (!Directory.StartsWith(Root)) { continue; }
			for (const TCHAR* Name : {TEXT("raw.wav"), TEXT("processed.wav"), TEXT("metadata.json")})
			{
				IFileManager::Get().Delete(*FPaths::Combine(Directory, Name), false, true);
			}
			IFileManager::Get().DeleteDirectory(*Directory, false, false);
		}
	};
	struct FCleanup
	{
		TFunction<void()> Function;
		UMallVoiceComponent* Voice;
		~FCleanup() { FMallMicrophoneDiagnostics::Cancel(Voice); Function(); }
	} Cleanup{CleanSavedFiles, Voice};
	const auto Read16 = [](const TArray<uint8>& Bytes, int32 Offset)
	{
		return static_cast<uint16>(Bytes[Offset]) | static_cast<uint16>(Bytes[Offset + 1]) << 8;
	};
	const auto Read32 = [&Read16](const TArray<uint8>& Bytes, int32 Offset)
	{
		return static_cast<uint32>(Read16(Bytes, Offset)) | static_cast<uint32>(Read16(Bytes, Offset + 2)) << 16;
	};
	const auto ReadMetadata = [this](const FString& Directory)
	{
		FString Text;
		TSharedPtr<FJsonObject> Metadata;
		TestTrue(TEXT("Metadata file is readable"), FFileHelper::LoadFileToString(Text, *FPaths::Combine(Directory, TEXT("metadata.json"))));
		TestTrue(TEXT("Metadata is valid JSON"), FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Metadata));
		return Metadata;
	};
	FMallMicrophoneOptions Options;
	Options.GainDb = 6.f;
	Options.bAutoGain = false;
	Options.bNoiseFilter = false;
	FMallMicrophoneLevels Levels;
	Levels.AppliedGainDb = 6.f;
	TArray<int16> Raw = {0, 16384, -16384, 32767};
	TArray<int16> Processed = {0, 20000, -20000, 31784};
	FMallMicrophoneDiagnostics::BeginTurn(Voice);
	FMallMicrophoneDiagnostics::RecordRaw(Voice, Raw);
	FMallMicrophoneDiagnostics::RecordProcessed(Voice, Processed, Options, Levels);
	TestTrue(TEXT("Default-off helper does not record or save"), FMallMicrophoneDiagnostics::EndTurn(Voice, TEXT("Unarmed")).IsEmpty());
	TestTrue(TEXT("Explicit arm succeeds without accessing a microphone"), FMallMicrophoneDiagnostics::ArmNextTurn(Voice));
	FMallMicrophoneDiagnostics::BeginTurn(Voice);
	TestFalse(TEXT("An active diagnostic cannot be rearmed"), FMallMicrophoneDiagnostics::ArmNextTurn(Voice));
	FMallMicrophoneDiagnostics::RecordSampleClock(Voice, 100, Raw.Num());
	FMallMicrophoneDiagnostics::RecordRaw(Voice, Raw);
	FMallMicrophoneDiagnostics::RecordProcessed(Voice, Processed, Options, Levels);
	FMallMicrophoneDiagnostics::RecordSampleClock(Voice, 106, Raw.Num());
	FMallMicrophoneDiagnostics::RecordRaw(Voice, Raw);
	FMallMicrophoneDiagnostics::RecordProcessed(Voice, Processed, Options, Levels);
	const FString Directory = FMallMicrophoneDiagnostics::EndTurn(Voice, TEXT("SyntheticTestRelease"));
	if (!TestFalse(TEXT("Paired diagnostic saved a directory"), Directory.IsEmpty())) { return false; }
	SavedDirectories.Add(Directory);
	TArray<uint8> RawWave;
	TArray<uint8> ProcessedWave;
	TestTrue(TEXT("Raw WAV is readable"), FFileHelper::LoadFileToArray(RawWave, *FPaths::Combine(Directory, TEXT("raw.wav"))));
	TestTrue(TEXT("Processed WAV is readable"), FFileHelper::LoadFileToArray(ProcessedWave, *FPaths::Combine(Directory, TEXT("processed.wav"))));
	TestEqual(TEXT("WAV contains a standard 44 byte header and every sample"), RawWave.Num(), 44 + Raw.Num() * 2 * 2);
	TestEqual(TEXT("Paired raw/processed WAV sizes match"), ProcessedWave.Num(), RawWave.Num());
	if (RawWave.Num() >= 44 && ProcessedWave.Num() == RawWave.Num())
	{
		TestTrue(TEXT("WAV starts RIFF"), FMemory::Memcmp(RawWave.GetData(), "RIFF", 4) == 0);
		TestTrue(TEXT("WAV has WAVE format"), FMemory::Memcmp(RawWave.GetData() + 8, "WAVE", 4) == 0);
		TestEqual(TEXT("WAV RIFF size is exact"), Read32(RawWave, 4), static_cast<uint32>(RawWave.Num() - 8));
		TestEqual(TEXT("WAV integer PCM format"), static_cast<uint32>(Read16(RawWave, 20)), 1u);
		TestEqual(TEXT("WAV is mono"), static_cast<uint32>(Read16(RawWave, 22)), 1u);
		TestEqual(TEXT("WAV sample rate is 16 kHz"), Read32(RawWave, 24), 16000u);
		TestEqual(TEXT("WAV uses 16 bits"), static_cast<uint32>(Read16(RawWave, 34)), 16u);
		TestEqual(TEXT("WAV data size matches paired chunks"), Read32(RawWave, 40), static_cast<uint32>(Raw.Num() * 4));
		for (int32 Index = 0; Index < Raw.Num() * 2; ++Index)
		{
			TestEqual(TEXT("Raw WAV preserves signed sample and chunk order"), static_cast<int16>(Read16(RawWave, 44 + Index * 2)), Raw[Index % Raw.Num()]);
			TestEqual(TEXT("Processed WAV preserves its own distinct signed sample and chunk order"), static_cast<int16>(Read16(ProcessedWave, 44 + Index * 2)), Processed[Index % Processed.Num()]);
		}
	}
	if (const TSharedPtr<FJsonObject> Metadata = ReadMetadata(Directory))
	{
		TestEqual(TEXT("Two paired chunks are counted"), Metadata->GetNumberField(TEXT("chunk_count")), 2.);
		TestEqual(TEXT("Capture clock gap is counted"), Metadata->GetNumberField(TEXT("sample_clock_gap_count")), 1.);
		TestEqual(TEXT("Capture clock missing samples are exact"), Metadata->GetNumberField(TEXT("sample_clock_missing_samples")), 2.);
		TestEqual(TEXT("Short turn is not truncated"), Metadata->GetBoolField(TEXT("truncated")), false);
		TestEqual(TEXT("Raw near-full-scale sample is reflected in clip fraction"), Metadata->GetObjectField(TEXT("raw"))->GetNumberField(TEXT("clip_fraction")), .25);
		TestEqual(TEXT("Raw sample count matches WAV"), Metadata->GetObjectField(TEXT("raw"))->GetNumberField(TEXT("samples")), 8.);
		const double FullScaleSample = 32767. / 32768.;
		TestTrue(TEXT("Raw RMS is calculated from the recorded PCM, not the display meter"),
			FMath::IsNearlyEqual(Metadata->GetObjectField(TEXT("raw"))->GetNumberField(TEXT("rms")), FMath::Sqrt((.5 + FullScaleSample * FullScaleSample) / 4.), 1.e-9));
		TestEqual(TEXT("Raw peak is calculated from exact recorded PCM"), Metadata->GetObjectField(TEXT("raw"))->GetNumberField(TEXT("peak")), FullScaleSample);
		TestEqual(TEXT("Processed limiting sample has its own clip fraction"), Metadata->GetObjectField(TEXT("processed"))->GetNumberField(TEXT("clip_fraction")), .25);
		TestEqual(TEXT("User gain is recorded"), Metadata->GetArrayField(TEXT("options"))[0]->AsObject()->GetNumberField(TEXT("gain_db")), 6.);
		TestEqual(TEXT("Stop reason is recorded"), Metadata->GetStringField(TEXT("stop_reason")), FString(TEXT("SyntheticTestRelease")));
		TestTrue(TEXT("Timing metadata is nonnegative"), Metadata->GetNumberField(TEXT("ptt_wall_seconds")) >= 0. && Metadata->GetNumberField(TEXT("max_chunk_gap_seconds")) >= 0.);
	}
	FMallMicrophoneDiagnostics::BeginTurn(Voice);
	FMallMicrophoneDiagnostics::RecordRaw(Voice, Raw);
	FMallMicrophoneDiagnostics::RecordProcessed(Voice, Processed, Options, Levels);
	TestTrue(TEXT("Consent is consumed by one turn and does not rearm"), FMallMicrophoneDiagnostics::EndTurn(Voice, TEXT("SecondTurn")).IsEmpty());
	TestTrue(TEXT("Rearm for cancellation succeeds"), FMallMicrophoneDiagnostics::ArmNextTurn(Voice));
	FMallMicrophoneDiagnostics::BeginTurn(Voice);
	FMallMicrophoneDiagnostics::RecordRaw(Voice, Raw);
	FMallMicrophoneDiagnostics::RecordProcessed(Voice, Processed, Options, Levels);
	FMallMicrophoneDiagnostics::Cancel(Voice);
	TestTrue(TEXT("Cancellation discards the active audio without a save"), FMallMicrophoneDiagnostics::EndTurn(Voice, TEXT("Cancelled")).IsEmpty());
	TestTrue(TEXT("Cancellation status explains discard"), FMallMicrophoneDiagnostics::GetStatus(Voice).Contains(TEXT("discarded")));
	TestTrue(TEXT("An unused arm can be explicitly cancelled"), FMallMicrophoneDiagnostics::ArmNextTurn(Voice));
	FMallMicrophoneDiagnostics::Cancel(Voice);
	FMallMicrophoneDiagnostics::BeginTurn(Voice);
	FMallMicrophoneDiagnostics::RecordRaw(Voice, Raw);
	FMallMicrophoneDiagnostics::RecordProcessed(Voice, Processed, Options, Levels);
	TestTrue(TEXT("Cancelling an arm prevents the next turn being recorded"), FMallMicrophoneDiagnostics::EndTurn(Voice, TEXT("CancelledArm")).IsEmpty());
	TestTrue(TEXT("Rearm for bounded recording succeeds"), FMallMicrophoneDiagnostics::ArmNextTurn(Voice));
	FMallMicrophoneDiagnostics::BeginTurn(Voice);
	TArray<int16> LongRaw;
	LongRaw.Init(123, 128123);
	TArray<int16> LongProcessed;
	LongProcessed.Init(-321, LongRaw.Num());
	FMallMicrophoneDiagnostics::RecordRaw(Voice, LongRaw);
	FMallMicrophoneDiagnostics::RecordProcessed(Voice, LongProcessed, Options, Levels);
	const FString CappedDirectory = FMallMicrophoneDiagnostics::EndTurn(Voice, TEXT("CapTest"));
	if (!TestFalse(TEXT("Capped synthetic recording is saved"), CappedDirectory.IsEmpty())) { return false; }
	SavedDirectories.Add(CappedDirectory);
	TArray<uint8> CappedWave;
	TestTrue(TEXT("Capped WAV can be read"), FFileHelper::LoadFileToArray(CappedWave, *FPaths::Combine(CappedDirectory, TEXT("raw.wav"))));
	TestEqual(TEXT("Recorded memory and WAV are capped at exactly eight seconds"), CappedWave.Num(), 44 + 128000 * 2);
	if (const TSharedPtr<FJsonObject> Metadata = ReadMetadata(CappedDirectory))
	{
		TestTrue(TEXT("Oversize turn reports truncation"), Metadata->GetBoolField(TEXT("truncated")));
		TestEqual(TEXT("Capped turn reports exact duration"), Metadata->GetObjectField(TEXT("raw"))->GetNumberField(TEXT("seconds")), 8.);
	}
	TestFalse(TEXT("Ownerless diagnostics never changed push-to-talk state"), Voice->IsPushToTalkActive());
	return true;
}

#endif
