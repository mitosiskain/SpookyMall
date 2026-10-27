#include "Equipment/MallCartInteractionWidget.h"
#include "Equipment/MallPushCart.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

void UMallCartInteractionWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UMallCartInteractionWidget::SetPrompt(const FString& Prompt, const FString& Feedback, AMallPushCart* InFullScreenComputerCart)
{
	PromptText = Prompt;
	FeedbackText = Feedback;
	FullScreenComputerCart = InFullScreenComputerCart;
	InvalidateLayoutAndVolatility();
}

void UMallCartInteractionWidget::SetVitalityState(float InStaminaPct, float InHealthPct, bool bInSeatedOnBench)
{
	StaminaPercent = FMath::Clamp(InStaminaPct, 0.0f, 100.0f);
	HealthPercent = FMath::Clamp(InHealthPct, 0.0f, 100.0f);
	bSeatedOnBench = bInSeatedOnBench;
}

int32 UMallCartInteractionWidget::NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect,
	FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool bEnabled) const
{
	const int32 Base = Super::NativePaint(Args, Geometry, CullingRect, Elements, Layer, Style, bEnabled);
	const FVector2D View = Geometry.GetLocalSize();
	const FSlateBrush* White = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));

	auto DrawBox = [&](int32 InLayer, FVector2D Pos, FVector2D BoxSize, FLinearColor Color)
	{
		FSlateDrawElement::MakeBox(
			Elements, InLayer, Geometry.ToPaintGeometry(BoxSize, FSlateLayoutTransform(Pos)),
			White, ESlateDrawEffect::None, Color);
	};
	auto DrawTextLine = [&](int32 InLayer, FVector2D Pos, const FString& Value, int32 FontSize, bool bBold, FLinearColor Color)
	{
		FSlateDrawElement::MakeText(
			Elements, InLayer, Geometry.ToPaintGeometry(FVector2D(1.0, 1.0), FSlateLayoutTransform(Pos)),
			Value, FCoreStyle::GetDefaultFontStyle(bBold ? TEXT("Bold") : TEXT("Regular"), FontSize),
			ESlateDrawEffect::None, Color);
	};

	// 1. Full-Screen Cart Computer Terminal View (when local player is the exclusive ComputerOperator)
	if (IsValid(FullScreenComputerCart) && FullScreenComputerCart->bHasComputerTerminal)
	{
		DrawBox(Base + 1, FVector2D::ZeroVector, View, FLinearColor(0.005f, 0.01f, 0.015f, 0.78f));

		const float TermW = FMath::Clamp(View.X * 0.84f, 760.0f, 1540.0f);
		const float TermH = FMath::Clamp(View.Y * 0.80f, 520.0f, 920.0f);
		const FVector2D TermOrigin((View.X - TermW) * 0.5f, (View.Y - TermH) * 0.46f);

		// Outer rugged monitor bezel + inner CRT/LCD screen
		DrawBox(Base + 2, TermOrigin - FVector2D(10.0, 10.0), FVector2D(TermW + 20.0, TermH + 20.0), FLinearColor(0.08f, 0.10f, 0.12f, 0.98f));
		DrawBox(Base + 3, TermOrigin, FVector2D(TermW, TermH), FLinearColor(0.015f, 0.045f, 0.055f, 0.96f));
		DrawBox(Base + 4, TermOrigin, FVector2D(TermW, 58.0), FLinearColor(0.03f, 0.14f, 0.16f, 0.98f));

		DrawTextLine(
			Base + 5,
			TermOrigin + FVector2D(24.0, 14.0),
			TEXT("MALL PARANORMAL INVESTIGATION TERMINAL  —  SHARED CART LOGBOOK  [EXCLUSIVE 1-PLAYER LOCK]"),
			21,
			true,
			FLinearColor(0.45f, 1.0f, 0.88f));

		// 3 Interactive Tab Headers
		const uint8 Tab = FullScreenComputerCart->ActiveComputerTab;
		const float TabW = (TermW - 64.0f) / 3.0f;
		const TCHAR* TabTitles[3] = {
			TEXT("[TAB 1] SHARED EVIDENCE JOURNAL"),
			TEXT("[TAB 2] SAVED PHOTOS & TEAM PAYOUT"),
			TEXT("[TAB 3] REMOTE CAMERAS & SENSORS")
		};
		for (int32 i = 0; i < 3; ++i)
		{
			const bool bActiveTab = (Tab == i);
			const FVector2D TabPos = TermOrigin + FVector2D(20.0f + i * (TabW + 12.0f), 74.0f);
			DrawBox(
				Base + 4,
				TabPos,
				FVector2D(TabW, 48.0f),
				bActiveTab ? FLinearColor(0.08f, 0.36f, 0.32f, 0.98f) : FLinearColor(0.02f, 0.08f, 0.10f, 0.90f));
			DrawTextLine(
				Base + 5,
				TabPos + FVector2D(16.0f, 11.0f),
				TabTitles[i],
				19,
				bActiveTab,
				bActiveTab ? FLinearColor(1.0f, 0.95f, 0.45f) : FLinearColor(0.65f, 0.82f, 0.85f));
		}

		using namespace EMallGhostEvidenceBit;
		const uint8 SharedMask = FullScreenComputerCart->SharedEvidenceMask;
		auto CheckBadge = [&](uint8 Bit, const TCHAR* Label) -> FString
		{
			return FString::Printf(TEXT("%s  %s"), (SharedMask & Bit) ? TEXT("[CONFIRMED]") : TEXT("[    -    ]"), Label);
		};

		const FVector2D BodyStart = TermOrigin + FVector2D(32.0f, 148.0f);
		if (Tab == 0)
		{
			DrawTextLine(Base + 5, BodyStart, TEXT("PARANORMAL EVIDENCE CHECKLIST (Auto-Synced from Team Equipment & Cart Rack):"), 22, true, FLinearColor::White);

			DrawTextLine(Base + 5, BodyStart + FVector2D(12.0, 48.0),  CheckBadge(EMFLevel5,      TEXT("EMF Level 5 Spike (5-LED Reader)")), 21, (SharedMask & EMFLevel5) != 0, (SharedMask & EMFLevel5) ? FLinearColor(0.35f, 1.0f, 0.55f) : FLinearColor(0.78f, 0.86f, 0.88f));
			DrawTextLine(Base + 5, BodyStart + FVector2D(12.0, 90.0),  CheckBadge(UVFingerprints, TEXT("Ultraviolet (UV) Handprints / Footprints")), 21, (SharedMask & UVFingerprints) != 0, (SharedMask & UVFingerprints) ? FLinearColor(0.35f, 1.0f, 0.55f) : FLinearColor(0.78f, 0.86f, 0.88f));
			DrawTextLine(Base + 5, BodyStart + FVector2D(12.0, 132.0), CheckBadge(FreezingTemps,  TEXT("Freezing Temperatures (< 0.0 C / 32.0 F)")), 21, (SharedMask & FreezingTemps) != 0, (SharedMask & FreezingTemps) ? FLinearColor(0.35f, 1.0f, 0.55f) : FLinearColor(0.78f, 0.86f, 0.88f));
			DrawTextLine(Base + 5, BodyStart + FVector2D(12.0, 174.0), CheckBadge(SpiritBox,      TEXT("Spirit Box Paranormal Voice Response")), 21, (SharedMask & SpiritBox) != 0, (SharedMask & SpiritBox) ? FLinearColor(0.35f, 1.0f, 0.55f) : FLinearColor(0.78f, 0.86f, 0.88f));
			DrawTextLine(Base + 5, BodyStart + FVector2D(12.0, 216.0), CheckBadge(GhostWriting,   TEXT("Ghost Writing in Placed Evidence Book")), 21, (SharedMask & GhostWriting) != 0, (SharedMask & GhostWriting) ? FLinearColor(0.35f, 1.0f, 0.55f) : FLinearColor(0.78f, 0.86f, 0.88f));
			DrawTextLine(Base + 5, BodyStart + FVector2D(12.0, 258.0), CheckBadge(MotionDetected, TEXT("Infrared Motion Sensor Presence Trigger")), 21, (SharedMask & MotionDetected) != 0, (SharedMask & MotionDetected) ? FLinearColor(0.35f, 1.0f, 0.55f) : FLinearColor(0.78f, 0.86f, 0.88f));
		}
		else if (Tab == 1)
		{
			DrawTextLine(Base + 5, BodyStart, TEXT("TEAM EVIDENCE PHOTO LOG & BOUNTY PAYOUT:"), 22, true, FLinearColor::White);
			DrawTextLine(Base + 5, BodyStart + FVector2D(12.0, 56.0),
				FString::Printf(TEXT("Total Saved Photos Captured:   %d"), FullScreenComputerCart->SyncedPhotoCount),
				22, true, FLinearColor(0.45f, 0.95f, 1.0f));
			DrawTextLine(Base + 5, BodyStart + FVector2D(12.0, 104.0),
				FString::Printf(TEXT("Total Team Bounty Payout:      $%d"), FullScreenComputerCart->SyncedTotalPhotoReward),
				24, true, FLinearColor(1.0f, 0.90f, 0.35f));
			DrawTextLine(Base + 5, BodyStart + FVector2D(12.0, 156.0),
				FString::Printf(TEXT("Highest Rated Photo Subject:   %d-Star  —  %s"), FullScreenComputerCart->SyncedBestPhotoStars, *FullScreenComputerCart->SyncedLastPhotoSubject),
				21, false, FLinearColor(0.85f, 0.96f, 0.90f));
			DrawTextLine(Base + 5, BodyStart + FVector2D(12.0, 216.0),
				TEXT("Tip: Place Photo Cameras in haunted stores and trigger them remotely from Tab 3 with [V]!"),
				19, false, FLinearColor(0.72f, 0.86f, 0.88f));
		}
		else
		{
			DrawTextLine(Base + 5, BodyStart, TEXT("LIVE REMOTE CAMERAS & DEPLOYED SENSOR TELEMETRY:"), 22, true, FLinearColor::White);
			DrawBox(Base + 4, BodyStart + FVector2D(0.0, 48.0), FVector2D(TermW - 64.0f, 110.0f), FLinearColor(0.01f, 0.09f, 0.12f, 0.95f));
			DrawTextLine(Base + 5, BodyStart + FVector2D(20.0, 82.0),
				FullScreenComputerCart->SyncedRemoteFeedStatus,
				23, true, FLinearColor(0.45f, 1.0f, 0.88f));
			DrawTextLine(Base + 5, BodyStart + FVector2D(12.0, 186.0),
				TEXT("Press [V] to remotely fire the shutter of any placed Photo Camera or cycle sensor feeds!"),
				20, false, FLinearColor(1.0f, 0.90f, 0.45f));
		}

		// Deduced Ghost Archetype Banner
		const FVector2D DeductionPos = TermOrigin + FVector2D(24.0f, TermH - 132.0f);
		DrawBox(Base + 4, DeductionPos, FVector2D(TermW - 48.0f, 54.0f), FLinearColor(0.04f, 0.18f, 0.15f, 0.98f));
		DrawTextLine(
			Base + 5,
			DeductionPos + FVector2D(20.0f, 12.0f),
			FString::Printf(TEXT("GHOST DEDUCTION:   %s"), *FullScreenComputerCart->DeducedGhostArchetype),
			23,
			true,
			FLinearColor(1.0f, 0.92f, 0.40f));

		// Bottom Large Button Controls Bar
		const FVector2D FooterPos = TermOrigin + FVector2D(0.0f, TermH - 60.0f);
		DrawBox(Base + 4, FooterPos, FVector2D(TermW, 60.0f), FLinearColor(0.02f, 0.10f, 0.12f, 0.98f));
		DrawTextLine(
			Base + 5,
			FooterPos + FVector2D(24.0f, 16.0f),
			TEXT("[C] Switch Screen Tab     |     [V] Cycle Remote Feed / Remote Photo     |     [F] or [G] Exit Full-Screen Terminal"),
			20,
			true,
			FLinearColor::White);

		return Base + 6;
	}

	// 2. Persistent Stamina & Vitality Bar (Bottom-Left) + Non-Modal HUD Controls Banner
	const bool bHasPrompt = !PromptText.IsEmpty() || !FeedbackText.IsEmpty();
	TArray<FString> PromptLines;
	if (bHasPrompt)
	{
		PromptText.ParseIntoArrayLines(PromptLines, false);
	}
	const int32 TotalLines = bHasPrompt ? (FMath::Max(1, PromptLines.Num()) + (FeedbackText.IsEmpty() ? 0 : 1)) : 0;
	const float LineHeight = 31.0f;
	const float BoxHeight = bHasPrompt ? (22.0f + TotalLines * LineHeight) : 0.0f;
	const float BoxWidth = FMath::Min(1180.0f, static_cast<float>(View.X - 40.0));
	const FVector2D At(24.0, bHasPrompt ? FMath::Clamp(View.Y - BoxHeight - 28.0, 64.0, View.Y - 90.0) : (View.Y - 28.0));

	// Draw Stamina & Health Status Bar above the prompt box (or at bottom-left when idle)
	{
		const float StamPanelW = bSeatedOnBench ? 640.0f : 440.0f;
		const float StamPanelH = 40.0f;
		const FVector2D StamOrigin(24.0f, FMath::Max(16.0f, static_cast<float>(At.Y) - StamPanelH - 10.0f));
		DrawBox(Base + 1, StamOrigin, FVector2D(StamPanelW, StamPanelH), FLinearColor(0.01f, 0.02f, 0.03f, 0.90f));

		const float BarW = 210.0f;
		const float BarH = 14.0f;
		const FVector2D BarOrigin = StamOrigin + FVector2D(132.0f, 13.0f);
		DrawBox(Base + 2, BarOrigin, FVector2D(BarW, BarH), FLinearColor(0.08f, 0.10f, 0.12f, 0.95f));

		const float FillFrac = FMath::Clamp(StaminaPercent / 100.0f, 0.0f, 1.0f);
		const FLinearColor FillColor = bSeatedOnBench
			? FLinearColor(0.25f, 1.0f, 0.65f, 0.98f)
			: (FillFrac > 0.45f ? FLinearColor(0.20f, 0.90f, 0.80f, 0.95f)
				: (FillFrac > 0.18f ? FLinearColor(1.0f, 0.78f, 0.22f, 0.95f) : FLinearColor(1.0f, 0.25f, 0.25f, 0.98f)));
		if (FillFrac > 0.005f)
		{
			DrawBox(Base + 3, BarOrigin, FVector2D(BarW * FillFrac, BarH), FillColor);
		}

		DrawTextLine(Base + 4, StamOrigin + FVector2D(12.0f, 9.0f),
			FString::Printf(TEXT("STAMINA %.0f%%"), StaminaPercent),
			15, true, FillColor);

		const FString RightStatus = bSeatedOnBench
			? FString::Printf(TEXT("HP %.0f%%  |  RESTING ON BENCH [3RD-PERSON +24%%/s]"), HealthPercent)
			: FString::Printf(TEXT("HP %.0f%%"), HealthPercent);
		DrawTextLine(Base + 4, StamOrigin + FVector2D(356.0f, 9.0f),
			RightStatus,
			15, bSeatedOnBench, bSeatedOnBench ? FLinearColor(0.45f, 1.0f, 0.78f) : FLinearColor(0.85f, 0.92f, 0.95f));
	}

	if (!bHasPrompt)
	{
		return Base + 5;
	}

	// Dark high-contrast panel + bright cyan left accent bar
	DrawBox(Base + 1, At, FVector2D(BoxWidth, BoxHeight), FLinearColor(0.01f, 0.018f, 0.026f, 0.93f));
	DrawBox(Base + 2, At, FVector2D(6.0, BoxHeight), FLinearColor(0.20f, 0.92f, 0.82f, 0.98f));

	float CurY = 11.0f;
	for (int32 LineIdx = 0; LineIdx < PromptLines.Num(); ++LineIdx)
	{
		const bool bIsControlLine = (LineIdx == PromptLines.Num() - 1) || PromptLines[LineIdx].Contains(TEXT(":"));
		const FLinearColor LineColor = (LineIdx == 0)
			? FLinearColor(1.0f, 0.96f, 0.78f)
			: FLinearColor(0.95f, 0.98f, 1.0f);
		DrawTextLine(Base + 3, At + FVector2D(18.0, CurY), PromptLines[LineIdx], 20, bIsControlLine, LineColor);
		CurY += LineHeight;
	}
	if (!FeedbackText.IsEmpty())
	{
		DrawTextLine(Base + 3, At + FVector2D(18.0, CurY + 2.0f), FeedbackText, 19, true, FLinearColor(0.45f, 1.0f, 0.85f));
	}
	return Base + 5;
}
