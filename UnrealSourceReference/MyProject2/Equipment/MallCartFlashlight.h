#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Equipment/MallCartItem.h"
#include "MallCartFlashlight.generated.h"

class APlayerController;
class UBoxComponent;
class USceneComponent;
class UStaticMeshComponent;
class UTexture2D;

/** Replicated inventory flashlight; the existing character Blueprint continues rendering the held tool. */
UCLASS(Blueprintable)
class MYPROJECT2_API AMallCartFlashlight : public AActor, public IMallCartItem
{
	GENERATED_BODY()
public:
	AMallCartFlashlight();
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mall|Cart|Flashlight")
	bool TryPickup(APlayerController* Player);
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mall|Cart|Flashlight")
	bool TryDrop(APlayerController* Player);
	virtual APawn* GetCartItemCarrier_Implementation() const override { return Carrier; }
	virtual AMallPushCart* GetCartStorage_Implementation() const override { return StoredOnCart; }
	virtual bool TryStoreOnCart_Implementation(APlayerController* Player, AMallPushCart* Cart, const FTransform& WorldTransform) override;
	virtual bool TryTakeFromCart_Implementation(APlayerController* Player, AMallPushCart* Cart) override;
	virtual void ReleaseFromCart_Implementation(AMallPushCart* Cart, const FTransform& WorldTransform) override;
	bool IsSelectedByCarrier() const;
	UFUNCTION(BlueprintPure, Category="Mall|Cart|Flashlight")
	static AMallCartFlashlight* FindCarriedBy(const APawn* Pawn);
	UFUNCTION(BlueprintPure, Category="Mall|Cart|Flashlight")
	static bool CanPawnUseFlashlight(const APawn* Pawn);
	UFUNCTION(BlueprintCallable, Category="Mall|Cart|Flashlight")
	static bool TogglePawnFlashlight(APawn* Pawn);
	static int32 GetSelectedInventorySlot(const APawn* Pawn);
	static bool GetInventoryLightOn(const APawn* Pawn);
	static void SetInventoryLightOn(APawn* Pawn, bool bLightOn);
	static void UpdateOwnerState(APlayerController* Player, int32 SelectedSlot, bool bLightOn);
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Props) const override;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Cart|Flashlight") FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Cart|Flashlight") TObjectPtr<UTexture2D> Icon;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Cart|Flashlight") float ItemValue = 0;
	UPROPERTY(ReplicatedUsing=OnRep_ItemState, VisibleAnywhere, BlueprintReadOnly, Category="Mall|Cart|Flashlight") TObjectPtr<APawn> Carrier;
	UPROPERTY(ReplicatedUsing=OnRep_ItemState, VisibleAnywhere, BlueprintReadOnly, Category="Mall|Cart|Flashlight") TObjectPtr<AMallPushCart> StoredOnCart;
	UPROPERTY(ReplicatedUsing=OnRep_ItemState, VisibleAnywhere, BlueprintReadOnly, Category="Mall|Cart|Flashlight") bool bCarrierLightOn = false;
	UPROPERTY(ReplicatedUsing=OnRep_ItemState, VisibleAnywhere, BlueprintReadOnly, Category="Mall|Cart|Flashlight") int32 CarrierSelectedSlot = 0;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
	UFUNCTION() void OnRep_ItemState();
	bool CanReceive(APlayerController* Player) const;
	void MirrorInventories();
	void ApplyPresentation();
	void RecoverWorldPlacement();
	UPROPERTY(ReplicatedUsing=OnRep_ItemState) TArray<TObjectPtr<APawn>> KnownOwners;
	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> ItemRoot;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UBoxComponent> InteractionBounds;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mall|Cart|Flashlight", meta=(AllowPrivateAccess="true")) TObjectPtr<UStaticMeshComponent> FlashlightModel;
	// Local presentation state only: equip once per ownership transition, then honor manual hotbar selection.
	TWeakObjectPtr<APawn> LastLocallyEquippedCarrier;
	FTransform LastWorldTransform = FTransform::Identity;
	bool bHasWorldPlacement = false;
};
