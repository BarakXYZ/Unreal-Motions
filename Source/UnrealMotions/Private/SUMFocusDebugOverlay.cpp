#include "SUMFocusDebugOverlay.h"
#include "UMLogger.h"

FUMOnOutlineOverlayVisibilityChanged SUMFocusDebugOverlay::OnOutlineOverlayVisibilityChanged;

/** Construct the overlay. */
void SUMFocusDebugOverlay::Construct(const FArguments& InArgs)
{
	// If user provided an initial geometry, store it.
	TargetGeometry = InArgs._InitialGeometry;

	// If user provided a default visibility, set it, otherwise “HitTestInvisible”
	const EVisibility DefaultVis =
		(FVimInputProcessor::VimMode == EVimMode::Insert)
		? EVisibility::Hidden
		: EVisibility::HitTestInvisible;

	SetVisibility(DefaultVis);

	/**
	 * In case we will want to toggle this directly (with no connection to the
	 * current Vim Mode)
	 */
	OnOutlineOverlayVisibilityChanged.AddRaw(
		this, &SUMFocusDebugOverlay::HandleOnVisibilityChanged);

	// Refactor to listen from Vim Subsystem
	FVimInputProcessor::Get()->OnVimModeChanged.AddRaw(
		this, &SUMFocusDebugOverlay::HandleOnVimModeChanged);

	// No child content, purely a custom paint widget.
}

void SUMFocusDebugOverlay::SetTargetGeometry(const FPaintGeometry& InGeometry)
{
	TargetGeometry = InGeometry;
	Invalidate(EInvalidateWidget::LayoutAndVolatility);
}

void SUMFocusDebugOverlay::ClearTargetGeometry()
{
	TargetGeometry.Reset();
	Invalidate(EInvalidateWidget::LayoutAndVolatility);
}

void SUMFocusDebugOverlay::ToggleOutlineActiveColor(bool bIsActive)
{
	if (bIsActive)
		// Set to active (pure purple)
		OutlineColor = FLinearColor(1.0f, 0.0f, 1.0f, 1.0f);

	else
		// Set to inactive (gray-purple)
		OutlineColor = FLinearColor(0.2f, 0.0f, 0.2f, 1.0f);
}

void SUMFocusDebugOverlay::ToggleVisibility(bool bIsVisible)
{
	OnOutlineOverlayVisibilityChanged.Broadcast(bIsVisible);
}

void SUMFocusDebugOverlay::HandleOnVisibilityChanged(bool bIsVisible)
{
	Visibility = bIsVisible
		? EVisibility::HitTestInvisible
		: EVisibility::Hidden;

	SetVisibility(Visibility);
	Invalidate(EInvalidateWidgetReason::Visibility);
}

void SUMFocusDebugOverlay::HandleOnVimModeChanged(EVimMode NewVimMode)
{
	switch (NewVimMode)
	{
		case EVimMode::Insert:
		{
			HandleOnVisibilityChanged(false);
			break;
		}
		case EVimMode::Normal:
		{
			HandleOnVisibilityChanged(true);
			break;
		}
		default:
			break;
	}
}

//~ Begin SWidget interface
int32 SUMFocusDebugOverlay::OnPaint(
	const FPaintArgs&		 Args,
	const FGeometry&		 AllottedGeometry,
	const FSlateRect&		 MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32					 LayerId,
	const FWidgetStyle&		 InWidgetStyle,
	bool					 bParentEnabled) const
{
	if (TargetGeometry.IsSet())
	{
		// We assume TargetGeometry is already in the same coordinate space as the window.
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			++LayerId, // increment layer
			TargetGeometry.GetValue(),
			FCoreStyle::Get().GetBrush(TEXT("Debug.Border")),
			ESlateDrawEffect::None,
			// FLinearColor::Yellow);
			OutlineColor);
	}
	return LayerId;
}
//~ End SWidget interface
