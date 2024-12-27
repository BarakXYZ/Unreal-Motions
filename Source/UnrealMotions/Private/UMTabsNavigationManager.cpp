#include "UMTabsNavigationManager.h"

#include "Interfaces/IMainFrameModule.h"
#include "Framework/Commands/UICommandInfo.h"
#include "Widgets/Docking/SDockTab.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
// #include "BlueprintEditor.h"
// #include "Subsystems/AssetEditorSubsystem.h"

#include "UMHelpers.h"
#include "UnrealMotions.h"

DEFINE_LOG_CATEGORY_STATIC(LogUMTabsNavigation, NoLogging, All); // Prod
// DEFINE_LOG_CATEGORY_STATIC(LogUMTabsNavigation, Log, All); // Dev

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

	OnUserMovedToNewTab = FUnrealMotionsModule::GetOnUserMovedToNewTab();

	FUnrealMotionsModule::GetOnUserMovedToNewWindow().AddRaw(
		this, &FUMTabsNavigationManager::HandleOnUserMovedToNewWindow);

	// TODO: Why this doesn't work?
	// FUnrealMotionsModule::GetOnUserMovedToNewWindow().AddSP(
	// 	FUMTabsNavigationManager::TabsNavigationManager,
	// 	&FUMTabsNavigationManager::HandleOnUserMovedToNewWindow);
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
	TSharedRef<FGlobalTabmanager> GTM = FGlobalTabmanager::Get();
	GTM->OnActiveTabChanged_Subscribe(
		FOnActiveTabChanged::FDelegate::CreateRaw(
			this, &FUMTabsNavigationManager::OnActiveTabChanged));

	// This seems to trigger in some cases where OnActiveTabChanged won't.
	// Keeping this as a double check.
	GTM->OnTabForegrounded_Subscribe(
		FOnActiveTabChanged::FDelegate::CreateRaw(
			this, &FUMTabsNavigationManager::OnTabForegrounded));

	FSlateApplication& SlateApp = FSlateApplication::Get();

	SlateApp.OnFocusChanging()
		.AddRaw(this, &FUMTabsNavigationManager::OnFocusChanged);

	// Even more accurate but might be an overkill
	// SlateApp.OnApplicationMousePreInputButtonDownListener()
	// 	.AddRaw(this, &FUMTabsNavigationManager::OnMouseButtonDown);
}

void FUMTabsNavigationManager::OnTabForegrounded(
	TSharedPtr<SDockTab> NewActiveTab, TSharedPtr<SDockTab> PrevActiveTab)
{
	if (!NewActiveTab.IsValid() || !PrevActiveTab.IsValid())
		return;

	DebugTab(NewActiveTab, true, "OnTabForegrounded");
	if (NewActiveTab->GetVisualTabRole() == ETabRole::MajorTab)
	{
		SetCurrentTab(NewActiveTab);

		FString LogNewMajorTab = FString::Printf(
			TEXT("Tab Foregrounded: New Major Tab - Name: %s, ID: %d || Previous Tab - Name: %s, ID: %d"),
			*NewActiveTab->GetTabLabel().ToString(), NewActiveTab->GetId(),
			*PrevActiveTab->GetTabLabel().ToString(), PrevActiveTab->GetId());
		// FUMHelpers::NotifySuccess(
		// 	FText::FromString(LogNewMajorTab), VisualLog);
	}
}

// NOTE: there's an incorrect use of the delegate being called internally by UE.
// It specifies the delegate as firstly the NewActive, then OldTab. But in reality
// calls the OldTab, then NewTab. I wrote more about that in the BugNotes folder,
// but anyways, I've flipped the params to match what's actually being passed.
void FUMTabsNavigationManager::OnActiveTabChanged(
	TSharedPtr<SDockTab> PrevActiveTab, TSharedPtr<SDockTab> NewActiveTab)
{
	if (!NewActiveTab.IsValid() || !PrevActiveTab.IsValid())
		return;

	const FString& LogTabs = FString::Printf(
		TEXT("Active tab changed to: %s Previous Active Tab: %s"),
		*NewActiveTab->GetTabLabel().ToString(),
		*PrevActiveTab->GetTabLabel().ToString());

	UE_LOG(LogUMTabsNavigation, Log, TEXT("%s"), *LogTabs);
	// FUMHelpers::NotifySuccess(FText::FromString(LogTabs), VisualLog);
	DebugTab(NewActiveTab, true, "OnActiveTabChanged");

	SetCurrentTab(NewActiveTab); // Naturally Minor Tab

	if (const TSharedPtr<FTabManager> TabManager =
			NewActiveTab->GetTabManagerPtr())
	{
		if (const TSharedPtr<SDockTab> MajorTab =
				FGlobalTabmanager::Get()->GetMajorTabForTabManager(TabManager.ToSharedRef()))
		{
			SetCurrentTab(MajorTab);
			// Interesting
			// TabManager->GetPrivateApi().GetParentWindow();
		}
	}
}

void FUMTabsNavigationManager::HandleOnUserMovedToNewWindow(
	TWeakPtr<SWindow> NewWindow)
{
	TWeakPtr<SDockTab> NewMajorTab = nullptr;
	if (GetActiveMajorTab(NewMajorTab))
	{
		SetCurrentTab(NewMajorTab.Pin());
		return;
	}
}

void FUMTabsNavigationManager::OnFocusChanged(
	const FFocusEvent& FocusEvent, const FWeakWidgetPath& OldWidgetPath,
	const TSharedPtr<SWidget>& OldWidget, const FWidgetPath& NewWidgetPath,
	const TSharedPtr<SWidget>& NewWidget)
{
}

//** Isn't currently being used *//
void FUMTabsNavigationManager::OnMouseButtonDown(const FPointerEvent& PointerEvent)
{
	FUMHelpers::NotifySuccess(FText::FromString("On Mouse Button Down!"), VisualLog);
	// Set tab...
}

bool FUMTabsNavigationManager::ValidateTargetTab(
	TSharedPtr<SWidget>& OutTab, bool bIsMajorTab)
{
	if (bIsMajorTab && CurrMajorTab.IsValid())
		OutTab = CurrMajorTab.Pin();

	else if (!bIsMajorTab && CurrMinorTab.IsValid())
		OutTab = CurrMinorTab.Pin();
	else
		return false;
	return true;
}

bool FUMTabsNavigationManager::GetParentTabWellFromTab(
	const TSharedPtr<SWidget>& Tab, TSharedPtr<SWidget>& TabWell)
{
	const TSharedPtr<SWidget>& ParentWidget = Tab->GetParentWidget();
	if (!ParentWidget.IsValid() || ParentWidget->GetTypeAsString() != SDTabWell)
		return false;
	else
		TabWell = ParentWidget;
	return true;
}

void FUMTabsNavigationManager::MoveToTabIndex(int32 TabIndex, bool bIsMajorTab)
{
	UE_LOG(LogUMTabsNavigation, Display,
		TEXT("MoveToTabIndex, Tab Index: %d, bIsMajorTab: %d"),
		TabIndex, bIsMajorTab);

	TSharedPtr<SWidget> Tab = nullptr;
	TSharedPtr<SWidget> TabWell = nullptr;
	if (ValidateTargetTab(Tab, bIsMajorTab) && GetParentTabWellFromTab(Tab, TabWell))
		FocusTabIndex(TabWell, TabIndex);

	/**
	 * Deprecated (Not needed with the new method, keeping for reference)
	 * BringToFront is important to mitigate a bug where modal windows
	 * will semi-steal focus from the active window when switching tabs.
	 * Essentially, bring to front helps with solidifying the focus.
	 * */
	// ActiveWindow->BringToFront();
}

void FUMTabsNavigationManager::FocusTabIndex(
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

void FUMTabsNavigationManager::CycleTabs(bool bIsMajorTab, bool bIsNextTab)
{
	TSharedPtr<SWidget> TargetTab = nullptr;
	if (!ValidateTargetTab(TargetTab, bIsMajorTab))
		return;
	TSharedPtr<SWidget> TabWell = nullptr;
	if (!GetParentTabWellFromTab(TargetTab, TabWell))
		return;
	FChildren* Tabs = TabWell->GetChildren();
	if (!Tabs || Tabs->Num() == 0)
		return;

	// TODO: Potentially we may wanna pair Major & Minor tabs relationship so
	// we can determine if we should operate on the minor tabs at all in case
	// the major tab is not the currently active tab (since operating on them
	// mess up the focus situation in other tabs if manipulated)
	// if (bIsMajorTab)
	// {
	// }

	int32		CurrIndex = INDEX_NONE;
	const int32 TNum = Tabs->Num();

	UE_LOG(LogUMTabsNavigation, Display, TEXT("DockTarget Tab Label: %s"),
		*(bIsMajorTab ? CurrMajorTab.Pin() : CurrMinorTab.Pin())->GetTabLabel().ToString());

	for (int32 i{ 0 }; i < TNum; ++i)
	{
		auto Dock = StaticCastSharedRef<SDockTab>(Tabs->GetChildAt(i));
		if (Dock->IsForeground())
		{
			CurrIndex = i;
			UE_LOG(LogUMTabsNavigation, Display,
				TEXT("Current Active Tab Index: %d"), i);
			break;
		}
	}

	if (CurrIndex == INDEX_NONE)
		return;

	CurrIndex =
		bIsNextTab ? (CurrIndex + 1) % TNum : (CurrIndex - 1 + TNum) % TNum;

	TSharedPtr<SDockTab> DockTab = StaticCastSharedRef<SDockTab>(
		Tabs->GetChildAt(CurrIndex));
	if (DockTab.IsValid())
	{
		// This will trigger OnActiveTabChanged if it's a minor tab, and will
		// also trigger OnTabForegrounded for both Major & Minor tabs.
		DockTab->ActivateInParent(ETabActivationCause::SetDirectly);
	}
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

// TODO: Try to refine this method
bool FUMTabsNavigationManager::GetActiveMajorTab(TWeakPtr<SDockTab>& OutMajorTab)
{
	if (!CurrWin.IsValid() || !CurrWin.Pin()->IsActive())
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

bool FUMTabsNavigationManager::FindRootTargetWidgetName(FString& OutRootWidgetName)
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

void FUMTabsNavigationManager::FindAllTabWells()
{
	UE_LOG(LogUMTabsNavigation, Display, TEXT("Find All Tab Wells Started!"));

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
		UE_LOG(LogUMTabsNavigation, Display,
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
					UE_LOG(LogUMTabsNavigation, Display, TEXT("%sTab Name: %s"),
						*FString::ChrN(12, ' '), *Tab->GetTabLabel().ToString());
				}
			}
		}
		++WellNum;
	}
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
	FUMHelpers::NotifySuccess(FText::FromString(DebugMsg), VisualLog, 12.0f);
	// FUMHelpers::AddDebugMessage(
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

void FUMTabsNavigationManager::SetCurrentTab(const TSharedPtr<SDockTab>& NewTab)
{
	if (!NewTab.IsValid())
	{
		UE_LOG(LogUMTabsNavigation, Warning,
			TEXT("Attempted to set invalid tab as current"));
		return;
	}

	if (NewTab->GetVisualTabRole() == ETabRole::MajorTab)
	{
		FString PrevLabel = CurrMajorTab.IsValid() ? CurrMajorTab.Pin()->GetTabLabel().ToString() : TEXT("None");
		UE_LOG(LogUMTabsNavigation, Log,
			TEXT("Set new major tab - Previous: %s, New: %s"),
			*PrevLabel, *NewTab->GetTabLabel().ToString());

		CurrMajorTab = NewTab;
	}
	else // Minor Tab
	{
		FString PrevLabel = CurrMinorTab.IsValid() ? CurrMinorTab.Pin()->GetTabLabel().ToString() : TEXT("None");
		UE_LOG(LogUMTabsNavigation, Log, TEXT("Set new minor tab - Previous: %s, New: %s"),
			*PrevLabel, *NewTab->GetTabLabel().ToString());

		CurrMinorTab = NewTab;
	}
}

bool FUMTabsNavigationManager::TraverseWidgetTree(
	const TSharedPtr<SWidget>& ParentWidget,
	TArray<TWeakPtr<SWidget>>& OutWidgets,
	const FString& TargetType, int32 SearchCount, int32 Depth)
{
	// Log the current widget and depth
	UE_LOG(LogUMTabsNavigation, Display,
		TEXT("%s[Depth: %d] Checking widget: %s"),
		*FString::ChrN(Depth * 2, ' '), // Visual indentation
		Depth, *ParentWidget->GetTypeAsString());

	bool bFoundAllRequested = false;

	if (ParentWidget->GetTypeAsString() == TargetType)
	{
		UE_LOG(LogUMTabsNavigation, Display,
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

bool FUMTabsNavigationManager::TraverseWidgetTree(
	const TSharedPtr<SWidget>& ParentWidget,
	TWeakPtr<SWidget>&		   OutWidget,
	const FString&			   TargetType,
	int32					   Depth)
{
	// Log the current widget and depth
	UE_LOG(LogUMTabsNavigation, Display,
		TEXT("%s[Depth: %d] Checking widget: %s"),
		*FString::ChrN(Depth * 2, ' '), // Visual indentation
		Depth, *ParentWidget->GetTypeAsString());

	if (ParentWidget->GetTypeAsString() == TargetType)
	{
		UE_LOG(LogUMTabsNavigation, Display,
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
				return true;
		}
	}
	return false;
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

void FUMTabsNavigationManager::SetupFindTabWells(
	TSharedRef<FUICommandList>& CommandList,
	TSharedPtr<FBindingContext> MainFrameContext)
{
	FInputChord FindTabWellsChord(
		EModifierKey::FromBools(true, false, true, false), EKeys::L);
	UI_COMMAND_EXT(
		MainFrameContext.Get(),
		CmdInfoFindAllTabWells,
		"FindAllTabWells",
		"Find All Existing Tab Wells",
		"Traverse the widget hierarchy and store all tab wells found.",
		EUserInterfaceActionType::Button, FindTabWellsChord);

	CommandList->MapAction(
		CmdInfoFindAllTabWells,
		FExecuteAction::CreateLambda([this]() {
			FindAllTabWells();
		}));
}

#undef LOCTEXT_NAMESPACE
