#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "Framework/Commands/InputBindingManager.h"
#include "ISceneOutlinerTreeItem.h"
#include "UMHelpers.h"
#include "UMInputPreProcessor.h"
#include "VimEditorSubsystem.generated.h"

/**
 *
 */
UCLASS()
class UNREALMOTIONS_API UVimEditorSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

	virtual void Initialize(FSubsystemCollectionBase& Collction) override;
	virtual void Deinitialize() override;

	void BindCommands();

	UFUNCTION(BlueprintCallable, Category = "Vim Editor Subsystem")
	void ToggleVim(bool bEnabled);

	bool MapVimToArrowNavigation(
		const FKeyEvent& InKeyEvent, FKeyEvent& OutKeyEvent);

	void ProcessVimNavigationInput(
		FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	void DeleteItem(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	void ActivateNewInvokedTab(FSlateApplication& SlateApp, const TSharedPtr<SDockTab> NewTab);

	/**
	 * @brief Opens the Widget Reflector debugging tool and auto focus the debug
	 * button for convenience.
	 *
	 * @details This function performs the following operations:
	 * 1. Opens/activates the Widget Reflector tab
	 * 2. Traverses widget hierarchy to locate specific UI elements
	 * 3. Simulates user interactions to auto-focus the reflector
	 *
	 * @note The timer delay (150ms) may need adjustment on different platforms(?)
	 */
	void OpenWidgetReflector(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);
	void OpenOutputLog();
	void OpenContentBrowser(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	void RemoveActiveMajorTab();

	void OnResetSequence();
	void OnCountPrefix(FString AddedCount);

	FReply HandleDummyKeyChar(
		const FGeometry& MyGeometry, const FCharacterEvent& InCharacterEvent)
	{
		return FReply::Handled();
	}

	FReply HandleDummyKeyDown(
		const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
	{
		return FReply::Handled();
	}

	int32 FindWidgetIndexInParent(
		const TSharedRef<SWidget>& Widget);

	void DebugTreeItem(const TSharedPtr<ISceneOutlinerTreeItem, ESPMode::ThreadSafe>& TreeItem, int32 Index);

	void NavigateToFirstOrLastItem(
		FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	bool MapVimToNavigationEvent(const FKeyEvent& InKeyEvent,
		FNavigationEvent& OutNavigationEvent, bool bIsShiftDown);

	void OnVimModeChanged(const EVimMode NewVimMode);

	void Undo(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	void TrackVisualOffsetNavigation(const FKeyEvent& InKeyEvent);

	void UpdateTreeViewSelectionOnExitVisualMode(FSlateApplication& SlateApp);

	void GetCurrentTreeItemIndex(FSlateApplication&						 SlateApp,
		const TSharedPtr<SListView<TSharedPtr<ISceneOutlinerTreeItem>>>& ListView,
		const TSharedPtr<ISceneOutlinerTreeItem>&						 CurrItem);

	bool GetListView(FSlateApplication& SlateApp, TSharedPtr<SListView<TSharedPtr<ISceneOutlinerTreeItem>>>& OutListView);

	void CaptureFirstTreeViewItemSelectionAndIndex(FSlateApplication& SlateApp);

	void IsTreeViewVertical(FSlateApplication& SlateApp);

	DECLARE_DELEGATE_RetVal_TwoParams(FReply, FOnKeyDown, const FGeometry&, const FKeyEvent&);

	TWeakObjectPtr<UVimEditorSubsystem> VimSubWeak{ nullptr };
	TWeakPtr<FUMInputPreProcessor>		InputPP;
	EUMHelpersLogMethod					UMHelpersLogMethod = EUMHelpersLogMethod::PrintToScreen;
	FDelegateHandle						PreInputKeyDownDelegateHandle;
	bool								bVisualLog{ true };
	bool								bConsoleLog{ false };
	FString								CountBuffer;
	EVimMode							CurrentVimMode{ EVimMode::Insert };
	FKeyEvent							LastNavigationDirection;
	int32								VisualNavOffsetIndicator{ 0 };
	struct FTreeViewItemInfo
	{
		TWeakPtr<ISceneOutlinerTreeItem> Item = nullptr;
		int32							 Index = -1;
	};
	FTreeViewItemInfo FirstTreeViewItem;
};
