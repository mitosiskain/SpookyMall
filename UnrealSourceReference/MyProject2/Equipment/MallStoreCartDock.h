#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MallStoreCartDock.generated.h"

class AMallPushCart;
class UBoxComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

/** A marked equipment-cart parking position that seals exactly its assigned ghost's store. */
UCLASS(Blueprintable)
class MYPROJECT2_API AMallStoreCartDock : public AActor
{
	GENERATED_BODY()
public:
	AMallStoreCartDock();

	UFUNCTION(BlueprintPure, Category="Mall|Cart|Protection")
	bool IsSealed() const;
	UFUNCTION(BlueprintPure, Category="Mall|Cart|Protection")
	bool IsCartCorrectlyParked(const AMallPushCart* Cart) const;
	UFUNCTION(BlueprintPure, Category="Mall|Cart|Protection")
	bool MatchesGhost(const AActor* Ghost) const;

	/** Match an actor tag on the store ghost, e.g. East_F0_02. None never seals a store. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Cart|Protection")
	FName StoreTag = NAME_None;
	/** Optional explicit equipment cart. If unset, any parked non-light cart may occupy this dock. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Cart|Protection")
	TObjectPtr<AMallPushCart> AssignedCart;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Cart|Protection")
	bool bEnabled = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Cart|Protection", meta=(ClampMin="20.0", ClampMax="250.0", Units="cm"))
	float ParkingToleranceCm = 35.0f;
	/** Distance from the doorway plane, whose normal is dock local Y. Cart local X spans the doorway. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Cart|Protection", meta=(ClampMin="5.0", ClampMax="50.0", Units="cm"))
	float ThresholdDepthToleranceCm = 25.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Cart|Protection", meta=(ClampMin="5.0", ClampMax="90.0"))
	float FacingToleranceDegrees = 35.0f;
	/** Unsealed prototype ghosts may roam on their own floor, within this distance of their home store. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mall|Cart|Protection", meta=(ClampMin="1000.0", ClampMax="30000.0", Units="cm"))
	float UnsealedRoamDistanceCm = 10000.0f;
	UPROPERTY(ReplicatedUsing=OnRep_Sealed, VisibleAnywhere, BlueprintReadOnly, Category="Mall|Cart|Protection")
	bool bStoreSealed = false;
	UPROPERTY(ReplicatedUsing=OnRep_Sealed, VisibleAnywhere, BlueprintReadOnly, Category="Mall|Cart|Protection")
	bool bStoreSuppressed = false;

	virtual void Tick(float DeltaSeconds) override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
private:
	UFUNCTION()
	void OnRep_Sealed();
	void RefreshDisplay();
	UPROPERTY(VisibleAnywhere, Category="Mall|Cart|Protection")
	TObjectPtr<UBoxComponent> DockRoot;
	UPROPERTY(VisibleAnywhere, Category="Mall|Cart|Protection")
	TObjectPtr<UStaticMeshComponent> ParkingMarker;
	UPROPERTY(VisibleAnywhere, Category="Mall|Cart|Protection")
	TObjectPtr<UTextRenderComponent> DockDisplay;
};
