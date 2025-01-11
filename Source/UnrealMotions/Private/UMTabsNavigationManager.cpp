#include "UMTabsNavigationManager.h"

#include "Interfaces/IMainFrameModule.h"
#include "Framework/Commands/UICommandInfo.h"
#include "Widgets/Docking/SDockTab.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
// #include "BlueprintEditor.h"

#include "UMLogger.h"
#include "UMSlateHelpers.h"

// DEFINE_LOG_CATEGORY_STATIC(LogUMTabsNavigation, NoLogging, All); // Prod
DEFINE_LOG_CATEGORY_STATIC(LogUMTabsNavigation, Log, All); // Dev

#define LOCTEXT_NAMESPACE "UMTabsNavigationManager"

TSharedPtr<FUMTabsNavigationManager>
	FUMTabsNavigationManager::TabsNavigationManager =
		MakeShared<FUMTabsNavigationManager>();

FUMTabsNavigationManager::FUMTabsNavigationManager()
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

	FCoreDelegates::OnPostEngineInit.AddRaw(
		this, &FUMTabsNavigationManager::RegisterSlateEvents);
}

FUMTabsNavigationManager::~FUMTabsNavigationManager()
{
	TabChords.Empty();
	CommandInfoMajorTabs.Empty();
	CommandInfoMinorTabs.Empty();
}

FUMTabsNavigationManager& FUMTabsNavigationManager::Get()
{
	if (!FUMTabsNavigationManager::TabsNavigationManager.IsValid())
	{
		FUMTabsNavigationManager::TabsNavigationManager =
			MakeShared<FUMTabsNavigationManager>();
	}
	return *FUMTabsNavigationManager::TabsNavigationManager;
}

bool FUMTabsNavigationManager::IsInitialized()
{
	return FUMTabsNavigationManager::TabsNavigationManager.IsValid();
}

void FUMTabsNavigationManager::InitTabNavInputChords(
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

void FUMTabsNavigationManager::RemoveDefaultCommand(
	FInputBindingManager& InputBindingManager, FInputChord Command)
{
	const TSharedPtr<FUICommandInfo> CheckCommand =
		InputBindingManager.GetCommandInfoFromInputChord(MainFrameContextName, Command,
			false /* We want to check the current, not default command */);
	if (CheckCommand.IsValid())
		CheckCommand->RemoveActiveChord(EMultipleKeyBindingIndex::Primary);
}

void FUMTabsNavigationManager::MapTabCommands(
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

void FUMTabsNavigationManager::MapCycleTabsNavigation(
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

void FUMTabsNavigationManager::RegisterSlateEvents()
{
}

//** Isn't currently being used *//
void FUMTabsNavigationManager::OnMouseButtonDown(const FPointerEvent& PointerEvent)
{
	FUMLogger::NotifySuccess(FText::FromString("On Mouse Button Down!"), VisualLog);
	// Set tab...
}

bool FUMTabsNavigationManager::TryGetValidTargetTab(
	TSharedPtr<SDockTab>& OutTab, bool bIsMajorTab)
{
	TSharedRef<FGlobalTabmanager> GTM = FGlobalTabmanager::Get();
	if (const TSharedPtr<SDockTab> ActiveMinorTab = GTM->GetActiveTab())
	{
		if (bIsMajorTab)
		{
			if (const TSharedPtr<FTabManager> TabManager =
					ActiveMinorTab->GetTabManagerPtr())
			{
				if (const TSharedPtr<SDockTab> ActiveMajorTab =
						GTM->GetMajorTabForTabManager(TabManager.ToSharedRef()))
				{
					OutTab = ActiveMajorTab;
					return true;
				}
			}
		}
		else // Minor Tab
		{
			OutTab = ActiveMinorTab;
			return true;
		}
	}
	return false;
}

void FUMTabsNavigationManager::MoveToTabIndex(int32 TabIndex, bool bIsMajorTab)
{
	UE_LOG(LogUMTabsNavigation, Display,
		TEXT("MoveToTabIndex, Tab Index: %d, bIsMajorTab: %d"),
		TabIndex, bIsMajorTab);

	TSharedPtr<SDockTab> TargetTab;
	if (!TryGetValidTargetTab(TargetTab, bIsMajorTab))
		return;

	TSharedPtr<SWidget> TabWell = TargetTab->GetParentWidget();
	if (!TabWell.IsValid() || !TabWell->GetType().IsEqual("SDockingTabWell"))
		return;

	FocusTabIndex(TabWell, TabIndex, bIsMajorTab);
	return;
}

void FUMTabsNavigationManager::FocusTabIndex(
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

void FUMTabsNavigationManager::CycleTabs(bool bIsMajorTab, bool bIsNextTab)
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

//** Deprecated *//
bool FUMTabsNavigationManager::GetLastActiveNonMajorTab(TWeakPtr<SDockTab>& OutNonMajorTab)
{
	// TODO: Check if by some weird reason this detects something that the
	// delegate doesn't.
	const TSharedPtr<SDockTab>& T = FGlobalTabmanager::Get()->GetActiveTab();
	if (T.IsValid())
	{
		OutNonMajorTab = T;
		return true;
	}
	return false;
}

bool FUMTabsNavigationManager::FindActiveMinorTabs()
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

void FUMTabsNavigationManager::DebugTab(const TSharedPtr<SDockTab>& Tab,
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
	UE_LOG(LogUMTabsNavigation, Display, TEXT("%s"), *DebugMsg);
	FUMLogger::NotifySuccess(FText::FromString(DebugMsg), VisualLog, 12.0f);
	// FUMLogger::AddDebugMessage(
	// 	DebugMsg, EUMHelpersLogMethod::PrintToScreen,
	// 	16.0f, FLinearColor::Yellow, 1.0f, 10);
}

ENavSpecTabType FUMTabsNavigationManager::GetNavigationSpecificTabType(const TSharedPtr<SDockTab>& Tab)
{
	if (Tab)
	{
		FString TabId = Tab->GetLayoutIdentifier().ToString();
		if (TabId.EndsWith(TEXT("Toolkit")))
			return ENavSpecTabType::Toolkit;
		else if (TabId.StartsWith(TEXT("LevelEditor")))
			return ENavSpecTabType::LevelEditor;
	}
	return ENavSpecTabType::None;
}

void FUMTabsNavigationManager::RegisterCycleTabNavigation(const TSharedPtr<FBindingContext>& MainFrameContext)
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
void FUMTabsNavigationManager::AddMajorTabsNavigationCommandsToList(TSharedPtr<FBindingContext> MainFrameContext)
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

void FUMTabsNavigationManager::AddMinorTabsNavigationCommandsToList(TSharedPtr<FBindingContext> MainFrameContext)
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
