#pragma once

#include "CoreMinimal.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SWindow.h"
#include "Widgets/SWidget.h"
#include "framework/Application/SlateApplication.h"

class FUMFocusManager
{
public:
	FUMFocusManager();
	~FUMFocusManager();

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

	// ~ Last active Widget in Tab Well ~
	// You enter the ID of the Minor Tab:
	// if (The Minor Tab exists and valid, and the widget found is valid)
	// Focus on that widget.
	// else
	// Find the first widget that make sense to focus on
	// or focus Tab Content?
	TMap<uint64, TWeakPtr<SWidget>> LastActiveWidgetByMinorTabId;

	TWeakPtr<SWindow>  ActiveWindow;
	TWeakPtr<SDockTab> ActiveMajorTab;
	TWeakPtr<SWidget>  ActiveTabWell;
	TWeakPtr<SDockTab> ActiveMinorTab;
	TWeakPtr<SWidget>  ActiveWidget;

	static TSharedPtr<FUMFocusManager> FocusManager;

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
