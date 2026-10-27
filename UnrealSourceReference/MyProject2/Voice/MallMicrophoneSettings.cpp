#include "MallMicrophoneSettings.h"

#include "CoreGlobals.h"
#include "HAL/IConsoleManager.h"
#include "String/LexFromString.h"

DEFINE_LOG_CATEGORY_STATIC(LogMallMicrophoneSettings, Log, All);

namespace MallMicrophoneSettings
{
	FAutoConsoleCommand GainCommand(
		TEXT("mall.MicGain"), TEXT("Set and save this user's microphone boost in dB (-12 to 30)."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			float Value = 0.f;
			if (Args.Num() == 1 && LexTryParseString(Value, *Args[0]) && FMath::IsFinite(Value))
			{
				GetMutableDefault<UMallMicrophoneSettings>()->SetGainDb(Value);
			}
			else
			{
				UE_LOG(LogMallMicrophoneSettings, Display, TEXT("Usage: mall.MicGain <dB>, -12 to 30. Current: %.1f dB"),
					GetDefault<UMallMicrophoneSettings>()->GetOptions().GainDb);
			}
		}));

	FAutoConsoleCommand AutoGainCommand(
		TEXT("mall.MicAutoGain"), TEXT("Set and save microphone automatic gain: 0 or 1."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			if (Args.Num() == 1 && (Args[0] == TEXT("0") || Args[0] == TEXT("1")))
			{
				GetMutableDefault<UMallMicrophoneSettings>()->SetAutoGain(Args[0] == TEXT("1"));
			}
			else
			{
				UE_LOG(LogMallMicrophoneSettings, Display, TEXT("Usage: mall.MicAutoGain 0|1. Current: %d"),
					GetDefault<UMallMicrophoneSettings>()->GetOptions().bAutoGain);
			}
		}));

	FAutoConsoleCommand NoiseFilterCommand(
		TEXT("mall.MicNoiseFilter"), TEXT("Set and save the basic microphone rumble/noise filter: 0 or 1."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			if (Args.Num() == 1 && (Args[0] == TEXT("0") || Args[0] == TEXT("1")))
			{
				GetMutableDefault<UMallMicrophoneSettings>()->SetNoiseFilter(Args[0] == TEXT("1"));
			}
			else
			{
				UE_LOG(LogMallMicrophoneSettings, Display, TEXT("Usage: mall.MicNoiseFilter 0|1. Current: %d"),
					GetDefault<UMallMicrophoneSettings>()->GetOptions().bNoiseFilter);
			}
		}));

	FAutoConsoleCommand ResetCommand(
		TEXT("mall.MicReset"), TEXT("Reset and save this user's microphone preferences."),
		FConsoleCommandDelegate::CreateLambda([]
		{
			GetMutableDefault<UMallMicrophoneSettings>()->ResetToDefaults();
		}));
}

FMallMicrophoneOptions UMallMicrophoneSettings::GetOptions() const
{
	FMallMicrophoneOptions Options;
	Options.GainDb = FMath::Clamp(FMath::IsFinite(GainDb) ? GainDb : 0.f, -12.f, 30.f);
	Options.bAutoGain = bAutoGain;
	Options.bNoiseFilter = bNoiseFilter;
	Options.MaxAutoGainDb = FMath::Clamp(FMath::IsFinite(MaxAutoGainDb) ? MaxAutoGainDb : 30.f, 0.f, 30.f);
	return Options;
}

void UMallMicrophoneSettings::SaveProfile()
{
	const FMallMicrophoneOptions Options = GetOptions();
	GainDb = Options.GainDb;
	MaxAutoGainDb = Options.MaxAutoGainDb;
	SaveConfig(CPF_Config, *GGameUserSettingsIni);
}

void UMallMicrophoneSettings::SetGainDb(float InGainDb)
{
	if (!FMath::IsFinite(InGainDb)) return;
	GainDb = FMath::Clamp(InGainDb, -12.f, 30.f);
	SaveProfile();
}

void UMallMicrophoneSettings::SetAutoGain(bool bEnabled)
{
	bAutoGain = bEnabled;
	SaveProfile();
}

void UMallMicrophoneSettings::SetNoiseFilter(bool bEnabled)
{
	bNoiseFilter = bEnabled;
	SaveProfile();
}

void UMallMicrophoneSettings::ResetToDefaults()
{
	GainDb = 0.f;
	bAutoGain = true;
	bNoiseFilter = true;
	MaxAutoGainDb = 30.f;
	SaveProfile();
}
