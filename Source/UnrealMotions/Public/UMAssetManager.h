#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/InputBindingManager.h"
#include "Framework/Docking/TabManager.h"
#include "UnrealMotions.h"

class FUMAssetManager
{
public:
	FUMAssetManager();
	~FUMAssetManager();

	/**
	 * Returns the singleton instance of the Asset Manager.
	 * Creates a new instance if one doesn't exist.
	 * @return Reference to the UMAssetManager singleton instance
	 */
	static FUMAssetManager& Get();

	/**
	 * Checks if the Asset Manager singleton has been initialized.
	 * @return true if the manager has been initialized, false otherwise
	 */
	static bool IsInitialized();

	/**
	 * Registers keyboard shortcuts for window navigation in the main frame context.
	 * Sets up commands for cycling between windows and toggling the root window.
	 * @param MainFrameContext The binding context for the main frame where the commands will be registered
	 */
	void RegisterAssetCommands(
		const TSharedPtr<FBindingContext>& MainFrameContext);

	/**
	 * Maps the window navigation commands to their respective functions in the command list.
	 * This includes next window, previous window, and root window toggle actions.
	 * @param CommandList The command list where the actions will be mapped
	 */
	void MapAssetCommands(const TSharedRef<FUICommandList>& CommandList);

	void Call_RestorePreviouslyOpenAssets();

	void OnNotificationsWindowFocusLost();

	void ToggleNotificationVisualSelection(
		const TSharedRef<SNotificationItem> Item, const bool bToggleOn);

private:
	static TSharedPtr<FUMAssetManager> AssetManager;

public:
	TSharedPtr<FUICommandInfo>	CmdInfoOpenPreviouslyOpenAssets{ nullptr };
	bool						bIsNotificationFocused{ false };
	TWeakPtr<SWidget>			LastFocusedWidget = nullptr;
	TWeakPtr<SNotificationItem> LastFocusedNotificationItem = nullptr;
	const FName					MainFrameContextName = TEXT("MainFrame");
	bool						VisualLog{ false };
};
