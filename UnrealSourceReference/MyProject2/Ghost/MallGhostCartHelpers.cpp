#include "Ghost/MallGhostCartHelpers.h"

#include "Environment/MallHidingSpot.h"
#include "Environment/MallSpaAndFoodSystem.h"
#include "Equipment/MallCartProtectionSubsystem.h"
#include "Ghost/MallGhostInteractionComponent.h"
#include "Voice/MallGhostVoiceReceiver.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

namespace MallGhostCarts
{
	bool IsCartGhost(const AActor* Ghost)
	{
		return IsValid(Ghost) && Ghost->ActorHasTag(TEXT("MallCartBoundary"));
	}

	UMallCartProtectionSubsystem* Protection(AActor* Ghost)
	{
		return IsValid(Ghost) && Ghost->HasAuthority() && Ghost->GetWorld()
			? Ghost->GetWorld()->GetSubsystem<UMallCartProtectionSubsystem>() : nullptr;
	}

	void StopRoute(AActor* Ghost)
	{
		UFunction* Clear = IsValid(Ghost) ? Ghost->FindFunction(TEXT("ClearRoute")) : nullptr;
		if (Clear && Clear->NumParms == 0) { Ghost->ProcessEvent(Clear, nullptr); }
		if (ACharacter* Character = Cast<ACharacter>(Ghost))
		{
			if (UCharacterMovementComponent* Move = Character->GetCharacterMovement()) { Move->StopMovementImmediately(); }
		}
	}

	bool ReadVector(AActor* Actor, FName Name, FVector& Value)
	{
		const FStructProperty* Property = Actor ? FindFProperty<FStructProperty>(Actor->GetClass(), Name) : nullptr;
		if (!Property || Property->Struct != TBaseStructure<FVector>::Get()) { return false; }
		Value = *Property->ContainerPtrToValuePtr<FVector>(Actor);
		return !Value.ContainsNaN();
	}

	// Preserve the authored Blueprint's actual capsule and navigation checks for recovery.
	bool ValidateRecovery(AActor* Ghost, FVector Candidate, FVector& Result)
	{
		UFunction* Function = Ghost->FindFunction(TEXT("CheckSafeCenter"));
		const FStructProperty* Center = Function ? FindFProperty<FStructProperty>(Function, TEXT("Center")) : nullptr;
		const FStructProperty* Feet = Function ? FindFProperty<FStructProperty>(Function, TEXT("NavFeet")) : nullptr;
		const FBoolProperty* RequireNav = Function ? FindFProperty<FBoolProperty>(Function, TEXT("RequireNav")) : nullptr;
		const FBoolProperty* Safe = Function ? FindFProperty<FBoolProperty>(Function, TEXT("Safe")) : nullptr;
		if (!Center || !Feet || !RequireNav || !Safe
			|| Center->Struct != TBaseStructure<FVector>::Get() || Feet->Struct != TBaseStructure<FVector>::Get()) { return false; }
		FStructOnScope Parameters(Function);
		*Center->ContainerPtrToValuePtr<FVector>(Parameters.GetStructMemory()) = Candidate;
		RequireNav->SetPropertyValue_InContainer(Parameters.GetStructMemory(), true);
		Ghost->ProcessEvent(Function, Parameters.GetStructMemory());
		if (!Safe->GetPropertyValue_InContainer(Parameters.GetStructMemory())) { return false; }
		FVector OriginalCenter; float Radius, HalfHeight;
		if (!UMallCartProtectionSubsystem::GetCapsule(Ghost, OriginalCenter, Radius, HalfHeight)) { return false; }
		Result = *Feet->ContainerPtrToValuePtr<FVector>(Parameters.GetStructMemory()) + FVector(0, 0, HalfHeight);
		UMallCartProtectionSubsystem* Rules = Protection(Ghost);
		return Rules && UMallGhostCartHelpers::IsGhostCenterAllowed(Ghost, Result);
	}
}

bool UMallGhostCartHelpers::IsGhostCenterAllowed(AActor* Ghost, FVector Center)
{
	if (!IsValid(Ghost) || !Ghost->HasAuthority() || Center.ContainsNaN()) { return false; }
	UMallCartProtectionSubsystem* Rules = MallGhostCarts::Protection(Ghost);
	const bool bCartGhost = MallGhostCarts::IsCartGhost(Ghost);
	const bool bDormant = bCartGhost && Rules && Rules->IsStoreSuppressedByCart(Ghost);
	if (bCartGhost && !bDormant) { return Rules && Rules->IsGhostPositionAllowed(Ghost, Center); }
	// Legacy instances retain the exact original radius + 5 cm horizontal margin.
	FVector Min, Max, CapsuleCenter; float Radius, HalfHeight; double FloorZ;
	if (!UMallCartProtectionSubsystem::GetStoreBounds(Ghost, Min, Max, FloorZ)
		|| !UMallCartProtectionSubsystem::GetCapsule(Ghost, CapsuleCenter, Radius, HalfHeight)) { return false; }
	const double Padding = Radius + 5.0;
	return Center.X >= Min.X + Padding && Center.X <= Max.X - Padding
		&& Center.Y >= Min.Y + Padding && Center.Y <= Max.Y - Padding
		&& (!bDormant || (Center.Z - HalfHeight >= FloorZ - 40.0 && Center.Z + HalfHeight <= Max.Z));
}

bool UMallGhostCartHelpers::IsGhostFeetAllowed(AActor* Ghost, FVector Feet)
{
	FVector Center; float Radius, HalfHeight;
	return !Feet.ContainsNaN() && UMallCartProtectionSubsystem::GetCapsule(Ghost, Center, Radius, HalfHeight)
		&& IsGhostCenterAllowed(Ghost, Feet + FVector(0, 0, HalfHeight));
}

bool UMallGhostCartHelpers::CanGhostPursuePlayer(AActor* Ghost, AActor* Player)
{
	if (!IsValid(Ghost) || !Ghost->HasAuthority() || !IsValid(Player) || Ghost->GetWorld() != Player->GetWorld()) { return false; }
	if (AMallHidingSpot::IsPlayerConcealedFromGhost(Player, Ghost)) { return false; }
	if (const APawn* PlayerPawn = Cast<APawn>(Player))
	{
		if (AMallSpaHealingZone::IsPawnInsideAnySanctuary(PlayerPawn))
		{
			return false;
		}
		if (const UMallPlayerVitalityComponent* Vit = PlayerPawn->FindComponentByClass<UMallPlayerVitalityComponent>())
		{
			const float DetectMult = Vit->GetGhostDetectionDistanceMultiplier();
			if (DetectMult < 0.99f && FVector::DistSquared(Ghost->GetActorLocation(), Player->GetActorLocation()) > FMath::Square(1100.0f * DetectMult))
			{
				return false;
			}
		}
	}
	UMallCartProtectionSubsystem* Rules = MallGhostCarts::Protection(Ghost);
	if (MallGhostCarts::IsCartGhost(Ghost)) { return Rules && Rules->CanGhostPursueTarget(Ghost, Player); }
	// Match the original Blueprint target-center/feet eligibility for untouched instances.
	FVector Min, Max, Center; float Radius, HalfHeight; double FloorZ;
	return UMallCartProtectionSubsystem::GetStoreBounds(Ghost, Min, Max, FloorZ)
		&& UMallCartProtectionSubsystem::GetCapsule(Player, Center, Radius, HalfHeight)
		&& Center.X >= Min.X && Center.X <= Max.X && Center.Y >= Min.Y && Center.Y <= Max.Y
		&& Center.Z - HalfHeight >= FloorZ - 40.0 && Center.Z - HalfHeight <= Max.Z;
}

ACharacter* UMallGhostCartHelpers::ResolveTargetPawn(AActor* Ghost)
{
	if (!MallGhostCarts::IsCartGhost(Ghost)) { return UMallGhostVoiceReceiver::ResolveTargetPawn(Ghost); }
	if (!UMallGhostInteractionComponent::CanGhostHunt(Ghost) || !Ghost->GetWorld()) { return nullptr; }
	// Keep a valid voice preference, then search all permitted players, including unsealed hallways.
	ACharacter* Preferred = UMallGhostVoiceReceiver::ResolveTargetPawn(Ghost);
	if (CanGhostPursuePlayer(Ghost, Preferred)) { return Preferred; }
	ACharacter* Nearest = nullptr;
	double BestDistance = TNumericLimits<double>::Max();
	for (FConstPlayerControllerIterator It = Ghost->GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* Controller = It->Get();
		ACharacter* Candidate = IsValid(Controller) ? Cast<ACharacter>(Controller->GetPawn()) : nullptr;
		if (!CanGhostPursuePlayer(Ghost, Candidate)) { continue; }
		const double Distance = FVector::DistSquared(Ghost->GetActorLocation(), Candidate->GetActorLocation());
		if (Distance < BestDistance) { BestDistance = Distance; Nearest = Candidate; }
	}
	return Nearest;
}

bool UMallGhostCartHelpers::CanGhostCatchPlayer(AActor* Ghost, AActor* Player)
{
	if (!UMallGhostInteractionComponent::CanGhostHunt(Ghost)) { return false; }
	if (!MallGhostCarts::IsCartGhost(Ghost)) { return true; }
	return CanGhostPursuePlayer(Ghost, Player) && IsGhostCenterAllowed(Ghost, Ghost->GetActorLocation());
}

bool UMallGhostCartHelpers::IsGhostRouteSegmentAllowed(AActor* Ghost, FVector StartFeet, FVector EndFeet)
{
	if (!IsValid(Ghost) || !Ghost->HasAuthority() || StartFeet.ContainsNaN() || EndFeet.ContainsNaN()) { return false; }
	if (!MallGhostCarts::IsCartGhost(Ghost)) { return true; }
	UMallCartProtectionSubsystem* Rules = MallGhostCarts::Protection(Ghost);
	FVector Center; float Radius, HalfHeight;
	return Rules && UMallCartProtectionSubsystem::GetCapsule(Ghost, Center, Radius, HalfHeight)
		&& Rules->IsGhostSegmentAllowed(Ghost, StartFeet + FVector(0, 0, HalfHeight), EndFeet + FVector(0, 0, HalfHeight));
}

bool UMallGhostCartHelpers::CanFollowGhostWaypoint(AActor* Ghost, FVector EndFeet)
{
	if (!IsValid(Ghost) || !Ghost->HasAuthority() || EndFeet.ContainsNaN()) { return false; }
	if (!MallGhostCarts::IsCartGhost(Ghost)) { return true; }
	UMallCartProtectionSubsystem* Rules = MallGhostCarts::Protection(Ghost);
	FVector Center; float Radius, HalfHeight;
	return Rules && UMallCartProtectionSubsystem::GetCapsule(Ghost, Center, Radius, HalfHeight)
		&& Rules->IsGhostSegmentAllowed(Ghost, Center, EndFeet + FVector(0, 0, HalfHeight));
}

void UMallGhostCartHelpers::GuardCartGhostMovement(AActor* Ghost)
{
	if (!MallGhostCarts::IsCartGhost(Ghost) || !Ghost->HasAuthority() || !Ghost->GetWorld()) { return; }
	UMallCartProtectionSubsystem* Rules = MallGhostCarts::Protection(Ghost);
	ACharacter* Character = Cast<ACharacter>(Ghost);
	if (!Rules || !Character) { return; }
	if (Rules->IsStoreSuppressedByCart(Ghost))
	{
		MallGhostCarts::StopRoute(Ghost);
		if (UMallGhostInteractionComponent* Interaction = Ghost->FindComponentByClass<UMallGhostInteractionComponent>())
		{
			if (!Interaction->IsInvisible()) { Interaction->SetGhostMode(EMallGhostMode::InvisibleDisturbance); }
		}
		if (!Rules->IsActorInsideHomeStore(Ghost, Ghost))
		{
			FVector Home, Recovery;
			if (MallGhostCarts::ReadVector(Ghost, TEXT("HomeLocation"), Home)
				&& MallGhostCarts::ValidateRecovery(Ghost, Home, Recovery))
			{
				Ghost->SetActorLocation(Recovery, false, nullptr, ETeleportType::TeleportPhysics);
				Ghost->ForceNetUpdate();
			}
		}
		return;
	}
	const FVector Position = Ghost->GetActorLocation();
	if (!Rules->IsGhostPositionAllowed(Ghost, Position))
	{
		MallGhostCarts::StopRoute(Ghost);
		FVector Candidate, Recovery;
		bool bFound = MallGhostCarts::ReadVector(Ghost, TEXT("LastSafeLocation"), Candidate)
			&& MallGhostCarts::ValidateRecovery(Ghost, Candidate, Recovery);
		if (!bFound)
		{
			bFound = MallGhostCarts::ReadVector(Ghost, TEXT("HomeLocation"), Candidate)
				&& MallGhostCarts::ValidateRecovery(Ghost, Candidate, Recovery);
		}
		// A player can move a powered cone over both the ghost and its old safe point.
		// Find a nearby physically valid, navigable point instead of leaving it inside the cone.
		for (float Distance : {100.f, 250.f, 450.f, 700.f, 950.f})
		{
			if (bFound) { break; }
			for (int32 Direction = 0; Direction < 12 && !bFound; ++Direction)
			{
				const double Angle = 2.0 * PI * Direction / 12.0;
				Candidate = Position + FVector(FMath::Cos(Angle) * Distance, FMath::Sin(Angle) * Distance, 0.0);
				bFound = MallGhostCarts::ValidateRecovery(Ghost, Candidate, Recovery);
			}
		}
		if (bFound) { Ghost->SetActorLocation(Recovery, false, nullptr, ETeleportType::TeleportPhysics); Ghost->ForceNetUpdate(); }
		return;
	}
	// Revalidate a short look-ahead before CharacterMovement advances a previously issued AI move.
	const double LeadSeconds = FMath::Clamp(Ghost->GetWorld()->GetDeltaSeconds() * 2.0, 0.05, 0.25);
	const FVector Proposed = Position + Ghost->GetVelocity() * LeadSeconds;
	if (!Rules->IsGhostSegmentAllowed(Ghost, Position, Proposed)) { MallGhostCarts::StopRoute(Ghost); }
}
