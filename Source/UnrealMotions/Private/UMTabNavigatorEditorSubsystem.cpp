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

bool UUMTabNavigatorEditorSubsystem::FindActiveMinorTabs()
{
	TSharedPtr<SDockTab> ActiveMajorTab;
	if (!TryGetValidTargetTab(ActiveMajorTab, true))
		return false;

	// All panel tabs live inside a Docking Area. So we should really search
	// only within it.
	TWeakPtr<SWidget> DockingArea;
	if (!FUMSlateHelpers::TraverseWidgetTree(
			ActiveMajorTab->GetContent(), DockingArea, "SDockingArea"))
		return false;

	// Next stop will be the splitter within it, which also includes all.
	TWeakPtr<SWidget> Splitter;
	if (!FUMSlateHelpers::TraverseWidgetTree(
			ActiveMajorTab->GetContent(), Splitter, "SSplitter"))
		return false;

	// Collect all SDockTab(s)
	TArray<TWeakPtr<SWidget>>
		FoundPanelTabs;
	if (FUMSlateHelpers::TraverseWidgetTree(
			Splitter.Pin(), FoundPanelTabs, "SDockTab"))
	{
		for (const auto& Tab : FoundPanelTabs)
		{
			TSharedPtr<SDockTab> DockTab =
				StaticCastSharedRef<SDockTab>(Tab.Pin().ToSharedRef());
		}
		const int32 TabIndex = FMath::RandRange(0, FoundPanelTabs.Num() - 1);
		if (const TSharedPtr<SDockTab> DockTab = StaticCastSharedRef<SDockTab>(FoundPanelTabs[TabIndex].Pin().ToSharedRef()))
		{
			FSlateApplication::Get().ClearAllUserFocus();
			DockTab->ActivateInParent(ETabActivationCause::SetDirectly);
			FSlateApplication::Get().SetAllUserFocus(
				DockTab->GetContent(), EFocusCause::Navigation);

			OnNewMajorTabChanged.Broadcast(ActiveMajorTab, DockTab);
			return true;
		}
	}
	return false;
}

void UUMTabNavigatorEditorSubsystem::DebugTab(
	const TSharedPtr<SDockTab>& Tab,
	bool bDebugVisualTabRole, FString DelegateType)
{
	if (!Tab.IsValid())
		return;

	auto GetRoleString = [](ETabRole TabRole) -> FString {
		switch (TabRole)
		{
			case ETabRole::MajorTab:
				return TEXT("MajorTab");
			case ETabRole::PanelTab:
				return TEXT("PanelTab");
			case ETabRole::NomadTab:
				return TEXT("NomadTab");
			case ETabRole::DocumentTab:
				return TEXT("DocumentTab");
			case ETabRole::NumRoles:
				return TEXT("NumRoles");
			default:
				return TEXT("Unknown");
		}
	};

	FString VisualRole = GetRoleString(Tab->GetVisualTabRole());
	FString RegularRole = GetRoleString(Tab->GetTabRole());
	FString Name = Tab->GetTabLabel().ToString();
	int32	Id = Tab->GetId();
	FString LayoutId = Tab->GetLayoutIdentifier().ToString();
	bool	bIsToolkit = LayoutId.EndsWith(TEXT("Toolkit"));
	FString DebugMsg = FString::Printf(
		TEXT("Called From: %s\n"
			 "Tab Details:\n"
			 "  Name: %s\n"
			 "  Visual Role: %s\n"
			 "  Regular Role: %s\n"
			 "  Id: %d\n"
			 "  Layout ID: %s\n"
			 "  Is Toolkit: %s"),
		*DelegateType, *Name, *VisualRole, *RegularRole, Id, *LayoutId, bIsToolkit ? TEXT("true") : TEXT("false"));
}

// Vim Related Functions:

void UUMTabNavigatorEditorSubsystem::MoveActiveTabToWindow(
	FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	TSharedPtr<SWidget> TargetTabWell;

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

		TSharedPtr<SWindow> TargetWindow =
			(RootWindow.IsValid() && RootWindow->IsActive())
			? RegularVisWins[KeyAsDigit]
			: RegularVisWins[(RegularVisWins.Num() - 1) - KeyAsDigit];

		// Get the first TabWell in the Target Window (Major Tab's TabWell)
		TWeakPtr<SWidget> FoundTargetTabWell;
		if (!FUMSlateHelpers::TraverseWidgetTree(
				TargetWindow, FoundTargetTabWell, "SDockingTabWell"))
			return;

		if (const auto PinFoundTargetTabWell = FoundTargetTabWell.Pin())
			TargetTabWell = PinFoundTargetTabWell;
		else
			return;
	}

	// We want to drag our tab to the new TabWell and append it to the end
	const FVector2f TabWellTargetPosition =
		FUMSlateHelpers::GetWidgetTopRightScreenSpacePosition(
			TargetTabWell.ToSharedRef());

	// Store the origin position so we can restore it at the end
	const FVector2f MouseOriginPos = SlateApp.GetCursorPos();

	// Perform the dragging
	if (!DragAndReleaseActiveTabAtPosition(SlateApp, TabWellTargetPosition))
		return;

	// Restore the original position of the cursor to where it was pre-shenanigans
	FTimerHandle TimerHandle_MoveMouseToOrigin;
	GEditor->GetTimerManager()->SetTimer(
		TimerHandle_MoveMouseToOrigin,
		[&SlateApp, MouseOriginPos]() {
			SlateApp.SetCursorPos(FVector2D(MouseOriginPos));
			SlateApp.GetPlatformApplication()->Cursor->Show(true); // Reveal <3
		},
		0.06f,
		false);
}

void UUMTabNavigatorEditorSubsystem::MoveActiveTabOut()
{
}

bool UUMTabNavigatorEditorSubsystem::DragAndReleaseActiveTabAtPosition(
	FSlateApplication& SlateApp, FVector2f TargetPosition)
{
	// Get our target (currently focused) tab.
	TSharedPtr<SDockTab> ActiveMajorTab = FUMSlateHelpers::GetActiveMajorTab();
	if (!ActiveMajorTab.IsValid())
		return false;

	// Calculate the active tab center, which is where we will position to mouse
	// to simulate the mouse press on top of.
	FVector2f MajorTabCenterPos =
		FUMSlateHelpers::GetWidgetCenterScreenSpacePosition(
			ActiveMajorTab.ToSharedRef());

	// We will need the Native Window as a parameter for the MouseDown Simulation
	TSharedPtr<FGenericWindow> GenericActiveWindow =
		FUMSlateHelpers::GetGenericActiveTopLevelWindow();
	if (!GenericActiveWindow.IsValid())
		return false;

	// Hide the mouse cursor to reduce flickering (not very important - but nice)
	SlateApp.GetPlatformApplication()->Cursor->Show(false);

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
	// 10 seems to be around the minimum to trigger the dragging.
	// TODO: Test on more screen resolutions and on MacOS
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

	// We need a slight delay to let the drag adjust...
	// This seems to be important; else it won't catch up and break.
	TSharedRef<FTimerManager> TimerManager = GEditor->GetTimerManager();

	FTimerHandle TimerHandle_MoveMouseToTabWell;
	TimerManager->SetTimer(
		TimerHandle_MoveMouseToTabWell,
		[&SlateApp, TargetPosition]() {
			SlateApp.SetCursorPos(FVector2D(TargetPosition));
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

	return true;
}

void UUMTabNavigatorEditorSubsystem::BindVimCommands()
{
	TSharedRef<FVimInputProcessor> VimInputProcessor = FVimInputProcessor::Get();

	TWeakObjectPtr<UUMTabNavigatorEditorSubsystem> WeakTabSubsystem =
		MakeWeakObjectPtr(this);

	VimInputProcessor->AddKeyBinding_KeyEvent(
		// { EKeys::SpaceBar, EKeys::M, EKeys::T, EKeys::W, EKeys::Zero },
		{ EKeys::M, EKeys::T, EKeys::W, EKeys::Zero },
		WeakTabSubsystem,
		&UUMTabNavigatorEditorSubsystem::MoveActiveTabToWindow);

	VimInputProcessor->AddKeyBinding_KeyEvent(
		{ EKeys::M, EKeys::T, EKeys::W, EKeys::One },
		WeakTabSubsystem,
		&UUMTabNavigatorEditorSubsystem::MoveActiveTabToWindow);

	VimInputProcessor->AddKeyBinding_KeyEvent(
		{ EKeys::M, EKeys::T, EKeys::W, EKeys::Two },
		WeakTabSubsystem,
		&UUMTabNavigatorEditorSubsystem::MoveActiveTabToWindow);

	VimInputProcessor->AddKeyBinding_KeyEvent(
		{ EKeys::M, EKeys::T, EKeys::W, EKeys::Three },
		WeakTabSubsystem,
		&UUMTabNavigatorEditorSubsystem::MoveActiveTabToWindow);

	VimInputProcessor->AddKeyBinding_KeyEvent(
		{ EKeys::M, EKeys::T, EKeys::W, EKeys::Four },
		WeakTabSubsystem,
		&UUMTabNavigatorEditorSubsystem::MoveActiveTabToWindow);

	VimInputProcessor->AddKeyBinding_NoParam(
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
