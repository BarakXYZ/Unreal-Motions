#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SEditableText.h"

class SUMEditableText : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SUMEditableText) {}
	SLATE_ARGUMENT(TSharedPtr<SEditableText>, EditableTextToWrap)
	SLATE_ARGUMENT(FSlateBrush, NewCaretImage) // The user provides a brush by value
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		// Construct the base SCompoundWidget
		// SCompoundWidget::Construct(SCompoundWidget::FArguments()); // This also doesn't work

		// Ensure the wrapped widget is valid
		check(InArgs._EditableTextToWrap.IsValid());

		// Keep a copy of the brush in this widget so we can safely pass its pointer around
		MyCaretBrush = InArgs._NewCaretImage;

		// Create (or retrieve) a style and set the CaretImage pointer
		FEditableTextStyle NewStyle = FCoreStyle::Get().GetWidgetStyle<FEditableTextStyle>("NormalEditableText");
		NewStyle.CaretImage = MyCaretBrush; // Safe, because MyCaretBrush lives as long as this widget

		// Extract properties from the existing widget
		TSharedPtr<SEditableText> ExistingWidget = InArgs._EditableTextToWrap;
		const FText				  ExistingText = ExistingWidget->GetText();
		const FText				  ExistingHintText = ExistingWidget->GetHintText();
		const bool				  bIsReadOnly = ExistingWidget->IsTextReadOnly();
		const bool				  bIsPassword = ExistingWidget->IsTextPassword();

		// Construct your child widget
		ChildSlot
			[SAssignNew(EditableText, SEditableText)
					.Text(ExistingText)
					.HintText(ExistingHintText)
					.Style(&NewStyle)
					.IsReadOnly(bIsReadOnly)
					.IsPassword(bIsPassword)];
	}

	/** Provide access to the newly created editable text widget */
	TSharedPtr<SEditableText> GetEditableText() const
	{
		return EditableText;
	}

private:
	/** Pointer to the new SEditableText you built */
	TSharedPtr<SEditableText> EditableText;

	/** A copy of the brush so its memory is valid as long as SUMEditableText is alive */
	FSlateBrush MyCaretBrush;
};
