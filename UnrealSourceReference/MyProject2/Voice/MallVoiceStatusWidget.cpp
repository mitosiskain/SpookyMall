#include "MallVoiceStatusWidget.h"

#include "MallMicrophoneSettings.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/HorizontalBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/PlayerController.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"

void UMallVoiceStatusWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
	UBorder* Background = WidgetTree->ConstructWidget<UBorder>();
	Background->SetBrushColor(FLinearColor(0.015f, .02f, .025f, .78f));
	Background->SetPadding(FMargin(12.f, 8.f));
	UVerticalBox* Lines = WidgetTree->ConstructWidget<UVerticalBox>();
	Background->SetContent(Lines);
	WidgetTree->RootWidget = Background;
	auto MakeText = [this](const FString& Label, const FLinearColor Color)
	{
		UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>();
		FSlateFontInfo Font = Text->GetFont(); Font.Size = 14;
		Text->SetFont(Font); Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetText(FText::FromString(Label));
		Text->SetAutoWrapText(true);
		return Text;
	};
	auto MakeLine = [Lines, &MakeText](const FLinearColor Color)
	{
		UTextBlock* Text = MakeText(FString(), Color);
		Lines->AddChildToVerticalBox(Text);
		return Text;
	};
	VoiceLine = MakeLine(FLinearColor::White);
	SpeechLine = MakeLine(FLinearColor(.6f, .85f, .9f));
	PhraseLine = MakeLine(FLinearColor(.8f, .8f, .8f));

	MicrophoneControls = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MicrophoneControls"));
	Lines->AddChildToVerticalBox(MicrophoneControls)->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));
	MicrophoneControls->AddChildToVerticalBox(MakeText(TEXT("Microphone settings - F6 or Close to return to the game"), FLinearColor::White));
	GainLabel = MakeText(FString(), FLinearColor::White);
	MicrophoneControls->AddChildToVerticalBox(GainLabel);
	GainSlider = WidgetTree->ConstructWidget<USlider>(USlider::StaticClass(), TEXT("MicrophoneGainSlider"));
	GainSlider->SetMinValue(-12.f);
	GainSlider->SetMaxValue(30.f);
	GainSlider->SetStepSize(1.f);
	GainSlider->OnValueChanged.AddDynamic(this, &UMallVoiceStatusWidget::OnMicrophoneGainChanged);
	GainSlider->OnMouseCaptureEnd.AddDynamic(this, &UMallVoiceStatusWidget::OnMicrophoneGainCaptureEnd);
	GainSlider->OnControllerCaptureEnd.AddDynamic(this, &UMallVoiceStatusWidget::OnMicrophoneGainCaptureEnd);
	MicrophoneControls->AddChildToVerticalBox(GainSlider)->SetPadding(FMargin(0.f, 6.f));

	AutoGainCheckBox = WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(), TEXT("MicrophoneAutoGain"));
	AutoGainCheckBox->SetContent(MakeText(TEXT("Automatic microphone gain"), FLinearColor::White));
	AutoGainCheckBox->OnCheckStateChanged.AddDynamic(this, &UMallVoiceStatusWidget::OnAutoGainChanged);
	MicrophoneControls->AddChildToVerticalBox(AutoGainCheckBox);
	NoiseFilterCheckBox = WidgetTree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(), TEXT("MicrophoneNoiseFilter"));
	NoiseFilterCheckBox->SetContent(MakeText(TEXT("Basic rumble/noise filter"), FLinearColor::White));
	NoiseFilterCheckBox->OnCheckStateChanged.AddDynamic(this, &UMallVoiceStatusWidget::OnNoiseFilterChanged);
	MicrophoneControls->AddChildToVerticalBox(NoiseFilterCheckBox);
	MicrophoneControls->AddChildToVerticalBox(MakeText(TEXT("Use normal speech. A higher boost can also increase background noise."), FLinearColor(.8f, .8f, .8f)));
	MicrophoneControls->AddChildToVerticalBox(MakeText(TEXT("Optional local mic test: console command mall.MicRecordNext, then one V press. Saves raw/processed WAVs locally; no upload."), FLinearColor(.8f, .8f, .8f)));

	UHorizontalBox* Buttons = WidgetTree->ConstructWidget<UHorizontalBox>();
	UButton* ResetButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("MicrophoneReset"));
	ResetButton->SetContent(MakeText(TEXT("Reset defaults"), FLinearColor::White));
	ResetButton->SetClickMethod(EButtonClickMethod::MouseDown);
	ResetButton->OnClicked.AddDynamic(this, &UMallVoiceStatusWidget::OnMicrophoneReset);
	Buttons->AddChildToHorizontalBox(ResetButton);
	UButton* CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("MicrophoneClose"));
	CloseButton->SetContent(MakeText(TEXT("Close"), FLinearColor::White));
	CloseButton->SetClickMethod(EButtonClickMethod::MouseDown);
	CloseButton->OnClicked.AddDynamic(this, &UMallVoiceStatusWidget::OnMicrophoneClose);
	Buttons->AddChildToHorizontalBox(CloseButton);
	MicrophoneControls->AddChildToVerticalBox(Buttons)->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
	MicrophoneControls->SetVisibility(ESlateVisibility::Collapsed);
	RefreshMicrophoneControls();
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UMallVoiceStatusWidget::SetStatus(const FString& Voice, const FString& Speech, const FString& Phrase)
{
	if (VoiceLine) { VoiceLine->SetText(FText::FromString(Voice)); }
	if (SpeechLine) { SpeechLine->SetText(FText::FromString(Speech)); }
	if (PhraseLine) { PhraseLine->SetText(FText::FromString(Phrase)); }
}

void UMallVoiceStatusWidget::RefreshMicrophoneControls()
{
	if (!GainSlider || !GainLabel || !AutoGainCheckBox || !NoiseFilterCheckBox) return;
	bRefreshingControls = true;
	const FMallMicrophoneOptions Options = GetDefault<UMallMicrophoneSettings>()->GetOptions();
	GainSlider->SetValue(Options.GainDb);
	GainSlider->SetIsEnabled(!Options.bAutoGain);
	GainLabel->SetText(FText::FromString(FString::Printf(TEXT("Manual boost (AGC off): %+.0f dB"), Options.GainDb)));
	AutoGainCheckBox->SetIsChecked(Options.bAutoGain);
	NoiseFilterCheckBox->SetIsChecked(Options.bNoiseFilter);
	bRefreshingControls = false;
}

void UMallVoiceStatusWidget::SetMicrophoneControlsOpen(bool bOpen)
{
	if (bMicrophoneControlsOpen == bOpen || !MicrophoneControls) return;
	if (!bOpen) OnMicrophoneGainCaptureEnd();
	bMicrophoneControlsOpen = bOpen;
	MicrophoneControls->SetVisibility(bOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	SetVisibility(bOpen ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::HitTestInvisible);
	APlayerController* PC = GetOwningPlayer();
	if (bOpen)
	{
		RefreshMicrophoneControls();
		if (PC)
		{
			bPreviousShowMouseCursor = PC->bShowMouseCursor;
			PC->bShowMouseCursor = true;
			FInputModeGameAndUI InputMode;
			InputMode.SetWidgetToFocus(TakeWidget());
			InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			InputMode.SetHideCursorDuringCapture(false);
			PC->SetInputMode(InputMode);
			SetKeyboardFocus();
		}
	}
	else if (PC)
	{
		PC->bShowMouseCursor = bPreviousShowMouseCursor;
		PC->SetInputMode(FInputModeGameOnly());
	}
}

FReply UMallVoiceStatusWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (bMicrophoneControlsOpen && (InKeyEvent.GetKey() == EKeys::F6 || InKeyEvent.GetKey() == EKeys::Escape))
	{
		SetMicrophoneControlsOpen(false);
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UMallVoiceStatusWidget::NativeDestruct()
{
	SetMicrophoneControlsOpen(false);
	Super::NativeDestruct();
}

void UMallVoiceStatusWidget::OnMicrophoneGainChanged(float Value)
{
	if (bRefreshingControls || !FMath::IsFinite(Value)) return;
	// The capture path reads these local options each buffer; save only after adjustment ends.
	GetMutableDefault<UMallMicrophoneSettings>()->GainDb = FMath::Clamp(Value, -12.f, 30.f);
	bMicrophoneGainDirty = true;
	RefreshMicrophoneControls();
}

void UMallVoiceStatusWidget::OnMicrophoneGainCaptureEnd()
{
	if (!bMicrophoneGainDirty) return;
	UMallMicrophoneSettings* Settings = GetMutableDefault<UMallMicrophoneSettings>();
	Settings->SetGainDb(Settings->GainDb);
	bMicrophoneGainDirty = false;
}

void UMallVoiceStatusWidget::OnAutoGainChanged(bool bEnabled)
{
	if (bRefreshingControls) return;
	GetMutableDefault<UMallMicrophoneSettings>()->SetAutoGain(bEnabled);
	RefreshMicrophoneControls();
}

void UMallVoiceStatusWidget::OnNoiseFilterChanged(bool bEnabled)
{
	if (!bRefreshingControls) GetMutableDefault<UMallMicrophoneSettings>()->SetNoiseFilter(bEnabled);
}

void UMallVoiceStatusWidget::OnMicrophoneReset()
{
	GetMutableDefault<UMallMicrophoneSettings>()->ResetToDefaults();
	bMicrophoneGainDirty = false;
	RefreshMicrophoneControls();
}

void UMallVoiceStatusWidget::OnMicrophoneClose()
{
	SetMicrophoneControlsOpen(false);
}
