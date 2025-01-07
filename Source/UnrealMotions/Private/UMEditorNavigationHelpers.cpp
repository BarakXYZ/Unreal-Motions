#include "UMEditorNavigationHelpers.h"
#include "Types/SlateEnums.h"

TSharedPtr<FUMEditorNavigationHelpers> FUMEditorNavigationHelpers::EditorNavigationHelpers = MakeShared<FUMEditorNavigationHelpers>();

FUMEditorNavigationHelpers::FUMEditorNavigationHelpers()
{
}

FUMEditorNavigationHelpers::~FUMEditorNavigationHelpers()
{
}

const TSharedPtr<FUMEditorNavigationHelpers> FUMEditorNavigationHelpers::Get()
{
	return EditorNavigationHelpers;
}

bool FUMEditorNavigationHelpers::GetWidgetInDirection(
	const TSharedRef<SWidget> InBaseWidget,
	EKeys Direction, TSharedPtr<SWidget>& OutWidget)
{
	FSlateApplication& SlateApp = FSlateApplication::Get();

	// EUINavigation::Up;
	// // SlateApp.NavigateFromWidgetUnderCursor(0);
	return true;
}
