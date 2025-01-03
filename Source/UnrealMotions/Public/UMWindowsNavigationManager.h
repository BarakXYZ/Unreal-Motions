#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/InputBindingManager.h"
#include "Framework/Docking/TabManager.h"
#include "UnrealMotions.h"

class FUMWindowsNavigationManager
{
public:
	FUMWindowsNavigationManager();
	~FUMWindowsNavigationManager();

	/**
	 * Returns the singleton instance of the Windows Navigation Manager.
	 * Creates a new instance if one doesn't exist.
	 * @return Reference to the UMWindowsNavigationManager singleton instance
	 */
	static FUMWindowsNavigationManager& Get();

	/**
	 * Checks if the Windows Navigation Manager singleton has been initialized.
	 * @return true if the manager has been initialized, false otherwise
	 */
	static bool IsInitialized();

	/**
	 * Registers keyboard shortcuts for window navigation in the main frame context.
	 * Sets up commands for cycling between windows and toggling the root window.
	 * @param MainFrameContext The binding context for the main frame where the commands will be registered
	 */
	void RegisterCycleWindowsNavigation(const TSharedPtr<FBindingContext>& MainFrameContext);

	/**
	 * Maps the window navigation commands to their respective functions in the command list.
	 * This includes next window, previous window, and root window toggle actions.
	 * @param CommandList The command list where the actions will be mapped
	 */
	void MapCycleWindowsNavigation(const TSharedRef<FUICommandList>& CommandList);

	/**
	 * Registers necessary Slate event handlers after the engine initialization.
	 * Sets up window focus tracking and other window-related event handlers.
	 * Called after FSlateApplication delegates PostEngineInit.
	 */
	void RegisterSlateEvents();

	/**
	 * Handles focus change events between windows and widgets.
	 * Used to track window focus changes that might not trigger TabManager delegates.
	 * @param FocusEvent The focus event that triggered the callback
	 * @param OldWidgetPath Path to the previously focused widget
	 * @param OldWidget The previously focused widget
	 * @param NewWidgetPath Path to the newly focused widget
	 * @param NewWidget The newly focused widget
	 */
	void OnFocusChanged(const FFocusEvent& FocusEvent, const FWeakWidgetPath& OldWidgetPath,
		const TSharedPtr<SWidget>& OldWidget, const FWidgetPath& NewWidgetPath,
		const TSharedPtr<SWidget>& NewWidget);

	/**
	 * Detects if the user has moved to a different window and updates tracking accordingly.
	 * Also adds new windows to the tracking system if they haven't been tracked before.
	 * @param bSetNewWindow If true, updates the current window reference when a new window is detected
	 * @return true if the user has moved to a new window, false otherwise
	 */
	bool HasUserMovedToNewWindow(bool bSetNewWindow = true);

	/**
	 * Activates the specified window, bringing it to the front and setting proper focus.
	 * Handles focus management, window ordering, and ensuring proper activation state.
	 * @param Window The window to activate
	 */
	void ActivateWindow(const TSharedRef<SWindow> Window);

	/**
	 * Cycles through non-root windows in the specified direction.
	 * Will activate the next/previous visible regular window in the sequence.
	 * @param bIsNextWindow If true, cycles to the next window; if false, cycles to the previous window
	 */
	void CycleNoneRootWindows(bool bIsNextWindow);

	/**
	 * Toggles between the root window and other windows.
	 * If non-root windows are visible, minimizes them and brings the root window to the foreground.
	 * If non-root windows are minimized, restores them.
	 */
	void ToggleRootWindow();

	/**
	 * Removes invalid or closed windows from the tracking system.
	 * Updates the tracking map and logs cleanup information.
	 * @param CleanupWindowsIds Array of window IDs to remove from tracking
	 */
	void CleanupInvalidWindows(TArray<uint64> CleanupWindowsIds);

	/**
	 * Retrieves the map of currently tracked windows.
	 * @return Constant reference to the map of window IDs to window pointers
	 */
	const TMap<uint64, TWeakPtr<SWindow>>& GetTrackedWindows();


private:
	static TSharedPtr<FUMWindowsNavigationManager> WindowsNavigationManager;

	const FName				  MainFrameContextName = TEXT("MainFrame");
	FUMOnUserMovedToNewWindow OnUserMovedToNewWindow;

	TSharedPtr<FUICommandInfo> CmdInfoCycleNextWindow{ nullptr };
	TSharedPtr<FUICommandInfo> CmdInfoCyclePrevWindow{ nullptr };
	TSharedPtr<FUICommandInfo> CmdInfoGotoRootWindow{ nullptr };

	TWeakPtr<SWindow>				CurrWin{ nullptr };
	TMap<uint64, TWeakPtr<SWindow>> TrackedWindows;
	bool							VisualLog{ false };
};
