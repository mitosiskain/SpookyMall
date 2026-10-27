#include "Voice/MallVoiceSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMallPushToTalkUtteranceTest, "MyProject2.Voice.PushToTalkUtterance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMallPushToTalkUtteranceTest::RunTest(const FString& Parameters)
{
	FMallSpeechUtteranceBuffer Buffer;
	TArray<TArray<int16>> Captured;
	const auto Capture = [&Captured](TArray<int16>&& Audio) { Captured.Add(MoveTemp(Audio)); };

	// A quiet connecting word/pause longer than the former 0.6-second endpoint
	// belongs to the same turn. Every PCM sample must reach the recognizer.
	TArray<int16> FirstWord;
	FirstWord.Init(1400, 4000);
	TArray<int16> QuietMiddle;
	QuietMiddle.Init(40, 16000);
	TArray<int16> LastWord;
	LastWord.Init(-1200, 4000);
	TArray<int16> Expected = FirstWord;
	Expected.Append(QuietMiddle);
	Expected.Append(LastWord);
	Buffer.Append(FirstWord, false, Capture);
	Buffer.Append(QuietMiddle, false, Capture);
	Buffer.Append(TConstArrayView<int16>(), false, Capture);
	Buffer.Append(LastWord, false, Capture);
	TestEqual(TEXT("Quiet gaps and empty capture polls do not end a held push-to-talk turn"), Captured.Num(), 0);
	Buffer.Append(TConstArrayView<int16>(), true, Capture);
	TestEqual(TEXT("Key release submits the whole turn exactly once"), Captured.Num(), 1);
	if (Captured.Num() == 1)
	{
		TestTrue(TEXT("Words and low-level connecting audio are preserved in order"), Captured[0] == Expected);
	}
	Buffer.Append(TConstArrayView<int16>(), true, Capture);
	TestEqual(TEXT("Repeated end notifications cannot replay a turn"), Captured.Num(), 1);

	Captured.Reset();
	TArray<int16> Silence;
	Silence.Init(0, 32000);
	Buffer.Append(Silence, true, Capture);
	TArray<int16> Click;
	Click.Init(10000, 1000);
	Buffer.Append(Click, true, Capture);
	TestEqual(TEXT("Silence and sub-quarter-second pops are not submitted"), Captured.Num(), 0);
	Buffer.Append(FirstWord, false, Capture);
	Buffer.Reset();
	Buffer.Append(TConstArrayView<int16>(), true, Capture);
	TestEqual(TEXT("Reset during teardown/disable discards unfinished speech"), Captured.Num(), 0);

	// A capture buffer can straddle eight seconds. The old append clipped away
	// its tail; preserve it for the next bounded segment and release.
	TArray<int16> LongTurn;
	LongTurn.SetNumUninitialized(FMallSpeechUtteranceBuffer::MaxSamples + 4800);
	for (int32 Index = 0; Index < LongTurn.Num(); ++Index)
	{
		LongTurn[Index] = static_cast<int16>(1000 + Index % 300);
	}
	Buffer.Append(LongTurn, false, Capture);
	TestEqual(TEXT("Eight-second cap submits one bounded segment while the key remains held"), Captured.Num(), 1);
	if (Captured.Num() == 1)
	{
		TestEqual(TEXT("Capped segment has exactly eight seconds"), Captured[0].Num(), FMallSpeechUtteranceBuffer::MaxSamples);
	}
	Buffer.Append(TConstArrayView<int16>(), true, Capture);
	TestEqual(TEXT("Release submits the remaining 0.3 seconds"), Captured.Num(), 2);
	if (Captured.Num() == 2)
	{
		TestEqual(TEXT("The final segment remains bounded"), Captured[1].Num(), 4800);
		TArray<int16> Reassembled = Captured[0];
		Reassembled.Append(Captured[1]);
		TestTrue(TEXT("No samples are lost or duplicated at the size cap"), Reassembled == LongTurn);
	}
	return true;
}
#endif
