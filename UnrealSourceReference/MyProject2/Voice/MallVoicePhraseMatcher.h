#pragma once

#include "CoreMinimal.h"

/** Whole-utterance matching with explicit ASR variants; never match arbitrary substrings. */
namespace MallVoicePhraseMatcher
{
inline FString Normalize(const FString& Text)
{
	FString Result;
	bool bNeedSpace = false;
	for (const TCHAR Character : Text)
	{
		if (FChar::IsAlnum(Character))
		{
			if (bNeedSpace && !Result.IsEmpty())
			{
				Result.AppendChar(TEXT(' '));
			}
			Result.AppendChar(FChar::ToLower(Character));
			bNeedSpace = false;
		}
		else if (Character != TEXT('\'') && Character != TCHAR(0x2019))
		{
			bNeedSpace = true;
		}
	}
	return Result;
}

inline bool ContainsNegation(const TArray<FString>& Words)
{
	static const TSet<FString> Negations = {
		TEXT("not"), TEXT("no"), TEXT("never"), TEXT("dont"), TEXT("cant"), TEXT("cannot"),
		TEXT("wont"), TEXT("wouldnt"), TEXT("couldnt"), TEXT("shouldnt"), TEXT("isnt"),
		TEXT("arent"), TEXT("wasnt"), TEXT("werent"), TEXT("hasnt"), TEXT("havent"),
		TEXT("hadnt"), TEXT("didnt"), TEXT("doesnt"), TEXT("mustnt"), TEXT("aint"),
		TEXT("stop"), TEXT("without")
	};
	for (const FString& Word : Words)
	{
		if (Negations.Contains(Word)) { return true; }
	}
	return false;
}

/** Keep all meaningful words in order; only discard a small, explicit set of filler words. */
inline FString TolerantKey(const TArray<FString>& Words)
{
	TArray<FString> Significant;
	for (FString Word : Words)
	{
		if (Word == TEXT("the") || Word == TEXT("a") || Word == TEXT("an") || Word == TEXT("in")
			|| Word == TEXT("on") || Word == TEXT("of") || Word == TEXT("to") || Word == TEXT("please"))
		{
			continue;
		}
		if (Word == TEXT("right") || Word == TEXT("rite")) { Word = TEXT("write"); }
		else if (Word == TEXT("hear")) { Word = TEXT("here"); }
		else if (Word == TEXT("sine")) { Word = TEXT("sign"); }
		else if (Word == TEXT("wear") || Word == TEXT("were")) { Word = TEXT("where"); }
		Significant.Add(MoveTemp(Word));
	}
	if (Significant.Num() >= 2 && Significant[1] == TEXT("you")
		&& (Significant[0] == TEXT("can") || Significant[0] == TEXT("could")
			|| Significant[0] == TEXT("would") || Significant[0] == TEXT("will")))
	{
		Significant.RemoveAt(0, 2, EAllowShrinking::No);
	}
	return FString::Join(Significant, TEXT(" "));
}

inline FName Match(const FString& NormalizedText, float Confidence, float MinimumConfidence,
	const TMap<FName, TArray<FString>>& Aliases)
{
	if (NormalizedText.IsEmpty() || !FMath::IsFinite(Confidence) || Confidence < 0.0f || Confidence > 1.0f
		|| !FMath::IsFinite(MinimumConfidence) || MinimumConfidence < 0.0f || MinimumConfidence > 1.0f
		|| Confidence < MinimumConfidence)
	{
		return NAME_None;
	}
	TArray<FString> Words;
	NormalizedText.ParseIntoArray(Words, TEXT(" "), true);
	// Check before both exact and tolerant matching, including normalized contractions.
	if (ContainsNegation(Words)) { return NAME_None; }

	FName ExactId = NAME_None;
	for (const auto& Entry : Aliases)
	{
		if (Entry.Key.IsNone()) { continue; }
		for (const FString& Alias : Entry.Value)
		{
			if (Normalize(Alias) == NormalizedText)
			{
				if (!ExactId.IsNone() && ExactId != Entry.Key) { return NAME_None; }
				ExactId = Entry.Key;
			}
		}
	}
	// An exact configured alias takes precedence over a different tolerant interpretation.
	if (!ExactId.IsNone()) { return ExactId; }

	// Eight-second speech submissions are bounded already; keep the tolerance itself narrow too.
	if (Words.Num() > 16 || NormalizedText.Len() > 256) { return NAME_None; }
	const FString Key = TolerantKey(Words);
	if (Key.IsEmpty()) { return NAME_None; }
	FName TolerantId = NAME_None;
	for (const auto& Entry : Aliases)
	{
		if (Entry.Key.IsNone()) { continue; }
		for (const FString& Alias : Entry.Value)
		{
			TArray<FString> AliasWords;
			Normalize(Alias).ParseIntoArray(AliasWords, TEXT(" "), true);
			if (ContainsNegation(AliasWords) || TolerantKey(AliasWords) != Key) { continue; }
			if (!TolerantId.IsNone() && TolerantId != Entry.Key) { return NAME_None; }
			TolerantId = Entry.Key;
		}
	}
	return TolerantId;
}
}
