#include "UMTabNavigatorEditorSubsystem.h"

#include "Interfaces/IMainFrameModule.h"
#include "Framework/Commands/UICommandInfo.h"
#include "Widgets/Docking/SDockTab.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"

#include "UMSlateHelpers.h"
#include "UMConfig.h"
#include "VimInputProcessor.h"
#include "UMInputHelpers.h"

// DEFINE_LOG_CATEGORY_STATIC(LogUMTabNavigatorEditorSubsystem, NoLogging, All); // Prod
DEFINE_LOG_CATEGORY_STATIC(LogUMTabNavigatorEditorSubsystem, Log, All); // Dev

#define LOCTEXT_NAMESPACE "UMTabNavigatorEditorSubsystem"

bool UUMTabNavigatorEditorSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	TSharedRef<FUMConfig> Config = FUMConfig::Get();
	// if Vim is enabled, TabNavigator must also be enabled.
	return Config->IsTabNavigatorEnabled() || Config->IsVimEnabled();
}

void UUMTabNavigatorEditorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Logger = FUMLogger(&LogUMTabNavigatorEditorSubsystem);

	FInputBindingManager&		InputBindingManager = FInputBindingManager::Get();
	IMainFrameModule&			MainFrameModule = IMainFrameModule::Get();
	TSharedRef<FUICommandList>& CommandList =
		MainFrameModule.GetMainFrameCommandBindings();
	TSharedPtr<FBindingContext> MainFrameContext =
		InputBindingManager.GetContextByName(MainFrameContextName);
	/** We can know the name of the context by looking at the constructor
	 * of the TCommands that its extending. e.g. SystemWideCommands, MainFrame...
	 * */

	// Register Minor Tabs Navigation
	InitTabNavInputChords(true, true, true, false);
	AddMinorTabsNavigationCommandsToList(MainFrameContext);
	MapTabCommands(CommandList, CommandInfoMinorTabs, false);

	// Register Major Tabs Navigation
	InitTabNavInputChords();
	AddMajorTabsNavigationCommandsToList(MainFrameContext); // Macros
	MapTabCommands(CommandList, CommandInfoMajorTabs, true);

	// Register Next & Previous Tab Navigation
	RegisterCycleTabNavigation(MainFrameContext);
	MapCycleTabsNavigation(CommandList);

	if (FUMConfig::Get()->IsVimEnabled())
		BindVimCommands();

	Super::Initialize(Collection);
}

void UUMTabNavigatorEditorSubsystem::Deinitialize()
{
	TabChords.Empty();
	CommandInfoMajorTabs.Empty();
	CommandInfoMinorTabs.Empty();
	Super::Deinitialize();
}

void UUMTabNavigatorEditorSubsystem::InitTabNavInputChords(
	bool bCtrl, bool bAlt, bool bShift, bool bCmd)

{
	if (!TabChords.IsEmpty())
		TabChords.Empty();
	TabChords.Reserve(10);

	const FKey NumberKeys[] = {
		EKeys::Zero, EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four,
		EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine
	};

	// Setting by default to Control + Shift + 0-9
	const EModifierKey::Type ModifierKeys = EModifierKey::FromBools(
		bCtrl, bAlt, bShift, bCmd);

	for (const FKey& Key : NumberKeys)
		TabChords.Add(FInputChord(ModifierKeys, Key));
}

void UUMTabNavigatorEditorSubsystem::RemoveDefaultCommand(
	FInputBindingManager& InputBindingManager, FInputChord Command)
{
	const TSharedPtr<FUICommandInfo> CheckCommand =
		InputBindingManager.GetCommandInfoFromInputChord(MainFrameContextName, Command,
			false /* We want to check the current, not default command */);
	if (CheckCommand.IsValid())
		CheckCommand->RemoveActiveChord(EMultipleKeyBindingIndex::Primary);
}

void UUMTabNavigatorEditorSubsystem::MapTabCommands(
	const TSharedRef<FUICommandList>&		  CommandList,
	const TArray<TSharedPtr<FUICommandInfo>>& CommandInfo, bool bIsMajorTab)
{
	for (int i{ 0 }; i < TabChords.Num(); ++i)
	{
		CommandList->MapAction(CommandInfo[i],
			FExecuteAction::CreateLambda(
				[this, i, bIsMajorTab]() { MoveToTabIndex(i, bIsMajorTab); }));
	}
}

void UUMTabNavigatorEditorSubsystem::MapCycleTabsNavigation(
	const TSharedRef<FUICommandList>& CommandList)
{
	CommandList->MapAction(CmdInfoNextMajorTab,
		FExecuteAction::CreateLambda(
			[this]() { CycleTabs(true, true); }),
		EUIActionRepeatMode::RepeatEnabled);

	CommandList->MapAction(CmdInfoPrevMajorTab,
		FExecuteAction::CreateLambda(
			[this]() { CycleTabs(true, false); }),
		EUIActionRepeatMode::RepeatEnabled);

	CommandList->MapAction(CmdInfoNextMinorTab,
		FExecuteAction::CreateLambda(
			[this]() { CycleTabs(false, true); }),
		EUIActionRepeatMode::RepeatEnabled);

	CommandList->MapAction(CmdInfoPrevMinorTab,
		FExecuteAction::CreateLambda(
			[this]() { CycleTabs(false, false); }),
		EUIActionRepeatMode::RepeatEnabled);
}

bool UUMTabNavigatorEditorSubsystem::TryGetValidTargetTab(
	TSharedPtr<SDockTab>& OutTab, bool bIsMajorTab)
{
	if (bIsMajorTab)
	{
		if (const TSharedPtr<SDockTab> ActiveMajorTab =
				FUMSlateHelpers::GetActiveMajorTab())
		{
			OutTab = ActiveMajorTab;
			return true;
		}
	}
	else
	{
		if (const TSharedPtr<SDockTab> ActiveMinorTab =
				FUMSlateHelpers::GetActiveMinorTab())
		{
			OutTab = ActiveMinorTab;
			return true;
		}
	}
	return false;
}

void UUMTabNavigatorEditorSubsystem::MoveToTabIndex(
	int32 TabIndex, bool bIsMajorTab)
{
	TSharedPtr<SDockTab> TargetTab;
	if (!TryGetValidTargetTab(TargetTab, bIsMajorTab))
		return;

	TSharedPtr<SWidget> TabWell = TargetTab->GetParentWidget();
	if (!TabWell.IsValid() || !TabWell->GetType().IsEqual("SDockingTabWell"))
		return;

	FocusTabIndex(TabWell, TabIndex, bIsMajorTab);
	return;
}

void UUMTabNavigatorEditorSubsystem::FocusTabIndex(
	const TSharedPtr<SWidget>& DockingTabWell, int32 TabIndex, bool bIsMajorTab)
{
	FChildren* Tabs = DockingTabWell->GetChildren();
	/* Check if valid and if we have more are the same number of tabs as index */
	if (Tabs && Tabs->Num() >= TabIndex)
	{
		if (TabIndex == 0)
			TabIndex = Tabs->Num(); // Get last child

		const TSharedPtr<SDockTab>& NewTab =
			StaticCastSharedRef<SDockTab>(Tabs->GetChildAt(TabIndex - 1));

		// NOTE:
		// This will call the ActiveTabChanged // Foregrounded in the FocusManager
		// Also, we don't need to be in worry of being out-of-sync as we're
		// working with the TabWell itself, not depending on the indivdual
		// tab index.
		if (NewTab.IsValid())
			NewTab->ActivateInParent(ETabActivationCause::SetDirectly);
	}
}

void UUMTabNavigatorEditorSubsystem::CycleTabs(bool bIsMajorTab, bool bIsNextTab)
{
	TSharedPtr<SDockTab> TargetTab;
	if (!TryGetValidTargetTab(TargetTab, bIsMajorTab))
		return;

	TSharedPtr<SWidget> TabWell = TargetTab->GetParentWidget();
	if (!TabWell.IsValid() || !TabWell->GetType().IsEqual("SDockingTabWell"))
		return;

	FChildren* Tabs = TabWell->GetChildren();
	if (!Tabs || Tabs->Num() == 0)
		return;

	const int32 TNum = Tabs->Num();
	int32		CurrIndex = INDEX_NONE;

	for (int32 i{ 0 }; i < TNum; ++i)
	{
		const auto& Tab = StaticCastSharedRef<SDockTab>(Tabs->GetChildAt(i));
		if (Tab->IsForeground()) // Check if active tab
		{
			CurrIndex = i;
			break;
		}
	}
	if (CurrIndex == INDEX_NONE)
		return;

	CurrIndex = // Calculate the index of the new tab we want to activate
		bIsNextTab ? (CurrIndex + 1) % TNum : (CurrIndex - 1 + TNum) % TNum;
	TSharedPtr<SDockTab> NewTab = StaticCastSharedRef<SDockTab>(
		Tabs->GetChildAt(CurrIndex)); // Fetch that tab

	// See FocusTabIndex note on why this isn't a problem to not set the tab here.
	if (NewTab.IsValid())
		NewTab->ActivateInParent(ETabActivationCause::SetDirectly);
}

// Vim Related Functions:

void UUMTabNavigatorEditorSubsystem::MoveActiveTabToWindow(
	FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	TSharedPtr<SWidget> TargetTabWell;

	bool	   bIsNomadWindow = false;
	const FKey InKey = InKeyEvent.GetKey();
	if (InKey == EKeys::Zero)
	{
		// Fetch the Level Editor TabWell
		const TSharedPtr<FTabManager> LevelEditorTabManager =
			FUMSlateHelpers::GetLevelEditorTabManager();
		if (!LevelEditorTabManager.IsValid())
			return;

		TargetTabWell =
			FUMSlateHelpers::GetTabWellForTabManager(LevelEditorTabManager.ToSharedRef());
		if (!TargetTabWell.IsValid())
			return;

		// NOTE:
		// Cannot be Nomad Window, as this is the root window.
	}
	else
	{
		TArray<TSharedRef<SWindow>> VisibleWindows;
		SlateApp.GetAllVisibleWindowsOrdered(VisibleWindows);

		TArray<TSharedRef<SWindow>> RegularVisWins;
		for (const TSharedRef<SWindow>& Win : VisibleWindows)
		{
			if (Win->IsRegularWindow())
				RegularVisWins.Add(Win);
		}

		if (RegularVisWins.IsEmpty())
			return;

		int32 KeyAsDigit;
		if (!FUMInputHelpers::GetDigitFromKey(InKey, KeyAsDigit)
			// Check we have valid amount of windows to answer the input postfix
			|| KeyAsDigit > RegularVisWins.Num() - 1)
			return;

		// Check if our active window is the root window, if so, we will
		// need to fetch the TargetWindow in regular order as the root window is
		// always placed at the backmost position (0), else we're fetching in
		// reverse order.
		const TSharedPtr<SWindow> RootWindow =
			FGlobalTabmanager::Get()->GetRootWindow();

		TSharedRef<SWindow> TargetWindow =
			(RootWindow.IsValid() && RootWindow->IsActive())
			? RegularVisWins[KeyAsDigit]
			: RegularVisWins[(RegularVisWins.Num() - 1) - KeyAsDigit];

		// Check if the targeted window is a Nomad Tabs window.
		// if so, we will need to target the docking in a specific way (see below)
		bIsNomadWindow = FUMSlateHelpers::IsNomadWindow(TargetWindow);

		// Get the first TabWell in the Target Window (Major Tab's TabWell)
		TSharedPtr<SWidget> FoundTargetTabWell;
		if (!FUMSlateHelpers::TraverseFindWidget(
				TargetWindow, FoundTargetTabWell, "SDockingTabWell"))
			return;

		if (FoundTargetTabWell.IsValid())
			TargetTabWell = FoundTargetTabWell;
		else
			return;
	}
	const TSharedPtr<SDockTab> LastTabInWell =
		FUMSlateHelpers::GetLastTabInTabWell(TargetTabWell.ToSharedRef());
	if (!LastTabInWell.IsValid())
		return;

	// The drag motion we want to perform now, in order to cover a all edge cases
	// where the dragging will append before the last tab (or not be able to dock
	// at all) is to drag to the center of the last tab, wait a tiny bit, then
	// drag to the end of the TabWell.
	// The reason we can't directly drag to the end of the TabWell is because of
	// focus and heirarchy of layers, it won't detect our dragging if we won't
	// firstly drag to the center of the last tab.

	// First, we're dragging to the targeted last tab center position.
	const FVector2f DragToPos_LastTabCenter =
		FUMSlateHelpers::GetWidgetCenterRightScreenSpacePosition(
			LastTabInWell.ToSharedRef());

	// Finally we will drag the tab to the end of the TabWell to append it.

	// If the targeted window contains only Nomad Tabs; the TitleBar will be sort
	// of collapsed (shrinked). Thus we will need to offset by the size
	// of a tab.x to dock properly.
	// Else we can slightly offset to the left from the end of the TabWell itself.
	// This is true if the window contains any real Major Tabs (type)
	const FVector2f OffsetTabWellTargetPosBy =
		bIsNomadWindow
		? FVector2f(-LastTabInWell->GetCachedGeometry().GetAbsoluteSize().X, 0.0f)
		: FVector2f(-5.0f, 0.0f);

	const FVector2f DragToPos_TargetTabWellCenterRight =
		FUMSlateHelpers::GetWidgetCenterRightScreenSpacePosition(
			TargetTabWell.ToSharedRef(), OffsetTabWellTargetPosBy);

	// This is the array of positions we will pass on.
	// Firstly we move to the last tab center, then right edge of TabWell.
	TArray<FVector2f> TargetPositions({ DragToPos_LastTabCenter,
		DragToPos_TargetTabWellCenterRight });

	// Store the origin position so we can restore it at the end
	const FVector2f MouseOriginPos = SlateApp.GetCursorPos();

	const float MoveOffsetDelay = 0.050f; // Each move in the array will delay by
	const float TotalOffsetDelay = TargetPositions.Num() * MoveOffsetDelay;

	// Perform the dragging & releasing of the tab
	if (const TSharedPtr<SDockTab> MajTab = FUMSlateHelpers::GetActiveMajorTab())
		if (!FUMInputHelpers::DragAndReleaseWidgetAtPosition(
				MajTab.ToSharedRef(), TargetPositions, MoveOffsetDelay))
			return;

	// Restore the original position of the cursor to where it was pre-shenanigans
	FTimerHandle TimerHandle_MoveMouseToOrigin;
	GEditor->GetTimerManager()->SetTimer(
		TimerHandle_MoveMouseToOrigin,
		[&SlateApp, MouseOriginPos]() {
			SlateApp.SetCursorPos(FVector2D(MouseOriginPos));
			SlateApp.GetPlatformApplication()->Cursor->Show(true); // Reveal <3
		},
		// We want to compensate on the release delay too, so MoveOffset * 2
		(TotalOffsetDelay) + (MoveOffsetDelay * 2),
		false);
}

void UUMTabNavigatorEditorSubsystem::MoveActiveTabOut()
{
}

void UUMTabNavigatorEditorSubsystem::BindVimCommands()
{
	TSharedRef<FVimInputProcessor> VimInputProcessor = FVimInputProcessor::Get();

	TWeakObjectPtr<UUMTabNavigatorEditorSubsystem> WeakTabSubsystem =
		MakeWeakObjectPtr(this);

	VimInputProcessor->AddKeyBinding_NoParam(
		EUMBindingContext::Generic,
		{ EKeys::G, EKeys::T },
		[this]() { CycleTabs(true, true); });

	VimInputProcessor->AddKeyBinding_NoParam(
		EUMBindingContext::Generic,
		{ EKeys::G, FInputChord(EModifierKey::Shift, EKeys::T) },
		[this]() { CycleTabs(true, false); });

	VimInputProcessor->AddKeyBinding_KeyEvent(
		EUMBindingContext::Generic,
		{ EKeys::M, EKeys::T, EKeys::W, EKeys::Zero },
		WeakTabSubsystem,
		&UUMTabNavigatorEditorSubsystem::MoveActiveTabToWindow);

	VimInputProcessor->AddKeyBinding_KeyEvent(
		EUMBindingContext::Generic,
		{ EKeys::M, EKeys::T, EKeys::W, EKeys::One },
		WeakTabSubsystem,
		&UUMTabNavigatorEditorSubsystem::MoveActiveTabToWindow);

	VimInputProcessor->AddKeyBinding_KeyEvent(
		EUMBindingContext::Generic,
		{ EKeys::M, EKeys::T, EKeys::W, EKeys::Two },
		WeakTabSubsystem,
		&UUMTabNavigatorEditorSubsystem::MoveActiveTabToWindow);

	VimInputProcessor->AddKeyBinding_KeyEvent(
		EUMBindingContext::Generic,
		{ EKeys::M, EKeys::T, EKeys::W, EKeys::Three },
		WeakTabSubsystem,
		&UUMTabNavigatorEditorSubsystem::MoveActiveTabToWindow);

	VimInputProcessor->AddKeyBinding_KeyEvent(
		EUMBindingContext::Generic,
		{ EKeys::M, EKeys::T, EKeys::W, EKeys::Four },
		WeakTabSubsystem,
		&UUMTabNavigatorEditorSubsystem::MoveActiveTabToWindow);

	VimInputProcessor->AddKeyBinding_NoParam(
		EUMBindingContext::Generic,
		{ EKeys::M, EKeys::T, EKeys::O },
		WeakTabSubsystem,
		&UUMTabNavigatorEditorSubsystem::MoveActiveTabOut);
}

void UUMTabNavigatorEditorSubsystem::RegisterCycleTabNavigation(
	const TSharedPtr<FBindingContext>& MainFrameContext)
{
	UI_COMMAND_EXT(MainFrameContext.Get(), CmdInfoNextMajorTab,
		"CycleMajorTabNext", "Cycle Next Major Tab",
		"Move to the next major tab (if it exists)",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Control, EKeys::RightBracket));

	UI_COMMAND_EXT(MainFrameContext.Get(), CmdInfoPrevMajorTab,
		"CycleMajorTabPrevious", "Cycle Previous Major Tab",
		"Move to the previous major tab (if it exists)",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Control, EKeys::LeftBracket));

	UI_COMMAND_EXT(MainFrameContext.Get(), CmdInfoNextMinorTab,
		"CycleMinorTabNext", "Cycle Next Minor Tab",
		"Move to the next minor tab (if it exists)",
		EUserInterfaceActionType::Button, FInputChord(EModifierKey::FromBools(true, false, true, false), EKeys::RightBracket));

	UI_COMMAND_EXT(MainFrameContext.Get(), CmdInfoPrevMinorTab,
		"CycleMinorTabPrevious", "Cycle Previous Minor Tab",
		"Move to the previous minor tab (if it exists)",
		EUserInterfaceActionType::Button, FInputChord(EModifierKey::FromBools(true, false, true, false), EKeys::LeftBracket));
}

/**
 * I've tried to call this in a simple loop (instead of by hand), but no go.
 * DOC:Macro for extending the command list (add more actions to its context)
 */
void UUMTabNavigatorEditorSubsystem::AddMajorTabsNavigationCommandsToList(
	TSharedPtr<FBindingContext> MainFrameContext)
{
	UI_COMMAND_EXT(MainFrameContext.Get(), CommandInfoMajorTabs[0],
		"MoveToMajorTabLast", "Focus Major Last Tab",
		"Activates the last major tab in the current tab well", EUserInterfaceActionType::Button, TabChords[0]);

	UI_COMMAND_EXT(MainFrameContext.Get(), CommandInfoMajorTabs[1],
		"MoveToMajorTab1", "Focus Major Tab 1",
		"Activates the first major tab in the current tab well", EUserInterfaceActionType::Button, TabChords[1]);

	UI_COMMAND_EXT(MainFrameContext.Get(), CommandInfoMajorTabs[2],
		"MoveToMajorTab2", "Focus Major Tab 2",
		"Activates the second major tab in the current tab well", EUserInterfaceActionType::Button, TabChords[2]);

	UI_COMMAND_EXT(MainFrameContext.Get(), CommandInfoMajorTabs[3],
		"MoveToMajorTab3", "Focus Major Tab 3",
		"Activates the third major tab in the current tab well", EUserInterfaceActionType::Button, TabChords[3]);

	UI_COMMAND_EXT(MainFrameContext.Get(), CommandInfoMajorTabs[4],
		"MoveToMajorTab4", "Focus Major Tab 4",
		"Activates the fourth major tab in the current tab well", EUserInterfaceActionType::Button, TabChords[4]);

	UI_COMMAND_EXT(MainFrameContext.Get(), CommandInfoMajorTabs[5],
		"MoveToMajorTab5", "Focus Major Tab 5",
		"Activates the fifth major tab in the current tab well", EUserInterfaceActionType::Button, TabChords[5]);

	UI_COMMAND_EXT(MainFrameContext.Get(), CommandInfoMajorTabs[6],
		"MoveToMajorTab6", "Focus Major Tab 6",
		"Activates the sixth major tab in the current tab well", EUserInterfaceActionType::Button, TabChords[6]);

	UI_COMMAND_EXT(MainFrameContext.Get(), CommandInfoMajorTabs[7],
		"MoveToMajorTab7", "Focus Major Tab 7",
		"Activates the seventh major tab in the current tab well", EUserInterfaceActionType::Button, TabChords[7]);

	UI_COMMAND_EXT(MainFrameContext.Get(), CommandInfoMajorTabs[8],
		"MoveToMajorTab8", "Focus Major Tab 8",
		"Activates the eighth major tab in the current tab well", EUserInterfaceActionType::Button, TabChords[8]);

	UI_COMMAND_EXT(MainFrameContext.Get(), CommandInfoMajorTabs[9],
		"MoveToMajorTab9", "Focus Major Tab 9",
		"Activates the ninth major tab in the current tab well", EUserInterfaceActionType::Button, TabChords[9]);
}

void UUMTabNavigatorEditorSubsystem::AddMinorTabsNavigationCommandsToList(
	TSharedPtr<FBindingContext> MainFrameContext)
{
	UI_COMMAND_EXT(MainFrameContext.Get(), CommandInfoMinorTabs[0],
		"MoveToMinorTabLast", "Focus Minor Last Tab",
		"Activates the last minor tab in the current tab well",
		EUserInterfaceActionType::Button, TabChords[0]);

	UI_COMMAND_EXT(MainFrameContext.Get(), CommandInfoMinorTabs[1],
		"MoveToMinorTab1", "Focus Minor Tab 1",
		"Activates the first minor tab in the current tab well",
		EUserInterfaceActionType::Button, TabChords[1]);

	UI_COMMAND_EXT(MainFrameContext.Get(), CommandInfoMinorTabs[2],
		"MoveToMinorTab2", "Focus Minor Tab 2",
		"Activates the second minor tab in the current tab well",
		EUserInterfaceActionType::Button, TabChords[2]);

	UI_COMMAND_EXT(MainFrameContext.Get(), CommandInfoMinorTabs[3],
		"MoveToMinorTab3", "Focus Minor Tab 3",
		"Activates the third minor tab in the current tab well",
		EUserInterfaceActionType::Button, TabChords[3]);

	UI_COMMAND_EXT(MainFrameContext.Get(), CommandInfoMinorTabs[4],
		"MoveToMinorTab4", "Focus Minor Tab 4",
		"Activates the fourth minor tab in the current tab well",
		EUserInterfaceActionType::Button, TabChords[4]);

	UI_COMMAND_EXT(MainFrameContext.Get(), CommandInfoMinorTabs[5],
		"MoveToMinorTab5", "Focus Minor Tab 5",
		"Activates the fifth minor tab in the current tab well",
		EUserInterfaceActionType::Button, TabChords[5]);

	UI_COMMAND_EXT(MainFrameContext.Get(), CommandInfoMinorTabs[6],
		"MoveToMinorTab6", "Focus Minor Tab 6",
		"Activates the sixth minor tab in the current tab well",
		EUserInterfaceActionType::Button, TabChords[6]);

	UI_COMMAND_EXT(MainFrameContext.Get(), CommandInfoMinorTabs[7],
		"MoveToMinorTab7", "Focus Minor Tab 7",
		"Activates the seventh minor tab in the current tab well",
		EUserInterfaceActionType::Button, TabChords[7]);

	UI_COMMAND_EXT(MainFrameContext.Get(), CommandInfoMinorTabs[8],
		"MoveToMinorTab8", "Focus Minor Tab 8",
		"Activates the eighth minor tab in the current tab well",
		EUserInterfaceActionType::Button, TabChords[8]);

	UI_COMMAND_EXT(MainFrameContext.Get(), CommandInfoMinorTabs[9],
		"MoveToMinorTab9", "Focus Minor Tab 9",
		"Activates the ninth minor tab in the current tab well",
		EUserInterfaceActionType::Button, TabChords[9]);
}

#undef LOCTEXT_NAMESPACE
