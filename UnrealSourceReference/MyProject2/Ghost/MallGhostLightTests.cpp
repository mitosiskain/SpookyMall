#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Ghost/MallGhostLightComponent.h"
#include "Ghost/MallGhostInteractionComponent.h"
#include "Ghost/MallGhostSoundComponent.h"
#include "Voice/MallVoiceSettings.h"
#include "Components/CapsuleComponent.h"
#include "Components/LightComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/PointLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameMapsSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Tests/NetTestHelpers.h"
#include "UObject/UnrealType.h"

namespace MallGhostLightTest
{
	struct FRestoreSettings
	{
		UMallVoiceSettings* Settings = GetMutableDefault<UMallVoiceSettings>();
		bool bVoice = Settings->bEnabled, bSpeech = Settings->bSpeechRecognitionEnabled, bStatus = Settings->bShowVoiceStatus;
		FString Map = UGameMapsSettings::GetGameDefaultMap(EDefaultMapRequestType::Client);
		FString Mode = UGameMapsSettings::GetGlobalDefaultGameMode();
		TSubclassOf<UGameViewportClient> Viewport = GEngine->GameViewportClientClass;
		FRestoreSettings()
		{
			Settings->bEnabled = Settings->bSpeechRecognitionEnabled = Settings->bShowVoiceStatus = false;
			UGameMapsSettings::SetGameDefaultMap(TEXT("/Engine/Maps/Entry"));
			UGameMapsSettings::SetGlobalDefaultGameMode(TEXT("/Script/Engine.GameMode"));
			if (UClass* Compatible = LoadClass<UGameViewportClient>(nullptr, TEXT("/Script/CommonUI.CommonGameViewportClient")))
			{
				GEngine->GameViewportClientClass = Compatible;
			}
		}
		~FRestoreSettings()
		{
			Settings->bEnabled = bVoice; Settings->bSpeechRecognitionEnabled = bSpeech; Settings->bShowVoiceStatus = bStatus;
			UGameMapsSettings::SetGameDefaultMap(Map); UGameMapsSettings::SetGlobalDefaultGameMode(Mode);
			GEngine->GameViewportClientClass = Viewport;
		}
	};

	void FreezeCharacters(UWorld* World)
	{
		for (TActorIterator<ACharacter> It(World); It; ++It)
		{
			It->SetActorTickEnabled(false);
			It->GetCharacterMovement()->DisableMovement();
			It->GetCharacterMovement()->SetComponentTickEnabled(false);
		}
	}

	ACharacter* FindGhost(UWorld* World, UClass* Class)
	{
		for (TActorIterator<ACharacter> It(World); It; ++It) { if (It->IsA(Class)) { return *It; } }
		return nullptr;
	}

	APointLight* FindLight(UWorld* World, FVector Location)
	{
		for (TActorIterator<APointLight> It(World); It; ++It)
		{
			if (It->GetActorLocation().Equals(Location, 1.0)) { return *It; }
		}
		return nullptr;
	}

	bool SetNumber(AActor* Actor, FName Name, double Value)
	{
		FNumericProperty* Property = Actor ? FindFProperty<FNumericProperty>(Actor->GetClass(), Name) : nullptr;
		if (!Property || !Property->IsFloatingPoint()) { return false; }
		Property->SetFloatingPointPropertyValue(Property->ContainerPtrToValuePtr<void>(Actor), Value);
		return true;
	}

	bool SetChase(AActor* Ghost, const FString& State, APawn* Target)
	{
		FStrProperty* StateProperty = Ghost ? FindFProperty<FStrProperty>(Ghost->GetClass(), TEXT("PrototypeState")) : nullptr;
		FObjectPropertyBase* TargetProperty = Ghost ? FindFProperty<FObjectPropertyBase>(Ghost->GetClass(), TEXT("TargetPlayer")) : nullptr;
		if (!StateProperty || !TargetProperty) { return false; }
		StateProperty->SetPropertyValue_InContainer(Ghost, State);
		TargetProperty->SetObjectPropertyValue_InContainer(Ghost, Target);
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMallGhostLightNetworkTest, "Mall.Ghost.StoreLightNetwork",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMallGhostLightNetworkTest::RunTest(const FString& Parameters)
{
	using namespace MallGhostLightTest;
	// Sample the intended profiles before networking: dim endpoints must be real holds,
	// rather than an isolated zero that a frame or a late-joining client can never see.
	for (int32 Seed : {0, 17, 20261018})
	{
		FMallGhostLightState WarningProfile;
		WarningProfile.Mode = EMallGhostLightMode::Warning; WarningProfile.Seed = Seed;
		WarningProfile.MinimumBrightness = 0.10f; WarningProfile.Rate = 2.3f;
		FMallGhostLightState ChaseProfile = WarningProfile;
		ChaseProfile.Mode = EMallGhostLightMode::Chasing; ChaseProfile.MinimumBrightness = 0.0f;
		bool bBounded = true, bDeterministic = true;
		int32 ZeroRun = 0, LongestZeroRun = 0, WarningFloorRun = 0, LongestWarningFloorRun = 0;
		float ChasePeak = 0.0f;
		for (int32 SampleIndex = 0; SampleIndex <= 60; ++SampleIndex)
		{
			const double Elapsed = SampleIndex * 0.05;
			const float Warning = UMallGhostLightComponent::EvaluateMultiplier(WarningProfile, Elapsed);
			const float Chase = UMallGhostLightComponent::EvaluateMultiplier(ChaseProfile, Elapsed);
			bBounded &= FMath::IsFinite(Warning) && Warning >= 0.10f && Warning <= 1.0f
				&& FMath::IsFinite(Chase) && Chase >= 0.0f && Chase <= 1.0f;
			bDeterministic &= Warning == UMallGhostLightComponent::EvaluateMultiplier(WarningProfile, Elapsed)
				&& Chase == UMallGhostLightComponent::EvaluateMultiplier(ChaseProfile, Elapsed);
			ZeroRun = Chase == 0.0f ? ZeroRun + 1 : 0;
			WarningFloorRun = Warning == 0.10f ? WarningFloorRun + 1 : 0;
			LongestZeroRun = FMath::Max(LongestZeroRun, ZeroRun);
			LongestWarningFloorRun = FMath::Max(LongestWarningFloorRun, WarningFloorRun);
			ChasePeak = FMath::Max(ChasePeak, Chase);
		}
		TestTrue(TEXT("Warning and chase profiles remain finite and within their configured bounds"), bBounded);
		TestTrue(TEXT("Profile samples are identical for the same server seed and elapsed time"), bDeterministic);
		TestTrue(TEXT("Chase reaches exactly zero for consecutive 50 ms samples"), LongestZeroRun >= 2);
		TestTrue(TEXT("Warning holds its exact 10 percent floor for consecutive 50 ms samples"), LongestWarningFloorRun >= 2);
		TestTrue(TEXT("Chase recovers to a bright interval after its blackouts"), ChasePeak > 0.5f);
		ChaseProfile.Mode = EMallGhostLightMode::Off;
		TestEqual(TEXT("Off returns full original intensity even when the chase floor is zero"),
			UMallGhostLightComponent::EvaluateMultiplier(ChaseProfile, 1.0), 1.0f);
	}
	FRestoreSettings Restore;
	UE::Net::FTestWorlds Worlds(TEXT("/Engine/Maps/Entry"), TEXT("/Script/Engine.GameMode"));
	if (!TestTrue(TEXT("Light authority world loads"), Worlds.Server.IsLoaded())
		|| !TestTrue(TEXT("First real IP peer connects"), Worlds.CreateAndConnectClient())
		|| !TestTrue(TEXT("Second real IP peer connects"), Worlds.CreateAndConnectClient())) { return false; }
	Worlds.PreTickAllDelegate.AddLambda([&]()
	{
		FreezeCharacters(Worlds.Server.GetWorld());
		for (UE::Net::FTestWorldInstance& Client : Worlds.Clients) { FreezeCharacters(Client.GetWorld()); }
	});
	UWorld* Server = Worlds.Server.GetWorld();
	APlayerController* PC[2] = {};
	if (!TestTrue(TEXT("Connected authoritative controllers exist"), Worlds.TickUntil([&]()
	{
		PC[0] = Worlds.GetServerPlayerControllerOfClient(0); PC[1] = Worlds.GetServerPlayerControllerOfClient(1);
		return PC[0] && PC[1];
	}, 120))) { return false; }
	FActorSpawnParameters Spawn;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ACharacter* Player[2] = {};
	for (int32 Index = 0; Index < 2; ++Index)
	{
		Player[Index] = Server->SpawnActor<ACharacter>(FVector(4800, 200 * Index, 90), FRotator::ZeroRotator, Spawn);
		if (!TestNotNull(TEXT("Possessed light-test pawn spawns"), Player[Index])) { return false; }
		Player[Index]->GetCapsuleComponent()->SetCapsuleSize(34.0f, 90.0f);
		Player[Index]->SetReplicates(true); Player[Index]->SetReplicateMovement(true); PC[Index]->Possess(Player[Index]);
	}
	UClass* GhostClass = LoadClass<ACharacter>(nullptr, TEXT("/Game/AI/StoreGhost/BP_StoreGhost.BP_StoreGhost_C"));
	if (!TestNotNull(TEXT("Actual ghost Blueprint loads"), GhostClass)) { return false; }
	const FTransform Home(FRotator::ZeroRotator, FVector(5050, 100, 92.15));
	ACharacter* Ghost = Server->SpawnActorDeferred<ACharacter>(GhostClass, Home, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!TestNotNull(TEXT("Actual ghost Blueprint spawns"), Ghost)) { return false; }
	Ghost->Tags.AddUnique(TEXT("StoreGhostPrototype")); Ghost->SetReplicates(true); Ghost->SetReplicateMovement(true);
	const bool bConfigured = SetNumber(Ghost, TEXT("StoreMinX"), 3950) && SetNumber(Ghost, TEXT("StoreMaxX"), 5950)
		&& SetNumber(Ghost, TEXT("StoreMinY"), -816.6666667) && SetNumber(Ghost, TEXT("StoreMaxY"), 816.6666667)
		&& SetNumber(Ghost, TEXT("StoreFloorZ"), 0) && SetNumber(Ghost, TEXT("CooldownUntil"), 0);
	UGameplayStatics::FinishSpawningActor(Ghost, Home); Ghost->SetActorTransform(Home); Ghost->SetActorTickEnabled(false);
	if (!TestTrue(TEXT("Fixture has authored store bounds"), bConfigured)) { return false; }
	UMallGhostInteractionComponent* State = nullptr;
	TArray<UMallGhostLightComponent*> Local;
	if (!TestTrue(TEXT("Voice-independent lighting component reaches both peers"), Worlds.TickUntil([&]()
	{
		State = Ghost->FindComponentByClass<UMallGhostInteractionComponent>();
		Local.Reset(); Local.Add(Ghost->FindComponentByClass<UMallGhostLightComponent>());
		for (UE::Net::FTestWorldInstance& Client : Worlds.Clients)
		{
			ACharacter* PeerGhost = FindGhost(Client.GetWorld(), GhostClass);
			Local.Add(PeerGhost ? PeerGhost->FindComponentByClass<UMallGhostLightComponent>() : nullptr);
		}
		return State && !Local.Contains(nullptr);
	}, 180))) { return false; }
	State->bAutomaticHunts = State->bAutomaticDisturbances = false;
	if (UMallGhostSoundComponent* Sound = Ghost->FindComponentByClass<UMallGhostSoundComponent>()) { Sound->bAutomaticSounds = false; }
	UMallGhostLightComponent* Lighting = Local[0];
	const FName RoomTag(TEXT("MallLightNetworkFixtureRoom"));
	const FVector RoomLocation(4950, 0, 600), RoomSecondLocation(5100, -300, 600), NeighborLocation(6500, 0, 600), UpstairsLocation(4950, 0, 1450);
	auto SpawnLight = [&](FVector Location, FName Tag, EComponentMobility::Type Mobility)
	{
		APointLight* Light = Server->SpawnActorDeferred<APointLight>(APointLight::StaticClass(),
			FTransform(FRotator::ZeroRotator, Location), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (Light)
		{
			Light->GetLightComponent()->SetMobility(Mobility); Light->Tags.AddUnique(Tag);
			Light->SetReplicates(true); Light->SetReplicateMovement(true);
			UGameplayStatics::FinishSpawningActor(Light, FTransform(FRotator::ZeroRotator, Location)); Light->ForceNetUpdate();
		}
		return Light;
	};
	APointLight* Room = SpawnLight(RoomLocation, RoomTag, EComponentMobility::Stationary);
	APointLight* RoomSecond = SpawnLight(RoomSecondLocation, RoomTag, EComponentMobility::Stationary);
	APointLight* Neighbor = SpawnLight(NeighborLocation, TEXT("DifferentStoreLightTag"), EComponentMobility::Stationary);
	APointLight* Upstairs = SpawnLight(UpstairsLocation, TEXT("UpstairsStoreLightTag"), EComponentMobility::Stationary);
	APointLight* Static = SpawnLight(FVector(5000, 0, 600), RoomTag, EComponentMobility::Static);
	APointLight* Signal = SpawnLight(FVector(5050, 100, 700), RoomTag, EComponentMobility::Movable);
	if (!TestNotNull(TEXT("Room light spawns"), Room) || !TestNotNull(TEXT("Second room light spawns"), RoomSecond)
		|| !TestNotNull(TEXT("Neighbor light spawns"), Neighbor)
		|| !TestNotNull(TEXT("Upstairs light spawns"), Upstairs) || !TestNotNull(TEXT("Static light spawns"), Static)
		|| !TestNotNull(TEXT("Ghost signal light spawns"), Signal)) { return false; }
	Signal->Tags.AddUnique(TEXT("MallGhostSignal")); Signal->GetLightComponent()->ComponentTags.Add(TEXT("MallGhostSignal"));
	Signal->SetOwner(Ghost); Signal->AttachToActor(Ghost, FAttachmentTransformRules::KeepWorldTransform);
	UPointLightComponent* Flashlight = NewObject<UPointLightComponent>(Player[0], NAME_None, RF_Transient);
	Flashlight->SetupAttachment(Player[0]->GetRootComponent()); Flashlight->SetMobility(EComponentMobility::Movable);
	Flashlight->SetIntensity(123.25f); Flashlight->RegisterComponent();
	const float RoomBase = Room->GetLightComponent()->Intensity, NeighborBase = Neighbor->GetLightComponent()->Intensity;
	const float UpstairsBase = Upstairs->GetLightComponent()->Intensity, StaticBase = Static->GetLightComponent()->Intensity;
	const float SignalBase = Signal->GetLightComponent()->Intensity;
	TArray<APointLight*> LocalRoom, LocalRoomSecond;
	TArray<float> LocalBase, LocalBaseSecond;
	if (!TestTrue(TEXT("Replicated room lights arrive before any flicker"), Worlds.TickUntil([&]()
	{
		LocalRoom.Reset(); LocalRoom.Add(Room); LocalRoomSecond.Reset(); LocalRoomSecond.Add(RoomSecond);
		for (UE::Net::FTestWorldInstance& Client : Worlds.Clients)
		{
			LocalRoom.Add(FindLight(Client.GetWorld(), RoomLocation));
			LocalRoomSecond.Add(FindLight(Client.GetWorld(), RoomSecondLocation));
		}
		return !LocalRoom.Contains(nullptr) && !LocalRoomSecond.Contains(nullptr);
	}, 180))) { return false; }
	for (APointLight* Light : LocalRoom) { LocalBase.Add(Light->GetLightComponent()->Intensity); }
	// Mirror two different authored map brightness values before this transient fixture starts its effect.
	for (APointLight* Light : LocalRoomSecond)
	{
		Light->GetLightComponent()->SetIntensity(2512.75f); LocalBaseSecond.Add(Light->GetLightComponent()->Intensity);
	}
	Lighting->WarningMinimumBrightness = 0.81f; Lighting->ChaseMinimumBrightness = 0.35f; Lighting->FlickerRate = 1.7f;
	auto Refresh = [&]() { Lighting->NextTargetRefreshTime = 0.0; Lighting->UpdateAuthoritativeState(); };
	auto WaitMode = [&](EMallGhostLightMode Mode, int32 Count)
	{
		return Worlds.TickUntil([&]()
		{
			for (UMallGhostLightComponent* Component : Local)
			{
				if (Component->GetFlickerMode() != Mode || Component->GetTargetLightCount() != Count) { return false; }
			}
			return true;
		}, 120);
	};
	auto CheckRestored = [&](const TCHAR* Label)
	{
		bool bRestored = true;
		for (int32 Index = 0; Index < LocalRoom.Num(); ++Index)
		{
			bRestored &= LocalRoom[Index]->GetLightComponent()->Intensity == LocalBase[Index]
				&& LocalRoomSecond[Index]->GetLightComponent()->Intensity == LocalBaseSecond[Index];
		}
		return TestTrue(Label, bRestored);
	};
	auto CheckExcluded = [&]()
	{
		TestTrue(TEXT("Neighboring and upstairs store lights retain exact original intensity"),
			Neighbor->GetLightComponent()->Intensity == NeighborBase && Upstairs->GetLightComponent()->Intensity == UpstairsBase);
		TestTrue(TEXT("Static light and ghost signal retain exact original intensity"),
			Static->GetLightComponent()->Intensity == StaticBase && Signal->GetLightComponent()->Intensity == SignalBase);
		TestEqual(TEXT("Player flashlight component remains untouched"), Flashlight->Intensity, 123.25f);
	};
	// No tag is the production fail-closed default; a guessed room is never selected.
	Lighting->TargetLightTag = NAME_None; State->SetGhostMode(EMallGhostMode::VisibleWarning); Refresh();
	if (!TestTrue(TEXT("Unbound room fails closed on every peer"), WaitMode(EMallGhostLightMode::Off, 0))) { return false; }
	CheckRestored(TEXT("No tag leaves the room's authored brightness intact"));
	Lighting->TargetLightTag = RoomTag; Refresh();
	if (!TestTrue(TEXT("Harmless warning flickers only the explicitly tagged dynamic room lamp"), WaitMode(EMallGhostLightMode::Warning, 2))) { return false; }
	for (int32 Index = 1; Index < Local.Num(); ++Index)
	{
		TestTrue(TEXT("Peers receive exact server-selected light references without replicated actor tags"),
			Local[Index]->FlickerState.Targets.Num() == 2 && Local[Index]->FlickerState.Targets.Contains(LocalRoom[Index])
			&& Local[Index]->FlickerState.Targets.Contains(LocalRoomSecond[Index])
			&& !LocalRoom[Index]->ActorHasTag(RoomTag));
		TestTrue(TEXT("Peers use one shared seed and server epoch"),
			Local[Index]->FlickerState.Seed == Lighting->FlickerState.Seed
			&& Local[Index]->FlickerState.EpochServerTime == Lighting->FlickerState.EpochServerTime
			&& Local[Index]->FlickerState.MinimumBrightness == 0.81f && Local[Index]->FlickerState.Rate == 1.7f);
	}
	float PreviousSample = -1.0f; bool bPatternVaries = false;
	for (int32 Index = 0; Index < 48; ++Index)
	{
		const double Elapsed = Index / 12.0;
		const float Sample = UMallGhostLightComponent::EvaluateMultiplier(Lighting->FlickerState, Elapsed);
		TestTrue(TEXT("Shared pattern evaluation is finite and respects the configured floor"), FMath::IsFinite(Sample) && Sample >= 0.81f && Sample <= 1.0f);
		TestEqual(TEXT("The same seed and elapsed time always produce the same brightness"), Sample,
			UMallGhostLightComponent::EvaluateMultiplier(Lighting->FlickerState, Elapsed));
		bPatternVaries |= PreviousSample >= 0.0f && !FMath::IsNearlyEqual(PreviousSample, Sample, 0.0001f); PreviousSample = Sample;
	}
	TestTrue(TEXT("The common deterministic pattern changes over time"), bPatternVaries);
	TArray<bool> bVaried; bVaried.Init(false, Local.Num());
	for (int32 Frame = 0; Frame < 75; ++Frame)
	{
		Worlds.TickAll(1);
		for (int32 Index = 0; Index < Local.Num(); ++Index)
		{
			const float Intensity = LocalRoom[Index]->GetLightComponent()->Intensity;
			// NetTestHelpers runs its authority world as dedicated even though its URL says listen.
			// That world evaluates diagnostics without changing render-only lamp properties.
			const bool bDedicated = Local[Index]->GetNetMode() == NM_DedicatedServer;
			const float SampleMultiplier = bDedicated ? Local[Index]->AppliedIntensityMultiplier : Intensity / LocalBase[Index];
			bVaried[Index] = bVaried[Index] || !FMath::IsNearlyEqual(SampleMultiplier, 1.0f, 0.0001f);
			if (bDedicated)
			{
				TestTrue(TEXT("Dedicated authority evaluates a bounded brightness multiplier without mutating lights"),
					FMath::IsFinite(SampleMultiplier) && SampleMultiplier >= 0.81f && SampleMultiplier <= 1.0f
					&& Intensity == LocalBase[Index]);
			}
			if (!TestTrue(TEXT("Flicker intensity stays finite and bounded"), FMath::IsFinite(Intensity) && Intensity >= 0.0f && Intensity <= LocalBase[Index])) { return false; }
			TestTrue(TEXT("Differently bright lamps receive the same factor while retaining their own baselines"),
				FMath::IsNearlyEqual(Intensity / LocalBase[Index], LocalRoomSecond[Index]->GetLightComponent()->Intensity / LocalBaseSecond[Index], 0.001f));
		}
	}
	for (int32 Index = 0; Index < bVaried.Num(); ++Index)
	{
		TestTrue(FString::Printf(TEXT("Warning evaluates a changing factor or varies rendered light intensity on local %d"), Index), bVaried[Index]);
	}
	CheckExcluded();

	State->SetGhostMode(EMallGhostMode::VisibleHunt); SetChase(Ghost, TEXT("Idle"), Player[0]); Refresh();
	// Deliberately poison unreplicated Blueprint text on peers. Only server state may drive lighting.
	for (int32 Index = 1; Index < Local.Num(); ++Index)
	{
		ACharacter* PeerGhost = FindGhost(Worlds.Clients[Index - 1].GetWorld(), GhostClass);
		SetChase(PeerGhost, TEXT("Chasing"), Worlds.Clients[Index - 1].GetWorld()->GetFirstPlayerController()->GetPawn());
	}
	if (!TestTrue(TEXT("Visible idle mode is not a chase, even with stale client Blueprint text"), WaitMode(EMallGhostLightMode::Off, 0))) { return false; }
	CheckRestored(TEXT("Ending warning restores exact brightness on every peer"));
	SetChase(Ghost, TEXT("Chasing"), Player[0]); Refresh();
	if (!TestTrue(TEXT("Actual confined chase enables the stronger flicker mode"), WaitMode(EMallGhostLightMode::Chasing, 2))) { return false; }
	Player[0]->SetActorLocation(FVector(3600, 0, 90)); Player[0]->ForceNetUpdate(); Refresh();
	if (!TestTrue(TEXT("A chase target leaving the store stops flicker"), WaitMode(EMallGhostLightMode::Off, 0))) { return false; }
	CheckRestored(TEXT("Target exit restores every room light exactly"));
	Player[0]->SetActorLocation(FVector(4800, 0, 90)); Player[0]->ForceNetUpdate(); Refresh();
	if (!TestTrue(TEXT("An eligible resumed chase flickers again"), WaitMode(EMallGhostLightMode::Chasing, 2))) { return false; }
	SetNumber(Ghost, TEXT("CooldownUntil"), Server->GetTimeSeconds() + 10.0); Refresh();
	if (!TestTrue(TEXT("Blueprint catch or voice recovery suppresses stale Chasing"), WaitMode(EMallGhostLightMode::Off, 0))) { return false; }
	CheckRestored(TEXT("Existing movement recovery restores original room brightness"));
	SetNumber(Ghost, TEXT("CooldownUntil"), 0);
	State->bAutomaticHunts = true; State->SetGhostMode(EMallGhostMode::InvisibleDisturbance); Refresh();
	if (!TestTrue(TEXT("Invisible inter-hunt cooldown keeps the room normal"), WaitMode(EMallGhostLightMode::Off, 0))) { return false; }
	TestEqual(TEXT("The light test exercised a real automatic cooldown phase"), State->CyclePhase, EMallGhostCyclePhase::Cooldown);
	CheckRestored(TEXT("Hunt cooldown restores authored brightness on all peers"));

	State->bAutomaticHunts = false; State->SetGhostMode(EMallGhostMode::VisibleHunt); SetChase(Ghost, TEXT("Chasing"), Player[0]); Refresh();
	if (!TestTrue(TEXT("Chase restarts before late connection"), WaitMode(EMallGhostLightMode::Chasing, 2))) { return false; }
	// The engine helper gives initial replication only 60 ticks. Keep that same connection
	// and allow a bounded extra budget for this fixture's light actors and reference payload.
	const bool bLateConnectedImmediately = Worlds.CreateAndConnectClient();
	const bool bLateConnected = bLateConnectedImmediately || Worlds.TickUntil([&]()
	{
		UWorld* LateWorld = Worlds.Clients.IsValidIndex(2) ? Worlds.Clients[2].GetWorld() : nullptr;
		APlayerController* LatePC = LateWorld ? LateWorld->GetFirstPlayerController() : nullptr;
		return IsValid(LatePC) && LatePC->GetLocalRole() == ROLE_AutonomousProxy;
	}, 240);
	if (!TestTrue(TEXT("Late peer acquires an autonomous controller during active flicker"), bLateConnected)) { return false; }
	UMallGhostLightComponent* Late = nullptr; APointLight* LateRoom = nullptr; APointLight* LateRoomSecond = nullptr;
	if (!TestTrue(TEXT("Late join receives the active mode, seed, original epoch and lamp reference"), Worlds.TickUntil([&]()
	{
		ACharacter* LateGhost = FindGhost(Worlds.Clients[2].GetWorld(), GhostClass);
		Late = LateGhost ? LateGhost->FindComponentByClass<UMallGhostLightComponent>() : nullptr;
		LateRoom = FindLight(Worlds.Clients[2].GetWorld(), RoomLocation);
		LateRoomSecond = FindLight(Worlds.Clients[2].GetWorld(), RoomSecondLocation);
		return Late && LateRoom && LateRoomSecond && Late->GetFlickerMode() == EMallGhostLightMode::Chasing && Late->GetTargetLightCount() == 2
			&& Late->FlickerState.Targets.Contains(LateRoom) && Late->FlickerState.Targets.Contains(LateRoomSecond) && Late->FlickerState.Seed == Lighting->FlickerState.Seed
			&& Late->FlickerState.EpochServerTime == Lighting->FlickerState.EpochServerTime;
	}, 180))) { return false; }
	Local.Add(Late); LocalRoom.Add(LateRoom); LocalRoomSecond.Add(LateRoomSecond);
	// Late-spawned engine fixture lights use the class defaults rather than the initial peers\' authored overrides.
	LocalBase.Add(RoomBase); LocalBaseSecond.Add(RoomBase);
	Lighting->bEnableFlicker = false; Refresh();
	if (!TestTrue(TEXT("Disabling flicker publishes Off to current and late peers"), WaitMode(EMallGhostLightMode::Off, 0))) { return false; }
	CheckRestored(TEXT("Disabling restores all four local original intensities"));
	Lighting->bEnableFlicker = true; State->SetGhostMode(EMallGhostMode::VisibleWarning); Refresh();
	if (!TestTrue(TEXT("Fixture can re-enable warning before lifecycle cleanup"), WaitMode(EMallGhostLightMode::Warning, 2))) { return false; }
	Lighting->UnregisterComponent();
	TestEqual(TEXT("Unregister restores the authoritative lamp immediately"), Room->GetLightComponent()->Intensity, RoomBase);
	TestEqual(TEXT("Unregister restores the differently bright second lamp exactly"), RoomSecond->GetLightComponent()->Intensity, LocalBaseSecond[0]);
	Lighting->RegisterComponent(); Refresh(); Worlds.TickAll(20);
	Ghost->Destroy();
	if (!TestTrue(TEXT("Ghost destruction restores each client's cached lamp intensity"), Worlds.TickUntil([&]()
	{
		for (int32 Index = 0; Index < LocalRoom.Num(); ++Index)
		{
			if (LocalRoom[Index]->GetLightComponent()->Intensity != LocalBase[Index]
				|| LocalRoomSecond[Index]->GetLightComponent()->Intensity != LocalBaseSecond[Index]) { return false; }
		}
		return true;
	}, 120))) { return false; }
	CheckExcluded();
	return true;
}

#endif
