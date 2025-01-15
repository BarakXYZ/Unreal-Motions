#include "SUMStatusBarWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "UMStatusBarWidget"

EVimMode SUMStatusBarWidget::StaticCurrVimMode = EVimMode::Insert;

void SUMStatusBarWidget::Construct(const FArguments& InArgs)
{
	ChildSlot
		[SNew(SButton)
				.ContentPadding(FMargin(6.0f, 0.0f))
				.ToolTipText(this, &SUMStatusBarWidget::GetStatusBarTooltip)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.OnClicked(InArgs._OnClicked)
					[SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							.HAlign(HAlign_Center)
								[SAssignNew(StatusBarImage, SImage)
										.Image(this, &SUMStatusBarWidget::GetCurrentStatusBarIcon)]
						+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							.Padding(FMargin(5, 0, 0, 0))
								[SAssignNew(StatusBarText, STextBlock)
										.TextStyle(&FAppStyle::Get().GetWidgetStyle<FTextBlockStyle>("NormalText"))
										.Text(this, &SUMStatusBarWidget::GetStatusBarText)]]];

	// Update the icon when the on vim mode changed.
	FUMInputPreProcessor::Get()->RegisterOnVimModeChanged(
		[this](const EVimMode CurrentVimMode) {
			UpdateStatusBar(CurrentVimMode);
		});
}

SUMStatusBarWidget::~SUMStatusBarWidget()
{
	FUMInputPreProcessor::Get()->UnregisterOnVimModeChanged(this);
}

void SUMStatusBarWidget::UpdateStatusBar(const EVimMode CurrentVimMode)
{
	if (StatusBarImage.IsValid())
	{
		StatusBarImage->Invalidate(EInvalidateWidget::Layout);
	}
	StaticCurrVimMode = CurrentVimMode;
}

FVimModeInfo SUMStatusBarWidget::GetVimModeInfo(EVimMode Mode) const
{
	switch (Mode)
	{
		case EVimMode::Normal:
			return {
				FAppStyle::GetBrush("TextureEditor.GreenChannel.Small"),
				LOCTEXT("Normal_Mode", "Normal")
			};
		case EVimMode::Visual:
			return {
				FAppStyle::GetBrush("TextureEditor.RedChannel.Small"),
				LOCTEXT("Visual_Mode", "Visual")
			};
		case EVimMode::Insert:
		default:
			return {
				FAppStyle::GetBrush("TextureEditor.AlphaChannel.Small"),
				LOCTEXT("Insert_Mode", "Insert")
			};
	}
}

const FSlateBrush* SUMStatusBarWidget::GetCurrentStatusBarIcon() const
{
	return GetVimModeInfo(StaticCurrVimMode).Icon;
}

FText SUMStatusBarWidget::GetStatusBarText() const
{
	return FText::Format(
		LOCTEXT("Mode_Status", "Unreal Motions [{0}]"),
		GetVimModeInfo(StaticCurrVimMode).DisplayText);
}

FText SUMStatusBarWidget::GetStatusBarTooltip() const
{
	return FText::Format(
		LOCTEXT("Mode_Tooltip", "Current Vim Mode: {0}"),
		GetVimModeInfo(StaticCurrVimMode).DisplayText);
}

#undef LOCTEXT_NAMESPACE
