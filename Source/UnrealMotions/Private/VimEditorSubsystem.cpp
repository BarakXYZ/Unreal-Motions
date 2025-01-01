#include "VimEditorSubsystem.h"
#include "Brushes/SlateColorBrush.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Templates/SharedPointer.h"
#include "Types/SlateEnums.h"
#include "UMHelpers.h"
#include "UMInputPreProcessor.h"
#include "Widgets/Docking/SDockTab.h"
#include "UMWindowsNavigationManager.h"
#include "UMTabsNavigationManager.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STreeView.h"
#include "Editor/SceneOutliner/Public/SSceneOutliner.h"
// #include "Editor/SceneOutliner/Public/ISceneOutlinerTreeItem.h"
#include "ISceneOutlinerTreeItem.h"
#include "Folder.h"
#include "SceneOutlinerStandaloneTypes.h"
#include "FolderTreeItem.h"

// DEFINE_LOG_CATEGORY_STATIC(LogVimEditorSubsystem, NoLogging, All); // Prod
DEFINE_LOG_CATEGORY_STATIC(LogVimEditorSubsystem, Log, All); // Development

static constexpr int32 MIN_REPEAT_COUNT = 1;
static constexpr int32 MAX_REPEAT_COUNT = 999;

static const TMap<FKey, FKey> VimToArrowKeys = {
	{ EKeys::H, FKey(EKeys::Left) }, { EKeys::J, FKey(EKeys::Down) },
	{ EKeys::K, FKey(EKeys::Up) }, { EKeys::L, FKey(EKeys::Right) }
};

static const TMap<FKey, EUINavigation> VimToUINavigation = {
	{ EKeys::H, EUINavigation::Left }, { EKeys::J, EUINavigation::Down },
	{ EKeys::K, EUINavigation::Up }, { EKeys::L, EUINavigation::Right }
};

static const TSet<FName> ValidWidgetNavigationTypes{
	"SAssetTileView",
	"SSceneOutlinerTreeView",
	"STreeView",
	"SSubobjectEditorDragDropTree"
};

void UVimEditorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	const FConfigFile& ConfigFile = FUMHelpers::ConfigFile;
	FString			   OutLog = "Vim Editor Subsystem Initialized: ";

	FString TestGetStr;
	bool	bStartVim = true;

	if (!ConfigFile.IsEmpty())
	{
		ConfigFile.GetBool(*FUMHelpers::VimSection, TEXT("bStartVim"), bStartVim);

		// TODO: Remove to a more general place? Debug
		ConfigFile.GetBool(*FUMHelpers::DebugSection, TEXT("bVisualLog"), bVisualLog);
	}

	ToggleVim(bStartVim);
	OutLog += bStartVim ? "Enabled by Config." : "Disabled by Config.";
	FUMHelpers::NotifySuccess(FText::FromString(OutLog), bVisualLog);

	VimSubWeak = this;
	InputPP = FUMInputPreProcessor::Get().ToWeakPtr();
	BindCommands();
	Super::Initialize(Collection);
}

void UVimEditorSubsystem::Deinitialize()
{
	ToggleVim(false);
	Super::Deinitialize();
}

void UVimEditorSubsystem::OnResetSequence()
{
	CountBuffer.Empty();
}
void UVimEditorSubsystem::OnCountPrefix(FString AddedCount)
{
	// Building the buffer -> "1" + "7" + "3" == "173"
	CountBuffer += AddedCount;
	// FUMHelpers::NotifySuccess(FText::FromString(
	// 	FString::Printf(TEXT("On Count Prefix: %s"), *CountBuffer)));
}

void UVimEditorSubsystem::UpdateTreeViewSelectionOnExitVisualMode(
	FSlateApplication& SlateApp)
{
	TSharedPtr<SListView<TSharedPtr<ISceneOutlinerTreeItem>>> ListView = nullptr;
	if (!GetListView(SlateApp, ListView))
		return;

	const auto SelItems = ListView->GetSelectedItems();
	if (SelItems.IsEmpty())
		return;

	// Check if we should set selection on the first or last item
	// in the selected item array. Using the nav offset, we can deduce the correct
	// selection (x <= 0 first, else last)
	TSharedPtr<ISceneOutlinerTreeItem> CurrItem =
		VisualNavOffsetIndicator <= 0 ? SelItems[0] : SelItems.Last();
	ListView->ClearSelection();
	ListView->SetItemSelection(CurrItem, true,
		ESelectInfo::OnNavigation); // OnNavigation is important here to clear
									// the selection array more deeply (it seems)
}

void UVimEditorSubsystem::OnVimModeChanged(const EVimMode NewVimMode)
{
	CurrentVimMode = NewVimMode;
	FSlateApplication& SlateApp = FSlateApplication::Get();

	switch (CurrentVimMode)
	{
		case EVimMode::Normal:
			UpdateTreeViewSelectionOnExitVisualMode(SlateApp);
			break;

		case EVimMode::Insert:
			break;

		case EVimMode::Visual:
			VisualNavOffsetIndicator = 0;
			CaptureFirstTreeViewItemSelectionAndIndex(SlateApp);
			break;
	}
}

void UVimEditorSubsystem::CaptureFirstTreeViewItemSelectionAndIndex(
	FSlateApplication& SlateApp)
{
	TSharedPtr<SListView<TSharedPtr<ISceneOutlinerTreeItem>>> ListView;
	if (GetListView(SlateApp, ListView))
	{
		const auto& SelItems = ListView->GetSelectedItems();
		if (SelItems.IsEmpty())
			return;

		int32		ItemIndex = INDEX_NONE;
		int32		Cursor{ 0 };
		const auto& FirstSelItem = SelItems[0];

		for (const auto& Item : SelItems)
		{
			if (Item == FirstSelItem)
			{
				ItemIndex = Cursor;
				break;
			}
			++Cursor;
		}

		if (ItemIndex == INDEX_NONE)
			return;

		FirstTreeViewItem.Item = FirstSelItem.ToWeakPtr();
		FirstTreeViewItem.Index = ItemIndex;
	}
}

void UVimEditorSubsystem::ToggleVim(bool bEnable)
{
	FString OutLog = bEnable ? "Start Vim: " : "Stop Vim: ";

	if (FSlateApplication::IsInitialized())
	{
		if (bEnable)
		{
			// TODO:
		}
		else
		{
			if (PreInputKeyDownDelegateHandle.IsValid())
			{
				FSlateApplication::Get().OnApplicationPreInputKeyDownListener().Remove(PreInputKeyDownDelegateHandle);
				PreInputKeyDownDelegateHandle.Reset(); // Clear the handle to avoid reuse
				OutLog += "Delegate resetted successfully.";
			}
			else
				OutLog += "Delegate wasn't valid / bound. Skipping.";
		}
	}

	FUMHelpers::NotifySuccess(FText::FromString(OutLog), bVisualLog);
}

bool UVimEditorSubsystem::MapVimToArrowNavigation(
	const FKeyEvent& InKeyEvent, FKeyEvent& OutKeyEvent)
{
	const FKey* MappedKey = VimToArrowKeys.Find(InKeyEvent.GetKey());
	if (!MappedKey)
		return false;
	OutKeyEvent = FKeyEvent(
		*MappedKey,
		InKeyEvent.GetModifierKeys(),
		0,	   // User index
		false, // Is repeat
		0,	   // Character code
		0	   // Key code
	);
	return true;
}

bool UVimEditorSubsystem::MapVimToNavigationEvent(const FKeyEvent& InKeyEvent,
	FNavigationEvent& OutNavigationEvent, bool bIsShiftDown)
{
	const FModifierKeysState ModKeysShift(bIsShiftDown, bIsShiftDown,
		false, false, false, false, false, false, false);

	const EUINavigation* NavigationType =
		VimToUINavigation.Find(InKeyEvent.GetKey());

	if (!NavigationType)
		return false;

	FNavigationEvent NavEvent(ModKeysShift, 0, *NavigationType,
		ENavigationGenesis::Keyboard);
	OutNavigationEvent = NavEvent;
	return true;
}

int32 UVimEditorSubsystem::FindWidgetIndexInParent(
	const TSharedRef<SWidget>& Widget)
{
	TSharedPtr<SWidget> ParentWidget = Widget->GetParentWidget();
	if (!ParentWidget.IsValid())
	{
		return INDEX_NONE; // No parent found
	}

	TSharedPtr<SPanel> ParentPanel = StaticCastSharedPtr<SPanel>(ParentWidget);
	if (!ParentPanel.IsValid())
	{
		return INDEX_NONE; // Parent is not a panel
	}

	int32 Index = 0;
	int32 FoundIndex = INDEX_NONE;

	ParentPanel->GetChildren()->ForEachWidget([&](SWidget& ChildWidget) {
		if (&ChildWidget == &Widget.Get())
		{
			FoundIndex = Index;
			return false; // Stop iteration
		}
		++Index;
		return true; // Continue iteration
	});

	return FoundIndex;
}

bool UVimEditorSubsystem::GetListView(FSlateApplication& SlateApp, TSharedPtr<SListView<TSharedPtr<ISceneOutlinerTreeItem>>>& OutListView)
{
	TSharedPtr<SWidget> FocusedWidget = SlateApp.GetUserFocusedWidget(0);
	if (!FocusedWidget.IsValid())
	{
		FUMHelpers::NotifySuccess(
			FText::FromString("Focused widget NOT valid"), bVisualLog);
		return false;
	}

	if (!ValidWidgetNavigationTypes.Find(FocusedWidget->GetType())
		&& !ValidWidgetNavigationTypes.Find(
			FName(FocusedWidget->GetTypeAsString().Left(9))))
	{
		FUMHelpers::NotifySuccess(
			FText::FromString(FString::Printf(TEXT("Not a valid type: %s"), *FocusedWidget->GetTypeAsString())), bVisualLog);
		return false;
	}

	OutListView = StaticCastSharedPtr<SListView<
		TSharedPtr<ISceneOutlinerTreeItem>>>(FocusedWidget);
	if (!OutListView.IsValid())
	{
		FUMHelpers::NotifySuccess(FText::FromString("Not a ListView"), bVisualLog);
		return false;
	}
	return true;
}

void UVimEditorSubsystem::NavigateToFirstOrLastItem(
	FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	TSharedPtr<SListView<TSharedPtr<ISceneOutlinerTreeItem>>> ListView = nullptr;
	if (!GetListView(SlateApp, ListView))
		return;

	TArrayView<const TSharedPtr<ISceneOutlinerTreeItem, ESPMode::ThreadSafe>>
		ListItems = ListView->GetItems();
	if (ListItems.IsEmpty())
		return;

	bool		bIsShiftDown = InKeyEvent.IsShiftDown();
	const auto& GotoItem =
		bIsShiftDown ? ListItems.Last()
					 : ListItems[0];

	if (CurrentVimMode == EVimMode::Normal)
		ListView->SetSelection(GotoItem, ESelectInfo::OnNavigation);
	else // Has to be Visual Mode
	{
		const auto& SelItems = ListView->GetSelectedItems();
		if (SelItems.IsEmpty())
			return; // I think

		// Going from where we are to the beginning (gg) or end (G), we will
		// consider the first item selected as the new first item (gg) or if going
		// to the end (G), we will want the last item selected.
		// NOTE: Not sure if this is actually stable!
		const auto& NewFirstSelItem =
			// bIsShiftDown ? SelItems.Last() : SelItems[0];
			bIsShiftDown ? SelItems[0] : SelItems.Last();

		// Crash
		// FUMHelpers::NotifySuccess(FText::FromString(FString::Printf(TEXT("Last: %s, First: %s"), *SelItems.Last()->GetDisplayString(), *SelItems[0]->GetDisplayString())));
		// return;

		// Check what's the index of that item, so we can calculate the delta
		// distance we gonna execute on either gg/G (i.e. go to beginning or end)
		int32 NewFirstSelItemIndex = INDEX_NONE;
		int32 Cursor{ 1 }; // It make more sense to calculate in index 1
		for (const auto& Item : ListItems)
		{
			// A way to know if something is a folder in outliner potentially!
			// if (ListView->Private_IsItemSelectableOrNavigable(Item))
			// {
			if (Item == NewFirstSelItem)
			{
				NewFirstSelItemIndex = Cursor;
				break;
			}
			++Cursor;
			// }
		}
		if (NewFirstSelItemIndex == INDEX_NONE)
			return; // Item was not found

		ListView->ClearSelection(); // ??
		VisualNavOffsetIndicator = 0;

		// if (!FirstTreeViewItem.Item.IsValid())
		// 	CaptureFirstTreeViewItemSelectionAndIndex(SlateApp);

		int32 LoopIndex = bIsShiftDown
			? ListItems.Num() - NewFirstSelItemIndex
			: NewFirstSelItemIndex - 1;

		for (int32 i{ 0 }; i < LoopIndex; ++i)
		{
			// if (ListView->Private_IsItemSelectableOrNavigable(Item))
			// {
			ProcessVimNavigationInput(SlateApp,
				FUMInputPreProcessor::GetKeyEventFromKey(
					// FKey(bIsShiftDown ? FKey(EKeys::J) : FKey(EKeys::K)),
					// TODO: Determine if View is Vertical or Horizontal
					FKey(bIsShiftDown ? FKey(EKeys::L) : FKey(EKeys::H)),
					true));
			// }
		}
		return;

		int32 NewNavOffsetIndex = bIsShiftDown
			? ListItems.Num() - NewFirstSelItemIndex
			: 1 - NewFirstSelItemIndex;
		VisualNavOffsetIndicator = NewNavOffsetIndex;

		// UpdateTreeViewSelectionOnExitVisualMode(SlateApp);
		ListView->ClearSelection();
		// ListView->SetItemSelection(
		// 	NewFirstSelItem, true, ESelectInfo::OnNavigation);
		// ListView->Private_SelectRangeFromCurrentTo(GotoItem);
		// for (const auto& Item : ListItems)
		// {
		// 	if (ListView->Private_IsItemSelectableOrNavigable(Item))
		// 	{
		// 		ProcessVimNavigationInput(SlateApp,
		// 			FUMInputPreProcessor::GetKeyEventFromKey(
		// 				FKey(bIsShiftDown ? FKey(EKeys::J) : FKey(EKeys::K)),
		// 				true));
		// 		ListView->SetItemSelection(
		// 			Item,
		// 			(bIsShiftDown
		// 					? Cursor >= NewFirstSelItemIndex
		// 					: Cursor <= NewFirstSelItemIndex),
		// 			ESelectInfo::OnNavigation);
		// 	}
		// }
		return;
	}

	ListView->RequestNavigateToItem(GotoItem, 0);
}

void UVimEditorSubsystem::GetCurrentTreeItemIndex(FSlateApplication& SlateApp,
	const TSharedPtr<SListView<TSharedPtr<ISceneOutlinerTreeItem>>>& ListView,
	const TSharedPtr<ISceneOutlinerTreeItem>&						 CurrItem)
{
	ListView->GetItems();
}

void UVimEditorSubsystem::TrackVisualOffsetNavigation(const FKeyEvent& InKeyEvent)
{
	const static TMap<FKey, int32> OffsetIndexByVimNavigation = {
		{ FKey(EKeys::H), -1 },
		{ FKey(EKeys::J), 1 },
		{ FKey(EKeys::K), -1 },
		{ FKey(EKeys::L), 1 }
	};

	LastNavigationDirection = InKeyEvent; // For future reference
	const FKey	 PressedKey = InKeyEvent.GetKey();
	const int32* OffsetIndex = OffsetIndexByVimNavigation.Find(PressedKey);
	if (!OffsetIndex)
		return;

	VisualNavOffsetIndicator += *OffsetIndex;
	// FUMHelpers::NotifySuccess(
	// 	FText::FromString(FString::Printf(TEXT("Visual Nav Offset: %d"), VisualNavOffsetIndicator)), bVisualLog);
}

void UVimEditorSubsystem::ProcessVimNavigationInput(
	FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	TSharedPtr<SListView<TSharedPtr<ISceneOutlinerTreeItem>>> ListView = nullptr;
	if (CurrentVimMode == EVimMode::Visual)
	{
		if (!GetListView(SlateApp, ListView))
			return;

		TrackVisualOffsetNavigation(InKeyEvent);

		FNavigationEvent NavEvent;
		MapVimToNavigationEvent(InKeyEvent, NavEvent, true /** Shift Down */);

		ListView->OnNavigation(
			SlateApp.GetUserFocusedWidget(0)->GetCachedGeometry(), NavEvent);
		return;
	}

	FKeyEvent OutKeyEvent;
	MapVimToArrowNavigation(InKeyEvent, OutKeyEvent);

	int32 Count = MIN_REPEAT_COUNT;
	if (!CountBuffer.IsEmpty())
	{
		Count = FMath::Clamp(
			FCString::Atoi(*CountBuffer),
			MIN_REPEAT_COUNT,
			MAX_REPEAT_COUNT);
	}
	// else
	// FUMHelpers::NotifySuccess(FText::FromString("Empty Buffer"));

	// FUMHelpers::NotifySuccess(
	// 	FText::FromString(FString::Printf(TEXT("Count: %d"), Count)));
	for (int32 i{ 0 }; i < Count; ++i)
	{
		FUMInputPreProcessor::ToggleNativeInputHandling(true);
		SlateApp.ProcessKeyDownEvent(OutKeyEvent);
	}
}

void UVimEditorSubsystem::Undo(
	FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	// FUMHelpers::NotifySuccess(FText::FromString("UNDO"));
	FUMInputPreProcessor::SimulateKeyPress(SlateApp,
		FKey(EKeys::Z),
		FModifierKeysState(
			false, false,
			true, true, // Only Ctrl down (Ctrl+Z) TODO: Check on MacOS
			false, false, false, false, false));
}

void UVimEditorSubsystem::DebugTreeItem(
	const TSharedPtr<ISceneOutlinerTreeItem, ESPMode::ThreadSafe>& TreeItem, int32 Index)
{
	FUMHelpers::NotifySuccess(
		FText::FromString(FString::Printf(
			TEXT("Move to NewItem: %s, CanInteract: %s, Index: %d"),
			*TreeItem->GetDisplayString(),
			TreeItem->CanInteract() ? TEXT("true") : TEXT("false"),
			Index)));
}
void UVimEditorSubsystem::DeleteItem(
	FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	// EKeys::Delete;
	FKeyEvent DeleteEvent(
		FKey(EKeys::Delete),
		FModifierKeysState(),
		0, 0, 0, 0);
	FUMInputPreProcessor::ToggleNativeInputHandling(true);
	// SlateApp.ProcessKeyDownEvent(DeleteEvent);
	SlateApp.ProcessKeyDownEvent(DeleteEvent); // Will just block the entire process until the delete window is handled, thus not really helping.
}

void UVimEditorSubsystem::OpenWidgetReflector(
	FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	static const FName REFLECTOR = "WidgetReflector";

	TSharedPtr<SDockTab> RefTab = // Invoke the Widget Reflector tab
		FGlobalTabmanager::Get()->TryInvokeTab(FTabId(REFLECTOR));
	ActivateNewInvokedTab(FSlateApplication::Get(), RefTab); // For proper focus

	// FGlobalTabmanager::Get()->GetTabManagerForMajorTab(RefTab)->GetPrivateApi().GetLiveDockAreas();
	TSharedPtr<SWindow> RefWin = RefTab->GetParentWindow();
	if (!RefWin)
		return;

	// Fetching the first (maybe only) instance of this (parent of our target)
	TWeakPtr<SWidget> FoundWidget;
	if (!FUMTabsNavigationManager::TraverseWidgetTree(
			RefWin, FoundWidget, "SToolBarButtonBlock"))
		return;

	// Locate the button within it
	if (FUMTabsNavigationManager::TraverseWidgetTree(
			FoundWidget.Pin(), FoundWidget, "SButton"))
	{
		TSharedPtr<SButton> Button =
			StaticCastSharedPtr<SButton>(FoundWidget.Pin());
		if (!Button)
			return;
		Button->SimulateClick(); // Will spawn a fetchable Menu Box (&& SWindow)

		// Get this window so we can search for the final buttons within it
		TArray<TSharedRef<SWindow>> ChildRefWins = RefWin->GetChildWindows();
		if (ChildRefWins.Num() == 0)
			return;

		// Remove the annoying SearchBox that is taking focus from HJKL
		if (FUMTabsNavigationManager::TraverseWidgetTree(
				ChildRefWins[0], FoundWidget, "SSearchBox"))
		{
			// Won't work
			// TSharedPtr<SHorizontalBox> ParentBox =
			// 	StaticCastSharedPtr<SHorizontalBox>(
			// 		FoundWidget.Pin()->GetParentWidget());
			// if (ParentBox)
			// {
			// 	// ParentBox->RemoveSlot(FoundWidget.Pin().ToSharedRef());
			// 	// ParentBox->SetEnabled(false);
			// }

			// Works but still catch 1 key!
			TSharedPtr<SSearchBox> SearchBox =
				StaticCastSharedPtr<SSearchBox>(FoundWidget.Pin());

			// Maybe can try to clear the input from a reference?
			// The problem is that it gets the characters from somewhere else
			// the actual handling does work but the first key will always be
			// received because it's being sent from outside.
			// Need to understand from where it's getting the input.

			SearchBox->SetOnKeyCharHandler(
				FOnKeyChar::CreateUObject(
					this, &UVimEditorSubsystem::HandleDummyKeyChar));

			SearchBox->SetOnKeyDownHandler(
				FOnKeyDown::CreateUObject(
					this, &UVimEditorSubsystem::HandleDummyKeyDown));
		}

		// Find all menu entry buttons in the found window
		TArray<TWeakPtr<SWidget>> FoundButtons;
		if (!FUMTabsNavigationManager::TraverseWidgetTree(
				ChildRefWins[0], FoundButtons, "SMenuEntryButton"))
			return;

		if (FoundButtons.Num() == 0 || !FoundButtons[1].IsValid())
			return;
		TSharedPtr<SButton> EntryButton = // SMenuEntry inherits from SButton
			StaticCastSharedPtr<SButton>(FoundButtons[1].Pin());
		if (!EntryButton.IsValid())
			return;

		bool bIsImmediatelyTriggered = InKeyEvent.IsShiftDown();

		// We need a slight delay before drawing focus to this new button
		FTimerHandle TimerHandle;
		GEditor->GetTimerManager()->SetTimer(
			TimerHandle,
			[EntryButton, bIsImmediatelyTriggered]() {
				FSlateApplication& SlateApp = FSlateApplication::Get();
				SlateApp.ClearAllUserFocus();
				if (bIsImmediatelyTriggered)
					EntryButton->SimulateClick();
				// TODO: This is fragile - let's add a manual focus visualizer
				SlateApp.SetAllUserFocus(EntryButton, EFocusCause::Navigation);
			},
			0.01f, // 10ms delay seems to be enough (need to check on mac)
			false  // Do not loop
		);
		FUMHelpers::NotifySuccess(FText::AsNumber(FoundButtons.Num()));
	}
}

void UVimEditorSubsystem::OpenOutputLog()
{
	static const FName LOG = "OutputLog";
	ActivateNewInvokedTab(FSlateApplication::Get(),
		FGlobalTabmanager::Get()->TryInvokeTab(FTabId(LOG)));
}

void UVimEditorSubsystem::OpenContentBrowser(
	FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	static FString DEFAULT_TAB_INDEX = "1";
	FString		   ContentBrowserId = "ContentBrowserTab";
	FString		   TabNum;
	if (FUMInputPreProcessor::GetStrDigitFromKey(
			InKeyEvent.GetKey(), TabNum, 1, 4)) // Valid tabs are only 1-4
		ContentBrowserId += TabNum;
	else
		ContentBrowserId += DEFAULT_TAB_INDEX;

	ActivateNewInvokedTab(SlateApp,
		FGlobalTabmanager::Get()->TryInvokeTab(FName(*ContentBrowserId)));
}

void UVimEditorSubsystem::ActivateNewInvokedTab(
	FSlateApplication& SlateApp, const TSharedPtr<SDockTab> NewTab)
{
	SlateApp.ClearAllUserFocus(); // NOTE: In order to actually draw focus

	if (TSharedPtr<SWindow> Win = NewTab->GetParentWindow())
		FUMWindowsNavigationManager::ActivateWindow(Win.ToSharedRef());
	NewTab->ActivateInParent(ETabActivationCause::SetDirectly);
}

void UVimEditorSubsystem::RemoveActiveMajorTab()
{
	if (!FUMTabsNavigationManager::RemoveActiveMajorTab())
		return;
	FUMWindowsNavigationManager::FocusNextFrontmostWindow();
}

void UVimEditorSubsystem::BindCommands()
{
	using VimSub = UVimEditorSubsystem;

	if (!InputPP.IsValid())
		return;
	FUMInputPreProcessor& Input = *InputPP.Pin();

	// Listeners
	Input.OnResetSequence.AddUObject(
		this, &VimSub::OnResetSequence);

	Input.OnCountPrefix.AddUObject(
		this, &VimSub::OnCountPrefix);

	Input.OnVimModeChanged.AddUObject(
		this, &VimSub::OnVimModeChanged);

	// ~ Commands ~ //
	//
	// Delete item - Simulate the Delete key (WIP)
	Input.AddKeyBinding_KeyEvent(
		{ EKeys::X },
		VimSubWeak, &VimSub::DeleteItem);

	Input.AddKeyBinding_KeyEvent(
		{ EKeys::SpaceBar, EKeys::O, EKeys::W },
		VimSubWeak, &VimSub::OpenWidgetReflector);

	Input.AddKeyBinding_KeyEvent(
		{ EKeys::SpaceBar, EKeys::O, FInputChord(EModifierKey::Shift, EKeys::W) },
		VimSubWeak, &VimSub::OpenWidgetReflector);

	Input.AddKeyBinding_NoParam(
		{ EKeys::SpaceBar, EKeys::O, EKeys::O },
		VimSubWeak, &VimSub::OpenOutputLog);

	//** Open Content Browsers 1-4 */
	Input.AddKeyBinding_KeyEvent(
		{ EKeys::SpaceBar, EKeys::O, EKeys::C, EKeys::One },
		VimSubWeak, &VimSub::OpenContentBrowser);
	Input.AddKeyBinding_KeyEvent(
		{ EKeys::SpaceBar, EKeys::O, EKeys::C, EKeys::Two },
		VimSubWeak, &VimSub::OpenContentBrowser);
	Input.AddKeyBinding_KeyEvent(
		{ EKeys::SpaceBar, EKeys::O, EKeys::C, EKeys::Three },
		VimSubWeak, &VimSub::OpenContentBrowser);
	Input.AddKeyBinding_KeyEvent(
		{ EKeys::SpaceBar, EKeys::O, EKeys::C, EKeys::Four },
		VimSubWeak, &VimSub::OpenContentBrowser);

	Input.AddKeyBinding_NoParam(
		{ FInputChord(EModifierKey::Control, EKeys::W) },
		VimSubWeak, &VimSub::RemoveActiveMajorTab);

	//  Move HJKL
	Input.AddKeyBinding_KeyEvent(
		{ EKeys::H },
		VimSubWeak, &VimSub::ProcessVimNavigationInput);
	Input.AddKeyBinding_KeyEvent(
		{ EKeys::J },
		VimSubWeak, &VimSub::ProcessVimNavigationInput);
	Input.AddKeyBinding_KeyEvent(
		{ EKeys::K },
		VimSubWeak, &VimSub::ProcessVimNavigationInput);
	Input.AddKeyBinding_KeyEvent(
		{ EKeys::L },
		VimSubWeak, &VimSub::ProcessVimNavigationInput);

	// Selection + Move HJKL
	Input.AddKeyBinding_KeyEvent(
		{ FInputChord(EModifierKey::Shift, EKeys::H) },
		VimSubWeak, &VimSub::ProcessVimNavigationInput);
	Input.AddKeyBinding_KeyEvent(
		{ FInputChord(EModifierKey::Shift, EKeys::J) },
		VimSubWeak, &VimSub::ProcessVimNavigationInput);
	Input.AddKeyBinding_KeyEvent(
		{ FInputChord(EModifierKey::Shift, EKeys::K) },
		VimSubWeak, &VimSub::ProcessVimNavigationInput);
	Input.AddKeyBinding_KeyEvent(
		{ FInputChord(EModifierKey::Shift, EKeys::L) },
		VimSubWeak, &VimSub::ProcessVimNavigationInput);

	Input.AddKeyBinding_KeyEvent(
		{ FInputChord(EModifierKey::Shift, EKeys::G) },
		VimSubWeak, &VimSub::NavigateToFirstOrLastItem);
	Input.AddKeyBinding_KeyEvent(
		{ EKeys::G, EKeys::G },
		VimSubWeak, &VimSub::NavigateToFirstOrLastItem);

	Input.AddKeyBinding_KeyEvent(
		{ EKeys::U },
		VimSubWeak, &VimSub::Undo);
}
