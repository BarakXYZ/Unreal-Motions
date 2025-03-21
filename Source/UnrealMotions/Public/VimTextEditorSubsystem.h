#pragma once

#include "Framework/Application/SlateApplication.h"
#include "UMLogger.h"
#include "EditorSubsystem.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Text/SMultiLineEditableText.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "VimInputProcessor.h"
#include "VimTextEditorSubsystem.generated.h"

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

	void UpdateEditables();

	void ToggleReadOnly(bool bNegateCurrentState = false, bool bHandleBlinking = true);
	void ToggleReadOnlySingle(bool bNegateCurrentState = false, bool bHandleBlinking = true);
	void ToggleReadOnlyMulti(bool bNegateCurrentState = false, bool bHandleBlinking = true);

	void SetNormalModeCursor();

	bool GetActiveEditableTextContent(FString& OutText);
	bool GetSelectedText(FString& OutText);

	bool IsCursorAlignedRight(FSlateApplication& SlateApp);

	bool SetActiveEditableTextContent(const FText& InText);

	bool RetrieveActiveEditableCursorBlinking();

	bool SelectAllActiveEditableText();

	void OnEditableFocusLost();

	bool IsNewEditableText(const TSharedRef<SWidget> NewEditableText);
	bool IsDefaultEditableBuffer(const FString& InBuffer);

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

	void ClearTextSelection(bool bKeepInputInNormalMode = true);

	void ToggleCursorBlinkingOff();
	bool IsEditableTextWithDefaultBuffer();

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

	bool IsMultiLineCursorAtEndOfDocument();
	bool IsMultiLineCursorAtEndOfDocument(FSlateApplication& SlateApp, const TSharedRef<SMultiLineEditableTextBox> InMultiLine);

	bool IsMultiLineCursorAtEndOrBeginningOfLine();
	bool IsMultiLineCursorAtEndOfLine();

	bool IsCursorAtBeginningOfLine();
	bool IsMultiLineCursorAtBeginningOfLine();
	bool IsSingleLineCursorAtBeginningOfLine();

	bool IsCursorAtEndOfLine(FSlateApplication& SlateApp);
	bool IsCursorAtEndOfLineSingle(FSlateApplication& SlateApp);
	bool IsCursorAtEndOfLineMulti(FSlateApplication& SlateApp);

	void SwitchInsertToNormalMultiLine(FSlateApplication& SlateApp);

	void Delete(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);
	void DeleteLine(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);
	void DeleteLineSingle(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);
	void DeleteLineMulti(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);
	void ShiftDeleteNormalMode(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	void AppendNewLine(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);
	bool AppendBreakMultiLine();

	EUMSelectionState GetSelectionState();

	void DeleteCurrentLineContent(FSlateApplication& SlateApp);
	void DeleteCurrentSelection(FSlateApplication& SlateApp);

	void ChangeEntireLine(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);
	void ChangeToEndOfLine(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);
	void ChangeVisualMode(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	void AddDebuggingText(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	bool TrackVisualModeStartLocation();

	FUMLogger		   Logger;
	EVimMode		   CurrentVimMode{ EVimMode::Insert };
	EVimMode		   PreviousVimMode{ EVimMode::Insert };
	FModifierKeysState ModKeysShiftDown =
		FModifierKeysState(true, true, /*ShiftDown*/
			false, false, false, false, false, false, false);

	TWeakPtr<SWidget>					ActiveEditableGeneric{ nullptr };
	TWeakPtr<SEditableText>				ActiveEditableText{ nullptr };
	TWeakPtr<SMultiLineEditableText>	ActiveMultiLineEditableText{ nullptr };
	TWeakPtr<SEditableTextBox>			ActiveEditableTextBox{ nullptr };
	TWeakPtr<SMultiLineEditableTextBox> ActiveMultiLineEditableTextBox{ nullptr };
	EUMEditableWidgetsFocusState		EditableWidgetsFocusState{ EUMEditableWidgetsFocusState::None };
	FTextLocation						VisualModeStartLocation;
};
