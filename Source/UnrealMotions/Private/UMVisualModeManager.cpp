#include "UMVisualModeManager.h"
#include "Widgets/Input/SEditableText.h"

FUMVisualModeManager::FUMVisualModeManager() {};
FUMVisualModeManager::~FUMVisualModeManager() {};

bool FUMVisualModeManager::IsVisualTextSelected(FSlateApplication& SlateApp)
{
	const FName EditTextType = "SEditableTExt";

	TSharedPtr<SWidget> FocusedWidget = SlateApp.GetUserFocusedWidget(0);
	if (!FocusedWidget.IsValid())
		return false;
	if (!FocusedWidget->GetType().IsEqual(EditTextType))
		return false;

	TSharedPtr<SEditableText> WidgetAsEditText =
		StaticCastSharedPtr<SEditableText>(FocusedWidget);

	// WidgetAsEditText->SetTextBlockStyle();
	return WidgetAsEditText->AnyTextSelected();
}
