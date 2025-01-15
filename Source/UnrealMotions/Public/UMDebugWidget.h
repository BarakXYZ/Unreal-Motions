#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class SUMDebugWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SUMDebugWidget) {}
	SLATE_ARGUMENT(TWeakPtr<SWidget>, TargetWidget)			  // Target widget for debug visualization
	SLATE_ARGUMENT(TOptional<FPaintGeometry>, CustomGeometry) // Custom geometry for debug
	SLATE_END_ARGS()

	/** Construct the widget with the provided arguments. */
	void Construct(const FArguments& InArgs)
	{
		TargetWidget = InArgs._TargetWidget;
		CustomGeometry = InArgs._CustomGeometry;
	}

	/** Update the target widget dynamically. */
	void SetTargetWidget(const TWeakPtr<SWidget>& NewTargetWidget)
	{
		TargetWidget = NewTargetWidget;
		Invalidate(EInvalidateWidget::LayoutAndVolatility); // Ensure the widget updates its visual state
	}

	/** Update the custom geometry dynamically. */
	void SetCustomGeometry(const TOptional<FPaintGeometry>& NewCustomGeometry)
	{
		CustomGeometry = NewCustomGeometry;
		Invalidate(EInvalidateWidget::LayoutAndVolatility); // Ensure the widget updates its visual state
	}

	/** Paint the widget with a debug outline. */
	virtual int32 OnPaint(
		const FPaintArgs&		 Args,
		const FGeometry&		 AllottedGeometry,
		const FSlateRect&		 MyClippingRect,
		FSlateWindowElementList& OutDrawElements,
		int32					 LayerId,
		const FWidgetStyle&		 InWidgetStyle,
		bool					 bParentEnabled) const override
	{
		// Use custom geometry if provided; fallback to the target widget's geometry.
		const FPaintGeometry TargetPaintGeometry = CustomGeometry.IsSet()
			? CustomGeometry.GetValue()
			: TargetWidget.IsValid() ? TargetWidget.Pin()->GetCachedGeometry().ToPaintGeometry()
									 : AllottedGeometry.ToPaintGeometry();

		// Draw a border or debug box.
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId,
			TargetPaintGeometry,
			FCoreStyle::Get().GetBrush(TEXT("Debug.Border")),
			ESlateDrawEffect::None,
			// FLinearColor::Red		// Red border for debug visualization
			FLinearColor::Yellow // Red border for debug visualization
		);

		return LayerId + 1;
	}

private:
	TWeakPtr<SWidget>		  TargetWidget;	  // Widget to debug
	TOptional<FPaintGeometry> CustomGeometry; // Optional custom geometry
};
