#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "MallMicrophoneProcessor.h"
#include "MallMicrophoneSettings.generated.h"

// These capture preferences belong to this Windows user, never to replicated game settings.
UCLASS(Config=GameUserSettings)
class MYPROJECT2_API UMallMicrophoneSettings : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(Config) float GainDb = 0.f;
	UPROPERTY(Config) bool bAutoGain = true;
	UPROPERTY(Config) bool bNoiseFilter = true;
	UPROPERTY(Config) float MaxAutoGainDb = 30.f;

	FMallMicrophoneOptions GetOptions() const;
	void SetGainDb(float InGainDb);
	void SetAutoGain(bool bEnabled);
	void SetNoiseFilter(bool bEnabled);
	void ResetToDefaults();

private:
	void SaveProfile();
};
