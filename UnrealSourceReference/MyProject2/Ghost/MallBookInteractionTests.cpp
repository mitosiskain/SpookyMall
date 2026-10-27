#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Ghost/MallBookInteractionComponent.h"
#include "Ghost/MallGhostWritingBook.h"
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
#include "GameFramework/PlayerState.h"
#include "GameMapsSettings.h"
#include "Misc/AutomationTest.h"
#include "Tests/NetTestHelpers.h"

namespace MallBookInteractionTest
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

	void FreezeCharacters(UWorld* World)
	{
		for (TActorIterator<ACharacter> It(World); It; ++It)
		{
			It->SetActorTickEnabled(false);
			It->GetCharacterMovement()->DisableMovement();
			It->GetCharacterMovement()->SetComponentTickEnabled(false);
		}
	}

	AMallGhostWritingBook* FirstBook(UWorld* World)
	{
		for (TActorIterator<AMallGhostWritingBook> It(World); It; ++It) { return *It; }
		return nullptr;
	}

	ACharacter* SpawnPlayer(APlayerController* Controller, const FVector& Location)
	{
		FActorSpawnParameters Spawn;
		Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ACharacter* Pawn = Controller->GetWorld()->SpawnActor<ACharacter>(Location, FRotator::ZeroRotator, Spawn);
		if (!Pawn) { return nullptr; }
		Pawn->GetCapsuleComponent()->SetCapsuleSize(34.0f, 90.0f);
		Pawn->SetReplicates(true);
		Pawn->SetReplicateMovement(true);
		Pawn->GetCharacterMovement()->DisableMovement();
		Pawn->GetCharacterMovement()->SetComponentTickEnabled(false);
		Controller->Possess(Pawn);
		Pawn->ForceNetUpdate();
		Controller->ForceNetUpdate();
		return Pawn;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMallBookInteractionTest, "Mall.Ghost.BookInteraction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMallBookInteractionTest::RunTest(const FString& Parameters)
{
	using namespace MallBookInteractionTest;
	FRestoreSettings Restore;
	UE::Net::FTestWorlds Worlds(TEXT("/Engine/Maps/Entry"), TEXT("/Script/Engine.GameMode"));
	if (!TestTrue(TEXT("Temporary authority world loads"), Worlds.Server.IsLoaded())
		|| !TestTrue(TEXT("First actual IP client connects"), Worlds.CreateAndConnectClient())
		|| !TestTrue(TEXT("Second actual IP client connects"), Worlds.CreateAndConnectClient())) { return false; }
	Worlds.PreTickAllDelegate.AddLambda([&Worlds]()
	{
		FreezeCharacters(Worlds.Server.GetWorld());
		for (UE::Net::FTestWorldInstance& Client : Worlds.Clients) { FreezeCharacters(Client.GetWorld()); }
	});
	UWorld* Server = Worlds.Server.GetWorld();
	APlayerController* ServerPC[2] = {};
	APlayerController* ClientPC[2] = {};
	UMallBookInteractionComponent* ServerInteraction[2] = {};
	UMallBookInteractionComponent* ClientInteraction[2] = {};
	if (!TestTrue(TEXT("Voice-independent owner interaction endpoints replicate to both clients"), Worlds.TickUntil([&]()
	{
		for (int32 Index = 0; Index < 2; ++Index)
		{
			ServerPC[Index] = Worlds.GetServerPlayerControllerOfClient(Index);
			ClientPC[Index] = Worlds.Clients[Index].GetWorld()->GetFirstPlayerController();
			ServerInteraction[Index] = ServerPC[Index] ? ServerPC[Index]->FindComponentByClass<UMallBookInteractionComponent>() : nullptr;
			ClientInteraction[Index] = ClientPC[Index] ? ClientPC[Index]->FindComponentByClass<UMallBookInteractionComponent>() : nullptr;
			if (!ServerInteraction[Index] || !ClientInteraction[Index] || !ServerPC[Index]->PlayerState || !ClientPC[Index]->PlayerState) { return false; }
		}
		return true;
	}, 180))) { return false; }
	for (int32 Index = 0; Index < 2; ++Index)
	{
		TestTrue(TEXT("Interaction RPC endpoint is owned by its actual local client controller"), ClientPC[Index]->IsLocalController()
			&& !ClientPC[Index]->HasAuthority() && ServerPC[Index]->HasAuthority());
		TestEqual(TEXT("Owner endpoints refer to the same network player"), ClientPC[Index]->PlayerState->GetPlayerId(),
			ServerPC[Index]->PlayerState->GetPlayerId());
		TestTrue(TEXT("Interaction component replicates independently of voice"), ClientInteraction[Index]->GetIsReplicated());
	}
	ACharacter* Pawn[2] = {
		SpawnPlayer(ServerPC[0], FVector(4800.0, -50.0, 90.0)),
		SpawnPlayer(ServerPC[1], FVector(4800.0, 100.0, 90.0))
	};
	if (!TestNotNull(TEXT("First possessed fixture pawn spawns"), Pawn[0])
		|| !TestNotNull(TEXT("Second possessed fixture pawn spawns"), Pawn[1])) { return false; }

	FActorSpawnParameters Spawn;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* Floor = Server->SpawnActor<AActor>(FVector(4950.0, 0.0, -10.0), FRotator::ZeroRotator, Spawn);
	if (!TestNotNull(TEXT("Static placement surface spawns"), Floor)) { return false; }
	UBoxComponent* FloorShape = NewObject<UBoxComponent>(Floor);
	FloorShape->SetBoxExtent(FVector(500.0, 500.0, 10.0));
	FloorShape->SetMobility(EComponentMobility::Static);
	FloorShape->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	FloorShape->SetCollisionObjectType(ECC_WorldStatic);
	FloorShape->SetCollisionResponseToAllChannels(ECR_Block);
	FloorShape->SetCanEverAffectNavigation(false);
	Floor->SetRootComponent(FloorShape);
	FloorShape->SetWorldLocation(FVector(4950.0, 0.0, -10.0));
	FloorShape->RegisterComponent();
	AMallGhostWritingBook* Book = Server->SpawnActor<AMallGhostWritingBook>(FVector(4950.0, 0.0, 0.3), FRotator::ZeroRotator, Spawn);
	if (!TestNotNull(TEXT("Closed evidence book spawns"), Book)) { return false; }
	AMallGhostWritingBook* ClientBook[2] = {};
	if (!TestTrue(TEXT("Book and possessed pawn references arrive on both clients"), Worlds.TickUntil([&]()
	{
		for (int32 Index = 0; Index < 2; ++Index)
		{
			ClientBook[Index] = FirstBook(Worlds.Clients[Index].GetWorld());
			if (!ClientBook[Index] || !ClientPC[Index]->GetPawn()) { return false; }
		}
		return true;
	}, 180))) { return false; }
	TestFalse(TEXT("New book is neither open nor written nor carried"), Book->IsOpen() || Book->IsWritten() || Book->GetCarrier() != nullptr);
	TestFalse(TEXT("Client cannot bypass authority with a direct pickup"), ClientBook[0]->TryPickup(ClientPC[0]));
	TestFalse(TEXT("Client cannot bypass authority with a direct open"), ClientBook[0]->TrySetOpen(ClientPC[0], true));
	TestFalse(TEXT("An uncarried book cannot accept a placement from a controller"),
		Book->TryPlace(ServerPC[0], FTransform(FRotator::ZeroRotator, FVector(4900.0, -50.0, 0.3))));

	// Call the real private owner RPC rather than its implementation: these requests cross
	// the actual IP connection, while the fixture supplies deterministic valid camera poses.
	auto SendRay = [&](const int32 Index, const FVector& Target)
	{
		const FVector Origin = Pawn[Index]->GetPawnViewLocation();
		const FVector Direction = (Target - Origin).GetSafeNormal();
		ServerPC[Index]->SetControlRotation(Direction.Rotation());
		ClientPC[Index]->SetControlRotation(Direction.Rotation());
		ClientInteraction[Index]->ServerInteract(Origin, Direction);
	};
	// The forged ray would still hit the nearby book and satisfy the pawn reach check;
	// rejection therefore exercises the camera-origin guard, rather than just a missed ray.
	const FVector ForgedOrigin(5650.0, -50.0, 154.0);
	const FVector ForgedDirection = (Book->GetActorLocation() + FVector(0.0, 0.0, 1.0) - ForgedOrigin).GetSafeNormal();
	ServerPC[0]->SetControlRotation(ForgedDirection.Rotation());
	ClientPC[0]->SetControlRotation(ForgedDirection.Rotation());
	ClientInteraction[0]->ServerInteract(ForgedOrigin, ForgedDirection);
	Worlds.TickAll(25);
	TestTrue(TEXT("Forged camera origin cannot pick up a remote book"), Book->GetCarrier() == nullptr
		&& ServerInteraction[0]->GetHeldBook() == nullptr);
	const FVector ValidPickupDirection = (Book->GetActorLocation() + FVector(0.0, 0.0, 1.0) - Pawn[0]->GetPawnViewLocation()).GetSafeNormal();
	ServerPC[0]->SetControlRotation((-ValidPickupDirection).Rotation());
	ClientPC[0]->SetControlRotation((-ValidPickupDirection).Rotation());
	ClientInteraction[0]->ServerInteract(Pawn[0]->GetPawnViewLocation(), ValidPickupDirection);
	Worlds.TickAll(25);
	TestTrue(TEXT("A ray opposite the controller's view cannot pick up a book"), Book->GetCarrier() == nullptr);
	SendRay(0, Book->GetActorLocation() + FVector(0.0, 0.0, 1.0));
	if (!TestTrue(TEXT("Actual owning client RPC picks up a nearby book"), Worlds.TickUntil([&]()
	{
		return Book->GetCarrier() == Pawn[0] && ServerInteraction[0]->GetHeldBook() == Book
			&& ClientInteraction[0]->GetHeldBook() == ClientBook[0]
			&& ClientBook[0]->GetCarrier() == ClientPC[0]->GetPawn();
	}, 120))) { return false; }
	TestTrue(TEXT("Other player does not acquire the carrier's held-book endpoint"), ClientInteraction[1]->GetHeldBook() == nullptr
		&& ServerInteraction[1]->GetHeldBook() == nullptr);
	TestFalse(TEXT("Competing authority-side controller cannot take a carried book"), Book->TryPickup(ServerPC[1]));
	TestFalse(TEXT("Competing controller cannot open someone else's carried book"), Book->TrySetOpen(ServerPC[1], true));
	Worlds.TickAll(25);
	SendRay(1, Book->GetActorLocation() + FVector(0.0, 0.0, 1.0));
	Worlds.TickAll(25);
	TestTrue(TEXT("Competing actual client RPC leaves the original carrier intact"), Book->GetCarrier() == Pawn[0]
		&& ServerInteraction[1]->GetHeldBook() == nullptr && ClientInteraction[1]->GetHeldBook() == nullptr);
	ClientInteraction[0]->RequestToggleOpen();
	if (!TestTrue(TEXT("Carrier's public toggle request opens the replicated held book"), Worlds.TickUntil([&]()
	{
		return Book->IsOpen() && ClientBook[0]->IsOpen() && ClientBook[1]->IsOpen();
	}, 120))) { return false; }
	TestFalse(TEXT("Open carried book remains unavailable for writing"), Book->IsPlacedForWriting());
	Worlds.TickAll(25);
	ClientInteraction[1]->RequestToggleOpen();
	Worlds.TickAll(25);
	TestTrue(TEXT("Non-carrier toggle cannot alter another player's book"), Book->IsOpen());
	TestFalse(TEXT("Carrying and opening does not create evidence"), Book->IsWritten());
	TestFalse(TEXT("Carrier cannot place a book on a remote surface"),
		Book->TryPlace(ServerPC[0], FTransform(FRotator::ZeroRotator, FVector(8000.0, 0.0, 0.3))));
	SendRay(0, FVector(4900.0, -50.0, 700.0));
	Worlds.TickAll(25);
	TestTrue(TEXT("A real owner RPC aimed at unsupported empty space leaves the book carried"), Book->GetCarrier() == Pawn[0]
		&& ServerInteraction[0]->GetHeldBook() == Book);

	if (!TestTrue(TEXT("A third actual client joins while the book is carried"), Worlds.CreateAndConnectClient())) { return false; }
	AMallGhostWritingBook* LateBook = nullptr;
	if (!TestTrue(TEXT("Late join reconstructs the open book's original carrier"), Worlds.TickUntil([&]()
	{
		LateBook = FirstBook(Worlds.Clients[2].GetWorld());
		const ACharacter* Carrier = LateBook ? Cast<ACharacter>(LateBook->GetCarrier()) : nullptr;
		return LateBook && LateBook->IsOpen() && !LateBook->IsWritten() && Carrier && Carrier->GetPlayerState()
			&& Carrier->GetPlayerState()->GetPlayerId() == ServerPC[0]->PlayerState->GetPlayerId();
	}, 180))) { return false; }
	Worlds.TickAll(25);
	SendRay(0, FVector(4900.0, -150.0, 0.0));
	if (!TestTrue(TEXT("Carrier's next actual client RPC places the book on a supported static surface"), Worlds.TickUntil([&]()
	{
		return Book->GetCarrier() == nullptr && ServerInteraction[0]->GetHeldBook() == nullptr
			&& ClientInteraction[0]->GetHeldBook() == nullptr && ClientBook[0]->GetCarrier() == nullptr
			&& ClientBook[1]->GetCarrier() == nullptr && LateBook->GetCarrier() == nullptr;
	}, 120))) { return false; }
	TestTrue(TEXT("Successful placement retains the open state and enables writing"), Book->IsOpen() && Book->IsPlacedForWriting());
	TestFalse(TEXT("Pickup, toggle, late join and placement all preserve blank evidence"), Book->IsWritten());
	TestTrue(TEXT("Placed book rests on the validated surface"), FMath::Abs(Book->GetActorLocation().Z - 0.3) <= 2.0
		&& FVector::Dist2D(Book->GetActorLocation(), FVector(4900.0, -150.0, 0.0)) <= 5.0);
	return true;
}

#endif
