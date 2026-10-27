#include "Ghost/MallGhostLightComponent.h"
#include "Ghost/MallGhostCartHelpers.h"
#include "Equipment/MallCartProtectionSubsystem.h"

#include "Ghost/MallGhostInteractionComponent.h"
#include "Components/LocalLightComponent.h"
#include "Engine/Light.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "UObject/UnrealType.h"

namespace MallGhostLights
{
	float SafeMinimum(const float Requested, const float Fallback)
	{
		return FMath::Clamp(FMath::IsFinite(Requested) ? Requested : Fallback, 0.0f, 1.0f);
	}

	float SafeRate(const float Requested)
	{
		return FMath::Clamp(FMath::IsFinite(Requested) ? Requested : 2.3f, 0.25f, 3.0f);
	}

	float KnotValue(const int64 Knot, const int32 Seed)
	{
		uint32 Hash = static_cast<uint32>(Knot) ^ static_cast<uint32>(Seed);
		Hash ^= Hash >> 16;
		Hash *= 0x7feb352du;
		Hash ^= Hash >> 15;
		Hash *= 0x846ca68bu;
		Hash ^= Hash >> 16;
		return static_cast<float>(Hash & 0x00ffffffu) / 16777215.0f;
	}

	bool IsLivingPlayer(const APawn* Pawn)
	{
		if (!IsValid(Pawn) || Pawn->IsActorBeingDestroyed()) { return false; }
		if (const FNumericProperty* Health = FindFProperty<FNumericProperty>(Pawn->GetClass(), TEXT("Health")))
		{
			const void* Value = Health->ContainerPtrToValuePtr<void>(Pawn);
			const double Amount = Health->IsFloatingPoint() ? Health->GetFloatingPointPropertyValue(Value)
				: static_cast<double>(Health->GetSignedIntPropertyValue(Value));
			if (!FMath::IsFinite(Amount) || Amount <= 0.0) { return false; }
		}
		const FBoolProperty* Dead = FindFProperty<FBoolProperty>(Pawn->GetClass(), TEXT("bIsDead"));
		return !Dead || !Dead->GetPropertyValue_InContainer(Pawn);
	}
}

UMallGhostLightComponent::UMallGhostLightComponent()
{
	SetIsReplicatedByDefault(true);
	bAutoActivate = true;
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.05f;
}

void UMallGhostLightComponent::BeginPlay()
{
	Super::BeginPlay();
	UpdateAuthoritativeState();
	ApplyFlicker();
}

void UMallGhostLightComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UMallGhostLightComponent, FlickerState);
}

EMallGhostLightMode UMallGhostLightComponent::DetermineMode() const
{
	const AActor* Ghost = GetOwner();
	if (!bEnableFlicker || !IsActive() || !IsRegistered() || !IsValid(Ghost)
		|| Ghost->IsActorBeingDestroyed() || !Ghost->HasAuthority() || !GetWorld()) { return EMallGhostLightMode::Off; }
	const UMallGhostInteractionComponent* Interaction = Ghost->FindComponentByClass<UMallGhostInteractionComponent>();
	if (!Interaction) { return EMallGhostLightMode::Off; }
	const bool bCartGhost = Ghost->ActorHasTag(TEXT("MallCartBoundary"));
	const UMallCartProtectionSubsystem* Protection = bCartGhost ? GetWorld()->GetSubsystem<UMallCartProtectionSubsystem>() : nullptr;
	if (bCartGhost && (!Protection || Protection->IsStoreSuppressedByCart(const_cast<AActor*>(Ghost)))) { return EMallGhostLightMode::Off; }
	if (Interaction->GetGhostMode() == EMallGhostMode::VisibleWarning) { return EMallGhostLightMode::Warning; }
	if (Interaction->GetGhostMode() != EMallGhostMode::VisibleHunt
		|| (!bCartGhost && !UMallGhostInteractionComponent::IsActorInGhostStore(Ghost, Ghost))) { return EMallGhostLightMode::Off; }

	const FStrProperty* State = FindFProperty<FStrProperty>(Ghost->GetClass(), TEXT("PrototypeState"));
	const FObjectPropertyBase* TargetProperty = FindFProperty<FObjectPropertyBase>(Ghost->GetClass(), TEXT("TargetPlayer"));
	const APawn* Target = TargetProperty ? Cast<APawn>(TargetProperty->GetObjectPropertyValue_InContainer(Ghost)) : nullptr;
	const APlayerController* Controller = IsValid(Target) ? Cast<APlayerController>(Target->GetController()) : nullptr;
	const FNumericProperty* Cooldown = FindFProperty<FNumericProperty>(Ghost->GetClass(), TEXT("CooldownUntil"));
	if (!State || State->GetPropertyValue_InContainer(Ghost) != TEXT("Chasing")
		|| !MallGhostLights::IsLivingPlayer(Target) || !IsValid(Controller) || Controller->IsActorBeingDestroyed()
		|| !Controller->HasAuthority() || !Target->HasAuthority() || Controller->GetPawn() != Target
		|| Controller->GetWorld() != GetWorld() || Target->GetWorld() != GetWorld()
		|| (bCartGhost ? !UMallGhostCartHelpers::CanGhostPursuePlayer(const_cast<AActor*>(Ghost), const_cast<APawn*>(Target))
			: !UMallGhostInteractionComponent::IsActorInGhostStore(Ghost, Target))
		|| !Cooldown || !Cooldown->IsFloatingPoint()) { return EMallGhostLightMode::Off; }
	const double Until = Cooldown->GetFloatingPointPropertyValue(Cooldown->ContainerPtrToValuePtr<void>(Ghost));
	return FMath::IsFinite(Until) && GetWorld()->GetTimeSeconds() >= Until
		? EMallGhostLightMode::Chasing : EMallGhostLightMode::Off;
}

bool UMallGhostLightComponent::IsEligibleLight(const ALight* Light) const
{
	const AActor* Ghost = GetOwner();
	if (TargetLightTag.IsNone() || !IsValid(Light) || Light->IsActorBeingDestroyed() || Light == Ghost
		|| Light->GetWorld() != GetWorld() || !Light->ActorHasTag(TargetLightTag)
		|| Light->ActorHasTag(TEXT("MallGhostSignal")) || Light->ActorHasTag(TEXT("StoreGhostPrototype"))
		|| (Ghost && Light->IsAttachedTo(Ghost))) { return false; }
	for (const AActor* Owner = Light->GetOwner(); IsValid(Owner); Owner = Owner->GetOwner())
	{
		if (Owner == Ghost || Owner->ActorHasTag(TEXT("MallGhostSignal"))) { return false; }
	}
	const ULocalLightComponent* Local = Cast<ULocalLightComponent>(Light->GetLightComponent());
	return IsValid(Local) && Local->IsRegistered() && Local->Mobility != EComponentMobility::Static
		&& !Local->ComponentHasTag(TEXT("MallGhostSignal")) && FMath::IsFinite(Local->Intensity) && Local->Intensity >= 0.0f;
}

void UMallGhostLightComponent::UpdateAuthoritativeState()
{
	AActor* Ghost = GetOwner();
	if (!IsValid(Ghost) || !Ghost->HasAuthority() || !GetWorld()) { return; }
	const double Now = GetWorld()->GetTimeSeconds();
	const EMallGhostLightMode Requested = DetermineMode();
	if (Requested == EMallGhostLightMode::Off)
	{
		SelectedLights.Reset();
	}
	else if (Requested != LastRequestedMode || TargetLightTag != LastTargetTag || Now >= NextTargetRefreshTime)
	{
		SelectedLights.Reset();
		if (!TargetLightTag.IsNone())
		{
			for (TActorIterator<ALight> It(GetWorld()); It; ++It)
			{
				if (IsEligibleLight(*It)) { SelectedLights.Add(*It); }
			}
		}
		NextTargetRefreshTime = Now + 0.5;
	}
	LastRequestedMode = Requested;
	LastTargetTag = TargetLightTag;

	TArray<TObjectPtr<ALight>> Targets;
	if (Requested != EMallGhostLightMode::Off)
	{
		for (const TWeakObjectPtr<ALight>& Candidate : SelectedLights)
		{
			if (IsEligibleLight(Candidate.Get())) { Targets.Add(Candidate.Get()); }
		}
	}
	const EMallGhostLightMode NewMode = Targets.IsEmpty() ? EMallGhostLightMode::Off : Requested;
	const float Minimum = NewMode == EMallGhostLightMode::Warning
		? MallGhostLights::SafeMinimum(WarningMinimumBrightness, 0.1f)
		: NewMode == EMallGhostLightMode::Chasing ? MallGhostLights::SafeMinimum(ChaseMinimumBrightness, 0.0f) : 1.0f;
	const float Rate = MallGhostLights::SafeRate(FlickerRate);
	if (FlickerState.Mode == NewMode && FlickerState.Targets == Targets
		&& FlickerState.MinimumBrightness == Minimum && FlickerState.Rate == Rate) { return; }

	RestoreLights();
	if (NewMode != FlickerState.Mode)
	{
		FlickerState.Seed = NewMode == EMallGhostLightMode::Off ? 0 : FMath::Rand();
		FlickerState.EpochServerTime = NewMode == EMallGhostLightMode::Off ? 0.0 : Now;
	}
	FlickerState.Mode = NewMode;
	FlickerState.MinimumBrightness = Minimum;
	FlickerState.Rate = Rate;
	FlickerState.Targets = MoveTemp(Targets);
	Ghost->ForceNetUpdate();
}

float UMallGhostLightComponent::EvaluateMultiplier(const FMallGhostLightState& State, const double ElapsedSeconds)
{
	if (State.Mode == EMallGhostLightMode::Off || !FMath::IsFinite(ElapsedSeconds)) { return 1.0f; }
	const double Position = FMath::Clamp(ElapsedSeconds, 0.0, 1000000000.0) * MallGhostLights::SafeRate(State.Rate);
	const int64 Knot = FMath::FloorToInt64(Position);
	const float Fraction = static_cast<float>(Position - static_cast<double>(Knot));
	// Hold each endpoint briefly so a tick actually reaches the configured minimum,
	// including complete outages, rather than only approaching a random brightness.
	const float Blend = FMath::Clamp((Fraction - 0.15f) / 0.7f, 0.0f, 1.0f);
	const float Smooth = Blend * Blend * (3.0f - 2.0f * Blend);
	const auto KnotBrightness = [&State](const int64 Index)
	{
		return (Index & 1) != 0 ? 0.0f : FMath::Lerp(0.65f, 1.0f, MallGhostLights::KnotValue(Index, State.Seed));
	};
	const float Noise = FMath::Lerp(KnotBrightness(Knot), KnotBrightness(Knot + 1), Smooth);
	const float Minimum = MallGhostLights::SafeMinimum(State.MinimumBrightness, 1.0f);
	return FMath::Lerp(Minimum, 1.0f, Noise);
}

void UMallGhostLightComponent::ApplyFlicker()
{
	if (!GetWorld() || !bEnableFlicker || !IsActive() || !IsRegistered() || FlickerState.Mode == EMallGhostLightMode::Off)
	{
		RestoreLights();
		return;
	}
	const AGameStateBase* State = GetWorld()->GetGameState();
	const double Now = State ? State->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
	const float Multiplier = EvaluateMultiplier(FlickerState, Now - FlickerState.EpochServerTime);
	TSet<TWeakObjectPtr<ULocalLightComponent>> CurrentLights;
	for (ALight* Light : FlickerState.Targets)
	{
		// Clients consume the server's explicit references, without consulting local tags or AI state.
		ULocalLightComponent* Local = IsValid(Light) && !Light->IsActorBeingDestroyed()
			? Cast<ULocalLightComponent>(Light->GetLightComponent()) : nullptr;
		if (!IsValid(Local) || !Local->IsRegistered() || Local->Mobility == EComponentMobility::Static) { continue; }
		const TWeakObjectPtr<ULocalLightComponent> Key(Local);
		CurrentLights.Add(Key);
		if (!OriginalIntensities.Contains(Key))
		{
			if (!FMath::IsFinite(Local->Intensity) || Local->Intensity < 0.0f) { CurrentLights.Remove(Key); continue; }
			OriginalIntensities.Add(Key, Local->Intensity);
		}
		if (GetNetMode() != NM_DedicatedServer)
		{
			Local->SetIntensity(OriginalIntensities.FindChecked(Key) * Multiplier);
		}
	}
	for (auto It = OriginalIntensities.CreateIterator(); It; ++It)
	{
		if (CurrentLights.Contains(It.Key())) { continue; }
		if (ULocalLightComponent* Local = It.Key().Get(); IsValid(Local) && GetNetMode() != NM_DedicatedServer)
		{
			Local->SetIntensity(It.Value());
		}
		It.RemoveCurrent();
	}
	TargetLightCount = CurrentLights.Num();
	AppliedIntensityMultiplier = TargetLightCount > 0 ? Multiplier : 1.0f;
}

void UMallGhostLightComponent::RestoreLights()
{
	for (const TPair<TWeakObjectPtr<ULocalLightComponent>, float>& Entry : OriginalIntensities)
	{
		if (ULocalLightComponent* Light = Entry.Key.Get(); IsValid(Light) && GetNetMode() != NM_DedicatedServer)
		{
			Light->SetIntensity(Entry.Value);
		}
	}
	OriginalIntensities.Reset();
	TargetLightCount = 0;
	AppliedIntensityMultiplier = 1.0f;
}

void UMallGhostLightComponent::OnRep_FlickerState()
{
	RestoreLights();
	ApplyFlicker();
}

void UMallGhostLightComponent::ClearAuthoritativeState()
{
	if (AActor* Ghost = GetOwner(); IsValid(Ghost) && Ghost->HasAuthority())
	{
		FlickerState = FMallGhostLightState();
		SelectedLights.Reset();
		LastRequestedMode = EMallGhostLightMode::Off;
		NextTargetRefreshTime = 0.0;
		Ghost->ForceNetUpdate();
	}
}

void UMallGhostLightComponent::Deactivate()
{
	Super::Deactivate();
	ClearAuthoritativeState();
	RestoreLights();
}

void UMallGhostLightComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RestoreLights();
	ClearAuthoritativeState();
	Super::EndPlay(EndPlayReason);
}

void UMallGhostLightComponent::OnUnregister()
{
	RestoreLights();
	ClearAuthoritativeState();
	Super::OnUnregister();
}

void UMallGhostLightComponent::OnComponentDestroyed(const bool bDestroyingHierarchy)
{
	RestoreLights();
	ClearAuthoritativeState();
	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

void UMallGhostLightComponent::TickComponent(const float DeltaTime, const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdateAuthoritativeState();
	ApplyFlicker();
}
