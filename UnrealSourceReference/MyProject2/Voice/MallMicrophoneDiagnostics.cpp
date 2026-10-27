#include "Voice/MallMicrophoneDiagnostics.h"

#include "Voice/MallVoiceComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

DEFINE_LOG_CATEGORY_STATIC(LogMallMicrophoneDiagnostics, Log, All);

namespace MallMicrophoneDiagnostics
{
	constexpr int32 SampleRate = 16000;
	constexpr int32 MaximumSamples = SampleRate * 8;
	constexpr int32 MaximumContexts = 4;
	constexpr int32 MaximumOptionSnapshots = 32;

	struct FRecording
	{
		bool bArmed = false;
		bool bActive = false;
		bool bTruncated = false;
		bool bClockSeen = false;
		bool bOptionsTruncated = false;
		int32 PendingCount = 0;
		int32 ChunkCount = 0;
		int32 ClockChunkCount = 0;
		int32 ClockGapCount = 0;
		int32 ClockOverlapCount = 0;
		int32 ClippingChunkCount = 0;
		uint64 ExpectedClock = 0;
		uint64 MissingClockSamples = 0;
		uint64 OverlapClockSamples = 0;
		double StartTime = 0.;
		double LastChunkTime = 0.;
		double MaximumChunkGap = 0.;
		double MinimumAppliedGainDb = 0.;
		double MaximumAppliedGainDb = 0.;
		FString StartedUtc;
		FString Status;
		TArray<int16> Raw;
		TArray<int16> Processed;
		TArray<int16> PendingRaw;
		TArray<TSharedPtr<FJsonValue>> OptionSnapshots;
		FMallMicrophoneOptions LastOptions;
	};

	TMap<TWeakObjectPtr<UMallVoiceComponent>, FRecording> Recordings;

	void Prune()
	{
		for (auto It = Recordings.CreateIterator(); It; ++It)
		{
			if (!It.Key().IsValid()) { It.RemoveCurrent(); }
		}
	}

	FRecording* Find(UMallVoiceComponent* Component)
	{
		check(IsInGameThread());
		Prune();
		return IsValid(Component) ? Recordings.Find(Component) : nullptr;
	}

	void DiscardAudio(FRecording& Recording)
	{
		Recording.bArmed = false;
		Recording.bActive = false;
		Recording.Raw.Empty();
		Recording.Processed.Empty();
		Recording.PendingRaw.Empty();
		Recording.PendingCount = 0;
		Recording.OptionSnapshots.Empty();
	}

	void Fail(FRecording& Recording, const FString& Message)
	{
		DiscardAudio(Recording);
		Recording.Status = TEXT("Mic diagnostic failed: ") + Message;
		UE_LOG(LogMallMicrophoneDiagnostics, Warning, TEXT("%s"), *Recording.Status);
	}

	void Append16(TArray<uint8>& Bytes, uint16 Value)
	{
		Bytes.Add(static_cast<uint8>(Value));
		Bytes.Add(static_cast<uint8>(Value >> 8));
	}

	void Append32(TArray<uint8>& Bytes, uint32 Value)
	{
		Append16(Bytes, static_cast<uint16>(Value));
		Append16(Bytes, static_cast<uint16>(Value >> 16));
	}

	TArray<uint8> MakeWave(TConstArrayView<int16> Samples)
	{
		TArray<uint8> Bytes;
		Bytes.Reserve(44 + Samples.Num() * 2);
		Bytes.Append(reinterpret_cast<const uint8*>("RIFF"), 4);
		Append32(Bytes, 36 + Samples.Num() * 2);
		Bytes.Append(reinterpret_cast<const uint8*>("WAVEfmt "), 8);
		Append32(Bytes, 16);
		Append16(Bytes, 1); // Integer PCM.
		Append16(Bytes, 1); // Mono.
		Append32(Bytes, SampleRate);
		Append32(Bytes, SampleRate * 2);
		Append16(Bytes, 2);
		Append16(Bytes, 16);
		Bytes.Append(reinterpret_cast<const uint8*>("data"), 4);
		Append32(Bytes, Samples.Num() * 2);
		for (int16 Sample : Samples) { Append16(Bytes, static_cast<uint16>(Sample)); }
		return Bytes;
	}

	TSharedRef<FJsonObject> AudioMetrics(TConstArrayView<int16> Samples, double ClipThreshold)
	{
		double Energy = 0.;
		double Peak = 0.;
		int32 ClipCount = 0;
		for (int16 Sample : Samples)
		{
			const double Value = static_cast<double>(Sample) / 32768.;
			Energy += Value * Value;
			Peak = FMath::Max(Peak, FMath::Abs(Value));
			ClipCount += FMath::Abs(Value) >= ClipThreshold ? 1 : 0;
		}
		TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetNumberField(TEXT("samples"), Samples.Num());
		Result->SetNumberField(TEXT("seconds"), static_cast<double>(Samples.Num()) / SampleRate);
		Result->SetNumberField(TEXT("rms"), Samples.IsEmpty() ? 0. : FMath::Sqrt(Energy / Samples.Num()));
		Result->SetNumberField(TEXT("peak"), Peak);
		Result->SetNumberField(TEXT("clip_threshold"), ClipThreshold);
		Result->SetNumberField(TEXT("clip_fraction"), Samples.IsEmpty() ? 0. : static_cast<double>(ClipCount) / Samples.Num());
		return Result;
	}

	bool SameOptions(const FMallMicrophoneOptions& A, const FMallMicrophoneOptions& B)
	{
		return A.GainDb == B.GainDb && A.bAutoGain == B.bAutoGain
			&& A.bNoiseFilter == B.bNoiseFilter && A.MaxAutoGainDb == B.MaxAutoGainDb;
	}

	TSharedRef<FJsonObject> OptionsJson(const FMallMicrophoneOptions& Options, int32 SampleOffset)
	{
		TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetNumberField(TEXT("sample_offset"), SampleOffset);
		Result->SetNumberField(TEXT("gain_db"), FMath::IsFinite(Options.GainDb) ? Options.GainDb : 0.f);
		Result->SetBoolField(TEXT("auto_gain"), Options.bAutoGain);
		Result->SetBoolField(TEXT("noise_filter"), Options.bNoiseFilter);
		Result->SetNumberField(TEXT("max_auto_gain_db"), FMath::IsFinite(Options.MaxAutoGainDb) ? Options.MaxAutoGainDb : 0.f);
		return Result;
	}

	UMallVoiceComponent* LocalVoice(UWorld* World)
	{
		if (!World || !World->IsGameWorld()) { return nullptr; }
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* Controller = It->Get();
			if (Controller && Controller->IsLocalController())
			{
				if (UMallVoiceComponent* Voice = Controller->FindComponentByClass<UMallVoiceComponent>()) { return Voice; }
			}
		}
		return nullptr;
	}

#if !UE_BUILD_SHIPPING
	FAutoConsoleCommandWithWorld RecordNextCommand(TEXT("mall.MicRecordNext"),
		TEXT("Explicitly save the next V turn locally (at most 8 seconds), raw/processed WAV and metrics. No upload."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (UMallVoiceComponent* Voice = LocalVoice(World)) { FMallMicrophoneDiagnostics::ArmNextTurn(Voice); }
			else { UE_LOG(LogMallMicrophoneDiagnostics, Warning, TEXT("Mic diagnostic needs a local player in PIE or a running game.")); }
		}));
	FAutoConsoleCommandWithWorld RecordCancelCommand(TEXT("mall.MicRecordCancel"),
		TEXT("Discard the armed or active local microphone diagnostic without saving audio."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (UMallVoiceComponent* Voice = LocalVoice(World)) { FMallMicrophoneDiagnostics::Cancel(Voice); }
		}));
#endif
}

bool FMallMicrophoneDiagnostics::ArmNextTurn(UMallVoiceComponent* Component)
{
	using namespace MallMicrophoneDiagnostics;
	check(IsInGameThread());
	Prune();
	if (!IsValid(Component)) { return false; }
	if (Component->IsPushToTalkActive())
	{
		UE_LOG(LogMallMicrophoneDiagnostics, Warning, TEXT("Release V before arming the next local mic diagnostic."));
		return false;
	}
	FRecording* Existing = Recordings.Find(Component);
	if (Existing && Existing->bActive) { return false; }
	if (!Existing && Recordings.Num() >= MaximumContexts)
	{
		for (auto It = Recordings.CreateIterator(); It; ++It)
		{
			if (!It.Value().bArmed && !It.Value().bActive) { It.RemoveCurrent(); break; }
		}
		if (Recordings.Num() >= MaximumContexts)
		{
			UE_LOG(LogMallMicrophoneDiagnostics, Warning, TEXT("Four mic diagnostics are already armed or active; cancel one first."));
			return false;
		}
	}
	FRecording& Recording = Recordings.FindOrAdd(Component);
	Recording = FRecording();
	Recording.bArmed = true;
	Recording.Status = TEXT("Mic diagnostic armed: next V press only, up to 8 seconds, saved locally; no upload.");
	UE_LOG(LogMallMicrophoneDiagnostics, Display,
		TEXT("Mic diagnostic armed: saves the next V press only locally, up to 8 seconds, as raw.wav, processed.wav and metadata.json under Saved/VoiceDiagnostics. No upload. Use mall.MicRecordCancel to discard."));
	return true;
}

void FMallMicrophoneDiagnostics::BeginTurn(UMallVoiceComponent* Component)
{
	using namespace MallMicrophoneDiagnostics;
	FRecording* Recording = Find(Component);
	if (!Recording || !Recording->bArmed || Recording->bActive) { return; }
	Recording->bArmed = false;
	Recording->bActive = true;
	Recording->StartTime = FPlatformTime::Seconds();
	Recording->StartedUtc = FDateTime::UtcNow().ToIso8601();
	Recording->Status = TEXT("Recording this V turn locally (first 8 seconds). mall.MicRecordCancel discards it.");
}

void FMallMicrophoneDiagnostics::RecordRaw(UMallVoiceComponent* Component, TConstArrayView<int16> Samples)
{
	using namespace MallMicrophoneDiagnostics;
	FRecording* Recording = Find(Component);
	if (!Recording || !Recording->bActive || Samples.IsEmpty()) { return; }
	if (Recording->PendingCount != 0)
	{
		Fail(*Recording, TEXT("unpaired raw capture chunk; audio discarded."));
		return;
	}
	const double Now = FPlatformTime::Seconds();
	if (Recording->ChunkCount > 0) { Recording->MaximumChunkGap = FMath::Max(Recording->MaximumChunkGap, Now - Recording->LastChunkTime); }
	Recording->LastChunkTime = Now;
	Recording->PendingCount = Samples.Num();
	const int32 Count = FMath::Min(Samples.Num(), MaximumSamples - Recording->Raw.Num());
	Recording->PendingRaw.Append(Samples.GetData(), Count);
	Recording->bTruncated |= Count != Samples.Num();
}

void FMallMicrophoneDiagnostics::RecordProcessed(UMallVoiceComponent* Component, TConstArrayView<int16> Samples,
	const FMallMicrophoneOptions& Options, const FMallMicrophoneLevels& Levels)
{
	using namespace MallMicrophoneDiagnostics;
	FRecording* Recording = Find(Component);
	if (!Recording || !Recording->bActive || Samples.IsEmpty()) { return; }
	if (Recording->PendingCount != Samples.Num())
	{
		Fail(*Recording, TEXT("raw and processed sample counts differ; audio discarded."));
		return;
	}
	const int32 Count = Recording->PendingRaw.Num();
	if (Count > 0)
	{
		if (Recording->OptionSnapshots.IsEmpty() || !SameOptions(Options, Recording->LastOptions))
		{
			if (Recording->OptionSnapshots.Num() < MaximumOptionSnapshots)
			{
				Recording->OptionSnapshots.Add(MakeShared<FJsonValueObject>(OptionsJson(Options, Recording->Raw.Num())));
			}
			else { Recording->bOptionsTruncated = true; }
			Recording->LastOptions = Options;
		}
		Recording->Raw.Append(Recording->PendingRaw);
		Recording->Processed.Append(Samples.GetData(), Count);
		if (FMath::IsFinite(Levels.AppliedGainDb))
		{
			if (Recording->ChunkCount == 0)
			{
				Recording->MinimumAppliedGainDb = Recording->MaximumAppliedGainDb = Levels.AppliedGainDb;
			}
			else
			{
				Recording->MinimumAppliedGainDb = FMath::Min(Recording->MinimumAppliedGainDb, static_cast<double>(Levels.AppliedGainDb));
				Recording->MaximumAppliedGainDb = FMath::Max(Recording->MaximumAppliedGainDb, static_cast<double>(Levels.AppliedGainDb));
			}
		}
		Recording->ClippingChunkCount += Levels.bClipping ? 1 : 0;
	}
	++Recording->ChunkCount;
	Recording->PendingRaw.Reset();
	Recording->PendingCount = 0;
}

void FMallMicrophoneDiagnostics::RecordSampleClock(UMallVoiceComponent* Component, uint64 FirstClock, int32 Count)
{
	using namespace MallMicrophoneDiagnostics;
	FRecording* Recording = Find(Component);
	if (!Recording || !Recording->bActive || Count <= 0 || Recording->Raw.Num() >= MaximumSamples) { return; }
	if (Recording->bClockSeen)
	{
		if (FirstClock > Recording->ExpectedClock)
		{
			++Recording->ClockGapCount;
			Recording->MissingClockSamples += FirstClock - Recording->ExpectedClock;
		}
		else if (FirstClock < Recording->ExpectedClock)
		{
			++Recording->ClockOverlapCount;
			Recording->OverlapClockSamples += Recording->ExpectedClock - FirstClock;
		}
	}
	Recording->bClockSeen = true;
	Recording->ExpectedClock = FirstClock + static_cast<uint64>(Count);
	++Recording->ClockChunkCount;
}

FString FMallMicrophoneDiagnostics::EndTurn(UMallVoiceComponent* Component, const FString& Reason)
{
	using namespace MallMicrophoneDiagnostics;
	FRecording* Recording = Find(Component);
	if (!Recording || !Recording->bActive) { return FString(); }
	if (Recording->PendingCount != 0 || Recording->Raw.Num() != Recording->Processed.Num())
	{
		Fail(*Recording, TEXT("unfinished capture pair; audio discarded."));
		return FString();
	}
	if (Recording->Raw.IsEmpty())
	{
		DiscardAudio(*Recording);
		Recording->Status = TEXT("Mic diagnostic finished without audio; nothing saved.");
		return FString();
	}
	const FString Directory = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("VoiceDiagnostics"),
		FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ")) + TEXT("_") + FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	TSharedRef<FJsonObject> Metadata = MakeShared<FJsonObject>();
	Metadata->SetNumberField(TEXT("schema_version"), 1);
	Metadata->SetStringField(TEXT("format"), TEXT("PCM16 little-endian"));
	Metadata->SetNumberField(TEXT("sample_rate_hz"), SampleRate);
	Metadata->SetNumberField(TEXT("channels"), 1);
	Metadata->SetStringField(TEXT("started_utc"), Recording->StartedUtc);
	Metadata->SetStringField(TEXT("finished_utc"), FDateTime::UtcNow().ToIso8601());
	Metadata->SetStringField(TEXT("stop_reason"), Reason.Left(64));
	Metadata->SetNumberField(TEXT("ptt_wall_seconds"), FMath::Max(0., FPlatformTime::Seconds() - Recording->StartTime));
	Metadata->SetNumberField(TEXT("max_chunk_gap_seconds"), Recording->MaximumChunkGap);
	Metadata->SetNumberField(TEXT("chunk_count"), Recording->ChunkCount);
	Metadata->SetBoolField(TEXT("truncated"), Recording->bTruncated);
	Metadata->SetNumberField(TEXT("max_recorded_seconds"), 8);
	Metadata->SetObjectField(TEXT("raw"), AudioMetrics(Recording->Raw, .98));
	Metadata->SetObjectField(TEXT("processed"), AudioMetrics(Recording->Processed, .969));
	Metadata->SetNumberField(TEXT("clipping_chunk_count"), Recording->ClippingChunkCount);
	Metadata->SetNumberField(TEXT("min_applied_gain_db"), Recording->MinimumAppliedGainDb);
	Metadata->SetNumberField(TEXT("max_applied_gain_db"), Recording->MaximumAppliedGainDb);
	Metadata->SetArrayField(TEXT("options"), Recording->OptionSnapshots);
	Metadata->SetBoolField(TEXT("options_truncated"), Recording->bOptionsTruncated);
	Metadata->SetBoolField(TEXT("sample_clock_available"), Recording->bClockSeen);
	Metadata->SetNumberField(TEXT("sample_clock_chunk_count"), Recording->ClockChunkCount);
	Metadata->SetNumberField(TEXT("sample_clock_gap_count"), Recording->ClockGapCount);
	Metadata->SetNumberField(TEXT("sample_clock_missing_samples"), static_cast<double>(Recording->MissingClockSamples));
	Metadata->SetNumberField(TEXT("sample_clock_overlap_count"), Recording->ClockOverlapCount);
	Metadata->SetNumberField(TEXT("sample_clock_overlap_samples"), static_cast<double>(Recording->OverlapClockSamples));
	FString Json;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
	const bool bSerialized = FJsonSerializer::Serialize(Metadata, Writer);
	const TArray<uint8> RawWave = MakeWave(Recording->Raw);
	const TArray<uint8> ProcessedWave = MakeWave(Recording->Processed);
	const FString RawPath = FPaths::Combine(Directory, TEXT("raw.wav"));
	const FString ProcessedPath = FPaths::Combine(Directory, TEXT("processed.wav"));
	const FString MetadataPath = FPaths::Combine(Directory, TEXT("metadata.json"));
	const bool bSaved = bSerialized && IFileManager::Get().MakeDirectory(*Directory, true)
		&& FFileHelper::SaveArrayToFile(RawWave, *RawPath)
		&& FFileHelper::SaveArrayToFile(ProcessedWave, *ProcessedPath)
		&& FFileHelper::SaveStringToFile(Json, *MetadataPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	if (!bSaved)
	{
		// These are only the three just-created files; never recursively delete a directory.
		IFileManager::Get().Delete(*RawPath, false, true);
		IFileManager::Get().Delete(*ProcessedPath, false, true);
		IFileManager::Get().Delete(*MetadataPath, false, true);
		IFileManager::Get().DeleteDirectory(*Directory, false, false);
		Fail(*Recording, TEXT("could not save the local diagnostic files; audio discarded."));
		return FString();
	}
	DiscardAudio(*Recording);
	Recording->Status = TEXT("Mic diagnostic saved locally: ") + Directory;
	UE_LOG(LogMallMicrophoneDiagnostics, Display, TEXT("%s"), *Recording->Status);
	return Directory;
}

void FMallMicrophoneDiagnostics::Cancel(UMallVoiceComponent* Component)
{
	using namespace MallMicrophoneDiagnostics;
	if (FRecording* Recording = Find(Component))
	{
		DiscardAudio(*Recording);
		Recording->Status = TEXT("Mic diagnostic cancelled; buffered audio discarded without saving.");
	}
}

FString FMallMicrophoneDiagnostics::GetStatus(UMallVoiceComponent* Component)
{
	if (MallMicrophoneDiagnostics::FRecording* Recording = MallMicrophoneDiagnostics::Find(Component)) { return Recording->Status; }
	return TEXT("Mic diagnostics off. mall.MicRecordNext explicitly records one V turn locally.");
}
