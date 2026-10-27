#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Equipment/MallCartItem.h"
#include "MallEscalatorFuse.generated.h"

class AMallPushCart;
class APawn;
class APlayerController;
class UBoxComponent;
class UPointLightComponent;
class UStaticMeshComponent;
class UTexture2D;

/**
 * Heavy-duty Escalator Relay Fuse module that players can pick up into the 4-slot Hotbar (F),
 * store on the Equipment Cart (E), drop (G), or insert into a Broken & Blocked Escalator (F)
 * to restore power and unlock vertical floor progression.
 */
UCLASS(BlueprintType, Blueprintable)
class MYPROJECT2_API AMallEscalatorFuse : public AActor, public IMallCartItem
{
	GENERATED_BODY()

public:
	AMallEscalatorFuse();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mall|Escalator|Fuse")
	TObjectPtr<UBoxComponent> InteractionBounds;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mall|Escalator|Fuse")
	TObjectPtr<UStaticMeshComponent> FuseBodyMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mall|Escalator|Fuse")
	TObjectPtr<UStaticMeshComponent> FuseCoreMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mall|Escalator|Fuse")
	TObjectPtr<UPointLightComponent> IndicatorLight;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Escalator|Fuse")
	TObjectPtr<UTexture2D> HotbarIcon;

	UPROPERTY(ReplicatedUsing=OnRep_FuseState, BlueprintReadOnly, Category="Mall|Escalator|Fuse")
	TObjectPtr<APawn> Carrier;

	UPROPERTY(ReplicatedUsing=OnRep_FuseState, BlueprintReadOnly, Category="Mall|Escalator|Fuse")
	TObjectPtr<AMallPushCart> StoredOnCart;

	UPROPERTY(ReplicatedUsing=OnRep_FuseState, BlueprintReadOnly, Category="Mall|Escalator|Fuse")
	bool bConsumedInEscalator = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Mall|Escalator|Fuse")
	int32 InventorySlotIndex = INDEX_NONE;

	static FText GetHotbarDisplayName();
	static AMallEscalatorFuse* FindCarriedBy(const APawn* Pawn);
	static AMallEscalatorFuse* FindFuseInView(const APawn* Pawn, const FVector& Origin, const FVector& Direction);

	UFUNCTION(BlueprintCallable, Category="Mall|Escalator|Fuse")
	bool TryPickup(APlayerController* Player);

	UFUNCTION(BlueprintCallable, Category="Mall|Escalator|Fuse")
	bool TryDrop(APlayerController* Player);

	UFUNCTION(BlueprintCallable, Category="Mall|Escalator|Fuse")
	bool ConsumeForEscalatorRepair(APawn* ConsumingPawn);

	// IMallCartItem interface
	virtual APawn* GetCartItemCarrier_Implementation() const override { return Carrier; }
	virtual AMallPushCart* GetCartStorage_Implementation() const override { return StoredOnCart; }
	virtual bool TryStoreOnCart_Implementation(APlayerController* Player, AMallPushCart* Cart, const FTransform& WorldTransform) override;
	virtual bool TryTakeFromCart_Implementation(APlayerController* Player, AMallPushCart* Cart) override;
	virtual void ReleaseFromCart_Implementation(AMallPushCart* Cart, const FTransform& WorldTransform) override;

protected:
	UFUNCTION()
	void OnRep_FuseState();

	void RefreshVisualAndHotbarState();

private:
	TWeakObjectPtr<APawn> PreviousCarrier;
	FTransform LastWorldTransform;
};

/**
 * Wall-mounted Mall Breaker / Escalator Service Panel that players can flip with F
 * to energize and repair any AMallEscalator sharing its LinkedBreakerTag.
 */
UCLASS(BlueprintType, Blueprintable)
class MYPROJECT2_API AMallEscalatorBreakerPanel : public AActor
{
	GENERATED_BODY()

public:
	AMallEscalatorBreakerPanel();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mall|Escalator|Breaker")
	TObjectPtr<UBoxComponent> PanelBounds;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mall|Escalator|Breaker")
	TObjectPtr<UStaticMeshComponent> CabinetMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mall|Escalator|Breaker")
	TObjectPtr<UStaticMeshComponent> HandleMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mall|Escalator|Breaker")
	TObjectPtr<UPointLightComponent> StatusLight;

	/** Matches AMallEscalator::LinkedBreakerTag on the escalator(s) controlled by this breaker. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Escalator|Breaker")
	FName LinkedBreakerTag = TEXT("EscalatorCircuit_Upper");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_BreakerTripped, Category="Mall|Escalator|Breaker")
	bool bBreakerTripped = true;

	UFUNCTION(BlueprintCallable, Category="Mall|Escalator|Breaker")
	bool TryResetBreaker(APlayerController* Player, FString& OutFeedback);

	static AMallEscalatorBreakerPanel* FindBreakerInView(const APawn* Pawn, const FVector& Origin, const FVector& Direction);

protected:
	UFUNCTION()
	void OnRep_BreakerTripped();

	void RefreshBreakerAppearance();
};
