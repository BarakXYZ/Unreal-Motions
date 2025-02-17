#include "UMEditorCommands.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Interfaces/IMainFrameModule.h"
#include "LevelEditor.h"
#include "LevelEditorActions.h"
#include "Logging/LogVerbosity.h"
#include "Templates/SharedPointer.h"
#include "VimInputProcessor.h"
#include "UMFocuserEditorSubsystem.h"
#include "UMSlateHelpers.h"
#include "Widgets/Input/SButton.h"
#include "Editor.h"
#include "UMInputHelpers.h"
#include "Framework/Docking/TabManager.h"
#include "SUMSearch.h"

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

void FUMEditorCommands::UndoRedo(
	FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	FVimInputProcessor::SimulateKeyPress(SlateApp,
		FKey(InKeyEvent.GetKey() == EKeys::U ? EKeys::Z : EKeys::Y),
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

	TSharedPtr<SWindow> RefWin = RefTab->GetParentWindow();
	if (!RefWin)
		return;

	// Fetching the first (maybe only) instance of this (parent of our target)
	TSharedPtr<SWidget> FoundWidget;
	if (!FUMSlateHelpers::TraverseFindWidget(
			RefWin.ToSharedRef(), FoundWidget, "SToolBarButtonBlock"))
		return;

	// Locate the button within it
	if (FUMSlateHelpers::TraverseFindWidget(
			FoundWidget.ToSharedRef(), FoundWidget, "SButton"))
	{
		TSharedPtr<SButton> Button =
			StaticCastSharedPtr<SButton>(FoundWidget);
		if (!Button)
			return;
		Button->SimulateClick(); // Will spawn a fetchable Menu Box (&& SWindow)

		// Get this window so we can search for the final buttons within it
		TArray<TSharedRef<SWindow>> ChildRefWins = RefWin->GetChildWindows();
		if (ChildRefWins.Num() == 0)
			return;

		// Find all menu entry buttons in the found window
		TArray<TSharedPtr<SWidget>> FoundButtons;
		if (!FUMSlateHelpers::TraverseFindWidget(
				ChildRefWins[0], FoundButtons, "SMenuEntryButton"))
			return;

		if (FoundButtons.Num() == 0 || !FoundButtons[1].IsValid())
			return;
		TSharedPtr<SButton> EntryButton = // SMenuEntry inherits from SButton
			StaticCastSharedPtr<SButton>(FoundButtons[1]);
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
				SlateApp.SetAllUserFocus(EntryButton, EFocusCause::Navigation);
			},
			0.01f, // 10ms delay seems to be enough (need to check on mac)
			false  // Do not loop
		);
	}
}

void FUMEditorCommands::FocusSearchBox(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	// static const FString SearchBoxType{ "SSearchBox" };
	// static const FString AssetSearchBoxType{ "SAssetSearchBox" };
	// static const FString FilterSearchBoxType{ "SFilterSearchBox" };
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

	TArray<TSharedPtr<SWidget>> EditableTexts;
	if (FUMSlateHelpers::TraverseFindWidget(
			// SearchInWidget, SearchBox, SearchBoxType))
			SearchContent.ToSharedRef(), EditableTexts, EditableTextType))
	{
		Logger.Print(TabLabel, ELogVerbosity::Verbose, true);
		if (TabLabel.StartsWith("Content Browser"))
		{
			if (const auto PinText = EditableTexts.Last())
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
			if (Text)
			{
				if (Text->GetVisibility() == EVisibility::Visible)
				{
					SlateApp.SetAllUserFocus(
						Text, EFocusCause::Navigation);
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

void FUMEditorCommands::FindNearestSearchBox(
	FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	static const FString EditableType = "SEditableText";
	if (const TSharedPtr<SWidget> FocusedWidget =
			SlateApp.GetUserFocusedWidget(0))
	{
		TSharedPtr<SWidget> FoundWidget;
		if (FUMSlateHelpers::TraverseFindWidgetUpwards(
				FocusedWidget.ToSharedRef(), FoundWidget, EditableType))
		{
			SlateApp.SetAllUserFocus(FoundWidget.ToSharedRef(), EFocusCause::Navigation);
			Logger.Print("Editable Text Found!", ELogVerbosity::Verbose, true);
		}
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

void FUMEditorCommands::ResetEditorToDefaultLayout()
{
	static FString				 TitleBarType = "SWindowTitleBar";
	static const TArray<FString> MenuEntries{
		"Window", "Load Layout", "Default Editor Layout"
	};

	const TSharedPtr<SWindow> RootWin = FGlobalTabmanager::Get()->GetRootWindow();
	if (!RootWin.IsValid())
		return;

	// Get SWindowTitleBar; this is where we want to perform our searching
	TSharedPtr<SWidget> WinBar;
	if (!FUMSlateHelpers::TraverseFindWidget(RootWin.ToSharedRef(), WinBar, TitleBarType))
		return;

	if (WinBar.IsValid())
		FUMSlateHelpers::SimulateMenuClicks(WinBar.ToSharedRef(), MenuEntries);
}

void FUMEditorCommands::RunUtilityWidget()
{
	const TSharedPtr<SDockTab> MajorTab = FUMSlateHelpers::GetActiveMajorTab();
	if (!MajorTab.IsValid())
		return;

	TArray<TSharedPtr<SWidget>> FoundTexts;
	if (!FUMSlateHelpers::TraverseFindWidget(
			MajorTab->GetContent(), FoundTexts, "STextBlock"))
		return;

	for (const TSharedPtr<SWidget>& Text : FoundTexts)
	{
		const TSharedPtr<STextBlock> TB = StaticCastSharedPtr<STextBlock>(Text);
		if (TB->GetText().ToString().Equals("Run Utility Widget"))
		{
			TSharedPtr<SWidget> ButtonAsWidget;
			if (!FUMSlateHelpers::TraverseFindWidgetUpwards(
					TB.ToSharedRef(), ButtonAsWidget, "SButton"))
				return;

			const TSharedPtr<SButton> AsButton = StaticCastSharedPtr<SButton>(ButtonAsWidget);

			AsButton->SimulateClick();
		}
	}
}

void FUMEditorCommands::Search(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	// Pop an SEditable text to enter the search
	FVimInputProcessor::Get()->SetVimMode(FSlateApplication::Get(), EVimMode::Insert);
	SUMSearch::Open();
}

void FUMEditorCommands::FocusWindowRoot(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	TSharedPtr<SWindow> RootWindow = FGlobalTabmanager::Get()->GetRootWindow();
	if (!RootWindow.IsValid())
		return;

	TSharedPtr<SWindow> ActiveWindow = SlateApp.GetActiveTopLevelRegularWindow();
	if (!ActiveWindow.IsValid())
		return;

	if (ActiveWindow->GetId() == RootWindow->GetId())
		return; // Already at root window

	// Alert focuser
	UUMFocuserEditorSubsystem* Focuser =
		GEditor->GetEditorSubsystem<UUMFocuserEditorSubsystem>();
	Focuser->HandleOnWindowChanged(ActiveWindow.ToSharedRef(), RootWindow.ToSharedRef());
}
