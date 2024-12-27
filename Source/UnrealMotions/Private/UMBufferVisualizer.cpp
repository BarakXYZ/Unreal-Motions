#include "UMBufferVisualizer.h"
#include "Types/SlateEnums.h"
#include "Widgets/Layout/SBorder.h"

void SUMBufferVisualizer::Construct(const FArguments& InArgs)
{
	// Set overlay visibility
	SOverlay::Construct(SOverlay::FArguments().Visibility(EVisibility::HitTestInvisible));

	// Create the border widget
	TSharedPtr<SBorder> Border =
		SNew(SBorder)
			.HAlign(EHorizontalAlignment::HAlign_Fill)
			// .HAlign(EHorizontalAlignment::HAlign_Center)
			.VAlign(EVerticalAlignment::VAlign_Fill)
			// .VAlign(EVerticalAlignment::VAlign_Center)
			.BorderBackgroundColor(
				FSlateColor(FLinearColor(0.0077f, 0.0077f, 0.0128f, 1.0f)))
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.Padding(FMargin(4.0f));

	// Create the text block to add as content inside the border
	BufferText =
		SNew(STextBlock)
			.Text(FText::FromString("Buffer Visualizer Test")) // Placeholder text
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 24));

	// Add the text block as content of the border
	Border->SetContent(
		SNew(SBox)
			.Padding(FMargin(16.0f)) // Set padding around the text block
				[BufferText.ToSharedRef()]);

	// Add the border to the overlay slot
	this->AddSlot()
		.HAlign(EHorizontalAlignment::HAlign_Fill)
		.VAlign(EVerticalAlignment::VAlign_Bottom)
			[Border.ToSharedRef()];
}

void SUMBufferVisualizer::UpdateBuffer(const FString& NewBuffer)
{
	if (BufferText.IsValid())
	{
		BufferText->SetText(FText::FromString(NewBuffer));
	}
}

void SUMBufferVisualizer::SetWidgetVisibility(EVisibility InVisibility)
{
	SWidget::SetVisibility(InVisibility);
}
