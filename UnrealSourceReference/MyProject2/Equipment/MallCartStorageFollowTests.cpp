#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Equipment/MallCartFlashlight.h"
#include "Equipment/MallCartItem.h"
#include "Equipment/MallPushCart.h"
#include "Ghost/MallBookInteractionComponent.h"
#include "Ghost/MallGhostWritingBook.h"
#include "Ghost/MallSpiritBox.h"
#include "Voice/MallVoiceSettings.h"
#include "Components/ActorComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
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

namespace MallCartStorageFollowTest
{
	struct FRestoreSettings
	{
		UMallVoiceSettings* Voice = GetMutableDefault<UMallVoiceSettings>();
		bool Enabled = Voice->bEnabled, Speech = Voice->bSpeechRecognitionEnabled, Status = Voice->bShowVoiceStatus;
		FString Map = UGameMapsSettings::GetGameDefaultMap(EDefaultMapRequestType::Client);
		FString Mode = UGameMapsSettings::GetGlobalDefaultGameMode();
		TSubclassOf<UGameViewportClient> Viewport = GEngine->GameViewportClientClass;
		FRestoreSettings()
		{
			Voice->bEnabled = Voice->bSpeechRecognitionEnabled = Voice->bShowVoiceStatus = false;
			UGameMapsSettings::SetGameDefaultMap(TEXT("/Engine/Maps/Entry"));
			UGameMapsSettings::SetGlobalDefaultGameMode(TEXT("/Script/Engine.GameMode"));
			if (UClass* Compatible = LoadClass<UGameViewportClient>(nullptr, TEXT("/Script/CommonUI.CommonGameViewportClient")))
			{
				GEngine->GameViewportClientClass = Compatible;
			}
		}
		~FRestoreSettings()
		{
			Voice->bEnabled = Enabled; Voice->bSpeechRecognitionEnabled = Speech; Voice->bShowVoiceStatus = Status;
			UGameMapsSettings::SetGameDefaultMap(Map); UGameMapsSettings::SetGlobalDefaultGameMode(Mode);
			GEngine->GameViewportClientClass = Viewport;
		}
	};

	void HoldTestCharacters(UWorld* World)
	{
		for (TActorIterator<ACharacter> It(World); It; ++It)
		{
			It->SetActorTickEnabled(false);
			It->GetCharacterMovement()->SetComponentTickEnabled(false);
			It->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		}
	}

	bool AddAuthorityInventory(ACharacter* Pawn)
	{
		UClass* InventoryClass = LoadClass<UActorComponent>(nullptr, TEXT("/Game/Player/Inventory/BP_InventoryComponent.BP_InventoryComponent_C"));
		if (!Pawn || !InventoryClass) { return false; }
		UActorComponent* Inventory = NewObject<UActorComponent>(Pawn, InventoryClass, TEXT("StorageFollowInventory"));
		Pawn->AddInstanceComponent(Inventory); Inventory->RegisterComponent();
		UFunction* Initialize = Inventory->FindFunction(TEXT("InitializeInventory"));
		if (!Initialize) { return false; }
		Inventory->ProcessEvent(Initialize, nullptr);
		return true;
	}

	AActor* MakeFloor(UWorld* World)
	{
		AActor* Actor = World->SpawnActor<AActor>();
		if (!Actor) { return nullptr; }
		UBoxComponent* Floor = NewObject<UBoxComponent>(Actor);
		Floor->SetMobility(EComponentMobility::Static);
		Floor->InitBoxExtent(FVector(10000, 10000, 50));
		Floor->SetCollisionObjectType(ECC_WorldStatic);
		Floor->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Floor->SetCollisionResponseToAllChannels(ECR_Block);
		Actor->SetRootComponent(Floor); Actor->AddInstanceComponent(Floor);
		Floor->SetWorldLocation(FVector(0, 0, -50)); Floor->RegisterComponent();
		return Actor;
	}

	bool FitsSlot(AActor* Item, AMallPushCart* Cart, int32 Index)
	{
		if (!IsValid(Item) || !IsValid(Cart) || Item->GetAttachParentActor() != Cart
			|| !Item->GetClass()->ImplementsInterface(UMallCartItem::StaticClass())
			|| IMallCartItem::Execute_GetCartStorage(Item) != Cart
			|| IMallCartItem::Execute_GetCartItemCarrier(Item)) { return false; }
		const FTransform Expected = Cart->GetSlotWorldTransform(Index).GetRelativeTransform(Cart->GetActorTransform());
		const FTransform Actual = Item->GetActorTransform().GetRelativeTransform(Cart->GetActorTransform());
		return FVector::Dist(Actual.GetLocation(), Expected.GetLocation()) <= 0.5
			&& Actual.GetRotation().AngularDistance(Expected.GetRotation()) <= FMath::DegreesToRadians(0.5)
			&& Actual.GetScale3D().Equals(Expected.GetScale3D(), 0.001);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMallCartStorageFollowNetworkTest, "Mall.Cart.FPlacementStorageFollowNetwork",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMallCartStorageFollowNetworkTest::RunTest(const FString& Parameters)
{
	using namespace MallCartStorageFollowTest;
	FRestoreSettings Restore;
	UE::Net::FTestWorlds Worlds(TEXT("/Engine/Maps/Entry"), TEXT("/Script/Engine.GameMode"));
	if (!TestTrue(TEXT("Storage-follow authority world loads"), Worlds.Server.IsLoaded())
		|| !TestTrue(TEXT("Actual owner peer connects"), Worlds.CreateAndConnectClient())
		|| !TestTrue(TEXT("Actual observer peer connects"), Worlds.CreateAndConnectClient())) { return false; }
	Worlds.PreTickAllDelegate.AddLambda([&]()
	{
		HoldTestCharacters(Worlds.Server.GetWorld());
		for (UE::Net::FTestWorldInstance& Client : Worlds.Clients) { HoldTestCharacters(Client.GetWorld()); }
	});
	UWorld* Server = Worlds.Server.GetWorld();
	APlayerController* Owner = nullptr; APlayerController* ClientOwner = nullptr;
	UMallBookInteractionComponent* OwnerEvidence = nullptr;
	UMallBookInteractionComponent* ClientEvidence = nullptr;
	if (!TestTrue(TEXT("Replicated owner evidence RPC endpoint exists"), Worlds.TickUntil([&]()
	{
		Owner = Worlds.GetServerPlayerControllerOfClient(0);
		ClientOwner = Worlds.Clients[0].GetWorld()->GetFirstPlayerController();
		OwnerEvidence = Owner ? Owner->FindComponentByClass<UMallBookInteractionComponent>() : nullptr;
		ClientEvidence = ClientOwner ? ClientOwner->FindComponentByClass<UMallBookInteractionComponent>() : nullptr;
		return OwnerEvidence && ClientEvidence;
	}, 180))) { return false; }

	FActorSpawnParameters Spawn;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ACharacter* Player = Server->SpawnActor<ACharacter>(FVector(4880, 0, 90), FRotator::ZeroRotator, Spawn);
	if (!TestNotNull(TEXT("Possessable owner character exists"), Player)
		|| !TestNotNull(TEXT("Static floor supports physical cart motion"), MakeFloor(Server))) { return false; }
	for (UE::Net::FTestWorldInstance& Client : Worlds.Clients)
	{
		if (!TestNotNull(TEXT("Peer floor supports its replicated simulated cart"), MakeFloor(Client.GetWorld()))) { return false; }
	}
	Player->GetCapsuleComponent()->SetCapsuleSize(34, 90);
	Player->SetReplicates(true); Player->SetReplicateMovement(true); Owner->Possess(Player);
	if (!TestTrue(TEXT("Project inventory initializes for the physical flashlight"), AddAuthorityInventory(Player))) { return false; }
	if (!TestTrue(TEXT("Owner pawn is mapped before any client object lookup"), Worlds.TickUntil([&]()
	{
		return Worlds.IsServerObjectReplicated(Player) && ClientOwner->GetPawn()
			&& Worlds.FindReplicatedObjectOnClient(Player, 0) == ClientOwner->GetPawn();
	}, 180))) { return false; }
	AMallPushCart* Cart = Server->SpawnActor<AMallPushCart>(FVector(5000, 0, 45), FRotator::ZeroRotator, Spawn);
	AMallGhostWritingBook* Book = Server->SpawnActor<AMallGhostWritingBook>(FVector(4800, -45, 0.3), FRotator::ZeroRotator, Spawn);
	AMallSpiritBox* Radio = Server->SpawnActor<AMallSpiritBox>(FVector(4800, 45, 0.3), FRotator::ZeroRotator, Spawn);
	AMallCartFlashlight* Flashlight = Server->SpawnActor<AMallCartFlashlight>(FVector(4820, 85, 0.3), FRotator::ZeroRotator, Spawn);
	if (!TestNotNull(TEXT("Cart fixture exists"), Cart) || !TestNotNull(TEXT("Book fixture exists"), Book)
		|| !TestNotNull(TEXT("Radio fixture exists"), Radio) || !TestNotNull(TEXT("Flashlight fixture exists"), Flashlight)) { return false; }
	Book->bWritten = true; Book->WritingStartedAtServerTime = Server->GetTimeSeconds(); Book->TestWriting = TEXT("PERSISTENT TEST EVIDENCE");
	Radio->ResponseCount = 3; Radio->LastPhraseId = TEXT("who_are_you"); Radio->LastSubtitle = TEXT("Persistent radio history");
	Book->ForceNetUpdate(); Radio->ForceNetUpdate();
	AActor* Items[3] = {Book, Radio, Flashlight};
	if (!TestTrue(TEXT("Cart and every item arrive before owner F interaction"), Worlds.TickUntil([&]()
	{
		if (!Worlds.IsServerObjectReplicated(Cart) || !Worlds.FindReplicatedObjectOnClient(Cart, 0)) { return false; }
		for (AActor* Item : Items)
		{
			if (!Worlds.IsServerObjectReplicated(Item) || !Worlds.FindReplicatedObjectOnClient(Item, 0)) { return false; }
		}
		return true;
	}, 180))) { return false; }
	if (!TestTrue(TEXT("Physical cart settles before the owner attempts rack storage"),
		Worlds.TickUntil([&]() { return Cart->IsSettled(); }, 180))) { return false; }
	auto AimOwner = [&](const FVector& Target)
	{
		const FVector Direction = (Target - Player->GetPawnViewLocation()).GetSafeNormal();
		Owner->SetControlRotation(Direction.Rotation()); ClientOwner->SetControlRotation(Direction.Rotation());
		Worlds.TickAll(3);
	};
	AimOwner(Book->GetActorLocation() + FVector(0, 0, 0.7));
	ClientEvidence->RequestInteract();
	if (!TestTrue(TEXT("Actual owner F RPC picks up the written book"), Worlds.TickUntil([&]()
	{
		return Book->GetCarrier() == Player && OwnerEvidence->GetHeldBook() == Book && ClientEvidence->GetHeldBook();
	}, 120))) { return false; }
	Worlds.TickAll(20);
	AimOwner(Cart->GetActorLocation() + FVector(0, 0, 30));
	ClientEvidence->RequestInteract();
	if (!TestTrue(TEXT("Actual owner F RPC stores book on cart rather than fixing it in world space"), Worlds.TickUntil([&]()
	{
		return Book->StoredOnCart == Cart && !Book->GetCarrier() && !OwnerEvidence->GetHeldBook() && !ClientEvidence->GetHeldBook();
	}, 120))) { return false; }
	TestTrue(TEXT("Stored book is closed, written, and unavailable for evidence writing"),
		!Book->IsOpen() && Book->IsWritten() && !Book->IsPlacedForWriting());
	Worlds.TickAll(20);
	AimOwner(Radio->GetActorLocation() + FVector(0, 0, 12));
	ClientEvidence->RequestInteract();
	if (!TestTrue(TEXT("Actual owner F RPC picks up radio after clearing book ownership"), Worlds.TickUntil([&]()
	{
		return Radio->GetCarrier() == Player && OwnerEvidence->GetHeldSpiritBox() == Radio && ClientEvidence->GetHeldSpiritBox();
	}, 120))) { return false; }
	Worlds.TickAll(20);
	TestTrue(TEXT("Carried radio can be switched on before storage"), Radio->TrySetPowered(Owner, true));
	AimOwner(Cart->GetActorLocation() + FVector(0, 0, 30));
	ClientEvidence->RequestInteract();
	if (!TestTrue(TEXT("Actual owner F RPC stores radio in the moving rack"), Worlds.TickUntil([&]()
	{
		return Radio->StoredOnCart == Cart && !Radio->GetCarrier() && !OwnerEvidence->GetHeldSpiritBox() && !ClientEvidence->GetHeldSpiritBox();
	}, 120))) { return false; }
	TestTrue(TEXT("Storage powers off radio without discarding its response history"),
		!Radio->IsPowered() && Radio->ResponseCount == 3 && Radio->LastPhraseId == TEXT("who_are_you"));
	TestTrue(TEXT("Physical flashlight can join the same rack through its existing adapter"),
		Flashlight->TryPickup(Owner) && Cart->TryDeposit(Owner, Flashlight));
	auto EveryCopyFits = [&]()
	{
		for (int32 Index = 0; Index < 3; ++Index) { if (!FitsSlot(Items[Index], Cart, Index)) { return false; } }
		for (int32 Peer = 0; Peer < Worlds.Clients.Num(); ++Peer)
		{
			AMallPushCart* PeerCart = Worlds.IsServerObjectReplicated(Cart) ? Worlds.FindReplicatedObjectOnClient(Cart, Peer) : nullptr;
			if (!PeerCart || FVector::Dist(PeerCart->GetActorLocation(), Cart->GetActorLocation()) > 20.0
				|| PeerCart->GetActorQuat().AngularDistance(Cart->GetActorQuat()) > FMath::DegreesToRadians(5.0)) { return false; }
			for (int32 Index = 0; Index < 3; ++Index)
			{
				AActor* PeerItem = Worlds.IsServerObjectReplicated(Items[Index]) ? Worlds.FindReplicatedObjectOnClient(Items[Index], Peer) : nullptr;
				if (!FitsSlot(PeerItem, PeerCart, Index)
					|| FVector::Dist(PeerItem->GetActorLocation(), Items[Index]->GetActorLocation()) > 20.0
					|| PeerItem->GetActorQuat().AngularDistance(Items[Index]->GetActorQuat()) > FMath::DegreesToRadians(5.0)) { return false; }
			}
		}
		return true;
	};
	if (!TestTrue(TEXT("All three stored item actors attach at rack-relative positions on both peers"),
		Worlds.TickUntil(EveryCopyFits, 180))) { return false; }
	Owner->SetControlRotation(FRotator::ZeroRotator); Player->SetActorRotation(FRotator::ZeroRotator);
	if (!TestTrue(TEXT("Owner can begin a real controlled push with stored items"), Cart->TryBeginPush(Owner))) { return false; }
	const FVector BeforePush = Cart->GetActorLocation();
	for (int32 Step = 0; Step < 45; ++Step)
	{
		Player->SetActorLocation(Player->GetActorLocation() + FVector(2, 0, 0));
		Worlds.TickAll(1);
	}
	TestTrue(TEXT("Controlled push actually translates the cart"), FVector::Dist2D(BeforePush, Cart->GetActorLocation()) > 30.0);
	TestTrue(TEXT("Book, radio and flashlight follow actual pushing on server and peers"), Worlds.TickUntil(EveryCopyFits, 120));
	TestTrue(TEXT("Releasing the cart exits controlled push"), Cart->StopPushing(Owner));
	UPrimitiveComponent* Body = Cast<UPrimitiveComponent>(Cart->GetRootComponent());
	if (!TestNotNull(TEXT("Cart has a primitive physical root"), Body)) { return false; }
	if (!TestTrue(TEXT("Released cart enters real free physics and settles before its impulse"), Worlds.TickUntil([&]() { return Body->IsSimulatingPhysics() && Cart->IsSettled(); }, 240))) { return false; }
	const FVector BeforePhysics = Cart->GetActorLocation();
	const FQuat BeforeRotation = Cart->GetActorQuat();
	Body->AddImpulse(FVector(110, 35, 0), NAME_None, true);
	Body->AddAngularImpulseInDegrees(FVector(0, 0, 35), NAME_None, true);
	Worlds.TickAll(60);
	TestTrue(TEXT("An actual simulated impulse translates the released cart"), FVector::Dist2D(BeforePhysics, Cart->GetActorLocation()) > 5.0);
	TestTrue(TEXT("An actual simulated angular impulse rotates the released cart"), BeforeRotation.AngularDistance(Cart->GetActorQuat()) > FMath::DegreesToRadians(1.0));
	TestTrue(TEXT("Stored items retain relative positions and rotations during free physics"), Worlds.TickUntil(EveryCopyFits, 180));
	Body->SetPhysicsLinearVelocity(FVector::ZeroVector); Body->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	if (!TestTrue(TEXT("Late observer connects after the cart and its contents have moved"), Worlds.CreateAndConnectClient())) { return false; }
	if (!TestNotNull(TEXT("Late observer floor supports its newly replicated simulated cart"), MakeFloor(Worlds.Clients.Last().GetWorld()))) { return false; }
	if (!TestTrue(TEXT("Late observer resolves current cart attachment and every rack-relative item pose"),
		Worlds.TickUntil(EveryCopyFits, 240))) { return false; }
	TestTrue(TEXT("Storage contains exactly one reference to every original actor"), Cart->StoredItems.FilterByPredicate([](const TObjectPtr<AActor>& Item) { return IsValid(Item.Get()); }).Num() == 3
		&& Cart->StoredItems[0] == Book && Cart->StoredItems[1] == Radio && Cart->StoredItems[2] == Flashlight);
	if (!TestTrue(TEXT("Physical cart settles again before retrieval"), Worlds.TickUntil([&]() { return Cart->IsSettled(); }, 180))) { return false; }
	Player->SetActorLocation(Cart->GetActorLocation() + Cart->GetActorQuat().RotateVector(FVector(-120, 0, 45)));
	TestTrue(TEXT("Original book can be retrieved after simulated travel"), Cart->TryRetrieve(Owner, 0));
	TestTrue(TEXT("Retrieval detaches the same written actor and preserves evidence"), Book->GetCarrier() == Player
		&& !Book->StoredOnCart && Book->GetAttachParentActor() != Cart && Book->IsWritten() && Book->TestWriting == TEXT("PERSISTENT TEST EVIDENCE"));
	TestTrue(TEXT("Radio history and stored flashlight remain on their original actors"),
		Radio->ResponseCount == 3 && Radio->LastSubtitle == TEXT("Persistent radio history")
		&& Cart->ContainsStoredItem(Radio) && Cart->ContainsStoredItem(Flashlight));
	return true;
}
#endif
