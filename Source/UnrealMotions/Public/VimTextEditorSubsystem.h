#pragma once

#include "Brushes/SlateRoundedBoxBrush.h"
#include "Framework/Application/SlateApplication.h"
#include "UMLogger.h"
#include "EditorSubsystem.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Text/SMultiLineEditableText.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "VimInputProcessor.h"
#include "VimTextEditorSubsystem.generated.h"

/**
 * Character type enumeration used for word boundary detection
 */
enum class EUMCharType
{
	Word,	   // Alphanumeric or underscore
	Symbol,	   // Non-word, non-whitespace (punctuation, etc.)
	Whitespace // Space, tab, etc.
};

enum class EUMEditableWidgetsFocusState : uint8
{
	None,
	SingleLine,
	MultiLine
};

enum class EUMSelectionState : uint8
{
	None,
	OneChar,
	ManyChars
};

struct FUMStringInfo
{
	int32 LineCount;
	int32 LastCharIndex;
};

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

	bool GetActiveEditableTextContent(FString& OutText);
	bool GetSelectedText(FString& OutText);

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

	void HandleVimTextNavigation(
		FSlateApplication& SlateApp, const TArray<FInputChord>& InSequence);

	void HandleRightNavigation(
		FSlateApplication& SlateApp, const TArray<FInputChord>& InSequence);
	void HandleRightNavigationSingle(
		FSlateApplication& SlateApp, const TArray<FInputChord>& InSequence);
	void HandleRightNavigationMulti(
		FSlateApplication& SlateApp, const TArray<FInputChord>& InSequence);

	void HandleLeftNavigation(
		FSlateApplication& SlateApp, const TArray<FInputChord>& InSequence);
	void HandleLeftNavigationSingle(
		FSlateApplication& SlateApp, const TArray<FInputChord>& InSequence);
	void HandleLeftNavigationMulti(
		FSlateApplication& SlateApp, const TArray<FInputChord>& InSequence);

	void AbsoluteOffsetToTextLocation(const FString& Text, int32 AbsoluteOffset, FTextLocation& OutLocation);

	int32 TextLocationToAbsoluteOffset(const FString& Text, const FTextLocation& Location);

	int32 GetCursorOffsetSingle();
	void  SetCursorOffsetSingle(int32 NewOffset);

	void NavigateWord(
		FSlateApplication& SlateApp,
		bool			   bBigWord,
		int32 (UVimTextEditorSubsystem::*FindWordBoundary)(
			const FString& Text, int32 CurrentPos, bool bBigWord));

	void ToggleCursorBlinkingOff();
	bool IsEditableTextWithDefaultBuffer();
	void SetDefaultBuffer();
	void HandleInsertMode(const FString& InCurrentText);
	void HandleNormalModeOneChar();
	void HandleNormalModeMultipleChars();
	void HandleNormalModeMultipleChars(float InDelay);
	bool TrySetHintTextForVimMode();

	void InsertAndAppend(
		FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	//////////////////////////////////////////////////////////////////////////
	// ~			GoTo Start or End (gg + Shift + G)				~
	//
	void GotoStartOrEnd(
		FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	void HandleGoToStartMultiLine(FSlateApplication& SlateApp);
	void HandleGoToEndMultiLine(FSlateApplication& SlateApp);
	void HandleVisualModeGoToStartOrEndMultiLine(FSlateApplication& SlateApp, bool bGoToStart);

	void HandleGoToStartOrEndSingleLine(FSlateApplication& SlateApp, bool bGoToEnd);
	//
	//
	//////////////////////////////////////////////////////////////////////////

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
	void DeleteLineSingle(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);
	void DeleteLineNormalModeSingle(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);
	void DeleteLineMulti(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);
	void DeleteLineNormalModeMulti(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);
	void ShiftDeleteNormalMode(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	void AppendNewLine(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);
	bool AppendBreakMultiLine();

	EUMSelectionState GetSelectionState();

	void DeleteCurrentLineContent(FSlateApplication& SlateApp);
	void DeleteCurrentSelection(FSlateApplication& SlateApp);

	void ChangeEntireLine(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);
	void ChangeEntireLineMulti(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	void ChangeToEndOfLine(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);
	void ChangeVisualMode(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	void AddDebuggingText(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	bool TrackVisualModeStartLocation();

	void AlignCursorToIndex(FSlateApplication& SlateApp, int32 CurrIndex, int32 AlignToIndex, bool bAlignRight);

	bool SelectTextInRange(FSlateApplication& SlateApp, const FTextLocation& StartLocation, const FTextLocation& EndLocation, bool bJumpToStart = true);

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

	void AssignEditableBorder(bool bAssignDefaultBorder = false);

	int32 GetMultiLineCount();

	//							~ Word Navigation ~
	//
	bool IsWordChar(TCHAR Char);

	// Public word boundary functions

	// Helper functions

	/**
	 * Determines the character type at a given position in text
	 *
	 * @param Text - The string to analyze
	 * @param Position - The position to check
	 * @return The character type
	 */
	EUMCharType GetCharType(const FString& Text, int32 Position);

	/**
	 * Skips consecutive characters of the specified type in the given direction
	 *
	 * @param Text - The string to traverse
	 * @param StartPos - The starting position
	 * @param Direction - 1 for forward, -1 for backward
	 * @param TypeToSkip - The character type to skip
	 * @return The new position after skipping
	 */
	int32 SkipCharType(const FString& Text, int32 StartPos, int32 Direction, EUMCharType TypeToSkip);

	/**
	 * Skips consecutive non-whitespace characters in the given direction
	 *
	 * @param Text - The string to traverse
	 * @param StartPos - The starting position
	 * @param Direction - 1 for forward, -1 for backward
	 * @return The new position after skipping
	 */
	int32 SkipNonWhitespace(const FString& Text, int32 StartPos, int32 Direction);

	/**
	 * Skips whitespace in the given direction
	 *
	 * @param Text - The string to traverse
	 * @param StartPos - The starting position
	 * @param Direction - 1 for forward, -1 for backward
	 * @return The new position after skipping whitespace
	 */
	int32 SkipWhitespace(const FString& Text, int32 StartPos, int32 Direction);

	/**
	 * Returns the absolute offset (0-based) for the next word boundary.
	 * bBigWord == false means using "small word" rules (w), where punctuation is a delimiter;
	 * bBigWord == true means whitespace-delimited words (W).
	 *
	 * @param Text - The string to search
	 * @param CurrentPos - The current position
	 * @param bBigWord - Whether to use "big word" rules
	 * @return The position of the next word boundary
	 */
	int32 FindNextWordBoundary(const FString& Text, int32 CurrentPos, bool bBigWord);

	/**
	 * Returns the absolute offset for the previous word boundary.
	 *
	 * @param Text - The string to search
	 * @param CurrentPos - The current position
	 * @param bBigWord - Whether to use "big word" rules
	 * @return The position of the previous word boundary
	 */
	int32 FindPreviousWordBoundary(const FString& Text, int32 CurrentPos, bool bBigWord);

	/**
	 * Finds the next boundary of a "big word" (W)
	 *
	 * @param Text - The string to traverse
	 * @param CurrentPos - The current position
	 * @return The position of the next big word boundary
	 */
	int32 FindNextBigWordBoundary(const FString& Text, int32 CurrentPos);

	/**
	 * Finds the previous boundary of a "big word" (B)
	 *
	 * @param Text - The string to traverse
	 * @param CurrentPos - The current position
	 * @return The position of the previous big word boundary
	 */
	int32 FindPreviousBigWordBoundary(const FString& Text, int32 CurrentPos);

	/**
	 * Finds the next boundary of a "small word" (w)
	 *
	 * @param Text - The string to traverse
	 * @param CurrentPos - The current position
	 * @return The position of the next small word boundary
	 */
	int32 FindNextSmallWordBoundary(const FString& Text, int32 CurrentPos);

	/**
	 * Finds the previous boundary of a "small word" (b)
	 *
	 * @param Text - The string to traverse
	 * @param CurrentPos - The current position
	 * @return The position of the previous small word boundary
	 */
	int32 FindPreviousSmallWordBoundary(const FString& Text, int32 CurrentPos);

	// New functions for word ending

	/**
	 * Returns the absolute offset (0-based) for the next word end.
	 * bBigWord == false means using "small word" rules (e), where punctuation is a delimiter;
	 * bBigWord == true means whitespace-delimited words (E).
	 *
	 * @param Text - The string to search
	 * @param CurrentPos - The current position
	 * @param bBigWord - Whether to use "big word" rules
	 * @return The position of the next word end
	 */
	int32 FindNextWordEnd(const FString& Text, int32 CurrentPos, bool bBigWord);

	/**
	 * Returns the absolute offset for the previous word end.
	 *
	 * @param Text - The string to search
	 * @param CurrentPos - The current position
	 * @param bBigWord - Whether to use "big word" rules
	 * @return The position of the previous word end
	 */
	int32 FindPreviousWordEnd(const FString& Text, int32 CurrentPos, bool bBigWord);

	/**
	 * Finds the next ending of a "big word" (E)
	 *
	 * @param Text - The string to traverse
	 * @param CurrentPos - The current position
	 * @return The position of the next big word end
	 */
	int32 FindNextBigWordEnd(const FString& Text, int32 CurrentPos);

	/**
	 * Finds the previous ending of a "big word" (gE)
	 *
	 * @param Text - The string to traverse
	 * @param CurrentPos - The current position
	 * @return The position of the previous big word end
	 */
	int32 FindPreviousBigWordEnd(const FString& Text, int32 CurrentPos);

	/**
	 * Finds the next ending of a "small word" (e)
	 *
	 * @param Text - The string to traverse
	 * @param CurrentPos - The current position
	 * @return The position of the next small word end
	 */
	int32 FindNextSmallWordEnd(const FString& Text, int32 CurrentPos);

	/**
	 * Finds the previous ending of a "small word" (ge)
	 *
	 * @param Text - The string to traverse
	 * @param CurrentPos - The current position
	 * @return The position of the previous small word end
	 */
	int32 FindPreviousSmallWordEnd(const FString& Text, int32 CurrentPos);

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

	FUMLogger		   Logger;
	EVimMode		   CurrentVimMode{ EVimMode::Insert };
	EVimMode		   PreviousVimMode{ EVimMode::Insert };
	FModifierKeysState ModShiftDown =
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

	const FText	  InsertModeHintText = FText::FromString("Start Typing... ('Esc'-> Normal Mode)");
	const FText	  NormalModeHintText = FText::FromString("Press 'i' to Start Typing...");
	const FString SingleEditableTextType = "SEditableText";
	const FString MultiEditableTextType = "SMultiLineEditableText";
};
