#include "Ghost/MallGhostSoundComponent.h"
#include "Ghost/MallGhostCartHelpers.h"

#include "Ghost/MallGhostAudio.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundBase.h"
#include "UObject/UnrealType.h"

namespace MallGhostSounds
{
	constexpr double AudibleRangeCm = 2500.0;
	constexpr double MaximumCueSeconds = 15.0;

	bool HasExpiredCooldown(const AActor* Ghost)
	{
		const FNumericProperty* Cooldown = IsValid(Ghost)
			? FindFProperty<FNumericProperty>(Ghost->GetClass(), TEXT("CooldownUntil")) : nullptr;
		if (!Cooldown || !Cooldown->IsFloatingPoint() || !Ghost->GetWorld()) { return false; }
		const double Until = Cooldown->GetFloatingPointPropertyValue(Cooldown->ContainerPtrToValuePtr<void>(Ghost));
		return FMath::IsFinite(Until) && Ghost->GetWorld()->GetTimeSeconds() >= Until;
	}

	bool IsAlive(const APawn* Pawn)
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

UMallGhostSoundComponent::UMallGhostSoundComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.25f;
	InvisibleSounds.Add(TSoftObjectPtr<USoundBase>(FSoftObjectPath(
		TEXT("/Game/AI/StoreGhost/Audio/SW_Ghost_InvisibleBreath.SW_Ghost_InvisibleBreath"))));
	InvisibleSounds.Add(TSoftObjectPtr<USoundBase>(FSoftObjectPath(
		TEXT("/Game/AI/StoreGhost/Audio/SW_Ghost_InvisibleMoan.SW_Ghost_InvisibleMoan"))));
	HuntSounds.Add(TSoftObjectPtr<USoundBase>(FSoftObjectPath(
		TEXT("/Game/AI/StoreGhost/Audio/SW_Ghost_HuntGrowl.SW_Ghost_HuntGrowl"))));
}

void UMallGhostSoundComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		// Attachment makes local audio follow the ghost without a client tick.
		SetComponentTickEnabled(false);
		return;
	}
	if (const UMallGhostInteractionComponent* Interaction = GetOwner()->FindComponentByClass<UMallGhostInteractionComponent>())
	{
		ObservedMode = Interaction->GetGhostMode();
	}
	bObservedChasing = IsEligibleChase();
	NextCueTime = GetWorld()->GetTimeSeconds() + 6.0;
}

void UMallGhostSoundComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopLocalAudio();
	bActiveCue = false;
	Super::EndPlay(EndPlayReason);
}

void UMallGhostSoundComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UMallGhostSoundComponent, CueCount);
	DOREPLIFETIME(UMallGhostSoundComponent, LastCueKind);
	DOREPLIFETIME(UMallGhostSoundComponent, LastSound);
	DOREPLIFETIME(UMallGhostSoundComponent, LastSourceLocation);
}

bool UMallGhostSoundComponent::IsEligiblePlayer(const APawn* Pawn) const
{
	const AActor* Ghost = GetOwner();
	const APlayerController* Controller = IsValid(Pawn) ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	const UMallGhostInteractionComponent* Interaction = IsValid(Ghost) ? Ghost->FindComponentByClass<UMallGhostInteractionComponent>() : nullptr;
	const bool bCartChase = IsValid(Ghost) && Ghost->ActorHasTag(TEXT("MallCartBoundary"))
		&& Interaction && Interaction->GetGhostMode() == EMallGhostMode::VisibleHunt;
	const bool bDomainEligible = bCartChase
		? UMallGhostCartHelpers::CanGhostPursuePlayer(const_cast<AActor*>(Ghost), const_cast<APawn*>(Pawn))
		: UMallGhostInteractionComponent::IsActorInGhostStore(Ghost, Pawn);
	if (!Ghost || !GetWorld() || !MallGhostSounds::IsAlive(Pawn) || !IsValid(Controller)
		|| Controller->IsActorBeingDestroyed() || !Controller->HasAuthority() || Controller->GetPawn() != Pawn
		|| !Pawn->HasAuthority() || Pawn->GetWorld() != GetWorld() || Controller->GetWorld() != GetWorld()
		|| !bDomainEligible) { return false; }
	const FVector GhostLocation = MallGhostAudio::SourceLocation(Ghost);
	const FVector PlayerLocation = Pawn->GetActorLocation();
	return !GhostLocation.ContainsNaN() && !PlayerLocation.ContainsNaN()
		&& FVector::DistSquared(GhostLocation, PlayerLocation) <= FMath::Square(MallGhostSounds::AudibleRangeCm);
}

bool UMallGhostSoundComponent::IsEligibleChase() const
{
	const AActor* Ghost = GetOwner();
	const UMallGhostInteractionComponent* Interaction = IsValid(Ghost)
		? Ghost->FindComponentByClass<UMallGhostInteractionComponent>() : nullptr;
	if (!Ghost || Ghost->IsActorBeingDestroyed() || !Ghost->HasAuthority() || !Interaction
		|| !UMallGhostInteractionComponent::CanGhostHunt(const_cast<AActor*>(Ghost))
		|| !MallGhostSounds::HasExpiredCooldown(Ghost)
		|| (!Ghost->ActorHasTag(TEXT("MallCartBoundary"))
			&& !UMallGhostInteractionComponent::IsActorInGhostStore(Ghost, Ghost))) { return false; }
	// These are the authored BP_StoreGhost's actual movement state and current player.
	// A visible but idle ghost must not emit a chase cue.
	const FStrProperty* State = FindFProperty<FStrProperty>(Ghost->GetClass(), TEXT("PrototypeState"));
	const FObjectPropertyBase* Target = FindFProperty<FObjectPropertyBase>(Ghost->GetClass(), TEXT("TargetPlayer"));
	return State && State->GetPropertyValue_InContainer(Ghost) == TEXT("Chasing") && Target
		&& IsEligiblePlayer(Cast<APawn>(Target->GetObjectPropertyValue_InContainer(Ghost)));
}

bool UMallGhostSoundComponent::GetEligibleKind(EMallGhostSoundKind& OutKind) const
{
	const AActor* Ghost = GetOwner();
	const UMallGhostInteractionComponent* Interaction = IsValid(Ghost)
		? Ghost->FindComponentByClass<UMallGhostInteractionComponent>() : nullptr;
	if (!Ghost || Ghost->IsActorBeingDestroyed() || !Ghost->HasAuthority() || !GetWorld()
		|| !Interaction || !MallGhostSounds::HasExpiredCooldown(Ghost)) { return false; }
	if (Interaction->GetGhostMode() == EMallGhostMode::VisibleHunt)
	{
		OutKind = EMallGhostSoundKind::Hunt;
		return IsEligibleChase();
	}
	if (Interaction->GetGhostMode() != EMallGhostMode::InvisibleDisturbance
		|| !Interaction->CanPerformDisturbance()
		|| !UMallGhostInteractionComponent::IsActorInGhostStore(Ghost, Ghost)) { return false; }
	OutKind = EMallGhostSoundKind::Invisible;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* Controller = It->Get();
		if (IsValid(Controller) && IsEligiblePlayer(Controller->GetPawn())) { return true; }
	}
	return false;
}

double UMallGhostSoundComponent::CueGap(const EMallGhostSoundKind Kind) const
{
	const bool bHunt = Kind == EMallGhostSoundKind::Hunt;
	const float RequestedMin = bHunt ? HuntMinGap : InvisibleMinGap;
	const float RequestedMax = bHunt ? HuntMaxGap : InvisibleMaxGap;
	const float Low = FMath::Clamp(FMath::IsFinite(RequestedMin) ? RequestedMin : (bHunt ? 3.0f : 8.0f), 0.25f, 120.0f);
	const float High = FMath::Clamp(FMath::IsFinite(RequestedMax) ? RequestedMax : (bHunt ? 6.0f : 16.0f), Low, 120.0f);
	return FMath::FRandRange(Low, High);
}

bool UMallGhostSoundComponent::TryPlayCue()
{
	EMallGhostSoundKind Kind;
	if (!GetEligibleKind(Kind)) { return false; }
	const double Now = GetWorld()->GetTimeSeconds();
	if (bActiveCue || Now < ActiveUntil || Now < NextCueTime) { return false; }
	const float Volume = Kind == EMallGhostSoundKind::Hunt ? HuntVolume : InvisibleVolume;
	if (!FMath::IsFinite(Volume) || Volume <= 0.0f) { return false; }
	const TArray<TSoftObjectPtr<USoundBase>>& Choices = Kind == EMallGhostSoundKind::Hunt ? HuntSounds : InvisibleSounds;
	if (Choices.IsEmpty()) { NextCueTime = Now + CueGap(Kind); return false; }
	USoundBase* Selected = nullptr;
	double Duration = 0.0;
	const int32 Start = FMath::RandHelper(Choices.Num());
	for (int32 Offset = 0; Offset < Choices.Num(); ++Offset)
	{
		USoundBase* Candidate = Choices[(Start + Offset) % Choices.Num()].LoadSynchronous();
		const double CandidateDuration = IsValid(Candidate) ? Candidate->GetDuration() : 0.0;
		if (IsValid(Candidate) && !Candidate->IsLooping() && FMath::IsFinite(CandidateDuration)
			&& CandidateDuration > 0.0 && CandidateDuration <= MallGhostSounds::MaximumCueSeconds)
		{
			Selected = Candidate;
			Duration = CandidateDuration;
			break;
		}
	}
	if (!Selected) { NextCueTime = Now + CueGap(Kind); return false; }
	const FVector Source = MallGhostAudio::SourceLocation(GetOwner());
	if (Source.ContainsNaN()) { return false; }
	bActiveCue = true;
	ObservedMode = Kind == EMallGhostSoundKind::Hunt ? EMallGhostMode::VisibleHunt : EMallGhostMode::InvisibleDisturbance;
	bObservedChasing = Kind == EMallGhostSoundKind::Hunt;
	ActiveUntil = Now + Duration;
	NextCueTime = ActiveUntil + CueGap(Kind);
	LastCueKind = Kind;
	LastSound = Selected;
	LastSourceLocation = Source;
	++CueCount;
	MulticastCue(Selected, FMath::Clamp(Volume, 0.0f, 4.0f), ++NextEventSerial);
	GetOwner()->ForceNetUpdate();
	return true;
}

void UMallGhostSoundComponent::StopLocalAudio()
{
	if (IsValid(ActiveAudio))
	{
		UAudioComponent* Audio = ActiveAudio;
		ActiveAudio = nullptr;
		Audio->Stop();
		if (IsValid(Audio)) { Audio->DestroyComponent(); }
	}
	else { ActiveAudio = nullptr; }
}

void UMallGhostSoundComponent::StopCue()
{
	if (!bActiveCue) { return; }
	bActiveCue = false;
	ActiveUntil = 0.0;
	MulticastCue(nullptr, 0.0f, ++NextEventSerial);
}

void UMallGhostSoundComponent::MulticastCue_Implementation(USoundBase* Sound, const float Volume, const int32 Serial)
{
	if (Serial <= LastAppliedSerial) { return; }
	LastAppliedSerial = Serial;
	StopLocalAudio();
	if (!IsValid(Sound)) { return; }
	// Count delivered events even with -nosound so network tests do not require hardware.
	++LocalPlaybackCount;
	ActiveAudio = MallGhostAudio::PlayAttached(GetOwner(), Sound, Volume);
}

void UMallGhostSoundComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!GetOwner() || !GetOwner()->HasAuthority() || !GetWorld()) { return; }
	const UMallGhostInteractionComponent* Interaction = GetOwner()->FindComponentByClass<UMallGhostInteractionComponent>();
	if (!Interaction) { StopCue(); return; }
	const double Now = GetWorld()->GetTimeSeconds();
	const EMallGhostMode Mode = Interaction->GetGhostMode();
	const bool bChasing = IsEligibleChase();
	if (Mode != ObservedMode || bChasing != bObservedChasing)
	{
		StopCue();
		ObservedMode = Mode;
		bObservedChasing = bChasing;
		NextCueTime = Now + (bChasing ? 0.2 : CueGap(EMallGhostSoundKind::Invisible));
	}
	EMallGhostSoundKind EligibleKind;
	const bool bEligible = GetEligibleKind(EligibleKind);
	if (bActiveCue && (!bEligible || EligibleKind != LastCueKind || Now >= ActiveUntil)) { StopCue(); }
	if (bAutomaticSounds && bEligible && !bActiveCue && Now >= NextCueTime) { TryPlayCue(); }
}
