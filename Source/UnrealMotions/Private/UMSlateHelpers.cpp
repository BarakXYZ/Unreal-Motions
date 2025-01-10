#include "UMSlateHelpers.h"
#include "Framework/Application/SlateApplication.h"
#include "Layout/ChildrenBase.h"
#include "Widgets/Docking/SDockTab.h"

DEFINE_LOG_CATEGORY_STATIC(LogUMSlateHelpers, NoLogging, All); // Prod
// DEFINE_LOG_CATEGORY_STATIC(LogUMSlateHelpers, Log, All); // Dev

const TSharedPtr<FUMSlateHelpers> FUMSlateHelpers::SlateHelpers =
	MakeShared<FUMSlateHelpers>();

FUMSlateHelpers::FUMSlateHelpers()
{
	Logger.SetLogCategory(&LogUMSlateHelpers);
}
FUMSlateHelpers::~FUMSlateHelpers()
{
}

bool FUMSlateHelpers::TraverseWidgetTree(
	const TSharedPtr<SWidget>& ParentWidget,
	TArray<TWeakPtr<SWidget>>& OutWidgets,
	const FString& TargetType, int32 SearchCount, int32 Depth)
{
	// Log the current widget and depth
	UE_LOG(LogUMSlateHelpers, Display,
		TEXT("%s[Depth: %d] Checking widget: %s"),
		*FString::ChrN(Depth * 2, ' '), // Visual indentation
		Depth, *ParentWidget->GetTypeAsString());

	bool bFoundAllRequested = false;

	if (ParentWidget->GetTypeAsString() == TargetType)
	{
		UE_LOG(LogUMSlateHelpers, Display,
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

bool FUMSlateHelpers::TraverseWidgetTree(
	const TSharedPtr<SWidget>& ParentWidget,
	TWeakPtr<SWidget>&		   OutWidget,
	const FString&			   TargetType,
	int32					   Depth)
{
	// Log the current widget and depth
	UE_LOG(LogUMSlateHelpers, Display,
		TEXT("%s[Depth: %d] Checking widget: %s"),
		*FString::ChrN(Depth * 2, ' '), // Visual indentation
		Depth, *ParentWidget->GetTypeAsString());

	if (ParentWidget->GetTypeAsString() == TargetType)
	{
		UE_LOG(LogUMSlateHelpers, Display,
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

// We can't rely on the GlobalTabmanager for this because our current minor tab
// doesn't necessarily reflect the frontmost major tab (for example if we have
// some other window focused that doesn't match the minor tab parent window)
bool FUMSlateHelpers::GetFrontmostForegroundedMajorTab(
	TSharedPtr<SDockTab>& OutMajorTab)
{
	static const FString DockWellType{ "SDockingTabWell" };
	static const FName	 DockType{ "SDockTab" };

	const TSharedPtr<SWindow> ActiveWin =
		FSlateApplication::Get().GetActiveTopLevelWindow();
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
		SlateHelpers->Logger.Print(
			"Focused Widget is NOT valid.", ELogVerbosity::Warning);
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
