#pragma once

#include "CoreMinimal.h"
#include "Framework/SlateDelegates.h"
#include "Templates/SharedPointer.h"
#include "Widgets/SCompoundWidget.h"

class FUnsavedAssetsTracker;

/**
 */
class SUMStatusBarWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SUMStatusBarWidget)
	{
	}
	/** Event fired when the status bar button is clicked */
	SLATE_EVENT(FOnClicked, OnClicked)
	SLATE_END_ARGS()

	/** Constructs the widget */
	void Construct(const FArguments& InArgs);

private:
	const FSlateBrush* GetStatusBarIcon() const;
	FText			   GetStatusBarText() const;
	FText			   GetStatusBarTooltip() const;
};
