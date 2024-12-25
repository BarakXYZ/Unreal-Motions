#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Widgets/SWidget.h"
#include "Widgets/Input/SButton.h"

class FUMWidgetHelpers
{
public:
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

	static TSharedPtr<SButton> FindSButtonUsingAccessibilityTraversal(
		TSharedRef<SWidget> InRootWidget);
};
