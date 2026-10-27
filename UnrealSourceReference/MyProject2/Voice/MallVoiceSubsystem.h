#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MallSpeechRecognizer.h"
#include "MallVoiceSubsystem.generated.h"

class UMallVoiceComponent;
class UMallVoiceStatusWidget;
class APlayerController;

/** Joins one push-to-talk turn without treating quiet gaps as phrase boundaries. */
class FMallSpeechUtteranceBuffer
{
public:
	static constexpr int32 MaxSamples = 128000; // Eight seconds of mono 16 kHz PCM.
	void Append(TConstArrayView<int16> Samples, bool bEnd, TFunctionRef<void(TArray<int16>&&)> OnUtterance);
	void Reset();
private:
	void Finish(TFunctionRef<void(TArray<int16>&&)> OnUtterance);
	TArray<int16> BufferedSamples;
	bool bContainsSpeech = false;
};

/** Runtime-only wiring; existing player classes do not need reparenting. */
UCLASS()
class MYPROJECT2_API UMallVoiceSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	UMallVoiceSubsystem();
	virtual ~UMallVoiceSubsystem() override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }
	UFUNCTION(BlueprintPure, Category="Mall Voice") FString GetSpeechStatus() const { return SpeechStatus; }
	UFUNCTION(BlueprintPure, Category="Mall Voice") bool IsSpeechReady() const;
	UFUNCTION(BlueprintPure, Category="Mall Voice") UMallVoiceComponent* GetLocalVoice() const;
protected:
	virtual bool DoesSupportWorldType(EWorldType::Type Type) const override;
private:
	void RefreshEndpoints();
	void InitializeLocal(UMallVoiceComponent* Voice);
	void ReceivePCM(const TArray<int16>& Samples, bool bEnd);
	void SubmitUtterance(TArray<int16>&& Audio);
	void HandleServerPhrase(APlayerController* Speaker, FName PhraseId, float Confidence);
	void UpdateStatus();
	TUniquePtr<FMallSpeechRecognizer> Recognizer;
	TWeakObjectPtr<UMallVoiceComponent> LocalVoice;
	TSet<TWeakObjectPtr<UMallVoiceComponent>> BoundEndpoints;
	FMallSpeechUtteranceBuffer Utterance;
	bool bTalkKeyWasDown = false;
	bool bMicrophoneControlsWereOpen = false;
	double NextRefreshTime = 0.;
	double NextStatusTime = 0.;
	FString SpeechStatus = TEXT("Loading offline phrase recognition...");
	FString LastPhraseStatus;
	UPROPERTY(Transient) TObjectPtr<UMallVoiceStatusWidget> StatusWidget;
};
