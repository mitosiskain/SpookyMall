#include "Voice/MallVoicePhraseMatcher.h"
#include "Voice/MallVoiceSettings.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMallVoicePhraseMatcherTest, "MyProject2.Voice.PhraseMatching",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMallVoicePhraseMatcherTest::RunTest(const FString& Parameters)
{
	const UMallVoiceSettings* Settings = GetDefault<UMallVoiceSettings>();
	TestTrue(TEXT("Configured push-to-talk key resolves to a registered input key"), Settings->PushToTalkKey.IsValid());
	TestEqual(TEXT("Default phrase matching has no confidence floor"), Settings->MinimumPhraseConfidence, 0.0f);
	// Exercise the actual configured aliases and threshold used by the live recognizer.
	TMap<FName, TArray<FString>> Aliases = Settings->GetPhraseAliases();
	const float MinimumConfidence = Settings->MinimumPhraseConfidence;
	const auto Match = [&Aliases, MinimumConfidence](const FString& Text, float Confidence = 0.95f)
	{
		return MallVoicePhraseMatcher::Match(MallVoicePhraseMatcher::Normalize(Text), Confidence, MinimumConfidence, Aliases);
	};
	TestEqual(TEXT("Normalize case and punctuation"), MallVoicePhraseMatcher::Normalize(TEXT(" WHERE,  are you?! ")), FString(TEXT("where are you")));
	TestTrue(TEXT("Exact phrase accepted"), Match(TEXT("Come here!")) == FName(TEXT("come_here")));
	TestTrue(TEXT("Second phrase accepted"), Match(TEXT("Give me a sign.")) == FName(TEXT("give_me_a_sign")));
	TestTrue(TEXT("Writing command accepted"), Match(TEXT("Write in the book.")) == FName(TEXT("write_in_book")));
	TestTrue(TEXT("Writing alias accepted"), Match(TEXT("Write something")) == FName(TEXT("write_in_book")));
	TestTrue(TEXT("Location question accepted"), Match(TEXT("Where are you?")) == FName(TEXT("where_are_you")));
	TestTrue(TEXT("Identity question accepted"), Match(TEXT("Who are you?")) == FName(TEXT("who_are_you")));
	TestTrue(TEXT("Name question alias accepted"), Match(TEXT("What is your name?")) == FName(TEXT("who_are_you")));
	TestTrue(TEXT("Age question accepted"), Match(TEXT("How old are you?")) == FName(TEXT("how_old_are_you")));
	TestTrue(TEXT("Age question alias accepted"), Match(TEXT("What is your age?")) == FName(TEXT("how_old_are_you")));
	TestTrue(TEXT("Sign alias remains accepted"), Match(TEXT("Show me a sign")) == FName(TEXT("give_me_a_sign")));
	TestTrue(TEXT("Come-here alias remains accepted"), Match(TEXT("Come over here")) == FName(TEXT("come_here")));
	TestTrue(TEXT("Pause phrase remains accepted"), Match(TEXT("Leave me alone")) == FName(TEXT("leave_me_alone")));
	TestTrue(TEXT("Pause alias remains accepted"), Match(TEXT("Go away")) == FName(TEXT("leave_me_alone")));
	TestTrue(TEXT("Negated writing command rejected"), Match(TEXT("do not write in the book")).IsNone());
	TestTrue(TEXT("Contracted negated writing command rejected"), Match(TEXT("don't write in the book")).IsNone());
	TestTrue(TEXT("Negated identity question rejected"), Match(TEXT("do not tell me who you are")).IsNone());
	TestTrue(TEXT("Negated name question rejected"), Match(TEXT("don't tell me your name")).IsNone());
	TestTrue(TEXT("Negated age question rejected"), Match(TEXT("do not tell me how old you are")).IsNone());
	TestTrue(TEXT("Negated age alias rejected"), Match(TEXT("what is not your age")).IsNone());
	TestTrue(TEXT("Question inside longer speech is rejected"), Match(TEXT("I asked who are you yesterday")).IsNone());
	TestTrue(TEXT("Unrelated speech rejected"), Match(TEXT("I like this store")).IsNone());
	TestTrue(TEXT("Negation cannot become command"), Match(TEXT("do not come here")).IsNone());
	TestTrue(TEXT("Quoted or longer speech is not a substring command"), Match(TEXT("I said where are you yesterday")).IsNone());
	for (const float Confidence : {0.0f, 0.1f, 0.4f})
	{
		TestTrue(*FString::Printf(TEXT("Exact writing phrase accepted at confidence %.1f"), Confidence),
			Match(TEXT("write in the book"), Confidence) == FName(TEXT("write_in_book")));
		TestTrue(*FString::Printf(TEXT("Right-in-book ASR variant accepted at confidence %.1f"), Confidence),
			Match(TEXT("right in the book"), Confidence) == FName(TEXT("write_in_book")));
		TestTrue(*FString::Printf(TEXT("Write-of-book ASR variant accepted at confidence %.1f"), Confidence),
			Match(TEXT("the write of the book"), Confidence) == FName(TEXT("write_in_book")));
	}
	TestTrue(TEXT("Low-confidence exact come-here phrase accepted"), Match(TEXT("come here"), 0.1f) == FName(TEXT("come_here")));
	TestTrue(TEXT("Zero-confidence exact question accepted"), Match(TEXT("who are you"), 0.0f) == FName(TEXT("who_are_you")));
	TestTrue(TEXT("Article and preposition variants accepted"), Match(TEXT("write on a book")) == FName(TEXT("write_in_book")));
	TestTrue(TEXT("Polite request prefix accepted"), Match(TEXT("could you please rite in the book")) == FName(TEXT("write_in_book")));
	TestTrue(TEXT("Can-you prefix accepted"), Match(TEXT("can you write to the book please")) == FName(TEXT("write_in_book")));
	TestTrue(TEXT("Would-you prefix accepted"), Match(TEXT("would you write in an book")) == FName(TEXT("write_in_book")));
	TestTrue(TEXT("Will-you prefix and sign homophone accepted"), Match(TEXT("will you give me a sine please")) == FName(TEXT("give_me_a_sign")));
	TestTrue(TEXT("Hear homophone accepted"), Match(TEXT("please come hear")) == FName(TEXT("come_here")));
	TestTrue(TEXT("Wear homophone accepted"), Match(TEXT("wear are you")) == FName(TEXT("where_are_you")));
	TestTrue(TEXT("Were homophone accepted"), Match(TEXT("were are you")) == FName(TEXT("where_are_you")));
	TestTrue(TEXT("Negated ASR homophone rejected"), Match(TEXT("do not right in the book"), 0.0f).IsNone());
	TestTrue(TEXT("Negated preposition variant rejected"), Match(TEXT("don't write of the book"), 0.1f).IsNone());
	TestTrue(TEXT("Never-request rejected"), Match(TEXT("never rite on the book")).IsNone());
	TestTrue(TEXT("No-request rejected"), Match(TEXT("no please write in the book")).IsNone());
	TestTrue(TEXT("Cannot-request rejected"), Match(TEXT("you can't write in the book")).IsNone());
	TestTrue(TEXT("Wont-request rejected"), Match(TEXT("you won't write in the book")).IsNone());
	TestTrue(TEXT("Multiple intents remain rejected"), Match(TEXT("write in the book and come here")).IsNone());
	TestTrue(TEXT("Alternative intents remain rejected"), Match(TEXT("can you come here or go away")).IsNone());
	TestTrue(TEXT("Unrelated book sentence rejected"), Match(TEXT("the book is on the right of the shop")).IsNone());
	TestTrue(TEXT("Empty significant-word sequence rejected"), Match(TEXT("please in the a on of to an")).IsNone());
	TestTrue(TEXT("Tolerance cannot erase arbitrarily long speech"), Match(TEXT("please please please please please please please please please please please please please please please please right book")).IsNone());
	TestTrue(TEXT("NaN confidence metadata rejected"), Match(TEXT("write in the book"), std::numeric_limits<float>::quiet_NaN()).IsNone());
	TestTrue(TEXT("Negative confidence metadata rejected"), Match(TEXT("write in the book"), -0.1f).IsNone());
	TestTrue(TEXT("Above-one confidence metadata rejected"), Match(TEXT("write in the book"), 1.1f).IsNone());
	TestTrue(TEXT("Explicit nonzero threshold remains functional"),
		MallVoicePhraseMatcher::Match(TEXT("right in the book"), 0.1f, 0.7f, Aliases).IsNone());
	TestTrue(TEXT("Empty speech rejected"), Match(TEXT("... ")).IsNone());
	Aliases.Add(TEXT("exact_override"), {TEXT("right in the book")});
	TestTrue(TEXT("Exact configured alias takes precedence over tolerant alias"), Match(TEXT("right in the book"), 0.1f) == FName(TEXT("exact_override")));
	Aliases.Remove(TEXT("exact_override"));
	Aliases.Add(TEXT("tolerant_conflict"), {TEXT("write on a book")});
	TestTrue(TEXT("Conflicting tolerant aliases rejected"), Match(TEXT("right of the book")).IsNone());
	Aliases.Remove(TEXT("tolerant_conflict"));
	Aliases.Add(TEXT("negated_alias"), {TEXT("do not right in the book")});
	TestTrue(TEXT("Negation is rejected even for an exact configured alias"), Match(TEXT("do not right in the book")).IsNone());
	Aliases.Remove(TEXT("negated_alias"));
	Aliases.Add(TEXT("ambiguous"), {TEXT("come here")});
	TestTrue(TEXT("Conflicting aliases rejected"), Match(TEXT("come here")).IsNone());
	return true;
}
#endif
