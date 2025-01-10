#pragma once

#include "CoreMinimal.h"
#include "ISceneOutlinerTreeItem.h"
#include "Widgets/Views/STreeView.h"
#include "Widgets/SWidget.h"
#include "UMLogger.h"

class FUMSlateHelpers
{
public:
	FUMSlateHelpers();
	~FUMSlateHelpers();

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
		const TSharedPtr<SWidget>& ParentWidget,
		TArray<TWeakPtr<SWidget>>& OutWidgets,
		const FString&			   TargetType,
		int32					   SearchCount = -1,
		int32					   Depth = 0);

	/**
	 * Recursively searches a widget tree for a single widget of the specified type.
	 * @param ParentWidget The root widget to start traversing from
	 * @param OutWidget Output parameter that will store the found widget
	 * @param TargetType Type of widget we're looking for
	 * @param Depth Current depth in the widget tree (used for logging)
	 * @return true if a widget was found, false otherwise
	 */
	static bool TraverseWidgetTree(
		const TSharedPtr<SWidget>& ParentWidget,
		TWeakPtr<SWidget>&		   OutWidget,
		const FString&			   TargetType,
		int32					   Depth = 0);

	static bool GetFrontmostForegroundedMajorTab(TSharedPtr<SDockTab>& OutMajorTab);

	static bool GetParentDockingTabStackAsWidget(
		const TSharedRef<SWidget> ParentWidget,
		TWeakPtr<SWidget>&		  OutDockingTabStack);

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

	static const TSharedPtr<FUMSlateHelpers> SlateHelpers;

	FUMLogger Logger;
};
