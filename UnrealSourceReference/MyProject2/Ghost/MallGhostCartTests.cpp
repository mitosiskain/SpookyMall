#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Equipment/MallCartProtectionSubsystem.h"
#include "Equipment/MallPushCart.h"
#include "Equipment/MallStoreCartDock.h"
#include "Ghost/MallGhostCartHelpers.h"
#include "Ghost/MallGhostInteractionComponent.h"
#include "Ghost/MallGhostSoundComponent.h"
#include "Voice/MallGhostVoiceReceiver.h"
#include "Voice/MallVoiceSettings.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
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

namespace MallCartGhostCycleTest
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

	void FreezeCartGhostCharacters(UWorld* World)
	{
		for (TActorIterator<ACharacter> It(World); It; ++It)
		{
			// Test actual native scheduling independently of authored navigation and player movement.
			It->SetActorTickEnabled(false);
			It->GetCharacterMovement()->SetComponentTickEnabled(false);
		}
	}

	bool SetNumber(AActor* Actor, FName Name, double Value)
	{
		FNumericProperty* Property = Actor ? FindFProperty<FNumericProperty>(Actor->GetClass(), Name) : nullptr;
		if (!Property) { return false; }
		void* Address = Property->ContainerPtrToValuePtr<void>(Actor);
		if (Property->IsFloatingPoint()) { Property->SetFloatingPointPropertyValue(Address, Value); }
		else if (Property->IsInteger()) { Property->SetIntPropertyValue(Address, static_cast<int64>(Value)); }
		else { return false; }
		return true;
	}

	ACharacter* FindGhost(UWorld* World, UClass* Class)
	{
		for (TActorIterator<ACharacter> It(World); It; ++It) { if (It->IsA(Class)) { return *It; } }
		return nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMallCartGhostSuppressionCycleTest, "Mall.Cart.GhostSuppressionCycleNetwork",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMallCartGhostSuppressionCycleTest::RunTest(const FString& Parameters)
{
	using namespace MallCartGhostCycleTest;
	FRestoreSettings Restore;
	UE::Net::FTestWorlds Worlds(TEXT("/Engine/Maps/Entry"), TEXT("/Script/Engine.GameMode"));
	if (!TestTrue(TEXT("Cart ghost authority world loads"), Worlds.Server.IsLoaded())
		|| !TestTrue(TEXT("First real IP peer connects"), Worlds.CreateAndConnectClient())
		|| !TestTrue(TEXT("Second real IP peer connects"), Worlds.CreateAndConnectClient())) { return false; }
	Worlds.PreTickAllDelegate.AddLambda([&]()
	{
		FreezeCartGhostCharacters(Worlds.Server.GetWorld());
		for (UE::Net::FTestWorldInstance& Client : Worlds.Clients) { FreezeCartGhostCharacters(Client.GetWorld()); }
	});
	UWorld* Server = Worlds.Server.GetWorld();
	APlayerController* PC[2] = {};
	if (!TestTrue(TEXT("Both authoritative player controllers exist"), Worlds.TickUntil([&]()
	{
		PC[0] = Worlds.GetServerPlayerControllerOfClient(0); PC[1] = Worlds.GetServerPlayerControllerOfClient(1);
		return PC[0] && PC[1];
	}, 120))) { return false; }
	FActorSpawnParameters Spawn;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ACharacter* Players[2] = {};
	for (int32 Index = 0; Index < 2; ++Index)
	{
		Players[Index] = Server->SpawnActor<ACharacter>(FVector(3600, Index * 400, 90), FRotator::ZeroRotator, Spawn);
		if (!TestNotNull(TEXT("Possessable player fixture exists"), Players[Index])) { return false; }
		Players[Index]->GetCapsuleComponent()->SetCapsuleSize(34, 90);
		Players[Index]->SetReplicates(true); Players[Index]->SetReplicateMovement(true); PC[Index]->Possess(Players[Index]);
	}
	AMallPushCart* Cart = Server->SpawnActor<AMallPushCart>(FVector(3650, 0, 45), FRotator(0, 90, 0), Spawn);
	AMallStoreCartDock* Dock = Server->SpawnActor<AMallStoreCartDock>(FVector(3950, 0, 0), FRotator(0, 90, 0), Spawn);
	if (!TestNotNull(TEXT("Equipment cart fixture exists"), Cart) || !TestNotNull(TEXT("Doorway fixture exists"), Dock)) { return false; }
	Cart->SetFreePhysicsEnabled(false);
	Cart->SetActorTickEnabled(false);
	Dock->StoreTag = TEXT("CartGhostCycleFixture"); Dock->AssignedCart = Cart;

	UClass* GhostClass = LoadClass<ACharacter>(nullptr, TEXT("/Game/AI/StoreGhost/BP_StoreGhost.BP_StoreGhost_C"));
	if (!TestNotNull(TEXT("Actual authored store ghost Blueprint loads"), GhostClass)) { return false; }
	const FTransform Home(FRotator::ZeroRotator, FVector(5050, 100, 92.15));
	ACharacter* Ghost = Server->SpawnActorDeferred<ACharacter>(GhostClass, Home, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!TestNotNull(TEXT("Actual store ghost spawns"), Ghost)) { return false; }
	Ghost->Tags.AddUnique(TEXT("StoreGhostPrototype")); Ghost->Tags.AddUnique(TEXT("MallCartBoundary")); Ghost->Tags.AddUnique(Dock->StoreTag);
	Ghost->SetReplicates(true); Ghost->SetReplicateMovement(true);
	const bool bConfigured = SetNumber(Ghost, TEXT("StoreMinX"), 3950) && SetNumber(Ghost, TEXT("StoreMaxX"), 5950)
		&& SetNumber(Ghost, TEXT("StoreMinY"), -816.6666667) && SetNumber(Ghost, TEXT("StoreMaxY"), 816.6666667)
		&& SetNumber(Ghost, TEXT("StoreFloorZ"), 0) && SetNumber(Ghost, TEXT("CooldownUntil"), 0)
		&& SetNumber(Ghost, TEXT("CatchCount"), 0);
	UGameplayStatics::FinishSpawningActor(Ghost, Home); Ghost->SetActorTransform(Home); Ghost->SetActorTickEnabled(false);
	if (!TestTrue(TEXT("Ghost has finite authored store bounds"), bConfigured)) { return false; }
	UMallCartProtectionSubsystem* Protection = Server->GetSubsystem<UMallCartProtectionSubsystem>();
	UMallGhostInteractionComponent* State = nullptr;
	ACharacter* PeerGhost[2] = {};
	UMallGhostInteractionComponent* PeerState[2] = {};
	if (!TestNotNull(TEXT("Independent cart protection subsystem exists"), Protection)
		|| !TestTrue(TEXT("Ghost interaction state reaches both real peers"), Worlds.TickUntil([&]()
	{
		State = Ghost->FindComponentByClass<UMallGhostInteractionComponent>();
		for (int32 Index = 0; Index < 2; ++Index)
		{
			PeerGhost[Index] = FindGhost(Worlds.Clients[Index].GetWorld(), GhostClass);
			PeerState[Index] = PeerGhost[Index] ? PeerGhost[Index]->FindComponentByClass<UMallGhostInteractionComponent>() : nullptr;
			if (!PeerState[Index]) { return false; }
		}
		return State != nullptr;
	}, 180))) { return false; }
	State->bAutomaticHunts = true; State->bAutomaticDisturbances = false;
	State->InitialActivityMinSeconds = State->InitialActivityMaxSeconds = 2.0f;
	State->WarningDurationSeconds = 0.5f; State->HuntDurationSeconds = 4.0f;
	State->CooldownMinSeconds = State->CooldownMaxSeconds = 3.0f;
	if (UMallGhostSoundComponent* Sound = Ghost->FindComponentByClass<UMallGhostSoundComponent>()) { Sound->bAutomaticSounds = false; }
	UMallGhostVoiceReceiver* Voice = Ghost->FindComponentByClass<UMallGhostVoiceReceiver>();
	if (!Voice)
	{
		Voice = NewObject<UMallGhostVoiceReceiver>(Ghost, TEXT("CartGhostVoiceFixture"));
		Ghost->AddInstanceComponent(Voice); Voice->RegisterComponent();
	}
	if (!TestNotNull(TEXT("Native ghost voice endpoint exists without enabling microphone hardware"), Voice)) { return false; }
	auto PeersMatch = [&]()
	{
		return Worlds.TickUntil([&]()
		{
			for (UMallGhostInteractionComponent* Peer : PeerState)
			{
				if (Peer->CyclePhase != State->CyclePhase || Peer->GetGhostMode() != State->GetGhostMode()
					|| Peer->HuntCount != State->HuntCount
					|| !FMath::IsNearlyEqual(Peer->PhaseEndsServerTime, State->PhaseEndsServerTime, 0.001)) { return false; }
			}
			return true;
		}, 90);
	};
	auto WaitPhase = [&](EMallGhostCyclePhase Phase, int32 Frames = 300)
	{
		return Worlds.TickUntil([&]() { return State->CyclePhase == Phase; }, Frames);
	};
	Players[0]->SetActorLocation(FVector(4800, 0, 90));
	if (!TestTrue(TEXT("A visitor arms the actual native initial grace"), Worlds.TickUntil([&]()
	{
		return State->PhaseEndsServerTime > Server->GetTimeSeconds();
	}, 60))) { return false; }
	Cart->SetActorLocation(FVector(4500, 300, 45)); Cart->ForceNetUpdate();
	if (!TestTrue(TEXT("Moving the full cart inside disarms an already running grace"), Worlds.TickUntil([&]()
	{
		return State->CyclePhase == EMallGhostCyclePhase::Activity && State->IsInvisible() && State->PhaseEndsServerTime == 0.0;
	}, 60))) { return false; }
	TestTrue(TEXT("The inside cart is actual store suppression"), Protection->IsStoreSuppressedByCart(Ghost));
	TestFalse(TEXT("Dormant store rejects harmless disturbances and writing eligibility"), State->CanPerformDisturbance());
	const int32 BeforeVoice = Voice->ReactionCount;
	TestFalse(TEXT("Dormant store rejects spoken questions before device or fallback cues"), Voice->OnServerPhrase(PC[0], TEXT("where_are_you")));
	TestFalse(TEXT("Dormant store rejects requested signs"), Voice->OnServerPhrase(PC[0], TEXT("give_me_a_sign")));
	TestEqual(TEXT("Suppressed phrases cannot increment ghost reaction history"), Voice->ReactionCount, BeforeVoice);
	State->bAutomaticHunts = false;
	TestFalse(TEXT("Even a manual hunt cannot override inside-cart dormancy"), State->SetGhostMode(EMallGhostMode::VisibleHunt));
	TestFalse(TEXT("Even a manual warning cannot override inside-cart dormancy"), State->SetGhostMode(EMallGhostMode::VisibleWarning));
	State->bAutomaticHunts = true;
	Worlds.TickAll(180);
	TestTrue(TEXT("Dormancy remains invisible and harmless past the discarded grace"),
		State->IsInvisible() && !UMallGhostInteractionComponent::CanGhostHunt(Ghost)
		&& State->HuntCount == 0 && State->PhaseEndsServerTime == 0.0);
	if (!TestTrue(TEXT("Invisible disarmed dormancy replicates to both peers"), PeersMatch())) { return false; }
	for (ACharacter* Peer : PeerGhost) { TestTrue(TEXT("Replicated dormant ghost mesh stays hidden"), Peer->GetMesh()->bHiddenInGame != 0); }

	Cart->SetActorLocation(FVector(3650, 0, 45)); Cart->ForceNetUpdate();
	if (!TestTrue(TEXT("Removing the inside cart arms a complete new initial grace"), Worlds.TickUntil([&]()
	{
		return State->PhaseEndsServerTime > Server->GetTimeSeconds();
	}, 60))) { return false; }
	TestTrue(TEXT("Resumed grace has not inherited the old expired deadline"), State->PhaseEndsServerTime - Server->GetTimeSeconds() >= 1.7);
	TestTrue(TEXT("Ordinary invisible interactions become eligible again"), State->CanPerformDisturbance());
	TestTrue(TEXT("A spoken sign is accepted after the cart leaves"), Voice->OnServerPhrase(PC[0], TEXT("give_me_a_sign")));
	if (!TestTrue(TEXT("Resumed actual scheduling produces warning"), WaitPhase(EMallGhostCyclePhase::Warning))
		|| !TestTrue(TEXT("Resumed warning produces a bounded hunt"), WaitPhase(EMallGhostCyclePhase::Hunting))) { return false; }
	const double FirstHuntDeadline = State->PhaseEndsServerTime;
	Players[0]->SetActorLocation(FVector(3600, 0, 90));
	Worlds.TickAll(20);
	TestTrue(TEXT("Unsealed hallway pursuit does not prematurely end an active hunt"),
		State->CyclePhase == EMallGhostCyclePhase::Hunting && State->PhaseEndsServerTime == FirstHuntDeadline);
	TestTrue(TEXT("Actual target helper can select the nearest player outside the home store"),
		UMallGhostCartHelpers::ResolveTargetPawn(Ghost) == Players[0]);
	Cart->SetActorLocation(FVector(4500, 300, 45)); Cart->ForceNetUpdate();
	if (!TestTrue(TEXT("Moving the cart inside during a hunt resets actual scheduling to dormancy"), Worlds.TickUntil([&]()
	{
		return State->CyclePhase == EMallGhostCyclePhase::Activity && State->IsInvisible() && State->PhaseEndsServerTime == 0.0;
	}, 60))) { return false; }
	TestFalse(TEXT("An interrupted hunter cannot catch while dormant"), UMallGhostInteractionComponent::CanGhostHunt(Ghost));
	if (!TestTrue(TEXT("Hunt interruption and zero deadline replicate"), PeersMatch())) { return false; }

	Cart->SetActorLocation(FVector(3650, 0, 45)); Players[0]->SetActorLocation(FVector(4800, 0, 90));
	if (!TestTrue(TEXT("Another home-store visit starts a new warning"), WaitPhase(EMallGhostCyclePhase::Warning))
		|| !TestTrue(TEXT("Only the completed new warning starts another hunt"), WaitPhase(EMallGhostCyclePhase::Hunting))) { return false; }
	Players[0]->SetActorLocation(FVector(3600, 0, 90));
	Cart->SetActorLocation(FVector(3950, 0, 45)); Cart->SetActorRotation(FRotator(0, 90, 0)); Cart->ForceNetUpdate();
	TestTrue(TEXT("Equipment left physically across the doorway seals the store"), Protection->IsStoreSealed(Ghost));
	TestFalse(TEXT("A doorway cart does not suppress investigation within the store"), Protection->IsStoreSuppressedByCart(Ghost));
	TestFalse(TEXT("Sealing immediately rejects outside players as targets"), UMallGhostCartHelpers::CanGhostPursuePlayer(Ghost, Players[0]));
	if (!TestTrue(TEXT("A sealed hunt with only outside players ends in normal cooldown"), WaitPhase(EMallGhostCyclePhase::Cooldown, 60))) { return false; }
	TestTrue(TEXT("Cooldown remains harmless and invisible"), State->IsInvisible() && !UMallGhostInteractionComponent::CanGhostHunt(Ghost));
	return true;
}

#endif
