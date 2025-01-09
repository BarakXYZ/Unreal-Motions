#include "UMEditorNavigation.h"
#include "Types/SlateEnums.h"
#include "UMLogger.h"
#include "UMInputHelpers.h"
#include "UMSlateHelpers.h"
#include "UMInputPreProcessor.h"

DEFINE_LOG_CATEGORY(LogUMEditorNavigation);

TSharedPtr<FUMEditorNavigation> FUMEditorNavigation::EditorNavigation =
	MakeShared<FUMEditorNavigation>();

FUMEditorNavigation::FUMEditorNavigation()
{
	FocusManager = FUMFocusManager::Get();
}

FUMEditorNavigation::~FUMEditorNavigation()
{
}

const TSharedPtr<FUMEditorNavigation> FUMEditorNavigation::Get()
{
	return EditorNavigation;
}

void FUMEditorNavigation::NavigatePanelTabs(
	FSlateApplication& SlateApp, const FKeyEvent& InKey)
{
	// Get the desired navigation from the in Vim Key
	EUINavigation NavDirection;
	if (!FUMInputHelpers::GetUINavigationFromVimKey(InKey.GetKey(), NavDirection))
		return;

	FKey ArrowKey;
	if (!FUMInputHelpers::GetArrowKeyFromVimKey(InKey.GetKey(), ArrowKey))
		return;

	// Window to operate on: keep an eye of this. This should be aligned with
	// the currently focused window though.
	TSharedPtr<SWindow> Win = SlateApp.GetActiveTopLevelRegularWindow();
	if (!Win.IsValid())
		return;

	// TArray<TSharedRef<SWindow>> VisWins;
	// SlateApp.GetAllVisibleWindowsOrdered(VisWins);
	// if (VisWins.IsEmpty())
	// 	return;
	// Win = VisWins.Last(); // test

	const auto& FocusManager = FUMFocusManager::Get();
	if (const auto& CurrMinorTab = FocusManager->ActiveMinorTab.Pin())
	{
		// FUMLogger::NotifySuccess(
		// 	FText::FromString(CurrMinorTab->GetLayoutIdentifier().ToString()));

		TWeakPtr<SWidget> DockingTabStack;
		if (FUMSlateHelpers::GetParentDockingTabStackAsWidget(
				CurrMinorTab.ToSharedRef(), DockingTabStack))
		{
			// Alert the focus manager to bypass the next focus event in SetTab
			FocusManager->bBypassAutoFocusLastActiveWidget = true;

			// TODO: Refactor
			// if (const TSharedPtr<SWidget> PinnedTabStack = DockingTabStack.Pin())
			// {
			// 	SlateApp.SetAllUserFocus(
			// 		PinnedTabStack, EFocusCause::Navigation);

			// 	// PinnedTabStack->OnNavigation(
			// 	// 	PinnedTabStack->GetCachedGeometry());
			// }

			// Bring focus to the Docking Tab Stack to achieve the built-in
			// panel navigation when it is focused.
			// SlateApp.ClearAllUserFocus(); // TODO: Check if this is really needed!
			SlateApp.SetAllUserFocus(
				DockingTabStack.Pin(), EFocusCause::Navigation);

			// For some reason the Editor Viewport wants arrows *~* try to mitigate
			if (CurrMinorTab->GetLayoutIdentifier().ToString().Equals("LevelEditorViewport"))
			{
				FUMInputPreProcessor::SimulateKeyPress(SlateApp, ArrowKey);
			}
			else // Others does not want arrows! *~* wtf
			{
				// Create a widget path for the active widget
				FWidgetPath WidgetPath;
				if (SlateApp.FindPathToWidget(CurrMinorTab.ToSharedRef(), WidgetPath))
				{
					// Craft and process the event
					FReply Reply = FReply::Handled()
									   .SetNavigation(
										   NavDirection,
										   ENavigationGenesis::Keyboard, ENavigationSource::FocusedWidget);

					FSlateApplication::Get().ProcessReply(
						WidgetPath, // CurrentEventPath
						Reply,		// TheReply
						nullptr,	// WidgetsUnderMouse (not needed here)
						nullptr,	// InMouseEvent (optional)
						0			// UserIndex
					);
				}
			}
		}
	}
}
