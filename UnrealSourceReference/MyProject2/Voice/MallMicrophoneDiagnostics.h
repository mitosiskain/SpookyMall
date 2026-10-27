#pragma once

#include "CoreMinimal.h"
#include "Voice/MallMicrophoneProcessor.h"

class UMallVoiceComponent;

/** Explicit, local, one-turn developer recording. These helpers never open a microphone. */
class FMallMicrophoneDiagnostics final
{
public:
	/** Refuses an active push-to-talk turn. No setting or consent is persisted. */
	static bool ArmNextTurn(UMallVoiceComponent* Component);
	static void BeginTurn(UMallVoiceComponent* Component);
	static void RecordRaw(UMallVoiceComponent* Component, TConstArrayView<int16> Samples);
	static void RecordProcessed(UMallVoiceComponent* Component, TConstArrayView<int16> Samples,
		const FMallMicrophoneOptions& Options, const FMallMicrophoneLevels& Levels);
	/** FirstClock is the platform sample clock for this chunk, not a wall-clock timestamp. */
	static void RecordSampleClock(UMallVoiceComponent* Component, uint64 FirstClock, int32 Count);
	/** Returns the new local directory on success, or empty if inactive, empty or unsuccessful. */
	static FString EndTurn(UMallVoiceComponent* Component, const FString& Reason);
	static void Cancel(UMallVoiceComponent* Component);
	static FString GetStatus(UMallVoiceComponent* Component);
};
