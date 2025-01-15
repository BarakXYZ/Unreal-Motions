#include "VimEditorSubsystem.h"
#include "Editor.h"
#include "Templates/SharedPointer.h"
#include "Types/SlateEnums.h"
#include "UMFocusManager.h"
#include "UMInputPreProcessor.h"
#include "ISceneOutlinerTreeItem.h"
#include "Input/Events.h"
#include "UMSlateHelpers.h"
#include "UMInputHelpers.h"
#include "UMEditorNavigation.h"
#include "UMEditorCommands.h"
#include "UMConfig.h"

DEFINE_LOG_CATEGORY_STATIC(LogVimEditorSubsystem, Log, All);

void UVimEditorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	if (!FUMConfig::Get()->IsVimEnabled())
		return;

	Logger.SetLogCategory(&LogVimEditorSubsystem);

	VimSubWeak = this;
	InputPP = FUMInputPreProcessor::Get().ToWeakPtr();
	BindCommands();
	Super::Initialize(Collection);

	FCoreDelegates::OnPostEngineInit.AddUObject(
		this, &UVimEditorSubsystem::WrapAndSetCustomMessageHandler);
}

void UVimEditorSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UVimEditorSubsystem::WrapAndSetCustomMessageHandler()
{
	TSharedPtr<GenericApplication> PlatformApp =
		FSlateApplication::Get().GetPlatformApplication();
	if (!PlatformApp.IsValid())
		return;

	// Grab the current/original handler
	TSharedPtr<FGenericApplicationMessageHandler> OriginGenericAppMessageHandler =
		PlatformApp->GetMessageHandler();

	// Create chaining handler, passing in the old one
	UMGenericAppMessageHandler =
		MakeShared<FUMGenericAppMessageHandler>(OriginGenericAppMessageHandler);

	// Set the platform app’s active handler to your new one
	PlatformApp->SetMessageHandler(UMGenericAppMessageHandler.ToSharedRef());
}

void UVimEditorSubsystem::OnResetSequence()
{
	CountBuffer.Empty();
}

void UVimEditorSubsystem::OnCountPrefix(FString AddedCount)
{
	// Building the buffer -> "1" + "7" + "3" == "173"
	CountBuffer += AddedCount;
	// Logger.Print(FString::Printf(TEXT("On Count Prefix: %s"), *CountBuffer));
}

void UVimEditorSubsystem::OnVimModeChanged(const EVimMode NewVimMode)
{
	PreviousVimMode = CurrentVimMode;
	CurrentVimMode = NewVimMode;

	FSlateApplication& SlateApp = FSlateApplication::Get();

	switch (CurrentVimMode)
	{
		case EVimMode::Normal:
		{
			// Logger.Print("Vim Mode Changed: Normal Mode", ELogVerbosity::Verbose, true);
			if (PreviousVimMode == EVimMode::Visual
				&& FUMSlateHelpers::IsVisualTextSelected(SlateApp))
				FUMInputPreProcessor::SimulateKeyPress(SlateApp, EKeys::Escape);

			UMGenericAppMessageHandler->ToggleBlockAllCharInput(true);
			FUMSlateHelpers::UpdateTreeViewSelectionOnExitVisualMode(
				SlateApp, AnchorTreeViewItem.Item);
			break;
		}
		case EVimMode::Insert:
		{
			// Slight delay before allowing character input
			// to prevent 'i' from being entered into editable text fields
			FTimerHandle TimerHandle;
			GEditor->GetTimerManager()->SetTimer(
				TimerHandle,
				[this]() { UMGenericAppMessageHandler->ToggleBlockAllCharInput(false); },
				0.05f, false);
			break;
		}
		case EVimMode::Visual:
		{
			VisualNavOffsetIndicator = 0;
			CaptureAnchorTreeViewItemSelectionAndIndex(SlateApp);
			break;
		}
	}
}

void UVimEditorSubsystem::CaptureAnchorTreeViewItemSelectionAndIndex(
	FSlateApplication& SlateApp)
{
	TSharedPtr<SListView<TSharedPtr<ISceneOutlinerTreeItem>>> ListView;
	if (FUMSlateHelpers::TryGetListView(SlateApp, ListView))
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

		// Find the index of this item in the list
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
		else
		{
			AnchorTreeViewItem.Item = FirstSelItem;
			AnchorTreeViewItem.Index = ItemIndex;
		}
	}
}

void UVimEditorSubsystem::NavigateToFirstOrLastItem(
	FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	if (HandleTreeViewFirstOrLastItemNavigation(SlateApp, InKeyEvent))
		return;
	HandleArrowKeysNavigationToFirstOrLastItem(SlateApp, InKeyEvent);
}

void UVimEditorSubsystem::HandleArrowKeysNavigationToFirstOrLastItem(
	FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	static const int32 TIMES_TO_NAVIGATE{ 50 };
	const FKeyEvent	   NavKeyEvent =
		FUMInputPreProcessor::GetKeyEventFromKey(InKeyEvent.IsShiftDown()
				? EKeys::J
				: EKeys::K,
			InKeyEvent.IsShiftDown());

	for (int32 i{ 0 }; i < TIMES_TO_NAVIGATE; ++i)
		HandleArrowKeysNavigation(SlateApp, NavKeyEvent);
}

bool UVimEditorSubsystem::HandleTreeViewFirstOrLastItemNavigation(
	FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	TSharedPtr<SListView<TSharedPtr<ISceneOutlinerTreeItem>>> ListView;
	if (!FUMSlateHelpers::TryGetListView(SlateApp, ListView))
		return false;

	TArrayView<const TSharedPtr<ISceneOutlinerTreeItem, ESPMode::ThreadSafe>>
		AllItems = ListView->GetItems();
	if (AllItems.IsEmpty())
		return true; // Maybe we want to return false in here? Need some testings

	// if G, our GoTo item will be the last one, if gg, first
	bool bIsShiftDown = InKeyEvent.IsShiftDown();

	if (CurrentVimMode == EVimMode::Normal)
	{
		HandleTreeViewNormalModeFirstOrLastItemNavigation(
			ListView.ToSharedRef(), AllItems, bIsShiftDown);
	}
	else // Has to be Visual Mode
	{
		HandleTreeViewVisualModeFirstOrLastItemNavigation(
			SlateApp, ListView.ToSharedRef(), AllItems, bIsShiftDown);
	}
	return true;
}

void UVimEditorSubsystem::HandleTreeViewNormalModeFirstOrLastItemNavigation(
	TSharedRef<SListView<TSharedPtr<ISceneOutlinerTreeItem>>> ListView,
	TArrayView<const TSharedPtr<ISceneOutlinerTreeItem>>&	  AllItems,
	bool													  bIsShiftDown)
{
	const auto& GotoItem =
		bIsShiftDown ? AllItems.Last()
					 : AllItems[0];

	ListView->SetSelection(GotoItem, ESelectInfo::OnNavigation);
	ListView->RequestNavigateToItem(GotoItem, 0); // Does that work solo?
}

void UVimEditorSubsystem::HandleTreeViewVisualModeFirstOrLastItemNavigation(
	FSlateApplication&										  SlateApp,
	TSharedRef<SListView<TSharedPtr<ISceneOutlinerTreeItem>>> ListView,
	TArrayView<const TSharedPtr<ISceneOutlinerTreeItem>>&	  AllItems,
	bool													  bIsShiftDown)
{
	const auto& SelItems = ListView->GetSelectedItems();
	if (SelItems.IsEmpty())
		return;

	if (!AnchorTreeViewItem.Item.IsValid())
	{
		CaptureAnchorTreeViewItemSelectionAndIndex(SlateApp);
		FUMLogger::NotifySuccess(FText::FromString(
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

	for (int32 i{ 0 }; i < TimesToNavigate; ++i)
	{
		ProcessVimNavigationInput(SlateApp,
			FUMInputPreProcessor::GetKeyEventFromKey(
				NavKeyDirection,
				true));
	}
}

void UVimEditorSubsystem::ProcessVimNavigationInput(
	FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	if (HandleListViewNavigation(SlateApp, InKeyEvent))
		return; // User is in a valid navigatable list view.

	// Fallback is the default arrows simulation navigation
	HandleArrowKeysNavigation(SlateApp, InKeyEvent);
}

void UVimEditorSubsystem::HandleArrowKeysNavigation(
	FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	FKeyEvent OutKeyEvent;
	FUMInputHelpers::MapVimToArrowNavigation(InKeyEvent, OutKeyEvent,
		CurrentVimMode == EVimMode::Visual /* bIsShiftDown true if Visual */);

	const int32 Count{ GetPracticalCountBuffer() };

	for (int32 i{ 0 }; i < Count; ++i)
	{
		FUMInputPreProcessor::ToggleNativeInputHandling(true);
		SlateApp.ProcessKeyDownEvent(OutKeyEvent);
		SlateApp.ProcessKeyUpEvent(OutKeyEvent);
	}
}

bool UVimEditorSubsystem::HandleListViewNavigation(
	FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	const auto& FocusedWidget = SlateApp.GetUserFocusedWidget(0);
	if (!FocusedWidget.IsValid() || !FUMSlateHelpers::IsValidTreeViewType(FocusedWidget->GetTypeAsString()))
		return false;

	FNavigationEvent NavEvent;
	FUMInputHelpers::GetNavigationEventFromVimKey(
		InKeyEvent, NavEvent,
		CurrentVimMode == EVimMode::Visual /* Shift-Down in Visual Mode */);

	const int32 Count{ GetPracticalCountBuffer() };

	for (int32 i{ 0 }; i < Count; ++i)
	{
		const FNavigationReply NavReply =
			FocusedWidget->OnNavigation( // Navigate to the next or previous item
				FocusedWidget->GetCachedGeometry(), NavEvent);

		// DEPRECATED: keeping for reference how we can use BoundaryRule
		// if (NavReply.GetBoundaryRule() != EUINavigationRule::Escape)
		// 	TrackVisualOffsetNavigation(InKeyEvent); // Only track if moving
	}
	return true;
}

int32 UVimEditorSubsystem::GetPracticalCountBuffer()
{
	if (!CountBuffer.IsEmpty())
	{
		return FMath::Clamp(
			FCString::Atoi(*CountBuffer),
			MIN_REPEAT_COUNT,
			MAX_REPEAT_COUNT);
	}
	return MIN_REPEAT_COUNT;
}

bool UVimEditorSubsystem::IsTreeViewVertical(
	const TSharedRef<SListView<TSharedPtr<ISceneOutlinerTreeItem>>>& ListView)
{
	const static FName AssetTileView = "SAssetTileView";

	if (!ListView->GetType().IsEqual(AssetTileView))
		return true;
	return false;
}

FKey UVimEditorSubsystem::GetTreeNavigationDirection(
	const TSharedRef<SListView<TSharedPtr<ISceneOutlinerTreeItem>>> ListView, bool bGetForwardDirection)
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
	const bool			   bScrollDown = InKeyEvent.GetKey() == EKeys::D;
	FKey				   NavDirection;

	TSharedPtr<SListView<TSharedPtr<ISceneOutlinerTreeItem>>> ListView;
	if (FUMSlateHelpers::TryGetListView(SlateApp, ListView)) // Valid list view handling
		NavDirection = GetTreeNavigationDirection(ListView.ToSharedRef(), bScrollDown);
	else // Fallback use arrows for navigation
		NavDirection = FKey(bScrollDown ? EKeys::J : EKeys::K);

	const FKeyEvent KeyEvent =
		FUMInputPreProcessor::GetKeyEventFromKey(NavDirection,
			CurrentVimMode == EVimMode::Visual);

	for (int32 i{ 0 }; i < SCROLL_NUM; ++i)
		ProcessVimNavigationInput(SlateApp, KeyEvent);
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
		&FUMEditorCommands::OpenWidgetReflector);

	Input.AddKeyBinding_KeyEvent(
		{ EKeys::SpaceBar, EKeys::O, FInputChord(EModifierKey::Shift, EKeys::W) },
		&FUMEditorCommands::OpenWidgetReflector);

	Input.AddKeyBinding_NoParam(
		{ EKeys::SpaceBar, EKeys::O, EKeys::O },
		&FUMEditorCommands::OpenOutputLog);

	Input.AddKeyBinding_KeyEvent(
		{ EKeys::SpaceBar, EKeys::O, EKeys::P },
		&FUMEditorCommands::OpenPreferences);

	//** Open Content Browsers 1-4 */
	Input.AddKeyBinding_KeyEvent(
		{ EKeys::SpaceBar, EKeys::O, EKeys::C, EKeys::One },
		&FUMEditorCommands::OpenContentBrowser);
	Input.AddKeyBinding_KeyEvent(
		{ EKeys::SpaceBar, EKeys::O, EKeys::C, EKeys::Two },
		&FUMEditorCommands::OpenContentBrowser);
	Input.AddKeyBinding_KeyEvent(
		{ EKeys::SpaceBar, EKeys::O, EKeys::C, EKeys::Three },
		&FUMEditorCommands::OpenContentBrowser);
	Input.AddKeyBinding_KeyEvent(
		{ EKeys::SpaceBar, EKeys::O, EKeys::C, EKeys::Four },
		&FUMEditorCommands::OpenContentBrowser);

	// Input.AddKeyBinding_NoParam(
	// 	{ FInputChord(EModifierKey::Control, EKeys::W) },
	// 	&FUMFocusManager::RemoveActiveMajorTab);

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
		&FUMEditorCommands::Undo);

	// Delete item - Simulate the Delete key (WIP)
	Input.AddKeyBinding_KeyEvent(
		{ EKeys::X },
		&FUMEditorCommands::DeleteItem);

	Input.AddKeyBinding_KeyEvent(
		{ EKeys::D },
		&FUMEditorCommands::DeleteItem);

	Input.AddKeyBinding_KeyEvent(
		{ EKeys::Enter },
		&FUMInputHelpers::Enter);

	Input.AddKeyBinding_KeyEvent(
		{ FInputChord(EModifierKey::Control, EKeys::N) },
		&FUMEditorCommands::NavigateNextPrevious);

	Input.AddKeyBinding_KeyEvent(
		{ FInputChord(EModifierKey::Control, EKeys::P) },
		&FUMEditorCommands::NavigateNextPrevious);

	Input.AddKeyBinding_KeyEvent(
		{ FInputChord(EModifierKey::Control, EKeys::F) },
		&FUMEditorCommands::FocusSearchBox);

	Input.AddKeyBinding_KeyEvent(
		{ EKeys::SpaceBar, EKeys::R },
		&FUMInputHelpers::SimulateRightClick);

	Input.AddKeyBinding_KeyEvent(
		{ EKeys::SpaceBar, FInputChord(EModifierKey::Shift, EKeys::R) },
		&FUMInputHelpers::ToggleRightClickPress);

	/////////////////////////////////////////////////////////////////////////
	//						~ Panel Navigation ~
	//
	Input.AddKeyBinding_KeyEvent(
		{ FInputChord(EModifierKey::Control, EKeys::H) },
		&FUMEditorNavigation::NavigatePanelTabs);

	Input.AddKeyBinding_KeyEvent(
		{ FInputChord(EModifierKey::Control, EKeys::J) },
		&FUMEditorNavigation::NavigatePanelTabs);

	Input.AddKeyBinding_KeyEvent(
		{ FInputChord(EModifierKey::Control, EKeys::K) },
		&FUMEditorNavigation::NavigatePanelTabs);

	Input.AddKeyBinding_KeyEvent(
		{ FInputChord(EModifierKey::Control, EKeys::L) },
		&FUMEditorNavigation::NavigatePanelTabs);

	Input.AddKeyBinding_NoParam(
		{ EKeys::SpaceBar, EKeys::D, EKeys::C },
		&FUMEditorCommands::ClearAllDebugMessages);

	Input.AddKeyBinding_NoParam(
		{ EKeys::SpaceBar, EKeys::D, EKeys::T, EKeys::N },
		&FUMEditorCommands::ToggleAllowNotifications);

	//
	/////////////////////////////////////////////////////////////////////////
}
