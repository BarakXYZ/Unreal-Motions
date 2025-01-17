#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/InputBindingManager.h"
#include "ISceneOutlinerTreeItem.h"
#include "UMLogger.h"
#include "UMInputPreProcessor.h"
#include "UMGenericAppMessageHandler.h"
#include "Framework/Application/SlateApplication.h"
#include "EditorSubsystem.h"
#include "VimEditorSubsystem.generated.h"

/**
 *
 */
UCLASS()
class UNREALMOTIONS_API UVimEditorSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

	/**
	 * Depending on if Vim is enabled in the config, will control if the
	 * subsystem should be created.
	 */
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	/**
	 * @brief Initializes the Vim editor subsystem with configuration settings.
	 *
	 * @details Performs the following setup operations:
	 * 1. Reads configuration file for startup preferences
	 * 2. Initializes Vim mode based on config
	 * 3. Sets up weak pointers and input processor
	 * 4. Binds command handlers
	 *
	 * @param Collection The subsystem collection being initialized
	 */
	virtual void Initialize(FSubsystemCollectionBase& Collction) override;

	virtual void Deinitialize() override;

	void WrapAndSetCustomMessageHandler();

	/**
	 * @brief Binds all Vim command handlers to their respective key sequences.
	 *
	 * @details Sets up bindings for:
	 * - Navigation commands (HJKL)
	 * - Selection commands (Shift + HJKL)
	 * - Jump commands (gg, G)
	 * - Scroll commands (Ctrl+U, Ctrl+D)
	 * - Tab management commands (Space+O...)
	 * - Delete commands (x)
	 */
	void BindCommands();

	/**
	 * Processes Vim-style navigation input for list views.
	 * Handles navigation in both Normal and Visual modes:
	 * 1. Maps Vim keys (HJKL) to directional navigation
	 * 2. Applies count prefix for repeated navigation
	 * 3. Maintains selection state in Visual mode
	 * 4. Tracks navigation offset for Visual mode operations
	 *
	 * @param SlateApp Reference to the Slate application instance
	 * @param InKeyEvent The key event containing navigation input
	 */
	void ProcessVimNavigationInput(
		FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	/**
	 * This is pretty generic. We're not doing something too special. Might
	 * want to refactor and share this with more types.
	 */
	bool HandleListViewNavigation(
		FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	void HandleArrowKeysNavigation(
		FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	int32 GetPracticalCountBuffer();

	/**
	 * @brief Resets the count sequence buffer.
	 *
	 * @details Clears any stored count prefix when.
	 */
	void OnResetSequence();

	/**
	 * Handles count prefix input for command repetition.
	 * Manages numeric prefix accumulation - Appends new digit to existing buffer
	 * @param AddedCount The new digit to add to count buffer
	 */
	void OnCountPrefix(FString AddedCount);

	/**
	 * @brief Handles dummy key character events for input suppression.
	 *
	 * @details Used to prevent unwanted key processing in specific UI contexts
	 *
	 * @param MyGeometry Widget geometry
	 * @param InCharacterEvent The character event to handle
	 * @return FReply::Handled() to consume the event
	 */
	FReply HandleDummyKeyChar(
		const FGeometry& MyGeometry, const FCharacterEvent& InCharacterEvent)
	{
		return FReply::Handled();
	}

	/**
	 * @brief Handles dummy key down events for input suppression.
	 *
	 * @details Used to prevent unwanted key processing in specific UI contexts
	 *
	 * @param MyGeometry Widget geometry
	 * @param InKeyEvent The key event to handle
	 * @return FReply::Handled() to consume the event
	 */
	FReply HandleDummyKeyDown(
		const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
	{
		return FReply::Handled();
	}

	/**
	 * Navigates to either the first or last item in a ListView based on the current Vim mode and input.
	 * In Normal mode, directly sets selection to first/last item.
	 * In Visual mode, extends the selection from the anchor point to first/last item.
	 *
	 * @param SlateApp The Slate application instance used to access UI elements
	 * @param InKeyEvent The key event that triggered this navigation (checks for Shift modifier)
	 *
	 * @note In Visual mode, uses 'G' to navigate to last item and 'gg' for first item
	 * @note Maintains selection anchor point in Visual mode for proper range selection
	 */
	void NavigateToFirstOrLastItem(
		FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	void HandleArrowKeysNavigationToFirstOrLastItem(
		FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	bool HandleTreeViewFirstOrLastItemNavigation(
		FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	/**
	 * @brief Handles navigation to first or last item in Normal mode.
	 *
	 * @details Performs direct navigation to extremes of the list:
	 * 1. Selects target item
	 * 2. Requests visual scroll to item
	 * 3. Clears any existing multi-selection
	 *
	 * @param ListView The list view to navigate
	 * @param AllItems Array of all items in the list
	 * @param bIsShiftDown Whether shift key is pressed (G vs gg)
	 */
	void HandleTreeViewNormalModeFirstOrLastItemNavigation(
		TSharedRef<SListView<TSharedPtr<ISceneOutlinerTreeItem>>> ListView,
		TArrayView<const TSharedPtr<ISceneOutlinerTreeItem>>&	  AllItems,
		bool													  bIsShiftDown);

	/**
	 * @brief Handles navigation to first/last item in Visual mode.
	 *
	 * @details Manages range selection:
	 * 1. Validates anchor point
	 * 2. Calculates navigation count based on current position
	 * 3. Performs incremental navigation to maintain selection
	 *
	 * @param SlateApp Reference to Slate application
	 * @param ListView The list view being navigated
	 * @param AllItems Array of all items in list
	 * @param bIsShiftDown Whether shift is held (G vs gg)
	 */
	void HandleTreeViewVisualModeFirstOrLastItemNavigation(
		FSlateApplication&										  SlateApp,
		TSharedRef<SListView<TSharedPtr<ISceneOutlinerTreeItem>>> ListView,
		TArrayView<const TSharedPtr<ISceneOutlinerTreeItem>>&	  AllItems,
		bool													  bIsShiftDown);

	/**
	 * @brief Handles Vim mode state changes.
	 * Receives the delegate from the UMInputPreProcessor class
	 *
	 * @details Performs mode-specific operations:
	 * Normal Mode: Updates selection state
	 * Insert Mode: Basic mode switch
	 * Visual Mode: Initializes selection tracking and captures anchor point
	 *
	 * @param NewVimMode The Vim mode to switch to
	 */
	void OnVimModeChanged(const EVimMode NewVimMode);

	/**
	 * @brief Captures the current tree view item selection as an anchor point.
	 *
	 * @details Used in Visual mode to:
	 * 1. Store the initial selection point
	 * 2. Clear multiple selections if present
	 * 3. Record the item's index in the list to determine which item should be
	 * focused when exiting Visual Mode
	 *
	 * @param SlateApp Reference to the Slate application instance
	 */
	void CaptureAnchorTreeViewItemSelectionAndIndex(FSlateApplication& SlateApp);

	/**
	 * @brief Determines if a list view is vertically oriented.
	 *
	 * @details Checks widget type against known horizontal layouts
	 * like AssetTileView.
	 * Used to determine appropriate navigation directions.
	 *
	 * @param ListView The list view widget to check
	 * @return true if vertically oriented, false if horizontal
	 */
	bool IsTreeViewVertical(
		const TSharedRef<SListView<TSharedPtr<ISceneOutlinerTreeItem>>>& ListView);

	/**
	 * Scrolls the current list view by half a page.
	 * Implements Vim-style Ctrl+D and Ctrl+U scrolling:
	 * - Determines scroll direction based on key input
	 * - Maintains Visual mode selection if active
	 * - Scrolls by a fixed number of items (SCROLL_NUM - 6)
	 *
	 * @param SlateApp Reference to the Slate application instance
	 * @param InKeyEvent The key event that triggered the scroll
	 */
	void ScrollHalfPage(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	/**
	 * @brief Gets appropriate navigation direction key based on list orientation.
	 *
	 * @details Determines navigation keys based on:
	 * 1. List view orientation (vertical/horizontal)
	 * 2. Desired direction (forward/backward)
	 *
	 * @param ListView The list view to check orientation
	 * @param bGetForwardDirection true for forward navigation, false for backward
	 * @return FKey representing appropriate navigation direction
	 */
	FKey GetTreeNavigationDirection(
		const TSharedRef<SListView<TSharedPtr<ISceneOutlinerTreeItem>>> ListView, bool bGetForwardDirection);

	DECLARE_DELEGATE_RetVal_TwoParams(FReply, FOnKeyDown, const FGeometry&, const FKeyEvent&);

	TWeakObjectPtr<UVimEditorSubsystem> VimSubWeak{ nullptr };
	FDelegateHandle						PreInputKeyDownDelegateHandle;
	FString								CountBuffer;
	EVimMode							CurrentVimMode{ EVimMode::Insert };
	EVimMode							PreviousVimMode{ EVimMode::Insert };
	FKeyEvent							LastNavigationDirection;
	int32								VisualNavOffsetIndicator{ 0 };
	const int32							MIN_REPEAT_COUNT = 1;
	const int32							MAX_REPEAT_COUNT = 999;

	TSharedPtr<FUMGenericAppMessageHandler> UMGenericAppMessageHandler;

	FUMLogger Logger;

	struct FTreeViewItemInfo
	{
		TWeakPtr<ISceneOutlinerTreeItem> Item = nullptr;
		int32							 Index = -1;
	};
	FTreeViewItemInfo AnchorTreeViewItem;

	bool bSyntheticInsertToggle = false;
};
