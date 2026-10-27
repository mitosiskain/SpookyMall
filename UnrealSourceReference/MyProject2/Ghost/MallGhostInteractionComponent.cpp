#include "Ghost/MallGhostInteractionComponent.h"
#include "Equipment/MallCartProtectionSubsystem.h"

#include "Ghost/MallGhostDisturbanceProp.h"
#include "Ghost/MallGhostSoundComponent.h"
#include "Ghost/MallGhostLightComponent.h"
#include "Ghost/MallGhostWritingBook.h"
#include "Voice/MallGhostVoiceReceiver.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Net/UnrealNetwork.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogMallGhostInteraction, Log, All);

namespace MallGhostInteraction
{
	bool IsSuppressedByCart(const AActor* Ghost)
	{
		if (!IsValid(Ghost) || !Ghost->HasAuthority() || !Ghost->GetWorld()
			|| !Ghost->ActorHasTag(TEXT("MallCartBoundary"))) { return false; }
		const UMallCartProtectionSubsystem* Protection = Ghost->GetWorld()->GetSubsystem<UMallCartProtectionSubsystem>();
		return Protection && Protection->IsStoreSuppressedByCart(const_cast<AActor*>(Ghost));
	}

	bool ReadNumber(const UObject* Object, const FName Name, double& Value)
	{
		const FNumericProperty* Property = Object ? FindFProperty<FNumericProperty>(Object->GetClass(), Name) : nullptr;
		if (!Property || !Property->IsFloatingPoint()) { return false; }
		Value = Property->GetFloatingPointPropertyValue(Property->ContainerPtrToValuePtr<void>(Object));
		return FMath::IsFinite(Value);
	}

	bool ReadStore(const AActor* Ghost, double& MinX, double& MaxX, double& MinY, double& MaxY, double& FloorZ)
	{
		return IsValid(Ghost) && Ghost->ActorHasTag(TEXT("StoreGhostPrototype"))
			&& ReadNumber(Ghost, TEXT("StoreMinX"), MinX) && ReadNumber(Ghost, TEXT("StoreMaxX"), MaxX)
			&& ReadNumber(Ghost, TEXT("StoreMinY"), MinY) && ReadNumber(Ghost, TEXT("StoreMaxY"), MaxY)
			&& ReadNumber(Ghost, TEXT("StoreFloorZ"), FloorZ) && MinX < MaxX && MinY < MaxY;
	}

	double SafeSeconds(const float Requested, const float Fallback)
	{
		return FMath::Clamp(FMath::IsFinite(Requested) ? Requested : Fallback, 0.25f, 600.0f);
	}

	double RandomSeconds(const float RequestedMin, const float RequestedMax, const float DefaultMin, const float DefaultMax)
	{
		const float Low = static_cast<float>(SafeSeconds(RequestedMin, DefaultMin));
		const float High = FMath::Max(Low, static_cast<float>(SafeSeconds(RequestedMax, DefaultMax)));
		return FMath::FRandRange(Low, High);
	}

	bool ReadCatchCount(const AActor* Ghost, int32& Count)
	{
		const FNumericProperty* Property = IsValid(Ghost)
			? FindFProperty<FNumericProperty>(Ghost->GetClass(), TEXT("CatchCount")) : nullptr;
		if (!Property || !Property->IsInteger()) { return false; }
		Count = static_cast<int32>(Property->GetSignedIntPropertyValue(Property->ContainerPtrToValuePtr<void>(Ghost)));
		return true;
	}

	bool IsLivingPlayer(const ACharacter* Character)
	{
		if (!IsValid(Character) || Character->IsActorBeingDestroyed()) { return false; }
		if (const FNumericProperty* Health = FindFProperty<FNumericProperty>(Character->GetClass(), TEXT("Health")))
		{
			const void* Value = Health->ContainerPtrToValuePtr<void>(Character);
			const double Amount = Health->IsFloatingPoint() ? Health->GetFloatingPointPropertyValue(Value)
				: static_cast<double>(Health->GetSignedIntPropertyValue(Value));
			if (!FMath::IsFinite(Amount) || Amount <= 0.0) { return false; }
		}
		const FBoolProperty* Dead = FindFProperty<FBoolProperty>(Character->GetClass(), TEXT("bIsDead"));
		return !Dead || !Dead->GetPropertyValue_InContainer(Character);
	}

	void ClearRoute(AActor* Ghost)
	{
		UFunction* Function = Ghost ? Ghost->FindFunction(TEXT("ClearRoute")) : nullptr;
		if (Function && Function->NumParms == 0) { Ghost->ProcessEvent(Function, nullptr); }
	}

	void ModeCommand(const TArray<FString>& Args, UWorld* World)
	{
		if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogMallGhostInteraction, Log, TEXT("Ghost mode command requires the authoritative game world."));
			return;
		}
		if (Args.Num() != 1 || (!Args[0].Equals(TEXT("invisible"), ESearchCase::IgnoreCase)
			&& !Args[0].Equals(TEXT("hunt"), ESearchCase::IgnoreCase)))
		{
			UE_LOG(LogMallGhostInteraction, Log, TEXT("Usage: mall.GhostMode invisible|hunt"));
			return;
		}
		for (TActorIterator<ACharacter> It(World); It; ++It)
		{
			if (!It->ActorHasTag(TEXT("StoreGhostPrototype"))) { continue; }
			if (UMallGhostInteractionComponent* Component = It->FindComponentByClass<UMallGhostInteractionComponent>())
			{
				Component->SetGhostMode(Args[0].Equals(TEXT("hunt"), ESearchCase::IgnoreCase)
					? EMallGhostMode::VisibleHunt : EMallGhostMode::InvisibleDisturbance);
			}
		}
	}

	void DisturbCommand(const TArray<FString>& Args, UWorld* World)
	{
		if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogMallGhostInteraction, Log, TEXT("Ghost disturbance command requires the authoritative game world."));
			return;
		}
		if (Args.Num() != 1 || (!Args[0].Equals(TEXT("write"), ESearchCase::IgnoreCase)
			&& !Args[0].Equals(TEXT("move"), ESearchCase::IgnoreCase) && !Args[0].Equals(TEXT("throw"), ESearchCase::IgnoreCase)))
		{
			UE_LOG(LogMallGhostInteraction, Log, TEXT("Usage: mall.GhostDisturb write|move|throw"));
			return;
		}
		for (TActorIterator<ACharacter> It(World); It; ++It)
		{
			if (!It->ActorHasTag(TEXT("StoreGhostPrototype"))) { continue; }
			if (UMallGhostInteractionComponent* Component = It->FindComponentByClass<UMallGhostInteractionComponent>())
			{
				const bool bPerformed = Args[0].Equals(TEXT("write"), ESearchCase::IgnoreCase) ? Component->DisturbNearestBook()
					: Args[0].Equals(TEXT("throw"), ESearchCase::IgnoreCase) ? Component->ThrowNearestProp() : Component->MoveNearestProp();
				UE_LOG(LogMallGhostInteraction, Log, TEXT("%s disturbance %s: %s"), *It->GetName(), *Args[0], bPerformed ? TEXT("accepted") : TEXT("unavailable"));
			}
		}
	}

	FAutoConsoleCommandWithWorldAndArgs ModeConsole(TEXT("mall.GhostMode"),
		TEXT("Prototype: set authoritative store ghosts to invisible or hunt."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&ModeCommand));
	FAutoConsoleCommandWithWorldAndArgs DisturbConsole(TEXT("mall.GhostDisturb"),
		TEXT("Prototype: ask invisible store ghosts to write, move, or throw an opt-in prop."), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&DisturbCommand));
}

UMallGhostInteractionComponent::UMallGhostInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.25f;
	SetIsReplicatedByDefault(true);
}

void UMallGhostInteractionComponent::BeginPlay()
{
	Super::BeginPlay();
	RefreshCartCollisionPolicy();
	ApplyVisualVisibility();
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		if (GhostMode != EMallGhostMode::VisibleHunt) { StopHunt(); }
		MallGhostInteraction::ReadCatchCount(GetOwner(), LastObservedCatchCount);
		NextDisturbanceTime = GetWorld()->GetTimeSeconds() + FMath::Max(1.0f, DisturbanceIntervalSeconds);
	}
}

void UMallGhostInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bIgnoresCartPhysics = false;
	ApplyCartCollisionPolicy();
	Super::EndPlay(EndPlayReason);
}

void UMallGhostInteractionComponent::OnRep_CartCollisionPolicy()
{
	ApplyCartCollisionPolicy();
}

void UMallGhostInteractionComponent::RefreshCartCollisionPolicy()
{
	AActor* Ghost = GetOwner();
	if (IsValid(Ghost) && Ghost->HasAuthority())
	{
		// Runtime actor tags do not replicate; publish the authoritative opt-in explicitly.
		const bool bWanted = Ghost->ActorHasTag(TEXT("MallCartBoundary"));
		if (bIgnoresCartPhysics != bWanted)
		{
			bIgnoresCartPhysics = bWanted;
			Ghost->ForceNetUpdate();
		}
	}
	ApplyCartCollisionPolicy();
}

void UMallGhostInteractionComponent::ApplyCartCollisionPolicy()
{
	if (!bIgnoresCartPhysics)
	{
		for (const auto& Saved : OriginalCartCollisionResponses)
		{
			if (UPrimitiveComponent* Body = Saved.Key.Get())
			{
				Body->SetCollisionResponseToChannel(ECC_GameTraceChannel1, static_cast<ECollisionResponse>(Saved.Value));
			}
		}
		OriginalCartCollisionResponses.Reset();
		return;
	}
	ACharacter* Ghost = Cast<ACharacter>(GetOwner());
	if (!IsValid(Ghost)) { return; }
	auto ExcludeCart = [this](UPrimitiveComponent* Body)
	{
		if (!IsValid(Body)) { return; }
		if (!OriginalCartCollisionResponses.Contains(Body))
		{
			OriginalCartCollisionResponses.Add(Body, static_cast<uint8>(Body->GetCollisionResponseToChannel(ECC_GameTraceChannel1)));
		}
		if (Body->GetCollisionResponseToChannel(ECC_GameTraceChannel1) != ECR_Ignore)
		{
			Body->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Ignore);
		}
	};
	ExcludeCart(Ghost->GetCapsuleComponent());
	if (USkeletalMeshComponent* Mesh = Ghost->GetMesh(); IsValid(Mesh) && Mesh->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
	{
		ExcludeCart(Mesh);
	}
}

void UMallGhostInteractionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UMallGhostInteractionComponent, GhostMode);
	DOREPLIFETIME(UMallGhostInteractionComponent, bIgnoresCartPhysics);
	DOREPLIFETIME(UMallGhostInteractionComponent, CyclePhase);
	DOREPLIFETIME(UMallGhostInteractionComponent, PhaseEndsServerTime);
	DOREPLIFETIME(UMallGhostInteractionComponent, HuntCount);
	DOREPLIFETIME(UMallGhostInteractionComponent, DisturbanceCount);
	DOREPLIFETIME(UMallGhostInteractionComponent, LastDisturbance);
	DOREPLIFETIME(UMallGhostInteractionComponent, LastDisturbanceServerTime);
}

bool UMallGhostInteractionComponent::CanGhostHunt(AActor* Ghost)
{
	const UMallGhostInteractionComponent* Component = IsValid(Ghost) ? Ghost->FindComponentByClass<UMallGhostInteractionComponent>() : nullptr;
	return Ghost && Ghost->HasAuthority() && Component && Component->GetGhostMode() == EMallGhostMode::VisibleHunt
		&& !MallGhostInteraction::IsSuppressedByCart(Ghost);
}

bool UMallGhostInteractionComponent::IsActorInGhostStore(const AActor* Ghost, const AActor* Other)
{
	if (!IsValid(Other) || !IsValid(Ghost) || Ghost->GetWorld() != Other->GetWorld()) { return false; }
	double MinX, MaxX, MinY, MaxY, FloorZ;
	if (!MallGhostInteraction::ReadStore(Ghost, MinX, MaxX, MinY, MaxY, FloorZ)) { return false; }
	FVector Origin, Extent;
	if (const ACharacter* Character = Cast<ACharacter>(Other))
	{
		const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
		if (!IsValid(Capsule)) { return false; }
		// Cameras and query-only presentation volumes are not the character's body.
		Origin = Capsule->Bounds.Origin;
		Extent = Capsule->Bounds.BoxExtent;
	}
	else
	{
		Other->GetActorBounds(false, Origin, Extent, true);
	}
	const FVector Low = Origin - Extent, High = Origin + Extent;
	const double MaxZOffset = Ghost->ActorHasTag(TEXT("MultiFloorAnchorGhost")) ? 1350.0 : 450.0;
	return FMath::IsFinite(Low.X) && FMath::IsFinite(Low.Y) && FMath::IsFinite(Low.Z)
		&& FMath::IsFinite(High.X) && FMath::IsFinite(High.Y) && FMath::IsFinite(High.Z)
		&& Low.X >= MinX && High.X <= MaxX && Low.Y >= MinY && High.Y <= MaxY
		&& Low.Z >= FloorZ - 40.0 && High.Z <= FloorZ + MaxZOffset;
}

bool UMallGhostInteractionComponent::SetGhostMode(const EMallGhostMode NewMode)
{
	AActor* Ghost = GetOwner();
	if (!IsValid(Ghost) || !Ghost->HasAuthority() || !GetWorld()
		|| (NewMode != EMallGhostMode::InvisibleDisturbance && NewMode != EMallGhostMode::VisibleHunt
			&& NewMode != EMallGhostMode::VisibleWarning)) { return false; }
	if (MallGhostInteraction::IsSuppressedByCart(Ghost))
	{
		TransitionCycle(EMallGhostCyclePhase::Activity, 0.0);
		return NewMode == EMallGhostMode::InvisibleDisturbance;
	}
	if (bAutomaticHunts)
	{
		// Host development commands remain useful, but automatic mode still bounds a manual hunt.
		const EMallGhostCyclePhase NewPhase = NewMode == EMallGhostMode::VisibleHunt ? EMallGhostCyclePhase::Hunting
			: NewMode == EMallGhostMode::VisibleWarning ? EMallGhostCyclePhase::Warning : EMallGhostCyclePhase::Cooldown;
		const double Duration = NewPhase == EMallGhostCyclePhase::Hunting ? MallGhostInteraction::SafeSeconds(HuntDurationSeconds, 20.0f)
			: NewPhase == EMallGhostCyclePhase::Warning ? MallGhostInteraction::SafeSeconds(WarningDurationSeconds, 3.0f)
			: MallGhostInteraction::RandomSeconds(CooldownMinSeconds, CooldownMaxSeconds, 40.0f, 60.0f);
		TransitionCycle(NewPhase, Duration);
	}
	else
	{
		// Disabling the scheduler preserves the original explicit-mode testing behavior.
		bCycleOwnsMode = false;
		PhaseEndsServerTime = 0.0;
		CyclePhase = NewMode == EMallGhostMode::VisibleHunt ? EMallGhostCyclePhase::Hunting
			: NewMode == EMallGhostMode::VisibleWarning ? EMallGhostCyclePhase::Warning : EMallGhostCyclePhase::Activity;
		if (NewMode == EMallGhostMode::VisibleHunt && GhostMode != NewMode) { ++HuntCount; }
		ApplyGhostMode(NewMode);
	}
	return true;
}

void UMallGhostInteractionComponent::ApplyGhostMode(const EMallGhostMode NewMode)
{
	if (!IsValid(GetOwner()) || !GetOwner()->HasAuthority() || !GetWorld()) { return; }
	GhostMode = NewMode;
	if (NewMode != EMallGhostMode::VisibleHunt)
	{
		StopHunt();
		if (IsInvisible())
		{
			NextDisturbanceTime = GetWorld()->GetTimeSeconds() + FMath::Max(1.0f, DisturbanceIntervalSeconds);
		}
	}
	ApplyVisualVisibility();
	GetOwner()->ForceNetUpdate();
	const TCHAR* Description = IsInvisible() ? TEXT("invisible disturbance")
		: NewMode == EMallGhostMode::VisibleWarning ? TEXT("visible harmless warning") : TEXT("visible hunt");
	UE_LOG(LogMallGhostInteraction, Log, TEXT("%s mode: %s"), *GetOwner()->GetName(), Description);
}

float UMallGhostInteractionComponent::GetPhaseTimeRemaining() const
{
	if (!GetWorld() || !FMath::IsFinite(PhaseEndsServerTime) || PhaseEndsServerTime <= 0.0) { return 0.0f; }
	const AGameStateBase* State = GetWorld()->GetGameState();
	const double Now = State ? State->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
	return static_cast<float>(FMath::Max(0.0, PhaseEndsServerTime - Now));
}

void UMallGhostInteractionComponent::TransitionCycle(const EMallGhostCyclePhase NewPhase, const double DurationSeconds)
{
	if (!IsValid(GetOwner()) || !GetOwner()->HasAuthority() || !GetWorld()) { return; }
	const bool bEnteringHunt = NewPhase == EMallGhostCyclePhase::Hunting && CyclePhase != NewPhase;
	CyclePhase = NewPhase;
	PhaseEndsServerTime = FMath::IsFinite(DurationSeconds) && DurationSeconds > 0.0
		? GetWorld()->GetTimeSeconds() + DurationSeconds : 0.0;
	bCycleOwnsMode = bAutomaticHunts && (NewPhase == EMallGhostCyclePhase::Warning || NewPhase == EMallGhostCyclePhase::Hunting);
	if (NewPhase == EMallGhostCyclePhase::Hunting)
	{
		MallGhostInteraction::ReadCatchCount(GetOwner(), LastObservedCatchCount);
		if (bEnteringHunt) { ++HuntCount; }
	}
	ApplyGhostMode(NewPhase == EMallGhostCyclePhase::Hunting ? EMallGhostMode::VisibleHunt
		: NewPhase == EMallGhostCyclePhase::Warning ? EMallGhostMode::VisibleWarning : EMallGhostMode::InvisibleDisturbance);
}

bool UMallGhostInteractionComponent::HasEligibleHuntPlayerInStore() const
{
	const AActor* Ghost = GetOwner();
	int32 IgnoredCount;
	if (!IsValid(Ghost) || Ghost->IsActorBeingDestroyed() || !GetWorld()
		|| !MallGhostInteraction::ReadCatchCount(Ghost, IgnoredCount)) { return false; }
	const bool bCartBoundary = Ghost->ActorHasTag(TEXT("MallCartBoundary"));
	const UMallCartProtectionSubsystem* Protection = bCartBoundary ? GetWorld()->GetSubsystem<UMallCartProtectionSubsystem>() : nullptr;
	if (!bCartBoundary && !IsActorInGhostStore(Ghost, Ghost)) { return false; }
	if (bCartBoundary && !Protection) { return false; }
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* Controller = It->Get();
		const ACharacter* Character = IsValid(Controller) ? Cast<ACharacter>(Controller->GetPawn()) : nullptr;
		if (IsValid(Controller) && !Controller->IsActorBeingDestroyed() && Controller->HasAuthority()
			&& Controller->GetWorld() == GetWorld() && MallGhostInteraction::IsLivingPlayer(Character)
			&& Character->HasAuthority() && Character->GetController() == Controller
			&& (bCartBoundary ? Protection->CanGhostMaintainHuntForPlayer(const_cast<AActor*>(Ghost), const_cast<ACharacter*>(Character),
				CyclePhase != EMallGhostCyclePhase::Hunting) : IsActorInGhostStore(Ghost, Character))) { return true; }
	}
	return false;
}

void UMallGhostInteractionComponent::UpdateHuntCycle()
{
	if (!IsValid(GetOwner()) || !GetOwner()->HasAuthority() || !GetWorld()) { return; }
	const double Now = GetWorld()->GetTimeSeconds();
	if (MallGhostInteraction::IsSuppressedByCart(GetOwner()))
	{
		if (GhostMode != EMallGhostMode::InvisibleDisturbance || CyclePhase != EMallGhostCyclePhase::Activity
			|| PhaseEndsServerTime != 0.0 || bCycleOwnsMode)
		{
			TransitionCycle(EMallGhostCyclePhase::Activity, 0.0);
		}
		return;
	}
	if (!bAutomaticHunts)
	{
		if (bCycleOwnsMode || (GhostMode == EMallGhostMode::InvisibleDisturbance
			&& (CyclePhase != EMallGhostCyclePhase::Activity || PhaseEndsServerTime != 0.0)))
		{
			// Stop a scheduler-owned warning/hunt, without overriding a new explicit manual hunt.
			TransitionCycle(EMallGhostCyclePhase::Activity, 0.0);
		}
		return;
	}
	const bool bEligible = HasEligibleHuntPlayerInStore();
	double ExistingCooldown = 0.0;
	const bool bCooldownValid = MallGhostInteraction::ReadNumber(GetOwner(), TEXT("CooldownUntil"), ExistingCooldown);
	const bool bMayStart = bCooldownValid && Now >= ExistingCooldown && Now >= DisturbancePausedUntil;
	if (CyclePhase == EMallGhostCyclePhase::Hunting)
	{
		if (!bCycleOwnsMode || PhaseEndsServerTime <= 0.0 || !FMath::IsFinite(PhaseEndsServerTime))
		{
			// Turning automatic scheduling on also bounds an already active manual hunt.
			bCycleOwnsMode = true;
			PhaseEndsServerTime = Now + MallGhostInteraction::SafeSeconds(HuntDurationSeconds, 20.0f);
			MallGhostInteraction::ReadCatchCount(GetOwner(), LastObservedCatchCount);
			GetOwner()->ForceNetUpdate();
		}
		int32 CatchCount = LastObservedCatchCount;
		const bool bCaught = MallGhostInteraction::ReadCatchCount(GetOwner(), CatchCount) && CatchCount > LastObservedCatchCount;
		if (!bEligible || bCaught || Now >= PhaseEndsServerTime)
		{
			TransitionCycle(EMallGhostCyclePhase::Cooldown,
				MallGhostInteraction::RandomSeconds(CooldownMinSeconds, CooldownMaxSeconds, 40.0f, 60.0f));
		}
		return;
	}
	if (CyclePhase == EMallGhostCyclePhase::Warning)
	{
		if (!bEligible || !bCooldownValid)
		{
			TransitionCycle(EMallGhostCyclePhase::Activity, 0.0);
			return;
		}
		if (!bCycleOwnsMode || PhaseEndsServerTime <= 0.0 || !FMath::IsFinite(PhaseEndsServerTime))
		{
			bCycleOwnsMode = true;
			PhaseEndsServerTime = Now + MallGhostInteraction::SafeSeconds(WarningDurationSeconds, 3.0f);
			GetOwner()->ForceNetUpdate();
		}
		if (!bMayStart)
		{
			// Give a complete warning after a voice-requested pause, rather than hunting as it expires.
			const double ResumeAt = FMath::Max(ExistingCooldown, DisturbancePausedUntil);
			const double NewDeadline = ResumeAt + MallGhostInteraction::SafeSeconds(WarningDurationSeconds, 3.0f);
			if (NewDeadline > PhaseEndsServerTime) { PhaseEndsServerTime = NewDeadline; GetOwner()->ForceNetUpdate(); }
			return;
		}
		if (Now >= PhaseEndsServerTime)
		{
			TransitionCycle(EMallGhostCyclePhase::Hunting, MallGhostInteraction::SafeSeconds(HuntDurationSeconds, 20.0f));
		}
		return;
	}
	if (CyclePhase == EMallGhostCyclePhase::Cooldown)
	{
		if (Now < PhaseEndsServerTime) { return; }
		if (!bEligible)
		{
			TransitionCycle(EMallGhostCyclePhase::Activity, 0.0);
		}
		else if (bMayStart)
		{
			TransitionCycle(EMallGhostCyclePhase::Warning, MallGhostInteraction::SafeSeconds(WarningDurationSeconds, 3.0f));
		}
		return;
	}
	if (!bEligible)
	{
		if (PhaseEndsServerTime != 0.0) { PhaseEndsServerTime = 0.0; GetOwner()->ForceNetUpdate(); }
		return;
	}
	if (PhaseEndsServerTime <= 0.0 || !FMath::IsFinite(PhaseEndsServerTime))
	{
		// The first grace period starts on entry, not when the empty map begins running.
		PhaseEndsServerTime = Now + MallGhostInteraction::RandomSeconds(
			InitialActivityMinSeconds, InitialActivityMaxSeconds, 30.0f, 45.0f);
		GetOwner()->ForceNetUpdate();
	}
	else if (Now >= PhaseEndsServerTime && bMayStart)
	{
		TransitionCycle(EMallGhostCyclePhase::Warning, MallGhostInteraction::SafeSeconds(WarningDurationSeconds, 3.0f));
	}
}

void UMallGhostInteractionComponent::OnRep_GhostMode()
{
	ApplyVisualVisibility();
}

void UMallGhostInteractionComponent::ApplyVisualVisibility()
{
	AActor* Ghost = GetOwner();
	if (!Ghost) { return; }
	TInlineComponentArray<UPrimitiveComponent*> Visuals(Ghost);
	for (UPrimitiveComponent* Visual : Visuals)
	{
		if (!IsValid(Visual)) { continue; }
		// Native voice signals opt out explicitly; duplicated PIE component flags
		// are not a reliable way to distinguish authored labels from temporary cues.
		if (Visual->ComponentHasTag(TEXT("MallGhostSignal"))) { continue; }
		const TWeakObjectPtr<UPrimitiveComponent> Key(Visual);
		if (!OriginalVisualVisibility.Contains(Key))
		{
			OriginalVisualVisibility.Add(Key, {Visual->bHiddenInGame != 0, Visual->bCastHiddenShadow != 0});
		}
		const FOriginalVisualVisibility& Original = OriginalVisualVisibility.FindChecked(Key);
		// Do not propagate to unrelated attached components: voice signal text/light remain usable.
		Visual->SetHiddenInGame(IsInvisible() || Original.bHiddenInGame, false);
		Visual->SetCastHiddenShadow(IsInvisible() ? false : Original.bCastHiddenShadow);
	}
}

void UMallGhostInteractionComponent::StopHunt()
{
	AActor* Ghost = GetOwner();
	if (!Ghost || !Ghost->HasAuthority()) { return; }
	MallGhostInteraction::ClearRoute(Ghost);
	if (UMallGhostVoiceReceiver* Voice = Ghost->FindComponentByClass<UMallGhostVoiceReceiver>())
	{
		Voice->ResetHuntingPreference();
	}
	if (ACharacter* Character = Cast<ACharacter>(Ghost))
	{
		Character->GetCharacterMovement()->StopMovementImmediately();
	}
	if (FObjectPropertyBase* Target = FindFProperty<FObjectPropertyBase>(Ghost->GetClass(), TEXT("TargetPlayer")))
	{
		Target->SetObjectPropertyValue_InContainer(Ghost, nullptr);
	}
	if (FStrProperty* State = FindFProperty<FStrProperty>(Ghost->GetClass(), TEXT("PrototypeState")))
	{
		State->SetPropertyValue_InContainer(Ghost, GhostMode == EMallGhostMode::VisibleWarning ? TEXT("Warning") : TEXT("Disturbing"));
	}
	if (FNumericProperty* PlanTime = FindFProperty<FNumericProperty>(Ghost->GetClass(), TEXT("NextPlanTime")))
	{
		if (PlanTime->IsFloatingPoint()) { PlanTime->SetFloatingPointPropertyValue(PlanTime->ContainerPtrToValuePtr<void>(Ghost), 0.0); }
	}
}

bool UMallGhostInteractionComponent::CanPerformDisturbance() const
{
	const AActor* Ghost = GetOwner();
	if (!Ghost || !Ghost->HasAuthority() || !GetWorld() || !IsInvisible()
		|| MallGhostInteraction::IsSuppressedByCart(Ghost)) { return false; }
	double MinX, MaxX, MinY, MaxY, FloorZ;
	if (!MallGhostInteraction::ReadStore(Ghost, MinX, MaxX, MinY, MaxY, FloorZ)) { return false; }
	const double Now = GetWorld()->GetTimeSeconds();
	double ExistingCooldown = 0.0;
	if (!MallGhostInteraction::ReadNumber(Ghost, TEXT("CooldownUntil"), ExistingCooldown)) { return false; }
	return Now >= DisturbancePausedUntil && Now >= ExistingCooldown;
}

bool UMallGhostInteractionComponent::HasEligiblePlayerInStore() const
{
	if (!GetWorld() || !GetOwner()) { return false; }
	double MinX, MaxX, MinY, MaxY, FloorZ;
	if (!MallGhostInteraction::ReadStore(GetOwner(), MinX, MaxX, MinY, MaxY, FloorZ)) { return false; }
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* Controller = It->Get();
		ACharacter* Pawn = Controller ? Cast<ACharacter>(Controller->GetPawn()) : nullptr;
		if (!IsValid(Pawn) || Pawn->GetController() != Controller || !Pawn->GetCapsuleComponent()) { continue; }
		const FVector Position = Pawn->GetActorLocation();
		const double FeetZ = Position.Z - Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		if (Position.X >= MinX && Position.X <= MaxX && Position.Y >= MinY && Position.Y <= MaxY
			&& FeetZ >= FloorZ - 40.0 && FeetZ <= FloorZ + 450.0) { return true; }
	}
	return false;
}

void UMallGhostInteractionComponent::RecordDisturbance(const FName Kind)
{
	++DisturbanceCount;
	LastDisturbance = Kind;
	LastDisturbanceServerTime = GetWorld()->GetTimeSeconds();
	GetOwner()->ForceNetUpdate();
}

bool UMallGhostInteractionComponent::DisturbNearestBook()
{
	if (!CanPerformDisturbance()) { return false; }
	AMallGhostWritingBook* Nearest = nullptr;
	double BestDistanceSquared = FMath::Square(FMath::Clamp(InteractionRangeCm, 50.0f, 2000.0f));
	for (TActorIterator<AMallGhostWritingBook> It(GetWorld()); It; ++It)
	{
		if (It->IsWritten() || !It->IsPlacedForWriting() || !IsActorInGhostStore(GetOwner(), *It)) { continue; }
		const double DistanceSquared = FVector::DistSquared(GetOwner()->GetActorLocation(), It->GetActorLocation());
		if (DistanceSquared <= BestDistanceSquared) { BestDistanceSquared = DistanceSquared; Nearest = *It; }
	}
	if (!Nearest || !Nearest->TryWrite(GetOwner())) { return false; }
	RecordDisturbance(TEXT("write"));
	return true;
}

bool UMallGhostInteractionComponent::DisturbNearestProp(const bool bThrow)
{
	if (!CanPerformDisturbance()) { return false; }
	AMallGhostDisturbanceProp* Nearest = nullptr;
	double BestDistanceSquared = FMath::Square(FMath::Clamp(InteractionRangeCm, 50.0f, 2000.0f));
	for (TActorIterator<AMallGhostDisturbanceProp> It(GetWorld()); It; ++It)
	{
		if (!IsActorInGhostStore(GetOwner(), *It) || !It->CanBeDisturbedBy(GetOwner())) { continue; }
		const double DistanceSquared = FVector::DistSquared(GetOwner()->GetActorLocation(), It->GetActorLocation());
		if (DistanceSquared <= BestDistanceSquared) { BestDistanceSquared = DistanceSquared; Nearest = *It; }
	}
	if (!Nearest || !Nearest->Disturb(GetOwner(), bThrow)) { return false; }
	RecordDisturbance(bThrow ? TEXT("throw") : TEXT("move"));
	return true;
}

bool UMallGhostInteractionComponent::MoveNearestProp() { return DisturbNearestProp(false); }
bool UMallGhostInteractionComponent::ThrowNearestProp() { return DisturbNearestProp(true); }

void UMallGhostInteractionComponent::PauseDisturbances(const float Seconds)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !GetWorld() || !FMath::IsFinite(Seconds)) { return; }
	DisturbancePausedUntil = FMath::Max(DisturbancePausedUntil, GetWorld()->GetTimeSeconds() + FMath::Clamp(Seconds, 0.0f, 30.0f));
	NextDisturbanceTime = FMath::Max(NextDisturbanceTime, DisturbancePausedUntil);
}

void UMallGhostInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	RefreshCartCollisionPolicy();
	if (!GetOwner() || !GetOwner()->HasAuthority() || !GetWorld()) { return; }
	UpdateHuntCycle();
	if (!bAutomaticDisturbances
		|| GetWorld()->GetTimeSeconds() < NextDisturbanceTime || !CanPerformDisturbance() || !HasEligiblePlayerInStore()) { return; }
	NextDisturbanceTime = GetWorld()->GetTimeSeconds() + FMath::Clamp(DisturbanceIntervalSeconds, 1.0f, 120.0f);
	// Evidence stays blank until an explicit voice/question request (or host test command).
	const uint8 Action = AutomaticActionIndex++ % 2;
	if (Action == 1) { ThrowNearestProp(); }
	else { MoveNearestProp(); }
}

bool UMallGhostInteractionSubsystem::DoesSupportWorldType(const EWorldType::Type Type) const
{
	return Type == EWorldType::Game || Type == EWorldType::PIE;
}

void UMallGhostInteractionSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	AttachComponents();
}

TStatId UMallGhostInteractionSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UMallGhostInteractionSubsystem, STATGROUP_Tickables);
}

void UMallGhostInteractionSubsystem::AttachComponents()
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client) { return; }
	for (TActorIterator<ACharacter> It(World); It; ++It)
	{
		if (!It->ActorHasTag(TEXT("StoreGhostPrototype"))) { continue; }
		bool bAddedComponent = false;
		if (!It->FindComponentByClass<UMallGhostInteractionComponent>())
		{
			UMallGhostInteractionComponent* Component = NewObject<UMallGhostInteractionComponent>(*It, TEXT("MallGhostInteraction"));
			It->AddInstanceComponent(Component);
			Component->RegisterComponent();
			bAddedComponent = true;
		}
		if (!It->FindComponentByClass<UMallGhostSoundComponent>())
		{
			UMallGhostSoundComponent* Sound = NewObject<UMallGhostSoundComponent>(*It, TEXT("MallGhostSounds"));
			It->AddInstanceComponent(Sound);
			Sound->RegisterComponent();
			bAddedComponent = true;
		}
		if (!It->FindComponentByClass<UMallGhostLightComponent>())
		{
			UMallGhostLightComponent* Lights = NewObject<UMallGhostLightComponent>(*It, TEXT("MallGhostLights"));
			It->AddInstanceComponent(Lights);
			Lights->RegisterComponent();
			bAddedComponent = true;
		}
		if (bAddedComponent) { It->ForceNetUpdate(); }
	}
}

void UMallGhostInteractionSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!GetWorld() || !GetWorld()->HasBegunPlay() || GetWorld()->GetTimeSeconds() < NextRefreshTime) { return; }
	AttachComponents();
	NextRefreshTime = GetWorld()->GetTimeSeconds() + 0.5;
}
