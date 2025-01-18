#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/InputBindingManager.h"
#include "Framework/Docking/TabManager.h"
#include "EditorSubsystem.h"
#include "UMTabNavigatorEditorSubsystem.generated.h"

DECLARE_MULTICAST_DELEGATE_TwoParams(FUMOnNewMajorTabChanged,
	TWeakPtr<SDockTab> /* New Major Tab */,
	TWeakPtr<SDockTab> /* New Minor Tab */);

/**
 *
 */
UCLASS()
class UNREALMOTIONS_API UUMTabNavigatorEditorSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

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

	/** DEPRECATED
	 * Return the last active non-major tab. Can be a Panel Tab, a Nomad Tab, etc.
	 * @param OutNonMajorTab Where we will store the found tab (if any).
	 */
	bool GetLastActiveNonMajorTab(TWeakPtr<SDockTab>& OutNonMajorTab);

	void HandleOnUserMovedToNewWindow(TWeakPtr<SWindow> NewWindow);

	void MoveTabToWindow(
		FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	void BindVimCommands();

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

	TSharedPtr<FUICommandInfo> CmdInfoFindAllTabWells{ nullptr };
	FOnActiveTabChanged		   OnActiveTabChanged(FOnActiveTabChanged::FDelegate);
	FUMOnNewMajorTabChanged	   OnNewMajorTabChanged;

	bool VisualLog{ true };
};
