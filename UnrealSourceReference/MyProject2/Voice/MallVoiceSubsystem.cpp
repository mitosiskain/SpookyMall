#include "MallVoiceSubsystem.h"
#include "MallGhostVoiceReceiver.h"
#include "MallSpeechRecognizer.h"
#include "MallVoiceComponent.h"
#include "MallVoiceSettings.h"
#include "MallMicrophoneSettings.h"
#include "MallMicrophoneDiagnostics.h"
#include "MallVoiceStatusWidget.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

void FMallSpeechUtteranceBuffer::Reset()
{
	BufferedSamples.Reset();
	bContainsSpeech = false;
}

void FMallSpeechUtteranceBuffer::Finish(TFunctionRef<void(TArray<int16>&&)> OnUtterance)
{
	const bool bSubmit = bContainsSpeech && BufferedSamples.Num() >= 4000;
	TArray<int16> Audio = MoveTemp(BufferedSamples);
	Reset();
	if (bSubmit) { OnUtterance(MoveTemp(Audio)); }
}

void FMallSpeechUtteranceBuffer::Append(TConstArrayView<int16> Samples, bool bEnd,
	TFunctionRef<void(TArray<int16>&&)> OnUtterance)
{
	int32 Offset = 0;
	while (Offset < Samples.Num())
	{
		const int32 Count = FMath::Min(MaxSamples - BufferedSamples.Num(), Samples.Num() - Offset);
		double Energy = 0.;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const double Value = static_cast<double>(Samples[Offset + Index]) / 32768.;
			Energy += Value * Value;
		}
		bContainsSpeech |= FMath::Sqrt(Energy / Count) >= .006;
		BufferedSamples.Append(Samples.GetData() + Offset, Count);
		Offset += Count;
		// Preserve samples that straddle the cap in the next bounded segment.
		if (BufferedSamples.Num() == MaxSamples) { Finish(OnUtterance); }
	}
	// Release/focus loss ends capture. Quiet gaps while held must not split words.
	if (bEnd) { Finish(OnUtterance); }
}

UMallVoiceSubsystem::UMallVoiceSubsystem() = default;
UMallVoiceSubsystem::~UMallVoiceSubsystem() = default;
UMallVoiceComponent* UMallVoiceSubsystem::GetLocalVoice() const { return LocalVoice.Get(); }

bool UMallVoiceSubsystem::DoesSupportWorldType(EWorldType::Type Type) const
{
	return Type == EWorldType::Game || Type == EWorldType::PIE;
}

void UMallVoiceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LastPhraseStatus = TEXT("Ghost phrases: write in the book / where are you / who are you / how old are you / give me a sign / come here / leave me alone");
}

void UMallVoiceSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	RefreshEndpoints();
}

TStatId UMallVoiceSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UMallVoiceSubsystem, STATGROUP_Tickables);
}

void UMallVoiceSubsystem::RefreshEndpoints()
{
	UWorld* World = GetWorld();
	const UMallVoiceSettings* Settings = GetDefault<UMallVoiceSettings>();
	if (World->GetNetMode() != NM_Client)
	{
		for (TActorIterator<ACharacter> It(World); It; ++It)
		{
			ACharacter* Ghost = *It;
			if (!Ghost->ActorHasTag(TEXT("StoreGhostPrototype"))) { continue; }
			if (!Ghost->FindComponentByClass<UMallGhostVoiceReceiver>())
			{
				UMallGhostVoiceReceiver* Receiver = NewObject<UMallGhostVoiceReceiver>(Ghost, TEXT("MallGhostVoice"));
				Receiver->MinimumConfidence = Settings->MinimumPhraseConfidence;
				Ghost->AddInstanceComponent(Receiver); Receiver->RegisterComponent(); Ghost->ForceNetUpdate();
			}
		}
	}
	// Receiver BeginPlay also keeps ghost Pawn collision consistent on every peer.
	// Keep that safety setup active even when microphone/voice endpoints are disabled.
	if (!Settings->bEnabled) { return; }
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC) { continue; }
		UMallVoiceComponent* Voice = PC->FindComponentByClass<UMallVoiceComponent>();
		if (!Voice && PC->HasAuthority())
		{
			PC->SetReplicates(true);
			Voice = NewObject<UMallVoiceComponent>(PC, TEXT("MallVoiceTransport"));
			PC->AddInstanceComponent(Voice); Voice->RegisterComponent(); PC->ForceNetUpdate();
		}
		if (!Voice) { continue; }
		Voice->SetVoiceDistances(Settings->MaxVoiceDistanceCm, Settings->FullVolumeDistanceCm);
		if (PC->HasAuthority() && !BoundEndpoints.Contains(Voice))
		{
			Voice->OnServerPhrase.AddUObject(this, &UMallVoiceSubsystem::HandleServerPhrase);
			BoundEndpoints.Add(Voice);
		}
		if (PC->IsLocalController() && !LocalVoice.IsValid()) { InitializeLocal(Voice); }
	}
}

void UMallVoiceSubsystem::InitializeLocal(UMallVoiceComponent* Voice)
{
	if (Recognizer) { Recognizer->Shutdown(); Recognizer.Reset(); }
	if (StatusWidget) { StatusWidget->RemoveFromParent(); StatusWidget = nullptr; }
	Utterance.Reset();
	bTalkKeyWasDown = false; bMicrophoneControlsWereOpen = false;
	LocalVoice = Voice;
	Voice->OnLocalPCM.AddUObject(this, &UMallVoiceSubsystem::ReceivePCM);
	const UMallVoiceSettings* Settings = GetDefault<UMallVoiceSettings>();
	APlayerController* PC = Cast<APlayerController>(Voice->GetOwner());
	if (Settings->bShowVoiceStatus && PC)
	{
		StatusWidget = CreateWidget<UMallVoiceStatusWidget>(PC, UMallVoiceStatusWidget::StaticClass());
		if (StatusWidget)
		{
			StatusWidget->AddToPlayerScreen(100);
			StatusWidget->SetDesiredSizeInViewport(FVector2D(610.f, 136.f));
			StatusWidget->SetPositionInViewport(FVector2D(20.f, -20.f), false);
			// UE resets viewport anchors when size/position changes. Apply them last
			// so bottom-left alignment cannot put the whole panel above the screen.
			StatusWidget->SetAnchorsInViewport(FAnchors(0.f, 1.f));
			StatusWidget->SetAlignmentInViewport(FVector2D(0.f, 1.f));
		}
	}
	if (!Settings->bSpeechRecognitionEnabled) { SpeechStatus = TEXT("Phrase recognition disabled"); return; }
	Recognizer = MakeUnique<FMallSpeechRecognizer>();
	const TWeakObjectPtr<UMallVoiceSubsystem> WeakThis(this);
	SpeechStatus = TEXT("Loading offline Whisper English model...");
	Recognizer->InitializeWhisper(FPaths::Combine(FPaths::ProjectDir(), TEXT("ThirdParty/Whisper/Win64")),
		FPaths::Combine(FPaths::ProjectDir(), TEXT("ThirdParty/Whisper/Models/ggml-small.en.bin")),
		Settings->GetPhraseAliases(), [WeakThis](bool bReady, FString Status)
		{
			if (UMallVoiceSubsystem* Self = WeakThis.Get()) { Self->SpeechStatus = MoveTemp(Status); }
		}, Settings->MinimumPhraseConfidence);
}

void UMallVoiceSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UWorld* World = GetWorld();
	if (!World || !World->HasBegunPlay()) { return; }
	const UMallVoiceSettings* Settings = GetDefault<UMallVoiceSettings>();
	const double Now = World->GetRealTimeSeconds();
	if (Now >= NextRefreshTime) { RefreshEndpoints(); NextRefreshTime = Now + .5; }
	if (!Settings->bEnabled)
	{
		if (LocalVoice.IsValid()) { LocalVoice->SetPushToTalk(false); }
		bTalkKeyWasDown = false;
		return;
	}
	if (LocalVoice.IsValid())
	{
		APlayerController* PC = Cast<APlayerController>(LocalVoice->GetOwner());
		ULocalPlayer* Player = PC ? PC->GetLocalPlayer() : nullptr;
		FViewport* Viewport = Player && Player->ViewportClient ? Player->ViewportClient->Viewport : nullptr;
		if (PC && StatusWidget && !StatusWidget->IsMicrophoneControlsOpen() && PC->WasInputKeyJustPressed(EKeys::F6))
		{
			StatusWidget->SetMicrophoneControlsOpen(true);
		}
		const bool bMicControlsOpen = StatusWidget && StatusWidget->IsMicrophoneControlsOpen();
		const bool bFocused = Viewport && (Viewport->HasFocus() || (bMicControlsOpen && Viewport->IsForegroundWindow()));
		const bool bPressed = PC && bFocused && !World->IsPaused() && PC->IsInputKeyDown(Settings->PushToTalkKey);
		if (bPressed != bTalkKeyWasDown)
		{
			if (bPressed)
			{
				LastPhraseStatus = TEXT("Listening for a ghost phrase...");
				LocalVoice->LastServerFeedback.Reset();
			}
			LocalVoice->SetPushToTalk(bPressed);
			if (!bPressed && LastPhraseStatus.StartsWith(TEXT("Listening")))
			{
				LastPhraseStatus = TEXT("No usable speech captured. Check microphone levels in F6.");
			}
			bTalkKeyWasDown = bPressed;
		}
	}
	if (Now >= NextStatusTime) { UpdateStatus(); NextStatusTime = Now + .1; }
}

void UMallVoiceSubsystem::ReceivePCM(const TArray<int16>& Samples, bool bEnd)
{
	const UMallVoiceSettings* Settings = GetDefault<UMallVoiceSettings>();
	if (!Settings->bEnabled || !Settings->bSpeechRecognitionEnabled || !Recognizer || !Recognizer->IsReady())
	{
		Utterance.Reset();
		return;
	}
	Utterance.Append(Samples, bEnd, [this](TArray<int16>&& Audio) { SubmitUtterance(MoveTemp(Audio)); });
}

void UMallVoiceSubsystem::SubmitUtterance(TArray<int16>&& Audio)
{
	if (!Recognizer || !Recognizer->IsReady()) { return; }
	LastPhraseStatus = TEXT("Processing speech...");
	const TWeakObjectPtr<UMallVoiceSubsystem> WeakThis(this);
	if (!Recognizer->SubmitUtterance(MoveTemp(Audio), [WeakThis](FName Id, float Confidence, FString Text)
	{
		UMallVoiceSubsystem* Self = WeakThis.Get();
		if (!Self || !Self->LocalVoice.IsValid()) { return; }
		const UMallVoiceSettings* Settings = GetDefault<UMallVoiceSettings>();
		if (!Settings->bEnabled || !Settings->bSpeechRecognitionEnabled) { return; }
		if (Id.IsNone())
		{
			// Keep text local to the panel. Audio is retained only by the separate,
			// explicitly user-armed one-turn microphone diagnostic.
			Self->LastPhraseStatus = Text.IsEmpty()
				? TEXT("No clear words heard - hold V before speaking, then release after the phrase.")
				: FString::Printf(TEXT("Not recognized. Heard: \"%s\" (confidence %.0f%%)"), *Text.Left(64), Confidence * 100.f);
			return;
		}
		Self->LastPhraseStatus = FString::Printf(TEXT("Recognized: %s"), *Id.ToString().Replace(TEXT("_"), TEXT(" ")));
		Self->LocalVoice->SubmitRecognizedPhrase(Id, Confidence);
	})) { LastPhraseStatus = TEXT("Recognition busy - try the phrase again"); }
}

void UMallVoiceSubsystem::HandleServerPhrase(APlayerController* Speaker, FName Id, float Confidence)
{
	if (!Speaker || !Speaker->HasAuthority() || GetWorld()->GetNetMode() == NM_Client) { return; }
	const UMallVoiceSettings* Settings = GetDefault<UMallVoiceSettings>();
	if (!Settings->bEnabled || !Settings->bSpeechRecognitionEnabled) { return; }
	bool bAccepted = false;
	for (TActorIterator<ACharacter> It(GetWorld()); It; ++It)
	{
		if (!It->ActorHasTag(TEXT("StoreGhostPrototype"))) { continue; }
		if (UMallGhostVoiceReceiver* Receiver = It->FindComponentByClass<UMallGhostVoiceReceiver>())
		{
			bAccepted |= Receiver->OnServerPhrase(Speaker, Id, Confidence);
		}
	}
	if (UMallVoiceComponent* Voice = Speaker->FindComponentByClass<UMallVoiceComponent>())
	{
		Voice->ReportPhraseResult(Id, bAccepted);
	}
}

bool UMallVoiceSubsystem::IsSpeechReady() const { return Recognizer && Recognizer->IsReady(); }

void UMallVoiceSubsystem::UpdateStatus()
{
	if (!StatusWidget || !LocalVoice.IsValid()) { return; }
	const FString Key = GetDefault<UMallVoiceSettings>()->PushToTalkKey.GetDisplayName().ToString();
	const FMallMicrophoneOptions Options = GetDefault<UMallMicrophoneSettings>()->GetOptions();
	const float OutputDb = 20.f * FMath::LogX(10.f, FMath::Max(LocalVoice->MicrophoneRMS, 0.00001f));
	const float RawDb = 20.f * FMath::LogX(10.f, FMath::Max(LocalVoice->RawMicrophoneRMS, 0.00001f));
	const int32 Meter = FMath::Clamp(FMath::RoundToInt((OutputDb + 60.f) * (100.f / 60.f)), 0, 100);
	FString Voice = FString::Printf(TEXT("%s | Mic %d%% | Auto gain %s | F6 mic settings"),
		LocalVoice->IsPushToTalkActive() ? TEXT("Talking") : *FString::Printf(TEXT("Hold %s to talk"), *Key),
		Meter, Options.bAutoGain ? TEXT("ON") : TEXT("OFF"));
	if (!LocalVoice->IsPushToTalkActive() && !LocalVoice->Status.StartsWith(TEXT("Ready"))) { Voice += TEXT(" | ") + LocalVoice->Status; }
	if (LocalVoice->bMicrophoneClipping) { Voice += TEXT(" | CLIPPING: move mic away or lower source input"); }
	const bool bControlsOpen = StatusWidget->IsMicrophoneControlsOpen();
	if (bControlsOpen)
	{
		Voice += FString::Printf(TEXT("\nSource %.0f dBFS | Output %.0f dBFS | Applied boost %+.0f dB"), RawDb, OutputDb, LocalVoice->AppliedMicrophoneGainDb);
		const FString Diagnostic = FMallMicrophoneDiagnostics::GetStatus(LocalVoice.Get());
		if (!Diagnostic.IsEmpty()) { Voice += TEXT("\n") + Diagnostic; }
	}
	if (bMicrophoneControlsWereOpen != bControlsOpen)
	{
		StatusWidget->SetDesiredSizeInViewport(FVector2D(680.f, bControlsOpen ? 445.f : 136.f));
		StatusWidget->SetPositionInViewport(FVector2D(20.f, -20.f), false);
		StatusWidget->SetAnchorsInViewport(FAnchors(0.f, 1.f));
		StatusWidget->SetAlignmentInViewport(FVector2D(0.f, 1.f));
		bMicrophoneControlsWereOpen = bControlsOpen;
	}
	const FString Feedback = LocalVoice->LastServerFeedback.IsEmpty()
		? LastPhraseStatus : LastPhraseStatus + TEXT(" | ") + LocalVoice->LastServerFeedback;
	StatusWidget->SetStatus(Voice, SpeechStatus, Feedback);
}

void UMallVoiceSubsystem::Deinitialize()
{
	if (LocalVoice.IsValid()) { LocalVoice->SetPushToTalk(false); LocalVoice->OnLocalPCM.RemoveAll(this); }
	for (const TWeakObjectPtr<UMallVoiceComponent>& Voice : BoundEndpoints)
	{
		if (Voice.IsValid()) { Voice->OnServerPhrase.RemoveAll(this); }
	}
	if (Recognizer) { Recognizer->Shutdown(); Recognizer.Reset(); }
	if (StatusWidget) { StatusWidget->RemoveFromParent(); StatusWidget = nullptr; }
	LocalVoice.Reset(); BoundEndpoints.Reset(); Utterance.Reset();
	Super::Deinitialize();
}
