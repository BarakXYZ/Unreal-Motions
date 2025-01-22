#pragma once

#include "CoreMinimal.h"
#include "Framework/Docking/TabManager.h"
#include "ISceneOutlinerTreeItem.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Views/STreeView.h"
#include "Widgets/SWidget.h"
#include "UMLogger.h"

class FUMSlateHelpers
{
public:
	FUMSlateHelpers(const FUMSlateHelpers&) = default;
	FUMSlateHelpers(FUMSlateHelpers&&) = default;
	FUMSlateHelpers& operator=(const FUMSlateHelpers&) = default;
	FUMSlateHelpers& operator=(FUMSlateHelpers&&) = default;
	/**
	 * Recursively searches a widget tree for a SDockingTabWell widget.
	 * @param ParentWidget The root widget to start traversing from
	 * @param OutWidget Output parameter that will store the found widgets
	 * @param TargetType Type of widget we're looking for (e.g. "SDockingTabWell")
	 * @param SearchCount How many instances of this widget type we will try to look for. -1 will result in trying to find all widgets of this type in the widget's tree. 0 will be ignored, 1 will find the first instance, then return.
	 * @param Depth Current depth in the widget tree (used for logging)
	 * @return true if OutWidgets >= SearchCount, or OutWidget > 0 if SearchCount is set to -1, false otherwise
	 */
	static bool TraverseWidgetTree(
		const TSharedRef<SWidget>	 ParentWidget,
		TArray<TSharedPtr<SWidget>>& OutWidgets,
		const FString&				 TargetType,
		int32						 SearchCount = -1,
		int32						 Depth = 0);

	/**
	 * Recursively searches a widget tree for a single widget of the specified type.
	 * @param ParentWidget The root widget to start traversing from
	 * @param OutWidget Output parameter that will store the found widget
	 * @param TargetType Type of widget we're looking for
	 * @param Depth Current depth in the widget tree (used for logging)
	 * @return true if a widget was found, false otherwise
	 */
	static bool TraverseWidgetTree(
		const TSharedRef<SWidget> ParentWidget,
		TSharedPtr<SWidget>&	  OutWidget,
		const FString&			  TargetType,
		const uint64			  IgnoreWidgetId = INDEX_NONE,
		int32					  Depth = 0);

	static bool TraverseWidgetTree(
		const TSharedRef<SWidget> ParentWidget,
		TSharedPtr<SWidget>&	  OutWidget,
		const uint64			  LookupWidgetId,
		const uint64			  IgnoreWidgetId = INDEX_NONE,
		int32					  Depth = 0);

	static TSharedPtr<SWidget> FindNearstWidgetType(
		const TSharedRef<SWidget> StartWidget,
		const FString&			  TargetType);

	static void LogTraversalSearch(const int32 Depth,
		const TSharedRef<SWidget>			   CurrWidget);
	static void LogTraverseFoundWidget(const int32 Depth,
		const TSharedRef<SWidget> CurrWidget, const FString& TargetType);

	static bool GetFrontmostForegroundedMajorTab(TSharedPtr<SDockTab>& OutMajorTab);

	static bool GetParentDockingTabStackAsWidget(
		const TSharedRef<SWidget> ParentWidget,
		TWeakPtr<SWidget>&		  OutDockingTabStack,
		const ETabRole			  TabRole = ETabRole::PanelTab);

	///////////////////////////////////////////////////////////////////////////
	//						~ List View Helpers ~
	//
	static bool IsValidTreeViewType(const FString& InWidgetType);

	static bool GetSelectedTreeViewItemAsWidget(
		FSlateApplication& SlateApp, TSharedPtr<SWidget>& OutWidget,
		const TOptional<TSharedPtr<SListView<TSharedPtr<ISceneOutlinerTreeItem>>>>& OptionalListView);

	/**
	 * @brief Retrieves a list view widget from the currently focused UI element.
	 *
	 * @details Validates that:
	 * 1. The focused widget is valid
	 * 2. The widget type is supported for navigation
	 * 3. The widget can be cast to appropriate list view type (static only)
	 *
	 * @param SlateApp Reference to the Slate application instance
	 * @param OutListView Output parameter containing the found list view widget
	 * @return true if a valid list view was found, false otherwise
	 */
	static bool TryGetListView(FSlateApplication& SlateApp, TSharedPtr<SListView<TSharedPtr<ISceneOutlinerTreeItem>>>& OutListView);

	/**
	 * @brief Updates tree view selection when exiting Visual mode.
	 *
	 * @details Handles the transition by:
	 * 1. Determining final selection based on navigation offset (and Anchor)
	 * 2. Clearing existing selection array
	 * 3. Setting the new single item selection
	 *
	 * @param SlateApp Reference to the Slate application instance
	 */
	static void UpdateTreeViewSelectionOnExitVisualMode(
		FSlateApplication& SlateApp, const TWeakPtr<ISceneOutlinerTreeItem>& AnchorItem);
	//
	//						~ List View Helpers ~
	///////////////////////////////////////////////////////////////////////////

	static bool IsValidEditableText(const FString& InWidgetType);

	static bool DoesTabResideInWindow(
		const TSharedRef<SWindow>  ParentWindow,
		const TSharedRef<SDockTab> ChildTab);

	static bool DoesWidgetResideInTab(
		const TSharedRef<SDockTab> ParentTab,
		const TSharedRef<SWidget>  ChildWidget);

	static TSharedPtr<SDockTab> GetActiveMajorTab();

	static TSharedPtr<SDockTab> GetDefactoMajorTab();

	static TSharedPtr<SDockTab> GetActiveMinorTab();

	static bool IsVisualTextSelected(FSlateApplication& SlateApp);

	/**
	 * Activates the specified window, bringing it to the front and setting proper focus.
	 * Handles focus management, window ordering, and ensuring proper activation state.
	 * @param Window The window to activate
	 */
	static void ActivateWindow(const TSharedRef<SWindow> InWindow);

	static FVector2f GetWidgetCenterScreenSpacePosition(
		const TSharedRef<SWidget> InWidget);

	static FVector2f GetWidgetTopRightScreenSpacePosition(
		const TSharedRef<SWidget> InWidget,
		const FVector2f			  Offset = FVector2f(0.0f, 0.0f));

	static FVector2f GetWidgetCenterRightScreenSpacePosition(
		const TSharedRef<SWidget> InWidget,
		const FVector2f			  Offset = FVector2f(0.0f, 0.0f));

	static TSharedPtr<FTabManager> GetLevelEditorTabManager();

	static TSharedPtr<SWidget> GetTabWellForTabManager(
		const TSharedRef<FTabManager> InTabManager);

	static TSharedPtr<FGenericWindow> GetGenericActiveTopLevelWindow();

	/**
	 * @Note we're passing the array by ref, as this is invoked immediately.
	 */
	static void SimulateMenuClicks(
		const TSharedRef<SWidget>		ParentWidget,
		const TArrayView<const FString> TargetEntries,
		int32							ArrayIndex = 0);

	/**
	 * @Note we're passing the array by value cause this is a delegate
	 *  and our Array will be invalid if passed by ref
	 */
	static void GetActiveMenuWindowAndCallSimulateMenuClicks(
		const TArrayView<const FString> TargetEntries, const int32 ArrayIndex);

	static void LogTab(const TSharedRef<SDockTab> InTab);

	static TSharedPtr<SDockTab> GetForegroundTabInTabWell(
		const TSharedRef<SWidget> InTabWell);

	static TSharedPtr<SDockTab> GetLastTabInTabWell(
		const TSharedRef<SWidget> InTabWell);

	/**
	 * If a window contains only Nomad Tabs, its TitleBar will be sort of
	 * shrinked. We need to know that for docking operations to determine our
	 * offset and dock properly.
	 * Currently this is being checked by look if the TitleBar contains the
	 * SMultiBoxWidget. This seems to be a strong indication if whether or not
	 * this is a "Nomad Window"
	 */
	static bool IsNomadWindow(const TSharedRef<SWindow> InWindow);

	static bool CheckReplaceIfWindowChanged(
		const TSharedPtr<SWindow> CurrWin,
		TSharedPtr<SWindow>&	  OutNewWinIfChanged);

	static void LogWidgetResidesInTab(const TSharedRef<SDockTab> ParentTab, const TSharedRef<SWidget> ChildWidget, bool bDoesWidgetResidesInTab);

	/** Will clear the old timer handle before starting a new one to mitigate
	 * potential loops */
	static void SetWidgetFocusWithDelay(const TSharedRef<SWidget> InWidget, FTimerHandle& TimerHandle, const float Delay, const bool bClearUserFocus);

	static bool IsCurrentlyActiveTabNomad();

	static TSharedPtr<SWidget> GetAssociatedParentSplitterChild(const TSharedRef<SWidget> InWidget);

	/** Clean Tab Label from digits and trailing whitespaces for generic lookups
	 *  For example Content Browser 3 == Content Browser
	 *  @param InTab The tab to clean the label for.
	 *  @return Clean Tab Label
	 */
	static FString GetCleanTabLabel(const TSharedRef<SDockTab> InTab);

	static FUMLogger Logger;
};
