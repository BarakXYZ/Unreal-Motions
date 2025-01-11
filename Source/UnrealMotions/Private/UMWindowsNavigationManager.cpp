
#include "UMWindowsNavigationManager.h"
#include "Framework/Docking/TabManager.h"
#include "Interfaces/IMainFrameModule.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/InputBindingManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogUMWindowsNavigation, NoLogging, All); // Prod
// DEFINE_LOG_CATEGORY_STATIC(LogUMTWindowsNavigation, Log, All); // Dev
FUMLogger FUMWindowsNavigationManager::Logger(&LogUMWindowsNavigation);

#define LOCTEXT_NAMESPACE "UMWindowsNavigationManager"

TSharedPtr<FUMWindowsNavigationManager> FUMWindowsNavigationManager::WindowsNavigationManager = MakeShared<FUMWindowsNavigationManager>();

FUMOnWindowChanged FUMWindowsNavigationManager::OnWindowChanged;

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

	// Maybe useful?
	SlateApp.OnWindowBeingDestroyed().AddLambda([](const SWindow& Window) {
		// Need to make some checks to filter out non-useful windows
		// like notification windows (which will cause this to loop for ever *~*)
		if (Window.IsRegularWindow() && !Window.GetTitle().IsEmpty())
			Logger.Print(FString::Printf(TEXT("Window Being Destroyed: %s"),
				*Window.GetTitle().ToString()));
	});
}

void FUMWindowsNavigationManager::ToggleRootWindow()
{
	const TSharedPtr<SWindow> RootWin = FGlobalTabmanager::Get()->GetRootWindow();
	if (!RootWin.IsValid())
		return;
	TArray<TSharedRef<SWindow>> ChildWins = RootWin->GetChildWindows();

	TSharedPtr<SWindow> ActiveWindow = // Track previous window for the delegate
		FSlateApplication::Get().GetActiveTopLevelRegularWindow();

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
		OnWindowChanged.Broadcast(ActiveWindow, RootWin);
		return;
	}

	// If no visible windows were found; we're assuming the user intention is
	// to bring all non-root windows to the foreground (restoring them).
	int32 LastFoundChildWinIndex{ INDEX_NONE };

	for (int32 i{ ChildWins.Num() - 1 }; i >= 0; --i)
	{
		if (ChildWins[i]->IsRegularWindow()) // Wins minimized were checked
		{
			ChildWins[i]->Restore();
			// Last active win will be the last found regular win (retain order)
			LastFoundChildWinIndex = i;
		}
	}
	if (LastFoundChildWinIndex != INDEX_NONE)
	{
		OnWindowChanged.Broadcast(ActiveWindow, ChildWins[LastFoundChildWinIndex]);
	}
}

void FUMWindowsNavigationManager::CycleNoneRootWindows(bool bIsNextWindow)
{
	const TSharedPtr<SWindow> RootWin = FGlobalTabmanager::Get()->GetRootWindow();
	if (!RootWin.IsValid() || !RootWin->HasActiveChildren())
		return;
	TArray<TSharedRef<SWindow>> ChildWins = RootWin->GetChildWindows();

	TSharedPtr<SWindow> ActiveWindow = // Track previous window for the delegate
		FSlateApplication::Get().GetActiveTopLevelRegularWindow();

	int32		   Cursor{ 0 };
	TArray<uint64> VisWinIndexes;

	for (const TSharedRef<SWindow>& Win : ChildWins)
	{
		if (Win->IsRegularWindow() && !Win->GetTitle().IsEmpty()
			&& !Win->IsActive() && !Win->IsWindowMinimized())
		{
			if (bIsNextWindow)
			{
				OnWindowChanged.Broadcast(ActiveWindow, Win);
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

		OnWindowChanged.Broadcast(
			ActiveWindow, ChildWins[VisWinIndexes[VisWinIndexes.Num() - 1]]);
	}
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
