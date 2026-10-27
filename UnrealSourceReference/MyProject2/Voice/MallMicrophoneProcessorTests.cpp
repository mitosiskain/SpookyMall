#include "Voice/MallMicrophoneProcessor.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include <limits>

// Reuse the same checked PCM16/16 kHz WAV decoder as the offline-recognizer tests.
// This reads a committed synthetic voice fixture; it never opens a microphone.
namespace MallSpeechRecognitionTests
{
	bool LoadPCMFixture(const FString& Path, TArray<int16>& Samples);
}

namespace MallMicrophoneTests
{
	TArray<int16> Tone(float Amplitude, float Frequency, int32 Count)
	{
		TArray<int16> Samples;
		Samples.SetNumUninitialized(Count);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const float Value = Amplitude * FMath::Sin(2.0f * PI * Frequency * Index / 16000.0f);
			Samples[Index] = static_cast<int16>(FMath::Clamp(FMath::RoundToInt(Value * 32768.0f), -32768, 32767));
		}
		return Samples;
	}

	float RMS(const TArray<int16>& Samples)
	{
		double Power = 0.0;
		for (const int16 Sample : Samples)
		{
			const double Value = static_cast<double>(Sample) / 32768.0;
			Power += Value * Value;
		}
		return Samples.IsEmpty() ? 0.0f : static_cast<float>(FMath::Sqrt(Power / Samples.Num()));
	}

	bool Finite(const FMallMicrophoneLevels& Levels)
	{
		return FMath::IsFinite(Levels.RawRMS) && FMath::IsFinite(Levels.ProcessedRMS)
			&& FMath::IsFinite(Levels.Peak) && FMath::IsFinite(Levels.AppliedGainDb);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMallMicrophoneQuietInputTest, "MyProject2.Voice.Microphone.QuietInput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMallMicrophoneQuietInputTest::RunTest(const FString& Parameters)
{
	using namespace MallMicrophoneTests;
	TArray<int16> Voice;
	if (!TestTrue(TEXT("Committed synthetic speech fixture loads without microphone capture"),
		MallSpeechRecognitionTests::LoadPCMFixture(
			FPaths::Combine(FPaths::ProjectDir(), TEXT("Automation/Voice/Fixtures/come_here.wav")), Voice)))
	{
		return false;
	}
	const float Scale = 0.0004f / FMath::Max(RMS(Voice), 1.0e-6f);
	for (int16& Sample : Voice)
	{
		Sample = static_cast<int16>(FMath::RoundToInt(Sample * Scale));
	}
	const float QuietRMS = RMS(Voice);
	TestTrue(TEXT("Real speech fixture is scaled to a very quiet microphone"), QuietRMS > 0.00035f && QuietRMS < 0.00045f);

	FMallMicrophoneProcessor Processor;
	const FMallMicrophoneOptions Options;
	const TArray<int16> OriginalQuietSpeech = Voice;
	const int32 OriginalCount = Voice.Num();
	const FMallMicrophoneLevels Levels = Processor.Process(Voice, Options);
	TestEqual(TEXT("AGC preserves all spoken samples including pauses"), Voice.Num(), OriginalCount);
	TestTrue(TEXT("The very first quiet command is usable without a warm-up tone"), Levels.ProcessedRMS > 0.006f);
	TestTrue(TEXT("Unprocessed RMS is measured before any gain"), FMath::IsNearlyEqual(Levels.RawRMS, QuietRMS, 1.0e-7f));
	TestTrue(TEXT("Quiet speech never exceeds total gain cap"), Levels.AppliedGainDb <= 30.001f);
	TestFalse(TEXT("Quiet speech does not clip"), Levels.bClipping);
	TArray<int16> LongPause;
	LongPause.Init(0, 64000);
	Processor.Process(LongPause, Options);
	TArray<int16> RepeatedSpeech = OriginalQuietSpeech;
	TestTrue(TEXT("A quiet command is reacquired after a long silent pause"),
		Processor.Process(RepeatedSpeech, Options).ProcessedRMS > 0.006f);

	Processor.Reset();
	TArray<int16> ToneSpeech = Tone(0.001f, 440.0f, 48000);
	Processor.Process(ToneSpeech, Options);
	TArray<int16> Tail = Tone(0.001f, 440.0f, 8000);
	TestTrue(TEXT("Automatic gain can lift a sustained quiet signal"), Processor.Process(Tail, Options).ProcessedRMS > 0.015f);

	FMallMicrophoneOptions BiasedOptions;
	BiasedOptions.GainDb = 30.0f;
	Processor.Reset();
	TArray<int16> Biased = Tone(0.0006f, 440.0f, 48000);
	const FMallMicrophoneLevels BiasedLevels = Processor.Process(Biased, BiasedOptions);
	TestTrue(TEXT("Saved manual boost cannot bypass the AGC +30 dB cap"), BiasedLevels.AppliedGainDb <= 30.001f);
	TestTrue(TEXT("Automatic gain cannot become +60 dB"), BiasedLevels.ProcessedRMS < 0.014f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMallMicrophoneAutomaticTargetTest, "MyProject2.Voice.Microphone.AutomaticTargetIgnoresManualBoost",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMallMicrophoneAutomaticTargetTest::RunTest(const FString& Parameters)
{
	using namespace MallMicrophoneTests;
	TArray<int16> Fixture;
	if (!TestTrue(TEXT("Speech fixture loads without accessing the microphone"),
		MallSpeechRecognitionTests::LoadPCMFixture(
			FPaths::Combine(FPaths::ProjectDir(), TEXT("Automation/Voice/Fixtures/Whisper/where_0.wav")), Fixture)))
	{
		return false;
	}
	const float FixtureRMS = RMS(Fixture);
	if (!TestTrue(TEXT("Speech fixture contains audio"), FixtureRMS > 0.0f)) return false;
	for (const float InputRMS : {0.0004f, 0.01f, 0.06f})
	{
		TArray<int16> Reference;
		Reference.Reserve(Fixture.Num());
		for (const int16 Sample : Fixture)
		{
			Reference.Add(static_cast<int16>(FMath::Clamp(
				FMath::RoundToInt(Sample * InputRMS / FixtureRMS), -32768, 32767)));
		}
		TArray<int16> SavedBoost = Reference;
		FMallMicrophoneOptions DefaultOptions;
		FMallMicrophoneOptions SavedOptions;
		SavedOptions.GainDb = 19.705271f; // Reported local profile when decoding failed.
		FMallMicrophoneProcessor DefaultProcessor;
		FMallMicrophoneProcessor SavedProcessor;
		const FMallMicrophoneLevels DefaultLevels = DefaultProcessor.Process(Reference, DefaultOptions);
		const FMallMicrophoneLevels SavedLevels = SavedProcessor.Process(SavedBoost, SavedOptions);
		TestTrue(*FString::Printf(TEXT("Saved +19.7 dB manual boost leaves automatic speech PCM unchanged at %.4f RMS"), InputRMS),
			Reference == SavedBoost);
		TestEqual(TEXT("Saved manual boost cannot raise the automatic speech level"), SavedLevels.ProcessedRMS, DefaultLevels.ProcessedRMS);
		TestEqual(TEXT("Saved manual boost cannot cause extra automatic limiting"), SavedLevels.bClipping, DefaultLevels.bClipping);
		TestTrue(TEXT("Automatic gain retains its total positive gain cap"), SavedLevels.AppliedGainDb <= 30.001f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMallMicrophoneFloorTest, "MyProject2.Voice.Microphone.SilenceAndFloor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMallMicrophoneFloorTest::RunTest(const FString& Parameters)
{
	using namespace MallMicrophoneTests;
	FMallMicrophoneProcessor Processor;
	FMallMicrophoneOptions Options;
	TArray<int16> Silence;
	Silence.Init(0, 48000);
	FMallMicrophoneLevels Levels = Processor.Process(Silence, Options);
	TestEqual(TEXT("Silence remains exactly silent"), RMS(Silence), 0.0f);
	TestEqual(TEXT("Silence cannot turn automatic gain up"), Levels.AppliedGainDb, 0.0f);

	TArray<int16> Floor = Tone(0.0001f, 440.0f, 48000);
	Levels = Processor.Process(Floor, Options);
	TestTrue(TEXT("Sub-floor background does not increase automatic gain"), Levels.AppliedGainDb <= 0.001f);
	TestTrue(TEXT("Quiet floor stays below server speech threshold"), Levels.ProcessedRMS < 0.004f);
	TestTrue(TEXT("Gentle expansion reduces the absolute noise floor"), Levels.ProcessedRMS < Levels.RawRMS);

	TArray<int16> Speech = Tone(0.001f, 440.0f, 48000);
	const float BeforePause = Processor.Process(Speech, Options).AppliedGainDb;
	Silence.Init(0, 3200);
	const float AfterShortPause = Processor.Process(Silence, Options).AppliedGainDb;
	TestTrue(TEXT("Gain never rises during a pause"), AfterShortPause <= BeforePause + 0.001f);
	Silence.Init(0, 64000);
	Levels = Processor.Process(Silence, Options);
	TestTrue(TEXT("Long silence returns gain toward unity"), Levels.AppliedGainDb < BeforePause * 0.1f);
	TestTrue(TEXT("No synthetic speech is generated from a long pause"), Levels.ProcessedRMS < 0.0001f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMallMicrophoneLimiterTest, "MyProject2.Voice.Microphone.LoudTransitionsAndManualGain",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMallMicrophoneLimiterTest::RunTest(const FString& Parameters)
{
	using namespace MallMicrophoneTests;
	FMallMicrophoneProcessor Processor;
	FMallMicrophoneOptions Options;
	TArray<int16> Quiet = Tone(0.001f, 440.0f, 48000);
	const float QuietGain = Processor.Process(Quiet, Options).AppliedGainDb;
	TArray<int16> Plosive = {32767, -32768, 32767, 0};
	const FMallMicrophoneLevels PlosiveLevels = Processor.Process(Plosive, Options);
	TestTrue(TEXT("Raw near-rail microphone input reports clipping"), PlosiveLevels.bClipping);
	TestTrue(TEXT("A plosive reduces accumulated automatic gain immediately"), PlosiveLevels.AppliedGainDb < QuietGain - 20.0f);
	TestTrue(TEXT("Loud plosives are bounded below .98 full scale"), PlosiveLevels.Peak < 0.98f);

	TArray<int16> Loud = Tone(0.75f, 440.0f, 16000);
	Processor.Process(Loud, Options);
	Loud = Tone(0.75f, 440.0f, 16000);
	const FMallMicrophoneLevels LoudLevels = Processor.Process(Loud, Options);
	TestTrue(TEXT("Loud sustained speech settles near the speech RMS target"),
		LoudLevels.ProcessedRMS > 0.045f && LoudLevels.ProcessedRMS < 0.075f);
	TestTrue(TEXT("Loud transition keeps finite metering"), Finite(LoudLevels));

	Options.bAutoGain = false;
	Options.bNoiseFilter = false;
	Options.GainDb = 6.0f;
	Processor.Reset();
	TArray<int16> Manual = Tone(0.01f, 440.0f, 16000);
	const FMallMicrophoneLevels ManualLevels = Processor.Process(Manual, Options);
	TestTrue(TEXT("Disabling AGC makes +6 dB a reproducible manual volume"),
		FMath::IsNearlyEqual(ManualLevels.ProcessedRMS / ManualLevels.RawRMS, 1.995262f, 0.005f));
	TestEqual(TEXT("Meter reports the manual gain"), ManualLevels.AppliedGainDb, 6.0f);
	Options.GainDb = -12.0f;
	Manual = Tone(0.01f, 440.0f, 16000);
	const FMallMicrophoneLevels Attenuated = Processor.Process(Manual, Options);
	TestTrue(TEXT("Manual control also attenuates a loud microphone"),
		FMath::IsNearlyEqual(Attenuated.ProcessedRMS / Attenuated.RawRMS, 0.251189f, 0.005f));
	Options.GainDb = 30.0f;
	Manual = Tone(0.9f, 440.0f, 16000);
	const FMallMicrophoneLevels ManualClipping = Processor.Process(Manual, Options);
	TestTrue(TEXT("Manual amplification is also limited below .98"), ManualClipping.Peak < 0.98f);
	TestTrue(TEXT("Manual limiting is visible to the user"), ManualClipping.bClipping);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMallMicrophoneFilterTest, "MyProject2.Voice.Microphone.LowFrequencyFilter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMallMicrophoneFilterTest::RunTest(const FString& Parameters)
{
	using namespace MallMicrophoneTests;
	FMallMicrophoneOptions Options;
	Options.bAutoGain = false;
	FMallMicrophoneProcessor Processor;
	TArray<int16> DC;
	DC.Init(3277, 16000);
	Processor.Process(DC, Options);
	DC.Init(3277, 16000);
	TestTrue(TEXT("High-pass removes steady DC microphone offset"), Processor.Process(DC, Options).ProcessedRMS < 0.00001f);

	Processor.Reset();
	TArray<int16> Low = Tone(0.1f, 20.0f, 16000);
	Processor.Process(Low, Options);
	Low = Tone(0.1f, 20.0f, 16000);
	const FMallMicrophoneLevels LowLevels = Processor.Process(Low, Options);
	Processor.Reset();
	TArray<int16> VoiceBand = Tone(0.1f, 1000.0f, 16000);
	Processor.Process(VoiceBand, Options);
	VoiceBand = Tone(0.1f, 1000.0f, 16000);
	const FMallMicrophoneLevels VoiceBandLevels = Processor.Process(VoiceBand, Options);
	TestTrue(TEXT("Filter attenuates low-frequency rumble"), LowLevels.ProcessedRMS < LowLevels.RawRMS * 0.3f);
	TestTrue(TEXT("Filter retains the speech band"), VoiceBandLevels.ProcessedRMS > VoiceBandLevels.RawRMS * 0.9f);

	Processor.Reset();
	Options.bNoiseFilter = false;
	TArray<int16> Bypassed = Tone(0.1f, 20.0f, 16000);
	const TArray<int16> Original = Bypassed;
	Processor.Process(Bypassed, Options);
	TestTrue(TEXT("Filter and AGC bypass at zero manual gain preserves PCM exactly"), Bypassed == Original);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMallMicrophoneChunkTest, "MyProject2.Voice.Microphone.ChunkInvarianceAndInvalidOptions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMallMicrophoneChunkTest::RunTest(const FString& Parameters)
{
	using namespace MallMicrophoneTests;
	TArray<int16> Original;
	Original.Init(0, 4000);
	Original.Append(Tone(0.001f, 440.0f, 18000));
	Original.Append({32767, -32768, 0});
	Original.Append(Tone(0.4f, 200.0f, 8000));
	Original.Append(Tone(0.0001f, 20.0f, 8000));
	Original.AddZeroed(8000);
	FMallMicrophoneOptions Options;
	FMallMicrophoneProcessor WholeProcessor;
	TArray<int16> Whole = Original;
	const FMallMicrophoneLevels WholeLevels = WholeProcessor.Process(Whole, Options);
	FMallMicrophoneProcessor ChunkProcessor;
	TArray<int16> Chunked;
	const int32 ChunkSizes[] = {1, 7, 160, 1024, 57};
	int32 Position = 0;
	int32 ChunkIndex = 0;
	FMallMicrophoneLevels LastLevels;
	bool bAllCountsPreserved = true;
	while (Position < Original.Num())
	{
		const int32 Count = FMath::Min(ChunkSizes[ChunkIndex++ % UE_ARRAY_COUNT(ChunkSizes)], Original.Num() - Position);
		TArray<int16> Chunk;
		Chunk.Append(Original.GetData() + Position, Count);
		LastLevels = ChunkProcessor.Process(Chunk, Options);
		bAllCountsPreserved &= Chunk.Num() == Count;
		Chunked.Append(Chunk);
		Position += Count;
	}
	TestTrue(TEXT("Every arbitrary-sized capture buffer preserves sample count"), bAllCountsPreserved);
	TestTrue(TEXT("Audio samples are identical for large and irregular capture buffers"), Whole == Chunked);
	TestEqual(TEXT("Final gain is independent of capture buffer boundaries"), LastLevels.AppliedGainDb, WholeLevels.AppliedGainDb);

	ChunkProcessor.Reset();
	TArray<int16> ResetAudio = Original;
	ChunkProcessor.Process(ResetAudio, Options);
	TestTrue(TEXT("Reset clears all detector, gain, and filter history"), ResetAudio == Whole);

	Options.GainDb = std::numeric_limits<float>::quiet_NaN();
	Options.MaxAutoGainDb = std::numeric_limits<float>::infinity();
	TArray<int16> BadOptions = Tone(0.001f, 440.0f, 8000);
	TestTrue(TEXT("NaN and infinity in settings never poison audio or meters"), Finite(ChunkProcessor.Process(BadOptions, Options)));
	Options.GainDb = 1.0e30f;
	Options.MaxAutoGainDb = -1.0e30f;
	BadOptions = Tone(0.001f, 440.0f, 8000);
	const FMallMicrophoneLevels Clamped = ChunkProcessor.Process(BadOptions, Options);
	TestTrue(TEXT("Extreme options are bounded and finite"), Finite(Clamped) && Clamped.AppliedGainDb <= 0.001f);
	TArray<int16> Empty;
	TestTrue(TEXT("Empty buffers are safe and do not advance state"), Finite(ChunkProcessor.Process(Empty, Options)));
	return true;
}
#endif
