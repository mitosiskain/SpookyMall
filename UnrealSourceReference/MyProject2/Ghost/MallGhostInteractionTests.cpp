#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Ghost/MallGhostInteractionComponent.h"
#include "Ghost/MallGhostDisturbanceProp.h"
#include "Ghost/MallGhostWritingBook.h"
#include "Voice/MallVoiceSettings.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
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

namespace MallGhostInteractionTest
{
	struct FRestoreSettings
	{
		UMallVoiceSettings* Settings = GetMutableDefault<UMallVoiceSettings>();
		bool bVoice = Settings->bEnabled;
		bool bSpeech = Settings->bSpeechRecognitionEnabled;
		bool bStatus = Settings->bShowVoiceStatus;
		FString Map = UGameMapsSettings::GetGameDefaultMap(EDefaultMapRequestType::Client);
		FString GameMode = UGameMapsSettings::GetGlobalDefaultGameMode();
		TSubclassOf<UGameViewportClient> Viewport = GEngine->GameViewportClientClass;

		FRestoreSettings()
		{
			// Ghost state must exist with voice disabled, without touching microphone hardware.
			Settings->bEnabled = false;
			Settings->bSpeechRecognitionEnabled = false;
			Settings->bShowVoiceStatus = false;
			UGameMapsSettings::SetGameDefaultMap(TEXT("/Engine/Maps/Entry"));
			UGameMapsSettings::SetGlobalDefaultGameMode(TEXT("/Script/Engine.GameMode"));
			if (UClass* Compatible = LoadClass<UGameViewportClient>(nullptr, TEXT("/Script/CommonUI.CommonGameViewportClient")))
			{
				GEngine->GameViewportClientClass = Compatible;
			}
		}
		~FRestoreSettings()
		{
			Settings->bEnabled = bVoice;
			Settings->bSpeechRecognitionEnabled = bSpeech;
			Settings->bShowVoiceStatus = bStatus;
			UGameMapsSettings::SetGameDefaultMap(Map);
			UGameMapsSettings::SetGlobalDefaultGameMode(GameMode);
			GEngine->GameViewportClientClass = Viewport;
		}
	};

	bool SetNumber(AActor* Object, const FName Name, const double Value)
	{
		FNumericProperty* Property = Object ? FindFProperty<FNumericProperty>(Object->GetClass(), Name) : nullptr;
		if (!Property || !Property->IsFloatingPoint()) { return false; }
		Property->SetFloatingPointPropertyValue(Property->ContainerPtrToValuePtr<void>(Object), Value);
		return true;
	}

	template<class T> T* First(UWorld* World)
	{
		for (TActorIterator<T> It(World); It; ++It) { return *It; }
		return nullptr;
	}

	ACharacter* FindGhost(UWorld* World, UClass* GhostClass)
	{
		for (TActorIterator<ACharacter> It(World); It; ++It) { if (It->IsA(GhostClass)) { return *It; } }
		return nullptr;
	}

	void FreezeCharacters(UWorld* World)
	{
		for (TActorIterator<ACharacter> It(World); It; ++It)
		{
			It->SetActorTickEnabled(false);
			It->GetCharacterMovement()->DisableMovement();
			It->GetCharacterMovement()->SetComponentTickEnabled(false);
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMallGhostInteractionTest, "Mall.Ghost.StateAndDisturbance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMallGhostInteractionTest::RunTest(const FString& Parameters)
{
	using namespace MallGhostInteractionTest;
	FRestoreSettings Restore;
	UE::Net::FTestWorlds Worlds(TEXT("/Engine/Maps/Entry"), TEXT("/Script/Engine.GameMode"));
	if (!TestTrue(TEXT("Temporary server loads"), Worlds.Server.IsLoaded())
		|| !TestTrue(TEXT("Real IP client connects"), Worlds.CreateAndConnectClient())) { return false; }
	Worlds.PreTickAllDelegate.AddLambda([&Worlds]()
	{
		FreezeCharacters(Worlds.Server.GetWorld());
		for (UE::Net::FTestWorldInstance& Client : Worlds.Clients) { FreezeCharacters(Client.GetWorld()); }
	});
	UWorld* Server = Worlds.Server.GetWorld();
	UWorld* Client = Worlds.Clients[0].GetWorld();
	APlayerController* ServerPC = nullptr;
	if (!TestTrue(TEXT("Authoritative connected controller exists"), Worlds.TickUntil([&]()
	{
		ServerPC = Worlds.GetServerPlayerControllerOfClient(0);
		return ServerPC != nullptr;
	}, 120))) { return false; }

	FActorSpawnParameters Spawn;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ACharacter* Player = Server->SpawnActor<ACharacter>(FVector(4950.0, 150.0, 90.0), FRotator::ZeroRotator, Spawn);
	if (!TestNotNull(TEXT("Possessable player fixture spawns"), Player)) { return false; }
	Player->GetCapsuleComponent()->SetCapsuleSize(34.0f, 90.0f);
	Player->SetReplicates(true);
	ServerPC->Possess(Player);
	TestFalse(TEXT("Missing state component cannot authorize hunting"), UMallGhostInteractionComponent::CanGhostHunt(Player));

	UClass* GhostClass = LoadClass<ACharacter>(nullptr, TEXT("/Game/AI/StoreGhost/BP_StoreGhost.BP_StoreGhost_C"));
	if (!TestNotNull(TEXT("Actual store ghost Blueprint loads"), GhostClass)) { return false; }
	const FTransform Home(FRotator::ZeroRotator, FVector(4950.0, 0.0, 92.15));
	ACharacter* Ghost = Server->SpawnActorDeferred<ACharacter>(GhostClass, Home, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!TestNotNull(TEXT("Actual store ghost Blueprint spawns"), Ghost)) { return false; }
	Ghost->Tags.AddUnique(TEXT("StoreGhostPrototype"));
	Ghost->SetReplicates(true);
	Ghost->SetReplicateMovement(true);
	Ghost->SetActorTickEnabled(false);
	const bool bStoreConfigured = SetNumber(Ghost, TEXT("StoreMinX"), 3950.0) && SetNumber(Ghost, TEXT("StoreMaxX"), 5950.0)
		&& SetNumber(Ghost, TEXT("StoreMinY"), -816.6666667) && SetNumber(Ghost, TEXT("StoreMaxY"), 816.6666667)
		&& SetNumber(Ghost, TEXT("StoreFloorZ"), 0.0) && SetNumber(Ghost, TEXT("CooldownUntil"), 0.0);
	UGameplayStatics::FinishSpawningActor(Ghost, Home);
	Ghost->SetActorTransform(Home);
	Ghost->SetActorTickEnabled(false);
	Ghost->ForceNetUpdate();
	if (!TestTrue(TEXT("Fixture uses the authored ground-floor store contract"), bStoreConfigured)) { return false; }

	UMallGhostInteractionComponent* State = nullptr;
	UMallGhostInteractionComponent* ClientState = nullptr;
	ACharacter* ClientGhost = nullptr;
	if (!TestTrue(TEXT("Voice-independent state subsystem attaches and replicates"), Worlds.TickUntil([&]()
	{
		State = Ghost->FindComponentByClass<UMallGhostInteractionComponent>();
		ClientGhost = FindGhost(Client, GhostClass);
		ClientState = ClientGhost ? ClientGhost->FindComponentByClass<UMallGhostInteractionComponent>() : nullptr;
		return State && ClientState;
	}, 180))) { return false; }
	State->bAutomaticDisturbances = false;
	State->bAutomaticHunts = false;
	TestTrue(TEXT("Ghost defaults to invisible disturbances"), State->IsInvisible() && ClientState->IsInvisible());
	TestFalse(TEXT("Invisible ghost cannot authorize a catch"), UMallGhostInteractionComponent::CanGhostHunt(Ghost));
	TestFalse(TEXT("Client cannot authorize hunting"), UMallGhostInteractionComponent::CanGhostHunt(ClientGhost));
	TestTrue(TEXT("Invisible server may perform disturbances"), State->CanPerformDisturbance());
	TestFalse(TEXT("Client may not perform disturbances"), ClientState->CanPerformDisturbance());
	TestTrue(TEXT("Server ghost mesh is hidden"), Ghost->GetMesh()->bHiddenInGame != 0);
	TestTrue(TEXT("Client ghost mesh is hidden"), ClientGhost->GetMesh()->bHiddenInGame != 0);
	TestFalse(TEXT("Client cannot switch authoritative mode"), ClientState->SetGhostMode(EMallGhostMode::VisibleHunt));

	// State changes preserve original visibility and keep transient voice signals independent.
	UTextRenderComponent* AuthoredLabel = NewObject<UTextRenderComponent>(Ghost);
	AuthoredLabel->SetupAttachment(Ghost->GetRootComponent());
	AuthoredLabel->SetText(FText::FromString(TEXT("STORE GHOST")));
	AuthoredLabel->RegisterComponent();
	UTextRenderComponent* NativeSignal = NewObject<UTextRenderComponent>(Ghost, NAME_None, RF_Transient);
	NativeSignal->ComponentTags.Add(TEXT("MallGhostSignal"));
	NativeSignal->SetupAttachment(Ghost->GetRootComponent());
	NativeSignal->RegisterComponent();
	Ghost->GetCharacterMovement()->Velocity = FVector(100.0, 0.0, 0.0);
	State->SetGhostMode(EMallGhostMode::InvisibleDisturbance);
	TestTrue(TEXT("Invisible transition stops existing velocity"), Ghost->GetVelocity().IsNearlyZero());
	TestTrue(TEXT("Invisible transition hides the authored label"), AuthoredLabel->bHiddenInGame != 0);
	TestFalse(TEXT("Native transient voice signal is not hidden"), NativeSignal->bHiddenInGame != 0);
	TestTrue(TEXT("Invisibility preserves actor collision"), Ghost->GetActorEnableCollision());

	AMallGhostWritingBook* Book = Server->SpawnActor<AMallGhostWritingBook>(FVector(4950.0, 250.0, 60.0), FRotator::ZeroRotator, Spawn);
	AMallGhostDisturbanceProp* Prop = Server->SpawnActor<AMallGhostDisturbanceProp>(FVector(5050.0, 100.0, 60.0), FRotator::ZeroRotator, Spawn);
	if (!TestNotNull(TEXT("Native writing book spawns"), Book) || !TestNotNull(TEXT("Native opted-in physics prop spawns"), Prop)) { return false; }
	// The Entry map has no floor; this fixture isolates horizontal movement/replication.
	Prop->PropMesh->SetEnableGravity(false);
	Book->WritingDuration = 0.5f;
	TestFalse(TEXT("A new evidence book is closed and blank"), Book->IsOpen() || Book->IsWritten());
	TestFalse(TEXT("Closed book rejects ghost writing"), Book->TryWrite(Ghost));
	TestTrue(TEXT("Nearby player explicitly opens the placed book"), Book->TrySetOpen(ServerPC, true));
	TestTrue(TEXT("An open, uncarried book is placed for writing"), Book->IsPlacedForWriting());
	Book->ForceNetUpdate();
	TestTrue(TEXT("Book's complete bounds fit its store"), UMallGhostInteractionComponent::IsActorInGhostStore(Ghost, Book));
	Book->SetActorLocation(FVector(5945.0, 250.0, 60.0));
	TestFalse(TEXT("A center inside the store is insufficient when book bounds protrude"), UMallGhostInteractionComponent::IsActorInGhostStore(Ghost, Book));
	TestFalse(TEXT("Protruding book cannot be written in"), Book->TryWrite(Ghost));
	Book->SetActorLocation(FVector(4950.0, 250.0, 910.0));
	TestFalse(TEXT("Upstairs book is excluded"), UMallGhostInteractionComponent::IsActorInGhostStore(Ghost, Book));
	Book->SetActorLocation(FVector(4950.0, 997.0, 60.0));
	TestFalse(TEXT("Neighbor-store book is excluded"), Book->TryWrite(Ghost));
	Book->SetActorLocation(FVector(4950.0, 250.0, 60.0));
	Book->ForceNetUpdate();

	TestTrue(TEXT("Server can enter visible hunt"), State->SetGhostMode(EMallGhostMode::VisibleHunt));
	TestTrue(TEXT("Visible server mode authorizes existing hunt path"), UMallGhostInteractionComponent::CanGhostHunt(Ghost));
	TestFalse(TEXT("Visible hunt cannot write evidence"), Book->TryWrite(Ghost));
	TestFalse(TEXT("Visible hunt cannot throw props"), Prop->Disturb(Ghost, true));
	TestFalse(TEXT("Visible mode restores authored label visibility"), AuthoredLabel->bHiddenInGame != 0);
	if (!TestTrue(TEXT("Visible hunt mode replicates"), Worlds.TickUntil([&]()
	{
		return ClientState->GetGhostMode() == EMallGhostMode::VisibleHunt && !ClientGhost->GetMesh()->bHiddenInGame;
	}, 120))) { return false; }
	TestFalse(TEXT("Client still cannot authorize a hunt after replication"), UMallGhostInteractionComponent::CanGhostHunt(ClientGhost));
	State->SetGhostMode(EMallGhostMode::InvisibleDisturbance);
	State->PauseDisturbances(0.3f);
	TestFalse(TEXT("Pause blocks writing immediately"), State->DisturbNearestBook());
	TestFalse(TEXT("Pause blocks direct native throw"), Prop->Disturb(Ghost, true));
	Worlds.TickAll(25);
	SetNumber(Ghost, TEXT("CooldownUntil"), Server->GetTimeSeconds() + 0.3);
	TestFalse(TEXT("Existing voice cooldown also blocks disturbances"), State->CanPerformDisturbance());
	Worlds.TickAll(25);
	TestTrue(TEXT("Paused disturbance can resume after both deadlines"), State->CanPerformDisturbance());

	TestTrue(TEXT("Nearest opted-in book accepts invisible ghost writing"), State->DisturbNearestBook());
	TestTrue(TEXT("Accepted writing becomes permanent evidence"), Book->IsWritten());
	TestFalse(TEXT("Repeated writing leaves original evidence unchanged"), Book->TryWrite(Ghost));
	TestEqual(TEXT("Only accepted book write increments state count"), State->DisturbanceCount, 1);
	AMallGhostWritingBook* ClientBook = nullptr;
	AMallGhostDisturbanceProp* ClientProp = nullptr;
	if (!TestTrue(TEXT("Writing state and start time replicate to client"), Worlds.TickUntil([&]()
	{
		ClientBook = First<AMallGhostWritingBook>(Client);
		ClientProp = First<AMallGhostDisturbanceProp>(Client);
		return ClientBook && ClientProp && ClientBook->bWritten
			&& FMath::IsNearlyEqual(ClientBook->WritingStartedAtServerTime, Book->WritingStartedAtServerTime, 0.001)
			&& ClientState->DisturbanceCount == 1 && ClientState->LastDisturbance == TEXT("write");
	}, 120))) { return false; }
	TestFalse(TEXT("Client cannot write directly"), ClientBook->TryWrite(ClientGhost));
	TestFalse(TEXT("Client cannot disturb directly"), ClientProp->Disturb(ClientGhost, true));
	Worlds.TickAll(50);
	TestTrue(TEXT("Server and client writing reveal reaches completion"), Book->WritingProgress >= 0.99f && ClientBook->WritingProgress >= 0.99f);
	const double OriginalWritingStart = Book->WritingStartedAtServerTime;
	AActor* BookSupport = Server->SpawnActor<AActor>(FVector(4950.0, 250.0, 55.0), FRotator::ZeroRotator, Spawn);
	if (!TestNotNull(TEXT("Static book placement support spawns"), BookSupport)) { return false; }
	UBoxComponent* SupportShape = NewObject<UBoxComponent>(BookSupport);
	SupportShape->SetBoxExtent(FVector(60.0, 60.0, 5.0));
	SupportShape->SetMobility(EComponentMobility::Static);
	SupportShape->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SupportShape->SetCollisionObjectType(ECC_WorldStatic);
	SupportShape->SetCollisionResponseToAllChannels(ECR_Block);
	SupportShape->SetCanEverAffectNavigation(false);
	BookSupport->SetRootComponent(SupportShape);
	SupportShape->SetWorldLocation(FVector(4950.0, 250.0, 55.0));
	SupportShape->RegisterComponent();
	TestTrue(TEXT("Player can pick up written evidence"), Book->TryPickup(ServerPC));
	TestFalse(TEXT("A carried book is not placed for ghost writing"), Book->IsPlacedForWriting());
	TestTrue(TEXT("Carrier can close written evidence"), Book->TrySetOpen(ServerPC, false));
	TestTrue(TEXT("Closing evidence preserves its writing and timestamp"), Book->IsWritten()
		&& Book->WritingStartedAtServerTime == OriginalWritingStart);
	TestTrue(TEXT("Carrier can reopen written evidence"), Book->TrySetOpen(ServerPC, true));
	TestTrue(TEXT("Carrier can put written evidence back on a supported surface"),
		Book->TryPlace(ServerPC, FTransform(FRotator::ZeroRotator, FVector(4950.0, 250.0, 60.3))));
	TestTrue(TEXT("Placement preserves completed writing and its original timestamp"), Book->IsPlacedForWriting()
		&& Book->IsWritten() && Book->WritingStartedAtServerTime == OriginalWritingStart && Book->WritingProgress >= 0.99f);
	if (!TestTrue(TEXT("Owner changes and closing never clear replicated evidence"), Worlds.TickUntil([&]()
	{
		return ClientBook->GetCarrier() == nullptr && ClientBook->IsOpen() && ClientBook->IsWritten()
			&& ClientBook->WritingStartedAtServerTime == OriginalWritingStart && ClientBook->WritingProgress >= 0.99f;
	}, 120))) { return false; }
	TestEqual(TEXT("Server prop ignores player capsules"), Prop->PropMesh->GetCollisionResponseToChannel(ECC_Pawn), ECR_Ignore);
	TestEqual(TEXT("Client prop ignores player capsules"), ClientProp->PropMesh->GetCollisionResponseToChannel(ECC_Pawn), ECR_Ignore);
	TestTrue(TEXT("Invisible ghost can move opted-in prop"), State->MoveNearestProp());
	TestFalse(TEXT("Prop cannot accumulate repeated immediate impulses"), State->ThrowNearestProp());
	TestTrue(TEXT("Accepted movement has nonzero bounded speed"), Prop->PropMesh->GetPhysicsLinearVelocity().Size() > 0.0
		&& Prop->PropMesh->GetPhysicsLinearVelocity().Size() <= Prop->MaximumSpeedCmPerSecond);
	const int32 PreviousRecovery = Prop->BoundaryRecoveryCount;
	Prop->SetActorLocation(FVector(6100.0, 100.0, 60.0), false, nullptr, ETeleportType::TeleportPhysics);
	if (!TestTrue(TEXT("Displaced disturbed prop recovers inside its owning store"), Worlds.TickUntil([&]()
	{
		return Prop->BoundaryRecoveryCount == PreviousRecovery + 1 && UMallGhostInteractionComponent::IsActorInGhostStore(Ghost, Prop);
	}, 60))) { return false; }
	Worlds.TickAll(130);
	TestTrue(TEXT("Invisible ghost can throw after prop cooldown"), State->ThrowNearestProp());
	if (!TestTrue(TEXT("Throw telemetry replicates to client"), Worlds.TickUntil([&]()
	{
		return ClientProp->DisturbanceCount == 2 && ClientProp->bLastDisturbanceWasThrow
			&& ClientState->LastDisturbance == TEXT("throw") && ClientState->DisturbanceCount == 3;
	}, 120))) { return false; }
	TestFalse(TEXT("Invisible ghost remains unable to authorize a catch after disturbances"), UMallGhostInteractionComponent::CanGhostHunt(Ghost));

	Book->ResetWriting();
	State->DisturbanceIntervalSeconds = 1.0f;
	State->SetGhostMode(EMallGhostMode::InvisibleDisturbance);
	State->bAutomaticDisturbances = true;
	Player->SetActorLocation(FVector(3770.0, 0.0, 90.0));
	Player->ForceNetUpdate();
	Worlds.TickAll(85);
	TestFalse(TEXT("Automatic disturbances do not write without a player in the store"), Book->IsWritten());
	TestEqual(TEXT("Automatic disturbances do not act without an eligible player"), State->DisturbanceCount, 3);
	Player->SetActorLocation(FVector(4950.0, 150.0, 90.0));
	Player->ForceNetUpdate();
	if (!TestTrue(TEXT("Automatic scheduler disturbs a loose prop when a possessed player enters"), Worlds.TickUntil([&]()
	{
		return Prop->DisturbanceCount > 2 && State->DisturbanceCount > 3
			&& (State->LastDisturbance == TEXT("move") || State->LastDisturbance == TEXT("throw"));
	}, 90))) { return false; }
	Worlds.TickAll(85);
	TestFalse(TEXT("Player presence alone never automatically writes evidence"), Book->IsWritten());
	State->bAutomaticDisturbances = false;
	const int32 DisturbancesBeforeExplicitWrite = State->DisturbanceCount;
	TestTrue(TEXT("Explicit writing request can produce evidence after automatic prop activity"), State->DisturbNearestBook());
	TestEqual(TEXT("Explicit writing adds exactly one disturbance"), State->DisturbanceCount, DisturbancesBeforeExplicitWrite + 1);
	const int32 ExpectedLateDisturbanceCount = State->DisturbanceCount;
	Worlds.TickAll(50);
	if (!TestTrue(TEXT("Second client can join after the writing is complete"), Worlds.CreateAndConnectClient())) { return false; }
	AMallGhostWritingBook* LateBook = nullptr;
	UMallGhostInteractionComponent* LateState = nullptr;
	if (!TestTrue(TEXT("Late join receives completed writing and invisible mode from persistent state"), Worlds.TickUntil([&]()
	{
		UWorld* LateWorld = Worlds.Clients[1].GetWorld();
		LateBook = First<AMallGhostWritingBook>(LateWorld);
		ACharacter* LateGhost = FindGhost(LateWorld, GhostClass);
		LateState = LateGhost ? LateGhost->FindComponentByClass<UMallGhostInteractionComponent>() : nullptr;
		return LateBook && LateBook->IsWritten() && LateBook->WritingProgress >= 0.99f
			&& LateState && LateState->IsInvisible() && LateGhost->GetMesh()->bHiddenInGame
			&& LateState->DisturbanceCount == ExpectedLateDisturbanceCount;
	}, 180))) { return false; }
	return true;
}

#endif
