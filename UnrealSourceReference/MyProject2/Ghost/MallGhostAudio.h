#pragma once

#include "CoreMinimal.h"

class AActor;
class UAudioComponent;
class USoundBase;
struct FSoundAttenuationSettings;

/** Local spatial playback shared by ghost cues. Invoke from an authoritative multicast for network events. */
namespace MallGhostAudio
{
	/** Pawn eye position, or the actor origin for non-pawns. */
	MYPROJECT2_API FVector SourceLocation(const AActor* Ghost);

	/** Full volume within one metre, fading over the next 24 metres; no baked occlusion/reverb. */
	MYPROJECT2_API FSoundAttenuationSettings SpatialAttenuation();

	/** Attach an owned component to the ghost, preserving its initial eye-height offset. Does not play audio. */
	MYPROJECT2_API bool ConfigureAndAttach(UAudioComponent* Audio, AActor* Ghost);

	/** Play locally at the ghost and follow its movement. Requires neither a radio nor a powered device. */
	MYPROJECT2_API UAudioComponent* PlayAttached(AActor* Ghost, USoundBase* Sound, float Volume = 0.6f);
}
