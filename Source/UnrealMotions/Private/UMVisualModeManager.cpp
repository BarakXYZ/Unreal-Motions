#include "UMVisualModeManager.h"
#include "Engine/Scene.h"
#include "Logging/LogVerbosity.h"
#include "SSceneOutliner.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Input/SEditableText.h"
#include "Widgets/Text/SMultiLineEditableText.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/InvalidateWidgetReason.h"
#include "Widgets/Layout/SBox.h"
#include "Editor/EditorWidgets/Public/SAssetSearchBox.h"

// DEFINE_LOG_CATEGORY_STATIC(LogUMVisualModeManager, NoLoggin, All);
DEFINE_LOG_CATEGORY_STATIC(LogUMVisualModeManager, Log, All);

TSharedPtr<FUMVisualModeManager> FUMVisualModeManager::VisualModeManager =
	MakeShared<FUMVisualModeManager>();

FEditableTextStyle			FUMVisualModeManager::TextStyle;
const FSlateBrush*			FUMVisualModeManager::CaretBrush;
FSlateBrush					FUMVisualModeManager::TransparentBrush;
TSharedPtr<SUMEditableText> FUMVisualModeManager::WrappedEditableText;
TSharedPtr<SEditableText>	FUMVisualModeManager::MyEditableText;

FUMVisualModeManager::FUMVisualModeManager()
{
	Logger.SetLogCategory(&LogUMVisualModeManager);
	TextStyle = FEditableTextStyle::GetDefault();
	CaretBrush = FCoreStyle::Get().GetBrush("Icons.Info");
	// TransparentBrush.TintColor = FLinearColor(0, 0, 0, 0); // Fully transparent
	TransparentBrush.TintColor = FLinearColor(FLinearColor::Blue); // Fully transparent

	MyEditableText = SNew(SEditableText);
};

FUMVisualModeManager::~FUMVisualModeManager() {

};

bool FUMVisualModeManager::IsVisualTextSelected(FSlateApplication& SlateApp)
{
	const FName EditTextType = "SEditableText";

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
