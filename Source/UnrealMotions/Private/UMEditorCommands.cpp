#include "UMEditorCommands.h"
#include "Framework/Notifications/NotificationManager.h"
#include "UMInputPreProcessor.h"
#include "UMFocusManager.h"
#include "UMSlateHelpers.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Input/SButton.h"
#include "Editor.h"

// DEFINE_LOG_CATEGORY_STATIC(LogUMEditorCommands, NoLogging, All);
DEFINE_LOG_CATEGORY_STATIC(LogUMEditorCommands, Log, All);

TSharedPtr<FUMEditorCommands> FUMEditorCommands::EditorCommands =
	MakeShared<FUMEditorCommands>();

FUMEditorCommands::FUMEditorCommands()
{
	Logger.SetLogCategory(&LogUMEditorCommands);
}

FUMEditorCommands::~FUMEditorCommands()
{
}

const TSharedPtr<FUMEditorCommands> FUMEditorCommands::Get()
{
	return EditorCommands;
}

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
		EditorCommands->Logger.Print("ToggleAllowNotificaiton: False");
	}
	else
	{
		NotifyManager.SetAllowNotifications(true);
		EditorCommands->Logger.Print("ToggleAllowNotificaiton: True");
	}
}

void FUMEditorCommands::Undo(
	FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	// FUMLogger::NotifySuccess(FText::FromString("UNDO"));
	FUMInputPreProcessor::SimulateKeyPress(SlateApp,
		FKey(EKeys::Z),
		FModifierKeysState(
			false, false,
			true, true, // Only Ctrl down (Ctrl+Z) TODO: Check on MacOS
			false, false, false, false, false));
}

void FUMEditorCommands::OpenOutputLog()
{
	static const FName LOG = "OutputLog";
	FUMFocusManager::ActivateNewInvokedTab(FSlateApplication::Get(),
		FGlobalTabmanager::Get()->TryInvokeTab(FTabId(LOG)));
}

void FUMEditorCommands::OpenContentBrowser(
	FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	const static FString DEFAULT_TAB_INDEX = "1";
	FString				 ContentBrowserId = "ContentBrowserTab";
	FString				 TabNum;

	if (FUMInputPreProcessor::GetStrDigitFromKey(
			InKeyEvent.GetKey(), TabNum, 1, 4)) // Valid tabs are only 1-4
		ContentBrowserId += TabNum;
	else
		ContentBrowserId += DEFAULT_TAB_INDEX;

	FUMFocusManager::ActivateNewInvokedTab(SlateApp,
		FGlobalTabmanager::Get()->TryInvokeTab(FName(*ContentBrowserId)));
}

void FUMEditorCommands::OpenPreferences(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	static const FName PREFERENCES = "EditorSettings";
	FUMFocusManager::ActivateNewInvokedTab(FSlateApplication::Get(),
		FGlobalTabmanager::Get()->TryInvokeTab(FTabId(PREFERENCES)));
}

void FUMEditorCommands::OpenWidgetReflector(
	FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	static const FName REFLECTOR = "WidgetReflector";

	TSharedPtr<SDockTab> RefTab = // Invoke the Widget Reflector tab
		FGlobalTabmanager::Get()->TryInvokeTab(FTabId(REFLECTOR));
	FUMFocusManager::ActivateNewInvokedTab(FSlateApplication::Get(), RefTab); // For proper focus

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
		FUMLogger::NotifySuccess(FText::AsNumber(FoundButtons.Num()));
	}
}

void FUMEditorCommands::FocusSearchBox(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	static const FString SearchBoxType{ "SSearchBox" };
	static const FString AssetSearchBoxType{ "SAssetSearchBox" };
	static const FString FilterSearchBoxType{ "SFilterSearchBox" };
	static const FString EditableTextType{ "SEditableText" };

	const TSharedRef<FGlobalTabmanager> GTabMngr = FGlobalTabmanager::Get();
	TSharedPtr<SDockTab>				ActiveMinorTab = GTabMngr->GetActiveTab();
	TSharedPtr<SDockTab>				MajorTab =
		GTabMngr->GetMajorTabForTabManager(
			ActiveMinorTab->GetTabManagerPtr().ToSharedRef());

	TSharedPtr<SWindow> ActiveWin = SlateApp.GetActiveTopLevelRegularWindow();
	if (!ActiveWin.IsValid() || !ActiveMinorTab.IsValid())
		return;

	TSharedPtr<SWidget> SearchInWidget;
	// NOTE: This is actually returning an invalid window for some reason
	// Currently fetching the window via SlateApp...
	TSharedPtr<SWindow> MinorTabWin = ActiveMinorTab->GetParentWindow();

	FWidgetPath WidgetPath;
	if (!SlateApp.FindPathToWidget(ActiveMinorTab.ToSharedRef(), WidgetPath))
		return;

	// MinorTabWin = SlateApp.FindWidgetWindow(ActiveMinorTab.ToSharedRef(), WidgetPath);
	MinorTabWin = SlateApp.FindWidgetWindow(ActiveMinorTab.ToSharedRef());
	if (MinorTabWin.IsValid())
	{
		if (MinorTabWin.Get() == ActiveWin.Get())
			SearchInWidget = ActiveMinorTab->GetContent();
		else
			SearchInWidget = ActiveWin->GetContent();
	}
	else
	{
		// Logger.Print("Minor tab window is not valid");
		return;
	}

	// Logger.Print(FString::Printf(TEXT("Minor Tab: %s"),
	// 	*ActiveMinorTab->GetTabLabel().ToString()));

	if (!SearchInWidget.IsValid())
		return;

	TWeakPtr<SWidget> SearchBox = nullptr;
	if (!FUMSlateHelpers::TraverseWidgetTree(
			// SearchInWidget, SearchBox, SearchBoxType))
			SearchInWidget, SearchBox, EditableTextType))
		return;

	// Logger.Print(FString::Printf(TEXT("Found Type: %s"),
	// 	*SearchBox.Pin()->GetTypeAsString()));
	SlateApp.SetAllUserFocus(SearchBox.Pin(), EFocusCause::Navigation);
}

void FUMEditorCommands::DeleteItem(
	FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	// TODO: Delegate to switch Vim mode to Normal

	FUMInputPreProcessor::OnRequestVimModeChange.Broadcast(SlateApp, EVimMode::Normal);
	FKeyEvent DeleteEvent(
		FKey(EKeys::Delete),
		FModifierKeysState(),
		0, 0, 0, 0);
	FUMInputPreProcessor::ToggleNativeInputHandling(true);
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
