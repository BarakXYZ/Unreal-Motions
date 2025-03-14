#pragma once

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

	void ToggleReadOnlySingle();
	void ToggleReadOnlyMulti();

	void SetNormalModeCursor();

	bool GetActiveEditableTextContent(FString& OutText);

	bool SetActiveEditableTextContent(const FText& InText);

	bool RetrieveActiveEditableCursorBlinking();

	bool SelectAllActiveEditableText();

	void OnEditableFocusLost();

	bool IsNewEditableText(const TSharedRef<SWidget> NewEditableText);
	bool IsDefaultEditableBuffer(const FString& InBuffer);

	void HandleVimTextNavigation(
		FSlateApplication& SlateApp, const TArray<FInputChord>& InSequence);

	void ClearTextSelection(bool bKeepInputInNormalMode = true);

	void ToggleCursorBlinkingOff();
	bool IsEditableTextWithDefaultBuffer();

	void InsertAndAppend(
		FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	void GotoStartOrEnd(
		FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	void SetCursorSelectionToDefaultLocation(FSlateApplication& SlateApp, bool bAlignCursorRight = true);

	bool DoesActiveEditableHasAnyTextSelected();

	bool IsCurrentLineInMultiEmpty();

	void DebugMultiLineCursorLocation(bool bIsPreNavigation, bool bIgnoreDelay = false);

	bool NavigateUpDownMultiLine(FSlateApplication& SlateApp, const FKey& InKeyDir);

	bool IsMultiLineCursorAtEndOfDocument();
	bool IsMultiLineCursorAtEndOfDocument(FSlateApplication& SlateApp, const TSharedRef<SMultiLineEditableTextBox> InMultiLine);

	bool IsMultiLineCursorAtBeginningOfDocument();

	FUMLogger Logger;
	EVimMode  CurrentVimMode{ EVimMode::Insert };
	EVimMode  PreviousVimMode{ EVimMode::Insert };

	TWeakPtr<SWidget>					ActiveEditableGeneric{ nullptr };
	TWeakPtr<SEditableText>				ActiveEditableText{ nullptr };
	TWeakPtr<SMultiLineEditableText>	ActiveMultiLineEditableText{ nullptr };
	TWeakPtr<SEditableTextBox>			ActiveEditableTextBox{ nullptr };
	TWeakPtr<SMultiLineEditableTextBox> ActiveMultiLineEditableTextBox{ nullptr };
	EUMEditableWidgetsFocusState		EditableWidgetsFocusState{ EUMEditableWidgetsFocusState::None };
};
