#include "Voice/MallMicrophoneProcessor.h"

namespace MallMicrophone
{
	constexpr float SampleRate = 16000.0f;
	constexpr float PCMScale = 1.0f / 32768.0f;
	constexpr float TargetRMS = 0.06f;
	// A .0002 RMS voice can reach the recognizer's .006 speech floor at +30 dB.
	// Keeping eligibility lower than that is essential for very quiet microphones.
	constexpr float ActivityFloorRMS = 0.00015f;
	constexpr float OutputCeiling = 0.97f;
	constexpr float RawClippingThreshold = 0.98f;
	constexpr int32 InactiveHangoverSamples = 3200; // 200 ms; retain quiet syllable tails.
	constexpr int32 InitialAcquisitionSamples = 640; // First 40 ms of eligible input.

	float Coefficient(float Seconds)
	{
		return 1.0f - FMath::Exp(-1.0f / (Seconds * SampleRate));
	}

	float ToDb(float Linear)
	{
		return 8.685889638f * FMath::Loge(FMath::Max(Linear, 1.0e-12f));
	}
}

void FMallMicrophoneProcessor::Reset()
{
	PreviousInput = 0.0f;
	PreviousHighPassOutput = 0.0f;
	DetectorPower = 0.0f;
	CurrentGainDb = 0.0f;
	InactiveSamples = 0;
	GainAcquisitionSamples = 0;
}

FMallMicrophoneLevels FMallMicrophoneProcessor::Process(
	TArray<int16>& Samples, const FMallMicrophoneOptions& Options)
{
	using namespace MallMicrophone;
	FMallMicrophoneLevels Levels;
	Levels.AppliedGainDb = CurrentGainDb;
	if (Samples.IsEmpty())
	{
		return Levels;
	}

	const float ManualGainDb = FMath::IsFinite(Options.GainDb)
		? FMath::Clamp(Options.GainDb, -12.0f, 30.0f) : 0.0f;
	const float MaximumGainDb = FMath::IsFinite(Options.MaxAutoGainDb)
		? FMath::Clamp(Options.MaxAutoGainDb, 0.0f, 30.0f) : 30.0f;
	const float HighPassCoefficient = FMath::Exp(-2.0f * PI * 80.0f / SampleRate);
	const float DetectorCoefficient = Coefficient(0.02f);
	const float GainIncreaseCoefficient = Coefficient(0.5f);
	const float InitialAcquisitionCoefficient = Coefficient(0.01f);
	const float GainDecreaseCoefficient = Coefficient(0.002f);
	double RawPower = 0.0;
	double OutputPower = 0.0;

	for (int16& Sample : Samples)
	{
		const float Raw = static_cast<float>(Sample) * PCMScale;
		RawPower += static_cast<double>(Raw) * Raw;
		Levels.bClipping |= FMath::Abs(Raw) >= RawClippingThreshold;

		// Keep filter history current even while bypassed, for predictable toggling.
		const float HighPassed = HighPassCoefficient * (PreviousHighPassOutput + Raw - PreviousInput);
		PreviousInput = Raw;
		PreviousHighPassOutput = HighPassed;
		float Conditioned = Options.bNoiseFilter ? HighPassed : Raw;
		DetectorPower += DetectorCoefficient * (Conditioned * Conditioned - DetectorPower);
		const float DetectorRMS = FMath::Sqrt(FMath::Max(DetectorPower, 0.0f));
		const bool bActive = DetectorRMS >= ActivityFloorRMS;
		InactiveSamples = bActive ? 0 : FMath::Min(InactiveSamples + 1, InactiveHangoverSamples + 1);
		if (InactiveSamples > InactiveHangoverSamples)
		{
			GainAcquisitionSamples = 0;
		}

		if (Options.bAutoGain)
		{
			float DesiredGainDb = CurrentGainDb;
			float Smoothing = GainIncreaseCoefficient;
			if (bActive)
			{
				if (FMath::Abs(Raw) >= ActivityFloorRMS)
				{
					GainAcquisitionSamples = FMath::Min(GainAcquisitionSamples + 1, InitialAcquisitionSamples + 1);
				}
				// Automatic gain keeps a fixed speech target. A saved manual boost must
				// not raise that target or make normal speech repeatedly hit the limiter.
				DesiredGainDb = FMath::Clamp(ToDb(TargetRMS / DetectorRMS),
					-30.0f, MaximumGainDb);
				// The detector retains past speech briefly; do not use that history to
				// turn up an actual pause or sub-floor samples between syllables.
				if (DesiredGainDb > CurrentGainDb && FMath::Abs(Raw) < ActivityFloorRMS)
				{
					DesiredGainDb = CurrentGainDb;
				}
				// Acquire a new quiet utterance promptly; a half-second ramp alone can
				// lose its entire first command. Subsequent increases remain gradual.
				const float IncreaseCoefficient = GainAcquisitionSamples <= InitialAcquisitionSamples
					? InitialAcquisitionCoefficient : GainIncreaseCoefficient;
				Smoothing = DesiredGainDb < CurrentGainDb ? GainDecreaseCoefficient : IncreaseCoefficient;
			}
			else if (InactiveSamples > InactiveHangoverSamples)
			{
				// Never turn up silence or the absolute noise floor. After the hangover,
				// slowly return positive gain to unity rather than hunting upward.
				DesiredGainDb = FMath::Min(CurrentGainDb, 0.0f);
			}
			CurrentGainDb += Smoothing * (DesiredGainDb - CurrentGainDb);
			CurrentGainDb = FMath::Clamp(CurrentGainDb, -30.0f, MaximumGainDb);
		}
		else
		{
			CurrentGainDb = ManualGainDb;
		}

		if (Options.bNoiseFilter)
		{
			// At most 12 dB attenuation of the very lowest floor, with no hard gate.
			// Normal quiet speech above .00015 RMS is left alone.
			Conditioned *= FMath::Clamp(DetectorRMS / ActivityFloorRMS, 0.25f, 1.0f);
		}

		float Gain = FMath::Pow(10.0f, CurrentGainDb * 0.05f);
		float Output = Conditioned * Gain;
		if (FMath::Abs(Output) > OutputCeiling)
		{
			Levels.bClipping = true;
			if (Options.bAutoGain)
			{
				// A loud plosive must not wait for the RMS detector or attack smoother.
				Gain = OutputCeiling / FMath::Max(FMath::Abs(Conditioned), 1.0e-12f);
				CurrentGainDb = FMath::Min(CurrentGainDb, ToDb(Gain));
				Output = Conditioned * Gain;
			}
			Output = FMath::Clamp(Output, -OutputCeiling, OutputCeiling);
		}

		Sample = static_cast<int16>(FMath::Clamp(FMath::RoundToInt(Output * 32768.0f), -32768, 32767));
		const float QuantizedOutput = static_cast<float>(Sample) * PCMScale;
		OutputPower += static_cast<double>(QuantizedOutput) * QuantizedOutput;
		Levels.Peak = FMath::Max(Levels.Peak, FMath::Abs(QuantizedOutput));
	}

	Levels.RawRMS = static_cast<float>(FMath::Sqrt(RawPower / Samples.Num()));
	Levels.ProcessedRMS = static_cast<float>(FMath::Sqrt(OutputPower / Samples.Num()));
	Levels.AppliedGainDb = CurrentGainDb;
	return Levels;
}
