#include "UMSlateHelpers.h"
#include "Framework/Application/SlateApplication.h"
#include "Input/Events.h"
#include "Layout/ChildrenBase.h"
#include "LevelEditor.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SEditableText.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SWindow.h"
#include "UMFocusVisualizer.h"

// DEFINE_LOG_CATEGORY_STATIC(LogUMSlateHelpers, NoLogging, All); // Prod
DEFINE_LOG_CATEGORY_STATIC(LogUMSlateHelpers, Log, All); // Dev
FUMLogger	  FUMSlateHelpers::Logger(&LogUMSlateHelpers);
const FString FUMSlateHelpers::TabWellType = "SDockingTabWell";

bool FUMSlateHelpers::TraverseFindWidget(
	const TSharedRef<SWidget> BaseWidget,
	TSharedPtr<SWidget>&	  OutWidget,
	const FString&			  TargetType,
	const uint64			  IgnoreWidgetId,
	const bool				  bSearchStartsWith,
	int32					  Depth)
{
	if (!BaseWidget->GetVisibility().IsVisible() || !BaseWidget->IsEnabled())
		return false;

	// LogTraversalSearch(Depth, BaseWidget);

	if (IsWidgetTargetType(BaseWidget, TargetType, bSearchStartsWith))
	{
		// LogTraverseFoundWidget(Depth, BaseWidget, TargetType);
		OutWidget = BaseWidget;
		return true;
	}

	// Recursively traverse the children of the current widget
	FChildren* Children = BaseWidget->GetChildren();
	if (Children)
	{
		for (int32 i = 0; i < Children->Num(); ++i)
		{
			const TSharedRef<SWidget> Child = Children->GetChildAt(i);

			if (IgnoreWidgetId != INDEX_NONE && Child->GetId() == IgnoreWidgetId)
				continue;

			if (TraverseFindWidget(Child, OutWidget, TargetType,
					IgnoreWidgetId, bSearchStartsWith, Depth + 1))
				return true;
		}
	}
	return false;
}

bool FUMSlateHelpers::TraverseFindWidget(
	const TSharedRef<SWidget>	 BaseWidget,
	TArray<TSharedPtr<SWidget>>& OutWidgets,
	const FString&				 TargetType,
	int32						 SearchCount,
	const bool					 bSearchStartsWith,
	int32						 Depth)
{
	bool bFoundAllRequested = false;

	if (!BaseWidget->GetVisibility().IsVisible() || !BaseWidget->IsEnabled())
		return false;

	// LogTraversalSearch(Depth, BaseWidget);

	if (IsWidgetTargetType(BaseWidget, TargetType, bSearchStartsWith))
	{
		// LogTraverseFoundWidget(Depth, BaseWidget, TargetType);
		OutWidgets.Add(BaseWidget);

		// If SearchCount is -1, continue searching but mark that we found at
		// least one.
		// If SearchCount is positive, check if we've found the targeted count.
		if (SearchCount == -1)
			bFoundAllRequested = true;
		else if (OutWidgets.Num() >= SearchCount)
			return true;
	}

	// Recursively traverse the children of the current widget
	FChildren* Children = BaseWidget->GetChildren();
	if (Children)
	{
		for (int32 i{ 0 }; i < Children->Num(); ++i)
		{
			const TSharedRef<SWidget> Child = Children->GetChildAt(i);
			bool					  bChildFound = TraverseFindWidget(
				 Child, OutWidgets, TargetType, SearchCount, bSearchStartsWith, Depth + 1);

			// If SearchCount is -1, accumulate the "found" status
			if (SearchCount == -1)
				bFoundAllRequested |= bChildFound;

			// If SearchCount is positive and enough widgets are found
			// (OutWidgets.Num() >= SearchCount, evaluated during child
			// traversal), immediately return true.
			// The child won't return true if not enough widgets were found!
			else if (bChildFound)
				return true;
		}
	}
	return bFoundAllRequested;
}

bool FUMSlateHelpers::TraverseFindWidget(
	const TSharedRef<SWidget>	 BaseWidget,
	TArray<TSharedRef<SWidget>>& OutWidgets,
	const TSet<FString>&		 TargetTypes,
	const TArray<FString>&		 StartsWithTargetTypes,
	int32 SearchCount, int32 Depth)
{
	bool bFoundAllRequested = false;

	if (!BaseWidget->GetVisibility().IsVisible() || !BaseWidget->IsEnabled())
		// if (!BaseWidget->GetVisibility().IsVisible())
		return false;

	// LogTraversalSearch(Depth, BaseWidget);

	if (TargetTypes.Contains(GetCleanWidgetType(BaseWidget->GetTypeAsString()))
		|| TargetTypes.Contains(GetCleanWidgetType(
			BaseWidget->GetWidgetClass().GetWidgetType().ToString()))
		|| IsWidgetTargetType(BaseWidget, StartsWithTargetTypes, true))
	{
		// LogTraverseFoundWidget(Depth, BaseWidget, TargetTypes);
		OutWidgets.Add(BaseWidget);

		// If SearchCount is -1, continue searching but mark that we found at least one
		// If SearchCount is positive, check if we've found enough
		if (SearchCount == -1)
			bFoundAllRequested = true;
		else if (OutWidgets.Num() >= SearchCount)
			return true;
	}

	// Recursively traverse the children of the current widget
	FChildren* Children = BaseWidget->GetChildren();
	if (Children)
	{
		for (int32 i{ 0 }; i < Children->Num(); ++i)
		{
			const TSharedRef<SWidget> Child = Children->GetChildAt(i);
			bool					  bChildFound = TraverseFindWidget(
				 Child, OutWidgets, TargetTypes, StartsWithTargetTypes, SearchCount, Depth + 1);

			// If SearchCount is -1, accumulate the "found" status
			if (SearchCount == -1)
				bFoundAllRequested |= bChildFound;

			// If SearchCount is positive and enough widgets are found
			// (OutWidgets.Num() >= SearchCount, evaluated during child
			// traversal), immediately return true.
			// The child won't return true if not enough widgets were found!
			else if (bChildFound)
				return true;
		}
	}
	return bFoundAllRequested;
}

bool FUMSlateHelpers::TraverseFindWidget(
	const TSharedRef<SWidget> BaseWidget,
	TSharedPtr<SWidget>&	  OutWidget,
	const uint64			  LookupWidgetId,
	const uint64			  IgnoreWidgetId,
	int32					  Depth)
{
	if (!BaseWidget->GetVisibility().IsVisible() || !BaseWidget->IsEnabled())
		return false;

	// LogTraversalSearch(Depth, BaseWidget);

	if (BaseWidget->GetId() == LookupWidgetId)
	{
		// LogTraverseFoundWidget(Depth, BaseWidget, "LookupId");
		OutWidget = BaseWidget;
		return true;
	}

	// Recursively traverse the children of the current widget
	FChildren* Children = BaseWidget->GetChildren();
	if (Children)
	{
		for (int32 i = 0; i < Children->Num(); ++i)
		{
			const TSharedRef<SWidget> Child = Children->GetChildAt(i);

			if (IgnoreWidgetId != INDEX_NONE && Child->GetId() == IgnoreWidgetId)
				continue;

			if (TraverseFindWidget(Child, OutWidget, LookupWidgetId,
					IgnoreWidgetId, Depth + 1))
				return true;
		}
	}
	return false;
}

bool FUMSlateHelpers::TraverseFindWidget(
	const TSharedRef<SWidget> BaseWidget,
	TSharedPtr<SWidget>&	  OutWidget,
	const TSet<FString>&	  TargetTypes,
	const uint64			  IgnoreWidgetId,
	int32					  Depth)
{
	if (!BaseWidget->GetVisibility().IsVisible() || !BaseWidget->IsEnabled())
		return false;

	// LogTraversalSearch(Depth, BaseWidget);

	if (TargetTypes.Contains(GetCleanWidgetType(BaseWidget->GetTypeAsString()))
		|| TargetTypes.Contains(GetCleanWidgetType(
			BaseWidget->GetWidgetClass().GetWidgetType().ToString())))
	{
		// LogTraverseFoundWidget(Depth, BaseWidget, BaseWidget->GetTypeAsString());
		OutWidget = BaseWidget;
		return true;
	}

	// Recursively traverse the children of the current widget
	FChildren* Children = BaseWidget->GetChildren();
	if (Children)
	{
		for (int32 i = 0; i < Children->Num(); ++i)
		{
			const TSharedRef<SWidget> Child = Children->GetChildAt(i);

			if (IgnoreWidgetId != INDEX_NONE && Child->GetId() == IgnoreWidgetId)
				continue;

			if (TraverseFindWidget(Child, OutWidget, TargetTypes,
					IgnoreWidgetId, Depth + 1))
				return true;
		}
	}
	return false;
}

bool FUMSlateHelpers::TraverseFindWidget(
	const TSharedRef<SWidget> BaseWidget,
	TSharedPtr<SWidget>&	  OutWidget,
	const TArray<FString>&	  TargetTypes,
	const uint64			  IgnoreWidgetId,
	int32					  Depth)
{
	if (!BaseWidget->GetVisibility().IsVisible() || !BaseWidget->IsEnabled())
		return false;

	// LogTraversalSearch(Depth, BaseWidget);

	for (const auto& Type : TargetTypes)
	{
		if (BaseWidget->GetTypeAsString().StartsWith(Type)
			|| BaseWidget->GetWidgetClass().GetWidgetType().ToString().StartsWith(Type))
		{
			// LogTraverseFoundWidget(Depth, BaseWidget, TargetType);
			OutWidget = BaseWidget;
			return true;
		}
	}

	// Recursively traverse the children of the current widget
	FChildren* Children = BaseWidget->GetChildren();
	if (Children)
	{
		for (int32 i = 0; i < Children->Num(); ++i)
		{
			const TSharedRef<SWidget> Child = Children->GetChildAt(i);

			if (IgnoreWidgetId != INDEX_NONE && Child->GetId() == IgnoreWidgetId)
				continue;

			if (TraverseFindWidget(Child, OutWidget, TargetTypes,
					IgnoreWidgetId, Depth + 1))
				return true;
		}
	}
	return false;
}

// TODO: Not sure if this is completely solid. I got some weird results
// using this function
bool FUMSlateHelpers::TraverseFindWidgetUpwards(
	const TSharedRef<SWidget> BaseWidget,
	TSharedPtr<SWidget>&	  OutWidget,
	const FString&			  TargetType,
	const bool				  bTraverseDownOnceBeforeUp)
{
	if (bTraverseDownOnceBeforeUp) // Collect Widgets Downwards
	{
		if (TraverseFindWidget(BaseWidget, OutWidget, TargetType))
			return true;
	}

	uint64				IgnoreWidgetId = BaseWidget->GetId();
	TSharedPtr<SWidget> Parent = BaseWidget->GetParentWidget();

	while (Parent.IsValid()) // Collect Widgets Upwards
	{
		if (TraverseFindWidget(Parent.ToSharedRef(), OutWidget,
				TargetType, IgnoreWidgetId))
			return true;

		// Update the ignore parent ID, as we've just traversed all its children.
		IgnoreWidgetId = Parent->GetId();
		Parent = Parent->GetParentWidget(); // Climb up to the next parent
	}
	return false;
}

bool FUMSlateHelpers::TraverseFindWidgetUpwards(
	const TSharedRef<SWidget> BaseWidget,
	TSharedPtr<SWidget>&	  OutWidget,
	const TArray<FString>&	  TargetTypes,
	const bool				  bTraverseDownOnceBeforeUp)
{
	if (bTraverseDownOnceBeforeUp) // Collect Widgets Downwards
	{
		if (TraverseFindWidget(BaseWidget, OutWidget, TargetTypes))
			return true;
	}

	uint64				IgnoreWidgetId = BaseWidget->GetId();
	TSharedPtr<SWidget> Parent = BaseWidget->GetParentWidget();

	while (Parent.IsValid()) // Collect Widgets Upwards
	{
		if (TraverseFindWidget(Parent.ToSharedRef(), OutWidget,
				TargetTypes, IgnoreWidgetId))
			return true;

		// Update the ignore parent ID, as we've just traversed all its children.
		IgnoreWidgetId = Parent->GetId();
		Parent = Parent->GetParentWidget(); // Climb up to the next parent
	}
	return false;
}

// TODO: In the traverse we want to be able to filter-out the widgets that
// are out of the viewports visibility.
bool FUMSlateHelpers::TraverseFindWidgetUpwards(
	const TSharedRef<SWidget>	 BaseWidget,
	TArray<TSharedRef<SWidget>>& OutWidgets,
	const TSet<FString>&		 TargetTypes,
	const TArray<FString>&		 StartsWithTargetTypes,
	const bool					 bTraverseDownOnceBeforeUp)
{
	if (bTraverseDownOnceBeforeUp) // Collect Widgets Downwards
		TraverseFindWidget(BaseWidget, OutWidgets, TargetTypes);

	uint64				IgnoreWidgetId = BaseWidget->GetId();
	TSharedPtr<SWidget> Parent = BaseWidget->GetParentWidget();

	while (Parent.IsValid()) // Collect Widgets Upwards
	{
		TraverseFindWidget(Parent.ToSharedRef(), OutWidgets,
			TargetTypes, StartsWithTargetTypes, IgnoreWidgetId);

		// Update the ignore parent ID, as we've just traversed all its children.
		IgnoreWidgetId = Parent->GetId();
		Parent = Parent->GetParentWidget(); // Climb up to the next parent
	}
	return !OutWidgets.IsEmpty();
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

	TSharedPtr<SWidget> FoundWidget;
	if (!TraverseFindWidget(ActiveWin->GetContent(), FoundWidget, DockWellType))
		return false;

	if (FChildren* Tabs = FoundWidget->GetChildren())
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
	const TSharedRef<SWidget> ParentWidget,
	TWeakPtr<SWidget>& OutDockingTabStack, const ETabRole TabRole)
{
	static const FString DockType = "SDockingTabStack";
	const FString		 SplitterType = "SSplitter";

	// TODO: better method for getting the proper content for Nomad Tabs
	// const FString LookupType =
	// 	TabRole == ETabRole::NomadTab
	// 	? SplitterType
	// 	: DockType;

	TSharedPtr<SWidget> DockingTabStack;
	if (TraverseFindWidgetUpwards(ParentWidget, DockingTabStack, DockType))
	{
		OutDockingTabStack = DockingTabStack;
		return true;
	}
	return false;

	// Not sure what is better
	// TSharedPtr<SWidget> CursorWidget = ParentWidget->GetParentWidget();
	// if (CursorWidget.IsValid())
	// 	// while (!CursorWidget->GetTypeAsString().Equals(DockType))
	// 	while (!CursorWidget->GetTypeAsString().Equals(LookupType))
	// 	{
	// 		CursorWidget = CursorWidget->GetParentWidget();
	// 		if (!CursorWidget.IsValid())
	// 			return false;
	// 	}
	// OutDockingTabStack = CursorWidget;
	// return true;
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
	if (!FSlateApplication::Get().FindPathToWidget(ChildWidget, ChildWidgetPath))
		return false;

	// if (ParentTab->GetTabRole() == ETabRole::NomadTab)
	// {
	// 	TSharedPtr<SWidget> _;
	// 	if (TraverseFindWidget(ParentTab->GetContent(), _, ChildWidget->GetId()))
	// 	{
	// 		LogWidgetResidesInTab(ParentTab, ChildWidget, true);
	// 		return true;
	// 	}
	// }
	// else if (ChildWidgetPath.ContainsWidget(&ParentTab->GetContent().Get()))
	if (ChildWidgetPath.ContainsWidget(&ParentTab->GetContent().Get()))
	{
		LogWidgetResidesInTab(ParentTab, ChildWidget, true);
		return true;
	}

	LogWidgetResidesInTab(ParentTab, ChildWidget, false);
	return false;
}

void FUMSlateHelpers::LogWidgetResidesInTab(const TSharedRef<SDockTab> ParentTab, const TSharedRef<SWidget> ChildWidget, bool bDoesWidgetResidesInTab)
{
	const bool bVisLog = true;
	if (bDoesWidgetResidesInTab)
		Logger.Print(
			FString::Printf(
				TEXT("Tab %s contain Widget %s"),
				*ParentTab->GetTabLabel().ToString(),
				*ChildWidget->GetTypeAsString()),
			ELogVerbosity::Verbose, bVisLog);
	else
		Logger.Print(
			FString::Printf(
				TEXT("Tab %s does NOT contain Widget %s, ID: %d"),
				*ParentTab->GetTabLabel().ToString(),
				*ChildWidget->GetTypeAsString(),
				ChildWidget->GetId()),
			ELogVerbosity::Warning, bVisLog);
}

TSharedPtr<SDockTab> FUMSlateHelpers::GetActiveMajorTab()
{
	// Going for the defacto first, as this will be more solid most of the times.
	if (const TSharedPtr<SDockTab> DefactoMajorTab = GetDefactoMajorTab())
		return DefactoMajorTab;

	// In some cases, this method will still be a good fallback (for example
	// when there's currently an invalid widget focused) so it's good to have.
	// Sadly this method just isn't picking up all types of Major Tabs (AFAIK)
	// so we start with the more expensive GetDefactoMajorTab() method
	return GetOfficialMajorTab();

	// Potentially a cheaper method? (I think not though :/)
	// const TSharedPtr<SDockTab> OfficialMajorTab = GetOfficialMajorTab();
	// if (!OfficialMajorTab->IsForeground()
	// 	|| !OfficialMajorTab->IsActive()
	// 	|| !OfficialMajorTab->GetContent()->HasAnyUserFocusOrFocusedDescendants())
	// {
	// 	Logger.Print("Try Get Defacto Major Tab.", ELogVerbosity::Log, true);
	// 	if (const TSharedPtr<SDockTab> DefactoMajorTab = GetDefactoMajorTab())
	// 		return DefactoMajorTab;
	// }
	// Logger.Print("Got Official Major Tab.", ELogVerbosity::Log, true);
	// return OfficialMajorTab;
}

TSharedPtr<SDockTab> FUMSlateHelpers::GetOfficialMajorTab()
{
	TSharedRef<FGlobalTabmanager> GTM = FGlobalTabmanager::Get();
	if (const TSharedPtr<SDockTab> ActiveMinorTab = GTM->GetActiveTab())
	{
		// LogTab(ActiveMinorTab.ToSharedRef());
		if (const TSharedPtr<FTabManager> TabManager =
				ActiveMinorTab->GetTabManagerPtr())
		{
			if (const TSharedPtr<SDockTab> ActiveMajorTab =
					GTM->GetMajorTabForTabManager(TabManager.ToSharedRef()))
			{
				// LogTab(ActiveMajorTab.ToSharedRef());
				return ActiveMajorTab;
			}
		}
	}
	return nullptr;
}

/**
 * NOTE:
 * This method is a bit more expensive but way more robust to pick up all
 * types of Major Tabs (AFAIK)
 * Possibly we can fallback to this more expensive method after checking if
 * our fetched Major Tab doesn't reside in our actual Active Window (which
 * is a strong sign we haven't picked up the correct Major Tab) but not
 * sure if it is that more efficient as this isn't too expensive anyway?
 */
TSharedPtr<SDockTab> FUMSlateHelpers::GetDefactoMajorTab()
{
	// Find the first TabWell in our currently active window.
	if (const TSharedPtr<SWidget> TabWell = GetActiveWindowTabWell())
		return GetForegroundTabInTabWell(TabWell.ToSharedRef());
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

TSharedPtr<SWidget> FUMSlateHelpers::GetActiveWindowTabWell()
{
	FSlateApplication&		  SlateApp = FSlateApplication::Get();
	const TSharedPtr<SWindow> ActiveWin = SlateApp.GetActiveTopLevelRegularWindow();
	if (!ActiveWin.IsValid())
		return nullptr;

	// Find the first TabWell in our currently active window.
	TSharedPtr<SWidget> TargetTabWell;
	if (!TraverseFindWidget(ActiveWin.ToSharedRef(), TargetTabWell, TabWellType))
		return nullptr;
	return TargetTabWell;
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
	// Check if InWindow is already the active one. If so, we really don't want
	// to Draw Attention to it again, is it might distrupt the currently focused
	// widget within it.
	const TSharedPtr<SWindow> ActiveWindow =
		FSlateApplication::Get().GetActiveTopLevelRegularWindow();
	if (ActiveWindow.IsValid() && ActiveWindow->GetId() == InWindow->GetId())
		return;

	// This method won't pickup focused inner windows (like menu windows) so it
	// only partially works.
	// TSharedRef<SWidget>			   WinContent = InWindow->GetContent();
	// if (InWindow->IsActive() || InWindow->HasAnyUserFocusOrFocusedDescendants() || WinContent->HasAnyUserFocusOrFocusedDescendants())
	// {
	// 	Logger.Print("Window has focus", ELogVerbosity::Verbose, true);
	// }

	FWindowDrawAttentionParameters DrawParams(
		EWindowDrawAttentionRequestType::UntilActivated);

	InWindow->BringToFront(true);
	InWindow->DrawAttention(DrawParams); // Seems to work well!

	Logger.Print(FString::Printf(TEXT("Activated Window: %s"),
					 *InWindow->GetTitle().ToString()),
		ELogVerbosity::Log, true);
}

FVector2f FUMSlateHelpers::GetWidgetCenterScreenSpacePosition(
	const TSharedRef<SWidget> InWidget)
{
	const FGeometry& WidgetGeometry = InWidget->GetCachedGeometry();
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
	const FVector2f LocalTopRight = FVector2f(WidgetGeometry.GetLocalSize().X, 0.0f);

	// Convert local position to absolute screen space
	const FVector2f AbsoluteTopRight = WidgetGeometry.LocalToAbsolute(LocalTopRight);

	// Adjust the position to stay within the bounds
	// return AbsoluteTopRight - FVector2D(Adjustment, Adjustment);
	return AbsoluteTopRight + Offset;
}

FVector2f FUMSlateHelpers::GetWidgetTopLeftScreenSpacePosition(
	const TSharedRef<SWidget> InWidget,
	const FVector2f			  Offset)
{
	// Get the cached geometry
	const FGeometry& WidgetGeometry = InWidget->GetCachedGeometry();

	// Convert local position to absolute screen space
	const FVector2f AbsoluteTopLeft = WidgetGeometry.LocalToAbsolute(FVector2D(0.f, 0.f));

	// Adjust the position to stay within the bounds
	return AbsoluteTopLeft + Offset;
}

FVector2f FUMSlateHelpers::GetWidgetCenterRightScreenSpacePosition(
	const TSharedRef<SWidget> InWidget,
	const FVector2f			  Offset)
{
	// Get the cached geometry
	const FGeometry& WidgetGeometry = InWidget->GetCachedGeometry();

	// Calculate the local center-right position
	const FVector2f LocalCenterRight =
		FVector2f(WidgetGeometry.GetLocalSize().X,
			WidgetGeometry.GetLocalSize().Y * 0.5f);

	// Convert local position to absolute screen space
	const FVector2f AbsoluteCenterRight = WidgetGeometry.LocalToAbsolute(LocalCenterRight);

	// Adjust the position by the offset
	return AbsoluteCenterRight + Offset;
}

FVector2D FUMSlateHelpers::GetWidgetLocalPositionInWindow(
	const TSharedRef<SWidget>& InWidget,
	const TSharedRef<SWindow>& InWindow)
{
	const FGeometry& WidgetGeo = InWidget->GetCachedGeometry();
	const FGeometry	 WindowGeo = InWindow->GetWindowGeometryInScreen();
	const FVector2D	 WidgetScreen = WidgetGeo.LocalToAbsolute(FVector2D::ZeroVector);
	const FVector2D	 WindowScreen = WindowGeo.GetAbsolutePosition();
	const float		 WindowScale = WindowGeo.Scale;

	return (WidgetScreen - WindowScreen) / WindowScale;
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
	const TSharedRef<SWidget> ParentWidget,
	const TArray<FString>&	  TargetEntries,
	int32					  ArrayIndex)
{
	static const FString TextBlockType = "STextBlock";
	static const FString ButtonType = "SButton";

	if (!TargetEntries.IsValidIndex(ArrayIndex))
		return;

	// Get all Text Blocks in the target parent to look for the the entries.
	TArray<TSharedPtr<SWidget>> TargetTextBlocks;
	if (!TraverseFindWidget(ParentWidget, TargetTextBlocks, TextBlockType))
		return;

	TSharedPtr<SButton> TargetButton;

	for (const TSharedPtr<SWidget>& Text : TargetTextBlocks)
	{
		if (!Text.IsValid())
			continue;

		const TSharedPtr<STextBlock> AsTextBlock =
			StaticCastSharedPtr<STextBlock>(Text);

		// Check if it matches the target entry provided
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
	const TArray<FString> TargetEntries, const int32 ArrayIndex)
{
	const FSlateApplication&  SlateApp = FSlateApplication::Get();
	const TSharedPtr<SWindow> MenuWindow = SlateApp.GetActiveTopLevelWindow();
	if (!MenuWindow.IsValid() || MenuWindow->GetType() != EWindowType::Menu)
		return; // We're specifcally targeting only Menu Windows (non-regular)

	SimulateMenuClicks(MenuWindow.ToSharedRef(), TargetEntries, ArrayIndex);
}

TSharedPtr<SDockTab> FUMSlateHelpers::GetForegroundTabInTabWell(
	const TSharedRef<SWidget> InTabWell)
{
	static const FString TabType = "SDockTab";

	FChildren* Tabs = InTabWell->GetChildren();
	if (!Tabs)
		return nullptr;

	// Find and return the first Foregrounded Tab inside that TabWell.
	const int32 TNum = Tabs->Num();
	for (int32 i{ 0 }; i < TNum; ++i)
	{
		const TSharedRef<SWidget> TabAsWidget = Tabs->GetChildAt(i);
		if (!TabAsWidget->GetTypeAsString().Equals(TabType))
			continue;

		const TSharedRef<SDockTab> DockTab =
			StaticCastSharedRef<SDockTab>(TabAsWidget);

		if (DockTab->IsForeground())
			return DockTab;
	}
	return nullptr;
}

TSharedPtr<SDockTab> FUMSlateHelpers::GetLastTabInTabWell(
	const TSharedRef<SWidget> InTabWell)
{
	static const FString TabType = "SDockTab";

	FChildren* Tabs = InTabWell->GetChildren();
	if (!Tabs)
		return nullptr;

	const int32 TNum = Tabs->Num();
	if (TNum > 0)
	{
		const TSharedRef<SWidget> TabAsWidget = Tabs->GetChildAt(TNum - 1);
		if (TabAsWidget->GetTypeAsString().Equals(TabType))
			return StaticCastSharedRef<SDockTab>(TabAsWidget);
	}
	return nullptr;
}

void FUMSlateHelpers::LogTab(const TSharedRef<SDockTab> InTab)
{
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

	FString VisualRole = GetRoleString(InTab->GetVisualTabRole());
	FString RegularRole = GetRoleString(InTab->GetTabRole());
	FString Name = InTab->GetTabLabel().ToString();
	int32	Id = InTab->GetId();
	FString LayoutId = InTab->GetLayoutIdentifier().ToString();
	bool	bIsToolkit = LayoutId.EndsWith(TEXT("Toolkit"));
	bool	bIsTabPersistable = InTab->GetLayoutIdentifier().IsTabPersistable();
	FString TabTypeAsString = InTab->GetLayoutIdentifier().TabType.ToString();

	// Log Parent Window
	const TSharedPtr<SWindow> TabWin = InTab->GetParentWindow();
	const FString			  WinName = TabWin.IsValid()
					? TabWin->GetTitle().ToString()
					: "Invalid Parent Window";
	// const FString			  WinType = TabWin.IsValid()
	// 				? "Invalid Parent Window"
	// 				: TabWin->GetTitle().ToString();

	FString DebugMsg = FString::Printf(
		TEXT("InTab Details:\n"
			 "  Name: %s\n"
			 "  Visual Role: %s\n"
			 "  Regular Role: %s\n"
			 "  Id: %d\n"
			 "  Layout ID: %s\n"
			 "  Is Toolkit: %s\n"
			 "  Is Tab Persistable: %s\n"
			 "  TabType: %s\n"
			 "  Parent Window: %s"),
		*Name,
		*VisualRole,
		*RegularRole,
		Id,
		*LayoutId,
		bIsToolkit ? TEXT("true") : TEXT("false"),
		bIsTabPersistable ? TEXT("true") : TEXT("false"),
		*TabTypeAsString,
		*WinName);

	Logger.Print(DebugMsg, ELogVerbosity::Log, true);
}

void FUMSlateHelpers::LogTraversalSearch(const int32 Depth, const TSharedRef<SWidget> CurrWidget)
{
	Logger.Print(FString::Printf(TEXT("%s[Depth: %d] Checking widget: %s ID: %d"),
		*FString::ChrN(Depth * 2, ' '), // Visual indentation
		Depth, *CurrWidget->GetTypeAsString(), CurrWidget->GetId()));
}

void FUMSlateHelpers::LogTraverseFoundWidget(const int32 Depth, const TSharedRef<SWidget> CurrWidget, const FString& TargetType)
{
	Logger.Print(FString::Printf(
					 TEXT("%s[Depth: %d] Found %s | Origin Target: %s"),
					 *FString::ChrN(Depth * 2, ' '), Depth, *CurrWidget->ToString(),
					 *TargetType),
		ELogVerbosity::Verbose, true);
}

bool FUMSlateHelpers::IsNomadWindow(const TSharedRef<SWindow> InWindow)
{
	static const FString TitleBarType = "SWindowTitleBar";
	static const FString MultiBoxType = "SMultiBoxWidget";

	TSharedPtr<SWidget> TitleBar;
	TSharedPtr<SWidget> MultiBox;
	return !(TraverseFindWidget(InWindow, TitleBar, TitleBarType)
		&& TraverseFindWidget(TitleBar.ToSharedRef(), MultiBox, MultiBoxType));
}

bool FUMSlateHelpers::CheckReplaceIfWindowChanged(
	const TSharedPtr<SWindow> CurrWin,
	TSharedPtr<SWindow>&	  OutNewWinIfChanged)
{
	FSlateApplication& SlateApp = FSlateApplication::Get();

	TSharedPtr<SWindow> SysActiveWin = SlateApp.GetActiveTopLevelRegularWindow();
	if (!SysActiveWin.IsValid())
		return false;

	// Window hasn't changed
	// if (SysActiveWin->GetTitle().EqualTo(InTrackedWindow->GetTitle()))
	if (CurrWin.IsValid()
		&& CurrWin->GetId() == SysActiveWin->GetId())
		return false;

	OutNewWinIfChanged = SysActiveWin;
	return true;
}

bool FUMSlateHelpers::IsCurrentlyActiveTabNomad()
{
	if (const TSharedPtr<SDockTab> MajorTab = GetActiveMajorTab())
		return MajorTab->GetTabRole() == ETabRole::NomadTab;
	return false;
}

TSharedPtr<SWidget> FUMSlateHelpers::GetAssociatedParentSplitterChild(
	const TSharedRef<SWidget> InWidget)
{
	static const FString SplitterType = "SSplitter";
	TSharedPtr<SWidget>	 Parent = InWidget->GetParentWidget();
	TSharedPtr<SWidget>	 Child = InWidget;

	while (Parent.IsValid())
	{
		if (Parent->GetTypeAsString().Equals(SplitterType))
			return Child;

		Child = Parent;
		Parent = Parent->GetParentWidget();
	}
	return nullptr;
}

FString FUMSlateHelpers::GetCleanTabLabel(const TSharedRef<SDockTab> InTab)
{
	FString TabLabel = InTab->GetTabLabel().ToString().TrimEnd();

	// Clean-up any trailing digits
	while (!TabLabel.IsEmpty() && FChar::IsDigit(TabLabel[TabLabel.Len() - 1]))
		TabLabel.RemoveAt(TabLabel.Len() - 1);

	return TabLabel.TrimEnd(); // Clean-up any trailing whitespaces
}

FString FUMSlateHelpers::GetCleanWidgetType(const FString& InWidgetType)
{
	const int32 TemplatePos = InWidgetType.Find("<");
	return TemplatePos != INDEX_NONE ? InWidgetType.Left(TemplatePos) : InWidgetType;
}

void FUMSlateHelpers::DebugClimbUpFromWidget(
	const TSharedRef<SWidget> InWidget, int32 TimesToClimb)
{
	TSharedPtr<SWidget> Parent = InWidget->GetParentWidget();
	int32				ClimbCount = 0;

	while (Parent.IsValid()
		&& (TimesToClimb == INDEX_NONE || ClimbCount < TimesToClimb))
	{
		const FString ClassType = Parent->GetWidgetClass().GetWidgetType().ToString();
		const FString SpecificType = Parent->GetTypeAsString();

		Logger.Print(FString::Printf(TEXT("Class Type: %s | Specific Type: %s"),
			*ClassType, *SpecificType));

		Parent = Parent->GetParentWidget();
		++ClimbCount;
	}
}

const TSet<FString>& FUMSlateHelpers::GetInteractableWidgetTypes()
{
	static const TSet<FString> InteractableWidgetTypes = {
		"SButton",
		"SHyperlink",
		"SCheckBox",
		"SFilterCheckBox",
		"SDockTab",
		"SEditableText",
		"SMultiLineEditableText",
		"SChordEditor",
		// "SGraphNode",
		// "SNode",  // Need to check the base class for this
		// "SGraphNodeK2Event",  // Might need a more robust way to actually
		// set focus on found nodes
		"SAssetListViewRow",
		"SAssetTileView",
		"SSceneOutlinerTreeView",
		"STreeView",
		"SSubobjectEditorDragDropTree",
		// "SSingleProperty",
		// "DetailPropertyRow",
		// "SDetailSingleItemRow",
		"SPropertyValueWidget",

		// "SNumericEntryBox<NumericType>",
		"SNumericEntryBox",
		"SSpinBox",
		"SColorBlock",

		// "SPropertyNameWidget",
		// "SPropertyEditorTitle",
		// "SEditConditionWidget",
		// "SPropertyEditorNumeric<float>",
		// "SNumericEntryBox<NumericType>",

		// Takes care of all nodes & all pins?!
		// "SLevelOfDetailBranchNode",

		// ~ SGraphPin types ~  //
		// Kismet Pins:
		"SGraphPin", // Found in the wild
		"SGraphPinBool",
		"SGraphPinClass",
		"SGraphPinCollisionProfile",
		"SGraphPinColor",
		"SGraphPinDataTableRowName",
		"SGraphPinEnum",
		"SGraphPinExec",
		"SGraphPinIndex",
		"SGraphPinInteger",
		"SGraphPinIntegerSlider",
		"SGraphPinKey",
		"SGraphPinNameList",
		"SGraphPinObject",
		"SGraphPinString",
		"SGraphPinStruct",
		"SGraphPinStructInstance",
		"SGraphPinText",
		// "SNameComboBox", // Also part of pins?
		// Material Pins:
		"SGraphPinMaterialInput",

		// More encounters:
		"SGraphPinNum", // < SGraphPinNum<double> is cleaned-up to <

		// ~ SGraphPin types ~  //

		// ~ SGraphNode types ~  //
		// Kismet Nodes:
		"SGraphNode",
		"SGraphNodeK2Base",
		"SGraphNodeK2Composite",
		"SGraphNodeK2Copy",
		"SGraphNodeK2CreateDelegate",
		"SGraphNodeK2Default",
		"SGraphNodeK2Event",
		"SGraphNodeK2Sequence",
		"SGraphNodeK2Terminator",
		"SGraphNodeK2Timeline",
		"SGraphNodeK2Var", // Doesn't have SLevelOfDetailBranchNode

		"SGraphNodeMakeStruct",
		"SGraphNodeSpawnActor",
		"SGraphNodeSpawnActorFromClass",
		"SGraphNodeSwitchStatement",
		// Material Nodes:
		"SGraphNodeMaterialBase",
		"SGraphNodeMaterialComment",
		"SGraphNodeMaterialComposite",
		"SGraphNodeMaterialCustom",
		"SGraphNodeMaterialResult",
		"SGraphSubstrateMaterial",
		// Misc:
		"SGraphNodePromotableOperator", // Doesn't have SLevelOfDetailBranchNode
		"SGraphNodeCreateWidget",		// Appeared in the graph one day *~*

		// ~ SGraphNode types ~  //

		// ~ Test Table Rows ~ //
		// "STableRow<TSharedPtr<IMoviePipelineSettingTreeItem>>",
		// "STableRow< TSharedPtr<FTreeItem> >",

	};

	return InteractableWidgetTypes;
}

const TArray<FString>& FUMSlateHelpers::GetStartsWithInteractableWidgetTypes()
{
	static const TArray<FString> StartsWithInteractableWidgetTypes = {
		"STableRow",
		// TBC
	};
	return StartsWithInteractableWidgetTypes;
}

bool FUMSlateHelpers::DoesWidgetResidesInRegularWindow(FSlateApplication& SlateApp, const TSharedRef<SWidget> InWidget)
{
	const TSharedPtr<SWindow> ParentWindow = SlateApp.FindWidgetWindow(InWidget);
	return (ParentWindow.IsValid() && ParentWindow->IsRegularWindow());
}

TSharedPtr<SGraphPanel> FUMSlateHelpers::TryGetActiveGraphPanel(
	FSlateApplication& SlateApp)
{
	static const FString PanelType = "SGraphPanel";
	TSharedPtr<SDockTab> ActiveMinorTab = GetActiveMinorTab();
	if (!ActiveMinorTab.IsValid())
		return nullptr;

	TSharedPtr<SWidget> FocusedWidget = SlateApp.GetUserFocusedWidget(0);
	if (!FocusedWidget.IsValid() || !FocusedWidget->GetTypeAsString().Equals(PanelType))
		return nullptr;

	if (!DoesWidgetResideInTab(ActiveMinorTab.ToSharedRef(), FocusedWidget.ToSharedRef()))
	{
		if (!TraverseFindWidget(ActiveMinorTab->GetContent(), FocusedWidget, PanelType))
			return nullptr;

		SlateApp.SetAllUserFocus(FocusedWidget, EFocusCause::Navigation);
	}

	return StaticCastSharedPtr<SGraphPanel>(FocusedWidget);
}

bool FUMSlateHelpers::IsWidgetTargetType(
	const TSharedRef<SWidget> InWidget, const FString& TargetType, bool bSearchStartsWith)
{
	return bSearchStartsWith
		? (InWidget->GetTypeAsString().StartsWith(TargetType)
			  || InWidget->GetWidgetClass().GetWidgetType().ToString().StartsWith(TargetType))

		: (InWidget->GetTypeAsString().Equals(TargetType)
			  || InWidget->GetWidgetClass().GetWidgetType().ToString().Equals(TargetType));
}

bool FUMSlateHelpers::IsWidgetTargetType(const TSharedRef<SWidget> InWidget, const TArray<FString>& TargetTypes, bool bSearchStartsWith)
{
	const FString InWidgetType = InWidget->GetTypeAsString();
	const FString InWidgetClassType = InWidget->GetWidgetClass().GetWidgetType().ToString();

	if (bSearchStartsWith)
	{
		for (const auto& Type : TargetTypes)
		{
			if (InWidgetType.StartsWith(Type))
				return true;
			else if (InWidgetClassType.StartsWith(Type))
				return true;
		}
	}
	else
	{
		for (const auto& Type : TargetTypes)
		{
			if (InWidgetType.Equals(Type))
				return true;
			else if (InWidgetClassType.Equals(Type))
				return true;
		}
	}
	return false;
}

bool FUMSlateHelpers::IsLastTabInTabWell(const TSharedRef<SDockTab> InTab)
{
	TSharedPtr<SWidget> TabWell = InTab->GetParentWidget();
	if (!TabWell.IsValid() || !TabWell->GetType().IsEqual("SDockingTabWell"))
		return false;

	FChildren* Tabs = TabWell->GetChildren();
	if (!Tabs || Tabs->Num() == 0)
		return false;

	const int32			 TNum = Tabs->Num();
	TSharedRef<SDockTab> LastTab =
		StaticCastSharedRef<SDockTab>(Tabs->GetChildAt(TNum - 1));

	return LastTab->GetId() == InTab->GetId();
}
