#pragma once

#include "CoreMinimal.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SWindow.h"
#include "Widgets/SWidget.h"
#include "UMInputPreProcessor.h"
#include "framework/Application/SlateApplication.h"
#include "UMWindowsNavigationManager.h"

DECLARE_DELEGATE_RetVal_TwoParams(bool, FUMOnWindowAction, const TSharedRef<FGenericWindow>&, EWindowAction::Type);

class FUMFocusManager
{
public:
	FUMFocusManager();
	~FUMFocusManager();

	static const TSharedPtr<FUMFocusManager>& Get();

	void RegisterSlateEvents();

	/**
	 * TODO: Document
	 *
	 * @param FocusEvent The focus event that triggered the callback
	 * @param OldWidgetPath Path to the previously focused widget
	 * @param OldWidget The previously focused widget
	 * @param NewWidgetPath Path to the newly focused widget
	 * @param NewWidget The newly focused widget
	 */
	void OnFocusChanged(const FFocusEvent& FocusEvent, const FWeakWidgetPath& OldWidgetPath,
		const TSharedPtr<SWidget>& OldWidget, const FWidgetPath& NewWidgetPath,
		const TSharedPtr<SWidget>& NewWidget);

	void OnFocusChangedV2(const FFocusEvent& FocusEvent, const FWeakWidgetPath& OldWidgetPath,
		const TSharedPtr<SWidget>& OldWidget, const FWidgetPath& NewWidgetPath,
		const TSharedPtr<SWidget>& NewWidget);

	void DetectWidgetType(const TSharedRef<SWidget> InWidget);

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

	void OnActiveTabChangedV2(
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

	void OnTabForegroundedV2(
		TSharedPtr<SDockTab> NewActiveTab, TSharedPtr<SDockTab> PrevActiveTab);

	/**
	 * Updates the currently tracked tab (major or minor).
	 * @param NewTab The tab to set as current
	 */
	void SetCurrentTab(const TSharedRef<SDockTab> NewTab);

	void PreSetCurrentTab();

	void SetActiveMajorTab(const TSharedRef<SDockTab> InTab);

	void SetActiveMinorTab(const TSharedRef<SDockTab> InTab);

	void LogTabChange(const FString& TabType,
		const TWeakPtr<SDockTab>& CurrentTab, const TSharedRef<SDockTab>& NewTab);

	// Can be called from a few different stages:
	// If Major Tab Changed
	// If TabWell Changed
	// If Minor Tab Changed
	void SetUserFocus();

	bool ShouldFilterNewWidget(TSharedRef<SWidget> InWidget);

	bool TryFocusLastActiveWidget();

	bool DoesActiveMinorTabResidesInMajorTab(
		const TSharedRef<SDockTab> InMajorTab);

	bool DoesMinorTabResidesInMajorTab(
		const TSharedRef<SDockTab> MinorTab, const TSharedRef<SDockTab> MajorTab);

	void HandleOnWindowChanged(
		const TSharedPtr<SWindow> PrevWindow,
		const TSharedPtr<SWindow> NewWindow);

	void ToggleVisualLog(bool bIsVisualLog);

	bool TryGetFrontmostMajorTab();

	/**
	 * Used in the Vim Subsystem. When we remove the current Major Tab, this helps
	 * to focus and activate the next frontmost window.
	 */

	static bool FocusNextFrontmostWindow();

	/**
	 * Activates the specified window, bringing it to the front and setting proper focus.
	 * Handles focus management, window ordering, and ensuring proper activation state.
	 * @param Window The window to activate
	 */

	static void ActivateWindow(const TSharedRef<SWindow> Window);

	/**
	 * @brief Activates and properly focuses a newly invoked tab.
	 *
	 * @details Ensures proper tab activation by:
	 * 1. Clearing existing focus
	 * 2. Activating parent window
	 * 3. Setting tab as active in parent
	 *
	 * @param SlateApp Reference to Slate application
	 * @param NewTab Pointer to newly created tab
	 */
	static void ActivateNewInvokedTab(
		FSlateApplication& SlateApp, const TSharedPtr<SDockTab> NewTab);

	void TrackActiveWindow();

	void LogTabParentWindow(const TSharedRef<SDockTab> InTab);

	bool HasWindowChanged();

	bool FindTabWellAndActivateForegroundedTab(const TSharedRef<SDockTab> InMajorTab);

	static bool RemoveActiveMajorTab();

	bool VisualizeParentDockingTabStack(const TSharedRef<SDockTab> InTab);

	void DebugGlobalTabManagerTracking();

	void DebugPrevAndNewMinorTabsMajorTabs(
		TSharedPtr<SDockTab> PrevActiveTab, TSharedPtr<SDockTab> NewActiveTab);

	/**
	 * Try register the in Minor Tab TabWell with the currently active Major Tab.
	 * @param InBaseMinorTab - The minor tab that resides inside the parent
	 * TabWell we'll register
	 */
	bool RegisterTabWellWithActiveMajorTab(
		const TSharedRef<SDockTab> InBaseMinorTab);

	bool DoesTabHaveFocus(const TSharedRef<SDockTab> InTab);

	bool TryFocusOnLastActiveWidgetInMinorTab(const TSharedRef<SDockTab> InTab);

	FString TabRoleToString(ETabRole InTabRole);

	bool TryRegisterMinorWithParentMajorTab(
		const TSharedRef<SDockTab> InMinorTab);

	bool TryRegisterWidgetWithTab(
		const TWeakPtr<SWidget> InWidget, const TSharedRef<SDockTab> InTab);

	bool TryRegisterWidgetWithTab(const TSharedRef<SDockTab> InTab);

	bool TryActivateLastWidgetInTab(const TSharedRef<SDockTab> InTab);

	void ActivateTab(const TSharedRef<SDockTab> InTab);

	void RecordWidgetUse(TSharedRef<SWidget> InWidget);

	bool TryFocusLastActiveMinorForMajorTab(TSharedRef<SDockTab> InMajorTab);

	bool TryFocusFirstFoundMinorTab(TSharedRef<SWidget> InMajorTab);

	void GetDefaultTargetFallbackWidgetType(TSharedRef<SDockTab> InTab);

	bool TryFocusFirstFoundListView(TSharedRef<SDockTab> InMajorTab);

	bool TryFocusFirstFoundSearchBox(TSharedRef<SWidget> InContent);

	void FocusTabContent(TSharedRef<SDockTab> InTab);

	void OnWindowBeingDestroyed(const SWindow& Window);

	// ~ Last active Major Tab by Window ID ~
	// You enter the ID of the Window:
	// if (the Window exists in it and the Major Tab is valid)
	// Bring focus to it and pass it to the LastActiveTabWellByMajorTabId
	// else
	// GetActiveMajorTab which will traverse and get the foregrounded
	// Major Tab manually.
	TMap<uint64, TWeakPtr<SDockTab>> LastActiveMajorTabByWindowId;

	// ~ Last active TabWell by Major Tab ~
	// You enter the ID of the Major Tab:
	// if (the TabWell exists in it and is valid)
	// focus on it and pass it to the LastActiveMinorTabByTabWell
	// to find if there's a tracked Minor Tab we can focus on.
	// else
	// Traverse the Major Tab content to find all TabWells and focus on
	// the first one (or some condition depending on the editor)
	TMap<uint64, TWeakPtr<SWidget>> LastActiveTabWellByMajorTabId;

	// ~ Last active Minor Tab in Tab Well ~
	// You enter the ID of the TabWell,
	// if (the TabWell is tracked and the Minor Tab stored is valid)
	// Focus on that Minor Tab
	// else
	// Focus on the first tab inside that TabWell (or some other
	// condition depending on the editor)
	TMap<uint64, TWeakPtr<SDockTab>> LastActiveMinorTabByTabWellId;

	TMap<uint64, TWeakPtr<SDockTab>> LastActiveMinorTabByMajorTabId;

	// ~ Last active Widget in Tab Well ~
	// You enter the ID of the Minor Tab:
	// if (The Minor Tab exists and valid, and the widget found is valid)
	// Focus on that widget.
	// else
	// Find the first widget that make sense to focus on
	// or focus Tab Content?
	TMap<uint64, TWeakPtr<SWidget>> LastActiveWidgetByMinorTabId;

	TMap<uint64, TWeakPtr<SWidget>> LastActiveWidgetByTabId;

	// TODO: protected + friend classes?
	TWeakPtr<SWindow>  ActiveWindow;
	TWeakPtr<SDockTab> ActiveMajorTab;
	TWeakPtr<SWidget>  ActiveTabWell;
	TWeakPtr<SDockTab> ActiveMinorTab;
	TWeakPtr<SWidget>  ActiveWidget;
	TWeakPtr<SWidget>  PrevWidget;

	static const TSharedPtr<FUMFocusManager> FocusManager;

public:
	FDelegateHandle DelegateHandle_OnActiveTabChanged;
	FDelegateHandle DelegateHandle_OnTabForegrounded;

	FTimerHandle TimerHandleNewWidgetTracker;
	FTimerHandle TimerHandleNewMinorTabTracker;
	FTimerHandle TimerHandleTabForegrounding;
	FTimerHandle TimerHandle_OnActiveTabChanged;

	FTimerHandle TimerHandle_FocusTabContentFallback;

	TWeakPtr<SDockTab> ForegroundedProcessedTab;
	bool			   bIsTabForegrounding{ false };
	bool			   bIsDummyForegroundingCallbackCheck{ false };
	FUMOnMouseButtonUp OnMouseButtonUp;

	bool			  bHasFilteredAnIncomingNewWidget{ false };
	bool			  bBypassAutoFocusLastActiveWidget{ false };
	FUMOnWindowAction OnWindowAction;

	static FUMLogger Logger;
	bool			 bLog{ false };
	bool			 bVisLogTabFocusFlow{ false };

	// Holds the most recent widget at index 0, and the oldest at the end
	TArray<TWeakPtr<SWidget>> RecentlyUsedWidgets;

	// UMFocusManager
	// It will listen to:
	// OnWindowChanged
	// OnMajorTabChanged
	// OnTabWellChanged
	// OnMinorTabChanged
	// OnWidgetChanged -> Set the new widget

	// 1. Have a map of Major Tabs and their Docking Area
	// 2. Then when moving between major tabs we can try to search and check
	// if this Docking Area is still valid, if it's not, research it.
	// 3. After getting the Docking Area, we need to traverse and search for
	// all the Panel Tabs inside it. We don't even need to store them because
	// we can assume they will stay the same.
	// 4. After fetching the panel tabs, we get the last known active panel
	// tab inside that Docking Area and check if it is still valid and if
	// it's part of the found Panel Tabs. If it is, we bring focus to it.
	// Potentially we can check if when iterating the tabs we have a built-in
	// way to determine which one was the last active one.
	// If we can find our last known active tab inside this array, or we don't
	// have one at all because it's a fresh asset or something, we just focus
	// the first panel tab found (or have some sort of condition to prefer
	// a specific panel tab (e.g. Event Graph for Blueprints, etc.))
	// 5. We can then check if
};

// 1.
// When cycling Major Tabs, OnTabForegrounded will be triggered while
// OnActiveTabChanged won't.

// 2.
// When cycling Minor Tabs, both OnTabForegrounded & OnActiveTabChanged trigger.

// 3.
// When detaching (dragging-off) Major Tabs, only OnTabForegrounded will trigger.

// 4.
// When detaching (dragging-off) Minor Tabs, both OnForeground & OnActive trigger

// 5.
// When switching between minor tabs without cycling between tabs, for example
// in blueprint we can switch between the details panel, to the Event Graph, etc.
// This doesn't require foregrounding, thus only OnActiveTabChanged will trigger.

// Tab Roles:
// Tabs like Preferences, Output Log and Project Setting are considered
// Nomad Tabs. Nomad tabs are not constrained by the standard tab hierarchy.
// They can be docked or floated anywhere within the Unreal Engine editor.
