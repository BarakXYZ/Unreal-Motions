
#include "UMWindowNavigatorEditorSubsystem.h"
#include "Framework/Docking/TabManager.h"
#include "Interfaces/IMainFrameModule.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/InputBindingManager.h"
#include "UMConfig.h"

DEFINE_LOG_CATEGORY_STATIC(LogUMWindowNavigatorEditorSubsystem, NoLogging, All); // Prod
// DEFINE_LOG_CATEGORY_STATIC(LogUMWindowNavigatorEditorSubsystem, Log, All); // Dev

#define LOCTEXT_NAMESPACE "UMWindowNavigatorEditorSubsystem"

FUMOnWindowChanged UUMWindowNavigatorEditorSubsystem::OnWindowChanged;

bool UUMWindowNavigatorEditorSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return FUMConfig::Get()->IsWindowNavigatorEnabled();
}

void UUMWindowNavigatorEditorSubsystem::Initialize(
	FSubsystemCollectionBase& Collection)
{
	Logger = FUMLogger(&LogUMWindowNavigatorEditorSubsystem);

	FInputBindingManager&		InputBindingManager = FInputBindingManager::Get();
	IMainFrameModule&			MainFrameModule = IMainFrameModule::Get();
	TSharedRef<FUICommandList>& CommandList =
		MainFrameModule.GetMainFrameCommandBindings();
	TSharedPtr<FBindingContext> MainFrameContext =
		InputBindingManager.GetContextByName(MainFrameContextName);

	RegisterCycleWindowsNavigation(MainFrameContext);
	MapCycleWindowsNavigation(CommandList);

	Super::Initialize(Collection);
}

void UUMWindowNavigatorEditorSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UUMWindowNavigatorEditorSubsystem::MapCycleWindowsNavigation(
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

void UUMWindowNavigatorEditorSubsystem::ToggleRootWindow()
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

void UUMWindowNavigatorEditorSubsystem::CycleNoneRootWindows(bool bIsNextWindow)
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

void UUMWindowNavigatorEditorSubsystem::RegisterCycleWindowsNavigation(const TSharedPtr<FBindingContext>& MainFrameContext)
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
