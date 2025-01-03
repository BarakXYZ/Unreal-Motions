#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

/**
 * A per-window debug overlay that can draw an outline over any widget(s)
 * in that window. SetVisibility(EVisibility::HitTestInvisible) so it
 * never intercepts clicks.
 */
class SUMFocusDebugOverlay : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SUMFocusDebugOverlay) {}
	/** Optionally pass a default geometry */
	SLATE_ARGUMENT(TOptional<FPaintGeometry>, InitialGeometry)
	/** Optionally control default visibility */
	SLATE_ARGUMENT(EVisibility, InitialVisibility)
	SLATE_END_ARGS()

	/** Construct the overlay. */
	void Construct(const FArguments& InArgs)
	{
		// If user provided an initial geometry, store it.
		TargetGeometry = InArgs._InitialGeometry;

		// If user provided a default visibility, set it, otherwise “HitTestInvisible”
		const EVisibility DefaultVis = InArgs._InitialVisibility == EVisibility::Visible
			? EVisibility::Visible
			: EVisibility::HitTestInvisible;

		SetVisibility(DefaultVis);

		// No child content, purely a custom paint widget.
	}

	/** Sets (or updates) the geometry you want to highlight. */
	void SetTargetGeometry(const FPaintGeometry& InGeometry)
	{
		TargetGeometry = InGeometry;
		Invalidate(EInvalidateWidget::LayoutAndVolatility);
	}

	/** Clears the geometry so we draw nothing. */
	void ClearTargetGeometry()
	{
		TargetGeometry.Reset();
		Invalidate(EInvalidateWidget::LayoutAndVolatility);
	}

	//~ Begin SWidget interface
	virtual int32 OnPaint(
		const FPaintArgs&		 Args,
		const FGeometry&		 AllottedGeometry,
		const FSlateRect&		 MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32					 LayerId,
		const FWidgetStyle&		 InWidgetStyle,
		bool					 bParentEnabled) const override
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
				FLinearColor::Yellow);
		}
		return LayerId;
	}
	//~ End SWidget interface

private:
	/** The geometry we want to highlight in this window’s space. */
	TOptional<FPaintGeometry> TargetGeometry;
};
