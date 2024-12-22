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

	static FUMWindowsNavigationManager& Get();

	static bool IsInitialized();

	void RegisterCycleWindowsNavigation(const TSharedPtr<FBindingContext>& MainFrameContext);

	void MapCycleWindowsNavigation(const TSharedRef<FUICommandList>& CommandList);

	void CycleWindows(bool bIsNextWindow);

	void OnFocusChanged(const FFocusEvent& FocusEvent, const FWeakWidgetPath& OldWidgetPath, const TSharedPtr<SWidget>& OldWidget, const FWidgetPath& NewWidgetPath, const TSharedPtr<SWidget>& NewWidget);

	bool HasUserMovedToNewWindow(bool bSetNewWindow = true);

	void CleanupInvalidWindows(TArray<uint64> CleanupWindowsIds);

	void ToggleNonRootWindowsState(bool bToggleMinimized);

	/**
	 * Called after FSlateApplication delegates PostEngineInit.
	 * This is where we will register our Tab Tracking mechanism and such.
	 */
	void RegisterSlateEvents();

	const TMap<uint64, TWeakPtr<SWindow>>& GetTrackedWindows();

private:
	static TSharedPtr<FUMWindowsNavigationManager> WindowsNavigationManager;

public:
	const FName				  MainFrameContextName = TEXT("MainFrame");
	FUMOnUserMovedToNewWindow OnUserMovedToNewWindow;

	TSharedPtr<FUICommandInfo> CmdInfoCycleNextWindow{ nullptr };
	TSharedPtr<FUICommandInfo> CmdInfoCyclePrevWindow{ nullptr };

	TWeakPtr<SWindow>				CurrWin{ nullptr };
	TMap<uint64, TWeakPtr<SWindow>> TrackedWindows;
	bool							VisualLog{ false };
};
