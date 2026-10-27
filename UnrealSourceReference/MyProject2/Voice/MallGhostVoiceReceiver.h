#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/TimerHandle.h"
#include "MallGhostVoiceReceiver.generated.h"

class ACharacter;
class APawn;
class APlayerController;
class UAudioComponent;
class UPointLightComponent;
class UTextRenderComponent;

/** Phrase reactions and requested book evidence over BP_StoreGhost's confined route follower. */
UCLASS(ClassGroup=(Mall), meta=(BlueprintSpawnableComponent))
class MYPROJECT2_API UMallGhostVoiceReceiver : public UActorComponent
{
	GENERATED_BODY()

public:
	UMallGhostVoiceReceiver();

	/** Call only after the owning speaker's server voice endpoint accepts a phrase. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mall|Voice")
	bool OnServerPhrase(APlayerController* Speaker, FName Phrase, float Confidence = 1.0f);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mall|Voice")
	bool HandleRecognizedPhrase(APawn* Speaker, FName Phrase, float Confidence = 1.0f);

	/** Multiplayer replacement for GetPlayerCharacter(0) in the ghost graph. */
	UFUNCTION(BlueprintPure, Category="Mall|Voice")
	static ACharacter* ResolveTargetPawn(AActor* Ghost);

	/** Forget a hunt-only preference when the ghost becomes harmless. */
	void ResetHuntingPreference();

	UFUNCTION(BlueprintPure, Category="Mall|Voice")
	bool IsSpeakerEligible(ACharacter* Speaker) const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Voice", meta=(ClampMin="100.0"))
	float HearingRangeCm = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Voice", meta=(ClampMin="0.0", ClampMax="1.0"))
	float MinimumConfidence = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Voice", meta=(ClampMin="0.5"))
	float MinimumPhraseInterval = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Voice", meta=(ClampMin="0.0", ClampMax="10.0"))
	float LeaveMeAloneSeconds = 3.0f;

	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category="Mall|Voice")
	FName LastPhrase;

	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category="Mall|Voice")
	int32 ReactionCount = 0;

	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category="Mall|Voice")
	TObjectPtr<ACharacter> LastSpeaker;

	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category="Mall|Voice")
	double LastReactionServerTime = -1.0;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastShowReaction(FName Phrase);

private:
	ACharacter* ChooseTargetPawn() const;
	static bool IsInsideGhostStore(const AActor* Ghost, const ACharacter* Character);
	static FName CanonicalPhrase(FName Phrase);
	bool ValidateGhostContract() const;
	void EnsureSignalComponents();
	void PlaySignalChime(float FrequencyHz);

	TWeakObjectPtr<ACharacter> PreferredSpeaker;
	double PreferredSpeakerUntil = 0.0;
	TMap<TWeakObjectPtr<APlayerController>, double> LastPhraseTimes;
	double SignalUntil = 0.0;
	FTimerHandle ChimeStopTimer;

	UPROPERTY(Transient)
	TObjectPtr<UPointLightComponent> SignalLight;

	UPROPERTY(Transient)
	TObjectPtr<UTextRenderComponent> SignalText;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> ChimeAudio;
};
