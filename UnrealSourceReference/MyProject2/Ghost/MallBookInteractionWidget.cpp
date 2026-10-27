#include "Ghost/MallBookInteractionWidget.h"
#include "Ghost/MallGhostWritingBook.h"
#include "Engine/Texture2D.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

void UMallBookInteractionWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UMallBookInteractionWidget::SetBookState(AMallGhostWritingBook* Book, const FString& Prompt, const FString& Feedback, bool bInspect)
{
	EvidenceBook = Book;
	PromptText = Prompt;
	FeedbackText = Feedback;
	bInspecting = bInspect;
	InvalidateLayoutAndVolatility();
}

int32 UMallBookInteractionWidget::NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect,
	FSlateWindowElementList& DrawElements, int32 LayerId, const FWidgetStyle& Style, bool bParentEnabled) const
{
	const int32 Base = Super::NativePaint(Args, Geometry, CullingRect, DrawElements, LayerId, Style, bParentEnabled);
	const FVector2D View = Geometry.GetLocalSize();
	const FSlateBrush* White = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
	auto Box = [&](int32 Layer, FVector2D At, FVector2D Size, FLinearColor Color)
	{
		FSlateDrawElement::MakeBox(DrawElements, Layer, Geometry.ToPaintGeometry(Size, FSlateLayoutTransform(At)), White, ESlateDrawEffect::None, Color);
	};
	auto Text = [&](int32 Layer, FVector2D At, const FString& Value, int32 Size, FLinearColor Color)
	{
		FSlateDrawElement::MakeText(DrawElements, Layer, Geometry.ToPaintGeometry(FVector2D(1, 1), FSlateLayoutTransform(At)), Value,
			FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), Size), ESlateDrawEffect::None, Color);
	};
	if (bInspecting && IsValid(EvidenceBook))
	{
		Box(Base + 1, FVector2D::ZeroVector, View, FLinearColor(0, 0, 0, .56f));
		const float PageHeight = FMath::Min(View.Y * .64f, View.X * .54f / 1.36842f);
		const FVector2D Page(PageHeight * 19.5f / 28.5f, PageHeight);
		const FVector2D Start((View.X - Page.X * 2) * .5, (View.Y - Page.Y) * .43);
		Box(Base + 2, Start - FVector2D(8, 8), FVector2D(Page.X * 2 + 16, Page.Y + 16), FLinearColor(.065f, .035f, .02f));
		Box(Base + 3, Start, Page, FLinearColor(.79f, .72f, .57f));
		Box(Base + 3, Start + FVector2D(Page.X, 0), Page, FLinearColor(.79f, .72f, .57f));
		Box(Base + 4, Start + FVector2D(Page.X - 1.5, 0), FVector2D(3, Page.Y), FLinearColor(.22f, .18f, .13f));
		const float Progress = FMath::Clamp(EvidenceBook->WritingProgress, 0.f, 1.f);
		if (Progress > 0 && EvidenceBook->WritingTexture)
		{
			FSlateBrush Ink;
			Ink.SetResourceObject(EvidenceBook->WritingTexture);
			Ink.DrawAs = ESlateBrushDrawType::Image;
			Ink.ImageSize = Page;
			Ink.SetUVRegion(FBox2f(FVector2f(0, 0), FVector2f(1, Progress)));
			FSlateDrawElement::MakeBox(DrawElements, Base + 5, Geometry.ToPaintGeometry(FVector2D(Page.X, Page.Y * Progress),
				FSlateLayoutTransform(Start + FVector2D(Page.X, 0))), &Ink, ESlateDrawEffect::None, FLinearColor::White);
		}
		else if (Progress > 0)
		{
			const int32 Characters = FMath::FloorToInt(EvidenceBook->TestWriting.Len() * Progress);
			Text(Base + 5, Start + FVector2D(Page.X + 15, 35), EvidenceBook->TestWriting.Left(Characters), 22, FLinearColor(.08f, .06f, .04f));
		}
		Text(Base + 6, Start - FVector2D(0, 32), TEXT("Evidence book"), 18, FLinearColor::White);
	}
	if (!PromptText.IsEmpty() || !FeedbackText.IsEmpty())
	{
		const float Width = FMath::Min(1160.f, static_cast<float>(View.X - 48.f));
		const float BoxH = FeedbackText.IsEmpty() ? 56.f : 96.f;
		const FVector2D At(24.f, FMath::Max(24.f, View.Y - BoxH - 165.f));
		Box(Base + 7, At, FVector2D(Width, BoxH), FLinearColor(.012f, .02f, .028f, .93f));
		Box(Base + 8, At, FVector2D(6.f, BoxH), FLinearColor(1.0f, .82f, .28f, .98f));
		FSlateDrawElement::MakeText(DrawElements, Base + 9, Geometry.ToPaintGeometry(FVector2D(1, 1), FSlateLayoutTransform(At + FVector2D(18, 12))),
			PromptText, FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 20), ESlateDrawEffect::None, FLinearColor::White);
		if (!FeedbackText.IsEmpty())
		{
			FSlateDrawElement::MakeText(DrawElements, Base + 9, Geometry.ToPaintGeometry(FVector2D(1, 1), FSlateLayoutTransform(At + FVector2D(18, 52))),
				FeedbackText, FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 19), ESlateDrawEffect::None, FLinearColor(.65f, .98f, .88f));
		}
	}
	return Base + 9;
}
