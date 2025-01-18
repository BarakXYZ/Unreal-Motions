#include "UMEditorCommands.h"
#include "Framework/Notifications/NotificationManager.h"
#include "LevelEditor.h"
#include "LevelEditorActions.h"
#include "VimInputProcessor.h"
#include "UMFocuserEditorSubsystem.h"
#include "UMSlateHelpers.h"
#include "Widgets/Input/SButton.h"
#include "Editor.h"
#include "UMInputHelpers.h"
#include "Framework/Docking/TabManager.h"
#include "TabDragDropOperation.h"
#include "DragAndDrop/AssetDragDropOp.h"

// DEFINE_LOG_CATEGORY_STATIC(LogUMEditorCommands, NoLogging, All);
DEFINE_LOG_CATEGORY_STATIC(LogUMEditorCommands, Log, All);
FUMLogger FUMEditorCommands::Logger(&LogUMEditorCommands);

void FUMEditorCommands::ClearAllDebugMessages()
{
	// Remove regular debug messages
	if (GEngine)
		GEngine->ClearOnScreenDebugMessages();

	// Remove all Slate Notifications
	FSlateNotificationManager& NotifyManager = FSlateNotificationManager::Get();

	TArray<TSharedRef<SWindow>> Wins;
	NotifyManager.GetWindows(Wins);

	for (const auto& Win : Wins)
		Win->RequestDestroyWindow();

	// NotifyManager.SetAllowNotifications(true);
}

void FUMEditorCommands::ToggleAllowNotifications()
{
	FSlateNotificationManager& NotifyManager = FSlateNotificationManager::Get();
	if (NotifyManager.AreNotificationsAllowed())
	{
		NotifyManager.SetAllowNotifications(false);
		Logger.Print("ToggleAllowNotificaiton: False");
	}
	else
	{
		NotifyManager.SetAllowNotifications(true);
		Logger.Print("ToggleAllowNotificaiton: True");
	}
}

void FUMEditorCommands::Undo(
	FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	FVimInputProcessor::SimulateKeyPress(SlateApp,
		FKey(EKeys::Z),
		FModifierKeysState(
			false, false,
			true, true, // Only Ctrl down (Ctrl+Z) TODO: Check on MacOS
			false, false, false, false, false));
}

void FUMEditorCommands::OpenOutputLog()
{
	static const FName LOG = "OutputLog";

	UUMFocuserEditorSubsystem* Focuser =
		GEditor->GetEditorSubsystem<UUMFocuserEditorSubsystem>();

	if (Focuser)
		Focuser->ActivateNewInvokedTab(FSlateApplication::Get(),
			FGlobalTabmanager::Get()->TryInvokeTab(FTabId(LOG)));
}

void FUMEditorCommands::OpenContentBrowser(
	FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	const static FString DEFAULT_TAB_INDEX = "1";
	FString				 ContentBrowserId = "ContentBrowserTab";
	FString				 TabNum;

	if (FUMInputHelpers::GetStrDigitFromKey(
			InKeyEvent.GetKey(), TabNum, 1, 4)) // Valid tabs are only 1-4
		ContentBrowserId += TabNum;
	else
		ContentBrowserId += DEFAULT_TAB_INDEX;

	UUMFocuserEditorSubsystem* Focuser =
		GEditor->GetEditorSubsystem<UUMFocuserEditorSubsystem>();

	if (Focuser)
		Focuser->ActivateNewInvokedTab(SlateApp,
			FGlobalTabmanager::Get()->TryInvokeTab(FName(*ContentBrowserId)));
}

void FUMEditorCommands::OpenPreferences(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	static const FName PREFERENCES = "EditorSettings";
	// FUMFocusManager::ActivateNewInvokedTab(FSlateApplication::Get(),
	// 	FGlobalTabmanager::Get()->TryInvokeTab(FTabId(PREFERENCES)));
	if (const auto Tab =
			FGlobalTabmanager::Get()->TryInvokeTab(FTabId(PREFERENCES)))
	{
		SlateApp.ClearAllUserFocus();
		SlateApp.SetAllUserFocus(Tab->GetContent(), EFocusCause::Navigation);
		// FGlobalTabmanager::Get()->OnTabForegrounded(nullptr, nullptr);
		// FGlobalTabmanager::Get()->SetActiveTab(Tab);
	}
}

void FUMEditorCommands::OpenWidgetReflector(
	FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	static const FName REFLECTOR = "WidgetReflector";

	TSharedPtr<SDockTab> RefTab = // Invoke the Widget Reflector tab
		FGlobalTabmanager::Get()->TryInvokeTab(FTabId(REFLECTOR));

	UUMFocuserEditorSubsystem* Focuser =
		GEditor->GetEditorSubsystem<UUMFocuserEditorSubsystem>();

	if (Focuser)
		Focuser->ActivateNewInvokedTab(FSlateApplication::Get(), RefTab);
	else
		return;

	// FGlobalTabmanager::Get()->GetTabManagerForMajorTab(RefTab)->GetPrivateApi().GetLiveDockAreas();
	TSharedPtr<SWindow> RefWin = RefTab->GetParentWindow();
	if (!RefWin)
		return;

	// Fetching the first (maybe only) instance of this (parent of our target)
	TWeakPtr<SWidget> FoundWidget;
	if (!FUMSlateHelpers::TraverseWidgetTree(
			RefWin, FoundWidget, "SToolBarButtonBlock"))
		return;

	// Locate the button within it
	if (FUMSlateHelpers::TraverseWidgetTree(
			FoundWidget.Pin(), FoundWidget, "SButton"))
	{
		TSharedPtr<SButton> Button =
			StaticCastSharedPtr<SButton>(FoundWidget.Pin());
		if (!Button)
			return;
		Button->SimulateClick(); // Will spawn a fetchable Menu Box (&& SWindow)

		// Get this window so we can search for the final buttons within it
		TArray<TSharedRef<SWindow>> ChildRefWins = RefWin->GetChildWindows();
		if (ChildRefWins.Num() == 0)
			return;

		// Find all menu entry buttons in the found window
		TArray<TWeakPtr<SWidget>> FoundButtons;
		if (!FUMSlateHelpers::TraverseWidgetTree(
				ChildRefWins[0], FoundButtons, "SMenuEntryButton"))
			return;

		if (FoundButtons.Num() == 0 || !FoundButtons[1].IsValid())
			return;
		TSharedPtr<SButton> EntryButton = // SMenuEntry inherits from SButton
			StaticCastSharedPtr<SButton>(FoundButtons[1].Pin());
		if (!EntryButton.IsValid())
			return;

		bool bIsImmediatelyTriggered = InKeyEvent.IsShiftDown();

		// We need a slight delay before drawing focus to this new button
		FTimerHandle TimerHandle;
		GEditor->GetTimerManager()->SetTimer(
			TimerHandle,
			[EntryButton, bIsImmediatelyTriggered]() {
				FSlateApplication& SlateApp = FSlateApplication::Get();
				SlateApp.ClearAllUserFocus();
				if (bIsImmediatelyTriggered)
					EntryButton->SimulateClick();
				// TODO: This is fragile - let's add a manual focus visualizer
				SlateApp.SetAllUserFocus(EntryButton, EFocusCause::Navigation);
			},
			0.01f, // 10ms delay seems to be enough (need to check on mac)
			false  // Do not loop
		);
		Logger.Print(FString::FromInt(FoundButtons.Num()));
	}
}

void FUMEditorCommands::FocusSearchBox(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	static const FString SearchBoxType{ "SSearchBox" };
	static const FString AssetSearchBoxType{ "SAssetSearchBox" };
	static const FString FilterSearchBoxType{ "SFilterSearchBox" };
	static const FString EditableTextType{ "SEditableText" };

	TSharedPtr<SWidget> SearchContent;
	FString				TabLabel = "Invalid";

	if (const auto ActiveWindow = SlateApp.GetActiveTopLevelRegularWindow())
	{
		if (const auto MinorTab = FUMSlateHelpers::GetActiveMinorTab())
		{
			if (FUMSlateHelpers::DoesTabResideInWindow(ActiveWindow.ToSharedRef(), MinorTab.ToSharedRef()))
			{
				SearchContent = MinorTab->GetContent();
				TabLabel = MinorTab->GetTabLabel().ToString();
			}
			else
			{
				SearchContent = ActiveWindow->GetContent();
			}
		}
	}
	else
		return;

	TArray<TWeakPtr<SWidget>> EditableTexts;
	if (FUMSlateHelpers::TraverseWidgetTree(
			// SearchInWidget, SearchBox, SearchBoxType))
			SearchContent, EditableTexts, EditableTextType))
	{
		Logger.Print(TabLabel, ELogVerbosity::Verbose, true);
		if (TabLabel.StartsWith("Content Browser"))
		{
			if (const auto PinText = EditableTexts.Last().Pin())
			{
				SlateApp.SetAllUserFocus(
					PinText, EFocusCause::Navigation);
				Logger.Print("Found Editable in Content Browser to Focus on!",
					ELogVerbosity::Verbose, true);
				return;
			}
		}
		for (const auto& Text : EditableTexts)
		{
			if (const auto PinText = Text.Pin())
			{
				if (PinText->GetVisibility() == EVisibility::Visible)
				{
					SlateApp.SetAllUserFocus(
						PinText, EFocusCause::Navigation);
					Logger.Print("Found Editable Search Box to Focus On!",
						ELogVerbosity::Verbose, true);
					return;
				}
			}
		}
	}
	else
	{
		Logger.Print("Could not find any Editable Text to focus on.",
			ELogVerbosity::Error, true);
	}
}

void FUMEditorCommands::DeleteItem(
	FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	// TODO: Delegate to switch Vim mode to Normal

	FVimInputProcessor::Get()->SetVimMode(SlateApp, EVimMode::Normal);
	FKeyEvent DeleteEvent(
		FKey(EKeys::Delete),
		FModifierKeysState(),
		0, 0, 0, 0);
	FVimInputProcessor::ToggleNativeInputHandling(true);
	SlateApp.ProcessKeyDownEvent(DeleteEvent); // Will just block the entire process until the delete window is handled, thus not really helping.
}

void FUMEditorCommands::NavigateNextPrevious(
	FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	const auto& FocusedWidget = SlateApp.GetKeyboardFocusedWidget();
	if (!FocusedWidget.IsValid())
		return;

	FWidgetPath CurrentPath;
	if (!SlateApp.FindPathToWidget(FocusedWidget.ToSharedRef(), CurrentPath))
		return;

	// N = Next (aka Tab) | P = Previous (aka Shift + Tab)
	const bool	 bIsForwardNavigation = InKeyEvent.GetKey() == EKeys::N;
	const FReply NavigationReply =
		FReply::Handled().SetNavigation(
			bIsForwardNavigation ? EUINavigation::Next : EUINavigation::Previous,
			ENavigationGenesis::Keyboard,
			ENavigationSource::FocusedWidget);

	FSlateApplication::Get().ProcessReply(CurrentPath, NavigationReply,
		nullptr, nullptr);
}

void FUMEditorCommands::OpenLevelBlueprint()
{
	// Get the LevelEditor module
	FLevelEditorModule& LevelEditorModule =
		FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");

	TWeakPtr<ILevelEditor> LevelEditorInstance =
		LevelEditorModule.GetLevelEditorInstance();

	if (const TSharedPtr<ILevelEditor> LevelEditor = LevelEditorInstance.Pin())
	{
		LevelEditor->GetLevelEditorActions()->TryExecuteAction(
			FLevelEditorCommands::Get().OpenLevelBlueprint.ToSharedRef());
	}
}

void FUMEditorCommands::DockCurrentTabInRootWindow()
{
	// Get our target (currently focused) tab.
	TSharedPtr<SDockTab> ActiveMajorTab = FUMSlateHelpers::GetActiveMajorTab();
	if (!ActiveMajorTab.IsValid())
		return;

	FSlateApplication& SlateApp = FSlateApplication::Get();

	// Get the SWindow (to get the Native Window)
	TSharedPtr<SWindow> ActiveWindow = SlateApp.GetActiveTopLevelWindow();
	if (!ActiveWindow.IsValid())
		return;

	// We will need the Native Window as a parameter for the MouseDown Simulation
	TSharedPtr<FGenericWindow> GenericActiveWindow = ActiveWindow->GetNativeWindow();
	if (!GenericActiveWindow.IsValid())
		return;

	// Fetch the Level Editor TabWell
	FLevelEditorModule& LevelEditorModule =
		FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");

	TSharedPtr<ILevelEditor> LevelEditorPtr = LevelEditorModule.GetLevelEditorInstance().Pin();
	if (!LevelEditorPtr.IsValid())
		return;

	// Get the Tab Manager of the Level Editor
	TSharedPtr<FTabManager> LevelEditorTabManager =
		LevelEditorPtr->GetTabManager();
	if (!LevelEditorTabManager.IsValid())
		return;

	// We need the Owner Tab to access the TabWell parent
	const TSharedPtr<SDockTab> LevelEditorTab =
		LevelEditorTabManager->GetOwnerTab();
	if (!LevelEditorTab.IsValid())
		return;

	// Finally get our targeted TabWell (where we will drag our tab to)
	const TSharedPtr<SWidget> LevelEditorTabWell =
		LevelEditorTab->GetParentWidget();
	if (!LevelEditorTabWell.IsValid())
		return;

	// Store the origin position so we can restore it at the end
	const FVector2f MouseOriginPos = SlateApp.GetCursorPos();
	// Hide the mouse cursor to reduce flickering (not very important - but nice)
	SlateApp.GetPlatformApplication()->Cursor->Show(false);

	// Position the mouse over the target (current) tab and simulate a Mouse Press
	FVector2f MajorTabCenterPos =
		FUMSlateHelpers::GetWidgetCenterScreenSpacePosition(
			ActiveMajorTab.ToSharedRef());

	// Move to the center of the tab we want to drag and store current position
	SlateApp.SetCursorPos(FVector2d(MajorTabCenterPos));
	const FVector2f MouseTargetTabPos = SlateApp.GetCursorPos(); // Current Pos

	FPointerEvent MousePressEvent(
		0,
		0,
		MouseTargetTabPos,
		MouseTargetTabPos,
		TSet<FKey>({ EKeys::LeftMouseButton }),
		EKeys::LeftMouseButton,
		0,
		FModifierKeysState());

	// Simulate Press
	SlateApp.ProcessMouseButtonDownEvent(GenericActiveWindow, MousePressEvent);

	// Now we want to simulate the move event (to simulate dragging)
	// Our position hasn't changed so we can continue to use the MouseTargetTabPos

	// Dragging horizontally by 10 units just to trigger the drag at all.
	FVector2f EndPosition = MouseTargetTabPos + FVector2f(10.0f, 0.0f);

	FPointerEvent MouseMoveEvent(
		0,									  // PointerIndex
		EndPosition,						  // ScreenSpacePosition (Goto)
		MouseTargetTabPos,					  // LastScreenSpacePosition (Prev)
		TSet<FKey>{ EKeys::LeftMouseButton }, // Currently pressed buttons
		EKeys::Invalid,						  // No new button pressed
		0.0f,								  // WheelDelta
		FModifierKeysState()				  // ModifierKeys
	);

	// Simulate Move (drag)
	SlateApp.ProcessMouseMoveEvent(MouseMoveEvent);

	// Now we want to drag our tab to the new TabWell and append it to the end
	const FVector2f TabWellTargetPosition =
		FUMSlateHelpers::GetWidgetTopRightScreenSpacePosition(
			LevelEditorTabWell.ToSharedRef());

	// We need a slight delay to let the drag adjust...
	// This seems to be important; else it won't catch up and break.
	TSharedRef<FTimerManager> TimerManager = GEditor->GetTimerManager();

	FTimerHandle TimerHandle_MoveMouseToTabWell;
	TimerManager->SetTimer(
		TimerHandle_MoveMouseToTabWell,
		[&SlateApp, TabWellTargetPosition]() {
			SlateApp.SetCursorPos(FVector2D(TabWellTargetPosition));
		},
		0.025f,
		false);

	// Release the tab in the new TabWell
	FTimerHandle   TimerHandle_ReleaseMouseButton;
	FTimerDelegate Delegate_ReleaseMouseButton;
	Delegate_ReleaseMouseButton.BindStatic(
		&FUMInputHelpers::ReleaseMouseButtonAtCurrentPos, EKeys::LeftMouseButton);

	TimerManager->SetTimer(
		TimerHandle_ReleaseMouseButton,
		Delegate_ReleaseMouseButton,
		0.05f,
		false);

	// Restore the original position of the cursor to where it was pre-shenanigans
	FTimerHandle TimerHandle_MoveMouseToOrigin;
	TimerManager->SetTimer(
		TimerHandle_MoveMouseToOrigin,
		[&SlateApp, MouseOriginPos]() {
			SlateApp.SetCursorPos(FVector2D(MouseOriginPos));
			SlateApp.GetPlatformApplication()->Cursor->Show(true); // Reveal <3
		},
		0.06f,
		false);
}
