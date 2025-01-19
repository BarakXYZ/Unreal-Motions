#include "UMSlateHelpers.h"
#include "Framework/Application/SlateApplication.h"
#include "Layout/ChildrenBase.h"
#include "LevelEditor.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SEditableText.h"
#include "Widgets/Input/SButton.h"

// DEFINE_LOG_CATEGORY_STATIC(LogUMSlateHelpers, NoLogging, All); // Prod
DEFINE_LOG_CATEGORY_STATIC(LogUMSlateHelpers, Log, All); // Dev
FUMLogger FUMSlateHelpers::Logger(&LogUMSlateHelpers);

bool FUMSlateHelpers::TraverseWidgetTree(
	const TSharedPtr<SWidget>& ParentWidget,
	TArray<TWeakPtr<SWidget>>& OutWidgets,
	const FString& TargetType, int32 SearchCount, int32 Depth)
{
	// Log the current widget and depth
	// Logger.Print(FString::Printf(TEXT("%s[Depth: %d] Checking widget: %s"),
	// 	*FString::ChrN(Depth * 2, ' '), // Visual indentation
	// 	Depth, *ParentWidget->GetTypeAsString()));

	bool bFoundAllRequested = false;

	if (ParentWidget->GetTypeAsString() == TargetType)
	{
		// Logger.Print(FString::Printf(
		// 	TEXT("%s[Depth: %d] Found %s | Origin Target: %s"),
		// 	*FString::ChrN(Depth * 2, ' '), Depth, *ParentWidget->ToString(),
		// 	*TargetType));
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

bool FUMSlateHelpers::TraverseWidgetTree(
	const TSharedPtr<SWidget>& ParentWidget,
	TWeakPtr<SWidget>&		   OutWidget,
	const FString&			   TargetType,
	int32					   Depth)
{
	// Log the current widget and depth
	// Logger.Print(FString::Printf(TEXT("%s[Depth: %d] Checking widget: %s"),
	// 	*FString::ChrN(Depth * 2, ' '), // Visual indentation
	// 	Depth, *ParentWidget->GetTypeAsString()));

	if (ParentWidget->GetTypeAsString() == TargetType)
	{
		// Logger.Print(FString::Printf(
		// 	TEXT("%s[Depth: %d] Found %s | Origin Target: %s"),
		// 	*FString::ChrN(Depth * 2, ' '), Depth, *ParentWidget->ToString(),
		// 	*TargetType));

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

// We can't rely on the GlobalTabmanager for this because our current minor tab
// doesn't necessarily reflect the frontmost major tab (for example if we have
// some other window focused that doesn't match the minor tab parent window)
// This function puts frontmost, which is different than active, as the target to
// fetch.
bool FUMSlateHelpers::GetFrontmostForegroundedMajorTab(
	TSharedPtr<SDockTab>& OutMajorTab)
{
	static const FString DockWellType{ "SDockingTabWell" };
	static const FName	 DockType{ "SDockTab" };

	FSlateApplication& SlateApp = FSlateApplication::Get();

	// TArray<TSharedRef<SWindow>> VisWins;
	// SlateApp.GetAllVisibleWindowsOrdered(VisWins);
	// if (VisWins.IsEmpty())
	// 	return false;

	const TSharedPtr<SWindow> ActiveWin =
		// VisWins.Last();  // This seems less solid

		// NOTE:
		// Actually (LOL) not sure if this statement is true. The issue seems to
		// be that the window just doesn't have any focus to start with (like no
		// tab was selected once it was left) thus it won't be able to find him?
		// We want this version of get active top level (not regular) because
		// window like "Widget Reflector" and such, are considered non-regular
		// and we won't be able to bring focus to them (or find major tabs in
		// them) they will essentially be skipped.
		// FSlateApplication::Get().GetActiveTopLevelWindow();
		SlateApp.GetActiveTopLevelRegularWindow();
	if (!ActiveWin.IsValid())
		return false;

	TWeakPtr<SWidget> FoundWidget;
	if (!TraverseWidgetTree(ActiveWin->GetContent(), FoundWidget, DockWellType))
		return false;

	if (FChildren* Tabs = FoundWidget.Pin()->GetChildren())
	{
		for (int32 i{ 0 }; i < Tabs->Num(); ++i)
		{
			// if (Tabs->GetChildAt(i)->GetType().IsEqual(DockType))
			if (Tabs->GetChildAt(i)->GetType().IsEqual(DockType))
			{
				TSharedRef<SDockTab> Tab =
					StaticCastSharedRef<SDockTab>(Tabs->GetChildAt(i));
				if (Tab->IsForeground())
				{
					Logger.Print(FString::Printf(TEXT("Major Tab Found: %s"),
									 *Tab->GetTabLabel().ToString()),
						ELogVerbosity::Verbose, true);
					OutMajorTab = Tab;
					return true;
				}
			}
		}
	}
	return false;
}

bool FUMSlateHelpers::GetParentDockingTabStackAsWidget(
	const TSharedRef<SWidget> ParentWidget, TWeakPtr<SWidget>& OutDockingTabStack)
{
	static const FString DockType = "SDockingTabStack";
	// return TraverseWidgetTree(ParentWidget, OutDockingTabStack, DockType);

	TSharedPtr<SWidget> CursorWidget = ParentWidget->GetParentWidget();
	if (CursorWidget.IsValid())
		while (!CursorWidget->GetTypeAsString().Equals(DockType))
		{
			CursorWidget = CursorWidget->GetParentWidget();
			if (!CursorWidget.IsValid())
				return false;
		}
	OutDockingTabStack = CursorWidget;
	return true;
}

bool FUMSlateHelpers::IsValidTreeViewType(const FString& InWidgetType)
{
	static const TSet<FString> ValidWidgetNavigationTypes{
		"SAssetTileView",
		"SSceneOutlinerTreeView",
		"STreeView",
		"SSubobjectEditorDragDropTree"
	};

	return (ValidWidgetNavigationTypes.Contains(InWidgetType)
		|| ValidWidgetNavigationTypes.Contains(InWidgetType.Left(9)));
	// Either something specific like "SAssetTileView", etc.
	// Or more generically starts with "STreeView"
}

bool FUMSlateHelpers::TryGetListView(FSlateApplication& SlateApp, TSharedPtr<SListView<TSharedPtr<ISceneOutlinerTreeItem>>>& OutListView)
{
	TSharedPtr<SWidget> FocusedWidget = SlateApp.GetUserFocusedWidget(0);
	if (!FocusedWidget.IsValid())
	{
		Logger.Print("Focused Widget is NOT valid.", ELogVerbosity::Warning);
		return false;
	}

	if (!IsValidTreeViewType(FocusedWidget->GetTypeAsString()))
		return false;

	OutListView =
		StaticCastSharedPtr<SListView<
			TSharedPtr<ISceneOutlinerTreeItem>>>(FocusedWidget);
	return true;
}

bool FUMSlateHelpers::GetSelectedTreeViewItemAsWidget(
	FSlateApplication& SlateApp, TSharedPtr<SWidget>& OutWidget,
	const TOptional<TSharedPtr<SListView<TSharedPtr<ISceneOutlinerTreeItem>>>>& OptionalListView)
{
	// Check if we should fetch a new view or use an existing one that was passed
	TSharedPtr<SListView<TSharedPtr<ISceneOutlinerTreeItem>>> ListView;
	if (OptionalListView.IsSet())
		ListView = OptionalListView.GetValue();

	else if (!TryGetListView(SlateApp, ListView)) // Try fetch ListView
		return false;

	TArray<TSharedPtr<ISceneOutlinerTreeItem>> SelItems =
		ListView->GetSelectedItems();
	if (SelItems.IsEmpty() || !SelItems[0].IsValid())
		return false;

	TSharedPtr<ITableRow> TableRow = ListView->WidgetFromItem(SelItems[0]);
	if (!TableRow.IsValid())
		return false;

	OutWidget = TableRow->AsWidget();
	return true;
}

void FUMSlateHelpers::UpdateTreeViewSelectionOnExitVisualMode(
	FSlateApplication& SlateApp, const TWeakPtr<ISceneOutlinerTreeItem>& AnchorItem)
{
	TSharedPtr<SListView<TSharedPtr<ISceneOutlinerTreeItem>>> ListView = nullptr;
	if (!FUMSlateHelpers::TryGetListView(SlateApp, ListView))
		return;

	const auto SelItems = ListView->GetSelectedItems();
	if (SelItems.IsEmpty() || !AnchorItem.IsValid())
		return;

	// Determine if we should now focus on the first or last item.
	// We deduce that by checking if our Anchor is to the left of the right
	// of the list.
	TSharedPtr<ISceneOutlinerTreeItem> CurrItem =
		AnchorItem == SelItems[0] ? SelItems.Last() : SelItems[0];
	ListView->ClearSelection();
	ListView->SetItemSelection(CurrItem, true,
		ESelectInfo::OnNavigation); // OnNavigation is important here to clear
									// the selection array more deeply (it seems)
}

bool FUMSlateHelpers::IsValidEditableText(const FString& InWidgetType)
{
	static const TSet<FString> ValidEditableTextTypes{
		"SEditableText",
		"SMultiLineEditableText",
		// "SEditableTextBox",
		// "SMultiLineEditableTextBox"
	};

	return ValidEditableTextTypes.Contains(InWidgetType);
}

bool FUMSlateHelpers::DoesTabResideInWindow(
	const TSharedRef<SWindow>  ParentWindow,
	const TSharedRef<SDockTab> ChildTab)
{
	FWidgetPath ChildWidgetPath;
	if (FSlateApplication::Get().FindPathToWidget(
			ChildTab->GetContent(), ChildWidgetPath))
	{
		if (ChildWidgetPath.ContainsWidget(&ParentWindow->GetContent().Get()))
		{
			Logger.Print(
				FString::Printf(
					TEXT("Window: %s contains tab: %s"),
					*ParentWindow->GetTitle().ToString(),
					*ChildTab->GetTabLabel().ToString()),
				ELogVerbosity::Verbose);
			return true;
		}
		else
		{
			Logger.Print(
				FString::Printf(
					TEXT("Window: %s does not contains tab: %s"),
					*ParentWindow->GetTitle().ToString(),
					*ChildTab->GetTabLabel().ToString()),
				ELogVerbosity::Warning);
			return false;
		}
	}
	return false;
}

bool FUMSlateHelpers::DoesWidgetResideInTab(
	const TSharedRef<SDockTab> ParentTab, const TSharedRef<SWidget> ChildWidget)
{
	FWidgetPath ParentWidgetPath;
	FWidgetPath ChildWidgetPath;
	if (FSlateApplication::Get().FindPathToWidget(ChildWidget, ChildWidgetPath))
	{
		if (ChildWidgetPath.ContainsWidget(&ParentTab->GetContent().Get()))
		{
			Logger.Print(
				FString::Printf(
					TEXT("Tab %s contain Widget %s"),
					*ParentTab->GetTabLabel().ToString(),
					*ChildWidget->GetTypeAsString()),
				ELogVerbosity::Verbose);
			return true;
		}
		else
		{
			Logger.Print(
				FString::Printf(
					TEXT("Tab %s does NOT contain Widget %s"),
					*ParentTab->GetTabLabel().ToString(),
					*ChildWidget->GetTypeAsString()),
				ELogVerbosity::Warning);
			return false;
		}
	}
	return false;
}

// TODO: This needs more work to be more robust and be able to catch:
// Content Browser, Preferences, Settings, etc.
// Need some additional debugging and such.
// Potentially work with the Active Window and verify we're actually
// fetching an active major tab (one that doesn't reside in an inactive window)
TSharedPtr<SDockTab> FUMSlateHelpers::GetActiveMajorTab()
{
	TSharedRef<FGlobalTabmanager> GTM = FGlobalTabmanager::Get();
	if (const TSharedPtr<SDockTab> ActiveMinorTab = GTM->GetActiveTab())
	{
		if (const TSharedPtr<FTabManager> TabManager =
				ActiveMinorTab->GetTabManagerPtr())
		{
			if (const TSharedPtr<SDockTab> ActiveMajorTab =
					GTM->GetMajorTabForTabManager(TabManager.ToSharedRef()))
			{
				return ActiveMajorTab;
			}
			else if (ActiveMinorTab->GetVisualTabRole() == ETabRole::MajorTab)
				return ActiveMinorTab;
		}
	}
	return nullptr;
}

TSharedPtr<SDockTab> FUMSlateHelpers::GetActiveMinorTab()
{
	TSharedRef<FGlobalTabmanager> GTM = FGlobalTabmanager::Get();
	if (const TSharedPtr<SDockTab> ActiveMinorTab = GTM->GetActiveTab())
	{
		return ActiveMinorTab;
	}
	return nullptr;
}

bool FUMSlateHelpers::IsVisualTextSelected(FSlateApplication& SlateApp)
{
	const FName EditTextType = "SEditableText";

	TSharedPtr<SWidget> FocusedWidget = SlateApp.GetUserFocusedWidget(0);
	if (!FocusedWidget.IsValid())
		return false;
	if (!FocusedWidget->GetType().IsEqual(EditTextType))
		return false;

	TSharedPtr<SEditableText> WidgetAsEditText =
		StaticCastSharedPtr<SEditableText>(FocusedWidget);

	// WidgetAsEditText->SetTextBlockStyle();
	return WidgetAsEditText->AnyTextSelected();
}
//
// NOTE: Kept here some commented methods for future reference.
void FUMSlateHelpers::ActivateWindow(const TSharedRef<SWindow> InWindow)
{
	FSlateApplication& SlateApp = FSlateApplication::Get();
	InWindow->BringToFront(true);
	TSharedRef<SWidget>			   WinContent = InWindow->GetContent();
	FWindowDrawAttentionParameters DrawParams(
		EWindowDrawAttentionRequestType::UntilActivated);
	// Window->DrawAttention(DrawParams);
	// Window->ShowWindow();
	// Window->FlashWindow(); // Cool way to visually indicate windows.

	// I was a bit worried about this, but actually it seems that without this
	// we will have a weird focusing bug. So this actually seems to work pretty
	// well.
	// SlateApp.ClearAllUserFocus();

	// NOTE:
	// This is really interesting. It may help to soildfy focus and what we
	// actually want! Like pass in the MajorTab->MinorTab->*Widget*
	// I'm thinking maybe something like find major tab in window function ->
	// Then we have the major, we can do the check, get the minor, get the widget
	// and pass it in **before drawing attention**!
	// Window->SetWidgetToFocusOnActivate();

	// NOTE:
	// This will focus the window content, which isn't really useful. And it
	// looks like ->DrawAttention() Seems to do a better job!
	// SlateApp.SetAllUserFocus(
	// 	WinContent, EFocusCause::Navigation);

	InWindow->DrawAttention(DrawParams); // Seems to work well!

	if (InWindow->GetContent()->HasAnyUserFocusOrFocusedDescendants())
		Logger.Print("Window has focus", ELogVerbosity::Verbose, true);

	Logger.Print(FString::Printf(
		TEXT("Activated Window: %s"), *InWindow->GetTitle().ToString()));
}

FVector2f FUMSlateHelpers::GetWidgetCenterScreenSpacePosition(
	const TSharedRef<SWidget> InWidget)
{
	const FGeometry& WidgetGeometry = InWidget->GetCachedGeometry();
	// const FVector2D	 WidgetScreenPosition = WidgetGeometry.GetAbsolutePosition();
	// const FVector2D	 WidgetSize = WidgetGeometry.GetLocalSize();
	// return WidgetScreenPosition + (WidgetSize * 0.5f);

	// Calculate the center in screen space
	return WidgetGeometry.LocalToAbsolute(WidgetGeometry.GetLocalSize() * 0.5f);
}

FVector2f FUMSlateHelpers::GetWidgetTopRightScreenSpacePosition(
	const TSharedRef<SWidget> InWidget,
	const FVector2f			  Offset)
{
	// Get the cached geometry
	const FGeometry& WidgetGeometry = InWidget->GetCachedGeometry();

	// Calculate the local top-right position
	FVector2f LocalTopRight = FVector2f(WidgetGeometry.GetLocalSize().X, 0.0f);

	// Convert local position to absolute screen space
	FVector2f AbsoluteTopRight = WidgetGeometry.LocalToAbsolute(LocalTopRight);

	// Adjust the position to stay within the bounds
	// return AbsoluteTopRight - FVector2D(Adjustment, Adjustment);
	return AbsoluteTopRight - Offset;
}

TSharedPtr<FTabManager> FUMSlateHelpers::GetLevelEditorTabManager()
{
	FLevelEditorModule& LevelEditorModule =
		FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");

	if (const TSharedPtr<ILevelEditor> LevelEditorPtr =
			LevelEditorModule.GetLevelEditorInstance().Pin())
	{
		// Get the Tab Manager of the Level Editor
		if (const TSharedPtr<FTabManager> LevelEditorTabManager =
				LevelEditorPtr->GetTabManager())
		{
			return LevelEditorTabManager;
		}
	}
	return nullptr;
}

TSharedPtr<SWidget> FUMSlateHelpers::GetTabWellForTabManager(
	const TSharedRef<FTabManager> InTabManager)
{
	// We need the Owner Tab to access the TabWell parent
	if (const TSharedPtr<SDockTab> LevelEditorTab =
			InTabManager->GetOwnerTab())
	{
		if (const TSharedPtr<SWidget> LevelEditorTabWell =
				LevelEditorTab->GetParentWidget())
		{
			return LevelEditorTabWell;
		}
	}
	return nullptr;
}

TSharedPtr<FGenericWindow> FUMSlateHelpers::GetGenericActiveTopLevelWindow()
{
	if (const TSharedPtr<SWindow> ActiveWindow =
			FSlateApplication::Get().GetActiveTopLevelWindow())
	{
		if (const TSharedPtr<FGenericWindow> GenericActiveWindow =
				ActiveWindow->GetNativeWindow())
		{
			return GenericActiveWindow;
		}
	}
	return nullptr;
}

void FUMSlateHelpers::SimulateMenuClicks(
	const TSharedRef<SWidget>		ParentWidget,
	const TArrayView<const FString> TargetEntries,
	int32							ArrayIndex)
{
	static const FString TextBlockType = "STextBlock";
	static const FString ButtonType = "SButton";

	if (!TargetEntries.IsValidIndex(ArrayIndex))
		return;

	// Get all Text Blocks in the target parent to look for the the entries.
	TArray<TWeakPtr<SWidget>> TargetTextBlocks;
	if (!TraverseWidgetTree(ParentWidget, TargetTextBlocks, TextBlockType))
		return;

	TSharedPtr<SButton> TargetButton;

	for (const TWeakPtr<SWidget>& Text : TargetTextBlocks)
	{
		const TSharedPtr<SWidget> PinText = Text.Pin();
		if (!PinText.IsValid())
			continue;

		const TSharedPtr<STextBlock> AsTextBlock =
			StaticCastSharedPtr<STextBlock>(PinText);

		if (!AsTextBlock->GetText().ToString().Equals(TargetEntries[ArrayIndex]))
			continue;

		TSharedPtr<SWidget> Parent = AsTextBlock->GetParentWidget();
		while (Parent.IsValid())
		{
			// NOTE:
			// We can safely search for SButton more generically cause types like:
			// "SMenuEntryButton", "SSubMenuButton", are considered "SButton".
			// if we only search for Parent->GetType() we will find the more
			// specific type (e.g. "SMenuEntryButton"). But like that it's safe.
			if (Parent->GetWidgetClass().GetWidgetType().ToString().Equals(ButtonType))
			{
				TargetButton = StaticCastSharedPtr<SButton>(Parent);
				++ArrayIndex; // Move to the next entry in the array
				break;		  // Break for the 'while' loop
			}
			else // Continue climbing and searching
				Parent = Parent->GetParentWidget();
		}
		break; // Break from the 'for' loop
	}

	if (!TargetButton.IsValid())
		return;

	TargetButton->SimulateClick();
	if (!TargetEntries.IsValidIndex(ArrayIndex))
		return; // This will prevent an extra unnecessary call

	// We now want to wait a bit to let the Menu Window to open.
	// This is important to let the internals of Slate catch-up and find the Win.
	FTimerDelegate TimerDelegate;
	TimerDelegate.BindStatic(
		&FUMSlateHelpers::GetActiveMenuWindowAndCallSimulateMenuClicks,
		TargetEntries, ArrayIndex);

	FTimerHandle TimerHandle;
	GEditor->GetTimerManager()->SetTimer(
		TimerHandle, TimerDelegate, 0.025f, false); // Check on MacOS and verify
}

void FUMSlateHelpers::GetActiveMenuWindowAndCallSimulateMenuClicks(
	const TArrayView<const FString> TargetEntries, const int32 ArrayIndex)
{
	const FSlateApplication&  SlateApp = FSlateApplication::Get();
	const TSharedPtr<SWindow> MenuWindow = SlateApp.GetActiveTopLevelWindow();
	if (!MenuWindow.IsValid() || MenuWindow->GetType() != EWindowType::Menu)
		return; // We're specifcally targeting only Menu Windows (non-regular)

	SimulateMenuClicks(MenuWindow.ToSharedRef(), TargetEntries, ArrayIndex);
}
