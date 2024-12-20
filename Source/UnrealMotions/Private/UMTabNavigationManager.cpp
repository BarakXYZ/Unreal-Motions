#include "UMTabNavigationManager.h"
#include "Framework/Commands/UICommandInfo.h"
#include "Templates/SharedPointer.h"
#include "Widgets/Docking/SDockTab.h"
#include "UMHelpers.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
// #include "BlueprintEditor.h"
// #include "Subsystems/AssetEditorSubsystem.h"

/* Main window representation of the app.
 * All other windows are children of it. */
#include "Interfaces/IMainFrameModule.h"

// DEFINE_LOG_CATEGORY_STATIC(LogUMTabNavigation, NoLogging, All); // Prod
DEFINE_LOG_CATEGORY_STATIC(LogUMTabNavigation, Log, All); // Dev

#define LOCTEXT_NAMESPACE "UMTabNavigationManager"

FUMTabNavigationManager::FUMTabNavigationManager()
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
	RegisterNextPrevTabNavigation(MainFrameContext);
	MapNextPrevTabNavigation(CommandList);

	FCoreDelegates::OnPostEngineInit.AddRaw(
		this, &FUMTabNavigationManager::RegisterSlateEvents);

	// Debugging
	// SetupFindTabWells(CommandList, MainFrameContext);
}

FUMTabNavigationManager::~FUMTabNavigationManager()
{
	TabChords.Empty();
	CommandInfoMajorTabs.Empty();
	CommandInfoMinorTabs.Empty();
}

void FUMTabNavigationManager::InitTabNavInputChords(
	bool bCtrl, bool bAlt, bool bShift, bool bCmd)

{
	if (!TabChords.IsEmpty())
		TabChords.Empty();
	TabChords.Reserve(10);

	const FKey NumberKeys[] = { EKeys::Zero, EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four,
		EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine };

	// Setting by default to Control + Shift + 0-9
	const EModifierKey::Type ModifierKeys = EModifierKey::FromBools(
		bCtrl, bAlt, bShift, bCmd);

	for (const FKey& Key : NumberKeys)
		TabChords.Add(FInputChord(ModifierKeys, Key));
}

void FUMTabNavigationManager::RemoveDefaultCommand(
	FInputBindingManager& InputBindingManager, FInputChord Command)
{
	const TSharedPtr<FUICommandInfo> CheckCommand = InputBindingManager.GetCommandInfoFromInputChord(MainFrameContextName, Command,
		false /* We want to check the current, not default command */);
	if (CheckCommand.IsValid())
		CheckCommand->RemoveActiveChord(EMultipleKeyBindingIndex::Primary);
}

void FUMTabNavigationManager::MapTabCommands(const TSharedRef<FUICommandList>& CommandList, const TArray<TSharedPtr<FUICommandInfo>>& CommandInfo, bool bIsMajorTab)
{
	for (int i{ 0 }; i < TabChords.Num(); ++i)
	{
		CommandList->MapAction(CommandInfo[i],
			FExecuteAction::CreateLambda(
				[this, i, bIsMajorTab]() { OnMoveToTabIndex(i, bIsMajorTab); }));
	}
}

void FUMTabNavigationManager::MapNextPrevTabNavigation(const TSharedRef<FUICommandList>& CommandList)
{
	CommandList->MapAction(CmdInfoNextMajorTab,
		FExecuteAction::CreateLambda(
			[this]() { OnCycleTabs(true, true); }));

	CommandList->MapAction(CmdInfoPrevMajorTab,
		FExecuteAction::CreateLambda(
			[this]() { OnCycleTabs(true, false); }));

	CommandList->MapAction(CmdInfoNextMinorTab,
		FExecuteAction::CreateLambda(
			[this]() { OnCycleTabs(false, true); }));

	CommandList->MapAction(CmdInfoPrevMinorTab,
		FExecuteAction::CreateLambda(
			[this]() { OnCycleTabs(false, false); }));
}

void FUMTabNavigationManager::OnActiveTabChanged(
	TSharedPtr<SDockTab> PreviousActiveTab, TSharedPtr<SDockTab> NewActiveTab)
{
	if (!NewActiveTab.IsValid() || !PreviousActiveTab.IsValid())
		return;

	const FString& LogTabs = FString::Printf(
		TEXT("Active tab changed to: %s Previous Active Tab: %s"),
		*NewActiveTab->GetTabLabel().ToString(),
		*PreviousActiveTab->GetTabLabel().ToString());

	UE_LOG(LogUMTabNavigation, Log, TEXT("%s"), *LogTabs);
	FUMHelpers::NotifySuccess(FText::FromString(LogTabs), VisualLog);

	SetNewCurrentTab(NewActiveTab, false); // Set Minor Tab

	if (const TSharedPtr<FTabManager> TabManager =
			NewActiveTab->GetTabManagerPtr())
	{
		if (const TSharedPtr<SDockTab> MajorTab =
				FGlobalTabmanager::Get()->GetMajorTabForTabManager(TabManager.ToSharedRef()))
		{
			SetNewCurrentTab(MajorTab, true);
			// Interesting
			// TabManager->GetPrivateApi().GetParentWindow();
		}
	}
}

bool FUMTabNavigationManager::ValidateTargetTab(TSharedPtr<SWidget>& OutTab, bool bIsMajorTab)
{
	if (bIsMajorTab && CurrMajorTab.IsValid())
		OutTab = CurrMajorTab.Pin();

	else if (!bIsMajorTab && CurrMinorTab.IsValid())
		OutTab = CurrMinorTab.Pin();
	else
		return false;
	return true;
}

bool FUMTabNavigationManager::GetParentTabWellFromTab(TSharedPtr<SWidget>& InOutTabWell)
{
	const TSharedPtr<SWidget>& ParentWidget = InOutTabWell->GetParentWidget();
	if (!ParentWidget.IsValid())
		return false;
	else
		InOutTabWell = ParentWidget;
	return true;
}

void FUMTabNavigationManager::OnMoveToTabIndex(int32 TabIndex, bool bIsMajorTab)
{
	UE_LOG(LogUMTabNavigation, Display,
		TEXT("OnMoveToTabIndex, Tab Index: %d, bIsMajorTab: %d"),
		TabIndex, bIsMajorTab);

	TSharedPtr<SWidget> Tab = nullptr;
	if (ValidateTargetTab(Tab, bIsMajorTab) && GetParentTabWellFromTab(Tab))
		FocusTabIndex(Tab, TabIndex);

	/**
	 * Deprecated (Not needed with the new method, keeping for reference)
	 * BringToFront is important to mitigate a bug where modal windows
	 * will semi-steal focus from the active window when switching tabs.
	 * Essentially, bring to front helps with solidifying the focus.
	 * */
	// ActiveWindow->BringToFront();
}

void FUMTabNavigationManager::FocusTabIndex(
	const TSharedPtr<SWidget>& DockingTabWell, int32 TabIndex)
{
	FChildren* Tabs = DockingTabWell->GetChildren();
	/* Check if valid and if we have more are the same number of tabs as index */
	if (Tabs && Tabs->Num() >= TabIndex)
	{
		if (TabIndex == 0)
			TabIndex = Tabs->Num(); // Get last child
		const TSharedPtr<SDockTab>& TargetTab =
			StaticCastSharedRef<SDockTab>(Tabs->GetChildAt(TabIndex - 1));
		if (TargetTab.IsValid())
		{
			TargetTab->ActivateInParent(ETabActivationCause::SetDirectly);
			// TargetTab->DrawAttention();
		}
	}
}

void FUMTabNavigationManager::OnCycleTabs(bool bIsMajorTab, bool bIsNextTab)
{
	TSharedPtr<SWidget> TargetTab = nullptr;
	if (!ValidateTargetTab(TargetTab, bIsMajorTab))
		return;
	TSharedPtr<SWidget> TabWell = TargetTab;
	if (!GetParentTabWellFromTab(TabWell))
		return;
	FChildren* Tabs = TabWell->GetChildren();
	if (!Tabs || Tabs->Num() == 0)
		return;

	int32		CurrIndex = INDEX_NONE;
	const int32 TNum = Tabs->Num();

	const auto DockTarget = StaticCastSharedPtr<SDockTab>(TargetTab);
	UE_LOG(LogUMTabNavigation, Display, TEXT("DockTarget Tab Label: %s"), *DockTarget->GetTabLabel().ToString());

	for (int32 i = 0; i < TNum; ++i)
	{
		auto Dock = StaticCastSharedRef<SDockTab>(Tabs->GetChildAt(i));
		if (Dock->IsForeground())
		{
			CurrIndex = i;
			UE_LOG(LogUMTabNavigation, Display, TEXT("Current Active Tab Index: %d"), i);
			break;
		}
	}

	if (CurrIndex == INDEX_NONE)
		return;

	CurrIndex = bIsNextTab ? (CurrIndex + 1) % TNum : (CurrIndex - 1 + TNum) % TNum;

	TSharedPtr<SDockTab> DockTab = StaticCastSharedRef<SDockTab>(
		Tabs->GetChildAt(CurrIndex));
	if (DockTab.IsValid())
	{
		// None of these methods will actually trigger the:
		// "On Focus" / "Tab Changed" delegates :(
		DockTab->ActivateInParent(ETabActivationCause::SetDirectly);
		// DockTab->ActivateInParent(ETabActivationCause::UserClickedOnTab);
		// DockTab->DrawAttention();

		// When switching between major tabs, for some reason they won't
		// register our Delegate for tracking current tabs. Thus we need
		// to set this manually.
		if (bIsMajorTab)
			SetNewCurrentTab(DockTab, true);
	}
}

//** Deprecated *//
bool FUMTabNavigationManager::GetLastActiveNonMajorTab(TWeakPtr<SDockTab>& OutNonMajorTab)
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

bool FUMTabNavigationManager::GetActiveMajorTab(TWeakPtr<SDockTab>& OutMajorTab)
{
	if (!CurrWin.IsValid())
		CurrWin = FSlateApplication::Get().GetActiveTopLevelWindow();
	TWeakPtr<SWidget> FoundWidget;
	if (!TraverseWidgetTree(CurrWin.Pin(), FoundWidget, "SDockingTabWell", 1))
		return false;

	if (FChildren* Tabs = FoundWidget.Pin()->GetChildren())
	{
		for (int32 i{ 0 }; i < Tabs->Num(); ++i)
		{
			if (Tabs->GetChildAt(i)->GetTypeAsString() == "SDockTab")
			{
				TSharedRef<SDockTab> Tab =
					StaticCastSharedRef<SDockTab>(Tabs->GetChildAt(i));
				if (Tab->IsForeground())
				{
					OutMajorTab = Tab.ToWeakPtr();
					return true;
				}
			}
		}
	}
	return false;
}

bool FUMTabNavigationManager::FindRootTargetWidgetName(FString& OutRootWidgetName)
{
	TWeakPtr<SDockTab> MajorTab = nullptr;
	if (!GetActiveMajorTab(MajorTab))
		return false;

	ENavSpecTabType TabType = GetNavigationSpecificTabType(MajorTab.Pin());
	switch (TabType)
	{
		case ENavSpecTabType::Toolkit:
			OutRootWidgetName = SToolkit;
			return true;
		case ENavSpecTabType::LevelEditor:
			OutRootWidgetName = SLvlEditor;
			return true;
		case ENavSpecTabType::None:
			return false;
		default:
			return false;
	}
}

void FUMTabNavigationManager::FindAllTabWells()
{
	UE_LOG(LogUMTabNavigation, Display, TEXT("Find All Tab Wells Started!"));

	FString RootName;
	if (!FindRootTargetWidgetName(RootName) || !FSlateApplication::IsInitialized())
		return;
	FSlateApplication&		   App = FSlateApplication::Get();
	const TSharedPtr<SWindow>& Win = App.GetActiveTopLevelWindow();
	if (!Win)
		return;

	TWeakPtr<SWidget> RootWidget = nullptr;
	if (!TraverseWidgetTree(Win, RootWidget, RootName))
		return;

	EditorTabWells.Empty();
	if (!TraverseWidgetTree(RootWidget.Pin(), EditorTabWells, SDTabWell))
		return;

	int32 WellNum{ 0 };
	for (const TWeakPtr<SWidget>& Well : EditorTabWells)
	{
		UE_LOG(LogUMTabNavigation, Display,
			TEXT("TabWell(%d), Pos: %s"),
			WellNum, *Well.Pin()->GetTickSpaceGeometry().ToString());
		if (FChildren* Tabs = Well.Pin()->GetChildren())
		{
			for (int32 i{ 0 }; i < Tabs->Num(); ++i)
			{
				const TSharedPtr<SDockTab>& Tab =
					StaticCastSharedRef<SDockTab>(Tabs->GetChildAt(i));
				if (Tab.IsValid())
				{
					UE_LOG(LogUMTabNavigation, Display, TEXT("%sTab Name: %s"),
						*FString::ChrN(12, ' '), *Tab->GetTabLabel().ToString());
				}
			}
		}
		++WellNum;
	}
}

void FUMTabNavigationManager::DebugTab(const TSharedPtr<SDockTab>& Tab)
{
	FString Role;
	switch (Tab->GetVisualTabRole())
	{
		case ETabRole::MajorTab:
			Role = TEXT("MajorTab");
			break;
		case ETabRole::PanelTab:
			Role = TEXT("PanelTab");
			break;
		case ETabRole::NomadTab:
			Role = TEXT("NomadTab");
			break;
		case ETabRole::DocumentTab:
			Role = TEXT("DocumentTab");
			break;
		case ETabRole::NumRoles:
			Role = TEXT("NumRoles");
			break;
	}
	FString Name = Tab->GetTabLabel().ToString();
	int32	Id = Tab->GetId();
	UE_LOG(LogUMTabNavigation, Display, TEXT("Tab Details - Name: %s, Role: %s, Id: %d"), *Name, *Role, Id);
}

ENavSpecTabType FUMTabNavigationManager::GetNavigationSpecificTabType(const TSharedPtr<SDockTab>& Tab)
{
	if (Tab)
	{
		FString TabId = Tab->GetLayoutIdentifier().ToString();
		if (TabId.EndsWith(TEXT("Toolkit")))
		{
			return ENavSpecTabType::Toolkit;
		}
		else if (TabId.StartsWith(TEXT("LevelEditor")))
		{
			return ENavSpecTabType::LevelEditor;
		}
	}
	return ENavSpecTabType::None;
}

void FUMTabNavigationManager::DebugWindow(const SWindow& Window)
{
	switch (Window.GetType())
	{
		case (EWindowType::Normal):
			FUMHelpers::NotifySuccess(Window.GetTitle(), VisualLog);
			break;
		case (EWindowType::GameWindow):
			FUMHelpers::NotifySuccess(Window.GetTitle(), VisualLog);
			break;
		default:
			break;
	}
	// UE_LOG(LogUMTabNavigation, Display,
	// 	TEXT("Window Destroyed: %s"), *Window.GetTitle().ToString())
}

void FUMTabNavigationManager::RegisterSlateEvents()
{
	// Currently used!
	TSharedRef<FGlobalTabmanager> GTM = FGlobalTabmanager::Get();
	GTM->OnActiveTabChanged_Subscribe(
		FOnActiveTabChanged::FDelegate::CreateRaw(this, &FUMTabNavigationManager::OnActiveTabChanged));

	FSlateApplication& SlateApp = FSlateApplication::Get();

	// SlateApp.OnWindowBeingDestroyed().AddRaw(this, &FUMTabNavigationManager::DebugWindow);

	// Useful
	SlateApp.OnFocusChanging()
		.AddRaw(this, &FUMTabNavigationManager::OnFocusChanged);

	// SlateApp.OnApplicationMousePreInputButtonDownListener()
	// 	.AddRaw(this, &FUMTabNavigationManager::OnMouseButtonDown);
}

void FUMTabNavigationManager::OnMouseButtonDown(const FPointerEvent& PointerEvent)
{
	FUMHelpers::NotifySuccess(FText::FromString("On Mouse Button Down!"), VisualLog);
	CheckHasMovedToNewWinAndSetTab();
}

void FUMTabNavigationManager::OnFocusChanged(
	const FFocusEvent& FocusEvent, const FWeakWidgetPath& OldWidgetPath,
	const TSharedPtr<SWidget>& OldWidget, const FWidgetPath& NewWidgetPath,
	const TSharedPtr<SWidget>& NewWidget)
{
	CheckHasMovedToNewWinAndSetTab();
}

void FUMTabNavigationManager::CheckHasMovedToNewWinAndSetTab()
{
	if (HasUserMovedToNewWindow())
	{
		// This conditional won't work all the time, sadly. For exmaple when a modal window is up. We maybe can try to see activation time?
		// if (!CurrMajorTab.IsValid() || !CurrMajorTab.Pin()->IsForeground())
		// {
		TWeakPtr<SDockTab> NewMajorTab = nullptr;
		if (GetActiveMajorTab(NewMajorTab))
		{
			SetNewCurrentTab(NewMajorTab.Pin(), true);
			return;
		}
		// }
	}
}

void FUMTabNavigationManager::SetNewCurrentTab(const TSharedPtr<SDockTab>& NewTab, bool bIsMajorTab)
{
	if (!NewTab.IsValid())
	{
		UE_LOG(LogUMTabNavigation, Warning, TEXT("Attempted to set invalid tab as current"));
		return;
	}
	if (bIsMajorTab)
	{
		FString PrevLabel = CurrMajorTab.IsValid() ? CurrMajorTab.Pin()->GetTabLabel().ToString() : TEXT("None");
		CurrMajorTab = NewTab;
		UE_LOG(LogUMTabNavigation, Log, TEXT("Set new major tab - Previous: %s, New: %s"),
			*PrevLabel, *NewTab->GetTabLabel().ToString());
		return;
	}

	FString PrevLabel = CurrMinorTab.IsValid() ? CurrMinorTab.Pin()->GetTabLabel().ToString() : TEXT("None");
	CurrMinorTab = NewTab;
	UE_LOG(LogUMTabNavigation, Log, TEXT("Set new minor tab - Previous: %s, New: %s"),
		*PrevLabel, *NewTab->GetTabLabel().ToString());
	return;
}

bool FUMTabNavigationManager::HasUserMovedToNewWindow(bool bSetNewWindow)
{
	if (const TSharedPtr<SWindow>& Win =
			FSlateApplication::Get().GetActiveTopLevelWindow())
	{

		if (!CurrWin.IsValid() || Win->GetId() != CurrWin.Pin()->GetId())
		{
			if (bSetNewWindow)
				CurrWin = Win;
			FUMHelpers::NotifySuccess(FText::FromString(TEXT("New Window!")), VisualLog);
			return true;
		}
	}
	return false;
}

bool FUMTabNavigationManager::TraverseWidgetTree(
	const TSharedPtr<SWidget>& ParentWidget,
	TArray<TWeakPtr<SWidget>>& OutWidgets,
	const FString& TargetType, int32 SearchCount, int32 Depth)
{
	// Log the current widget and depth
	UE_LOG(LogUMTabNavigation, Display, TEXT("%s[Depth: %d] Checking widget: %s"),
		*FString::ChrN(Depth * 2, ' '), // Visual indentation
		Depth, *ParentWidget->GetTypeAsString());

	bool bFoundAllRequested = false;

	if (ParentWidget->GetTypeAsString() == TargetType)
	{
		UE_LOG(LogUMTabNavigation, Display,
			TEXT("%s[Depth: %d] Found %s | Origin Target: %s"),
			*FString::ChrN(Depth * 2, ' '), Depth, *ParentWidget->ToString(),
			*TargetType);
		OutWidgets.Add(ParentWidget);

		// If SearchCount is -1, continue searching but mark that we found at least one
		// If SearchCount is positive, check if we've found enough
		if (SearchCount == -1)
			bFoundAllRequested = true;
		else if (OutWidgets.Num() >= SearchCount)
			return true;
	}

	// Recursively traverse the children of the current widget
	FChildren* Children = ParentWidget->GetChildren();
	if (Children)
	{
		for (int32 i{ 0 }; i < Children->Num(); ++i)
		{
			TSharedPtr<SWidget> Child = Children->GetChildAt(i);
			bool				bChildFound = TraverseWidgetTree(
				   Child, OutWidgets, TargetType, SearchCount, Depth + 1);

			// If SearchCount is -1, accumulate the "found" status
			if (SearchCount == -1)
				bFoundAllRequested |= bChildFound;

			// If SearchCount is positive and we found enough, return immediately
			else if (bChildFound)
				return true;
		}
	}
	return bFoundAllRequested;
}

bool FUMTabNavigationManager::TraverseWidgetTree(
	const TSharedPtr<SWidget>& ParentWidget,
	TWeakPtr<SWidget>&		   OutWidget,
	const FString&			   TargetType,
	int32					   Depth)
{
	// Log the current widget and depth
	UE_LOG(LogUMTabNavigation, Display, TEXT("%s[Depth: %d] Checking widget: %s"),
		*FString::ChrN(Depth * 2, ' '), // Visual indentation
		Depth, *ParentWidget->GetTypeAsString());

	if (ParentWidget->GetTypeAsString() == TargetType)
	{
		UE_LOG(LogUMTabNavigation, Display,
			TEXT("%s[Depth: %d] Found %s | Origin Target: %s"),
			*FString::ChrN(Depth * 2, ' '), Depth, *ParentWidget->ToString(),
			*TargetType);

		OutWidget = ParentWidget;
		return true;
	}

	// Recursively traverse the children of the current widget
	FChildren* Children = ParentWidget->GetChildren();
	if (Children)
	{
		for (int32 i = 0; i < Children->Num(); ++i)
		{
			TSharedPtr<SWidget> Child = Children->GetChildAt(i);
			if (TraverseWidgetTree(Child, OutWidget, TargetType, Depth + 1))
			{
				return true;
			}
		}
	}

	return false;
}

void FUMTabNavigationManager::RegisterNextPrevTabNavigation(const TSharedPtr<FBindingContext>& MainFrameContext)
{
	UI_COMMAND_EXT(MainFrameContext.Get(), CmdInfoNextMajorTab,
		"MoveToNextMajorTab", "Focus the next major tab",
		"Moves to the next major tab",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Control, EKeys::RightBracket));

	UI_COMMAND_EXT(MainFrameContext.Get(), CmdInfoPrevMajorTab,
		"MoveToPreviousMajorTab", "Focus the previous major tab",
		"Moves to the previous major tab",
		EUserInterfaceActionType::Button,
		FInputChord(EModifierKey::Control, EKeys::LeftBracket));

	UI_COMMAND_EXT(MainFrameContext.Get(), CmdInfoNextMinorTab,
		"MoveToNextMinorTab", "Focus the next minor tab",
		"Moves to the next minor tab",
		EUserInterfaceActionType::Button, FInputChord(EModifierKey::FromBools(true, false, true, false), EKeys::RightBracket));

	UI_COMMAND_EXT(MainFrameContext.Get(), CmdInfoPrevMinorTab,
		"MoveToPreviousMinorTab", "Focus the previous minor tab",
		"Moves to the previous minor tab",
		EUserInterfaceActionType::Button, FInputChord(EModifierKey::FromBools(true, false, true, false), EKeys::LeftBracket));
}

/**
 * I've tried to call this in a simple loop (instead of by hand), but no go.
 * DOC:Macro for extending the command list (add more actions to its context)
 */
void FUMTabNavigationManager::AddMajorTabsNavigationCommandsToList(TSharedPtr<FBindingContext> MainFrameContext)
{
	UI_COMMAND_EXT(MainFrameContext.Get(), CommandInfoMajorTabs[0], "MoveToMajorTabLast", "Focus Major Last Tab",
		"Moves to the major last tab", EUserInterfaceActionType::Button, TabChords[0]);

	UI_COMMAND_EXT(MainFrameContext.Get(), CommandInfoMajorTabs[1], "MoveToMajorTab1", "Focus Major Tab 1",
		"If exists, draws attention to major tab 1", EUserInterfaceActionType::Button, TabChords[1]);

	UI_COMMAND_EXT(MainFrameContext.Get(), CommandInfoMajorTabs[2], "MoveToMajorTab2", "Focus Major Tab 2",
		"If exists, draws attention to major tab 2", EUserInterfaceActionType::Button, TabChords[2]);

	UI_COMMAND_EXT(MainFrameContext.Get(), CommandInfoMajorTabs[3], "MoveToMajorTab3", "Focus Major Tab 3",
		"If exists, draws attention to major tab 3", EUserInterfaceActionType::Button, TabChords[3]);

	UI_COMMAND_EXT(MainFrameContext.Get(), CommandInfoMajorTabs[4], "MoveToMajorTab4", "Focus Major Tab 4",
		"If exists, draws attention to major tab 4", EUserInterfaceActionType::Button, TabChords[4]);

	UI_COMMAND_EXT(MainFrameContext.Get(), CommandInfoMajorTabs[5], "MoveToMajorTab5", "Focus Major Tab 5",
		"If exists, draws attention to major tab 5", EUserInterfaceActionType::Button, TabChords[5]);

	UI_COMMAND_EXT(MainFrameContext.Get(), CommandInfoMajorTabs[6], "MoveToMajorTab6", "Focus Major Tab 6",
		"If exists, draws attention to major tab 6", EUserInterfaceActionType::Button, TabChords[6]);

	UI_COMMAND_EXT(MainFrameContext.Get(), CommandInfoMajorTabs[7], "MoveToMajorTab7", "Focus Major Tab 7",
		"If exists, draws attention to major tab 7", EUserInterfaceActionType::Button, TabChords[7]);

	UI_COMMAND_EXT(MainFrameContext.Get(), CommandInfoMajorTabs[8], "MoveToMajorTab8", "Focus Major Tab 8",
		"If exists, draws attention to major tab 8", EUserInterfaceActionType::Button, TabChords[8]);

	UI_COMMAND_EXT(MainFrameContext.Get(), CommandInfoMajorTabs[9], "MoveToMajorTab9", "Focus Major Tab 9",
		"If exists, draws attention to major tab 9", EUserInterfaceActionType::Button, TabChords[9]);
}

void FUMTabNavigationManager::AddMinorTabsNavigationCommandsToList(TSharedPtr<FBindingContext> MainFrameContext)
{
	UI_COMMAND_EXT(MainFrameContext.Get(), CommandInfoMinorTabs[0], "MoveToMinorTabLast", "Focus Minor Last Tab",
		"Moves to the last minor tab", EUserInterfaceActionType::Button, TabChords[0]);

	UI_COMMAND_EXT(MainFrameContext.Get(), CommandInfoMinorTabs[1], "MoveToMinorTab1", "Focus Minor Tab 1",
		"If exists, draws attention to minor tab 1", EUserInterfaceActionType::Button, TabChords[1]);

	UI_COMMAND_EXT(MainFrameContext.Get(), CommandInfoMinorTabs[2], "MoveToMinorTab2", "Focus Minor Tab 2",
		"If exists, draws attention to minor tab 2", EUserInterfaceActionType::Button, TabChords[2]);

	UI_COMMAND_EXT(MainFrameContext.Get(), CommandInfoMinorTabs[3], "MoveToMinorTab3", "Focus Minor Tab 3",
		"If exists, draws attention to minor tab 3", EUserInterfaceActionType::Button, TabChords[3]);

	UI_COMMAND_EXT(MainFrameContext.Get(), CommandInfoMinorTabs[4], "MoveToMinorTab4", "Focus Minor Tab 4",
		"If exists, draws attention to minor tab 4", EUserInterfaceActionType::Button, TabChords[4]);

	UI_COMMAND_EXT(MainFrameContext.Get(), CommandInfoMinorTabs[5], "MoveToMinorTab5", "Focus Minor Tab 5",
		"If exists, draws attention to minor tab 5", EUserInterfaceActionType::Button, TabChords[5]);

	UI_COMMAND_EXT(MainFrameContext.Get(), CommandInfoMinorTabs[6], "MoveToMinorTab6", "Focus Minor Tab 6",
		"If exists, draws attention to minor tab 6", EUserInterfaceActionType::Button, TabChords[6]);

	UI_COMMAND_EXT(MainFrameContext.Get(), CommandInfoMinorTabs[7], "MoveToMinorTab7", "Focus Minor Tab 7",
		"If exists, draws attention to minor tab 7", EUserInterfaceActionType::Button, TabChords[7]);

	UI_COMMAND_EXT(MainFrameContext.Get(), CommandInfoMinorTabs[8], "MoveToMinorTab8", "Focus Minor Tab 8",
		"If exists, draws attention to minor tab 8", EUserInterfaceActionType::Button, TabChords[8]);

	UI_COMMAND_EXT(MainFrameContext.Get(), CommandInfoMinorTabs[9], "MoveToMinorTab9", "Focus Minor Tab 9",
		"If exists, draws attention to minor tab 9", EUserInterfaceActionType::Button, TabChords[9]);
}

void FUMTabNavigationManager::SetupFindTabWells(
	TSharedRef<FUICommandList>& CommandList,
	TSharedPtr<FBindingContext> MainFrameContext)
{
	FInputChord FindTabWellsChord(
		EModifierKey::FromBools(true, false, true, false), EKeys::L);
	UI_COMMAND_EXT(
		MainFrameContext.Get(),
		CmdInfoFindAllTabWells,
		"FindAllTabWells",
		"Find all tab wells in the currently active window.",
		"Find all tab wells in the currently active window.", EUserInterfaceActionType::Button, FindTabWellsChord);

	CommandList->MapAction(
		CmdInfoFindAllTabWells,
		FExecuteAction::CreateLambda([this]() {
			FindAllTabWells();
		}));
}

#undef LOCTEXT_NAMESPACE
