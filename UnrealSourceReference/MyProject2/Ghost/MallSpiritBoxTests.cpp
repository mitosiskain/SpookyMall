#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Ghost/MallSpiritBox.h"
#include "Ghost/MallBookInteractionComponent.h"
#include "Ghost/MallGhostInteractionComponent.h"
#include "Voice/MallVoiceSettings.h"
#include "Components/AudioComponent.h"
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
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "Tests/NetTestHelpers.h"
#include "UObject/UnrealType.h"

namespace MallSpiritBoxTest
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
			// No microphone or recognizer is opened by this network interaction test.
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

	void FreezeCharacters(UWorld* World)
	{
		for (TActorIterator<ACharacter> It(World); It; ++It)
		{
			It->SetActorTickEnabled(false);
			It->GetCharacterMovement()->DisableMovement();
			It->GetCharacterMovement()->SetComponentTickEnabled(false);
		}
	}

	bool SetNumber(AActor* Actor, FName Name, double Value)
	{
		FNumericProperty* Property = FindFProperty<FNumericProperty>(Actor->GetClass(), Name);
		if (!Property || !Property->IsFloatingPoint()) { return false; }
		Property->SetFloatingPointPropertyValue(Property->ContainerPtrToValuePtr<void>(Actor), Value);
		return true;
	}

	ACharacter* SpawnPlayer(APlayerController* PC, FVector Location)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ACharacter* Pawn = PC->GetWorld()->SpawnActor<ACharacter>(Location, FRotator::ZeroRotator, Params);
		if (Pawn)
		{
			Pawn->GetCapsuleComponent()->SetCapsuleSize(34.0f, 90.0f);
			Pawn->SetReplicates(true); Pawn->SetReplicateMovement(true);
			Pawn->GetCharacterMovement()->DisableMovement();
			Pawn->GetCharacterMovement()->SetComponentTickEnabled(false);
			PC->Possess(Pawn); Pawn->ForceNetUpdate(); PC->ForceNetUpdate();
		}
		return Pawn;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMallSpiritBoxNetworkTest, "Mall.Ghost.SpiritBoxNetwork",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMallSpiritBoxNetworkTest::RunTest(const FString& Parameters)
{
	using namespace MallSpiritBoxTest;
	FRestoreSettings Restore;
	UE::Net::FTestWorlds Worlds(TEXT("/Engine/Maps/Entry"), TEXT("/Script/Engine.GameMode"));
	if (!TestTrue(TEXT("Authority world loads"), Worlds.Server.IsLoaded())
		|| !TestTrue(TEXT("First actual IP client connects"), Worlds.CreateAndConnectClient())
		|| !TestTrue(TEXT("Second actual IP client connects"), Worlds.CreateAndConnectClient())) { return false; }
	Worlds.PreTickAllDelegate.AddLambda([&]()
	{
		FreezeCharacters(Worlds.Server.GetWorld());
		for (UE::Net::FTestWorldInstance& Client : Worlds.Clients) { FreezeCharacters(Client.GetWorld()); }
	});
	UWorld* Server = Worlds.Server.GetWorld();
	APlayerController* ServerPC[2] = {};
	APlayerController* ClientPC[2] = {};
	UMallBookInteractionComponent* ServerInteraction[2] = {};
	UMallBookInteractionComponent* ClientInteraction[2] = {};
	if (!TestTrue(TEXT("Voice-independent owner interaction components replicate"), Worlds.TickUntil([&]()
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
		TestTrue(TEXT("Request endpoint is an actual non-authority owning client"), ClientPC[Index]->IsLocalController() && !ClientPC[Index]->HasAuthority());
		TestEqual(TEXT("Owner and authority refer to the same network player"),
			ClientPC[Index]->PlayerState->GetPlayerId(), ServerPC[Index]->PlayerState->GetPlayerId());
	}
	ACharacter* Pawn[2] = {SpawnPlayer(ServerPC[0], FVector(4800, -50, 90)), SpawnPlayer(ServerPC[1], FVector(4800, 150, 90))};
	if (!TestNotNull(TEXT("First fixture pawn spawns"), Pawn[0]) || !TestNotNull(TEXT("Second fixture pawn spawns"), Pawn[1])) { return false; }

	FActorSpawnParameters Spawn;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* Floor = Server->SpawnActor<AActor>(FVector(4950, 0, -10), FRotator::ZeroRotator, Spawn);
	if (!TestNotNull(TEXT("Supported placement floor spawns"), Floor)) { return false; }
	UBoxComponent* Surface = NewObject<UBoxComponent>(Floor);
	Surface->SetBoxExtent(FVector(900, 900, 10)); Surface->SetMobility(EComponentMobility::Static);
	Surface->SetCollisionEnabled(ECollisionEnabled::QueryOnly); Surface->SetCollisionObjectType(ECC_WorldStatic);
	Surface->SetCollisionResponseToAllChannels(ECR_Block); Surface->SetCanEverAffectNavigation(false);
	Floor->SetRootComponent(Surface); Surface->SetWorldLocation(FVector(4950, 0, -10)); Surface->RegisterComponent();
	AMallSpiritBox* Box = Server->SpawnActor<AMallSpiritBox>(FVector(4950, 0, .3), FRotator::ZeroRotator, Spawn);
	UClass* GhostClass = LoadClass<ACharacter>(nullptr, TEXT("/Game/AI/StoreGhost/BP_StoreGhost.BP_StoreGhost_C"));
	if (!TestNotNull(TEXT("Spirit box spawns"), Box) || !TestNotNull(TEXT("Actual store ghost Blueprint loads"), GhostClass)) { return false; }
	const FTransform Home(FRotator::ZeroRotator, FVector(5050, 100, 92.15));
	ACharacter* Ghost = Server->SpawnActorDeferred<ACharacter>(GhostClass, Home, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!TestNotNull(TEXT("Actual store ghost spawns"), Ghost)) { return false; }
	Ghost->Tags.AddUnique(TEXT("StoreGhostPrototype")); Ghost->SetReplicates(true); Ghost->SetReplicateMovement(true);
	const bool bStoreConfigured = SetNumber(Ghost, TEXT("StoreMinX"), 3950) && SetNumber(Ghost, TEXT("StoreMaxX"), 5950)
		&& SetNumber(Ghost, TEXT("StoreMinY"), -816.6666667) && SetNumber(Ghost, TEXT("StoreMaxY"), 816.6666667)
		&& SetNumber(Ghost, TEXT("StoreFloorZ"), 0) && SetNumber(Ghost, TEXT("CooldownUntil"), 0);
	UGameplayStatics::FinishSpawningActor(Ghost, Home); Ghost->SetActorTransform(Home); Ghost->SetActorTickEnabled(false);
	if (!TestTrue(TEXT("Ghost uses the authored ground-floor store contract"), bStoreConfigured)) { return false; }
	UMallGhostInteractionComponent* State = nullptr;
	AMallSpiritBox* Peer[2] = {};
	if (!TestTrue(TEXT("Spirit box, possessed pawns and ghost state reach both peers"), Worlds.TickUntil([&]()
	{
		State = Ghost->FindComponentByClass<UMallGhostInteractionComponent>();
		for (int32 Index = 0; Index < 2; ++Index)
		{
			Peer[Index] = First<AMallSpiritBox>(Worlds.Clients[Index].GetWorld());
			if (!Peer[Index] || !ClientPC[Index]->GetPawn()) { return false; }
		}
		return State != nullptr;
	}, 180))) { return false; }
	State->bAutomaticDisturbances = false;
	State->bAutomaticHunts = false;
	// Camera effect volumes and ghost signal components must not redefine the
	// physical character footprint used to decide whether a radio may answer.
	auto AddOversizedPresentation = [this, Ghost](ACharacter* Character, const TCHAR* Label)
	{
		UBoxComponent* Presentation = NewObject<UBoxComponent>(Character);
		Character->AddInstanceComponent(Presentation);
		Presentation->SetupAttachment(Character->GetRootComponent());
		Presentation->SetBoxExtent(FVector(1010.0));
		Presentation->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Presentation->SetGenerateOverlapEvents(false);
		Presentation->SetCanEverAffectNavigation(false);
		Presentation->SetHiddenInGame(true);
		Presentation->RegisterComponent();
		FVector Origin, Extent;
		Character->GetActorBounds(false, Origin, Extent, true);
		TestTrue(FString::Printf(TEXT("%s presentation bounds extend beyond the store"), Label),
			Origin.Y - Extent.Y < -816.6666667 && Origin.Y + Extent.Y > 816.6666667);
		TestTrue(FString::Printf(TEXT("%s remains in the store when its physical capsule fits"), Label),
			UMallGhostInteractionComponent::IsActorInGhostStore(Ghost, Character));
	};
	AddOversizedPresentation(Pawn[0], TEXT("Speaker"));
	AddOversizedPresentation(Ghost, TEXT("Ghost"));
	const FName Question(TEXT("where_are_you"));
	TestFalse(TEXT("New spirit box is powered off and not carried"), Box->IsPowered() || Box->GetCarrier() != nullptr);
	TestFalse(TEXT("Powered-off spirit box rejects a valid question"), Box->TryRespond(Ghost, ServerPC[0], Question));
	TestFalse(TEXT("Client cannot directly pick up authoritative equipment"), Peer[0]->TryPickup(ClientPC[0]));
	TestFalse(TEXT("Client cannot directly power equipment"), Peer[0]->TrySetPowered(ClientPC[0], true));
	TestEqual(TEXT("Rejected attempts do not create response state"), Box->ResponseCount, 0);
	TMap<FName, FMallSpiritBoxResponse> ExpectedResponses;
	for (const FName Phrase : {Question, FName(TEXT("who_are_you")), FName(TEXT("how_old_are_you"))})
	{
		const FMallSpiritBoxResponse* Entry = Box->Responses.FindByPredicate([Phrase](const FMallSpiritBoxResponse& Response)
		{
			return Response.PhraseId == Phrase && IsValid(Response.Sound) && !Response.Subtitle.IsEmpty();
		});
		if (!TestNotNull(FString::Printf(TEXT("%s has its own editable, playable response"), *Phrase.ToString()), Entry)) { return false; }
		ExpectedResponses.Add(Phrase, *Entry);
	}
	TestTrue(TEXT("The three questions have different response text"),
		ExpectedResponses[Question].Subtitle != ExpectedResponses[FName(TEXT("who_are_you"))].Subtitle
		&& ExpectedResponses[Question].Subtitle != ExpectedResponses[FName(TEXT("how_old_are_you"))].Subtitle
		&& ExpectedResponses[FName(TEXT("who_are_you"))].Subtitle != ExpectedResponses[FName(TEXT("how_old_are_you"))].Subtitle);
	for (AMallSpiritBox* LocalBox : {Box, Peer[0], Peer[1]})
	{
		const FSoundAttenuationSettings* Attenuation = LocalBox->ResponseAudio->GetAttenuationSettingsToApply();
		TestTrue(TEXT("Radio reply allows positional audio and is not a UI sound"),
			LocalBox->ResponseAudio->bAllowSpatialization && !LocalBox->ResponseAudio->bIsUISound);
		TestFalse(TEXT("Reply location follows radio attachment movement"), LocalBox->ResponseAudio->IsUsingAbsoluteLocation());
		if (!TestNotNull(TEXT("Radio reply has attenuation settings"), Attenuation)) { return false; }
		TestTrue(TEXT("Radio reply spatializes even close to the listener"), Attenuation->bSpatialize
			&& Attenuation->NonSpatializedRadiusStart == 0.0f && Attenuation->NonSpatializedRadiusEnd == 0.0f);
		TestTrue(TEXT("Radio replies retain distance attenuation"),
			Attenuation->bAttenuate && Attenuation->AttenuationShape == EAttenuationShape::Sphere
			&& Attenuation->AttenuationShapeExtents.X <= 100.0f
			&& Attenuation->AttenuationShapeExtents.X + Attenuation->FalloffDistance >= 2100.0f);
	}
	if (!TestTrue(TEXT("Both clients acknowledge possession of their fixture character"), Worlds.TickUntil([&]()
	{
		for (int32 Index = 0; Index < 2; ++Index)
		{
			APawn* ClientPawn = ClientPC[Index]->GetPawn();
			if (!ClientPawn || !ClientPawn->IsA<ACharacter>() || ClientPC[Index]->AcknowledgedPawn != ClientPawn
				|| FVector::DistSquared(ClientPawn->GetActorLocation(), Pawn[Index]->GetActorLocation()) > 1.0) { return false; }
		}
		return true;
	}, 120))) { return false; }
	// Initial ClientRestart/control-rotation messages must settle before arranging
	// a deterministic downward view; otherwise they overwrite the first RPC's aim.
	Worlds.TickAll(25);

	// Private RPC methods are deliberately invoked on client-owned components, not their
	// implementations. View poses are deterministic fixtures; transport is a real IP connection.
	auto SendRay = [&](int32 Index, FVector Target, bool bToggle = false)
	{
		const FVector Origin = Pawn[Index]->GetPawnViewLocation(), Direction = (Target - Origin).GetSafeNormal();
		ServerPC[Index]->SetControlRotation(Direction.Rotation()); ClientPC[Index]->SetControlRotation(Direction.Rotation());
		if (bToggle) { ClientInteraction[Index]->ServerToggleSpiritBox(Origin, Direction); }
		else { ClientInteraction[Index]->ServerInteract(Origin, Direction); }
	};
	FVector InitialCamera; FRotator InitialCameraRotation;
	ServerPC[0]->GetPlayerViewPoint(InitialCamera, InitialCameraRotation);
	AddInfo(FString::Printf(TEXT("Pickup fixture: authority camera=%s pawn eye=%s box=%s"),
		*InitialCamera.ToString(), *Pawn[0]->GetPawnViewLocation().ToString(), *Box->GetActorLocation().ToString()));
	SendRay(0, Box->GetActorLocation() + FVector(0, 0, 15));
	if (!TestTrue(TEXT("Owner RPC picks up and replicates carrier and held endpoint"), Worlds.TickUntil([&]()
	{
		return Box->GetCarrier() == Pawn[0] && ServerInteraction[0]->GetHeldSpiritBox() == Box
			&& ClientInteraction[0]->GetHeldSpiritBox() == Peer[0] && Peer[0]->GetCarrier() == ClientPC[0]->GetPawn()
			&& Peer[1]->GetCarrier() != nullptr;
	}, 120)))
	{
		FVector Camera; FRotator CameraRotation; ServerPC[0]->GetPlayerViewPoint(Camera, CameraRotation);
		const FVector Origin = Pawn[0]->GetPawnViewLocation();
		const FVector Direction = (Box->GetActorLocation() + FVector(0, 0, 15) - Origin).GetSafeNormal();
		AddInfo(FString::Printf(TEXT("Pickup diagnostics: camera=%s viewValid=%d focused=%s reach=%d feedback=%s serverHeld=%s clientHeld=%s carrier=%s"),
			*Camera.ToString(), ServerInteraction[0]->IsValidView(Origin, Direction),
			*GetNameSafe(ServerInteraction[0]->FindSpiritBox(Origin, Direction)), Box->CanReachPlacedBox(ServerPC[0]),
			*ClientInteraction[0]->LastFeedback, *GetNameSafe(ServerInteraction[0]->GetHeldSpiritBox()),
			*GetNameSafe(ClientInteraction[0]->GetHeldSpiritBox()), *GetNameSafe(Box->GetCarrier())));
		return false;
	}
	TestTrue(TEXT("Held endpoint remains private to its owner"), ClientInteraction[1]->GetHeldSpiritBox() == nullptr);
	TestFalse(TEXT("Other authority-side player cannot steal a carried radio"), Box->TryPickup(ServerPC[1]));
	TestFalse(TEXT("Other player cannot power a carried radio"), Box->TrySetPowered(ServerPC[1], true));
	Worlds.TickAll(25);
	SendRay(0, Box->GetActorLocation() + FVector(0, 0, 15), true);
	if (!TestTrue(TEXT("Owner R RPC powers the radio on for both peers"), Worlds.TickUntil([&]()
	{
		return Box->IsPowered() && Peer[0]->IsPowered() && Peer[1]->IsPowered();
	}, 120))) { return false; }
	Worlds.TickAll(25);
	SendRay(0, FVector(4900, -150, 0));
	if (!TestTrue(TEXT("Owner F RPC places powered radio on supported floor"), Worlds.TickUntil([&]()
	{
		return !Box->GetCarrier() && !ServerInteraction[0]->GetHeldSpiritBox() && !ClientInteraction[0]->GetHeldSpiritBox()
			&& !Peer[0]->GetCarrier() && !Peer[1]->GetCarrier();
	}, 120))) { return false; }
	TestTrue(TEXT("Placement keeps power and rests on validated floor"), Box->IsPowered()
		&& FMath::Abs(Box->GetActorLocation().Z - .3) <= 2 && FVector::Dist2D(Box->GetActorLocation(), FVector(4900, -150, 0)) <= 5);

	const FVector BoxHome = Box->GetActorLocation(), PlayerHome = Pawn[0]->GetActorLocation(), GhostHome = Ghost->GetActorLocation();
	TestFalse(TEXT("Unknown question does not create a response"), Box->TryRespond(Ghost, ServerPC[0], TEXT("unsupported_question")));
	TestFalse(TEXT("Missing speaker fails closed"), Box->TryRespond(Ghost, nullptr, Question));
	Pawn[0]->SetActorLocation(FVector(5700, -150, 90));
	TestFalse(TEXT("Speaker over six metres from radio is rejected within the same store"), Box->TryRespond(Ghost, ServerPC[0], Question));
	Box->SetActorLocation(FVector(4900, -700, .3)); Pawn[0]->SetActorLocation(FVector(4900, -800, 90));
	TestFalse(TEXT("Speaker bounds crossing into a neighboring store are rejected"), Box->TryRespond(Ghost, ServerPC[0], Question));
	Pawn[0]->SetActorLocation(PlayerHome);
	Box->SetActorLocation(FVector(4900, -150, 460));
	TestFalse(TEXT("Radio above the ground-floor interaction band is rejected"), Box->TryRespond(Ghost, ServerPC[0], Question));
	Box->SetActorLocation(FVector(5945, -150, .3)); Pawn[0]->SetActorLocation(FVector(5800, -150, 90));
	TestFalse(TEXT("Radio bounds crossing the store edge are rejected"), Box->TryRespond(Ghost, ServerPC[0], Question));
	Box->SetActorLocation(FVector(5750, -150, .3)); Pawn[0]->SetActorLocation(FVector(5700, -150, 90));
	Ghost->SetActorLocation(FVector(4000, -150, 92.15));
	TestFalse(TEXT("Ghost over fifteen metres from the radio is rejected"), Box->TryRespond(Ghost, ServerPC[0], Question));
	Ghost->SetActorLocation(GhostHome); Box->SetActorLocation(BoxHome); Pawn[0]->SetActorLocation(PlayerHome);
	State->SetGhostMode(EMallGhostMode::VisibleHunt);
	TestFalse(TEXT("Hunting ghost cannot answer through the harmless interaction path"), Box->TryRespond(Ghost, ServerPC[0], Question));
	State->SetGhostMode(EMallGhostMode::InvisibleDisturbance);
	TestFalse(TEXT("Non-authority peer cannot invent a response"), Peer[0]->TryRespond(Ghost, ServerPC[0], Question));
	TestEqual(TEXT("All rejected guard cases leave response counter unchanged"), Box->ResponseCount, 0);

	FMallSpiritBoxResponse Alternate = ExpectedResponses[Question];
	Alternate.PhraseId = Question; Alternate.Subtitle = TEXT("An alternate recorded response.");
	Box->Responses.Add(Alternate);
	if (!TestTrue(TEXT("Nearby invisible ghost accepts a supported question"), Box->TryRespond(Ghost, ServerPC[0], Question))) { return false; }
	TestFalse(TEXT("Immediate repeat is limited by the shared response cooldown"), Box->TryRespond(Ghost, ServerPC[0], Question));
	if (!TestTrue(TEXT("Both peers receive response metadata and one live playback event"), Worlds.TickUntil([&]()
	{
		return Peer[0]->ResponseCount == 1 && Peer[1]->ResponseCount == 1 && Peer[0]->LastPhraseId == Question && Peer[1]->LastPhraseId == Question
			&& Peer[0]->LastSubtitle == Box->LastSubtitle && Peer[1]->LastSubtitle == Box->LastSubtitle && !Box->LastSubtitle.IsEmpty()
			&& Peer[0]->LocalResponsePlaybackCount == 1 && Peer[1]->LocalResponsePlaybackCount == 1;
	}, 120))) { return false; }
	const FVector FirstResponseOrigin = Box->GetActorLocation();
	TestTrue(TEXT("Authority records the radio position rather than the ghost"), FVector(Box->LastResponseLocation).Equals(FirstResponseOrigin, 1.0));
	for (AMallSpiritBox* LocalBox : {Box, Peer[0], Peer[1]})
	{
		TestTrue(TEXT("Both peers receive the authority's initial radio position"), FVector(LocalBox->LastResponseLocation).Equals(FirstResponseOrigin, 1.0));
		TestTrue(TEXT("Live reply starts at the local radio"), LocalBox->ResponseAudio->GetComponentLocation().Equals(LocalBox->GetActorLocation(), 1.0));
		TestTrue(TEXT("Reply and scan originate at the same device"),
			LocalBox->ResponseAudio->GetComponentLocation().Equals(LocalBox->ScanAudio->GetComponentLocation(), 1.0));
		// Null-audio tests cannot keep a real clip playing. Enable only its tracking state
		// to verify the same ticking transform path used during audible playback.
		LocalBox->bResponseActive = true;
	}
	const FVector MovedGhost = GhostHome + FVector(-180.0, 175.0, 20.0);
	Ghost->SetActorLocation(MovedGhost); Ghost->ForceNetUpdate();
	if (!TestTrue(TEXT("Moving the ghost leaves the radio reply at the device on both clients"), Worlds.TickUntil([&]()
	{
		for (AMallSpiritBox* LocalBox : {Box, Peer[0], Peer[1]})
		{
			const ACharacter* LocalGhost = First<ACharacter>(LocalBox->GetWorld());
			// Find the actual replicated ghost, not a player fixture.
			for (TActorIterator<ACharacter> It(LocalBox->GetWorld()); It; ++It)
			{
				if (It->IsA(GhostClass)) { LocalGhost = *It; break; }
			}
			if (!LocalGhost || !LocalGhost->IsA(GhostClass) || !LocalGhost->GetActorLocation().Equals(MovedGhost, 1.0)
				|| !LocalBox->ResponseAudio->GetComponentLocation().Equals(FirstResponseOrigin, 1.0)) { return false; }
		}
		return true;
	}, 120))) { return false; }
	Box->SetActorLocation(BoxHome + FVector(100.0, -50.0, 0.0)); Box->ForceNetUpdate();
	Worlds.TickAll(25);
	for (AMallSpiritBox* LocalBox : {Box, Peer[0], Peer[1]})
	{
		TestTrue(TEXT("Moving the radio moves its reply on every peer"),
			LocalBox->GetActorLocation().Equals(BoxHome + FVector(100.0, -50.0, 0.0), 1.0)
			&& LocalBox->ResponseAudio->GetComponentLocation().Equals(LocalBox->GetActorLocation(), 1.0));
		TestTrue(TEXT("Radio movement preserves the historical response origin"), FVector(LocalBox->LastResponseLocation).Equals(FirstResponseOrigin, 1.0));
	}
	Ghost->SetActorLocation(GhostHome); Ghost->ForceNetUpdate(); Box->SetActorLocation(BoxHome); Box->ForceNetUpdate();
	// Playback counters and source transforms prove delivery and positioning; null-RHI
	// automation does not establish audibility, stereo perception, or microphone recognition.
	int32 ExpectedCount = 1;
	for (const FName Phrase : {FName(TEXT("who_are_you")), FName(TEXT("how_old_are_you"))})
	{
		Worlds.TickAll(260);
		if (!TestTrue(FString::Printf(TEXT("%s selects an independent response"), *Phrase.ToString()), Box->TryRespond(Ghost, ServerPC[0], Phrase))) { return false; }
		++ExpectedCount;
		const FMallSpiritBoxResponse Expected = ExpectedResponses[Phrase];
		TestEqual(TEXT("Question selects its own subtitle"), Box->LastSubtitle, Expected.Subtitle);
		if (!TestTrue(TEXT("Both clients play the selected question binding once"), Worlds.TickUntil([&]()
		{
			return Peer[0]->ResponseCount == ExpectedCount && Peer[1]->ResponseCount == ExpectedCount
				&& Peer[0]->LastPhraseId == Phrase && Peer[1]->LastPhraseId == Phrase
				&& Peer[0]->LastSubtitle == Expected.Subtitle && Peer[1]->LastSubtitle == Expected.Subtitle
				&& Peer[0]->ResponseAudio->Sound == Expected.Sound && Peer[1]->ResponseAudio->Sound == Expected.Sound
				&& Peer[0]->LocalResponsePlaybackCount == ExpectedCount && Peer[1]->LocalResponsePlaybackCount == ExpectedCount;
		}, 120))) { return false; }
	}
	Worlds.TickAll(260);
	TestTrue(TEXT("Response becomes available after the four-second cooldown"), Box->TryRespond(Ghost, ServerPC[0], Question));
	++ExpectedCount;
	TestEqual(TEXT("Adding a data entry selects the alternate subtitle without changing gameplay"), Box->LastSubtitle, Alternate.Subtitle);
	if (!TestTrue(TEXT("Both peers receive the second variant once"), Worlds.TickUntil([&]()
	{
		return Peer[0]->ResponseCount == ExpectedCount && Peer[1]->ResponseCount == ExpectedCount && Peer[0]->LastSubtitle == Alternate.Subtitle
			&& Peer[1]->LastSubtitle == Alternate.Subtitle && Peer[0]->LocalResponsePlaybackCount == ExpectedCount && Peer[1]->LocalResponsePlaybackCount == ExpectedCount;
	}, 120))) { return false; }
	Worlds.TickAll(25);
	SendRay(0, Box->GetActorLocation() + FVector(0, 0, 15));
	if (!TestTrue(TEXT("Owner can pick up the still-powered answered radio"), Worlds.TickUntil([&]()
	{
		return Box->GetCarrier() == Pawn[0] && ClientInteraction[0]->GetHeldSpiritBox() == Peer[0];
	}, 120))) { return false; }
	const FVector CarriedBoxHome = Box->GetActorLocation();
	for (AMallSpiritBox* LocalBox : {Box, Peer[0], Peer[1]}) { LocalBox->bResponseActive = true; }
	Pawn[0]->SetActorLocation(PlayerHome + FVector(50.0, -75.0, 0.0)); Pawn[0]->ForceNetUpdate();
	if (!TestTrue(TEXT("Carrying the radio moves its spoken reply with the player on both clients"), Worlds.TickUntil([&]()
	{
		if (FVector::DistSquared(Box->GetActorLocation(), CarriedBoxHome) < FMath::Square(50.0)) { return false; }
		for (AMallSpiritBox* LocalBox : {Box, Peer[0], Peer[1]})
		{
			if (FVector::DistSquared(LocalBox->GetActorLocation(), CarriedBoxHome) < FMath::Square(50.0)
				|| !LocalBox->ResponseAudio->GetComponentLocation().Equals(LocalBox->GetActorLocation(), 1.0)) { return false; }
		}
		return true;
	}, 120))) { return false; }
	if (!TestTrue(TEXT("Third actual IP client joins after the response"), Worlds.CreateAndConnectClient())) { return false; }
	AMallSpiritBox* Late = nullptr;
	if (!TestTrue(TEXT("Late join reconstructs power, carrier and response metadata without replaying old audio"), Worlds.TickUntil([&]()
	{
		Late = First<AMallSpiritBox>(Worlds.Clients[2].GetWorld());
		const ACharacter* Carrier = Late ? Cast<ACharacter>(Late->GetCarrier()) : nullptr;
		return Late && Late->IsPowered() && Late->ResponseCount == ExpectedCount && Late->LastPhraseId == Question
			&& Late->LastSubtitle == Box->LastSubtitle && Carrier && Carrier->GetPlayerState()
			&& Carrier->GetPlayerState()->GetPlayerId() == ServerPC[0]->PlayerState->GetPlayerId()
			&& FVector(Late->LastResponseLocation).Equals(FVector(Box->LastResponseLocation), 1.0)
			&& Late->LocalResponsePlaybackCount == 0;
	}, 180))) { return false; }
	Worlds.TickAll(25);
	SendRay(0, Box->GetActorLocation() + FVector(0, 0, 15), true);
	if (!TestTrue(TEXT("Owner powers off carried radio and all peers receive the change"), Worlds.TickUntil([&]()
	{
		return !Box->IsPowered() && !Peer[0]->IsPowered() && !Peer[1]->IsPowered() && !Late->IsPowered();
	}, 120))) { return false; }
	TestEqual(TEXT("Power changes preserve previous response history"), Late->ResponseCount, ExpectedCount);
	TestEqual(TEXT("Late join power changes still do not replay old audio"), Late->LocalResponsePlaybackCount, 0);
	return true;
}

#endif
