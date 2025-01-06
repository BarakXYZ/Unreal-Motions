#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/InputBindingManager.h"
#include "Framework/Docking/TabManager.h"
#include "UMFocusManager.h"

DECLARE_MULTICAST_DELEGATE_TwoParams(FUMOnNewMajorTabChanged,
	TWeakPtr<SDockTab> /* New Major Tab */,
	TWeakPtr<SDockTab> /* New Minor Tab */);

enum ENavSpecTabType : uint8
{
	LevelEditor,
	Toolkit,
	None,
};

class FUMTabsNavigationManager
{
public:
	FUMTabsNavigationManager();
	~FUMTabsNavigationManager();

	/**
	 * Returns the singleton instance of the Tabs Navigation Manager.
	 * Creates a new instance if one doesn't exist.
	 * @return Reference to the UMTabsNavigationManager singleton instance
	 */
	static FUMTabsNavigationManager& Get();

	/**
	 * Checks if the Tabs Navigation Manager singleton has been initialized.
	 * @return true if the manager has been initialized, false otherwise
	 */
	static bool IsInitialized();

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
	 * @note Used to resolve conflicts, e.g., removing default Ctrl+1 before remapping it. This is not the default goto of this plugin. The user will decide manually if he wants to change the default bindings that ships with Unreal.
	 */
	void RemoveDefaultCommand(FInputBindingManager& IBManager, FInputChord Cmd);

	/**
	 * Registers UI commands for Major Tab Navigation (last tab and tabs 1-9).
	 * @param MainFrameContext The binding context to register commands in
	 */
	void AddMajorTabsNavigationCommandsToList(
		TSharedPtr<FBindingContext> MainFrameContext);

	/**
	 * Registers UI commands for Minor Tab Navigation (last tab and tabs 1-9).
	 * @param MainFrameContext The binding context to register commands in
	 */
	void AddMinorTabsNavigationCommandsToList(
		TSharedPtr<FBindingContext> MainFrameContext);

	/**
	 * Maps registered tab navigation commands to their handlers.
	 * @param CommandList List to bind the tab navigation actions to
	 */
	void MapTabCommands(const TSharedRef<FUICommandList>& CommandList,
		const TArray<TSharedPtr<FUICommandInfo>>& CommandInfo, bool bIsMajorTab);

	/**
	 * Registers necessary Slate event handlers after the engine initialization.
	 * Start listening to OnActiveTabChanged, OnTabForegrounded & OnFocusChanging.
	 * Called after FSlateApplication delegates PostEngineInit.
	 */
	void RegisterSlateEvents();

	/**
	 * Being called when a new Minor Tab is being activated.
	 * Major tabs can also be deduced from this by fetching the TabManagerPtr
	 * and ->GetActiveMajorTab
	 * @param PrevActiveTab the currently set, old tab.
	 * @param NewActiveTab the newly to be set tab.
	 * @note Unreal incorrectly passes the internal delegate params in an inverted order. I've countered it with flipping the values again to compensate.
	 */
	void OnActiveTabChanged(
		TSharedPtr<SDockTab> PrevActiveTab, TSharedPtr<SDockTab> NewActiveTab);

	/**
	 * Being called when a new Minor or Major Tab is being foregrounded, though
	 * Major Tabs seems to be the main focus of this.
	 * We're using it as a second layer backup for setting the Major Tab
	 * in cases where OnActiveTabChanged won't be invoked.
	 * @param NewActiveTab the newly to be set tab.
	 * @param PrevActiveTab the currently set, old tab.
	 * @note In this case, Unreal passes the delegate params in the correctly documented order. So we keep it as is (firstly new tab, then old tab).
	 */
	void OnTabForegrounded(
		TSharedPtr<SDockTab> NewActiveTab, TSharedPtr<SDockTab> PrevActiveTab);

	/**
	 * Called when focus changes between widgets in the editor.
	 * @param FocusEvent Event data for the focus change
	 * @param OldWidgetPath Widget path before focus change
	 * @param OldWidget Previously focused widget
	 * @param NewWidgetPath Widget path after focus change
	 * @param NewWidget Newly focused widget
	 */
	void OnFocusChanged(
		const FFocusEvent& FocusEvent, const FWeakWidgetPath& OldWidgetPath,
		const TSharedPtr<SWidget>& OldWidget, const FWidgetPath& NewWidgetPath,
		const TSharedPtr<SWidget>& NewWidget);

	/**
	 * Command handler to focus a specific tab in the active window's tab well.
	 * @param TabIndex Index of tab to focus (1-based, 0 selects last tab)
	 * @param bIsMajorTab Dictates if we should move to a major or minor tab.
	 * @see MapTabCommands For the keybinding configuration
	 */
	void MoveToTabIndex(int32 TabIndex, bool bIsMajorTab);

	/**
	 * Activates a specific tab within a SDockingTabWell widget.
	 * @param DockingTabWell The tab well containing the tabs
	 * @param TabIndex Index of tab to focus (1-based, 0 selects last tab)
	 * @param bIsMajorTab Info to determine which tab we should set as the new one
	 */
	void FocusTabIndex(const TSharedPtr<SWidget>& DockingTabWell, int32 TabIndex, bool bIsMajorTab);

	/**
	 * Adds navigation commands for cycling between tabs to the binding context.
	 * @param MainFrameContext The binding context to register cycle commands in
	 */
	void RegisterCycleTabNavigation(const TSharedPtr<FBindingContext>& MainFrameContext);

	/**
	 * Maps the next/previous tab navigation commands to their respective actions.
	 * @param CommandList The command list to map the actions to
	 */
	void MapCycleTabsNavigation(const TSharedRef<FUICommandList>& CommandList);

	/**
	 * Cycles between tabs back and forth. Hotkeys can be held to continuously
	 * move between tabs repeatedly.
	 * @param bIsMajorTab Dictates if we should move to a major or minor tab.
	 * @param bIsNextTab Dictates if we should move to the next or previous tab.
	 */
	void CycleTabs(bool bIsMajorTab, bool bIsNextTab);

	/**
	 * Validates and sets the targeted tab (OutTab) to either the currently
	 * set Major Tab or currently set Minor Tab.
	 * @param OutTab Where the targeted tab will be stored at.
	 * @param bIsMajorTab Dictates if we should move to a major or minor tab.
	 */
	bool TryGetValidTargetTab(TSharedPtr<SDockTab>& OutTab, bool bIsMajorTab);

	/**
	 * Updates the currently tracked tab (major or minor).
	 * @param NewTab The tab to set as current
	 */
	void SetCurrentTab(const TSharedRef<SDockTab> NewTab);

	void LogTabChange(const FString& TabType,
		const TWeakPtr<SDockTab>& CurrentTab, const TSharedRef<SDockTab>& NewTab);

	bool FindActiveMinorTabs();

	/**
	 * Prints debug information about a tab.
	 * @param Tab Tab to debug
	 * @param bDebugVisualTabRole Whether to debug visual or regular tab role
	 * @param DelegateType String indicating which delegate triggered the debug
	 */
	void DebugTab(const TSharedPtr<SDockTab>& Tab,
		bool bDebugVisualTabRole, FString DelegateType);

	/**
	 * Called when mouse button is pressed down, can be used to track tab changes.
	 * Currently unused but kept for potential future use.
	 * @param PointerEvent Event data for the mouse button press
	 */
	void OnMouseButtonDown(const FPointerEvent& PointerEvent);

	/** DEPRECATED
	 * Return the last active non-major tab. Can be a Panel Tab, a Nomad Tab, etc.
	 * @param OutNonMajorTab Where we will store the found tab (if any).
	 */
	bool GetLastActiveNonMajorTab(TWeakPtr<SDockTab>& OutNonMajorTab);

	void HandleOnUserMovedToNewWindow(TWeakPtr<SWindow> NewWindow);

	static TWeakPtr<SDockTab> GetCurrentlySetMajorTab();
	static bool				  RemoveActiveMajorTab();

	/** DEPRECATED
	 * Gets the type of tab for navigation-specific purposes.
	 * Used to determine how to handle tab navigation for different tab types.
	 * @param Tab The tab to check the type of
	 * @return The navigation-specific tab type (LevelEditor, Toolkit, or None)
	 */
	ENavSpecTabType GetNavigationSpecificTabType(const TSharedPtr<SDockTab>& Tab);

private:
	static TSharedPtr<FUMTabsNavigationManager> TabsNavigationManager;

public:
	/**
	 * Mentioned in editor preferences as "System-wide" category
	 * Another option is MainFrame
	 * The general workflow to find the specific naming though is just
	 * to live grep and look for something similar.
	 * If not using the very specific names, the engine will crash on startup.
	 */
	const FName						   MainFrameContextName = TEXT("MainFrame");
	TArray<FInputChord>				   TabChords;
	TArray<TSharedPtr<FUICommandInfo>> CommandInfoMajorTabs = {
		nullptr, nullptr, nullptr, nullptr, nullptr,
		nullptr, nullptr, nullptr, nullptr, nullptr
	};
	TArray<TSharedPtr<FUICommandInfo>> CommandInfoMinorTabs = {
		nullptr, nullptr, nullptr, nullptr, nullptr,
		nullptr, nullptr, nullptr, nullptr, nullptr
	};

	TSharedPtr<FUICommandInfo> CmdInfoNextMajorTab{ nullptr };
	TSharedPtr<FUICommandInfo> CmdInfoPrevMajorTab{ nullptr };
	TSharedPtr<FUICommandInfo> CmdInfoNextMinorTab{ nullptr };
	TSharedPtr<FUICommandInfo> CmdInfoPrevMinorTab{ nullptr };

	TSharedPtr<FUICommandInfo>	CmdInfoFindAllTabWells{ nullptr };
	TArray<TWeakPtr<SWidget>>	EditorTabWells;
	FOnActiveTabChanged			OnActiveTabChanged(FOnActiveTabChanged::FDelegate);
	const FString				SDTabWell{ "SDockingTabWell" };
	const FString				SToolkit{ "SStandaloneAssetEditorToolkitHost" };
	const FString				SLvlEditor{ "SLevelEditor" };
	TWeakPtr<SWindow>			CurrWin{ nullptr }; // User for getting Maj Tab
	static TWeakPtr<SDockTab>	CurrMajorTab;
	TWeakPtr<SDockTab>			CurrMinorTab{ nullptr };
	FUMOnNewMajorTabChanged		OnNewMajorTabChanged;
	TSharedPtr<FUMFocusManager> FocusManager;

	bool VisualLog{ true };
};
