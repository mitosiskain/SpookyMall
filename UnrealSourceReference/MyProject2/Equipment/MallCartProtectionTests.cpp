#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Equipment/MallCartCharger.h"
#include "Equipment/MallCartProtectionSubsystem.h"
#include "Equipment/MallLightCart.h"
#include "Equipment/MallPushCart.h"
#include "Equipment/MallStoreCartDock.h"
#include "Ghost/MallGhostInteractionComponent.h"
#include "Ghost/MallGhostCartHelpers.h"
#include "Ghost/MallGhostSoundComponent.h"
#include "Voice/MallVoiceSettings.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
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

namespace MallCartPowerTest
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

	template<class T> T* First(UWorld* World)
	{
		for (TActorIterator<T> It(World); It; ++It) { return *It; }
		return nullptr;
	}

	void FreezeCartTestActors(UWorld* World)
	{
		for (TActorIterator<ACharacter> It(World); It; ++It)
		{
			It->SetActorTickEnabled(false);
			It->GetCharacterMovement()->SetComponentTickEnabled(false);
			It->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		}
	}

	bool SetNumber(AActor* Actor, FName Name, double Value)
	{
		FNumericProperty* Property = Actor ? FindFProperty<FNumericProperty>(Actor->GetClass(), Name) : nullptr;
		if (!Property || !Property->IsFloatingPoint()) { return false; }
		Property->SetFloatingPointPropertyValue(Property->ContainerPtrToValuePtr<void>(Actor), Value);
		return true;
	}

	AActor* Blocker(UWorld* World, FVector Centre, FVector Extent)
	{
		AActor* Actor = World->SpawnActor<AActor>();
		if (!Actor) { return nullptr; }
		UBoxComponent* Box = NewObject<UBoxComponent>(Actor);
		Box->SetMobility(EComponentMobility::Static);
		Box->InitBoxExtent(Extent);
		Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Box->SetCollisionObjectType(ECC_WorldStatic);
		Box->SetCollisionResponseToAllChannels(ECR_Block);
		Actor->SetRootComponent(Box); Actor->AddInstanceComponent(Box);
		Box->SetWorldLocation(Centre); Box->RegisterComponent();
		return Actor;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMallCartConeGeometryTest, "Mall.Cart.ConeGeometry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMallCartConeGeometryTest::RunTest(const FString& Parameters)
{
	const FVector Apex(0.0, 0.0, 365.0), Down(0.0, 0.0, -1.0), Player(110.0, 0.0, 90.0);
	TestTrue(TEXT("A player alongside the cart fits the complete illuminated cone"),
		AMallLightCart::ConeContainsCapsule(Apex, Down, 650.0f, 55.0f, Player, 34.0f, 90.0f));
	TestTrue(TEXT("A ghost touching the lit volume is excluded before its centre enters"),
		AMallLightCart::ConeIntersectsCapsule(Apex, Down, 650.0f, 55.0f, FVector(400, 0, 90), 42.0f, 96.0f));
	TestFalse(TEXT("A player partly outside the cone receives no full-body safety"),
		AMallLightCart::ConeContainsCapsule(Apex, Down, 650.0f, 55.0f, FVector(400, 0, 90), 34.0f, 90.0f));
	TestFalse(TEXT("A distant capsule outside the cone is not protected"),
		AMallLightCart::ConeIntersectsCapsule(Apex, Down, 650.0f, 55.0f, FVector(1200, 0, 90), 42.0f, 96.0f));
	TestFalse(TEXT("Upstairs actors above the downward lamp never receive protection"),
		AMallLightCart::ConeIntersectsCapsule(Apex, Down, 650.0f, 55.0f, FVector(0, 0, 940), 42.0f, 96.0f));
	TestFalse(TEXT("Protection is clipped to the spotlight's spherical attenuation distance"),
		AMallLightCart::ConeIntersectsCapsule(Apex, Down, 650.0f, 55.0f, FVector(500, 0, -250), 20.0f, 20.0f));
	TestFalse(TEXT("A capsule behind the lamp cannot intersect the forward cone"),
		AMallLightCart::ConeIntersectsCapsule(Apex, Down, 650.0f, 55.0f, Apex + FVector(0, 0, 100), 20.0f, 40.0f));
	TestFalse(TEXT("Invalid dimensions fail closed"),
		AMallLightCart::ConeContainsCapsule(Apex, Down, 650.0f, 55.0f, Player, 90.0f, 34.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMallCartProtectionNetworkTest, "Mall.Cart.ProtectionAndBatteryNetwork",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMallCartProtectionNetworkTest::RunTest(const FString& Parameters)
{
	using namespace MallCartPowerTest;
	FRestoreSettings Restore;
	UE::Net::FTestWorlds Worlds(TEXT("/Engine/Maps/Entry"), TEXT("/Script/Engine.GameMode"));
	if (!TestTrue(TEXT("Cart authority world loads"), Worlds.Server.IsLoaded())
		|| !TestTrue(TEXT("First real network peer connects"), Worlds.CreateAndConnectClient())
		|| !TestTrue(TEXT("Second real network peer connects"), Worlds.CreateAndConnectClient())) { return false; }
	Worlds.PreTickAllDelegate.AddLambda([&]()
	{
		FreezeCartTestActors(Worlds.Server.GetWorld());
		for (UE::Net::FTestWorldInstance& Client : Worlds.Clients) { FreezeCartTestActors(Client.GetWorld()); }
	});
	UWorld* Server = Worlds.Server.GetWorld();
	APlayerController* PC[2] = {};
	if (!TestTrue(TEXT("Connected authoritative controllers are present"), Worlds.TickUntil([&]()
	{
		PC[0] = Worlds.GetServerPlayerControllerOfClient(0); PC[1] = Worlds.GetServerPlayerControllerOfClient(1);
		return PC[0] && PC[1];
	}, 120))) { return false; }

	FActorSpawnParameters Spawn;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ACharacter* Players[2] = {};
	for (int32 Index = 0; Index < 2; ++Index)
	{
		Players[Index] = Server->SpawnActor<ACharacter>(FVector(Index == 0 ? -500 : 1200, Index == 0 ? -110 : 0, 90), FRotator::ZeroRotator, Spawn);
		if (!TestNotNull(TEXT("Cart test player spawns"), Players[Index])) { return false; }
		Players[Index]->GetCapsuleComponent()->SetCapsuleSize(34.0f, 90.0f);
		Players[Index]->SetReplicates(true); Players[Index]->SetReplicateMovement(true); PC[Index]->Possess(Players[Index]);
	}
	if (!TestNotNull(TEXT("Ground support fixture exists"), Blocker(Server, FVector(0, 0, -50), FVector(5000, 5000, 50)))) { return false; }
	AMallPushCart* Equipment = Server->SpawnActor<AMallPushCart>(FVector(-500, 0, 45), FRotator(0, 90, 0), Spawn);
	AMallLightCart* Light = Server->SpawnActor<AMallLightCart>(FVector(1000, 0, 45), FRotator::ZeroRotator, Spawn);
	AMallStoreCartDock* Dock = Server->SpawnActor<AMallStoreCartDock>(FVector(-500, 0, 0), FRotator(0, 90, 0), Spawn);
	AMallCartCharger* Charger = Server->SpawnActor<AMallCartCharger>(FVector(1000, -180, 100), FRotator::ZeroRotator, Spawn);
	if (!TestNotNull(TEXT("Equipment cart exists"), Equipment) || !TestNotNull(TEXT("Light cart exists"), Light)
		|| !TestNotNull(TEXT("Store dock exists"), Dock) || !TestNotNull(TEXT("Wall outlet exists"), Charger)) { return false; }
	Equipment->SetFreePhysicsEnabled(false);
	Light->SetFreePhysicsEnabled(false);
	Light->SetActorTickEnabled(false);
	Dock->StoreTag = TEXT("CartProtectionFixtureStore"); Dock->AssignedCart = Equipment;
	UClass* GhostClass = LoadClass<ACharacter>(nullptr, TEXT("/Game/AI/StoreGhost/BP_StoreGhost.BP_StoreGhost_C"));
	if (!TestNotNull(TEXT("Actual ghost Blueprint loads"), GhostClass)) { return false; }
	const FTransform Home(FRotator::ZeroRotator, FVector(300, 0, 92.15));
	ACharacter* Ghost = Server->SpawnActorDeferred<ACharacter>(GhostClass, Home, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!TestNotNull(TEXT("Store ghost fixture spawns"), Ghost)) { return false; }
	Ghost->Tags.AddUnique(TEXT("StoreGhostPrototype")); Ghost->Tags.AddUnique(TEXT("MallCartBoundary")); Ghost->Tags.AddUnique(Dock->StoreTag);
	Ghost->SetReplicates(true);
	const bool bBounds = SetNumber(Ghost, TEXT("StoreMinX"), -500) && SetNumber(Ghost, TEXT("StoreMaxX"), 500)
		&& SetNumber(Ghost, TEXT("StoreMinY"), -500) && SetNumber(Ghost, TEXT("StoreMaxY"), 500) && SetNumber(Ghost, TEXT("StoreFloorZ"), 0);
	UGameplayStatics::FinishSpawningActor(Ghost, Home); Ghost->SetActorTransform(Home); Ghost->SetActorTickEnabled(false);
	if (!TestTrue(TEXT("Ghost has the authored store bounds"), bBounds)) { return false; }
	UMallCartProtectionSubsystem* Protection = Server->GetSubsystem<UMallCartProtectionSubsystem>();
	if (!TestNotNull(TEXT("Independent protection subsystem exists"), Protection)) { return false; }
	Worlds.TickAll(2);
	UMallGhostInteractionComponent* State = Ghost->FindComponentByClass<UMallGhostInteractionComponent>();
	if (!TestNotNull(TEXT("Actual ghost interaction component exists"), State)) { return false; }
	State->bAutomaticHunts = State->bAutomaticDisturbances = false;
	TestEqual(TEXT("Opt-in ghost capsule cannot push physical carts"),
		Ghost->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_GameTraceChannel1), ECR_Ignore);
	TestEqual(TEXT("Ordinary players still block the physical cart channel"),
		Players[0]->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_GameTraceChannel1), ECR_Block);
	ACharacter* Legacy = Server->SpawnActor<ACharacter>(FVector(-2000, 1000, 90), FRotator::ZeroRotator, Spawn);
	if (!TestNotNull(TEXT("Legacy collision fixture exists"), Legacy)) { return false; }
	UMallGhostInteractionComponent* LegacyState = NewObject<UMallGhostInteractionComponent>(Legacy);
	Legacy->AddInstanceComponent(LegacyState); LegacyState->RegisterComponent();
	LegacyState->bAutomaticHunts = LegacyState->bAutomaticDisturbances = false;
	TestEqual(TEXT("Unmodified characters retain their authored cart-channel response"),
		Legacy->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_GameTraceChannel1), ECR_Block);
	if (UMallGhostSoundComponent* Sound = Ghost->FindComponentByClass<UMallGhostSoundComponent>()) { Sound->bAutomaticSounds = false; }

	TestTrue(TEXT("Correctly parked equipment cart seals its assigned store"), Dock->IsSealed() && Protection->IsStoreSealed(Ghost));
	TestTrue(TEXT("Sealed store permits a valid home position"), Protection->IsGhostPositionAllowed(Ghost, FVector(300, 0, 92.15)));
	TestFalse(TEXT("Sealed store refuses a ghost capsule outside its bounds"), Protection->IsGhostPositionAllowed(Ghost, FVector(-1000, 0, 92.15)));
	Equipment->SetActorRotation(FRotator::ZeroRotator);
	TestFalse(TEXT("Wrong cart orientation does not seal the entrance"), Dock->IsSealed());
	Equipment->SetActorRotation(FRotator(0, 90, 0)); Equipment->SetActorLocation(FVector(-550, 0, 45));
	TestFalse(TEXT("Cart near the doorway but clear of its threshold does not seal"), Dock->IsSealed());
	Equipment->SetActorLocation(FVector(-720, 0, 45));
	TestFalse(TEXT("Cart parked too far from the marked dock does not seal"), Dock->IsSealed());
	TestTrue(TEXT("Unsealed ghost can roam outside the shop on its current floor"), Protection->IsGhostPositionAllowed(Ghost, FVector(-1000, 0, 92.15)));
	TestFalse(TEXT("Unsealed ghost cannot roam onto another floor"), Protection->IsGhostPositionAllowed(Ghost, FVector(-1000, 0, 940)));
	TestFalse(TEXT("Unsealed prototype roaming stays inside its configured limit"), Protection->IsGhostPositionAllowed(Ghost, FVector(-15000, 0, 92.15)));
	TestTrue(TEXT("Unsealed hunt clock retains players inside a light refuge"),
		Protection->CanGhostMaintainHuntForPlayer(Ghost, Players[1]));
	TestFalse(TEXT("Initial grace and warning still require the player to be in the home store"),
		Protection->CanGhostMaintainHuntForPlayer(Ghost, Players[1], true));
	TestTrue(TEXT("Unsuppressed ghost can enter a manual hunt"), State->SetGhostMode(EMallGhostMode::VisibleHunt));
	const FVector PreviousGhostLocation = Ghost->GetActorLocation();
	UCharacterMovementComponent* CrouchMovement = Players[1]->GetCharacterMovement();
	const bool bPreviouslyCanCrouch = CrouchMovement->GetNavAgentPropertiesRef().bCanCrouch;
	const float PreviousCrouchHeight = CrouchMovement->GetCrouchedHalfHeight();
	CrouchMovement->GetNavAgentPropertiesRef().bCanCrouch = true;
	CrouchMovement->SetCrouchedHalfHeight(44.0f);
	Players[1]->SetActorLocation(FVector(-1000, 0, 90));
	Players[1]->Crouch();
	CrouchMovement->Crouch(false);
	TestTrue(TEXT("Regression fixture enters an actual low crouch"),
		Players[1]->IsCrouched() && FMath::IsNearlyEqual(Players[1]->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(), 44.0f));
	Players[1]->SetActorLocation(FVector(-1000, 0, 44));
	Ghost->SetActorLocation(FVector(-940, 0, 92.15));
	TestFalse(TEXT("Crouching in an unlit hallway does not grant light safety"), Protection->IsPlayerSafe(Players[1]));
	TestTrue(TEXT("Crouched player remains a valid target outside the safety cone"), Protection->CanGhostPursueTarget(Ghost, Players[1]));
	TestTrue(TEXT("Crouched player outside the light remains eligible for a ghost catch"),
		UMallGhostCartHelpers::CanGhostCatchPlayer(Ghost, Players[1]));
	Players[1]->SetActorLocation(FVector(1200, 0, 44));
	Ghost->SetActorLocation(PreviousGhostLocation);
	TestTrue(TEXT("Crouched player within the powered cone retains full-body safety"), Protection->IsPlayerSafe(Players[1]));
	TestFalse(TEXT("Crouched player inside the light cannot become a chase target"), Protection->CanGhostPursueTarget(Ghost, Players[1]));
	TestFalse(TEXT("Crouched player inside the light cannot be caught"),
		UMallGhostCartHelpers::CanGhostCatchPlayer(Ghost, Players[1]));
	Players[1]->UnCrouch();
	CrouchMovement->UnCrouch(false);
	CrouchMovement->GetNavAgentPropertiesRef().bCanCrouch = bPreviouslyCanCrouch;
	CrouchMovement->SetCrouchedHalfHeight(PreviousCrouchHeight);
	Players[1]->GetCapsuleComponent()->SetCapsuleSize(34.0f, 90.0f);
	Players[1]->SetActorLocation(FVector(1200, 0, 90));
	if (FObjectPropertyBase* Target = FindFProperty<FObjectPropertyBase>(Ghost->GetClass(), TEXT("TargetPlayer")))
	{
		Target->SetObjectPropertyValue_InContainer(Ghost, Players[1]);
	}
	if (FBoolProperty* Eligible = FindFProperty<FBoolProperty>(Ghost->GetClass(), TEXT("bPlayerEligible")))
	{
		Eligible->SetPropertyValue_InContainer(Ghost, true);
	}
	TestFalse(TEXT("A stale Blueprint target cannot authorize catching a protected player"),
		UMallGhostCartHelpers::CanGhostCatchPlayer(Ghost, Players[1]));
	TestFalse(TEXT("A route through a safety cone fails even with safe endpoints"),
		Protection->IsGhostSegmentAllowed(Ghost, FVector(300, 0, 92.15), FVector(1700, 0, 92.15)));
	TestTrue(TEXT("A clear unsealed segment on the same floor is permitted"),
		Protection->IsGhostSegmentAllowed(Ghost, FVector(-1000, -1000, 92.15), FVector(-1000, 1000, 92.15)));
	Equipment->SetActorLocation(FVector(0, 0, 45));
	Players[0]->SetActorLocation(FVector(100, 200, 90));
	TestTrue(TEXT("Full equipment cart inside the store makes investigation dormant"), Protection->IsStoreSuppressedByCart(Ghost));
	TestFalse(TEXT("Suppression immediately rejects hunt permission"), UMallGhostInteractionComponent::CanGhostHunt(Ghost));
	TestFalse(TEXT("Manual hunt requests cannot bypass a cart inside the store"), State->SetGhostMode(EMallGhostMode::VisibleHunt));
	TestFalse(TEXT("Dormant store cannot maintain a ghost hunt"), Protection->CanGhostMaintainHuntForPlayer(Ghost, Players[0], true));
	TestFalse(TEXT("Dormant store refuses all ghost movement"), Protection->IsGhostPositionAllowed(Ghost, FVector(300, 0, 92.15)));
	Equipment->SetActorLocation(FVector(-500, 0, 45));
	Players[0]->SetActorLocation(FVector(-500, -110, 90)); FreezeCartTestActors(Server);
	TestFalse(TEXT("Cart crossing the threshold is not fully inside the store"), Protection->IsStoreSuppressedByCart(Ghost));
	TestTrue(TEXT("Player at handle can start pushing the equipment cart"), Equipment->TryBeginPush(PC[0]));
	TestFalse(TEXT("Being pushed immediately unseals the store, even at the dock"), Dock->IsSealed());
	TestTrue(TEXT("Releasing the cart at the marked position seals again"), Equipment->StopPushing(PC[0]) && Dock->IsSealed());

	TestEqual(TEXT("Normal full-battery runtime defaults to fifteen minutes"), Light->RuntimeSeconds, 900.0f);
	TestEqual(TEXT("Normal empty-to-full charging defaults to five minutes"), Light->RechargeSeconds, 300.0f);
	TestTrue(TEXT("Powered light protects a complete player standing alongside"), Protection->IsPlayerSafe(Players[1]));
	TestTrue(TEXT("Protected player cannot become a ghost chase target"), !Protection->CanGhostPursueTarget(Ghost, Players[1]));
	AActor* Wall = Blocker(Server, FVector(1100, 0, 250), FVector(10, 500, 250));
	if (!TestNotNull(TEXT("Opaque wall fixture exists"), Wall)) { return false; }
	TestFalse(TEXT("Light cart safety never passes through a static wall"), Protection->IsPlayerSafe(Players[1]));
	Wall->Destroy(); Worlds.TickAll(1);
	TestTrue(TEXT("Removing the wall restores the actual lit refuge"), Protection->IsPlayerSafe(Players[1]));
	Players[1]->SetActorLocation(FVector(1000, 0, 940));
	TestFalse(TEXT("Light refuge never reaches the floor above"), Protection->IsPlayerSafe(Players[1]));
	Players[1]->SetActorLocation(FVector(1200, 0, 90));

	Light->RuntimeSeconds = 4.0f; Light->RechargeSeconds = 2.0f; Light->BatteryFraction = 1.0f;
	Light->Tick(1.0f);
	TestTrue(TEXT("One quarter of configured runtime consumes one quarter of charge"), FMath::IsNearlyEqual(Light->BatteryFraction, 0.75f));
	Light->Tick(3.0f);
	TestTrue(TEXT("Runtime exhaustion reaches exact zero"), Light->BatteryFraction == 0.0f);
	TestFalse(TEXT("An empty battery provides no ghost protection"), Light->IsSafetyActive() || Protection->IsPlayerSafe(Players[1]));
	AMallCartCharger* Distant = Server->SpawnActor<AMallCartCharger>(FVector(5000, 0, 100), FRotator::ZeroRotator, Spawn);
	TestFalse(TEXT("Cannot charge remotely from an out-of-range outlet"), Light->ConnectToCharger(Distant));
	TestTrue(TEXT("Parked cart can explicitly connect to an in-range wall outlet"), Light->ConnectToCharger(Charger));
	TestFalse(TEXT("Charging has no safety output"), Light->IsSafetyActive());
	Light->Tick(1.0f);
	TestTrue(TEXT("Half of configured recharge duration replenishes half a battery"), FMath::IsNearlyEqual(Light->BatteryFraction, 0.5f));
	Light->DisconnectCharger(); Light->SetPowerRequested(false); Light->Tick(1.0f);
	TestTrue(TEXT("Unplugged powered-off cart retains partial charge"), FMath::IsNearlyEqual(Light->BatteryFraction, 0.5f));
	TestTrue(TEXT("Partial charging can resume"), Light->ConnectToCharger(Charger)); Light->Tick(1.0f);
	TestTrue(TEXT("Charging stops at a full normalized battery"), Light->BatteryFraction == 1.0f);
	Light->SetPowerRequested(true);
	Charger->bPowered = false; Light->Tick(0.1f);
	TestTrue(TEXT("Loss of outlet power disconnects charging"), Light->CurrentCharger == nullptr);
	Charger->bPowered = true; Light->SetPowerRequested(false); Light->BatteryFraction = 0.5f;
	TestTrue(TEXT("Reconnection before moving is valid"), Light->ConnectToCharger(Charger));
	Players[1]->SetActorLocation(FVector(890, 0, 90)); FreezeCartTestActors(Server);
	TestTrue(TEXT("Light-cart handle accepts a pusher"), Light->TryBeginPush(PC[1]));
	TestTrue(TEXT("Starting to move immediately disconnects the charging lead"), Light->CurrentCharger == nullptr);
	TestFalse(TEXT("A moving cart cannot connect to an outlet"), Light->ConnectToCharger(Charger));
	Light->SetPowerRequested(true);
	TestTrue(TEXT("Default safety remains active while the powered cart is pushed"), Light->IsSafetyActive());
	Light->bSafetyWhilePushing = false;
	TestFalse(TEXT("Designer can require the light cart to be parked"), Light->IsSafetyActive());
	Light->bSafetyWhilePushing = true;
	Light->StopPushing(PC[1]);
	Light->SetPowerRequested(false); Light->BatteryFraction = 0.5f; Light->ForceNetUpdate();

	TestTrue(TEXT("Runtime ghost cart exclusion reaches both peers without actor-tag replication"), Worlds.TickUntil([&]()
	{
		for (int32 Index = 0; Index < 2; ++Index)
		{
			ACharacter* PeerGhost = Worlds.FindReplicatedObjectOnClient(Ghost, Index);
			if (!PeerGhost || PeerGhost->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_GameTraceChannel1) != ECR_Ignore) { return false; }
		}
		return true;
	}, 180));
	TArray<AMallLightCart*> PeerLights;
	if (!TestTrue(TEXT("Battery and switched-off state reach both IP peers"), Worlds.TickUntil([&]()
	{
		PeerLights.Reset();
		for (UE::Net::FTestWorldInstance& Client : Worlds.Clients)
		{
			AMallLightCart* Peer = First<AMallLightCart>(Client.GetWorld());
			if (!Peer || Peer->bPowerRequested || !FMath::IsNearlyEqual(Peer->BatteryFraction, 0.5f)) { return false; }
			PeerLights.Add(Peer);
		}
		return PeerLights.Num() == 2;
	}, 180))) { return false; }
	for (AMallLightCart* Peer : PeerLights)
	{
		TestFalse(TEXT("Client cannot authoritatively switch cart power"), Peer->SetPowerRequested(true));
		TestFalse(TEXT("Client cannot start charging from an arbitrary pointer"), Peer->ConnectToCharger(Charger));
		TestTrue(TEXT("Client rejection preserves its replicated power state"), !Peer->bPowerRequested);
		TestFalse(TEXT("Client protection query cannot grant authoritative safety"),
			Peer->GetWorld()->GetSubsystem<UMallCartProtectionSubsystem>()->IsPlayerSafe(Peer->GetWorld()->GetFirstPlayerController()->GetPawn()));
	}
	if (!TestTrue(TEXT("A late peer can join with an existing partially depleted cart"), Worlds.CreateAndConnectClient())) { return false; }
	TestTrue(TEXT("Late peer receives current battery and power, rather than constructor defaults"), Worlds.TickUntil([&]()
	{
		AMallLightCart* Peer = First<AMallLightCart>(Worlds.Clients.Last().GetWorld());
		return Peer && !Peer->bPowerRequested && FMath::IsNearlyEqual(Peer->BatteryFraction, 0.5f);
	}, 180));

	TestTrue(TEXT("Destroyed charger fixture can be connected before removal"), Light->ConnectToCharger(Charger));
	Charger->Destroy(); Light->Tick(0.1f);
	TestTrue(TEXT("Destroyed charger cannot continue replenishing battery"), !Light->CurrentCharger && FMath::IsNearlyEqual(Light->BatteryFraction, 0.5f));
	// The domain assertions above isolate pose changes. Exercise real physics here.
	// Replicated bodies need the same static support in each transient client world.
	for (UE::Net::FTestWorldInstance& Client : Worlds.Clients)
	{
		if (!TestNotNull(TEXT("Client physics floor fixture exists"), Blocker(Client.GetWorld(), FVector(0, 0, -50), FVector(5000, 5000, 50)))) { return false; }
	}
	Equipment->SetFreePhysicsEnabled(true);
	Light->SetFreePhysicsEnabled(true);
	Light->SetActorTickEnabled(true);
	if (!TestTrue(TEXT("Free equipment and light carts settle on the real floor"), Worlds.TickUntil([&]()
	{
		return Equipment->IsParked() && Light->IsParked();
	}, 180))) { return false; }
	UBoxComponent* EquipmentBody = Cast<UBoxComponent>(Equipment->GetRootComponent());
	UBoxComponent* LightBody = Cast<UBoxComponent>(Light->GetRootComponent());
	if (!TestNotNull(TEXT("Equipment has its physical cart body"), EquipmentBody)
		|| !TestNotNull(TEXT("Light cart has its physical body"), LightBody)) { return false; }
	TestTrue(TEXT("Settled equipment cart still seals the doorway"), Dock->IsSealed());
	EquipmentBody->SetPhysicsLinearVelocity(FVector(1, 0, 0));
	EquipmentBody->SetPhysicsAngularVelocityInDegrees(FVector(0, 0, 0.1));
	Equipment->Tick(0.01f);
	TestTrue(TEXT("Small resting physics jitter does not flicker the doorway seal"), Dock->IsSealed());
	EquipmentBody->SetPhysicsLinearVelocity(FVector(40, 0, 0)); Equipment->Tick(0.01f);
	TestFalse(TEXT("Unattended rolling equipment cart cannot seal the doorway"), Dock->IsSealed());
	EquipmentBody->SetPhysicsLinearVelocity(FVector::ZeroVector);
	EquipmentBody->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	Equipment->SetActorLocationAndRotation(FVector(-500, 0, 44.1), FRotator(0, 90, 0), false, nullptr, ETeleportType::TeleportPhysics);
	TestTrue(TEXT("Doorway protection returns after the physical cart comes to rest"), Worlds.TickUntil([&]() { return Dock->IsSealed(); }, 180));

	AMallCartCharger* PhysicsOutlet = Server->SpawnActor<AMallCartCharger>(FVector(1000, -180, 100), FRotator::ZeroRotator, Spawn);
	if (!TestNotNull(TEXT("Physics charging outlet exists"), PhysicsOutlet)) { return false; }
	TestTrue(TEXT("A settled physical light cart can connect"), Light->ConnectToCharger(PhysicsOutlet));
	LightBody->SetPhysicsLinearVelocity(FVector(1, 0, 0));
	LightBody->SetPhysicsAngularVelocityInDegrees(FVector(0, 0, 0.1)); Light->Tick(0.01f);
	TestTrue(TEXT("Small resting jitter retains the charging connection"), Light->CurrentCharger == PhysicsOutlet);
	LightBody->SetPhysicsLinearVelocity(FVector(40, 0, 0)); Light->Tick(0.01f);
	TestTrue(TEXT("A physical bump interrupts charging even before the cable is out of range"), !Light->CurrentCharger);
	TestFalse(TEXT("Rolling unattended light cart cannot connect to power"), Light->ConnectToCharger(PhysicsOutlet));
	LightBody->SetPhysicsLinearVelocity(FVector::ZeroVector);
	LightBody->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	Light->SetActorLocation(FVector(1000, 0, 44.1), false, nullptr, ETeleportType::TeleportPhysics);
	if (!TestTrue(TEXT("Physical light cart settles again before reconnecting"), Worlds.TickUntil([&]() { return Light->IsParked(); }, 180))) { return false; }
	TestTrue(TEXT("Rested light cart can reconnect after a bump"), Light->ConnectToCharger(PhysicsOutlet));
	AActor* CableWall = Blocker(Server, FVector(1007, -90, 110), FVector(150, 5, 100));
	if (!TestNotNull(TEXT("Charging cable obstruction fixture exists"), CableWall)) { return false; }
	Light->Tick(0.01f);
	TestTrue(TEXT("A blocked physical cable path disconnects charging"), !Light->CurrentCharger);
	CableWall->Destroy(); Worlds.TickAll(1);
	TestTrue(TEXT("Removing the obstruction permits reconnection"), Light->ConnectToCharger(PhysicsOutlet));
	Light->SetActorLocation(FVector(1000, 700, 45), false, nullptr, ETeleportType::TeleportPhysics); Light->Tick(0.01f);
	TestTrue(TEXT("Physical displacement beyond the lead range disconnects power"), !Light->CurrentCharger && !PhysicsOutlet->CanSupply(Light));
	Equipment->SetFreePhysicsEnabled(false); Light->SetFreePhysicsEnabled(false);
	Light->SetActorTickEnabled(false);
	PhysicsOutlet->Destroy();
	Ghost->Tags.Remove(TEXT("MallCartBoundary")); Ghost->ForceNetUpdate();
	TestTrue(TEXT("Removing cart opt-in restores authored collision on authority and all peers"), Worlds.TickUntil([&]()
	{
		if (Ghost->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_GameTraceChannel1) != ECR_Block) { return false; }
		for (int32 Index = 0; Index < Worlds.Clients.Num(); ++Index)
		{
			ACharacter* PeerGhost = Worlds.FindReplicatedObjectOnClient(Ghost, Index);
			if (!PeerGhost || PeerGhost->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_GameTraceChannel1) != ECR_Block) { return false; }
		}
		return true;
	}, 180));
	return true;
}

#endif
