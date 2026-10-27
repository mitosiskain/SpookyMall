#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Equipment/MallCartItem.h"
#include "Engine/NetSerialization.h"
#include "MallSpiritBox.generated.h"

class APawn;
class APlayerController;
class UAudioComponent;
class UBoxComponent;
class UPointLightComponent;
class USceneComponent;
class USoundBase;
class UStaticMeshComponent;
class UTextRenderComponent;
class UTexture2D;

/** Add several entries for a phrase to cycle through alternate recordings on the server. */
USTRUCT(BlueprintType)
struct MYPROJECT2_API FMallSpiritBoxResponse
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Ghost|Spirit Box")
	FName PhraseId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Ghost|Spirit Box")
	TObjectPtr<USoundBase> Sound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Ghost|Spirit Box")
	FString Subtitle;
};

/** A portable, server-owned radio. Persistent state replicates; response audio is a live event. */
UCLASS(Blueprintable)
class MYPROJECT2_API AMallSpiritBox : public AActor, public IMallCartItem
{
	GENERATED_BODY()

public:
	AMallSpiritBox();

	virtual APawn* GetCartItemCarrier_Implementation() const override { return GetCarrier(); }
	virtual AMallPushCart* GetCartStorage_Implementation() const override { return StoredOnCart; }
	virtual bool TryStoreOnCart_Implementation(APlayerController* Player, AMallPushCart* Cart, const FTransform& WorldTransform) override;
	virtual bool TryTakeFromCart_Implementation(APlayerController* Player, AMallPushCart* Cart) override;
	virtual void ReleaseFromCart_Implementation(AMallPushCart* Cart, const FTransform& WorldTransform) override;
	UPROPERTY(ReplicatedUsing=OnRep_InteractionState, VisibleAnywhere, BlueprintReadOnly, Category="Mall|Cart")
	TObjectPtr<AMallPushCart> StoredOnCart;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Ghost|Spirit Box")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Ghost|Spirit Box")
	TObjectPtr<UTexture2D> Icon;

	UPROPERTY(ReplicatedUsing=OnRep_InteractionState, VisibleAnywhere, BlueprintReadOnly, Category="Mall|Ghost|Spirit Box")
	int32 CarriedHotbarSlot = INDEX_NONE;

	UPROPERTY(ReplicatedUsing=OnRep_InteractionState, VisibleAnywhere, BlueprintReadOnly, Category="Mall|Ghost|Spirit Box")
	int32 CarrierSelectedSlot = 0;

	UFUNCTION(BlueprintPure, Category="Mall|Ghost|Spirit Box")
	bool IsSelectedByCarrier() const;

	void SetCarrierSelectedSlot(int32 NewSelectedSlot);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mall|Ghost|Spirit Box")
	bool TryPickup(APlayerController* Player);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mall|Ghost|Spirit Box")
	bool TryPlace(APlayerController* Player, const FTransform& Placement);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mall|Ghost|Spirit Box")
	bool TrySetPowered(APlayerController* Player, bool bNewPowered);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mall|Ghost|Spirit Box")
	bool TryCycleChannel(APlayerController* Player);

	UFUNCTION(BlueprintPure, Category="Mall|Ghost|Spirit Box")
	FString GetCurrentChannelName() const;

	UFUNCTION(BlueprintImplementableEvent, Category="Mall|Ghost|Spirit Box")
	void BP_OnPowerChanged(bool bNewPowered);

	UFUNCTION(BlueprintImplementableEvent, Category="Mall|Ghost|Spirit Box")
	void BP_OnChannelChanged(int32 NewChannelIndex, const FString& ChannelName);

	UFUNCTION(BlueprintPure, Category="Mall|Ghost|Spirit Box")
	APawn* GetCarrier() const;

	UFUNCTION(BlueprintPure, Category="Mall|Ghost|Spirit Box")
	bool IsPowered() const { return bPowered; }

	UPROPERTY(ReplicatedUsing=OnRep_Channel, EditAnywhere, BlueprintReadWrite, Category="Mall|Ghost|Spirit Box")
	int32 ChannelIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Ghost|Spirit Box")
	TArray<FString> ChannelNames;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mall|Ghost|Spirit Box")
	bool TryRespond(AActor* Ghost, APlayerController* Speaker, FName Phrase);

	/** Use mono clips for positional playback. Each phrase can have any number of variants. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Ghost|Spirit Box")
	TArray<FMallSpiritBoxResponse> Responses;

	/** Optional quiet looping radio bed; never required for a response. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Ghost|Spirit Box")
	TObjectPtr<USoundBase> ScanSound;

	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category="Mall|Ghost|Spirit Box")
	int32 ResponseCount = 0;

	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category="Mall|Ghost|Spirit Box")
	FName LastPhraseId;

	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category="Mall|Ghost|Spirit Box")
	FString LastSubtitle;

	/** Server-selected radio position when the latest reply began. Never replays audio on replication. */
	UPROPERTY(Replicated, VisibleAnywhere, BlueprintReadOnly, Category="Mall|Ghost|Spirit Box")
	FVector_NetQuantize LastResponseLocation = FVector::ZeroVector;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FMallSpiritBoxTest;
	friend class FMallSpiritBoxNetworkTest;
#endif

	UFUNCTION() void OnRep_InteractionState();
	UFUNCTION() void OnRep_Power();
	UFUNCTION() void OnRep_Channel();
	UFUNCTION() void OnCarrierDestroyed(AActor* DestroyedActor);
	UFUNCTION() void OnScanFinished();
	UFUNCTION() void OnResponseFinished();
	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayResponse(USoundBase* Sound, const FString& Subtitle, FName Phrase, int32 Serial,
		FVector_NetQuantize SourceLocation);

	bool IsPlayerEligible(APlayerController* Player) const;
	bool CanReachPlacedBox(APlayerController* Player) const;
	bool ValidatePlacement(APlayerController* Player, const FTransform& Requested, FTransform& Supported) const;
	void FollowCarrier();
	void ClearCarrier();
	void RestoreLastPlacement();
	void RefreshPresentation();
	void SyncHotbarState();
	void RefreshAudio();

	UPROPERTY(ReplicatedUsing=OnRep_InteractionState, VisibleAnywhere, Category="Mall|Ghost|Spirit Box")
	TObjectPtr<APawn> Carrier;

	UPROPERTY(ReplicatedUsing=OnRep_Power, VisibleAnywhere, Category="Mall|Ghost|Spirit Box")
	bool bPowered = false;

	TWeakObjectPtr<APlayerController> CarrierController;
	TWeakObjectPtr<APawn> LastSyncedCarrier;
	int32 LastSyncedSlot = INDEX_NONE;
	bool bLastSelectedByCarrier = true;
	FTransform LastPlacedTransform = FTransform::Identity;
	bool bHasLastPlacedTransform = false;
	double LastResponseTime = -BIG_NUMBER;
	double ResponseCooldownSeconds = 4.0;
	bool bResponseActive = false;
	double SubtitleUntil = 0.0;
	TMap<FName, int32> NextVariantIndex;
	int32 LastPlayedResponseSerial = 0;
	int32 LocalResponsePlaybackCount = 0;

	UPROPERTY(VisibleAnywhere, Category="Mall|Ghost|Spirit Box") TObjectPtr<USceneComponent> RadioRoot;
	UPROPERTY(VisibleAnywhere, Category="Mall|Ghost|Spirit Box") TObjectPtr<UBoxComponent> InteractionBounds;
	UPROPERTY(VisibleAnywhere, Category="Mall|Ghost|Spirit Box") TObjectPtr<UStaticMeshComponent> Casing;
	UPROPERTY(VisibleAnywhere, Category="Mall|Ghost|Spirit Box") TObjectPtr<UStaticMeshComponent> Antenna;
	UPROPERTY(VisibleAnywhere, Category="Mall|Ghost|Spirit Box") TObjectPtr<UTextRenderComponent> DeviceLabel;
	UPROPERTY(VisibleAnywhere, Category="Mall|Ghost|Spirit Box") TObjectPtr<UTextRenderComponent> PowerIndicator;
	UPROPERTY(VisibleAnywhere, Category="Mall|Ghost|Spirit Box") TObjectPtr<UTextRenderComponent> ResponseSubtitle;
	UPROPERTY(VisibleAnywhere, Category="Mall|Ghost|Spirit Box") TObjectPtr<UPointLightComponent> PowerLight;
	UPROPERTY(VisibleAnywhere, Category="Mall|Ghost|Spirit Box") TObjectPtr<UAudioComponent> ScanAudio;
	UPROPERTY(VisibleAnywhere, Category="Mall|Ghost|Spirit Box") TObjectPtr<UAudioComponent> ResponseAudio;
};
