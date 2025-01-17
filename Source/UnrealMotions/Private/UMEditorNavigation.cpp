#include "UMEditorNavigation.h"
#include "Framework/Docking/TabManager.h"
#include "Types/SlateEnums.h"
#include "UMInputHelpers.h"
#include "UMSlateHelpers.h"
#include "VimInputProcessor.h"
#include "Widgets/Docking/SDockTab.h"

DEFINE_LOG_CATEGORY(LogUMEditorNavigation);
FUMLogger FUMEditorNavigation::Logger(&LogUMEditorNavigation);

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
	// the currently focused window.
	TSharedPtr<SWindow> Win = SlateApp.GetActiveTopLevelRegularWindow();
	if (!Win.IsValid())
		return;

	if (const TSharedPtr<SDockTab> ActiveMinorTab =
			FGlobalTabmanager::Get()->GetActiveTab())
	{
		// Getting the parent window of the minor tab directly seems to always
		// return an invalid parent window so:
		if (!FUMSlateHelpers::DoesTabResideInWindow(
				Win.ToSharedRef(),
				ActiveMinorTab.ToSharedRef()))
			return;

		TWeakPtr<SWidget> DockingTabStack;
		if (FUMSlateHelpers::GetParentDockingTabStackAsWidget(
				ActiveMinorTab.ToSharedRef(), DockingTabStack))
		{
			const auto& PinDockingTab = DockingTabStack.Pin();
			if (!PinDockingTab.IsValid())
				return;

			// Bring focus to the Docking Tab Stack to achieve the built-in
			// panel navigation when it is focused.
			SlateApp.SetAllUserFocus(
				PinDockingTab, EFocusCause::Navigation);

			// For some reason the Editor Viewport wants arrows *~* try to mitigate
			if (ActiveMinorTab->GetLayoutIdentifier().ToString().Equals("LevelEditorViewport"))
			{
				FVimInputProcessor::SimulateKeyPress(SlateApp, ArrowKey);
			}
			else // Others does not want arrows! *~* wtf
			{
				// Create a widget path for the active widget
				FWidgetPath WidgetPath;
				if (SlateApp.FindPathToWidget(
						// CurrMinorTab.ToSharedRef(), WidgetPath))
						PinDockingTab.ToSharedRef(), WidgetPath))
				{
					// Craft and process the event
					FReply Reply =
						FReply::Handled().SetNavigation(
							NavDirection,
							ENavigationGenesis::Keyboard,
							ENavigationSource::FocusedWidget);

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
