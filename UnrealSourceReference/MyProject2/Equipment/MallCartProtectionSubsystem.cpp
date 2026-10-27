#include "Equipment/MallCartProtectionSubsystem.h"

#include "Environment/MallHidingSpot.h"
#include "Equipment/MallLightCart.h"
#include "Equipment/MallPushCart.h"
#include "Equipment/MallStoreCartDock.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "UObject/UnrealType.h"

namespace MallCartProtection
{
	bool ReadNumber(const AActor* Actor, FName Name, double& Result)
	{
		const FNumericProperty* Property = Actor ? FindFProperty<FNumericProperty>(Actor->GetClass(), Name) : nullptr;
		if (!Property) { return false; }
		const void* Value = Property->ContainerPtrToValuePtr<void>(Actor);
		Result = Property->IsFloatingPoint() ? Property->GetFloatingPointPropertyValue(Value) : static_cast<double>(Property->GetSignedIntPropertyValue(Value));
		return FMath::IsFinite(Result);
	}

	double GetMaxZAboveFloor(const AActor* Ghost)
	{
		return (IsValid(Ghost) && Ghost->ActorHasTag(TEXT("MultiFloorAnchorGhost"))) ? 1350.0 : 450.0;
	}

	bool InBounds(FVector Centre, float Radius, float HalfHeight, FVector Min, FVector Max, double FloorZ)
	{
		return Centre.X - Radius >= Min.X && Centre.X + Radius <= Max.X
			&& Centre.Y - Radius >= Min.Y && Centre.Y + Radius <= Max.Y
			&& Centre.Z - HalfHeight >= FloorZ - 40.0 && Centre.Z + HalfHeight <= Max.Z;
	}
}

bool UMallCartProtectionSubsystem::DoesSupportWorldType(EWorldType::Type Type) const
{
	return Type == EWorldType::Game || Type == EWorldType::PIE;
}

bool UMallCartProtectionSubsystem::GetStoreBounds(const AActor* Ghost, FVector& Min, FVector& Max, double& FloorZ)
{
	double MinX, MaxX, MinY, MaxY;
	if (!IsValid(Ghost) || !Ghost->ActorHasTag(TEXT("StoreGhostPrototype"))
		|| !MallCartProtection::ReadNumber(Ghost, TEXT("StoreMinX"), MinX)
		|| !MallCartProtection::ReadNumber(Ghost, TEXT("StoreMaxX"), MaxX)
		|| !MallCartProtection::ReadNumber(Ghost, TEXT("StoreMinY"), MinY)
		|| !MallCartProtection::ReadNumber(Ghost, TEXT("StoreMaxY"), MaxY)
		|| !MallCartProtection::ReadNumber(Ghost, TEXT("StoreFloorZ"), FloorZ) || MinX >= MaxX || MinY >= MaxY) { return false; }
	Min = FVector(MinX, MinY, FloorZ - 40.0);
	Max = FVector(MaxX, MaxY, FloorZ + MallCartProtection::GetMaxZAboveFloor(Ghost));
	return true;
}

bool UMallCartProtectionSubsystem::GetCapsule(const AActor* Actor, FVector& Centre, float& Radius, float& HalfHeight)
{
	const ACharacter* Character = Cast<ACharacter>(Actor);
	const UCapsuleComponent* Capsule = Character ? Character->GetCapsuleComponent() : nullptr;
	if (!IsValid(Capsule) || !Capsule->IsRegistered()) { return false; }
	Centre = Capsule->GetComponentLocation();
	Capsule->GetScaledCapsuleSize(Radius, HalfHeight);
	return !Centre.ContainsNaN() && FMath::IsFinite(Radius) && FMath::IsFinite(HalfHeight) && Radius > 0.0f && HalfHeight >= Radius;
}

AMallStoreCartDock* UMallCartProtectionSubsystem::FindDockForGhost(const AActor* Ghost) const
{
	if (!IsValid(Ghost) || Ghost->GetWorld() != GetWorld()) { return nullptr; }
	for (TActorIterator<AMallStoreCartDock> It(GetWorld()); It; ++It) { if (It->MatchesGhost(Ghost)) { return *It; } }
	return nullptr;
}

bool UMallCartProtectionSubsystem::IsGhostLocationProtected(AActor* Ghost, FVector CapsuleCentre, float Radius, float HalfHeight) const
{
	if (!GetWorld() || GetWorld()->GetNetMode() == NM_Client || !IsValid(Ghost) || !Ghost->HasAuthority() || Ghost->GetWorld() != GetWorld()) { return false; }
	for (TActorIterator<AMallLightCart> It(GetWorld()); It; ++It)
	{
		if (It->IsCapsuleProtected(CapsuleCentre, Radius, HalfHeight)) { return true; }
	}
	return false;
}

bool UMallCartProtectionSubsystem::IsPlayerSafe(AActor* Player) const
{
	if (!GetWorld() || GetWorld()->GetNetMode() == NM_Client || !IsValid(Player) || !Player->HasAuthority() || Player->GetWorld() != GetWorld()) { return false; }
	if (AMallHidingSpot::IsPlayerHiddenAndSafe(Player)) { return true; }
	FVector Centre; float Radius, HalfHeight;
	if (!GetCapsule(Player, Centre, Radius, HalfHeight)) { return false; }
	for (TActorIterator<AMallLightCart> It(GetWorld()); It; ++It) { if (It->IsCapsuleFullyProtected(Centre, Radius, HalfHeight)) { return true; } }
	return false;
}

bool UMallCartProtectionSubsystem::IsStoreSealed(AActor* Ghost) const
{
	if (!GetWorld() || GetWorld()->GetNetMode() == NM_Client || !IsValid(Ghost) || !Ghost->HasAuthority()) { return false; }
	const AMallStoreCartDock* Dock = FindDockForGhost(Ghost);
	return Dock && Dock->IsSealed();
}

bool UMallCartProtectionSubsystem::IsActorInsideHomeStore(AActor* Ghost, AActor* Other) const
{
	FVector Min, Max, Centre; float Radius, HalfHeight; double FloorZ;
	if (!IsValid(Other) || !IsValid(Ghost) || Other->GetWorld() != Ghost->GetWorld()
		|| !GetStoreBounds(Ghost, Min, Max, FloorZ) || !GetCapsule(Other, Centre, Radius, HalfHeight)) { return false; }
	return MallCartProtection::InBounds(Centre, Radius, HalfHeight, Min, Max, FloorZ);
}

bool UMallCartProtectionSubsystem::IsGhostPositionAllowed(AActor* Ghost, FVector Position) const
{
	if (!GetWorld() || GetWorld()->GetNetMode() == NM_Client || !IsValid(Ghost) || !Ghost->HasAuthority()
		|| Ghost->GetWorld() != GetWorld() || Position.ContainsNaN()) { return false; }
	FVector Centre, Min, Max; float Radius, HalfHeight; double FloorZ;
	if (!GetCapsule(Ghost, Centre, Radius, HalfHeight) || !GetStoreBounds(Ghost, Min, Max, FloorZ)) { return false; }
	if (IsStoreSuppressedByCart(Ghost) || IsGhostLocationProtected(Ghost, Position, Radius, HalfHeight)) { return false; }
	const AMallStoreCartDock* Dock = FindDockForGhost(Ghost);
	if (!Ghost->ActorHasTag(TEXT("MallCartBoundary")) || !Dock || Dock->IsSealed())
	{
		return MallCartProtection::InBounds(Position, Radius, HalfHeight, Min, Max, FloorZ);
	}
	const float Range = FMath::IsFinite(Dock->UnsealedRoamDistanceCm) ? FMath::Clamp(Dock->UnsealedRoamDistanceCm, 1000.0f, 30000.0f) : 10000.0f;
	const FVector HomeCentre((Min.X + Max.X) * 0.5, (Min.Y + Max.Y) * 0.5, FloorZ);
	return Position.Z - HalfHeight >= FloorZ - 40.0 && Position.Z + HalfHeight <= Max.Z
		&& FVector::DistSquared2D(Position, HomeCentre) <= FMath::Square(Range);
}

bool UMallCartProtectionSubsystem::IsStoreSuppressedByCart(AActor* Ghost) const
{
	if (!GetWorld() || GetWorld()->GetNetMode() == NM_Client || !IsValid(Ghost) || !Ghost->HasAuthority()
		|| !Ghost->ActorHasTag(TEXT("MallCartBoundary"))) { return false; }
	const AMallStoreCartDock* Dock = FindDockForGhost(Ghost);
	FVector Min, Max; double FloorZ;
	if (!Dock || !GetStoreBounds(Ghost, Min, Max, FloorZ)) { return false; }
	for (TActorIterator<AMallPushCart> It(GetWorld()); It; ++It)
	{
		const AMallPushCart* Cart = *It;
		if (!IsValid(Cart) || Cart->IsActorBeingDestroyed() || Cart->IsA<AMallLightCart>()
			|| (Dock->AssignedCart && Dock->AssignedCart != Cart)) { continue; }
		FVector Centre, Extent;
		Cart->GetActorBounds(true, Centre, Extent, false);
		if (Centre.ContainsNaN() || Extent.ContainsNaN() || Extent.IsNearlyZero()) { continue; }
		if (Centre.X - Extent.X >= Min.X && Centre.X + Extent.X <= Max.X
			&& Centre.Y - Extent.Y >= Min.Y && Centre.Y + Extent.Y <= Max.Y
			&& Centre.Z - Extent.Z >= FloorZ - 20.0 && Centre.Z + Extent.Z <= FloorZ + 200.0) { return true; }
	}
	return false;
}

bool UMallCartProtectionSubsystem::IsGhostSegmentAllowed(AActor* Ghost, FVector StartCentre, FVector EndCentre) const
{
	if (StartCentre.ContainsNaN() || EndCentre.ContainsNaN()) { return false; }
	const double Distance = FVector::Dist(StartCentre, EndCentre);
	if (!FMath::IsFinite(Distance) || Distance > 60000.0) { return false; }
	const int32 Steps = FMath::Max(1, FMath::CeilToInt(Distance / 20.0));
	for (int32 Index = 0; Index <= Steps; ++Index)
	{
		if (!IsGhostPositionAllowed(Ghost, FMath::Lerp(StartCentre, EndCentre, static_cast<double>(Index) / Steps))) { return false; }
	}
	return true;
}

bool UMallCartProtectionSubsystem::CanGhostMaintainHuntForPlayer(AActor* Ghost, AActor* Player, bool bRequireHomeStore) const
{
	if (!GetWorld() || GetWorld()->GetNetMode() == NM_Client || !IsValid(Ghost) || !Ghost->HasAuthority()
		|| Ghost->GetWorld() != GetWorld() || !IsValid(Player) || Player->GetWorld() != GetWorld()
		|| IsStoreSuppressedByCart(Ghost)) { return false; }
	const APawn* Pawn = Cast<APawn>(Player);
	const APlayerController* Controller = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!AMallPushCart::IsEligiblePlayer(Controller) || Controller->GetPawn() != Pawn) { return false; }
	const AMallStoreCartDock* Dock = FindDockForGhost(Ghost);
	if (bRequireHomeStore || !Ghost->ActorHasTag(TEXT("MallCartBoundary")) || IsStoreSealed(Ghost) || !Dock)
	{
		return IsActorInsideHomeStore(Ghost, Player);
	}
	FVector Centre, Min, Max; float Radius, HalfHeight; double FloorZ;
	if (!GetCapsule(Player, Centre, Radius, HalfHeight) || !GetStoreBounds(Ghost, Min, Max, FloorZ)) { return false; }
	const float Range = FMath::IsFinite(Dock->UnsealedRoamDistanceCm) ? FMath::Clamp(Dock->UnsealedRoamDistanceCm, 1000.0f, 30000.0f) : 10000.0f;
	const FVector HomeCentre((Min.X + Max.X) * 0.5, (Min.Y + Max.Y) * 0.5, FloorZ);
	return Centre.Z - HalfHeight >= FloorZ - 40.0 && Centre.Z + HalfHeight <= Max.Z
		&& FVector::DistSquared2D(Centre, HomeCentre) <= FMath::Square(Range);
}

bool UMallCartProtectionSubsystem::CanGhostPursueTarget(AActor* Ghost, AActor* Player, bool bRequireHomeStore) const
{
	if (!CanGhostMaintainHuntForPlayer(Ghost, Player, bRequireHomeStore) || IsPlayerSafe(Player)) { return false; }
	FVector PlayerCentre, GhostCentre;
	float PlayerRadius, PlayerHalfHeight, GhostRadius, GhostHalfHeight;
	if (!GetCapsule(Player, PlayerCentre, PlayerRadius, PlayerHalfHeight)
		|| !GetCapsule(Ghost, GhostCentre, GhostRadius, GhostHalfHeight)) { return false; }
	// Navigation targets the player's feet. A crouched player's centre is lower than
	// the taller ghost's centre, but both capsules must stand on the same surface.
	const FVector Destination = PlayerCentre + FVector(0.0, 0.0, GhostHalfHeight - PlayerHalfHeight);
	return IsGhostPositionAllowed(Ghost, Destination);
}
