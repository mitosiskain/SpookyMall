#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Ghost/MallGhostAudio.h"
#include "Components/AudioComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "Misc/AutomationTest.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundWave.h"

namespace MallGhostAudioTest
{
	struct FPreviewWorld
	{
		UWorld* World = nullptr;
		FPreviewWorld()
		{
			// Preview worlds do not start gameplay/voice subsystems or open microphone hardware.
			UWorld::InitializationValues Init;
			Init.AllowAudioPlayback(false).CreatePhysicsScene(false).ShouldSimulatePhysics(false)
				.CreateNavigation(false).CreateAISystem(false);
			World = UWorld::CreateWorld(EWorldType::EditorPreview, false, NAME_None, nullptr, true,
				ERHIFeatureLevel::Num, &Init);
		}
		~FPreviewWorld() { if (World) { World->DestroyWorld(false); } }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMallGhostSpatialAudioTest, "Mall.Ghost.SpatialAudio",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMallGhostSpatialAudioTest::RunTest(const FString& Parameters)
{
	MallGhostAudioTest::FPreviewWorld Fixture;
	if (!TestNotNull(TEXT("A silent preview world is available"), Fixture.World)) { return false; }
	FActorSpawnParameters Spawn;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ACharacter* Ghost = Fixture.World->SpawnActor<ACharacter>(FVector(300, 100, 90), FRotator::ZeroRotator, Spawn);
	if (!TestNotNull(TEXT("A ghost pawn exists without any radio"), Ghost)) { return false; }
	Ghost->BaseEyeHeight = 64.0f;
	UAudioComponent* Audio = NewObject<UAudioComponent>(Ghost, NAME_None, RF_Transient);
	if (!TestTrue(TEXT("Ghost sound can be configured without a spirit box"), MallGhostAudio::ConfigureAndAttach(Audio, Ghost))) { return false; }
	Audio->RegisterComponent();
	TestTrue(TEXT("Ghost cue starts at the pawn's eye position"),
		Audio->GetComponentLocation().Equals(Ghost->GetPawnViewLocation(), 0.01));
	TestTrue(TEXT("Ghost cue is attached to the ghost rather than held at a fixed world position"),
		Audio->GetAttachParent() == Ghost->GetRootComponent() && !Audio->IsUsingAbsoluteLocation());
	TestTrue(TEXT("Cue configuration never starts playback or replays a previous event"),
		!Audio->bAutoActivate && !Audio->IsPlaying() && Audio->Sound == nullptr);
	TestTrue(TEXT("Physical ghost sounds use positional world audio"), Audio->bAllowSpatialization && !Audio->bIsUISound);
	const FSoundAttenuationSettings* Settings = Audio->GetAttenuationSettingsToApply();
	if (!TestNotNull(TEXT("Ghost cue has directional attenuation"), Settings)) { return false; }
	TestTrue(TEXT("Ghost sounds spatialize through the one metre inner zone and 24 metre fade"),
		Settings->bAttenuate && Settings->bSpatialize && Settings->DistanceAlgorithm == EAttenuationDistanceModel::Linear
		&& Settings->AttenuationShape == EAttenuationShape::Sphere
		&& FMath::IsNearlyEqual(Settings->AttenuationShapeExtents.X, 100.0f)
		&& FMath::IsNearlyEqual(Settings->FalloffDistance, 2400.0f)
		&& Settings->NonSpatializedRadiusStart == 0.0f && Settings->NonSpatializedRadiusEnd == 0.0f
		&& Settings->StereoSpread == 0.0f);
	TestFalse(TEXT("Map-specific occlusion remains deferred"), Settings->bEnableOcclusion);

	Ghost->SetActorLocationAndRotation(FVector(450, -200, 120), FRotator(0, 70, 0), false, nullptr, ETeleportType::TeleportPhysics);
	TestTrue(TEXT("An attached ghost cue follows translation and turn without radio ticks"),
		Audio->GetComponentLocation().Equals(Ghost->GetPawnViewLocation(), 0.01));
	const FVector GhostSoundPosition = Audio->GetComponentLocation();
	AActor* Other = Fixture.World->SpawnActor<AActor>(FVector::ZeroVector, FRotator::ZeroRotator, Spawn);
	if (!TestNotNull(TEXT("An unrelated device actor exists"), Other)) { return false; }
	USceneComponent* DeviceRoot = NewObject<USceneComponent>(Other, NAME_None, RF_Transient);
	Other->SetRootComponent(DeviceRoot);
	DeviceRoot->RegisterComponent();
	Other->SetActorLocation(FVector(-1000, 2500, 500));
	TestTrue(TEXT("Moving another device does not move the physical ghost sound"),
		Audio->GetComponentLocation().Equals(GhostSoundPosition, 0.01));
	TestFalse(TEXT("A component belonging to another device cannot be rebound as a ghost cue"),
		MallGhostAudio::ConfigureAndAttach(Audio, Other));
	TestFalse(TEXT("A missing ghost cannot configure a sound"), MallGhostAudio::ConfigureAndAttach(Audio, nullptr));
	TestNull(TEXT("Missing sound cannot create physical playback"), MallGhostAudio::PlayAttached(Ghost, nullptr));
	TestNull(TEXT("Missing ghost cannot create physical playback"), MallGhostAudio::PlayAttached(nullptr, nullptr));
	USoundWave* Wave = NewObject<USoundWave>(Ghost, NAME_None, RF_Transient);
	Wave->Duration = 0.25f;
	const int32 ComponentCount = Ghost->GetComponents().Num();
	for (int32 Attempt = 0; Attempt < 3; ++Attempt)
	{
		TestNull(TEXT("Disabled world audio rejects a valid sound without allocating a cue"), MallGhostAudio::PlayAttached(Ghost, Wave));
	}
	TestEqual(TEXT("Failed silent playback does not accumulate ghost components"), Ghost->GetComponents().Num(), ComponentCount);
	// The -nosound suite has no device. In a normal editor, never disable its shared device just for a test.
	if (!Fixture.World->GetAudioDeviceRaw())
	{
		Fixture.World->bAllowAudioPlayback = true;
		TestNull(TEXT("A missing audio device rejects playback even when the world permits audio"), MallGhostAudio::PlayAttached(Ghost, Wave));
		TestEqual(TEXT("Missing-device rejection creates no component"), Ghost->GetComponents().Num(), ComponentCount);
		Fixture.World->bAllowAudioPlayback = false;
	}
	Audio->DestroyComponent();
	return true;
}

#endif
