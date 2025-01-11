#pragma once

#include "Framework/Commands/UICommandInfo.h"
#include "Framework/Commands/UICommandList.h"
#include "Widgets/SWindow.h"
#include "UMLogger.h"

DECLARE_MULTICAST_DELEGATE_TwoParams(FUMOnWindowChanged,
	const TSharedPtr<SWindow> PrevWindow, const TSharedPtr<SWindow> NewWindow);

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
	void RegisterCycleWindowsNavigation(
		const TSharedPtr<FBindingContext>& MainFrameContext);

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
	 * Detects if the user has moved to a different window and updates tracking accordingly.
	 * Also adds new windows to the tracking system if they haven't been tracked before.
	 * @param bSetNewWindow If true, updates the current window reference when a new window is detected
	 * @return true if the user has moved to a new window, false otherwise
	 */
	bool HasUserMovedToNewWindow(bool bSetNewWindow = true);

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
	 * Retrieves the map of currently tracked windows.
	 * @return Constant reference to the map of window IDs to window pointers
	 */
	const TMap<uint64, TWeakPtr<SWindow>>& GetTrackedWindows();

	static FUMOnWindowChanged OnWindowChanged;

private:
	static TSharedPtr<FUMWindowsNavigationManager> WindowsNavigationManager;
	static FUMLogger							   Logger;

	const FName MainFrameContextName = TEXT("MainFrame");

	TSharedPtr<FUICommandInfo> CmdInfoCycleNextWindow{ nullptr };
	TSharedPtr<FUICommandInfo> CmdInfoCyclePrevWindow{ nullptr };
	TSharedPtr<FUICommandInfo> CmdInfoGotoRootWindow{ nullptr };

	bool VisualLog{ true };
};
