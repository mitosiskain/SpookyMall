#pragma once

#include "CoreMinimal.h"

/** Local PCM processing only. GainDb applies only when automatic gain is off. */
struct FMallMicrophoneOptions
{
	float GainDb = 0.0f;
	bool bAutoGain = true;
	bool bNoiseFilter = true;
	/** Maximum positive gain when automatic gain is enabled; its speech target is fixed. */
	float MaxAutoGainDb = 30.0f;
};

struct FMallMicrophoneLevels
{
	float RawRMS = 0.0f;
	float ProcessedRMS = 0.0f;
	float Peak = 0.0f;
	float AppliedGainDb = 0.0f;
	bool bClipping = false;
};

/**
 * Stateful mono PCM16 / 16 kHz microphone conditioning.
 * Never removes, adds, reorders, or delays samples. State evolves per sample so
 * capture-buffer boundaries cannot alter the processed audio.
 *
 * Noise filtering means an 80 Hz high-pass and gentle expansion of the absolute
 * noise floor; it is not speech isolation or a neural denoiser. Noise above that
 * floor can still be amplified along with quiet speech.
 */
class MYPROJECT2_API FMallMicrophoneProcessor
{
public:
	void Reset();
	FMallMicrophoneLevels Process(TArray<int16>& Samples, const FMallMicrophoneOptions& Options);

private:
	float PreviousInput = 0.0f;
	float PreviousHighPassOutput = 0.0f;
	float DetectorPower = 0.0f;
	float CurrentGainDb = 0.0f;
	int32 InactiveSamples = 0;
	int32 GainAcquisitionSamples = 0;
};
