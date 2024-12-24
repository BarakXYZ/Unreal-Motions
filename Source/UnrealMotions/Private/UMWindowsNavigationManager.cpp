
#include "UMWindowsNavigationManager.h"
#include "Framework/Commands/UICommandInfo.h"
#include "UMHelpers.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/IMainFrameModule.h"

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

	// Might be useful later
	// SlateApp.OnWindowBeingDestroyed().AddRaw(this, &FUMTabsNavigationManager::DebugWindow);
}

void FUMWindowsNavigationManager::OnFocusChanged(
	const FFocusEvent& FocusEvent, const FWeakWidgetPath& OldWidgetPath,
	const TSharedPtr<SWidget>& OldWidget, const FWidgetPath& NewWidgetPath,
	const TSharedPtr<SWidget>& NewWidget)
{
	HasUserMovedToNewWindow();
}

bool FUMWindowsNavigationManager::HasUserMovedToNewWindow(bool bSetNewWindow)
{
	if (const TSharedPtr<SWindow>& Win =
			FSlateApplication::Get().GetActiveTopLevelRegularWindow())
	{
		uint64 WinId = Win->GetId();
		// Check if this window already exist in our tracked wins. Add if not.
		if (!TrackedWindows.Find(WinId))
		{
			TrackedWindows.Add(WinId, Win);
			FString WindowInfo = FString::Printf(TEXT("New Window Added!\nTitle: %s\nID: %d\nType: %d"),
				*Win->GetTitle().ToString(),
				WinId,
				Win->GetType());
			FUMHelpers::NotifySuccess(FText::FromString(WindowInfo), VisualLog);
		}

		// If our current window is invalid or doesn't match the currently
		// active window -> set the new active window as current.
		if (!CurrWin.IsValid() || Win->GetId() != CurrWin.Pin()->GetId())
		{
			if (bSetNewWindow)
				CurrWin = Win;
			FUMHelpers::NotifySuccess(FText::FromString(TEXT("New Window!")), VisualLog);
			OnUserMovedToNewWindow.Broadcast(CurrWin);
			return true;
		}
	}
	return false;
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
		// if (Win->IsRegularWindow() && !Win->IsWindowMinimized()
		// 	&& !Win->GetTitle().IsEmpty())
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
	// to bring all non-root windows to the foreground, thus restoring them.
	int32 LastFoundChildWinIndex{ INDEX_NONE };

	for (int32 i{ ChildWins.Num() - 1 }; i >= 0; --i)
	{
		if (ChildWins[i]->IsRegularWindow()) // All wins are minimized (checked)
		{
			ChildWins[i]->Restore();
			// Last active win will be the last found regular win (retain order)
			LastFoundChildWinIndex = i;
		}
	}
	if (LastFoundChildWinIndex != INDEX_NONE)
		ActivateWindow(ChildWins[LastFoundChildWinIndex]);
}

// NOTE: Kept here some commented methods for future reference.
void FUMWindowsNavigationManager::ActivateWindow(const TSharedRef<SWindow> Window)
{
	FSlateApplication& App = FSlateApplication::Get();
	Window->BringToFront(true);
	TSharedRef<SWidget> WinContent = Window->GetContent();
	// FWindowDrawAttentionParameters DrawParams(
	// 	EWindowDrawAttentionRequestType::UntilActivated);
	// Window->DrawAttention(DrawParams);
	// Window->ShowWindow();
	// Window->FlashWindow(); // Amazing way to visually indicate activated wins!

	App.ClearAllUserFocus(); // This is really important to actually draw focus
	App.SetAllUserFocus(
		WinContent, EFocusCause::SetDirectly);
	App.SetKeyboardFocus(WinContent);
	FWidgetPath WidgetPath;
	App.FindPathToWidget(WinContent, WidgetPath);
	App.SetAllUserFocusAllowingDescendantFocus(
		WidgetPath, EFocusCause::Mouse);

	// FUMHelpers::AddDebugMessage(FString::Printf(
	// 	TEXT("Activating Window: %s"), *Window->GetTitle().ToString()));
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
				return;
			}
			VisWinIndexes.Add(Cursor);
		}
		++Cursor;
	}
	if (!VisWinIndexes.IsEmpty())
	{
		for (const int32 WinIndex : VisWinIndexes)
		{
			ChildWins[WinIndex]->BringToFront();
		}
		ActivateWindow(ChildWins[VisWinIndexes[VisWinIndexes.Num() - 1]]);
	}
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
