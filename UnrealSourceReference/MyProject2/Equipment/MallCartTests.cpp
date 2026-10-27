#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Equipment/MallPushCart.h"
#include "Equipment/MallCartItem.h"
#include "Equipment/MallCartInteractionComponent.h"
#include "Ghost/MallBookInteractionComponent.h"
#include "Ghost/MallGhostWritingBook.h"
#include "Ghost/MallSpiritBox.h"
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
#include "Misc/AutomationTest.h"
#include "Tests/NetTestHelpers.h"

namespace MallCartTest
{
	struct FRestore
	{
		UMallVoiceSettings* Voice=GetMutableDefault<UMallVoiceSettings>();
		bool Enabled=Voice->bEnabled, Speech=Voice->bSpeechRecognitionEnabled, Status=Voice->bShowVoiceStatus;
		FString Map=UGameMapsSettings::GetGameDefaultMap(EDefaultMapRequestType::Client);
		FString Mode=UGameMapsSettings::GetGlobalDefaultGameMode();
		TSubclassOf<UGameViewportClient> Viewport=GEngine->GameViewportClientClass;
		FRestore()
		{
			Voice->bEnabled=Voice->bSpeechRecognitionEnabled=Voice->bShowVoiceStatus=false;
			UGameMapsSettings::SetGameDefaultMap(TEXT("/Engine/Maps/Entry"));
			UGameMapsSettings::SetGlobalDefaultGameMode(TEXT("/Script/Engine.GameMode"));
			if (UClass* Compatible=LoadClass<UGameViewportClient>(nullptr,TEXT("/Script/CommonUI.CommonGameViewportClient"))) { GEngine->GameViewportClientClass=Compatible; }
		}
		~FRestore()
		{
			Voice->bEnabled=Enabled; Voice->bSpeechRecognitionEnabled=Speech; Voice->bShowVoiceStatus=Status;
			UGameMapsSettings::SetGameDefaultMap(Map); UGameMapsSettings::SetGlobalDefaultGameMode(Mode);
			GEngine->GameViewportClientClass=Viewport;
		}
	};
	void FreezeCartTestActors(UWorld* World)
	{
		for (TActorIterator<ACharacter> It(World); It; ++It)
		{
			It->SetActorTickEnabled(false);
			It->GetCharacterMovement()->SetComponentTickEnabled(false);
		}
	}
	AActor* Box(UWorld* World,FVector Position,FVector Extent)
	{
		FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* Actor=World->SpawnActor<AActor>(Position,FRotator::ZeroRotator,Spawn);
		if (!Actor) { return nullptr; }
		UBoxComponent* Shape=NewObject<UBoxComponent>(Actor);
		Shape->InitBoxExtent(Extent);
		Shape->SetMobility(EComponentMobility::Static);
		Shape->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Shape->SetCollisionObjectType(ECC_WorldStatic);
		Shape->SetCollisionResponseToAllChannels(ECR_Block);
		Shape->SetCanEverAffectNavigation(false);
		Actor->SetRootComponent(Shape);
		Shape->SetWorldLocation(Position);
		Shape->RegisterComponent();
		return Actor;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMallCartMovementStorageNetworkTest,"Mall.Cart.MovementStorageNetwork",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)

bool FMallCartMovementStorageNetworkTest::RunTest(const FString& Parameters)
{
	using namespace MallCartTest;
	FRestore Restore;
	UE::Net::FTestWorlds Worlds(TEXT("/Engine/Maps/Entry"),TEXT("/Script/Engine.GameMode"));
	if (!TestTrue(TEXT("Server fixture loads"),Worlds.Server.IsLoaded())
		|| !TestTrue(TEXT("First IP peer connects"),Worlds.CreateAndConnectClient())
		|| !TestTrue(TEXT("Second IP peer connects"),Worlds.CreateAndConnectClient())) { return false; }
	Worlds.PreTickAllDelegate.AddLambda([&]()
	{
		FreezeCartTestActors(Worlds.Server.GetWorld());
		for (UE::Net::FTestWorldInstance& Client:Worlds.Clients) { FreezeCartTestActors(Client.GetWorld()); }
	});
	UWorld* Server=Worlds.Server.GetWorld();
	APlayerController* PC[2]={};
	APlayerController* ClientPC[2]={};
	UMallCartInteractionComponent* Endpoint[2]={};
	UMallCartInteractionComponent* ClientEndpoint[2]={};
	if (!TestTrue(TEXT("Cart endpoints replicate independently of voice"),Worlds.TickUntil([&]()
	{
		for (int32 I=0; I<2; ++I)
		{
			PC[I]=Worlds.GetServerPlayerControllerOfClient(I);
			ClientPC[I]=Worlds.Clients[I].GetWorld()->GetFirstPlayerController();
			Endpoint[I]=PC[I]?PC[I]->FindComponentByClass<UMallCartInteractionComponent>():nullptr;
			ClientEndpoint[I]=ClientPC[I]?ClientPC[I]->FindComponentByClass<UMallCartInteractionComponent>():nullptr;
			if (!Endpoint[I] || !ClientEndpoint[I] || !PC[I]->FindComponentByClass<UMallBookInteractionComponent>()) { return false; }
		}
		return true;
	},180))) { return false; }
	FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ACharacter* Pawn[2]={};
	for (int32 I=0; I<2; ++I)
	{
		Pawn[I]=Server->SpawnActor<ACharacter>(FVector(4880,I*120,90),FRotator::ZeroRotator,Spawn);
		if (!TestNotNull(TEXT("Possessable player fixture exists"),Pawn[I])) { return false; }
		Pawn[I]->GetCapsuleComponent()->SetCapsuleSize(34,90);
		Pawn[I]->SetReplicates(true); Pawn[I]->SetReplicateMovement(true);
		Pawn[I]->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		Pawn[I]->GetCharacterMovement()->MaxWalkSpeed=600;
		Pawn[I]->GetCharacterMovement()->MaxWalkSpeedCrouched=300;
		PC[I]->Possess(Pawn[I]);
		PC[I]->SetControlRotation(FRotator::ZeroRotator);
	}
	AActor* Floor=Box(Server,FVector(5000,0,-10),FVector(1200,1200,10));
	AMallPushCart* Cart=Server->SpawnActor<AMallPushCart>(FVector(5000,0,45),FRotator::ZeroRotator,Spawn);
	if (!TestNotNull(TEXT("Cart floor exists"),Floor) || !TestNotNull(TEXT("Actual native cart exists"),Cart)) { return false; }
	Cart->SetFreePhysicsEnabled(false); // This fixture isolates controlled movement and persistent storage.
	AMallPushCart* ClientCart[2]={};
	if (!TestTrue(TEXT("Cart and possession arrive on both peers"),Worlds.TickUntil([&]()
	{
		if (!Worlds.IsServerObjectReplicated(Cart)) { return false; }
		for (int32 I=0; I<2; ++I)
		{
			ClientCart[I]=Worlds.FindReplicatedObjectOnClient(Cart,I);
			if (!ClientCart[I] || !ClientPC[I]->GetPawn()) { return false; }
		}
		return true;
	},180))) { return false; }
	TestFalse(TEXT("A client cannot mutate push state directly"),ClientCart[0]->TryBeginPush(ClientPC[0]));
	TestFalse(TEXT("Distant player cannot claim a cart"),[&](){ const FVector Before=Pawn[1]->GetActorLocation(); Pawn[1]->SetActorLocation(FVector(4300,0,90)); const bool Result=Cart->TryBeginPush(PC[1]); Pawn[1]->SetActorLocation(Before); return Result; }());
	auto Send=[&](int32 I,uint8 Action,FVector Origin,FVector Direction,int32 Slot=-1)
	{
		PC[I]->SetControlRotation(Direction.Rotation());
		ClientPC[I]->SetControlRotation(Direction.Rotation());
		ClientEndpoint[I]->ServerCartAction(Action,Origin,Direction,Slot);
	};
	const FVector Eye=Pawn[0]->GetPawnViewLocation();
	const FVector Forged=Eye+FVector(760,0,0);
	Send(0,0,Forged,(Cart->GetActorLocation()-Forged).GetSafeNormal());
	Worlds.TickAll(25);
	TestTrue(TEXT("Forged camera cannot start pushing"),Cart->IsParked());
	const FVector HandleAim = Cart->GetActorLocation()+FVector(0,0,85);
	Send(0,0,Eye,(HandleAim-Eye).GetSafeNormal());
	if (!TestTrue(TEXT("Real owner RPC claims cart and replicates its pusher"),Worlds.TickUntil([&]()
	{
		return Cart->GetPusher()==Pawn[0] && ClientCart[0]->GetPusher()==ClientPC[0]->GetPawn()
			&& ClientEndpoint[0]->GetActiveCart()==ClientCart[0];
	},120))) { return false; }
	TestFalse(TEXT("Second player cannot steal a cart"),Cart->TryBeginPush(PC[1]));
	TestFalse(TEXT("Another player cannot release its pusher"),Cart->StopPushing(PC[1]));
	TestEqual(TEXT("Pusher uses configurable slower speed"),Pawn[0]->GetCharacterMovement()->MaxWalkSpeed,200.0f);
	Worlds.TickAll(30);
	TestEqual(TEXT("Repeated ticks retain speed limit"),Pawn[0]->GetCharacterMovement()->MaxWalkSpeed,200.0f);
	TestEqual(TEXT("Owner receives the same prediction speed"),Cast<ACharacter>(ClientPC[0]->GetPawn())->GetCharacterMovement()->MaxWalkSpeed,200.0f);
	const double InitialX=Cart->GetActorLocation().X;
	for (int32 I=0; I<10; ++I) { Pawn[0]->AddActorWorldOffset(FVector(2,0,0)); Worlds.TickAll(2); }
	TestTrue(TEXT("Cart follows forward movement on supported floor"),Cart->GetActorLocation().X>InitialX+12);
	{
		FHitResult PushSweepHit;
		Pawn[0]->GetCapsuleComponent()->IgnoreActorWhenMoving(Floor, true);
		Pawn[0]->AddActorWorldOffset(FVector(40,0,0), true, &PushSweepHit);
		Pawn[0]->GetCapsuleComponent()->IgnoreActorWhenMoving(Floor, false);
		TestFalse(TEXT("Swept forward character movement is not blocked by pushed cart"), PushSweepHit.bBlockingHit && PushSweepHit.GetActor() == Cart);
		Pawn[0]->AddActorWorldOffset(FVector(-40,0,0));
	}
	const double WallMin=Cart->GetActorLocation().X+90;
	AActor* Wall=Box(Server,FVector(WallMin+10,0,90),FVector(10,300,90));
	if (!TestNotNull(TEXT("Blocking wall exists"),Wall)) { return false; }
	for (int32 I=0; I<25; ++I)
	{
		Pawn[0]->AddActorWorldOffset(FVector(2,0,0)); Worlds.TickAll(2);
		if (Cart->GetActorLocation().X+55>=WallMin-.5) { break; }
	}
	TestTrue(TEXT("Swept cart cannot cross a blocking wall"),Cart->GetActorLocation().X+55<=WallMin+.1);
	TestTrue(TEXT("Wall collision stops movement velocity"),Pawn[0]->GetVelocity().IsNearlyZero());
	Wall->Destroy();
	Send(0,0,Pawn[0]->GetPawnViewLocation(),FVector::ForwardVector);
	if (!TestTrue(TEXT("Owner can park even looking away"),Worlds.TickUntil([&](){ return Cart->IsParked(); },120))) { return false; }
	TestEqual(TEXT("Releasing restores original walk speed"),Pawn[0]->GetCharacterMovement()->MaxWalkSpeed,600.0f);
	TestEqual(TEXT("Releasing restores crouched speed"),Pawn[0]->GetCharacterMovement()->MaxWalkSpeedCrouched,300.0f);
	Pawn[0]->SetActorLocation(Cart->GetActorLocation()+FVector(-120,0,45));
	TestTrue(TEXT("Cart can be pushed again"),Cart->TryBeginPush(PC[0]));
	Pawn[0]->GetCharacterMovement()->MaxWalkSpeed=125;
	Worlds.TickAll(2);
	Cart->StopPushing(PC[0]);
	TestEqual(TEXT("An unrelated lower speed survives releasing"),Pawn[0]->GetCharacterMovement()->MaxWalkSpeed,125.0f);
	Pawn[0]->GetCharacterMovement()->MaxWalkSpeed=600;
	TestTrue(TEXT("Third push begins"),Cart->TryBeginPush(PC[0]));
	PC[0]->UnPossess();
	Worlds.TickAll(2);
	TestTrue(TEXT("Possession loss releases cart"),Cart->IsParked());
	TestEqual(TEXT("Possession loss restores movement"),Pawn[0]->GetCharacterMovement()->MaxWalkSpeed,600.0f);
	PC[0]->Possess(Pawn[0]);
	Pawn[1]->SetActorLocation(Cart->GetActorLocation()+FVector(-120,110,45));

	const FVector BookHome=Cart->GetActorLocation()+FVector(-165,45,-44.7);
	AMallGhostWritingBook* Book=Server->SpawnActor<AMallGhostWritingBook>(BookHome,FRotator::ZeroRotator,Spawn);
	if (!TestNotNull(TEXT("Persistent book fixture exists"),Book)) { return false; }
	Book->bWritten=true; // Existing evidence fixture; cart must retain it rather than reset or clone the actor.
	TestTrue(TEXT("Player picks up actual book"),Book->TryPickup(PC[0]));
	TestFalse(TEXT("Foreign carrier cannot deposit another player's book"),Cart->TryDeposit(PC[1],Book));
	TestTrue(TEXT("Carrier deposits actual book"),Cart->TryDeposit(PC[0],Book));
	TestTrue(TEXT("Stored book retains its evidence"),Book->bWritten && !Book->GetCarrier() && IMallCartItem::Execute_GetCartStorage(Book)==Cart);
	TestFalse(TEXT("Stored book cannot be picked up through normal interaction"),Book->TryPickup(PC[1]));
	TestFalse(TEXT("Stored book cannot be opened on the rack"),Book->TrySetOpen(PC[0],true));
	TestFalse(TEXT("Stored book is unavailable for ghost writing"),Book->IsPlacedForWriting());
	if (!TestTrue(TEXT("Both peers receive the same stored actor and writing"),Worlds.TickUntil([&]()
	{
		if (!Worlds.IsServerObjectReplicated(Book)) { return false; }
		for (int32 I=0; I<2; ++I)
		{
			AMallGhostWritingBook* Copy=Worlds.FindReplicatedObjectOnClient(Book,I);
			if (!Copy || !Copy->bWritten || IMallCartItem::Execute_GetCartStorage(Copy)!=ClientCart[I]
				|| !ClientCart[I]->StoredItems.Contains(Copy)) { return false; }
		}
		return true;
	},180))) { return false; }
	TestTrue(TEXT("Third peer can join while equipment is stored"),Worlds.CreateAndConnectClient());
	TestTrue(TEXT("Late join reconstructs persistent cart storage"),Worlds.TickUntil([&]()
	{
		AMallPushCart* CopyCart=Worlds.FindReplicatedObjectOnClient(Cart,2);
		AMallGhostWritingBook* CopyBook=Worlds.FindReplicatedObjectOnClient(Book,2);
		return CopyCart && CopyBook && CopyBook->bWritten && CopyCart->StoredItems.Contains(CopyBook)
			&& IMallCartItem::Execute_GetCartStorage(CopyBook)==CopyCart;
	},180));
	TestFalse(TEXT("Bad storage index is rejected"),Cart->TryRetrieve(PC[1],99));
	TestTrue(TEXT("Another player can retrieve the same shared book"),Cart->TryRetrieve(PC[1],0));
	TestTrue(TEXT("Retrieval adopts existing book interaction controls"),PC[1]->FindComponentByClass<UMallBookInteractionComponent>()->GetHeldBook()==Book);
	TestTrue(TEXT("Retrieved evidence persists and can be inspected"),Book->bWritten && Book->TrySetOpen(PC[1],true));
	TestTrue(TEXT("Book can be stored again"),Cart->TryDeposit(PC[1],Book));
	Worlds.TickAll(25);
	Cart->StorageCapacity=1;
	const FVector RadioHome=Cart->GetActorLocation()+FVector(-165,-45,-44.7);
	AMallSpiritBox* Radio=Server->SpawnActor<AMallSpiritBox>(RadioHome,FRotator::ZeroRotator,Spawn);
	if (!TestNotNull(TEXT("Persistent radio fixture exists"),Radio)) { return false; }
	Radio->ResponseCount=7;
	TestTrue(TEXT("Player picks up radio"),Radio->TryPickup(PC[0]));
	TestTrue(TEXT("Radio can be powered before storage"),Radio->TrySetPowered(PC[0],true));
	TestFalse(TEXT("A full cart rejects without losing the held actor"),Cart->TryDeposit(PC[0],Radio));
	TestTrue(TEXT("Rejected radio remains held and powered"),Radio->GetCarrier()==Pawn[0] && Radio->IsPowered());
	Cart->StorageCapacity=2;
	TestTrue(TEXT("Increasing capacity allows the existing radio"),Cart->TryDeposit(PC[0],Radio));
	TestTrue(TEXT("Storage switches radio off without clearing history"),!Radio->IsPowered() && Radio->ResponseCount==7);
	TestTrue(TEXT("Stored radio rejects remote power"),!Radio->TrySetPowered(PC[0],true));
	Cart->Destroy();
	TestTrue(TEXT("Destroying cart releases original persistent actors"),IMallCartItem::Execute_GetCartStorage(Book)==nullptr
		&& IMallCartItem::Execute_GetCartStorage(Radio)==nullptr && Book->bWritten && Radio->ResponseCount==7);
	TestTrue(TEXT("Released evidence returns to its supported placement"),Book->GetActorLocation().Equals(BookHome,.1)
		&& Radio->GetActorLocation().Equals(RadioHome,.1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMallCartPhysicsNetworkTest,"Mall.Cart.PhysicsNetwork",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)

bool FMallCartPhysicsNetworkTest::RunTest(const FString& Parameters)
{
	using namespace MallCartTest;
	FRestore Restore;
	UE::Net::FTestWorlds Worlds(TEXT("/Engine/Maps/Entry"),TEXT("/Script/Engine.GameMode"));
	if (!TestTrue(TEXT("Physics server fixture loads"),Worlds.Server.IsLoaded())
		|| !TestTrue(TEXT("Physics owner connects"),Worlds.CreateAndConnectClient())
		|| !TestTrue(TEXT("Physics observer connects"),Worlds.CreateAndConnectClient())) { return false; }
	UWorld* Server=Worlds.Server.GetWorld();
	APlayerController* PC=nullptr;
	APlayerController* OwnerPC=nullptr;
	bool bDriveOwner=false;
	Worlds.PreTickAllDelegate.AddLambda([&]()
	{
		FreezeCartTestActors(Server);
		for (int32 I=0; I<Worlds.Clients.Num(); ++I)
		{
			UWorld* World=Worlds.Clients[I].GetWorld();
			FreezeCartTestActors(World);
			if (I==0 && bDriveOwner && OwnerPC)
			{
				if (ACharacter* Pawn=Cast<ACharacter>(OwnerPC->GetPawn()))
				{
					Pawn->SetActorTickEnabled(true);
					Pawn->GetCharacterMovement()->SetComponentTickEnabled(true);
				}
			}
		}
	});
	if (!TestTrue(TEXT("Physics owner controller exists"),Worlds.TickUntil([&]()
	{
		PC=Worlds.GetServerPlayerControllerOfClient(0);
		OwnerPC=Worlds.Clients[0].GetWorld()->GetFirstPlayerController();
		return PC && OwnerPC;
	},180))) { return false; }
	FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ACharacter* Player=Server->SpawnActor<ACharacter>(FVector(4750,0,90),FRotator::ZeroRotator,Spawn);
	if (!TestNotNull(TEXT("Running player fixture exists"),Player)) { return false; }
	Player->GetCapsuleComponent()->SetCapsuleSize(34,90);
	Player->SetReplicates(true); Player->SetReplicateMovement(true);
	Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	Player->GetCharacterMovement()->MaxWalkSpeed=600;
	PC->Possess(Player);
	PC->SetControlRotation(FRotator::ZeroRotator); OwnerPC->SetControlRotation(FRotator::ZeroRotator);
	// Replicated rigid bodies need the same static floor in every independent test world.
	if (!TestNotNull(TEXT("Authority physics floor exists"),Box(Server,FVector(5000,0,-10),FVector(1800,1000,10)))) { return false; }
	for (UE::Net::FTestWorldInstance& Client:Worlds.Clients)
	{
		if (!TestNotNull(TEXT("Peer physics floor exists"),Box(Client.GetWorld(),FVector(5000,0,-10),FVector(1800,1000,10)))) { return false; }
	}
	AMallPushCart* Cart=Server->SpawnActor<AMallPushCart>(FVector(5000,0,45),FRotator::ZeroRotator,Spawn);
	if (!TestNotNull(TEXT("Free physics cart exists"),Cart)) { return false; }
	UBoxComponent* Body=Cart->FindComponentByClass<UBoxComponent>();
	AMallPushCart* Copy[2]={};
	if (!TestTrue(TEXT("Free body and settled state replicate to both peers"),Worlds.TickUntil([&]()
	{
		if (!Body || !Body->IsSimulatingPhysics() || !Cart->IsSettled() || !Worlds.IsServerObjectReplicated(Cart)) { return false; }
		for (int32 I=0; I<2; ++I)
		{
			Copy[I]=Worlds.FindReplicatedObjectOnClient(Cart,I);
			UBoxComponent* CopyBody=Copy[I]?Copy[I]->FindComponentByClass<UBoxComponent>():nullptr;
			if (!CopyBody || !CopyBody->IsSimulatingPhysics() || !Copy[I]->IsSettled()) { return false; }
		}
		return OwnerPC->GetPawn()!=nullptr;
	},300))) { return false; }
	ACharacter* OwnerPawn=Cast<ACharacter>(OwnerPC->GetPawn());
	if (!TestNotNull(TEXT("Actual autonomous running pawn exists"),OwnerPawn)) { return false; }
	OwnerPawn->GetCapsuleComponent()->SetCapsuleSize(34,90);
	OwnerPawn->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	OwnerPawn->GetCharacterMovement()->MaxWalkSpeed=600;
	const FVector Before=Cart->GetActorLocation();
	bDriveOwner=true;
	// Run real character input and collision through client prediction and ServerMove,
	// rather than injecting a cart impulse as a stand-in for player contact.
	for (int32 Tick=0; Tick<100; ++Tick)
	{
		OwnerPawn->AddMovementInput(FVector::ForwardVector,1.0f);
		Worlds.TickAll();
	}
	bDriveOwner=false;
	TestTrue(TEXT("Running character collision bumps the authoritative physics cart"),Cart->GetActorLocation().X>Before.X+12.0);
	TestTrue(TEXT("Contact motion is limited to configured cart speed"),Body->GetPhysicsLinearVelocity().Size2D()<=Cart->MaxBumpSpeedCmPerSecond+.5);
	TestTrue(TEXT("Real rolling motion removes parked protection state"),!Cart->IsSettled());
	TestTrue(TEXT("Upright body stays near zero pitch and roll"),FMath::Abs(Cart->GetActorRotation().Pitch)<1.0 && FMath::Abs(Cart->GetActorRotation().Roll)<1.0);
	const FVector Away=Cart->GetActorLocation()+FVector(-350,220,46);
	Player->SetActorLocation(Away); Player->ForceNetUpdate();
	OwnerPawn->SetActorLocation(Away);
	Player->GetCharacterMovement()->StopMovementImmediately();
	OwnerPawn->GetCharacterMovement()->StopMovementImmediately();
	TestTrue(TEXT("Released cart settles and peers converge after physical contact"),Worlds.TickUntil([&]()
	{
		return Cart->IsSettled() && Copy[0]->IsSettled() && Copy[1]->IsSettled()
			&& FVector::Dist(Copy[0]->GetActorLocation(),Cart->GetActorLocation())<20
			&& FVector::Dist(Copy[1]->GetActorLocation(),Cart->GetActorLocation())<20;
	},360));
	Body->WakeAllRigidBodies();
	Body->SetPhysicsLinearVelocity(FVector(2,0,0));
	Worlds.TickAll(2);
	TestTrue(TEXT("Tiny resting contact jitter does not toggle parked state"),Cart->IsSettled());
	Body->AddImpulse(FVector(8000,0,0));
	Worlds.TickAll(2);
	TestTrue(TEXT("A new substantial physical bump clears settled state"),!Cart->IsSettled());
	Body->AddAngularImpulseInDegrees(FVector(4000,4000,2000),NAME_None,true);
	Worlds.TickAll(5);
	TestTrue(TEXT("Physics locks reject pitch and roll impulses"),FMath::Abs(Cart->GetActorRotation().Pitch)<1 && FMath::Abs(Cart->GetActorRotation().Roll)<1);
	Body->SetPhysicsLinearVelocity(FVector(10000,0,0));
	Worlds.TickAll();
	TestTrue(TEXT("Extreme external velocity is clamped before continued free movement"),Body->GetPhysicsLinearVelocity().Size2D()<=Cart->MaxBumpSpeedCmPerSecond+.5);
	Body->SetPhysicsLinearVelocity(FVector::ZeroVector);
	Body->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	Cart->SetActorRotation(FRotator::ZeroRotator,ETeleportType::TeleportPhysics);
	Player->SetActorLocation(Cart->GetActorLocation()+FVector(-120,0,46));
	Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	if (!TestTrue(TEXT("G can recover a free cart without requiring it to sleep first"),Cart->TryBeginPush(PC))) { return false; }
	TestFalse(TEXT("Controlled pushing disables server simulation"),Body->IsSimulatingPhysics());
	TestTrue(TEXT("Controlled mode reaches remote clients"),Worlds.TickUntil([&]()
	{
		return Copy[0]->GetPusher()==OwnerPC->GetPawn() && !Copy[0]->FindComponentByClass<UBoxComponent>()->IsSimulatingPhysics()
			&& Copy[1]->IsBeingPushed() && !Copy[1]->FindComponentByClass<UBoxComponent>()->IsSimulatingPhysics();
	},180));
	TestTrue(TEXT("Releasing controlled push restores free simulation"),Cart->StopPushing(PC) && Body->IsSimulatingPhysics());
	TestEqual(TEXT("Physics recovery restores player's normal speed"),Player->GetCharacterMovement()->MaxWalkSpeed,600.0f);
	TestTrue(TEXT("Cart settles again after controlled release"),Worlds.TickUntil([&](){ return Cart->IsSettled(); },300));
	const FVector Origin=Player->GetPawnViewLocation();
	const FVector BroadTarget=Cart->GetActorLocation()+FVector(0,0,85);
	const FVector Direction=(BroadTarget-Origin).GetSafeNormal();
	TestTrue(TEXT("Broad focus reaches a handle region above the narrow collision body"),AMallPushCart::FindCartInView(Player,Origin,Direction)==Cart);
	AActor* Wall=Box(Server,Player->GetActorLocation()+FVector(65,0,0),FVector(5,150,150));
	TestTrue(TEXT("The larger grab region still rejects a cart behind a wall"),Wall && !AMallPushCart::FindCartInView(Player,Origin,Direction));
	return true;
}

#endif
