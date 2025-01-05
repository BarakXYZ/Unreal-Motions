
#include "UMWindowsNavigationManager.h"
#include "Framework/Commands/UICommandInfo.h"
#include "UMHelpers.h"
#include "Interfaces/IMainFrameModule.h"
#include "Framework/Application/SlateApplication.h"

DEFINE_LOG_CATEGORY_STATIC(LogUMWindowsNavigation, NoLogging, All); // Prod
// DEFINE_LOG_CATEGORY_STATIC(LogUMTWindowsNavigation, Log, All); // Dev

#define LOCTEXT_NAMESPACE "UMWindowsNavigationManager"

TSharedPtr<FUMWindowsNavigationManager> FUMWindowsNavigationManager::WindowsNavigationManager = MakeShared<FUMWindowsNavigationManager>();

FUMWindowsNavigationManager::FUMWindowsNavigationManager()
{
	FInputBindingManager&		InputBindingManager = FInputBindingManager::Get();
	IMainFrameModule&			MainFrameModule = IMainFrameModule::Get();
	TSharedRef<FUICommandList>& CommandList =
		MainFrameModule.GetMainFrameCommandBindings();
	TSharedPtr<FBindingContext> MainFrameContext =
		InputBindingManager.GetContextByName(MainFrameContextName);

	RegisterCycleWindowsNavigation(MainFrameContext);
	MapCycleWindowsNavigation(CommandList);

	OnUserMovedToNewWindow = FUnrealMotionsModule::GetOnUserMovedToNewWindow();

	FCoreDelegates::OnPostEngineInit.AddRaw(
		this, &FUMWindowsNavigationManager::RegisterSlateEvents);
}

FUMWindowsNavigationManager::~FUMWindowsNavigationManager()
{
}

FUMWindowsNavigationManager& FUMWindowsNavigationManager::Get()
{
	if (!FUMWindowsNavigationManager::WindowsNavigationManager.IsValid())
	{
		FUMWindowsNavigationManager::WindowsNavigationManager = MakeShared<FUMWindowsNavigationManager>();
	}
	return *FUMWindowsNavigationManager::WindowsNavigationManager;
}

bool FUMWindowsNavigationManager::IsInitialized()
{
	return FUMWindowsNavigationManager::WindowsNavigationManager.IsValid();
}

void FUMWindowsNavigationManager::MapCycleWindowsNavigation(
	const TSharedRef<FUICommandList>& CommandList)
{
	CommandList->MapAction(CmdInfoCycleNextWindow,
		FExecuteAction::CreateLambda(
			[this]() { CycleNoneRootWindows(true); }),
		EUIActionRepeatMode::RepeatEnabled);

	CommandList->MapAction(CmdInfoCyclePrevWindow,
		FExecuteAction::CreateLambda(
			[this]() { CycleNoneRootWindows(false); }),
		EUIActionRepeatMode::RepeatEnabled);

	CommandList->MapAction(CmdInfoGotoRootWindow,
		FExecuteAction::CreateLambda(
			[this]() { ToggleRootWindow(); }));
}

void FUMWindowsNavigationManager::RegisterSlateEvents()
{
	FSlateApplication& SlateApp = FSlateApplication::Get();

	// Useful because sometimes we will jump between windows without the
	// TabManager Delegates being called. So this is an additional way to
	// keep track of the currently active tabs.
	SlateApp.OnFocusChanging()
		.AddRaw(this, &FUMWindowsNavigationManager::OnFocusChanged);

	// Maybe useful?
	SlateApp.OnWindowBeingDestroyed().AddLambda([](const SWindow& Window) {
		// Need to make some checks to filter out non-useful windows
		// like notification windows (which will cause this to loop for ever *~*)
		if (Window.IsRegularWindow() && !Window.GetTitle().IsEmpty())
			FUMHelpers::NotifySuccess(FText::FromString(
				FString::Printf(TEXT("Window Being Destroyed: %s"),
					*Window.GetTitle().ToString())));
	});
}

void FUMWindowsNavigationManager::OnFocusChanged(
	const FFocusEvent& FocusEvent, const FWeakWidgetPath& OldWidgetPath,
	const TSharedPtr<SWidget>& OldWidget, const FWidgetPath& NewWidgetPath,
	const TSharedPtr<SWidget>& NewWidget)
{
	HasUserMovedToNewWindow();
}

// NOTE: Thinking about this, there is a state where we can't fully know if we've
// moved to a new window because the user may drag the window but then regret and
// leave it in the same place. In that case we didn't really moved to a new win.
// It looks like when dragging windows around, they will be immediately marked as
// invalid (their ID & Pointer) so maybe we should use their name as a fallback or
// something?
// So yeah, not completely sure if windows should deduce Major Tabs?
// If we're moving to a new window by dragging our current tab to a new window,
// we also want it to be focused. So we can't really deduce the next active major
// tab in that scenario. So maybe we can check if the number of tabs has changed?
// Or, something with the drag events? Anyway Major Tabs should be in front of
// everything so not really sure what does it mean to focus on them?
// We can probably also just iterate over the tabwell of our currently focused
// major tab that can be fetched using FGlobalTabmanager?
// NOTE: Maybe we want to throw some delay on this operation in general too
// Bottom line -> not sure how important is to track if the user has actually
// moved to a new window since tabs are the real issue and they're being tracked
// pretty well. Need to see.
bool FUMWindowsNavigationManager::HasUserMovedToNewWindow(bool bSetNewWindow)
{
	const TSharedPtr<SWindow>& Win =
		FSlateApplication::Get().GetActiveTopLevelRegularWindow();
	if (!Win.IsValid())
		return false;

	if (!LastActiveWin.IsValid())
	{
		// FUMHelpers::NotifySuccess(FText::FromString(
		// 	FString::Printf(TEXT("LastActiveWin Invalid: Store New Window Ptr: %s"),
		// 		*Win->GetTitle().ToString())));
		LastActiveWin = Win;
	}
	if (LastActiveWin.Pin() != Win)
	{
		// FUMHelpers::NotifySuccess(FText::FromString(
		// 	FString::Printf(TEXT("LastActiveWin != Win | Actual New Window: %s"),
		// 		*Win->GetTitle().ToString())));
	}
	else if (!Win->GetTitle().EqualTo(LastActiveWinTitle))
	{
		// FUMHelpers::NotifySuccess(FText::FromString(
		// 	FString::Printf(TEXT("Title != LastActiveWinTitle | Actual New Window: %s"),
		// 		*Win->GetTitle().ToString())));
	}
	else
		return false;

	// const TSharedRef<FGlobalTabmanager>& GTM = FGlobalTabmanager::Get();
	// TSharedPtr<SDockTab>				 ActiveTab = GTM->GetActiveTab();
	// if (ActiveTab->GetVisualTabRole() != ETabRole::MajorTab) // ???
	// {
	// 	TSharedPtr<FTabManager> TabManager = ActiveTab->GetTabManagerPtr();
	// 	if (TabManager.IsValid())
	// 	{
	// 		TSharedPtr<SDockTab> MajorTab =
	// 			GTM->GetMajorTabForTabManager(TabManager.ToSharedRef());
	// 		if (MajorTab.IsValid())
	// 			FUMHelpers::NotifySuccess(MajorTab->GetTabLabel());
	// 	}
	// }
	LastActiveWin = Win;
	LastActiveWinTitle = Win->GetTitle();
	OnUserMovedToNewWindow.Broadcast(LastActiveWin);
	return true;
}

void FUMWindowsNavigationManager::ToggleRootWindow()
{
	const TSharedPtr<SWindow> RootWin = FGlobalTabmanager::Get()->GetRootWindow();
	if (!RootWin.IsValid())
		return;
	TArray<TSharedRef<SWindow>> ChildWins = RootWin->GetChildWindows();

	bool bHasVisibleWindows = false;

	// NOTE: It's important for both loops to start from end to retain wins order.
	for (int32 i = ChildWins.Num() - 1; i >= 0; --i)
	{ // Minimize all non-root windows (bring root window to foreground)
		const TSharedRef<SWindow>& Win = ChildWins[i];
		if (Win->IsRegularWindow() && !Win->IsWindowMinimized())
		{
			Win->Minimize();
			bHasVisibleWindows = true;
		}
	}

	if (bHasVisibleWindows)
	{
		ActivateWindow(RootWin.ToSharedRef());
		return;
	}

	// If no visible windows were found; we're assuming the user intention is
	// to bring all non-root windows to the foreground (restoring them).
	int32 LastFoundChildWinIndex{ INDEX_NONE };

	for (int32 i{ ChildWins.Num() - 1 }; i >= 0; --i)
	{
		if (ChildWins[i]->IsRegularWindow()) // Wins minimized was already checked
		{
			ChildWins[i]->Restore();
			// Last active win will be the last found regular win (retain order)
			LastFoundChildWinIndex = i;
		}
	}
	if (LastFoundChildWinIndex != INDEX_NONE)
		ActivateWindow(ChildWins[LastFoundChildWinIndex]);
}

void FUMWindowsNavigationManager::CycleNoneRootWindows(bool bIsNextWindow)
{
	const TSharedPtr<SWindow> RootWin = FGlobalTabmanager::Get()->GetRootWindow();
	if (!RootWin.IsValid() || !RootWin->HasActiveChildren())
		return;
	TArray<TSharedRef<SWindow>> ChildWins = RootWin->GetChildWindows();

	int32		   Cursor{ 0 };
	TArray<uint64> VisWinIndexes;

	for (const TSharedRef<SWindow>& Win : ChildWins)
	{
		if (Win->IsRegularWindow() && !Win->GetTitle().IsEmpty()
			&& !Win->IsActive() && !Win->IsWindowMinimized())
		{
			if (bIsNextWindow)
			{
				ActivateWindow(Win);
				return; // We can return here, moving to next window is trivial
			}
			VisWinIndexes.Add(Cursor);
		}
		++Cursor;
	}
	if (!VisWinIndexes.IsEmpty())
	{
		// For moving backwards, we will actually need to put the currently active
		// window in the back. And the only way I found to do so is to bring all
		// the none-active windows to the front and then to activate our targeted
		// window - which will be the last none-active window found in the
		// ChildWins stack
		for (const int32 WinIndex : VisWinIndexes)
			ChildWins[WinIndex]->BringToFront();

		ActivateWindow(ChildWins[VisWinIndexes[VisWinIndexes.Num() - 1]]);
	}
}

// NOTE: Kept here some commented methods for future reference.
void FUMWindowsNavigationManager::ActivateWindow(const TSharedRef<SWindow> Window)
{
	FSlateApplication& SlateApp = FSlateApplication::Get();
	Window->BringToFront(true);
	TSharedRef<SWidget> WinContent = Window->GetContent();
	// FWindowDrawAttentionParameters DrawParams(
	// 	EWindowDrawAttentionRequestType::UntilActivated);
	// Window->DrawAttention(DrawParams);
	// Window->ShowWindow();
	// Window->FlashWindow(); // Amazing way to visually indicate activated wins!

	SlateApp.ClearAllUserFocus(); // This is important to actually draw focus
	SlateApp.SetAllUserFocus(
		WinContent, EFocusCause::Navigation);
	// SlateApp.SetKeyboardFocus(WinContent);
	// FWidgetPath WidgetPath;
	// SlateApp.FindPathToWidget(WinContent, WidgetPath);
	// SlateApp.SetAllUserFocusAllowingDescendantFocus(
	// 	WidgetPath, EFocusCause::Navigation);

	// FUMHelpers::AddDebugMessage(FString::Printf(
	// 	TEXT("Activating Window: %s"), *Window->GetTitle().ToString()));
}

bool FUMWindowsNavigationManager::FocusNextFrontmostWindow()
{
	FSlateApplication&			SlateApp = FSlateApplication::Get();
	TArray<TSharedRef<SWindow>> Wins;

	SlateApp.GetAllVisibleWindowsOrdered(Wins);
	for (int32 i{ Wins.Num() - 1 }; i > 0; --i)
	{
		if (!Wins[i]->GetTitle().IsEmpty())
		{
			ActivateWindow(Wins[i]);
			return true; // Found the next window
		}
	}

	// If no windows we're found, try to bring focus to the root window
	if (TSharedPtr<SWindow> RootWin = FGlobalTabmanager::Get()->GetRootWindow())
	{
		ActivateWindow(RootWin.ToSharedRef());
		return true;
	}
	return false;
}

void FUMWindowsNavigationManager::CleanupInvalidWindows(
	TArray<uint64> WinIdsToCleanup)
{
	if (WinIdsToCleanup.Num() == 0)
		return;
	for (const auto& Key : WinIdsToCleanup)
		TrackedWindows.Remove(Key);
	FString Log = FString::Printf(
		TEXT("Cleaned: %d Invalid Windows"), WinIdsToCleanup.Num());
	FUMHelpers::NotifySuccess(FText::FromString(Log), VisualLog);
};

const TMap<uint64, TWeakPtr<SWindow>>& FUMWindowsNavigationManager::GetTrackedWindows()
{
	return TrackedWindows;
}

void FUMWindowsNavigationManager::RegisterCycleWindowsNavigation(const TSharedPtr<FBindingContext>& MainFrameContext)
{
	UI_COMMAND_EXT(MainFrameContext.Get(), CmdInfoCycleNextWindow,
		"CycleNoneRootWindowNext", "Cycle None-Root Window Next",
		"Tries to cycle to the next none-root window.",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Control, EKeys::Period));

	UI_COMMAND_EXT(MainFrameContext.Get(), CmdInfoCyclePrevWindow,
		"CycleNoneRootWindowPrevious", "Cycle None-Root Window Previous",
		"Tries to cycle to the previous none-root window.",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Control, EKeys::Comma));

	UI_COMMAND_EXT(MainFrameContext.Get(), CmdInfoGotoRootWindow,
		"ToggleRootWindowForeground", "Toggle Root-Window Foreground",
		"Toggle between root window and other windows. Minimizes non-root windows to focus root, or vice versa.",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Control, EKeys::Slash));
}

#undef LOCTEXT_NAMESPACE
