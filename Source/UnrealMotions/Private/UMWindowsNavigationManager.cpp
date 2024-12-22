
#include "UMWindowsNavigationManager.h"
#include "Framework/Commands/UICommandInfo.h"
#include "GenericPlatform/GenericWindow.h"
#include "Templates/SharedPointer.h"
#include "Widgets/Docking/SDockTab.h"
#include "UMHelpers.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/IMainFrameModule.h"

// DEFINE_LOG_CATEGORY_STATIC(LogUMWindowsNavigation, NoLogging, All); // Prod
DEFINE_LOG_CATEGORY_STATIC(LogUMTWindowsNavigation, Log, All); // Dev

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
	/** We can know the name of the context by looking at the constructor
	 * of the TCommands that its extending. e.g. SystemWideCommands, MainFrame...
	 * */

	// Register Cycle Window Navigation
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

void FUMWindowsNavigationManager::MapCycleWindowsNavigation(
	const TSharedRef<FUICommandList>& CommandList)
{
	CommandList->MapAction(CmdInfoCycleNextWindow,
		FExecuteAction::CreateLambda(
			[this]() { CycleWindows(true); }),
		EUIActionRepeatMode::RepeatEnabled);

	CommandList->MapAction(CmdInfoCyclePrevWindow,
		FExecuteAction::CreateLambda(
			[this]() { CycleWindows(false); }),
		EUIActionRepeatMode::RepeatEnabled);
}

// TODO: Save the states of the windows (i.e. maximized, position, etc.)
// to restore them properly to the expected positions.
// Also, we want to remember if the window was already minimized, to not
// restore it by default (in the case where it was minimized)
void FUMWindowsNavigationManager::ToggleNonRootWindowsState(bool bToggleMinimized)
{
	uint64 RootWinId = FGlobalTabmanager::Get()->GetRootWindow()->GetId();

	TArray<uint64> WinIdsToCleanup;
	for (const auto& Win : TrackedWindows)
	{
		if (!Win.Value.IsValid())
		{
			WinIdsToCleanup.Add(Win.Key);
			continue;
		}
		const TSharedPtr<SWindow>& PinnedWin = Win.Value.Pin();
		if (PinnedWin->GetId() != RootWinId)
		{
			if (bToggleMinimized)
				PinnedWin->Minimize();
			else
				PinnedWin->Restore();
		}
	}
	CleanupInvalidWindows(WinIdsToCleanup);
}

// Maybe it will be even more useful if we cycle through the windows
// by their visibility rather than the time they we're first created honestly.
// Harpoon like artchitecture will be more useful for moving between windows
// in determined order.
void FUMWindowsNavigationManager::CycleWindows(bool bIsNextWindow)
{
	const uint64 RootWinId = FGlobalTabmanager::Get()->GetRootWindow()->GetId();
	auto		 ActiveWin = FSlateApplication::Get().GetActiveTopLevelRegularWindow();

	if (!ActiveWin.IsValid())
	{
		FUMHelpers::NotifySuccess(
			FText::FromString("Current Window is NOT valid."));
		return;
	}

	uint64		   ActiveWinId = ActiveWin->GetId();
	TArray<uint64> CleanupWindowIds;

	auto SetActiveWindow = [this, RootWinId](
							   const TTuple<uint64, TWeakPtr<SWindow>>& Win) {
		ToggleNonRootWindowsState(Win.Value.Pin()->GetId() == RootWinId);
		Win.Value.Pin()->BringToFront();
	};

	if (!TrackedWindows.Find(ActiveWinId))
		TrackedWindows.Add(ActiveWinId, ActiveWin);

	bool bFoundActive = false;
	for (auto It = TrackedWindows.CreateConstIterator(); It; ++It)
	{
		if (!It->Value.IsValid())
			CleanupWindowIds.Add(It->Key);
		else if (bFoundActive)
		{
			SetActiveWindow(*It);
			bFoundActive = false;
		}
		else if (It->Key == ActiveWinId)
			bFoundActive = true;
	}

	CleanupInvalidWindows(CleanupWindowIds);
	if (bFoundActive)
		SetActiveWindow(*TrackedWindows.CreateConstIterator());
}

void FUMWindowsNavigationManager::CleanupInvalidWindows(
	TArray<uint64> CleanupWindowsIds)
{
	for (const auto& Key : CleanupWindowsIds)
		TrackedWindows.Remove(Key);
	FString Log = FString::Printf(
		TEXT("Cleaned: %d Invalid Windows"), CleanupWindowsIds.Num());
	FUMHelpers::NotifySuccess(FText::FromString(Log));
};

const TMap<uint64, TWeakPtr<SWindow>>& FUMWindowsNavigationManager::GetTrackedWindows()
{
	return TrackedWindows;
}

void FUMWindowsNavigationManager::RegisterCycleWindowsNavigation(const TSharedPtr<FBindingContext>& MainFrameContext)
{
	UI_COMMAND_EXT(MainFrameContext.Get(), CmdInfoCycleNextWindow,
		"CycleWindowNext", "Focus the next window",
		"Moves to the next window",
		EUserInterfaceActionType::Button,
		FInputChord(
			EModifierKey::FromBools(true, false, true, false), EKeys::Period));

	UI_COMMAND_EXT(MainFrameContext.Get(), CmdInfoCyclePrevWindow,
		"CycleWindowPrevious", "Focus the previous window",
		"Moves to the previous window",
		EUserInterfaceActionType::Button,
		FInputChord(
			EModifierKey::FromBools(true, false, true, false), EKeys::Comma));
}
