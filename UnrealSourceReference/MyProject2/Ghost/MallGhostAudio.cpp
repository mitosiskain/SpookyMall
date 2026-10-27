#include "Ghost/MallGhostAudio.h"

#include "Components/AudioComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"

FVector MallGhostAudio::SourceLocation(const AActor* Ghost)
{
	if (const APawn* Pawn = Cast<APawn>(Ghost)) { return Pawn->GetPawnViewLocation(); }
	return IsValid(Ghost) ? Ghost->GetActorLocation() : FVector::ZeroVector;
}

FSoundAttenuationSettings MallGhostAudio::SpatialAttenuation()
{
	FSoundAttenuationSettings Settings;
	Settings.bAttenuate = true;
	Settings.bSpatialize = true;
	Settings.bEnableOcclusion = false;
	Settings.bEnableListenerFocus = false;
	Settings.DistanceAlgorithm = EAttenuationDistanceModel::Linear;
	Settings.AttenuationShape = EAttenuationShape::Sphere;
	Settings.AttenuationShapeExtents = FVector(100.0f, 0.0f, 0.0f);
	Settings.FalloffDistance = 2400.0f;
	Settings.NonSpatializedRadiusStart = 0.0f;
	Settings.NonSpatializedRadiusEnd = 0.0f;
	Settings.StereoSpread = 0.0f;
	return Settings;
}

bool MallGhostAudio::ConfigureAndAttach(UAudioComponent* Audio, AActor* Ghost)
{
	USceneComponent* Root = IsValid(Ghost) ? Ghost->GetRootComponent() : nullptr;
	if (!IsValid(Audio) || !IsValid(Root) || Audio->GetOwner() != Ghost || !Ghost->GetWorld()
		|| Audio->GetWorld() != Ghost->GetWorld()) { return false; }
	const FVector Source = SourceLocation(Ghost);
	if (Source.ContainsNaN() || Root->GetComponentTransform().ContainsNaN()) { return false; }
	Audio->bAutoActivate = false;
	Audio->bAllowSpatialization = true;
	Audio->bIsUISound = false;
	Audio->bOverrideAttenuation = true;
	Audio->SetAttenuationOverrides(SpatialAttenuation());
	if (!Audio->AttachToComponent(Root, FAttachmentTransformRules::KeepWorldTransform)) { return false; }
	Audio->SetAbsolute(false, false, false);
	Audio->SetRelativeLocation(Root->GetComponentTransform().InverseTransformPosition(Source));
	return true;
}

UAudioComponent* MallGhostAudio::PlayAttached(AActor* Ghost, USoundBase* Sound, const float Volume)
{
	if (!IsValid(Ghost) || Ghost->IsActorBeingDestroyed() || !Ghost->GetWorld()
		|| Ghost->GetNetMode() == NM_DedicatedServer || !IsValid(Sound)
		|| !FMath::IsFinite(Volume) || Volume <= 0.0f) { return nullptr; }
	// Playback-completion auto cleanup never runs when audio cannot start.
	if (!Ghost->GetWorld()->AllowAudioPlayback() || !Ghost->GetWorld()->GetAudioDeviceRaw()) { return nullptr; }
	UAudioComponent* Audio = NewObject<UAudioComponent>(Ghost, NAME_None, RF_Transient);
	if (!ConfigureAndAttach(Audio, Ghost))
	{
		Audio->DestroyComponent();
		return nullptr;
	}
	Audio->bAutoDestroy = true;
	Audio->bStopWhenOwnerDestroyed = true;
	Audio->SetVolumeMultiplier(FMath::Clamp(Volume, 0.0f, 4.0f));
	Audio->SetSound(Sound);
	Audio->RegisterComponent();
	Audio->Play();
	if (IsValid(Audio) && !Audio->IsActive())
	{
		Audio->DestroyComponent();
		return nullptr;
	}
	return IsValid(Audio) ? Audio : nullptr;
}
