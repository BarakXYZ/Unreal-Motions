#pragma once

#include "CoreMinimal.h"

/**
 * A helper class to create and register a custom status-bar menu,
 * similar to how "Revision Control" does it.
 */
class FUMCustomStatusBar
{
public:
	/**
	 * Registers a new status-bar menu named "StatusBar.ToolBar.UnrealMotions"
	 * and inserts our custom widget (an SComboButton) in that menu.
	 */
	static void RegisterMenus();

private:
	/**
	 * Builds the pop-up menu content that appears when the status-bar button is clicked.
	 */
	static TSharedRef<SWidget> GenerateMyCustomMenuContent();

	/**
	 * Builds the status-bar button (SComboButton) itself.
	 */
	static TSharedRef<SWidget> MakeMyCustomStatusWidget();
};
