#include "MallVoiceSettings.h"

UMallVoiceSettings::UMallVoiceSettings()
{
	PushToTalkKey = EKeys::V;
	auto Add = [this](const TCHAR* Id, std::initializer_list<const TCHAR*> Aliases)
	{
		FMallVoicePhrase& Phrase = Phrases.AddDefaulted_GetRef();
		Phrase.Id = FName(Id);
		for (const TCHAR* Alias : Aliases) { Phrase.Aliases.Add(Alias); }
	};
	Add(TEXT("where_are_you"), {TEXT("where are you")});
	Add(TEXT("who_are_you"), {TEXT("who are you"), TEXT("what is your name")});
	Add(TEXT("how_old_are_you"), {TEXT("how old are you"), TEXT("what is your age")});
	Add(TEXT("give_me_a_sign"), {TEXT("give me a sign"), TEXT("show me a sign")});
	Add(TEXT("come_here"), {TEXT("come here"), TEXT("come over here")});
	Add(TEXT("leave_me_alone"), {TEXT("leave me alone"), TEXT("go away")});
	Add(TEXT("write_in_book"), {TEXT("write in the book"), TEXT("write something")});
}

TMap<FName, TArray<FString>> UMallVoiceSettings::GetPhraseAliases() const
{
	TMap<FName, TArray<FString>> Result;
	for (const FMallVoicePhrase& Phrase : Phrases)
	{
		if (!Phrase.Id.IsNone() && !Phrase.Aliases.IsEmpty()) { Result.Add(Phrase.Id, Phrase.Aliases); }
	}
	return Result;
}
