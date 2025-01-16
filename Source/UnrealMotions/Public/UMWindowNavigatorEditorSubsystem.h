#pragma once

#include "Framework/Commands/UICommandInfo.h"
#include "Framework/Commands/UICommandList.h"
#include "Widgets/SWindow.h"
#include "UMLogger.h"
#include "EditorSubsystem.h"
#include "UMWindowNavigatorEditorSubsystem.generated.h"

DECLARE_MULTICAST_DELEGATE_TwoParams(FUMOnWindowChanged,
	const TSharedPtr<SWindow> PrevWindow, const TSharedPtr<SWindow> NewWindow);

/**
 *
 */
UCLASS()
class UNREALMOTIONS_API UUMWindowNavigatorEditorSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * Controlled via the Unreal Motions config; should or shouldn't create the
	 * subsystem at all.
	 */
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	/**
	 * @param Collection The subsystem collection being initialized
	 */
	virtual void Initialize(FSubsystemCollectionBase& Collction) override;

	virtual void Deinitialize() override;

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
	FUMLogger Logger;

	const FName MainFrameContextName = TEXT("MainFrame");

	TSharedPtr<FUICommandInfo> CmdInfoCycleNextWindow{ nullptr };
	TSharedPtr<FUICommandInfo> CmdInfoCyclePrevWindow{ nullptr };
	TSharedPtr<FUICommandInfo> CmdInfoGotoRootWindow{ nullptr };

	bool VisualLog{ true };
};
