#include "Widgets/Accessibility/SlateCoreAccessibleWidgets.h"

#include "UMWidgetHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogUMWidgetHelpers, NoLogging, All); // Prod
// DEFINE_LOG_CATEGORY_STATIC(LogUMWidgetHelpers, Log, All); // Dev

bool FUMWidgetHelpers::TraverseWidgetTree(
	const TSharedPtr<SWidget>& ParentWidget,
	TArray<TWeakPtr<SWidget>>& OutWidgets,
	const FString& TargetType, int32 SearchCount, int32 Depth)
{
	// Log the current widget and depth
	UE_LOG(LogUMWidgetHelpers, Display,
		TEXT("%s[Depth: %d] Checking widget: %s"),
		*FString::ChrN(Depth * 2, ' '), // Visual indentation
		Depth, *ParentWidget->GetTypeAsString());

	bool bFoundAllRequested = false;

	if (ParentWidget->GetTypeAsString() == TargetType)
	{
		UE_LOG(LogUMWidgetHelpers, Display,
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

bool FUMWidgetHelpers::TraverseWidgetTree(
	const TSharedPtr<SWidget>& ParentWidget,
	TWeakPtr<SWidget>&		   OutWidget,
	const FString&			   TargetType,
	int32					   Depth)
{
	// Log the current widget and depth
	UE_LOG(LogUMWidgetHelpers, Display,
		TEXT("%s[Depth: %d] Checking widget: %s"),
		*FString::ChrN(Depth * 2, ' '), // Visual indentation
		Depth, *ParentWidget->GetTypeAsString());

	if (ParentWidget->GetTypeAsString() == TargetType)
	{
		UE_LOG(LogUMWidgetHelpers, Display,
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
