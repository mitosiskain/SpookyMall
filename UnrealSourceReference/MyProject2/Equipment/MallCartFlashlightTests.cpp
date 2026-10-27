#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Equipment/MallCartFlashlight.h"
#include "Equipment/MallCartInteractionComponent.h"
#include "Equipment/MallPushCart.h"
#include "Ghost/MallGhostWritingBook.h"
#include "Ghost/MallBookInteractionComponent.h"
#include "Ghost/MallSpiritBox.h"
#include "Voice/MallVoiceSettings.h"
#include "Components/ActorComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SpotLightComponent.h"
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

namespace MallFlashlightTest
{
	struct FRestore
	{
		UMallVoiceSettings* Voice=GetMutableDefault<UMallVoiceSettings>();
		bool Enabled=Voice->bEnabled, Speech=Voice->bSpeechRecognitionEnabled, Status=Voice->bShowVoiceStatus;
		FString Map=UGameMapsSettings::GetGameDefaultMap(EDefaultMapRequestType::Client), Mode=UGameMapsSettings::GetGlobalDefaultGameMode();
		TSubclassOf<UGameViewportClient> Viewport=GEngine->GameViewportClientClass;
		FRestore()
		{
			Voice->bEnabled=Voice->bSpeechRecognitionEnabled=Voice->bShowVoiceStatus=false;
			UGameMapsSettings::SetGameDefaultMap(TEXT("/Engine/Maps/Entry")); UGameMapsSettings::SetGlobalDefaultGameMode(TEXT("/Script/Engine.GameMode"));
			if (UClass* Compatible=LoadClass<UGameViewportClient>(nullptr,TEXT("/Script/CommonUI.CommonGameViewportClient"))) { GEngine->GameViewportClientClass=Compatible; }
		}
		~FRestore()
		{
			Voice->bEnabled=Enabled; Voice->bSpeechRecognitionEnabled=Speech; Voice->bShowVoiceStatus=Status;
			UGameMapsSettings::SetGameDefaultMap(Map); UGameMapsSettings::SetGlobalDefaultGameMode(Mode); GEngine->GameViewportClientClass=Viewport;
		}
	};
	void FreezeCartTestActors(UWorld* World)
	{
		for (TActorIterator<ACharacter> It(World); It; ++It) { It->SetActorTickEnabled(false); It->GetCharacterMovement()->SetComponentTickEnabled(false); }
	}
	bool AddInventory(ACharacter* Pawn,UClass* InventoryClass)
	{
		if (!Pawn || !InventoryClass) { return false; }
		// Use the project's actual inventory Blueprint, with named presentation parts.
		// A native pawn avoids the unrelated character BeginPlay widget creation in headless worlds.
		UActorComponent* Inventory=NewObject<UActorComponent>(Pawn,InventoryClass,TEXT("Inventory"));
		Pawn->AddInstanceComponent(Inventory); Inventory->RegisterComponent();
		UFunction* Initialize=Inventory->FindFunction(TEXT("InitializeInventory"));
		if (!Initialize) { return false; }
		Inventory->ProcessEvent(Initialize,nullptr);
		UStaticMeshComponent* Mesh=NewObject<UStaticMeshComponent>(Pawn,TEXT("FlashlightMesh"));
		Mesh->SetupAttachment(Pawn->GetRootComponent()); Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); Mesh->SetVisibility(false);
		Pawn->AddInstanceComponent(Mesh); Mesh->RegisterComponent();
		USpotLightComponent* Light=NewObject<USpotLightComponent>(Pawn,TEXT("FlashlightLight"));
		Light->SetupAttachment(Pawn->GetRootComponent()); Light->SetVisibility(false);
		Pawn->AddInstanceComponent(Light); Light->RegisterComponent();
		return true;
	}
	bool SelectInventorySlot(APawn* Pawn,int32 Index)
	{
		if (!Pawn) { return false; }
		TInlineComponentArray<UActorComponent*> Components; Pawn->GetComponents(Components);
		for (UActorComponent* Component:Components)
		{
			if (!Component || Component->GetClass()->GetName()!=TEXT("BP_InventoryComponent_C")) { continue; }
			if (UFunction* Select=Component->FindFunction(TEXT("SelectSlot")))
			{
				struct FSelectParameters { int32 Index; } Parameters{Index};
				Component->ProcessEvent(Select,&Parameters);
				return AMallCartFlashlight::GetSelectedInventorySlot(Pawn)==Index;
			}
		}
		return false;
	}
	USceneComponent* Part(APawn* Pawn,FName Name)
	{
		if (!Pawn) { return nullptr; }
		TInlineComponentArray<USceneComponent*> Parts; Pawn->GetComponents(Parts);
		for (USceneComponent* Component:Parts) { if (Component && Component->GetFName()==Name) { return Component; } }
		return nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMallCartFlashlightNetworkTest,"Mall.Cart.FlashlightNetwork",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)

bool FMallCartFlashlightNetworkTest::RunTest(const FString& Parameters)
{
	using namespace MallFlashlightTest;
	FRestore Restore;
	UE::Net::FTestWorlds Worlds(TEXT("/Engine/Maps/Entry"),TEXT("/Script/Engine.GameMode"));
	if (!TestTrue(TEXT("Fixture server loads"),Worlds.Server.IsLoaded())
		|| !TestTrue(TEXT("Owner peer connects"),Worlds.CreateAndConnectClient())
		|| !TestTrue(TEXT("Observer peer connects"),Worlds.CreateAndConnectClient())) { return false; }
	Worlds.PreTickAllDelegate.AddLambda([&]()
	{
		FreezeCartTestActors(Worlds.Server.GetWorld());
		for (UE::Net::FTestWorldInstance& Client:Worlds.Clients) { FreezeCartTestActors(Client.GetWorld()); }
	});
	UWorld* Server=Worlds.Server.GetWorld();
	APlayerController* PC[2]={}; APlayerController* ClientPC[2]={};
	UMallCartInteractionComponent* Endpoint[2]={};
	if (!TestTrue(TEXT("Owner RPC endpoints exist"),Worlds.TickUntil([&]()
	{
		for (int32 I=0; I<2; ++I)
		{
			PC[I]=Worlds.GetServerPlayerControllerOfClient(I);
			ClientPC[I]=Worlds.Clients[I].GetWorld()->GetFirstPlayerController();
			Endpoint[I]=ClientPC[I]?ClientPC[I]->FindComponentByClass<UMallCartInteractionComponent>():nullptr;
			if (!PC[I] || !Endpoint[I]) { return false; }
		}
		return true;
	},180))) { return false; }
	// This fixture supplies explicit owner input RPCs; its native pawn has no Blueprint power boolean.
	for (UMallCartInteractionComponent* Interaction:Endpoint) { Interaction->SetComponentTickEnabled(false); }
	UClass* InventoryClass=LoadClass<UActorComponent>(nullptr,TEXT("/Game/Player/Inventory/BP_InventoryComponent.BP_InventoryComponent_C"));
	if (!TestNotNull(TEXT("Actual project inventory Blueprint loads"),InventoryClass)) { return false; }
	FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ACharacter* Pawn[2]={};
	for (int32 I=0; I<2; ++I)
	{
		Pawn[I]=Server->SpawnActor<ACharacter>(FVector(4880,I*100,90),FRotator::ZeroRotator,Spawn);
		if (!TestNotNull(TEXT("Player fixture exists"),Pawn[I])) { return false; }
		Pawn[I]->GetCapsuleComponent()->SetCapsuleSize(34,90);
		Pawn[I]->SetReplicates(true); Pawn[I]->SetReplicateMovement(true); PC[I]->Possess(Pawn[I]);
		if (!TestTrue(TEXT("Actual authority inventory initializes"),AddInventory(Pawn[I],InventoryClass))) { return false; }
	}
	if (!TestTrue(TEXT("Both player actors arrive in both peer worlds"),Worlds.TickUntil([&]()
	{
		for (ACharacter* Player : Pawn) { if (!Worlds.IsServerObjectReplicated(Player)) { return false; } }
		for (int32 Client=0; Client<2; ++Client) { for (int32 Player=0; Player<2; ++Player) { if (!Worlds.FindReplicatedObjectOnClient(Pawn[Player],Client)) { return false; } } }
		return true;
	},180))) { return false; }
	for (int32 Client=0; Client<2; ++Client)
	{
		for (int32 Player=0; Player<2; ++Player)
		{
			TestTrue(TEXT("Each pawn copy initializes real Blueprint inventory"),AddInventory(Worlds.FindReplicatedObjectOnClient(Pawn[Player],Client),InventoryClass));
		}
	}
	AMallPushCart* Cart=Server->SpawnActor<AMallPushCart>(FVector(5000,0,45),FRotator::ZeroRotator,Spawn);
	AMallCartFlashlight* Item=Server->SpawnActor<AMallCartFlashlight>(FVector(4810,-45,1),FRotator::ZeroRotator,Spawn);
	if (!TestNotNull(TEXT("Cart exists"),Cart) || !TestNotNull(TEXT("Native world flashlight exists"),Item)) { return false; }
	Cart->SetFreePhysicsEnabled(false);
	AMallCartFlashlight* ClientItem[2]={};
	if (!TestTrue(TEXT("World flashlight replicates"),Worlds.TickUntil([&]()
	{
		if (!Worlds.IsServerObjectReplicated(Item)) { return false; }
		for (int32 I=0; I<2; ++I) { ClientItem[I]=Worlds.FindReplicatedObjectOnClient(Item,I); if (!ClientItem[I]) { return false; } }
		return true;
	},180))) { return false; }
	TestFalse(TEXT("Direct client pickup cannot bypass authority"),ClientItem[0]->TryPickup(ClientPC[0]));
	TestTrue(TEXT("Remote player starts with a different empty hotbar slot selected"),SelectInventorySlot(ClientPC[0]->GetPawn(),2));
	const FVector Eye=Pawn[0]->GetPawnViewLocation();
	const FVector Ray=(Item->GetActorLocation()+FVector(0,0,8)-Eye).GetSafeNormal();
	PC[0]->SetControlRotation(Ray.Rotation()); ClientPC[0]->SetControlRotation(Ray.Rotation());
	Endpoint[0]->ServerCartAction(1,Eye,Ray,-1);
	if (!TestTrue(TEXT("Actual owner RPC picks up flashlight and mirrors hotbar"),Worlds.TickUntil([&]()
	{
		return Item->Carrier==Pawn[0] && Item->IsSelectedByCarrier()
			&& ClientItem[0]->Carrier==ClientPC[0]->GetPawn() && ClientItem[0]->IsSelectedByCarrier()
			&& AMallCartFlashlight::GetSelectedInventorySlot(ClientPC[0]->GetPawn())==0;
	},180))) { return false; }
	TestFalse(TEXT("Second player cannot steal carried flashlight"),Item->TryPickup(PC[1]));
	TestTrue(TEXT("Owner can manually select an empty slot after initial equip"),SelectInventorySlot(ClientPC[0]->GetPawn(),2));
	Endpoint[0]->ServerFlashlightState(2,false);
	TestTrue(TEXT("Manual selection reaches server without being overwritten by mirroring"),Worlds.TickUntil([&]()
	{
		return Item->CarrierSelectedSlot==2 && !Item->IsSelectedByCarrier()
			&& AMallCartFlashlight::GetSelectedInventorySlot(ClientPC[0]->GetPawn())==2;
	},120));
	Worlds.TickAll(20);
	TestEqual(TEXT("Repeated mirror ticks preserve the owner's manual hotbar selection"),
		AMallCartFlashlight::GetSelectedInventorySlot(ClientPC[0]->GetPawn()),2);
	TestTrue(TEXT("Owner can select the flashlight again"),SelectInventorySlot(ClientPC[0]->GetPawn(),0));
	Endpoint[0]->ServerFlashlightState(0,true);
	if (!TestTrue(TEXT("Owner power state replicates to observer with visible hand spotlight"),Worlds.TickUntil([&]()
	{
		APawn* ObserverPawn=Worlds.FindReplicatedObjectOnClient(Pawn[0],1);
		USceneComponent* Light=Part(ObserverPawn,TEXT("FlashlightLight"));
		USceneComponent* Mesh=Part(ObserverPawn,TEXT("FlashlightMesh"));
		return Item->bCarrierLightOn && ClientItem[1]->bCarrierLightOn && Light && Light->IsVisible() && Mesh && Mesh->IsVisible();
	},180))) { return false; }
	Worlds.TickAll(15);
	Endpoint[0]->ServerFlashlightState(0,false);
	TestTrue(TEXT("Power off hides observer spotlight without changing selected slot"),Worlds.TickUntil([&]()
	{
		USceneComponent* Light=Part(Worlds.FindReplicatedObjectOnClient(Pawn[0],1),TEXT("FlashlightLight"));
		return !ClientItem[1]->bCarrierLightOn && Light && !Light->IsVisible();
	},120));
	TestTrue(TEXT("Flashlight can be stored as the same actor"),Cart->TryDeposit(PC[0],Item));
	TestTrue(TEXT("Storage removes hotbar ownership on both peers"),Worlds.TickUntil([&]()
	{
		return !Item->Carrier && Item->StoredOnCart==Cart && !AMallCartFlashlight::FindCarriedBy(ClientPC[0]->GetPawn())
			&& AMallCartFlashlight::GetSelectedInventorySlot(ClientPC[0]->GetPawn())==0
			&& !Part(Worlds.FindReplicatedObjectOnClient(Pawn[0],1),TEXT("FlashlightMesh"))->IsVisible();
	},180));
	TestTrue(TEXT("Second remote player selects a nonzero slot before retrieval"),SelectInventorySlot(ClientPC[1]->GetPawn(),3));
	TestTrue(TEXT("Other player retrieves shared flashlight"),Cart->TryRetrieve(PC[1],0));
	TestTrue(TEXT("New owner receives flashlight inventory and old owner stays without it"),Worlds.TickUntil([&]()
	{
		return Item->Carrier==Pawn[1] && AMallCartFlashlight::FindCarriedBy(ClientPC[1]->GetPawn())==ClientItem[1]
			&& ClientItem[1]->IsSelectedByCarrier() && !AMallCartFlashlight::FindCarriedBy(ClientPC[0]->GetPawn());
	},180));
	AMallCartFlashlight* Replacement=Server->SpawnActor<AMallCartFlashlight>(FVector(4810,-45,1),FRotator::ZeroRotator,Spawn);
	TestTrue(TEXT("Old owner can acquire another real flashlight"),Replacement && Replacement->TryPickup(PC[0]));
	Worlds.TickAll(30);
	TestTrue(TEXT("History on first flashlight cannot erase new owner's different flashlight"),Replacement->IsSelectedByCarrier()
		&& Item->Carrier==Pawn[1] && Item->IsSelectedByCarrier());
	TestFalse(TEXT("One player cannot acquire a second owned flashlight"),Item->TryPickup(PC[0]));
	// Ordinary T must find eligible evidence below a spare lamp the player cannot own.
	AMallGhostWritingBook* Book=Server->SpawnActor<AMallGhostWritingBook>(FVector(4810,45,.3),FRotator::ZeroRotator,Spawn);
	if (!TestNotNull(TEXT("Written evidence book fixture exists"),Book)) { return false; }
	Book->bWritten=true;
	TestTrue(TEXT("Lamp owner can pick up evidence book"),Book->TryPickup(PC[0]));
	TestTrue(TEXT("Written book occupies earlier rack slot"),Cart->TryDeposit(PC[0],Book));
	TestTrue(TEXT("Other player's spare flashlight occupies later slot"),Cart->TryDeposit(PC[1],Item));
	Worlds.TickAll(20);
	TestTrue(TEXT("Explicit spare flashlight request is rejected for existing lamp owner"),!Cart->TryRetrieve(PC[0],1));
	auto TakeByOwnerInput=[&]()
	{
		SelectInventorySlot(ClientPC[0]->GetPawn(),2);
		const FVector Origin=Pawn[0]->GetPawnViewLocation();
		const FVector Direction=(Cart->GetActorLocation()-Origin).GetSafeNormal();
		PC[0]->SetControlRotation(Direction.Rotation()); ClientPC[0]->SetControlRotation(Direction.Rotation());
		Endpoint[0]->ServerCartAction(1,Origin,Direction,2);
	};
	TakeByOwnerInput();
	if (!TestTrue(TEXT("Actual T request skips ineligible spare lamp and retrieves original written book"),Worlds.TickUntil([&]()
	{
		if (!Worlds.IsServerObjectReplicated(Book)) { return false; }
		AMallGhostWritingBook* Copy=Worlds.FindReplicatedObjectOnClient(Book,0);
		UMallBookInteractionComponent* OwnerBook=ClientPC[0]->FindComponentByClass<UMallBookInteractionComponent>();
		return Book->GetCarrier()==Pawn[0] && Book->bWritten && Copy && Copy->GetCarrier()==ClientPC[0]->GetPawn()
			&& OwnerBook && OwnerBook->GetHeldBook()==Copy && Item->StoredOnCart==Cart && Replacement->Carrier==Pawn[0];
	},180))) { return false; }
	AMallPushCart* OtherCart=Server->SpawnActor<AMallPushCart>(FVector(5000,230,45),FRotator::ZeroRotator,Spawn);
	if (!TestNotNull(TEXT("Separate evidence rack fixture exists"),OtherCart)) { return false; }
	OtherCart->SetFreePhysicsEnabled(false);
	TestTrue(TEXT("Written book can be parked on another rack"),OtherCart->TryDeposit(PC[0],Book));
	Worlds.TickAll(20);
	AMallSpiritBox* Radio=Server->SpawnActor<AMallSpiritBox>(FVector(4810,70,.3),FRotator::ZeroRotator,Spawn);
	if (!TestNotNull(TEXT("Radio history fixture exists"),Radio)) { return false; }
	Radio->ResponseCount=9;
	TestTrue(TEXT("Other player takes radio"),Radio->TryPickup(PC[1]));
	TestTrue(TEXT("Radio occupies free slot below spare flashlight"),Cart->TryDeposit(PC[1],Radio));
	TakeByOwnerInput();
	TestTrue(TEXT("Actual T also skips spare lamp for original radio and keeps reply history"),Worlds.TickUntil([&]()
	{
		if (!Worlds.IsServerObjectReplicated(Radio)) { return false; }
		AMallSpiritBox* Copy=Worlds.FindReplicatedObjectOnClient(Radio,0);
		UMallBookInteractionComponent* OwnerBook=ClientPC[0]->FindComponentByClass<UMallBookInteractionComponent>();
		return Radio->GetCarrier()==Pawn[0] && Radio->ResponseCount==9 && Copy && Copy->GetCarrier()==ClientPC[0]->GetPawn()
			&& OwnerBook && OwnerBook->GetHeldSpiritBox()==Copy && Item->StoredOnCart==Cart;
	},180));
	TestTrue(TEXT("Rejected spare remains available to a player without a lamp"),Cart->TryRetrieve(PC[1],1));
	PC[1]->UnPossess(); Worlds.TickAll(15);
	TestTrue(TEXT("Lost possession returns item to world and clears ownership"),!Item->Carrier && !Item->StoredOnCart);
	return true;
}
#endif
