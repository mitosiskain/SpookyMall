#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MallCartProtectionSubsystem.generated.h"

class AMallStoreCartDock;

/** Server-only decisions shared by target selection, path validation and catch checks. */
UCLASS()
class MYPROJECT2_API UMallCartProtectionSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintPure, Category="Mall|Cart|Protection")
	bool IsGhostLocationProtected(AActor* Ghost, FVector CapsuleCentre, float Radius, float HalfHeight) const;
	UFUNCTION(BlueprintPure, Category="Mall|Cart|Protection")
	bool IsPlayerSafe(AActor* Player) const;
	UFUNCTION(BlueprintPure, Category="Mall|Cart|Protection")
	bool IsStoreSealed(AActor* Ghost) const;
	UFUNCTION(BlueprintPure, Category="Mall|Cart|Protection")
	bool IsStoreSuppressedByCart(AActor* Ghost) const;
	UFUNCTION(BlueprintPure, Category="Mall|Cart|Protection")
	bool IsGhostPositionAllowed(AActor* Ghost, FVector Position) const;
	UFUNCTION(BlueprintPure, Category="Mall|Cart|Protection")
	bool IsGhostSegmentAllowed(AActor* Ghost, FVector StartCentre, FVector EndCentre) const;
	UFUNCTION(BlueprintPure, Category="Mall|Cart|Protection")
	bool IsActorInsideHomeStore(AActor* Ghost, AActor* Other) const;
	UFUNCTION(BlueprintPure, Category="Mall|Cart|Protection")
	bool CanGhostPursueTarget(AActor* Ghost, AActor* Player, bool bRequireHomeStore = false) const;
	UFUNCTION(BlueprintPure, Category="Mall|Cart|Protection")
	bool CanGhostMaintainHuntForPlayer(AActor* Ghost, AActor* Player, bool bRequireHomeStore = false) const;

	AMallStoreCartDock* FindDockForGhost(const AActor* Ghost) const;
	static bool GetStoreBounds(const AActor* Ghost, FVector& Min, FVector& Max, double& FloorZ);
	static bool GetCapsule(const AActor* Actor, FVector& Centre, float& Radius, float& HalfHeight);

protected:
	virtual bool DoesSupportWorldType(EWorldType::Type Type) const override;
};
