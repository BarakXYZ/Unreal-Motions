#pragma once

#include "Framework/Application/SlateApplication.h"
#include "UMLogger.h"

class FUMEditorCommands
{
public:
	static void ClearAllDebugMessages();

	static void ToggleAllowNotifications();

	/**
	 * @brief Performs undo operation using standard editor shortcut.
	 *
	 * @details Simulates Ctrl+Z key combination:
	 * 1. Creates appropriate modifier state
	 * 2. Simulates key press event
	 * 3. Processes through Slate application
	 *
	 * @param SlateApp Reference to the Slate application instance
	 * @param InKeyEvent Original triggering key event
	 */
	static void Undo(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	/**
	 * @brief Opens the Output Log tab.
	 *
	 * @details Invokes and activates the Output Log tab using global tab manager
	 */
	static void OpenOutputLog();

	/**
	 * @brief Opens specified Content Browser tab.
	 *
	 * @details Manages Content Browser tab activation:
	 * 1. Determines target tab number (1-4)
	 * 2. Constructs appropriate tab identifier
	 * 3. Invokes and activates the tab
	 *
	 * @param SlateApp Reference to the Slate application instance
	 * @param InKeyEvent Key event containing potential tab number
	 */
	static void OpenContentBrowser(
		FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	static void OpenPreferences(
		FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	/**
	 * @brief Opens the Widget Reflector debugging tool and auto focus the debug
	 * button for convenience.
	 *
	 * @details This function performs the following operations:
	 * 1. Opens/activates the Widget Reflector tab
	 * 2. Traverses widget hierarchy to locate specific UI elements
	 * 3. Simulates user interactions to auto-focus the reflector
	 *
	 * @note The timer delay (150ms) may need adjustment on different platforms(?)
	 */
	static void OpenWidgetReflector(
		FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	static void FocusSearchBox(
		FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	static void FindNearestSearchBox(
		FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	/**
	 * ~ WIP ~
	 * @brief Deletes the currently selected item.
	 *
	 * @details Simulates delete key press:
	 * 1. Creates synthetic delete key event
	 * 2. Toggles native input handling
	 * 3. Processes delete operation
	 *
	 * @param SlateApp Reference to Slate application
	 * @param InKeyEvent Original triggering key event
	 */
	static void DeleteItem(
		FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	static void NavigateNextPrevious(
		FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	static void OpenLevelBlueprint();

	static void ResetEditorToDefaultLayout();

	static void RunUtilityWidget();

	static void Search(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	// Collect all focusable widgets (buttons, hyperlinks, lists, etc.)
	// Create a small widget for each of them with a unique 2 characters id.
	// This widget (or in collaboration with the VimProcessor) should listen to
	// the user input and detect when a unique 2 character combination is pressed.
	// Once executed, the widget that is bound to the 2 char id should be focused.
	// All widget dissapear.

	static FUMLogger Logger;
};
