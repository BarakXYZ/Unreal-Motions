#pragma once

#include "CoreMinimal.h"
#include "Framework/SlateDelegates.h"
#include "Templates/SharedPointer.h"
#include "Widgets/SCompoundWidget.h"
#include "VimInputProcessor.h"

/**
 * Structure holding information about a specific Vim mode
 */
struct FVimModeInfo
{
	const FSlateBrush* Icon;
	FText			   DisplayText;
};

/**
 * Widget that displays the current Vim mode in the status bar
 */
class SUMStatusBarWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SUMStatusBarWidget) {}
	/** Event fired when the status bar button is clicked */
	SLATE_EVENT(FOnClicked, OnClicked)
	SLATE_END_ARGS()

	/** Constructs the widget */
	void Construct(const FArguments& InArgs);
	virtual ~SUMStatusBarWidget();

private:
	/** Gets both icon and text information for a given mode */
	FVimModeInfo GetVimModeInfo(EVimMode Mode) const;

	/** Gets the current status bar icon */
	const FSlateBrush* GetCurrentStatusBarIcon() const;

	/** Gets the status bar text */
	FText GetStatusBarText() const;

	/** Gets the tooltip text */
	FText GetStatusBarTooltip() const;

	/** Updates the status bar when Vim mode changes */
	void UpdateStatusBar(const EVimMode CurrentVimMode);

	/** Current Vim mode shared across instances */
	static EVimMode StaticCurrVimMode;

	/** UI Components */
	TSharedPtr<SImage>	   StatusBarImage;
	TSharedPtr<STextBlock> StatusBarText;
};
