#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MallGhostCartHelpers.generated.h"

class ACharacter;

/** Opt-in cart protection layered onto the existing validated Blueprint route follower. */
UCLASS()
class MYPROJECT2_API UMallGhostCartHelpers : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/** Replaces only the Blueprint's horizontal store-bound check; physical/nav/floor checks remain. */
	UFUNCTION(BlueprintPure, Category="Mall|Ghost|Carts")
	static bool IsGhostCenterAllowed(AActor* Ghost, FVector Center);

	UFUNCTION(BlueprintPure, Category="Mall|Ghost|Carts")
	static bool IsGhostFeetAllowed(AActor* Ghost, FVector Feet);

	UFUNCTION(BlueprintPure, Category="Mall|Ghost|Carts")
	static bool CanGhostPursuePlayer(AActor* Ghost, AActor* Player);

	UFUNCTION(BlueprintPure, Category="Mall|Ghost|Carts")
	static ACharacter* ResolveTargetPawn(AActor* Ghost);

	/** Rechecks actual target and protection at the instant of a Blueprint catch. */
	UFUNCTION(BlueprintPure, Category="Mall|Ghost|Carts")
	static bool CanGhostCatchPlayer(AActor* Ghost, AActor* Player);

	/** Start/End are navigation foot positions, matching CurrentValidWaypoints. */
	UFUNCTION(BlueprintPure, Category="Mall|Ghost|Carts")
	static bool IsGhostRouteSegmentAllowed(AActor* Ghost, FVector StartFeet, FVector EndFeet);

	UFUNCTION(BlueprintPure, Category="Mall|Ghost|Carts")
	static bool CanFollowGhostWaypoint(AActor* Ghost, FVector EndFeet);

	/** Tick before the Blueprint guard: stops stale AI moves and recovers from newly moved protection. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Mall|Ghost|Carts")
	static void GuardCartGhostMovement(AActor* Ghost);
};
