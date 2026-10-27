#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MallCartInteractionWidget.generated.h"

class AMallPushCart;

/** Non-modal cart controls, enlarged button prompts, and full-screen cart computer overlay. */
UCLASS()
class MYPROJECT2_API UMallCartInteractionWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void SetPrompt(const FString& Prompt, const FString& Feedback, AMallPushCart* InFullScreenComputerCart = nullptr);
	void SetVitalityState(float InStaminaPct, float InHealthPct, bool bInSeatedOnBench);
protected:
	virtual void NativeOnInitialized() override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect,
		FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool bEnabled) const override;
private:
	UPROPERTY(Transient)
	TObjectPtr<AMallPushCart> FullScreenComputerCart;
	FString PromptText;
	FString FeedbackText;
	float StaminaPercent = 100.0f;
	float HealthPercent = 100.0f;
	bool bSeatedOnBench = false;
};
