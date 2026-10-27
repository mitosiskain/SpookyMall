#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "InputCoreTypes.h"
#include "MallVoiceSettings.generated.h"

USTRUCT(BlueprintType)
struct FMallVoicePhrase
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category="Speech") FName Id;
	UPROPERTY(EditAnywhere, Category="Speech") TArray<FString> Aliases;
};

/** Push-to-talk audio is relayed to nearby players; speech recognition runs locally. */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Mall Voice Prototype"))
class MYPROJECT2_API UMallVoiceSettings : public UDeveloperSettings
{
	GENERATED_BODY()
public:
	UMallVoiceSettings();
	virtual FName GetCategoryName() const override { return TEXT("Game"); }
	UPROPERTY(EditAnywhere, Config, Category="Voice") bool bEnabled = true;
	UPROPERTY(EditAnywhere, Config, Category="Voice") FKey PushToTalkKey;
	UPROPERTY(EditAnywhere, Config, Category="Voice", meta=(ClampMin="0", ClampMax="10000")) float FullVolumeDistanceCm = 200.f;
	UPROPERTY(EditAnywhere, Config, Category="Voice", meta=(ClampMin="100", ClampMax="10000")) float MaxVoiceDistanceCm = 2000.f;
	UPROPERTY(EditAnywhere, Config, Category="Speech") bool bSpeechRecognitionEnabled = true;
	/** Zero accepts matched phrases regardless of the recognizer confidence score. */
	UPROPERTY(EditAnywhere, Config, Category="Speech", meta=(ClampMin="0", ClampMax="1")) float MinimumPhraseConfidence = 0.0f;
	UPROPERTY(EditAnywhere, Config, Category="Speech") TArray<FMallVoicePhrase> Phrases;
	UPROPERTY(EditAnywhere, Config, Category="Display") bool bShowVoiceStatus = true;
	/** Host/session capacity; gameplay targets four initially, larger sessions need balancing/tests. */
	UPROPERTY(EditAnywhere, Config, Category="Multiplayer", meta=(ClampMin="1", ClampMax="64")) int32 DefaultMaxPlayers = 4;
	TMap<FName, TArray<FString>> GetPhraseAliases() const;
};
