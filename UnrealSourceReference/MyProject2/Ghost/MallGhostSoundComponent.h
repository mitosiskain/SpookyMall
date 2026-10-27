#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/NetSerialization.h"
#include "Ghost/MallGhostInteractionComponent.h"
#include "MallGhostSoundComponent.generated.h"

class UAudioComponent;
class USoundBase;

UENUM(BlueprintType)
enum class EMallGhostSoundKind : uint8
{
	Invisible,
	Hunt
};

/** Server-scheduled physical ghost sounds. Independent of voice recognition and the spirit box. */
UCLASS(ClassGroup=(Mall), meta=(BlueprintSpawnableComponent))
class MYPROJECT2_API UMallGhostSoundComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMallGhostSoundComponent();

	/** Uses current mode, store, live-player and cooldown checks; does not bypass them. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mall|Ghost|Sounds")
	bool TryPlayCue();

	/** Disables automatic scheduling only; explicit eligible requests still work. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Ghost|Sounds")
	bool bAutomaticSounds = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Ghost|Sounds")
	TArray<TSoftObjectPtr<USoundBase>> InvisibleSounds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Ghost|Sounds")
	TArray<TSoftObjectPtr<USoundBase>> HuntSounds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Ghost|Sounds", meta=(ClampMin="0.0", ClampMax="4.0"))
	float InvisibleVolume = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Ghost|Sounds", meta=(ClampMin="0.0", ClampMax="4.0"))
	float HuntVolume = 0.75f;

	/** Random silent gaps after a clip finishes, in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Ghost|Sounds", meta=(ClampMin="0.25", ClampMax="120.0", Units="s"))
	float InvisibleMinGap = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Ghost|Sounds", meta=(ClampMin="0.25", ClampMax="120.0", Units="s"))
	float InvisibleMaxGap = 16.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Ghost|Sounds", meta=(ClampMin="0.25", ClampMax="120.0", Units="s"))
	float HuntMinGap = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Ghost|Sounds", meta=(ClampMin="0.25", ClampMax="120.0", Units="s"))
	float HuntMaxGap = 6.0f;

	/** Debug history only: replicated metadata never starts an old sound for late joiners. */
	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category="Mall|Ghost|Sounds")
	int32 CueCount = 0;

	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category="Mall|Ghost|Sounds")
	EMallGhostSoundKind LastCueKind = EMallGhostSoundKind::Invisible;

	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category="Mall|Ghost|Sounds")
	TObjectPtr<USoundBase> LastSound;

	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category="Mall|Ghost|Sounds")
	FVector_NetQuantize LastSourceLocation = FVector::ZeroVector;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	friend class FMallGhostSoundNetworkTest;

	bool GetEligibleKind(EMallGhostSoundKind& OutKind) const;
	bool IsEligiblePlayer(const class APawn* Pawn) const;
	bool IsEligibleChase() const;
	void StopCue();
	void StopLocalAudio();
	double CueGap(EMallGhostSoundKind Kind) const;

	/** A null Sound is a stop event; serials include stops so stale events cannot restart a cue. */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastCue(USoundBase* Sound, float Volume, int32 Serial);

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> ActiveAudio;

	double ActiveUntil = 0.0;
	double NextCueTime = 0.0;
	bool bActiveCue = false;
	bool bObservedChasing = false;
	EMallGhostMode ObservedMode = EMallGhostMode::InvisibleDisturbance;
	int32 NextEventSerial = 0;
	int32 LastAppliedSerial = 0;
	int32 LocalPlaybackCount = 0;
};
