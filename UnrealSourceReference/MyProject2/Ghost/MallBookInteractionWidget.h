#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "MallBookInteractionWidget.generated.h"

class AMallGhostWritingBook;

/** Local, non-modal inspection of the actual replicated book evidence. */
UCLASS()
class MYPROJECT2_API UMallBookInteractionWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void SetBookState(AMallGhostWritingBook* Book, const FString& Prompt, const FString& Feedback, bool bInspect);
protected:
	virtual void NativeOnInitialized() override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect,
		FSlateWindowElementList& DrawElements, int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const override;
private:
	UPROPERTY(Transient) TObjectPtr<AMallGhostWritingBook> EvidenceBook;
	FString PromptText;
	FString FeedbackText;
	bool bInspecting = false;
};
