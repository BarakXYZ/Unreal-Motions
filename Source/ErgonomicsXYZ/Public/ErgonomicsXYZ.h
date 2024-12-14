#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/InputBindingManager.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SWidget.h"

class FErgonomicsXYZModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/**
	 * Initializes keyboard shortcuts for tab navigation (number keys 0-9).
	 * @param bCtrl Use Control modifier (default: true)
	 * @param bAlt Use Alt modifier (default: false)
	 * @param bShift Use Shift modifier (default: true)
	 * @param bCmd Use Command modifier (default: false)
	 */
	void InitTabNavInputChords(
		bool bCtrl = true, bool bAlt = false,
		bool bShift = true, bool bCmd = false);

	/**
	 * Removes an existing key binding to allow remapping.
	 * @param InputBindingManager The manager handling input bindings
	 * @param Command The key chord to unmap from its current command
	 * @note Used to resolve conflicts, e.g., removing default Ctrl+1 before remapping it
	 */
	void RemoveDefaultCommand(FInputBindingManager& IBManager, FInputChord Cmd);

	/**
	 * Registers UI commands for tab navigation (last tab and tabs 1-9).
	 * @param MainFrameContext The binding context to register commands in
	 */
	void AddCommandsToList(TSharedPtr<FBindingContext> MainFrameContext);

	/**
	 * Maps registered tab navigation commands to their handlers.
	 * @param CommandList List to bind the tab navigation actions to
	 */
	void MapTabCommands(TSharedRef<FUICommandList>& CommandList);

	/**
	 * Command handler to focus a specific tab in the active window's tab well.
	 * @param TabIndex Index of tab to focus (1-based, 0 selects last tab)
	 * @see MapTabCommands For the keybinding configuration
	 */
	void OnMoveToTab(int32 TabIndex);

	/**
	 * Recursively searches a widget tree for a SDockingTabWell widget.
	 * @param TraverseWidget The root widget to start traversing from
	 * @param DockingTabWell Output parameter that will store the found SDockingTabWell widget
	 * @param Depth Current depth in the widget tree (used for logging)
	 * @return true if SDockingTabWell was found, false otherwise
	 */
	bool TraverseWidgetTree(
		const TSharedPtr<SWidget>& TraverseWidget,
		TSharedPtr<SWidget>&	   DockingTabWell,
		int32					   Depth = 0);

	/**
	 * Activates a specific tab within a SDockingTabWell widget.
	 * @param DockingTabWell The tab well containing the tabs
	 * @param TabIndex Index of tab to focus (1-based, 0 selects last tab)
	 */
	void FocusTab(const TSharedPtr<SWidget>& DockingTabWell, int TabIndex);

	// Members
public:
	/**
	 * Mentioned in editor preferences as "System-wide" category
	 * Another option is MainFrame
	 * The general workflow to find the specific naming though is just
	 * to live grep and look for something similar.
	 * If not using the very specific names, the engine won't be able to open at all.
	 */
	const FName MainFrameContextName = TEXT("MainFrame");

	TSharedPtr<class FUICommandInfo>   CommandInfoTab1 = nullptr;
	TArray<TSharedPtr<FUICommandInfo>> CommandInfoTabs = {
		nullptr, nullptr, nullptr, nullptr, nullptr,
		nullptr, nullptr, nullptr, nullptr, nullptr
	};

	TMap<FName, TSharedRef<SDockTab>> OpenTabs;
	TArray<FInputChord>				  TabChords;
};
