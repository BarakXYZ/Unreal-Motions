#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/InputBindingManager.h"
#include "Framework/Docking/TabManager.h"

enum ENavSpecTabType : uint8
{
	LevelEditor,
	Toolkit,
	None,
};

class FUMTabNavigationManager
{
public:
	FUMTabNavigationManager();
	~FUMTabNavigationManager();

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
	void AddMajorTabsNavigationCommandsToList(TSharedPtr<FBindingContext> MainFrameContext);

	/**
	 * Registers UI commands for Minor Tab Navigation (last tab and tabs 1-9).
	 * @param MainFrameContext The binding context to register commands in
	 */

	void AddMinorTabsNavigationCommandsToList(TSharedPtr<FBindingContext> MainFrameContext);

	/**
	 * Maps registered tab navigation commands to their handlers.
	 * @param CommandList List to bind the tab navigation actions to
	 */
	void MapTabCommands(const TSharedRef<FUICommandList>& CommandList, const TArray<TSharedPtr<FUICommandInfo>>& CommandInfo, bool bIsMajorTab);

	void OnActiveTabChanged(TSharedPtr<SDockTab> PreviousActiveTab, TSharedPtr<SDockTab> NewActiveTab);

	/**
	 * Bind to FSlateApplication delegates PostEngineInit.
	 * This is where we will register our Tab Tracking mechanism and such.
	 */
	void RegisterSlateEvents();

	/**
	 * Command handler to focus a specific tab in the active window's tab well.
	 * @param TabIndex Index of tab to focus (1-based, 0 selects last tab)
	 * @see MapTabCommands For the keybinding configuration
	 */
	void OnMoveToTabIndex(int32 TabIndex, bool bIsMajorTab);

	void OnCycleTabs(bool bIsMajorTab, bool bIsNextTab);

	bool ValidateTargetTab(TSharedPtr<SWidget>& OutTab, bool bIsMajorTab);

	bool GetParentTabWellFromTab(TSharedPtr<SWidget>& InOutTabWell);

	void RegisterNextPrevTabNavigation(const TSharedPtr<FBindingContext>& MainFrameContext);

	void MapNextPrevTabNavigation(const TSharedRef<FUICommandList>& CommandList);

	void OnMouseButtonDown(const FPointerEvent& PointerEvent);

	/**
	 * Activates a specific tab within a SDockingTabWell widget.
	 * @param DockingTabWell The tab well containing the tabs
	 * @param TabIndex Index of tab to focus (1-based, 0 selects last tab)
	 */
	void FocusTabIndex(const TSharedPtr<SWidget>& DockingTabWell, int32 TabIndex);

	/**
	 * Recursively searches a widget tree for a SDockingTabWell widget.
	 * @param ParentWidget The root widget to start traversing from
	 * @param OutWidget Output parameter that will store the found widgets
	 * @param TargetType Type of widget we're looking for (e.g. "SDockingTabWell")
	 * @param SearchCount How many instances of this widget type we will try to look for. -1 will result in trying to find all widgets of this type in the widget's tree. 0 will be ignored, 1 will find the first instance, then return.
	 * @param Depth Current depth in the widget tree (used for logging)
	 * @return true if OutWidgets >= SearchCount, or OutWidget > 0 if SearchCount is set to -1, false otherwise
	 */
	bool TraverseWidgetTree(
		const TSharedPtr<SWidget>& ParentWidget,
		TArray<TWeakPtr<SWidget>>& OutWidgets,
		const FString&			   TargetType,
		int32					   SearchCount = -1,
		int32					   Depth = 0);

	/**
	 * Recursively searches a widget tree for a single widget of the specified type.
	 * @param ParentWidget The root widget to start traversing from
	 * @param OutWidget Output parameter that will store the found widget
	 * @param TargetType Type of widget we're looking for
	 * @param Depth Current depth in the widget tree (used for logging)
	 * @return true if a widget was found, false otherwise
	 */
	bool TraverseWidgetTree(
		const TSharedPtr<SWidget>& ParentWidget,
		TWeakPtr<SWidget>&		   OutWidget,
		const FString&			   TargetType,
		int32					   Depth = 0);

	// ** Not used currently */
	void SetupFindTabWells(TSharedRef<FUICommandList>& CommandList,
		TSharedPtr<FBindingContext>					   MainFrameContext);

	// ** Not used currently */
	void FindAllTabWells();

	// ** Not used currently */
	void DebugTab(const TSharedPtr<SDockTab>& Tab);

	// ** Not used currently */
	ENavSpecTabType GetNavigationSpecificTabType(const TSharedPtr<SDockTab>& Tab);

	// ** Not used currently */
	bool GetActiveMajorTab(TWeakPtr<SDockTab>& OutMajorTab);

	// ** Not used currently */
	/**
	 * Return the last active non-major tab. Can be a Panel Tab, a Nomad Tab, etc.
	 * @param OutNonMajorTab Where we will store the found tab (if any).
	 */
	bool GetLastActiveNonMajorTab(TWeakPtr<SDockTab>& OutNonMajorTab);

	// ** Not used currently */
	bool FindRootTargetWidgetName(FString& OutRootWidgetName);

	// ** Not used currently */
	void DebugWindow(const SWindow& Window);

	// ** Not used currently */
	void OnFocusChanged(const FFocusEvent& FocusEvent, const FWeakWidgetPath& OldWidgetPath, const TSharedPtr<SWidget>& OldWidget, const FWidgetPath& NewWidgetPath, const TSharedPtr<SWidget>& NewWidget);

	// ** Not used currently */
	bool HasUserMovedToNewWindow(bool bSetNewWindow = true);

	void SetNewCurrentTab(const TSharedPtr<SDockTab>& NewTab, bool bIsMajorTab);

	void CheckHasMovedToNewWinAndSetTab();

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

	TSharedPtr<FUICommandInfo> CmdInfoNextMajorTab = nullptr;
	TSharedPtr<FUICommandInfo> CmdInfoPrevMajorTab = nullptr;
	TSharedPtr<FUICommandInfo> CmdInfoNextMinorTab = nullptr;
	TSharedPtr<FUICommandInfo> CmdInfoPrevMinorTab = nullptr;

	TSharedPtr<FUICommandInfo> CmdInfoFindAllTabWells{ nullptr };
	TArray<TWeakPtr<SWidget>>  EditorTabWells;
	FOnActiveTabChanged		   OnActiveTabChanged(FOnActiveTabChanged::FDelegate);
	const FString			   SDTabWell{ "SDockingTabWell" };
	const FString			   SToolkit{ "SStandaloneAssetEditorToolkitHost" };
	const FString			   SLvlEditor{ "SLevelEditor" };
	TWeakPtr<SWindow>		   CurrWin{ nullptr };
	TWeakPtr<SDockTab>		   CurrMajorTab{ nullptr };
	TWeakPtr<SDockTab>		   CurrMinorTab{ nullptr };

	bool VisualLog{ false };
};
