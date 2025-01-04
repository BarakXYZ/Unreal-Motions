#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "Framework/Commands/InputBindingManager.h"
#include "ISceneOutlinerTreeItem.h"
#include "UMHelpers.h"
#include "UMInputPreProcessor.h"
#include "UMGenericAppMessageHandler.h"
#include "Framework/Application/SlateApplication.h"
#include "UMVisualModeManager.h"
#include "VimEditorSubsystem.generated.h"

/**
 *
 */
UCLASS()
class UNREALMOTIONS_API UVimEditorSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

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
	 * DEPRECATED
	 * @brief Toggles the Vim editor functionality on or off.
	 *
	 * @details Manages:
	 * 1. Input delegate binding/unbinding
	 * 2. State cleanup on disable
	 * 3. Notification of status changes
	 *
	 * @param bEnable Whether to enable or disable Vim functionality
	 */
	UFUNCTION(BlueprintCallable, Category = "Vim Editor Subsystem")
	void ToggleVim(bool bEnabled);

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

	bool HandleListViewNavigation(
		FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	void HandleArrowKeysNavigation(
		FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	int32 GetPracticalCountBuffer();

	/**
	 * ~ WIP ~
	 * @brief Deletes the currently selected item.
	 *
	 * @details Simulates delete key press:
	 * 1. Creates synthetic delete key event
	 * 2. Toggles native input handling
	 * 3. Processes delete operation
	 *
	 * @param SlateApp Reference to Slate application
	 * @param InKeyEvent Original triggering key event
	 */
	void DeleteItem(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

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
	void ActivateNewInvokedTab(
		FSlateApplication& SlateApp, const TSharedPtr<SDockTab> NewTab);

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

	/**
	 * @brief Opens the Output Log tab.
	 *
	 * @details Invokes and activates the Output Log tab using global tab manager
	 */
	void OpenOutputLog();

	/**
	 * @brief Opens specified Content Browser tab.
	 *
	 * @details Manages Content Browser tab activation:
	 * 1. Determines target tab number (1-4)
	 * 2. Constructs appropriate tab identifier
	 * 3. Invokes and activates the tab
	 *
	 * @param SlateApp Reference to the Slate application instance
	 * @param InKeyEvent Key event containing potential tab number
	 */
	void OpenContentBrowser(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	void OpenPreferences(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	void TryFocusSearchBox(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	/**
	 * @brief Removes the currently active major tab.
	 *
	 * @details Handles tab removal:
	 * 1. Attempts to remove active major tab
	 * 2. Focuses next frontmost window if successful
	 */
	void RemoveActiveMajorTab();

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
	 * SEMI-DEPRECATED
	 * @brief Finds the index of a widget within its parent's children.
	 *
	 * @details Traverses parent widget hierarchy to:
	 * 1. Locate parent panel
	 * 2. Iterate through children
	 * 3. Match target widget
	 *
	 * @param Widget Reference to widget to find
	 * @return Index of widget in parent, or INDEX_NONE if not found
	 */
	int32 FindWidgetIndexInParent(
		const TSharedRef<SWidget>& Widget);

	void DebugTreeItem(const TSharedPtr<ISceneOutlinerTreeItem, ESPMode::ThreadSafe>& TreeItem, int32 Index);

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
		TSharedPtr<SListView<TSharedPtr<ISceneOutlinerTreeItem>>>& ListView,
		TArrayView<const TSharedPtr<ISceneOutlinerTreeItem>>&	   AllItems,
		bool													   bIsShiftDown);

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
		FSlateApplication&										   SlateApp,
		TSharedPtr<SListView<TSharedPtr<ISceneOutlinerTreeItem>>>& ListView,
		TArrayView<const TSharedPtr<ISceneOutlinerTreeItem>>&	   AllItems,
		bool													   bIsShiftDown);

	/**
	 * @brief Maps Vim navigation keys (HJKL) to corresponding UI nav events.
	 *
	 * @details Creates a navigation event that:
	 * - Translates H,J,K,L to Left,Down,Up,Right
	 * - Handles shift modifier for selection
	 * - Sets appropriate navigation genesis
	 *
	 * @param InKeyEvent The original key event to map
	 * @param OutNavigationEvent The resulting navigation event
	 * @param bIsShiftDown Whether shift is being held
	 * @return true if mapping was successful, false otherwise
	 */
	static bool MapVimToNavigationEvent(const FKeyEvent& InKeyEvent,
		FNavigationEvent& OutNavigationEvent, bool bIsShiftDown);

	/**
	 * SEMI-DEPRECATED
	 * @brief Maps Vim movement keys to corresponding arrow key events.
	 *
	 * @details Translates HJKL keys to arrow key events while preserving:
	 * - Modifier key states
	 * - Other key event properties
	 *
	 * @param InKeyEvent Original key event
	 * @param OutKeyEvent Mapped arrow key event
	 * @return true if mapping was successful, false if key wasn't mappable
	 */
	static bool MapVimToArrowNavigation(
		const FKeyEvent& InKeyEvent, FKeyEvent& OutKeyEvent, bool bIsShiftDown = false);

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
	 * @brief Performs undo operation using standard editor shortcut.
	 *
	 * @details Simulates Ctrl+Z key combination:
	 * 1. Creates appropriate modifier state
	 * 2. Simulates key press event
	 * 3. Processes through Slate application
	 *
	 * @param SlateApp Reference to the Slate application instance
	 * @param InKeyEvent Original triggering key event
	 */
	void Undo(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	/**
	 * ~ SEMI-DEPRECATED ~
	 * Tracks the navigation offset during Visual mode operations.
	 * Updates LastNavigationDirection and VisualNavOffsetIndicator based on
	 * HJKL key navigation.
	 * The offset indicator helps determine selection direction.
	 *
	 * @param InKeyEvent The key event that triggered the navigation
	 */
	void TrackVisualOffsetNavigation(const FKeyEvent& InKeyEvent);

	/**
	 * @brief Updates tree view selection when exiting Visual mode.
	 *
	 * @details Handles the transition by:
	 * 1. Determining final selection based on navigation offset (and Anchor)
	 * 2. Clearing existing selection array
	 * 3. Setting the new single item selection
	 *
	 * @param SlateApp Reference to the Slate application instance
	 */
	void UpdateTreeViewSelectionOnExitVisualMode(FSlateApplication& SlateApp);

	void GetCurrentTreeItemIndex(FSlateApplication&						 SlateApp,
		const TSharedPtr<SListView<TSharedPtr<ISceneOutlinerTreeItem>>>& ListView,
		const TSharedPtr<ISceneOutlinerTreeItem>&						 CurrItem);

	/**
	 * @brief Retrieves a list view widget from the currently focused UI element.
	 *
	 * @details Validates that:
	 * 1. The focused widget is valid
	 * 2. The widget type is supported for navigation
	 * 3. The widget can be cast to appropriate list view type (static only)
	 *
	 * @param SlateApp Reference to the Slate application instance
	 * @param OutListView Output parameter containing the found list view widget
	 * @return true if a valid list view was found, false otherwise
	 */
	bool GetListView(FSlateApplication& SlateApp, TSharedPtr<SListView<TSharedPtr<ISceneOutlinerTreeItem>>>& OutListView);

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
	bool IsTreeViewVertical(const TSharedPtr<SListView<TSharedPtr<ISceneOutlinerTreeItem>>>& ListView);

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
		const TSharedPtr<SListView<TSharedPtr<ISceneOutlinerTreeItem>>>& ListView, bool bGetForwardDirection);

	void Enter(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	void SimulateClickOnWidget(
		FSlateApplication& SlateApp, const TSharedRef<SWidget> Widget,
		const FKey& EffectingButton = EKeys::LeftMouseButton,
		bool		bIsDoubleClick = false);

	void SimulateRightClick(
		FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	void SimulateAnalogClick(
		FSlateApplication& SlateApp, const TSharedPtr<SWidget>& Widget);

	void NavigateNextPrevious(
		FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	bool GetSelectedTreeViewItemAsWidget(
		FSlateApplication& SlateApp, TSharedPtr<SWidget>& OutWidget,
		const TOptional<TSharedPtr<SListView<TSharedPtr<ISceneOutlinerTreeItem>>>>& OptionalListView);

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

	TSharedPtr<FUMGenericAppMessageHandler>		  UMGenericAppMessageHandler;
	TSharedPtr<FGenericApplicationMessageHandler> OriginGenericAppMessageHandler;

	TSharedPtr<FUMVisualModeManager> VisModeManager;

	struct FTreeViewItemInfo
	{
		TWeakPtr<ISceneOutlinerTreeItem> Item = nullptr;
		int32							 Index = -1;
	};
	FTreeViewItemInfo AnchorTreeViewItem;
};
