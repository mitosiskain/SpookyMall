#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MallVoiceStatusWidget.generated.h"

class UTextBlock;
class UVerticalBox;
class USlider;
class UCheckBox;

UCLASS()
class MYPROJECT2_API UMallVoiceStatusWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetStatus(const FString& Voice, const FString& Speech, const FString& Phrase);
	bool IsMicrophoneControlsOpen() const { return bMicrophoneControlsOpen; }
	void SetMicrophoneControlsOpen(bool bOpen);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	void RefreshMicrophoneControls();
	UFUNCTION() void OnMicrophoneGainChanged(float Value);
	UFUNCTION() void OnMicrophoneGainCaptureEnd();
	UFUNCTION() void OnAutoGainChanged(bool bEnabled);
	UFUNCTION() void OnNoiseFilterChanged(bool bEnabled);
	UFUNCTION() void OnMicrophoneReset();
	UFUNCTION() void OnMicrophoneClose();

	UPROPERTY(Transient) TObjectPtr<UTextBlock> VoiceLine;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> SpeechLine;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> PhraseLine;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> MicrophoneControls;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> GainLabel;
	UPROPERTY(Transient) TObjectPtr<USlider> GainSlider;
	UPROPERTY(Transient) TObjectPtr<UCheckBox> AutoGainCheckBox;
	UPROPERTY(Transient) TObjectPtr<UCheckBox> NoiseFilterCheckBox;
	bool bMicrophoneControlsOpen = false;
	bool bRefreshingControls = false;
	bool bMicrophoneGainDirty = false;
	bool bPreviousShowMouseCursor = false;
};
