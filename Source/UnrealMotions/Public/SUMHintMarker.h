#pragma once

#include "Math/Color.h"
#include "Styling/SlateTypes.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateColor.h"
#include "Brushes/SlateRoundedBoxBrush.h"
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
		, _TextColor(FSlateColor(FLinearColor::Black)) // Use FSlateColor for text as well
	{
	}

	/** The widget this marker is pointing to. */
	SLATE_ARGUMENT(TSharedPtr<SWidget>, TargetWidget)

	/** Local Position In Window */
	SLATE_ATTRIBUTE(FVector2D, InWidgetLocalPosition)

	/** The text displayed on the marker. */
	SLATE_ATTRIBUTE(FString, MarkerText)

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
		NumHintChars = MarkerText.Get().Len();

		const FSlateRoundedBoxBrush& BorderBrush = GetBorderBrush();
		const FTextBlockStyle&		 DefaultStyle = GetDefaultTextBlockStyle();

		// Create the horizontal box to hold all STextBlock widgets
		TSharedPtr<SHorizontalBox> HorizontalBox = SNew(SHorizontalBox);

		for (const TCHAR Char : MarkerText.Get())
		{
			// Create a new text block for each character
			TSharedPtr<STextBlock> CharTextBlock =
				SNew(STextBlock)
					.TextStyle(&DefaultStyle)
					.Text(FText::FromString(FString(1, &Char)))
					.Justification(ETextJustify::Center);

			// Add the text block to the horizontal box
			HorizontalBox->AddSlot()
				.AutoWidth()
					[CharTextBlock.ToSharedRef()];

			// Store the text block in the array for later manipulation
			TextBlocks.Add(CharTextBlock);
		}

		// Set the child slot to contain the horizontal box
		ChildSlot
			[SNew(SBorder)
					.BorderImage(&BorderBrush)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
						[HorizontalBox.ToSharedRef()]];
	}

	const FSlateRoundedBoxBrush& GetBorderBrush()
	{
		static UTexture2D* GradientTexture = LoadObject<UTexture2D>(
			nullptr,
			// TEXT("/Engine/EngineResources/GradientTexture0.GradientTexture0")
			TEXT("/Script/Engine.Texture2D'/UnrealMotions/Textures/HintMarkerGradientBackground.HintMarkerGradientBackground'") // Path inside .uasset
		);

		static FSlateRoundedBoxBrush RoundBorder(
			// Piping the path directly doesn't seem to have any effect?
			// TEXT("/Script/Engine.Texture2D'/UnrealMotions/Textures/HintMarkerGradientBackground.HintMarkerGradientBackground'"),
			FLinearColor::White,				   // Tint
			4.0f,								   // Corner Radius
			FLinearColor(0.75f, 0.5f, 0.0f, 1.0f), // OutlineColor
			1.25f);								   // OutlineWidth

		RoundBorder.SetResourceObject(GradientTexture);

		return RoundBorder;
	}

	const FTextBlockStyle& GetDefaultTextBlockStyle()
	{
		static const FTextBlockStyle DefaultStyle =
			FTextBlockStyle()
				.SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 10))
				.SetColorAndOpacity(FSlateColor(FLinearColor::Black))
				.SetShadowOffset(FVector2D(0.0f, 1.0f))
				.SetShadowColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.6f));

		return DefaultStyle;
	}

	const FTextBlockStyle& GetPressedTextBlockStyle()
	{
		static const FTextBlockStyle PressedStyle =
			FTextBlockStyle()
				.SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 10))
				.SetColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.5f))
				.SetShadowOffset(FVector2D(0.0f, 1.0f))
				.SetShadowColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.6f));

		return PressedStyle;
	}

	void VisualizePressedKey(bool bIncKeyPressed)
	{
		if (bIncKeyPressed)
		{
			if (TextBlocks.IsValidIndex(PressedKeyIndex))
			{
				TextBlocks[PressedKeyIndex]->SetTextStyle(&GetPressedTextBlockStyle());

				++PressedKeyIndex;
			}
		}
		else
		{
			if (TextBlocks.IsValidIndex(PressedKeyIndex - 1))
			{
				TextBlocks[PressedKeyIndex - 1]->SetTextStyle(&GetDefaultTextBlockStyle());

				--PressedKeyIndex;
			}
		}
	}

public:
	/** Weak reference to avoid extending the lifetime of the target widget. */
	TWeakPtr<SWidget> TargetWidgetWeak;

	/** The marker text displayed, e.g., "AF". */
	TAttribute<FString> MarkerText;

	/** */
	int32 NumHintChars;

	/** Where the widget should be placed on screen */
	FVector2D LocalPositionInWindow;

	TArray<TSharedPtr<STextBlock>> TextBlocks;

	int32 PressedKeyIndex{ 0 };
};
