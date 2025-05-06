#pragma once

#include "Brushes/SlateRoundedBoxBrush.h"
#include "Framework/Application/SlateApplication.h"
#include "UMLogger.h"
#include "EditorSubsystem.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Text/SMultiLineEditableText.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "UMYankData.h"
#include "VimInputProcessor.h"
#include "VimTextTypes.h"
#include "VimTextEditorSubsystem.generated.h"

/**
 *
 */
UCLASS()
class UNREALMOTIONS_API UVimTextEditorSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	virtual void Initialize(FSubsystemCollectionBase& Collction) override;

	virtual void Deinitialize() override;

	void BindCommands();

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

	void RegisterSlateEvents();

	void OnFocusChanged(
		const FFocusEvent& FocusEvent, const FWeakWidgetPath& OldWidgetPath,
		const TSharedPtr<SWidget>& OldWidget, const FWidgetPath& NewWidgetPath,
		const TSharedPtr<SWidget>& NewWidget);

	void ToggleReadOnly(bool bNegateCurrentState = false, bool bHandleBlinking = true);
	void ToggleReadOnlySingle(bool bNegateCurrentState = false, bool bHandleBlinking = true);
	void ToggleReadOnlyMulti(bool bNegateCurrentState = false, bool bHandleBlinking = true);

	void HandleEditableUX();

	bool GetActiveEditableTextContent(FString& OutText, const bool bIfMultiCurrLine = false);
	bool GetSelectedText(FString& OutText);

	bool GetSelectionRange(FSlateApplication& SlateApp, FTextSelection& OutSelectionRange);
	bool GetSelectionRangeSingleLine(FSlateApplication& SlateApp, FTextSelection& OutSelectionRange);
	bool GetSelectionRangeMultiLine(FSlateApplication& SlateApp, FTextSelection& OutSelectionRange);
	void DebugSelectionRange(const FTextSelection& InSelectionRange);

	bool IsCursorAlignedRight(FSlateApplication& SlateApp);

	bool SetActiveEditableText(const FText& InText);
	/**
	 * bConsiderDefaultHintText will make sure to set Hint Text only if this
	 * widget already had one available on initial focus. This is useful for
	 * MultiLine Editable Text which seems to be buggy when setting hint texts
	 * and initially they start without any.
	 */
	bool SetActiveEditableHintText(const FText& InText, bool bConsiderDefaultHintText);

	bool RetrieveActiveEditableCursorBlinking();

	void SelectAllActiveEditableText();

	void OnEditableFocusLost();

	bool IsNewEditableText(const TSharedRef<SWidget> NewEditableText);
	bool IsDefaultEditableBuffer(const FString& InBuffer);

	void ClearTextSelection(bool bKeepInputInNormalMode = true);

	void HandleVimTextNavigation(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	void HandleRightNavigation(FSlateApplication& SlateApp);
	void HandleLeftNavigation(FSlateApplication& SlateApp);

	int32 GetCursorOffsetSingle();
	void  SetCursorOffsetSingle(int32 NewOffset);

	void NavigateWord(
		FSlateApplication& SlateApp,
		bool			   bBigWord,
		int32 (*FindWordBoundary)(
			const FString& Text, int32 CurrentPos, bool bBigWord));

	void ToggleCursorBlinkingOff();
	bool IsEditableTextWithDefaultBuffer();
	void SetDefaultBuffer();
	void HandleInsertMode(const FString& InCurrentText);
	void HandleNormalModeOneChar();
	void HandleNormalModeMultipleChars();
	bool TrySetHintTextForVimMode();

	void InsertAndAppend(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	//////////////////////////////////////////////////////////////////////////
	// ~			GoTo Start or End (gg + Shift + G)				~
	//
	void GoToStartOrEnd(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	void HandleGoToStartMultiLine(FSlateApplication& SlateApp);
	void HandleGoToEndMultiLine(FSlateApplication& SlateApp);
	void HandleVisualModeGoToStartOrEndMultiLine(FSlateApplication& SlateApp, bool bGoToStart);

	void HandleGoToStartOrEndSingleLine(FSlateApplication& SlateApp, bool bGoToEnd);

	void HandleGoToStartOrEndOfLine(FSlateApplication& SlateApp, bool bGoToEnd);
	//
	//
	//////////////////////////////////////////////////////////////////////////

	void JumpUpOrDown(FSlateApplication& SlateApp, const TArray<FInputChord>& InSequence);

	void SetCursorSelectionToDefaultLocation(FSlateApplication& SlateApp, bool bAlignCursorRight = true);

	bool DoesActiveEditableHasAnyTextSelected();

	bool IsCurrentLineEmpty();
	bool IsCurrentLineInSingleEmpty();
	bool IsCurrentLineInMultiEmpty();
	bool IsCurrentLineInMultiEmpty(const TSharedRef<SMultiLineEditableTextBox> InMultiTextBox);

	void DebugMultiLineCursorLocation(bool bIsPreNavigation, bool bIgnoreDelay = false);

	bool HandleUpDownMultiLine(FSlateApplication& SlateApp, const FKey& InKeyDir);
	bool HandleUpDownMultiLineNormalMode(FSlateApplication& SlateApp, const FKey& InKeyDir);
	bool HandleUpDownMultiLineVisualMode(FSlateApplication& SlateApp, const FKey& InKeyDir);

	bool IsMultiLineCursorAtBeginningOfDocument();

	bool IsMultiLineCursorAtEndOfDocument(const bool bConsiderLastLineAsEnd = false);
	bool IsMultiLineCursorAtEndOfDocument(FSlateApplication& SlateApp, const TSharedRef<SMultiLineEditableTextBox> InMultiLine, const bool bConsiderLastLineAsEnd = false);

	bool IsMultiLineCursorAtEndOrBeginningOfLine();
	bool IsMultiLineCursorAtEndOfLine();

	bool IsCursorAtBeginningOfLine();
	bool IsMultiLineCursorAtBeginningOfLine(bool bForceZeroValidation = false);
	bool IsSingleLineCursorAtBeginningOfLine();

	bool IsCursorAtEndOfLine(FSlateApplication& SlateApp);
	bool IsCursorAtEndOfLineSingle(FSlateApplication& SlateApp);
	bool IsCursorAtEndOfLineMulti(FSlateApplication& SlateApp);

	void SwitchInsertToNormalMultiLine(FSlateApplication& SlateApp);

	void Delete(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);
	void DeleteLine(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);
	void DeleteLineVisualMode(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);
	void DeleteLineSingle(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);
	void DeleteLineNormalModeSingle(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);
	void DeleteLineMulti(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);
	void DeleteLineNormalModeMulti(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);
	void ShiftDeleteNormalMode(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);
	void DeleteUpOrDown(FSlateApplication& SlateApp, const TArray<FInputChord>& InSequence);

	void AppendNewLine(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);
	bool AppendBreakMultiLine();

	EUMSelectionState GetSelectionState();

	void DeleteCurrentLineContent(FSlateApplication& SlateApp);
	void DeleteCurrentSelection(FSlateApplication& SlateApp, const bool bYankSelection);
	void DeleteToEndOfLine(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	void DeleteInsideWord(FSlateApplication& SlateApp, const TArray<FInputChord>& InSequence);

	void ChangeEntireLine(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);
	void ChangeEntireLineMulti(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	void ChangeToEndOfLine(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);
	void ChangeVisualMode(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);
	void ChangeInsideWord(FSlateApplication& SlateApp, const TArray<FInputChord>& InSequence);

	void AddDebuggingText(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	bool TrackVisualModeStartLocation(FSlateApplication& SlateApp);

	void AlignCursorToIndex(FSlateApplication& SlateApp, int32 CurrIndex, int32 AlignToIndex, bool bAlignRight);

	bool SelectTextInRange(
		FSlateApplication&	 SlateApp,
		const FTextLocation& StartLocation,
		const FTextLocation& EndLocation,
		bool				 bJumpToStart = true,
		bool				 bIgnoreSetCursorToDefault = false);

	bool SelectTextToLineIndex(FSlateApplication& SlateApp, FTextLocation& CurrentTextLocation, const int32 EndLineIndex);
	bool SelectTextToOffset(FSlateApplication& SlateApp, FTextLocation& CurrentTextLocation, const int32 EndOffset);

	bool GoToTextLocation(FSlateApplication& SlateApp, const FTextLocation& InTextLocation);

	/** DEPRECATED - Discovered that we have a built-in GoTo method also
	 * for Single-Line Editable Text */
	void GoToTextLocationSingleLine(
		FSlateApplication&				   SlateApp,
		const TSharedRef<SEditableTextBox> InTextBox,
		const int32						   InGoToOffset);

	bool  GetCursorLocation(FSlateApplication& SlateApp, FTextLocation& OutTextLocation);
	int32 GetCursorLocationSingleLine(FSlateApplication& SlateApp, const TSharedRef<SEditableTextBox> InTextBox);

	void JumpToEndOrStartOfLine(FSlateApplication& SlateApp,
		void (UVimTextEditorSubsystem::*HandleLeftOrRightNavigation)(FSlateApplication&));

	void JumpToStartOfLine(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);
	void JumpToEndOfLine(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	TSharedPtr<SMultiLineEditableText> GetMultilineEditableFromBox(
		const TSharedRef<SMultiLineEditableTextBox> InMultiLineTextBox);
	TSharedPtr<SEditableText> GetSingleEditableFromBox(
		const TSharedRef<SEditableTextBox> InEditableTextBox);

	FUMStringInfo GetFStringInfo(const FString& InputString);

	bool ForceFocusActiveEditable(FSlateApplication& SlateApp);

	void VimCommandSelectAll(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	void DebugSelectStartToEnd(const FTextLocation& StartLocation, const FTextLocation& GoToLocation);

	void ResetEditableHintText(bool bResetTrackedHintText);

	void SetEditableUnifiedStyle(const float InDelay);
	void SetEditableUnifiedStyle();
	void SetEditableUnifiedStyleSingleLine(
		const TSharedRef<SEditableTextBox> InTextBox);
	void SetEditableUnifiedStyleMultiLine(
		const TSharedRef<SMultiLineEditableTextBox> InMultiTextBox);

	const FSlateRoundedBoxBrush& GetBorderBrush(EVimMode InVimMode);

	void AssignEditableBorder(bool bAssignDefaultBorder = false, const EVimMode OptVimModeOverride = EVimMode::Any);

	int32 GetMultiLineCount();

	// Vim Binding Functions Commands
	void NavigateW(FSlateApplication& SlateApp, const TArray<FInputChord>& InSequence);
	void NavigateBigW(FSlateApplication& SlateApp, const TArray<FInputChord>& InSequence);
	void NavigateB(FSlateApplication& SlateApp, const TArray<FInputChord>& InSequence);
	void NavigateBigB(FSlateApplication& SlateApp, const TArray<FInputChord>& InSequence);

	void NavigateE(FSlateApplication& SlateApp, const TArray<FInputChord>& InSequence);
	void NavigateBigE(FSlateApplication& SlateApp, const TArray<FInputChord>& InSequence);
	void NavigateGE(FSlateApplication& SlateApp, const TArray<FInputChord>& InSequence);
	void NavigateGBigE(FSlateApplication& SlateApp, const TArray<FInputChord>& InSequence);
	//
	//							~ Word Navigation ~

	//						~ Yanking & Pasting Commands ~
	//

	void YankCurrentlySelectedText();
	void YankCharacter(FSlateApplication& SlateApp, const TArray<FInputChord>& InSequence);
	void YankLine(FSlateApplication& SlateApp, const TArray<FInputChord>& InSequence);
	void Paste(FSlateApplication& SlateApp, const TArray<FInputChord>& InSequence);
	void YankInsideWord(FSlateApplication& SlateApp, const TArray<FInputChord>& InSequence);

	bool SelectInsideWord(FSlateApplication& SlateApp);
	void SelectInsideWord();

	void HandlePasteCharacterwise(FSlateApplication& SlateApp, const TArray<FInputChord>& InSequence);
	void HandlePasteCharacterwiseNormalMode(
		FSlateApplication&					 SlateApp,
		const TArray<FInputChord>&			 InSequence,
		const FText&						 TextToPaste,
		const TSharedRef<FVimInputProcessor> InputProc);
	void HandlePasteCharacterwiseVisualMode(
		FSlateApplication&					 SlateApp,
		const TArray<FInputChord>&			 InSequence,
		const FText&						 TextToPaste,
		const TSharedRef<FVimInputProcessor> InputProc);

	void HandlePasteLinewise(FSlateApplication& SlateApp, const TArray<FInputChord>& InSequence);
	void PasteVisualMode(FSlateApplication& SlateApp, const TArray<FInputChord>& InSequence);

	void ReplaceCharacter(FSlateApplication& SlateApp, const TArray<FInputChord>& InSequence);
	void ReplaceCharacterSingle(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	//
	//						~ Yanking & Pasting Commands ~

	/**
	 * When we edit our text, then try to navigate it in its most recent form
	 * without writing to disk; Our editable state seems to be confused sometimes
	 * with an older edit of itself. To mitigate that we're enforcing pre-cache
	 * (AFAIK) by setting text manually for the editable. That seems to "refresh"
	 * itself to be up-to-date with the most recent edits and give us stable
	 * navigation.
	 *
	 * TODO: Test on Single-Line Editable Text
	 */
	void RefreshActiveEditable(FSlateApplication& SlateApp);

	FReply OnMultiLineKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent);

	bool IsMultiChildOfConsole(const TSharedRef<SMultiLineEditableTextBox> InMultiBox);

	bool InsertTextAtCursor(FSlateApplication& SlateApp, const FText& InText);
	bool InsertTextAtCursorSingleLine(FSlateApplication& SlateApp, const FText& InText);
	bool InsertTextAtCursorMultiLine(FSlateApplication& SlateApp, const FText& InText);

	void BeginFindChar(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);
	/**
	 * Processes a key event to find a specific character in the current line of text
	 *
	 * @param SlateApp Reference to the Slate application
	 * @param InKeyEvent The key event containing the character to find
	 */
	void HandleFindChar(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	int32 GetOffsetAdjustmentForFind(FSlateApplication& SlateApp);

	/**
	 * Attempts to find the specified character in the current line and move the cursor to it
	 *
	 * @param SlateApp Reference to the Slate application
	 * @param CharToFind The character to search for
	 * @return True if the character was found and the cursor was moved, false otherwise
	 */
	bool TryFindAndMoveToCursor(FSlateApplication& SlateApp, TCHAR CharToFind);

	bool HandleFindAndMoveToCursorVisualMode(FSlateApplication& SlateApp, int32 FoundCharPos, FTextLocation OriginCursorLocation);

	FUMLogger				 Logger;
	EVimMode				 CurrentVimMode{ EVimMode::Insert };
	EVimMode				 PreviousVimMode{ EVimMode::Insert };
	const FModifierKeysState ModShiftDown =
		FModifierKeysState(true, true, /*ShiftDown*/
			false, false, false, false, false, false, false);

	TWeakPtr<SWidget>					ActiveEditableGeneric{ nullptr };
	TWeakPtr<SEditableText>				ActiveEditableText{ nullptr };
	TWeakPtr<SMultiLineEditableText>	ActiveMultiLineEditableText{ nullptr };
	TWeakPtr<SEditableTextBox>			ActiveEditableTextBox{ nullptr };
	TWeakPtr<SMultiLineEditableTextBox> ActiveMultiLineEditableTextBox{ nullptr };
	EUMEditableWidgetsFocusState		EditableWidgetsFocusState{ EUMEditableWidgetsFocusState::None };
	FTextLocation						StartCursorLocationVisualMode;
	FText								DefaultHintText = FText::GetEmpty();
	FUMYankData							YankData;
	FOnKeyDown							OnEditableKeyDown;
	bool								bIsCurrMultiLineChildOfConsole;
	bool								bFindPreviousChar{ false };

	const FText	  InsertModeHintText = FText::FromString("Start Typing... ('Esc'-> Normal Mode)");
	const FText	  NormalModeHintText = FText::FromString("Press 'i' to Start Typing...");
	const FString SingleEditableTextType = "SEditableText";
	const FString MultiEditableTextType = "SMultiLineEditableText";
};
