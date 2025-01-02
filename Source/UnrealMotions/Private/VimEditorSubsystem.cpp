#include "VimEditorSubsystem.h"
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
#include "Widgets/Views/STreeView.h"
#include "Editor/SceneOutliner/Public/SSceneOutliner.h"
#include "ISceneOutlinerTreeItem.h"

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
	if (SelItems.IsEmpty() || !AnchorTreeViewItem.Item.IsValid())
		return;

	// Commented out a deprecated method for deducing the new sel item.
	// I've realized the simpler method afterwards, keeping it for reference.
	TSharedPtr<ISceneOutlinerTreeItem> CurrItem =
		// VisualNavOffsetIndicator <= 0 ? SelItems[0] : SelItems.Last();
		AnchorTreeViewItem.Item == SelItems[0] ? SelItems.Last() : SelItems[0];
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
			CaptureAnchorTreeViewItemSelectionAndIndex(SlateApp);
			break;
	}
}

void UVimEditorSubsystem::CaptureAnchorTreeViewItemSelectionAndIndex(
	FSlateApplication& SlateApp)
{
	TSharedPtr<SListView<TSharedPtr<ISceneOutlinerTreeItem>>> ListView;
	if (GetListView(SlateApp, ListView))
	{
		const auto& SelItems = ListView->GetSelectedItems();
		if (SelItems.IsEmpty())
			return;

		const auto& FirstSelItem = SelItems[0];
		if (SelItems.Num() > 1)
		{
			ListView->ClearSelection();
			ListView->SetItemSelection(FirstSelItem,
				true, ESelectInfo::OnNavigation);
		}

		int32 ItemIndex = INDEX_NONE;
		int32 Cursor{ 0 };

		for (const auto& Item : ListView->GetItems())
		{
			if (Item == FirstSelItem)
			{
				ItemIndex = Cursor;
				break;
			}
			++Cursor;
		}

		if (ItemIndex == INDEX_NONE)
		{
			AnchorTreeViewItem.Item = nullptr;
			AnchorTreeViewItem.Index = -1;
		}

		AnchorTreeViewItem.Item = FirstSelItem.ToWeakPtr();
		AnchorTreeViewItem.Index = ItemIndex;
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
	TSharedPtr<SListView<TSharedPtr<ISceneOutlinerTreeItem>>> ListView;
	if (!GetListView(SlateApp, ListView))
		return;

	TArrayView<const TSharedPtr<ISceneOutlinerTreeItem, ESPMode::ThreadSafe>>
		AllItems = ListView->GetItems();
	if (AllItems.IsEmpty())
		return;

	// if G, our goto item will be the last one, if gg, first
	bool bIsShiftDown = InKeyEvent.IsShiftDown();

	if (CurrentVimMode == EVimMode::Normal)
	{
		HandleNormalNavToFirstOrLast(ListView, AllItems, bIsShiftDown);
	}
	else // Has to be Visual Mode
	{
		HandleVisualNavToFirstOrLast(SlateApp, ListView, AllItems, bIsShiftDown);
	}
}

void UVimEditorSubsystem::HandleNormalNavToFirstOrLast(
	TSharedPtr<SListView<TSharedPtr<ISceneOutlinerTreeItem>>>& ListView,
	TArrayView<const TSharedPtr<ISceneOutlinerTreeItem>>&	   AllItems,
	bool													   bIsShiftDown)
{
	const auto& GotoItem =
		bIsShiftDown ? AllItems.Last()
					 : AllItems[0];

	ListView->SetSelection(GotoItem, ESelectInfo::OnNavigation);
	ListView->RequestNavigateToItem(GotoItem, 0); // Does that work solo?
}

void UVimEditorSubsystem::HandleVisualNavToFirstOrLast(
	FSlateApplication&										   SlateApp,
	TSharedPtr<SListView<TSharedPtr<ISceneOutlinerTreeItem>>>& ListView,
	TArrayView<const TSharedPtr<ISceneOutlinerTreeItem>>&	   AllItems,
	bool													   bIsShiftDown)
{
	const auto& SelItems = ListView->GetSelectedItems();
	if (SelItems.IsEmpty())
		return; // I think

	if (!AnchorTreeViewItem.Item.IsValid())
	{
		CaptureAnchorTreeViewItemSelectionAndIndex(SlateApp);
		FUMHelpers::NotifySuccess(FText::FromString(
			"Capture Anchor was NOT valid. Now capturing..."));
	}

	FKey NavKeyDirection = GetTreeNavigationDirection(ListView, bIsShiftDown);

	int32	   TimesToNavigate{ 0 };
	const auto IsAnchorToTheLeft = AnchorTreeViewItem.Item == SelItems[0];
	if (bIsShiftDown) // Go to the end (G), go to the last index
	{
		TimesToNavigate = IsAnchorToTheLeft
			? AllItems.Num() - (AnchorTreeViewItem.Index + 1) - (SelItems.Num() - 1)
			: AllItems.Num() - (AnchorTreeViewItem.Index + 1) + (SelItems.Num() - 1);
	}
	else // Go to the start (gg)
	{
		// if 0, we are currently to the right of the selection (or at 0)
		// else we are to the left (or at 0)
		TimesToNavigate = IsAnchorToTheLeft
			? AnchorTreeViewItem.Index + (SelItems.Num() - 1)
			: (AnchorTreeViewItem.Index + 1) - SelItems.Num();
	}

	int32 DebugLoopNum{ 0 };
	for (int32 i{ 0 }; i < TimesToNavigate; ++i)
	{
		ProcessVimNavigationInput(SlateApp,
			FUMInputPreProcessor::GetKeyEventFromKey(
				NavKeyDirection,
				true));
		++DebugLoopNum;
	}
	// FUMHelpers::NotifySuccess(FText::FromString(
	// 	FString::Printf(TEXT("Times Navigated: %d, Anchor Index: %d"),
	// 		DebugLoopNum, AnchorTreeViewItem.Index)));
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
	if (!GetListView(SlateApp, ListView))
		return;

	// In Visual Mode, we want a constant simulation of Shift down for proper
	// range selection. Else, we will just directly navigate to items.
	const bool bShouldSimulateShiftDown{ CurrentVimMode == EVimMode::Visual };

	FNavigationEvent NavEvent;
	MapVimToNavigationEvent(InKeyEvent, NavEvent, bShouldSimulateShiftDown);

	// Normal
	// FKeyEvent OutKeyEvent;
	// MapVimToArrowNavigation(InKeyEvent, OutKeyEvent);

	int32 Count = MIN_REPEAT_COUNT;
	if (!CountBuffer.IsEmpty())
	{
		Count = FMath::Clamp(
			FCString::Atoi(*CountBuffer),
			MIN_REPEAT_COUNT,
			MAX_REPEAT_COUNT);
	}

	for (int32 i{ 0 }; i < Count; ++i)
	{
		const FNavigationReply NavReply =
			ListView->OnNavigation( // Navigate to the next or previous item
				SlateApp.GetUserFocusedWidget(0)->GetCachedGeometry(), NavEvent);

		if (NavReply.GetBoundaryRule() != EUINavigationRule::Escape)
			TrackVisualOffsetNavigation(InKeyEvent); // Only track if moving

		// FUMInputPreProcessor::ToggleNativeInputHandling(true);
		// SlateApp.ProcessKeyDownEvent(OutKeyEvent);
	}
}

bool UVimEditorSubsystem::IsTreeViewVertical(
	const TSharedPtr<SListView<TSharedPtr<ISceneOutlinerTreeItem>>>& ListView)
{
	const static FName AssetTileView = "SAssetTileView";

	if (ListView.IsValid())
		if (!ListView->GetType().IsEqual(AssetTileView))
			return true;
	return false;
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

void UVimEditorSubsystem::Enter(
	FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	static const FName		  ButtonType{ "SButton" };
	const TSharedPtr<SWidget> FocusedWidget = SlateApp.GetUserFocusedWidget(0);
	if (!FocusedWidget || !FocusedWidget->GetType().IsEqual(ButtonType))
	{
		FUMHelpers::NotifySuccess(
			FText::FromString(FString::Printf(TEXT("Not %s: %s"),
				*ButtonType.ToString(), *FocusedWidget->GetTypeAsString())),
			bVisualLog);
		FUMInputPreProcessor::SimulateKeyPress(SlateApp,
			FKey(EKeys::Enter),
			FModifierKeysState());
		return;
	}
	TSharedPtr<SButton> FocusedWidgetAsButton =
		StaticCastSharedPtr<SButton>(FocusedWidget);
	FocusedWidgetAsButton->SimulateClick();
}

void UVimEditorSubsystem::NavigateNextPrevious(
	FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	const auto& FocusedWidget = SlateApp.GetKeyboardFocusedWidget();
	if (!FocusedWidget.IsValid())
		return;

	FWidgetPath CurrentPath;
	if (!SlateApp.FindPathToWidget(FocusedWidget.ToSharedRef(), CurrentPath))
		return;

	// N = Next (aka Tab) | P = Previous (aka Shift + Tab)
	const bool	 bIsForwardNavigation = InKeyEvent.GetKey() == EKeys::N;
	const FReply NavigationReply =
		FReply::Handled().SetNavigation(
			bIsForwardNavigation ? EUINavigation::Next : EUINavigation::Previous,
			ENavigationGenesis::Keyboard,
			ENavigationSource::FocusedWidget);

	FSlateApplication::Get().ProcessReply(CurrentPath, NavigationReply,
		nullptr, nullptr);
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
	// TODO: Delegate to switch Vim mode to Normal

	FUMInputPreProcessor::OnRequestVimModeChange.Broadcast(SlateApp, EVimMode::Normal);
	FKeyEvent DeleteEvent(
		FKey(EKeys::Delete),
		FModifierKeysState(),
		0, 0, 0, 0);
	FUMInputPreProcessor::ToggleNativeInputHandling(true);
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

FKey UVimEditorSubsystem::GetTreeNavigationDirection(
	const TSharedPtr<SListView<TSharedPtr<ISceneOutlinerTreeItem>>>& ListView, bool bGetForwardDirection)
{
	// Get the navigation direction key (HJKL -> Left, Down, Up or Right)
	TArray<FKey> NavKeys;
	if (IsTreeViewVertical(ListView))
	{
		NavKeys.Add(FKey(EKeys::J));
		NavKeys.Add(FKey(EKeys::K));
	}
	else
	{
		NavKeys.Add(FKey(EKeys::L));
		NavKeys.Add(FKey(EKeys::H));
	}
	return bGetForwardDirection ? NavKeys[0] : NavKeys[1];
}

void UVimEditorSubsystem::ScrollHalfPage(
	FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	static constexpr int32 SCROLL_NUM{ 6 };

	TSharedPtr<SListView<TSharedPtr<ISceneOutlinerTreeItem>>> ListView;
	if (!GetListView(SlateApp, ListView))
		return;

	bool			bScrollDown = InKeyEvent.GetKey() == EKeys::D;
	const FKey		NavDirection = GetTreeNavigationDirection(ListView, bScrollDown);
	const FKeyEvent KeyEvent =
		FUMInputPreProcessor::GetKeyEventFromKey(NavDirection,
			CurrentVimMode == EVimMode::Visual);

	for (int32 i{ 0 }; i < SCROLL_NUM; ++i)
		ProcessVimNavigationInput(SlateApp, KeyEvent);
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
	const static FString DEFAULT_TAB_INDEX = "1";
	FString				 ContentBrowserId = "ContentBrowserTab";
	FString				 TabNum;

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
		{ FInputChord(EModifierKey::Control, EKeys::U) },
		VimSubWeak, &VimSub::ScrollHalfPage);

	Input.AddKeyBinding_KeyEvent(
		{ FInputChord(EModifierKey::Control, EKeys::D) },
		VimSubWeak, &VimSub::ScrollHalfPage);

	Input.AddKeyBinding_KeyEvent(
		{ EKeys::U },
		VimSubWeak, &VimSub::Undo);

	// Delete item - Simulate the Delete key (WIP)
	Input.AddKeyBinding_KeyEvent(
		{ EKeys::X },
		VimSubWeak, &VimSub::DeleteItem);

	Input.AddKeyBinding_KeyEvent(
		{ EKeys::D },
		VimSubWeak, &VimSub::DeleteItem);

	Input.AddKeyBinding_KeyEvent(
		{ EKeys::Enter },
		VimSubWeak, &VimSub::Enter);

	Input.AddKeyBinding_KeyEvent(
		{ FInputChord(EModifierKey::Control, EKeys::N) },
		VimSubWeak, &VimSub::NavigateNextPrevious);

	Input.AddKeyBinding_KeyEvent(
		{ FInputChord(EModifierKey::Control, EKeys::P) },
		VimSubWeak, &VimSub::NavigateNextPrevious);
}
