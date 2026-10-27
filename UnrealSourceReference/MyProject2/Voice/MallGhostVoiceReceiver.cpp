#include "Voice/MallGhostVoiceReceiver.h"
#include "Equipment/MallCartProtectionSubsystem.h"
#include "Ghost/MallGhostInteractionComponent.h"
#include "Ghost/MallGhostAudio.h"
#include "Ghost/MallSpiritBox.h"
#include "EngineUtils.h"

#include "Components/AudioComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundWaveProcedural.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogMallGhostVoice, Log, All);

namespace MallGhostVoice
{
	bool ReadNumber(const UObject* Object, const FName Name, double& Value)
	{
		const FNumericProperty* Property = Object ? FindFProperty<FNumericProperty>(Object->GetClass(), Name) : nullptr;
		if (!Property || !Property->IsFloatingPoint())
		{
			return false;
		}
		Value = Property->GetFloatingPointPropertyValue(Property->ContainerPtrToValuePtr<void>(Object));
		return FMath::IsFinite(Value);
	}

	bool WriteNumber(UObject* Object, const FName Name, const double Value)
	{
		FNumericProperty* Property = Object ? FindFProperty<FNumericProperty>(Object->GetClass(), Name) : nullptr;
		if (!Property || !Property->IsFloatingPoint() || !FMath::IsFinite(Value))
		{
			return false;
		}
		Property->SetFloatingPointPropertyValue(Property->ContainerPtrToValuePtr<void>(Object), Value);
		return true;
	}

	void ClearExistingRoute(AActor* Ghost)
	{
		if (UFunction* Function = Ghost ? Ghost->FindFunction(TEXT("ClearRoute")) : nullptr)
		{
			// This Blueprint function stops its controller and clears only its accepted route.
			if (Function->NumParms == 0)
			{
				Ghost->ProcessEvent(Function, nullptr);
			}
		}
	}
}

UMallGhostVoiceReceiver::UMallGhostVoiceReceiver()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetIsReplicatedByDefault(true);
}

void UMallGhostVoiceReceiver::BeginPlay()
{
	Super::BeginPlay();
	// Server AI is authoritative, but pawn collision must agree on every peer.
	if (ACharacter* Ghost = Cast<ACharacter>(GetOwner()))
	{
		Ghost->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	}
	EnsureSignalComponents();
}

void UMallGhostVoiceReceiver::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(ChimeStopTimer);
	}
	if (IsValid(ChimeAudio))
	{
		ChimeAudio->Stop();
	}
	if (SignalLight)
	{
		SignalLight->DestroyComponent();
	}
	if (SignalText)
	{
		SignalText->DestroyComponent();
	}
	Super::EndPlay(EndPlayReason);
}

void UMallGhostVoiceReceiver::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UMallGhostVoiceReceiver, LastPhrase);
	DOREPLIFETIME(UMallGhostVoiceReceiver, ReactionCount);
	DOREPLIFETIME(UMallGhostVoiceReceiver, LastSpeaker);
	DOREPLIFETIME(UMallGhostVoiceReceiver, LastReactionServerTime);
}

FName UMallGhostVoiceReceiver::CanonicalPhrase(const FName Phrase)
{
	FString Letters;
	for (const TCHAR Letter : Phrase.ToString())
	{
		if (FChar::IsAlpha(Letter))
		{
			Letters.AppendChar(FChar::ToLower(Letter));
		}
	}
	if (Letters == TEXT("whereareyou")) return TEXT("where_are_you");
	if (Letters == TEXT("whoareyou")) return TEXT("who_are_you");
	if (Letters == TEXT("howoldareyou")) return TEXT("how_old_are_you");
	if (Letters == TEXT("givemeasign")) return TEXT("give_me_a_sign");
	if (Letters == TEXT("comehere")) return TEXT("come_here");
	if (Letters == TEXT("leavemealone")) return TEXT("leave_me_alone");
	if (Letters == TEXT("writeinbook")) return TEXT("write_in_book");
	return NAME_None;
}

bool UMallGhostVoiceReceiver::IsInsideGhostStore(const AActor* Ghost, const ACharacter* Character)
{
	if (!IsValid(Ghost) || !IsValid(Character) || Ghost->GetWorld() != Character->GetWorld())
	{
		return false;
	}
	double MinX, MaxX, MinY, MaxY, FloorZ;
	if (!MallGhostVoice::ReadNumber(Ghost, TEXT("StoreMinX"), MinX)
		|| !MallGhostVoice::ReadNumber(Ghost, TEXT("StoreMaxX"), MaxX)
		|| !MallGhostVoice::ReadNumber(Ghost, TEXT("StoreMinY"), MinY)
		|| !MallGhostVoice::ReadNumber(Ghost, TEXT("StoreMaxY"), MaxY)
		|| !MallGhostVoice::ReadNumber(Ghost, TEXT("StoreFloorZ"), FloorZ)
		|| MinX >= MaxX || MinY >= MaxY)
	{
		return false;
	}
	const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	if (!Capsule)
	{
		return false;
	}
	const FVector Center = Character->GetActorLocation();
	const double FeetZ = Center.Z - Capsule->GetScaledCapsuleHalfHeight();
	// Match the already tested Blueprint eligibility, including normal ground-floor jumps.
	return Center.X >= MinX && Center.X <= MaxX && Center.Y >= MinY && Center.Y <= MaxY
		&& FeetZ >= FloorZ - 40.0 && FeetZ <= FloorZ + 450.0;
}

bool UMallGhostVoiceReceiver::IsSpeakerEligible(ACharacter* Speaker) const
{
	AActor* Ghost = GetOwner();
	const UMallCartProtectionSubsystem* Protection = GetWorld() ? GetWorld()->GetSubsystem<UMallCartProtectionSubsystem>() : nullptr;
	if (IsValid(Ghost) && Ghost->ActorHasTag(TEXT("MallCartBoundary"))
		&& Protection && Protection->IsStoreSuppressedByCart(Ghost)) { return false; }
	return IsInsideGhostStore(GetOwner(), Speaker)
		&& IsValid(Speaker->GetController()) && Speaker->GetController()->IsPlayerController();
}

ACharacter* UMallGhostVoiceReceiver::ChooseTargetPawn() const
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !GetWorld())
	{
		return nullptr;
	}
	if (GetWorld()->GetTimeSeconds() < PreferredSpeakerUntil && IsSpeakerEligible(PreferredSpeaker.Get()))
	{
		return PreferredSpeaker.Get();
	}
	ACharacter* Nearest = nullptr;
	double BestDistanceSquared = TNumericLimits<double>::Max();
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* Controller = It->Get();
		ACharacter* Candidate = Controller ? Cast<ACharacter>(Controller->GetPawn()) : nullptr;
		if (IsSpeakerEligible(Candidate))
		{
			const double DistanceSquared = FVector::DistSquared(GetOwner()->GetActorLocation(), Candidate->GetActorLocation());
			if (DistanceSquared < BestDistanceSquared)
			{
				Nearest = Candidate;
				BestDistanceSquared = DistanceSquared;
			}
		}
	}
	return Nearest;
}

ACharacter* UMallGhostVoiceReceiver::ResolveTargetPawn(AActor* Ghost)
{
	if (!IsValid(Ghost) || !Ghost->HasAuthority() || !Ghost->GetWorld())
	{
		return nullptr;
	}
	if (!UMallGhostInteractionComponent::CanGhostHunt(Ghost))
	{
		return nullptr;
	}
	if (UMallGhostVoiceReceiver* Receiver = Ghost->FindComponentByClass<UMallGhostVoiceReceiver>())
	{
		return Receiver->ChooseTargetPawn();
	}
	// The graph can BeginPlay before the world voice subsystem attaches its component.
	ACharacter* Nearest = nullptr;
	double BestDistanceSquared = TNumericLimits<double>::Max();
	for (FConstPlayerControllerIterator It = Ghost->GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* Controller = It->Get();
		ACharacter* Candidate = Controller ? Cast<ACharacter>(Controller->GetPawn()) : nullptr;
		if (IsInsideGhostStore(Ghost, Candidate))
		{
			const double DistanceSquared = FVector::DistSquared(Ghost->GetActorLocation(), Candidate->GetActorLocation());
			if (DistanceSquared < BestDistanceSquared)
			{
				Nearest = Candidate;
				BestDistanceSquared = DistanceSquared;
			}
		}
	}
	return Nearest;
}

bool UMallGhostVoiceReceiver::ValidateGhostContract() const
{
	const AActor* Ghost = GetOwner();
	const UFunction* ClearRoute = Ghost ? Ghost->FindFunction(TEXT("ClearRoute")) : nullptr;
	double Ignored;
	return Ghost && Ghost->ActorHasTag(TEXT("StoreGhostPrototype"))
		&& ClearRoute && ClearRoute->NumParms == 0
		&& MallGhostVoice::ReadNumber(Ghost, TEXT("CooldownUntil"), Ignored)
		&& MallGhostVoice::ReadNumber(Ghost, TEXT("NextPlanTime"), Ignored)
		&& FindFProperty<FObjectPropertyBase>(Ghost->GetClass(), TEXT("TargetPlayer"));
}

bool UMallGhostVoiceReceiver::HandleRecognizedPhrase(APawn* Speaker, const FName Phrase, const float Confidence)
{
	APlayerController* Controller = Speaker ? Cast<APlayerController>(Speaker->GetController()) : nullptr;
	return Controller && Controller->GetPawn() == Speaker && OnServerPhrase(Controller, Phrase, Confidence);
}

void UMallGhostVoiceReceiver::ResetHuntingPreference()
{
	PreferredSpeaker.Reset();
	PreferredSpeakerUntil = 0.0;
}

bool UMallGhostVoiceReceiver::OnServerPhrase(APlayerController* Speaker, const FName Phrase, const float Confidence)
{
	AActor* Ghost = GetOwner();
	ACharacter* Character = IsValid(Speaker) ? Cast<ACharacter>(Speaker->GetPawn()) : nullptr;
	const FName AcceptedPhrase = CanonicalPhrase(Phrase);
	if (!Ghost || !Ghost->HasAuthority() || !GetWorld() || !Speaker || !Speaker->HasAuthority()
		|| Speaker->GetWorld() != GetWorld() || !Character || Character->GetController() != Speaker
		|| !IsSpeakerEligible(Character) || !ValidateGhostContract() || AcceptedPhrase.IsNone()
		|| !FMath::IsFinite(Confidence) || Confidence < MinimumConfidence || Confidence > 1.0f
		|| FVector::DistSquared(Character->GetActorLocation(), Ghost->GetActorLocation()) > FMath::Square(HearingRangeCm))
	{
		return false;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	if (const double* Previous = LastPhraseTimes.Find(Speaker))
	{
		if (Now - *Previous < MinimumPhraseInterval)
		{
			return false;
		}
	}
	for (auto It = LastPhraseTimes.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid()) It.RemoveCurrent();
	}
	UMallGhostInteractionComponent* Interaction = Ghost->FindComponentByClass<UMallGhostInteractionComponent>();
	const bool bWritingRequest = AcceptedPhrase == TEXT("write_in_book");
	const bool bGhostQuestion = AcceptedPhrase == TEXT("where_are_you") || AcceptedPhrase == TEXT("who_are_you")
		|| AcceptedPhrase == TEXT("how_old_are_you");
	bool bSpiritResponse = false;
	if (bGhostQuestion)
	{
		TArray<AMallSpiritBox*> Boxes;
		for (TActorIterator<AMallSpiritBox> It(GetWorld()); It; ++It) { Boxes.Add(*It); }
		Boxes.Sort([Character](const AMallSpiritBox& A, const AMallSpiritBox& B)
		{
			return FVector::DistSquared(Character->GetActorLocation(), A.GetActorLocation())
				< FVector::DistSquared(Character->GetActorLocation(), B.GetActorLocation());
		});
		for (AMallSpiritBox* Box : Boxes)
		{
			if (Box->TryRespond(Ghost, Speaker, AcceptedPhrase)) { bSpiritResponse = true; break; }
		}
	}
	const bool bStartedWriting = (bWritingRequest || bGhostQuestion) && Interaction && Interaction->DisturbNearestBook();
	if (bWritingRequest && !bStartedWriting)
	{
		return false;
	}
	// Questions retain their cue when no eligible book is available. The test image
	// is shared evidence artwork, not a generated answer to the particular question.
	LastPhraseTimes.Add(Speaker, Now);
	LastPhrase = AcceptedPhrase;
	LastSpeaker = Character;
	LastReactionServerTime = Now;
	++ReactionCount;
	if (AcceptedPhrase == TEXT("come_here") && UMallGhostInteractionComponent::CanGhostHunt(Ghost))
	{
		PreferredSpeaker = Character;
		PreferredSpeakerUntil = Now + 12.0;
		FObjectPropertyBase* TargetProperty = FindFProperty<FObjectPropertyBase>(Ghost->GetClass(), TEXT("TargetPlayer"));
		if (TargetProperty->PropertyClass && Character->IsA(TargetProperty->PropertyClass))
		{
			TargetProperty->SetObjectPropertyValue_InContainer(Ghost, Character);
		}
		MallGhostVoice::ClearExistingRoute(Ghost);
		MallGhostVoice::WriteNumber(Ghost, TEXT("NextPlanTime"), 0.0);
	}
	else if (AcceptedPhrase == TEXT("leave_me_alone"))
	{
		double ExistingCooldown = 0.0;
		MallGhostVoice::ReadNumber(Ghost, TEXT("CooldownUntil"), ExistingCooldown);
		MallGhostVoice::ClearExistingRoute(Ghost);
		MallGhostVoice::WriteNumber(Ghost, TEXT("CooldownUntil"), FMath::Max(ExistingCooldown, Now + LeaveMeAloneSeconds));
		if (Interaction) { Interaction->PauseDisturbances(LeaveMeAloneSeconds); }
	}
	else if (Interaction && Interaction->IsInvisible()
		&& (AcceptedPhrase == TEXT("give_me_a_sign") || AcceptedPhrase == TEXT("come_here")))
	{
		// A voice request never turns an invisible, harmless ghost into a hunter.
		Interaction->MoveNearestProp();
	}
	// Movement, path validation, catch/reset and the physical guard stay in the Blueprint.
	if (!bSpiritResponse) { MulticastShowReaction(AcceptedPhrase); }
	Ghost->ForceNetUpdate();
	UE_LOG(LogMallGhostVoice, Log, TEXT("%s accepted %s from %s (reaction %d)"),
		*Ghost->GetName(), *AcceptedPhrase.ToString(), *Speaker->GetName(), ReactionCount);
	return true;
}

void UMallGhostVoiceReceiver::EnsureSignalComponents()
{
	AActor* Ghost = GetOwner();
	if (!Ghost || !Ghost->GetRootComponent() || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	if (!SignalLight)
	{
		SignalLight = NewObject<UPointLightComponent>(Ghost, NAME_None, RF_Transient);
		SignalLight->ComponentTags.Add(TEXT("MallGhostSignal"));
		SignalLight->SetMobility(EComponentMobility::Movable);
		SignalLight->SetupAttachment(Ghost->GetRootComponent());
		SignalLight->SetRelativeLocation(FVector(0.0, 0.0, 75.0));
		SignalLight->SetLightColor(FLinearColor(0.1f, 0.65f, 1.0f));
		SignalLight->SetAttenuationRadius(350.0f);
		SignalLight->SetCastShadows(false);
		SignalLight->SetIntensity(0.0f);
		SignalLight->RegisterComponent();
	}
	if (!SignalText)
	{
		SignalText = NewObject<UTextRenderComponent>(Ghost, NAME_None, RF_Transient);
		SignalText->ComponentTags.Add(TEXT("MallGhostSignal"));
		SignalText->SetupAttachment(Ghost->GetRootComponent());
		SignalText->SetRelativeLocation(FVector(0.0, 0.0, 150.0));
		SignalText->SetHorizontalAlignment(EHTA_Center);
		SignalText->SetWorldSize(20.0f);
		SignalText->SetTextRenderColor(FColor::Cyan);
		SignalText->SetVisibility(false);
		SignalText->RegisterComponent();
	}
}

void UMallGhostVoiceReceiver::MulticastShowReaction_Implementation(const FName Phrase)
{
	if (!GetWorld() || GetNetMode() == NM_DedicatedServer) return;
	EnsureSignalComponents();
	FString Response = TEXT("I HEAR YOU");
	float Frequency = 440.0f;
	if (Phrase == TEXT("give_me_a_sign")) { Response = TEXT("A SIGN"); Frequency = 660.0f; }
	else if (Phrase == TEXT("come_here"))
	{
		const UMallGhostInteractionComponent* Interaction = GetOwner()->FindComponentByClass<UMallGhostInteractionComponent>();
		Response = Interaction && Interaction->IsInvisible() ? TEXT("I AM NEAR") : TEXT("I'M COMING");
		Frequency = 330.0f;
	}
	else if (Phrase == TEXT("leave_me_alone")) { Response = TEXT("A MOMENT OF PEACE"); Frequency = 220.0f; }
	else if (Phrase == TEXT("write_in_book")) { Response = TEXT("LOOK AT THE BOOK"); Frequency = 550.0f; }
	else if (Phrase == TEXT("who_are_you") || Phrase == TEXT("how_old_are_you"))
	{
		Response = TEXT("I HEAR YOUR QUESTION");
		Frequency = 550.0f;
	}
	if (SignalText)
	{
		SignalText->SetText(FText::FromString(Response));
		SignalText->SetVisibility(true);
	}
	SignalUntil = GetWorld()->GetTimeSeconds() + 1.5;
	SetComponentTickEnabled(true);
	PlaySignalChime(Frequency);
}

void UMallGhostVoiceReceiver::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const double Remaining = GetWorld() ? SignalUntil - GetWorld()->GetTimeSeconds() : 0.0;
	if (SignalLight)
	{
		SignalLight->SetIntensity(Remaining > 0.0 ? 1500.0f * FMath::Clamp(static_cast<float>(Remaining / 1.5), 0.0f, 1.0f) : 0.0f);
	}
	if (Remaining <= 0.0)
	{
		if (SignalText) SignalText->SetVisibility(false);
		SetComponentTickEnabled(false);
	}
}

void UMallGhostVoiceReceiver::PlaySignalChime(const float FrequencyHz)
{
	if (!GetOwner() || !GetWorld()) return;
	if (IsValid(ChimeAudio)) ChimeAudio->Stop();
	constexpr int32 SampleRate = 22050;
	constexpr float Duration = 0.24f;
	const int32 Count = FMath::RoundToInt(SampleRate * Duration);
	TArray<int16> Samples;
	Samples.SetNumUninitialized(Count);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const float Time = static_cast<float>(Index) / SampleRate;
		const float Envelope = FMath::Sin(PI * Time / Duration) * FMath::Exp(-8.0f * Time);
		Samples[Index] = static_cast<int16>(5000.0f * Envelope * FMath::Sin(2.0f * PI * FrequencyHz * Time));
	}
	USoundWaveProcedural* Wave = NewObject<USoundWaveProcedural>(this);
	Wave->SetSampleRate(SampleRate);
	Wave->NumChannels = 1;
	Wave->Duration = Duration;
	Wave->bLooping = false;
	Wave->QueueAudio(reinterpret_cast<const uint8*>(Samples.GetData()), Samples.Num() * sizeof(int16));
	ChimeAudio = MallGhostAudio::PlayAttached(GetOwner(), Wave, 0.6f);
	if (ChimeAudio)
	{
		const TWeakObjectPtr<UAudioComponent> Audio = ChimeAudio;
		GetWorld()->GetTimerManager().SetTimer(ChimeStopTimer, FTimerDelegate::CreateLambda([Audio]()
		{
			if (Audio.IsValid()) Audio->Stop();
		}), Duration + 0.05f, false);
	}
}
