#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Ghost/MallGhostInteractionComponent.h"
#include "Ghost/MallGhostSoundComponent.h"
#include "Ghost/MallGhostWritingBook.h"
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

namespace MallGhostHuntCycleTest
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
			// Scheduling and its replication must work without touching microphone hardware.
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
			// Exercise the native scheduler independently of navigation and the authored chase tick.
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
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMallGhostHuntCycleNetworkTest, "Mall.Ghost.AutomaticHuntCycleNetwork",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMallGhostHuntCycleNetworkTest::RunTest(const FString& Parameters)
{
	using namespace MallGhostHuntCycleTest;
	FRestoreSettings Restore;
	UE::Net::FTestWorlds Worlds(TEXT("/Engine/Maps/Entry"), TEXT("/Script/Engine.GameMode"));
	if (!TestTrue(TEXT("Authority world loads"), Worlds.Server.IsLoaded())
		|| !TestTrue(TEXT("First real IP peer connects"), Worlds.CreateAndConnectClient())
		|| !TestTrue(TEXT("Second real IP peer connects"), Worlds.CreateAndConnectClient())) { return false; }
	Worlds.PreTickAllDelegate.AddLambda([&]()
	{
		FreezeCharacters(Worlds.Server.GetWorld());
		for (UE::Net::FTestWorldInstance& Client : Worlds.Clients) { FreezeCharacters(Client.GetWorld()); }
	});
	UWorld* Server = Worlds.Server.GetWorld();
	APlayerController* PC[2] = {};
	if (!TestTrue(TEXT("Connected server controllers exist"), Worlds.TickUntil([&]()
	{
		PC[0] = Worlds.GetServerPlayerControllerOfClient(0); PC[1] = Worlds.GetServerPlayerControllerOfClient(1);
		return PC[0] && PC[1];
	}, 120))) { return false; }
	FActorSpawnParameters Spawn;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ACharacter* Player[2] = {};
	for (int32 Index = 0; Index < 2; ++Index)
	{
		Player[Index] = Server->SpawnActor<ACharacter>(FVector(3600, 200 * Index, 90), FRotator::ZeroRotator, Spawn);
		if (!TestNotNull(TEXT("Possessable player fixture spawns"), Player[Index])) { return false; }
		Player[Index]->GetCapsuleComponent()->SetCapsuleSize(34.0f, 90.0f);
		Player[Index]->SetReplicates(true); Player[Index]->SetReplicateMovement(true);
		PC[Index]->Possess(Player[Index]);
	}
	UClass* GhostClass = LoadClass<ACharacter>(nullptr, TEXT("/Game/AI/StoreGhost/BP_StoreGhost.BP_StoreGhost_C"));
	if (!TestNotNull(TEXT("Actual store ghost Blueprint loads"), GhostClass)) { return false; }
	const FTransform Home(FRotator::ZeroRotator, FVector(5050, 100, 92.15));
	ACharacter* Ghost = Server->SpawnActorDeferred<ACharacter>(GhostClass, Home, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!TestNotNull(TEXT("Actual store ghost spawns"), Ghost)) { return false; }
	Ghost->Tags.AddUnique(TEXT("StoreGhostPrototype")); Ghost->SetReplicates(true); Ghost->SetReplicateMovement(true);
	const bool bConfigured = SetNumber(Ghost, TEXT("StoreMinX"), 3950) && SetNumber(Ghost, TEXT("StoreMaxX"), 5950)
		&& SetNumber(Ghost, TEXT("StoreMinY"), -816.6666667) && SetNumber(Ghost, TEXT("StoreMaxY"), 816.6666667)
		&& SetNumber(Ghost, TEXT("StoreFloorZ"), 0) && SetNumber(Ghost, TEXT("CooldownUntil"), 0)
		&& SetNumber(Ghost, TEXT("CatchCount"), 0);
	UGameplayStatics::FinishSpawningActor(Ghost, Home); Ghost->SetActorTransform(Home); Ghost->SetActorTickEnabled(false);
	if (!TestTrue(TEXT("Fixture configures physical store and catch history"), bConfigured)) { return false; }
	UMallGhostInteractionComponent* State = nullptr;
	UMallGhostInteractionComponent* Peer[2] = {};
	ACharacter* PeerGhost[2] = {};
	if (!TestTrue(TEXT("Independent state reaches both peers"), Worlds.TickUntil([&]()
	{
		State = Ghost->FindComponentByClass<UMallGhostInteractionComponent>();
		for (int32 Index = 0; Index < 2; ++Index)
		{
			PeerGhost[Index] = FindGhost(Worlds.Clients[Index].GetWorld(), GhostClass);
			Peer[Index] = PeerGhost[Index] ? PeerGhost[Index]->FindComponentByClass<UMallGhostInteractionComponent>() : nullptr;
			if (!Peer[Index]) { return false; }
		}
		return State != nullptr;
	}, 180))) { return false; }
	TestTrue(TEXT("Automatic hunts default on without enabling voice"), State->bAutomaticHunts);
	State->bAutomaticDisturbances = false;
	if (UMallGhostSoundComponent* Sound = Ghost->FindComponentByClass<UMallGhostSoundComponent>()) { Sound->bAutomaticSounds = false; }
	State->InitialActivityMinSeconds = State->InitialActivityMaxSeconds = 2.0f;
	State->WarningDurationSeconds = 2.0f;
	State->HuntDurationSeconds = 3.0f;
	State->CooldownMinSeconds = State->CooldownMaxSeconds = 5.0f;
	auto InStore = [&]() { Player[0]->SetActorLocation(FVector(4800, 0, 90)); Player[0]->ForceNetUpdate(); };
	auto EmptyStore = [&]()
	{
		for (ACharacter* Pawn : Player) { Pawn->SetActorLocation(FVector(3600, 0, 90)); Pawn->ForceNetUpdate(); }
	};
	auto WaitPhase = [&](EMallGhostCyclePhase Phase, int32 Frames = 240)
	{
		return Worlds.TickUntil([&]() { return State->CyclePhase == Phase; }, Frames);
	};
	auto PeersMatch = [&]()
	{
		return Worlds.TickUntil([&]()
		{
			for (UMallGhostInteractionComponent* Component : Peer)
			{
				if (Component->CyclePhase != State->CyclePhase || Component->GetGhostMode() != State->GetGhostMode()
					|| Component->HuntCount != State->HuntCount
					|| !FMath::IsNearlyEqual(Component->PhaseEndsServerTime, State->PhaseEndsServerTime, 0.001)) { return false; }
			}
			return true;
		}, 90);
	};
	Worlds.TickAll(90);
	TestTrue(TEXT("Empty store leaves the initial grace disarmed"), State->PhaseEndsServerTime == 0.0 && State->HuntCount == 0);
	// One center is inside but its capsule protrudes; the other player is on the next level.
	Player[0]->SetActorLocation(FVector(3960, 0, 90)); Player[1]->SetActorLocation(FVector(4800, 0, 940));
	Worlds.TickAll(90);
	TestTrue(TEXT("Capsule edges and upstairs players cannot arm a ground-floor hunt"), State->PhaseEndsServerTime == 0.0 && State->HuntCount == 0);
	Player[1]->SetActorLocation(FVector(3600, 0, 90));
	InStore();
	if (!TestTrue(TEXT("Entering fully starts harmless activity"), Worlds.TickUntil([&]()
	{
		return State->PhaseEndsServerTime > Server->GetTimeSeconds();
	}, 60))) { return false; }
	TestTrue(TEXT("Initial activity cannot harm the visitor"), State->IsInvisible() && !UMallGhostInteractionComponent::CanGhostHunt(Ghost));
	const double FirstGraceEnd = State->PhaseEndsServerTime;
	Worlds.TickAll(20); EmptyStore();
	if (!TestTrue(TEXT("Leaving during grace disarms it"), Worlds.TickUntil([&]() { return State->PhaseEndsServerTime == 0.0; }, 60))) { return false; }
	InStore();
	if (!TestTrue(TEXT("Returning starts a complete new grace period"), Worlds.TickUntil([&]() { return State->PhaseEndsServerTime > FirstGraceEnd; }, 60))) { return false; }
	TestTrue(TEXT("Fresh grace is not the abandoned deadline"), State->PhaseEndsServerTime - Server->GetTimeSeconds() >= 1.7);
	if (!TestTrue(TEXT("Activity automatically becomes a warning"), WaitPhase(EMallGhostCyclePhase::Warning))) { return false; }
	TestTrue(TEXT("Warning shows the ghost but cannot catch"), State->GetGhostMode() == EMallGhostMode::VisibleWarning
		&& !Ghost->GetMesh()->bHiddenInGame && !UMallGhostInteractionComponent::CanGhostHunt(Ghost));
	TestFalse(TEXT("Warning does not permit invisible evidence actions"), State->CanPerformDisturbance());
	// Make every older Blueprint catch precondition true, then invoke its real entry point.
	UFunction* CatchFunction = Ghost->FindFunction(TEXT("TryCatchPlayer"));
	FStrProperty* PrototypeState = FindFProperty<FStrProperty>(GhostClass, TEXT("PrototypeState"));
	FObjectPropertyBase* TargetPlayer = FindFProperty<FObjectPropertyBase>(GhostClass, TEXT("TargetPlayer"));
	bool bCatchFixture = CatchFunction && CatchFunction->NumParms == 0 && PrototypeState && TargetPlayer;
	for (FName Flag : {FName(TEXT("bPlayerEligible")), FName(TEXT("bPawnSafe")), FName(TEXT("bHomeValid"))})
	{
		FBoolProperty* Property = FindFProperty<FBoolProperty>(GhostClass, Flag);
		bCatchFixture &= Property != nullptr;
		if (Property) { Property->SetPropertyValue_InContainer(Ghost, true); }
	}
	if (!TestTrue(TEXT("Actual catch function and legacy preconditions exist"), bCatchFixture)) { return false; }
	PrototypeState->SetPropertyValue_InContainer(Ghost, TEXT("Chasing"));
	TargetPlayer->SetObjectPropertyValue_InContainer(Ghost, Player[0]);
	Player[0]->SetActorLocation(FVector(5000, 100, 90));
	Ghost->ProcessEvent(CatchFunction, nullptr);
	const FNumericProperty* CatchCounter = FindFProperty<FNumericProperty>(GhostClass, TEXT("CatchCount"));
	TestTrue(TEXT("Direct Blueprint catch remains blocked throughout visible warning"), CatchCounter
		&& CatchCounter->GetSignedIntPropertyValue(CatchCounter->ContainerPtrToValuePtr<void>(Ghost)) == 0);
	State->ApplyGhostMode(EMallGhostMode::VisibleWarning); InStore();
	if (!TestTrue(TEXT("Warning mode and deadline replicate to both peers"), PeersMatch())) { return false; }
	TestFalse(TEXT("A client cannot skip the warning"), Peer[0]->SetGhostMode(EMallGhostMode::VisibleHunt));
	Peer[0]->PhaseEndsServerTime = 0.01; Peer[0]->UpdateHuntCycle();
	TestEqual(TEXT("Client ticking cannot advance its own warning"), Peer[0]->CyclePhase, EMallGhostCyclePhase::Warning);
	EmptyStore();
	if (!TestTrue(TEXT("Leaving during warning cancels the hunt"), WaitPhase(EMallGhostCyclePhase::Activity, 60))) { return false; }
	TestTrue(TEXT("Cancelled warning hides the ghost without counting a hunt"), State->IsInvisible() && State->HuntCount == 0 && State->PhaseEndsServerTime == 0.0);

	InStore();
	if (!TestTrue(TEXT("A later visitor again receives warning"), WaitPhase(EMallGhostCyclePhase::Warning))
		|| !TestTrue(TEXT("Warning automatically becomes hunting"), WaitPhase(EMallGhostCyclePhase::Hunting))) { return false; }
	TestTrue(TEXT("Hunt scheduling is independent of prop scheduling"), !State->bAutomaticDisturbances && State->HuntCount == 1);
	TestTrue(TEXT("Only the server's active hunt authorizes catches"), UMallGhostInteractionComponent::CanGhostHunt(Ghost)
		&& !UMallGhostInteractionComponent::CanGhostHunt(PeerGhost[0]));
	if (!TestTrue(TEXT("Active hunt replicates to both existing peers"), PeersMatch())) { return false; }
	const double HuntEnd = State->PhaseEndsServerTime;
	State->PauseDisturbances(0.5f); SetNumber(Ghost, TEXT("CooldownUntil"), Server->GetTimeSeconds() + 0.5);
	Worlds.TickAll(15);
	TestTrue(TEXT("A short voice pause does not replace the hunt deadline"), State->CyclePhase == EMallGhostCyclePhase::Hunting
		&& State->PhaseEndsServerTime == HuntEnd);
	// Hold this phase long enough for a real late connection, then exercise natural deadline expiry.
	State->PhaseEndsServerTime = Server->GetTimeSeconds() + 10.0; Ghost->ForceNetUpdate();
	if (!TestTrue(TEXT("Late peer joins during a hunt"), Worlds.CreateAndConnectClient())) { return false; }
	UMallGhostInteractionComponent* LateState = nullptr;
	ACharacter* LateGhost = nullptr;
	if (!TestTrue(TEXT("Late join receives current hunt once, with its existing deadline"), Worlds.TickUntil([&]()
	{
		LateGhost = FindGhost(Worlds.Clients[2].GetWorld(), GhostClass);
		LateState = LateGhost ? LateGhost->FindComponentByClass<UMallGhostInteractionComponent>() : nullptr;
		return LateState && LateState->CyclePhase == EMallGhostCyclePhase::Hunting && LateState->HuntCount == 1
			&& LateState->GetGhostMode() == EMallGhostMode::VisibleHunt && !LateGhost->GetMesh()->bHiddenInGame
			&& FMath::IsNearlyEqual(LateState->PhaseEndsServerTime, State->PhaseEndsServerTime, 0.001);
	}, 180))) { return false; }
	State->PhaseEndsServerTime = Server->GetTimeSeconds() + 0.3; Ghost->ForceNetUpdate();
	if (!TestTrue(TEXT("Expired hunt automatically enters cooldown"), WaitPhase(EMallGhostCyclePhase::Cooldown, 90))) { return false; }
	TestTrue(TEXT("Cooldown is invisible and harmless"), State->IsInvisible() && !UMallGhostInteractionComponent::CanGhostHunt(Ghost));
	if (!TestTrue(TEXT("Cooldown replicates to both original peers"), PeersMatch())) { return false; }
	const double CooldownEnd = State->PhaseEndsServerTime;
	TestTrue(TEXT("A hunt cooldown preserves ordinary invisible interactions"), State->CanPerformDisturbance());
	AMallGhostWritingBook* Book = Server->SpawnActor<AMallGhostWritingBook>(FVector(4800, 150, 60), FRotator::ZeroRotator, Spawn);
	if (!TestNotNull(TEXT("Evidence book fixture spawns"), Book)) { return false; }
	TestTrue(TEXT("Player can open a blank book during cooldown"), Book->TrySetOpen(PC[0], true));
	TestTrue(TEXT("An explicit evidence request still succeeds during cooldown"), State->DisturbNearestBook() && Book->IsWritten());
	Worlds.TickAll(180);
	TestTrue(TEXT("Three-second movement recovery cannot bypass the longer hunt cooldown"),
		State->CyclePhase == EMallGhostCyclePhase::Cooldown && State->HuntCount == 1 && State->PhaseEndsServerTime == CooldownEnd);
	State->PhaseEndsServerTime = Server->GetTimeSeconds() + 0.3; Ghost->ForceNetUpdate();
	if (!TestTrue(TEXT("Cooldown eventually permits another warning"), WaitPhase(EMallGhostCyclePhase::Warning, 120))
		|| !TestTrue(TEXT("A second hunt starts only after cooldown and warning"), WaitPhase(EMallGhostCyclePhase::Hunting))) { return false; }
	TestEqual(TEXT("One completed cooldown produces exactly one new hunt"), State->HuntCount, 2);
	TestTrue(TEXT("Fixture reports a catch using the real Blueprint counter"), SetNumber(Ghost, TEXT("CatchCount"), 1));
	SetNumber(Ghost, TEXT("CooldownUntil"), Server->GetTimeSeconds() + 3.0);
	if (!TestTrue(TEXT("A catch ends the hunt immediately"), WaitPhase(EMallGhostCyclePhase::Cooldown, 60))) { return false; }
	TestFalse(TEXT("Existing post-catch recovery still blocks immediate disturbances"), State->CanPerformDisturbance());
	Worlds.TickAll(195);
	TestTrue(TEXT("Finishing catch recovery does not restart the same hunt"),
		State->CyclePhase == EMallGhostCyclePhase::Cooldown && State->HuntCount == 2 && State->CanPerformDisturbance());
	const double CatchCooldownEnd = State->PhaseEndsServerTime;
	EmptyStore(); Worlds.TickAll(20);
	TestTrue(TEXT("Leaving cannot erase a running inter-hunt cooldown"), State->CyclePhase == EMallGhostCyclePhase::Cooldown
		&& State->PhaseEndsServerTime == CatchCooldownEnd);
	State->PhaseEndsServerTime = Server->GetTimeSeconds() + 0.3; Ghost->ForceNetUpdate();
	if (!TestTrue(TEXT("An empty store disarms after its cooldown expires"), WaitPhase(EMallGhostCyclePhase::Activity, 90))) { return false; }
	TestEqual(TEXT("No queued hunt starts in an empty store"), State->HuntCount, 2);
	TestEqual(TEXT("Empty store requires a new visitor grace"), State->PhaseEndsServerTime, 0.0);

	InStore();
	if (!TestTrue(TEXT("Returning after empty cooldown receives a fresh warning"), WaitPhase(EMallGhostCyclePhase::Warning))) { return false; }
	State->bAutomaticHunts = false;
	Worlds.TickAll(20);
	TestTrue(TEXT("Disabling automatic hunts safely cancels its warning"), State->IsInvisible() && State->PhaseEndsServerTime == 0.0 && State->HuntCount == 2);
	State->bAutomaticHunts = true;
	if (!TestTrue(TEXT("Re-enabling receives a new activity period and warning"), WaitPhase(EMallGhostCyclePhase::Warning))
		|| !TestTrue(TEXT("Re-enabled cycle can hunt again"), WaitPhase(EMallGhostCyclePhase::Hunting))) { return false; }
	State->bAutomaticHunts = false;
	Worlds.TickAll(20);
	TestTrue(TEXT("Disabling automatic hunts safely ends its current hunt"), State->IsInvisible()
		&& State->CyclePhase == EMallGhostCyclePhase::Activity && State->PhaseEndsServerTime == 0.0);
	TestTrue(TEXT("Explicit manual hunting remains available with automation disabled"), State->SetGhostMode(EMallGhostMode::VisibleHunt));
	Worlds.TickAll(40);
	TestTrue(TEXT("Disabled scheduler does not rewrite an intentional manual mode"), State->GetGhostMode() == EMallGhostMode::VisibleHunt);
	State->SetGhostMode(EMallGhostMode::InvisibleDisturbance);
	TestTrue(TEXT("Fixture ends harmlessly without a pending phase"), State->IsInvisible() && State->PhaseEndsServerTime == 0.0);
	return true;
}

#endif
