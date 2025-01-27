#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateColor.h"
#include "UMSlateHelpers.h"

/**
 * A single "marker" widget that draws a label (e.g., "AF") on top of the UI
 * indicating a clickable/focusable target.
 */
class SUMHintMarker : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SUMHintMarker)
		: _TargetWidget()
		, _InWidgetLocalPosition()
		, _MarkerText()
		, _BackgroundColor(FSlateColor(FLinearColor::Yellow)) // Use FSlateColor directly
		, _TextColor(FSlateColor(FLinearColor::Black))		  // Use FSlateColor for text as well
	{
	}

	/** The widget this marker is pointing to. */
	SLATE_ARGUMENT(TSharedPtr<SWidget>, TargetWidget)

	/** Local Position In Window */
	SLATE_ATTRIBUTE(FVector2D, InWidgetLocalPosition)

	/** The text displayed on the marker. */
	SLATE_ATTRIBUTE(FText, MarkerText)

	/** Background color of the marker. */
	SLATE_ATTRIBUTE(FSlateColor, BackgroundColor)

	/** Text color of the marker. */
	SLATE_ATTRIBUTE(FSlateColor, TextColor)

	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs)
	{
		SetVisibility(EVisibility::HitTestInvisible);

		TargetWidgetWeak = InArgs._TargetWidget;
		LocalPositionInWindow = InArgs._InWidgetLocalPosition.Get();
		MarkerText = InArgs._MarkerText;

		// Build the marker's appearance
		ChildSlot
			[SNew(SBorder)
					.BorderBackgroundColor(InArgs._BackgroundColor)
					.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")) // Solid
																		   // .Padding(FMargin(2.f, 2.f))
						[SNew(STextBlock)
								.Text(MarkerText)
								.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
								.Justification(ETextJustify::Center)
								.ColorAndOpacity(InArgs._TextColor)]];
		NumHintChars = MarkerText.Get().ToString().Len();
	}

public:
	/** Weak reference to avoid extending the lifetime of the target widget. */
	TWeakPtr<SWidget> TargetWidgetWeak;

	/** The marker text displayed, e.g., "AF". */
	TAttribute<FText> MarkerText;

	/** */
	int32 NumHintChars;

	/** Where the widget should be placed on screen */
	FVector2D LocalPositionInWindow;
};
