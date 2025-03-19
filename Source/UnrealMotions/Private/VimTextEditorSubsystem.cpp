#include "VimTextEditorSubsystem.h"
#include "Framework/Application/SlateApplication.h"
#include "VimInputProcessor.h"
#include "UMInputHelpers.h"
#include "Editor.h"
#include "UMConfig.h"
#include "UMSlateHelpers.h"

// DEFINE_LOG_CATEGORY_STATIC(LogVimTextEditorSubsystem, NoLogging, All);
DEFINE_LOG_CATEGORY_STATIC(LogVimTextEditorSubsystem, Log, All);

bool UVimTextEditorSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return FUMConfig::Get()->IsVimEnabled();
}

void UVimTextEditorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Logger = FUMLogger(&LogVimTextEditorSubsystem);

	BindCommands();
	Super::Initialize(Collection);

	FCoreDelegates::OnPostEngineInit
		.AddUObject(this, &UVimTextEditorSubsystem::RegisterSlateEvents);
}

void UVimTextEditorSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UVimTextEditorSubsystem::OnVimModeChanged(const EVimMode NewVimMode)
{
	PreviousVimMode = CurrentVimMode;
	CurrentVimMode = NewVimMode;
	UpdateEditables();
}

void UVimTextEditorSubsystem::RegisterSlateEvents()
{
	FSlateApplication& SlateApp = FSlateApplication::Get();

	SlateApp.OnFocusChanging()
		.AddUObject(this, &UVimTextEditorSubsystem::OnFocusChanged);
}

void UVimTextEditorSubsystem::OnFocusChanged(
	const FFocusEvent& FocusEvent, const FWeakWidgetPath& OldWidgetPath,
	const TSharedPtr<SWidget>& OldWidget, const FWidgetPath& NewWidgetPath,
	const TSharedPtr<SWidget>& NewWidget)
{
	static const FName EditableTextType = "SEditableText";
	static const FName MultiEditableTextType = "SMultiLineEditableText";
	static const FName EditableTextBoxType = "SEditableTextBox";
	static const FName MultiEditableTextBoxType = "SMultiLineEditableTextBox";

	// TODO: Might wanna do an early return if we're in Insert mode.

	// Logger.ToggleLogging(true);
	if (NewWidget.IsValid())
	{
		const TSharedRef<SWidget> NewWidgetRef = NewWidget.ToSharedRef();
		Logger.Print("On Focus Changed -> Editable");
		// Check if Single-Line Editable Text & Track
		if (NewWidget->GetType().IsEqual(EditableTextType))
		{
			Logger.Print("Found SingleLine");
			if (IsNewEditableText(NewWidgetRef))
			{
				Logger.Print("New SingleLine");
				OnEditableFocusLost();
				ActiveEditableGeneric = NewWidget; // Track as Generic
				EditableWidgetsFocusState =
					EUMEditableWidgetsFocusState::SingleLine; // Track Mode

				TSharedPtr<SEditableText> EditableText =
					StaticCastSharedPtr<SEditableText>(NewWidget);
				ActiveEditableText = EditableText; // Track Editable

				Logger.Print("SEditableText Found", ELogVerbosity::Verbose);
			}
			else
			{
				Logger.Print("Same SingleLine");
				return;
			}
		}
		// Check if Multi-Line Editable Text & Track
		else if (NewWidget->GetType().IsEqual(MultiEditableTextType))
		{
			if (IsNewEditableText(NewWidgetRef))
			{
				OnEditableFocusLost();

				ActiveEditableGeneric = NewWidget; // Track as Generic
				EditableWidgetsFocusState =
					EUMEditableWidgetsFocusState::MultiLine; // Track Mode

				TSharedPtr<SMultiLineEditableText> MultiEditableText =
					StaticCastSharedPtr<SMultiLineEditableText>(NewWidget);
				ActiveMultiLineEditableText = MultiEditableText; // Track Editable

				Logger.Print("SMultiLineEditableText Found", ELogVerbosity::Verbose);
			}
			else
				return;
		}
		else // No Editables found:
		{
			// Check if any editables we're focused before, and handle them.
			OnEditableFocusLost();
			EditableWidgetsFocusState = EUMEditableWidgetsFocusState::None;
			Logger.Print("None-Editable");
			return; // Early return Non-Editables.
		}

		// Climb to the EditableTextBox parent
		TSharedPtr<SWidget> Parent =
			NewWidget->GetParentWidget(); // Usually SBox or some wrapper
		if (!Parent.IsValid())
			return;
		Parent = Parent->GetParentWidget(); // Usually SHorizontalBox
		if (!Parent.IsValid())
			return;
		// The actual SEditableTextBox or SMultiLineEditableTextBoxs
		Parent = Parent->GetParentWidget();
		if (!Parent.IsValid())
			return;

		Logger.Print("Climbed to parent");

		// NOTE: We can not look up the specific types, as they aren't represented
		// as Multi/SingleEditText, but by some other wrappers like "SSearchBox",
		// "SFilterSearchBoxImpl", etc. But they inherit from these Box types.

		if (EditableWidgetsFocusState == EUMEditableWidgetsFocusState::SingleLine)
		{
			if (Parent->GetWidgetClass().GetWidgetType().IsEqual("SBorder"))
			{
				TSharedPtr<SEditableTextBox> TextBox =
					StaticCastSharedPtr<SEditableTextBox>(Parent);
				ActiveEditableTextBox = TextBox;
				TextBox->SetSelectAllTextWhenFocused(false);
				TextBox->SetSelectAllTextOnCommit(false);
				Logger.Print("Set Active Editable Text Box");
			}
		}
		else // Multi-Line
		// else if (EditableWidgetsFocusState == EUMEditableWidgetsFocusState::MultiLine)
		{
			if (Parent->GetWidgetClass().GetWidgetType().IsEqual("SBorder"))
			{
				TSharedPtr<SMultiLineEditableTextBox> MultiTextBox =
					StaticCastSharedPtr<SMultiLineEditableTextBox>(Parent);
				ActiveMultiLineEditableTextBox = MultiTextBox;
				Logger.Print("Set Active MultiEditable Text Box");
			}
		}

		UpdateEditables(); // We early return if Non-Editables, so this is safe.
	}
}

// TODO: Refactor this, it's not very useful as we're handling everything
// through SetNormalModeCursor.
void UVimTextEditorSubsystem::UpdateEditables()
{
	switch (EditableWidgetsFocusState)
	{
		case EUMEditableWidgetsFocusState::None:
			break;

		case EUMEditableWidgetsFocusState::SingleLine:
			SetNormalModeCursor();
			break;

		case EUMEditableWidgetsFocusState::MultiLine:
			SetNormalModeCursor();
			break;
	}
}

void UVimTextEditorSubsystem::ToggleReadOnly(bool bNegateCurrentState)
{
	switch (EditableWidgetsFocusState)
	{
		case EUMEditableWidgetsFocusState::None:
			break;

		case EUMEditableWidgetsFocusState::SingleLine:
			ToggleReadOnlySingle(bNegateCurrentState);
			break;

		case EUMEditableWidgetsFocusState::MultiLine:
			ToggleReadOnlyMulti(bNegateCurrentState);
			break;
	}
}

void UVimTextEditorSubsystem::ToggleReadOnlySingle(bool bNegateCurrentState)
{
	if (const TSharedPtr<SEditableTextBox> TextBox = ActiveEditableTextBox.Pin())
	{
		if (bNegateCurrentState)
			TextBox->SetIsReadOnly(!TextBox->IsReadOnly());
		else
			TextBox->SetIsReadOnly(CurrentVimMode != EVimMode::Insert);

		// This coloring helps to unify the look of the editable text box. Still
		// WIP. There are some edge cases like Node Titles that still need some
		// testing.
		// TextBox->SetTextBoxBackgroundColor(
		// 	FSlateColor(FLinearColor::Black));
		// TextBox->SetReadOnlyForegroundColor(
		// 	FSlateColor(FLinearColor::White));

		// NOTE:
		// Unlike the MultiLineEditable we don't have GetCursorLocation
		// so we can't really know where we were for the GoTo() *~*
		// So the hack I found to mitigate the blinking vs. non-blinking
		// cursor not changing upon turnning IsReadOnly ON / OFF is to just
		// BeginSearch with an empty FText (LOL).
		// This seems to keep the cursor in the same location while aligning
		// the blinking cursor with the current mode (Normal vs. Insert) :0
		TextBox->BeginSearch(FText::GetEmpty());
	}
}

void UVimTextEditorSubsystem::ToggleReadOnlyMulti(bool bNegateCurrentState)
{
	if (const TSharedPtr<SMultiLineEditableTextBox> MultiTextBox =
			ActiveMultiLineEditableTextBox.Pin())
	{

		if (bNegateCurrentState)
		{
			// The MultiLineEditableTextBox does not override the ReadOnly getter
			// from the raw MultiLine Editable, does we need to get it manually
			// and check for the current ReadOnly value to negate it.
			TSharedPtr<SWidget> FoundMultiLine;
			if (!FUMSlateHelpers::TraverseFindWidget(MultiTextBox.ToSharedRef(), FoundMultiLine, "SMultiLineEditableText"))
				return;

			TSharedPtr<SMultiLineEditableText> MultiLine = StaticCastSharedPtr<SMultiLineEditableText>(FoundMultiLine);

			MultiTextBox->SetIsReadOnly(!MultiLine->IsTextReadOnly());
		}
		else
			MultiTextBox->SetIsReadOnly(CurrentVimMode != EVimMode::Insert);

		// MultiTextBox->SetTextBoxBackgroundColor(
		// 	FSlateColor(FLinearColor::Black));
		// MultiTextBox->SetReadOnlyForegroundColor(
		// 	FSlateColor(FLinearColor::White));

		// NOTE:
		// We set this dummy GoTo to refresh the cursor blinking to ON / OFF
		MultiTextBox->GoTo(MultiTextBox->GetCursorLocation());
	}
}
void UVimTextEditorSubsystem::SetNormalModeCursor()
{
	Logger.Print("Set Normal Mode Cursor", ELogVerbosity::Log, true);

	FString TextContent;
	if (!GetActiveEditableTextContent(TextContent))
		return;

	// In Insert mode:
	// 1. We clear the buffer if default setup ("  ")
	// 2. Or simply just clear any previous selection.
	if (CurrentVimMode == EVimMode::Insert)
	{
		if (IsDefaultEditableBuffer(TextContent)) // Clear default buffer if any
		{
			SetActiveEditableTextContent(FText::GetEmpty()); // Clear
			RetrieveActiveEditableCursorBlinking();
		}
		else
			ClearTextSelection(false /*Insert*/); // (2) Clear previous selection

		ToggleReadOnly();
		return;
	}

	// Normal & Visual Mode:

	// 1) If empty; set default dummy buffer ("  ") for visualization & select it:
	if (TextContent.IsEmpty())
	{
		ToggleReadOnly();
		SetActiveEditableTextContent(FText::FromString("  "));

		SelectAllActiveEditableText();
		// NOTE:
		// There's a slight delay that occurs sometimes when focusing
		// for example on the finder in the Content Browser // Preferences
		// This delay is important to set the text properly.
		FTimerHandle TimerHandle;
		GEditor->GetTimerManager()->SetTimer(
			TimerHandle, [this]() {
				if (!DoesActiveEditableHasAnyTextSelected())
				{
					SetActiveEditableTextContent(FText::FromString("  "));
					SelectAllActiveEditableText();
				}
			},
			0.025f, false);
	}

	// 2) There's 1 custom CHAR in buffer; so we can just select all.
	else if (TextContent.Len() == 1)
	{
		ToggleReadOnly();
		SelectAllActiveEditableText();
	}

	// 3) There are multiple custom chars, potentially words, etc.
	else
	{
		// Again delaying the processing as it seems to be needed in order
		// to process correctly
		FTimerHandle TimerHandle;
		GEditor->GetTimerManager()->SetTimer(
			TimerHandle,
			[this]() {
				FSlateApplication& SlateApp = FSlateApplication::Get();

				// This is important in order to mitigate a potential
				// Stack Overflow that seems to occur in Preferences
				// SearchBox
				if (SlateApp.IsProcessingInput())
					return;

				switch (GetSelectionState())
				{
					case (EUMSelectionState::None):
						break;

					case (EUMSelectionState::OneChar):
						return; // No need to do anything
					//
					case (EUMSelectionState::ManyChars):
					{
						if ((!IsCurrentLineInMultiEmpty()
								|| EditableWidgetsFocusState == EUMEditableWidgetsFocusState::SingleLine)
							&& !IsCursorAlignedRight(SlateApp))
						{
							ClearTextSelection();
							ToggleReadOnly();

							FVimInputProcessor::Get()->SimulateKeyPress(SlateApp, EKeys::Right, FUMInputHelpers::GetShiftDownModKeys());

							return;
						}
					}
				}

				ClearTextSelection();
				ToggleReadOnly();

				// Align cursor left or right depending on it's location
				if (EditableWidgetsFocusState == EUMEditableWidgetsFocusState::MultiLine)
					SwitchInsertToNormalMultiLine(SlateApp);
				else // Align cursor to the right in all other scenarios
					SetCursorSelectionToDefaultLocation(SlateApp);
			},
			0.025f, false);
	}
}

void UVimTextEditorSubsystem::OnEditableFocusLost()
{
	switch (EditableWidgetsFocusState)
	{
		case EUMEditableWidgetsFocusState::None:
		{
			break;
		}

		case EUMEditableWidgetsFocusState::SingleLine:
		{
			if (const auto EditTextBox = ActiveEditableTextBox.Pin())
			{
				const FString Text = EditTextBox->GetText().ToString();
				if (IsDefaultEditableBuffer(Text))
					EditTextBox->SetText(FText::GetEmpty());

				EditTextBox->SetIsReadOnly(false);
				ActiveEditableText.Reset();
				ActiveEditableGeneric.Reset();
			}

			break;
		}
		case EUMEditableWidgetsFocusState::MultiLine:
		{
			if (const TSharedPtr<SMultiLineEditableTextBox> MultiTextBox =
					ActiveMultiLineEditableTextBox.Pin())
			{
				const FString Text = MultiTextBox->GetText().ToString();
				if (IsDefaultEditableBuffer(Text))
					MultiTextBox->SetText(FText::GetEmpty());
				MultiTextBox->SetIsReadOnly(false);
				ActiveMultiLineEditableTextBox.Reset();
				ActiveEditableGeneric.Reset();
			}
			break;
		}
	}
}

bool UVimTextEditorSubsystem::IsNewEditableText(
	const TSharedRef<SWidget> NewEditableText)
{
	if (const TSharedPtr<SWidget> OldEditableText = ActiveEditableGeneric.Pin())
	{
		if (NewEditableText->GetId() == OldEditableText->GetId())
			return false; // ID match -> same editable widget.
		return true;	  // ID don't match -> incoming new one.
	}
	else // Invalid old one -> new incoming one.
		return true;
}

bool UVimTextEditorSubsystem::IsDefaultEditableBuffer(const FString& InBuffer)
{
	return (InBuffer.Len() == 2 && InBuffer == "  ");
}

void UVimTextEditorSubsystem::HandleVimTextNavigation(
	FSlateApplication& SlateApp, const TArray<FInputChord>& InSequence)
{
	if (CurrentVimMode == EVimMode::Insert
		|| IsEditableTextWithDefaultBuffer()) // Preserve default buffer sel
		return;

	DebugMultiLineCursorLocation(true /*Pre-Navigation*/);

	const bool bIsVisualMode = CurrentVimMode == EVimMode::Visual;

	TSharedRef<FVimInputProcessor> Input = FVimInputProcessor::Get();
	FKey						   ArrowKeyToSimulate;
	FUMInputHelpers::GetArrowKeyFromVimKey(InSequence.Last().Key, ArrowKeyToSimulate);
	FKey Left = EKeys::Left;
	FKey Right = EKeys::Right;

	// NOTE:
	// We're processing and simulating in a very specifc order because we need
	// to have a determinstic placement for our cursor at any given time.
	// We want to keep our cursor essentially always to the right of each
	// character so we can handle beginning & end of line properly to maintain
	// correct selection.
	// Another small note is that simulating keys seems a bit loosy-goosy
	// sometimes (for example, it seems like if we had another simulate left
	// when going left, it won't really effect the selection?) But this combo
	// below seems to work well for now.

	if (ArrowKeyToSimulate == Left)
	{
		if (!IsCurrentLineInMultiEmpty())
		{
			if (EditableWidgetsFocusState == EUMEditableWidgetsFocusState::MultiLine && IsMultiLineCursorAtBeginningOfLine())
				return; // Shouldn't continue navigation per Vim rules

			if (bIsVisualMode)
			{
				switch (GetSelectionState())
				{
					case (EUMSelectionState::None):
					{
						SetCursorSelectionToDefaultLocation(SlateApp);
						break;
					}
					case (EUMSelectionState::OneChar):
					{
						// We will need to align left to retain the anchor
						// selection and also highlight the next left char.
						// Thus, we're breaking, then aligning left, then
						// moving to the next char.
						if (IsCursorAlignedRight(SlateApp))
						{
							ClearTextSelection();
							Input->SimulateKeyPress(SlateApp, Left, ModKeysShiftDown);
						}
						break;
					}
					default:
						break;
				}
				Input->SimulateKeyPress(SlateApp, Left, ModKeysShiftDown);
			}
			else // Normal Mode
			{
				ClearTextSelection(); // Break from current selection

				// Because we're right aligned, we need to sim left to go to
				// the beginning of the currently selected char:
				// Given Asterisk (*) == Cursor Location
				// Pre: Exa*mple | Post: Ex*ample
				Input->SimulateKeyPress(SlateApp, Left);

				// GoTo previous char, highlight it (and align to the right)
				SetCursorSelectionToDefaultLocation(SlateApp);
			}
		}
	}
	else if (ArrowKeyToSimulate == Right)
	{
		if (EditableWidgetsFocusState == EUMEditableWidgetsFocusState::MultiLine
			&& IsMultiLineCursorAtEndOfLine())
			return; // Shouldn't continue navigation per Vim rules

		if (!IsCurrentLineInMultiEmpty())
		{
			if (!bIsVisualMode)
			{
				ClearTextSelection();					  // Break from curr sel
				Input->SimulateKeyPress(SlateApp, Right); // GoTo next char
				Input->SimulateKeyPress(SlateApp, Left);  // Prep right alignment
			}
			else
			{
				switch (GetSelectionState())
				{
					case (EUMSelectionState::None):
					{
						SetCursorSelectionToDefaultLocation(SlateApp);
						break;
					}
					case (EUMSelectionState::OneChar):
					{
						// We will need to align right to retain the anchor
						// selection and also highlight the next left right.
						// Thus, we're breaking, then aligning right, then
						// moving to the next char.
						if (!IsCursorAlignedRight(SlateApp))
						{
							ClearTextSelection();
							Input->SimulateKeyPress(SlateApp, Right, ModKeysShiftDown);
						}
						break;
					}
					default:
						break;
				}
			}
			Input->SimulateKeyPress(SlateApp, Right, ModKeysShiftDown); // Sel
		}
	}

	//					** Handle Up & Down **
	// for now we can process them although maybe we
	// souldn't in Single-Line Editable? Not sure what's a better UX.
	else
	{
		if (bIsVisualMode)
			Input->SimulateKeyPress(SlateApp, ArrowKeyToSimulate, ModKeysShiftDown);
		else if (EditableWidgetsFocusState == EUMEditableWidgetsFocusState::MultiLine) // Multi-Line Normal Mode
		{
			NavigateUpDownMultiLine(SlateApp, ArrowKeyToSimulate);
		}
		else // Single-line -> Just go up or down
			Input->SimulateKeyPress(SlateApp, ArrowKeyToSimulate);
	}

	DebugMultiLineCursorLocation(false /*Post-Navigation*/);
}

void UVimTextEditorSubsystem::ClearTextSelection(bool bKeepInputInNormalMode)
{
	switch (EditableWidgetsFocusState)
	{
		case EUMEditableWidgetsFocusState::None:
		{
			Logger.Print("NONE", ELogVerbosity::Log, true);
			break;
		}

		case EUMEditableWidgetsFocusState::SingleLine:
		{
			if (const auto EditTextBox = ActiveEditableTextBox.Pin())
			{
				// NOTE:
				// Magic sequence - I've tried other combinations but this seems
				// to be the only combo to properly clears the selection from
				// Normal Mode (read-only)
				EditTextBox->ClearSelection();
				EditTextBox->SetIsReadOnly(false);
				EditTextBox->BeginSearch(FText::GetEmpty()); // Retrieve blinking
				if (bKeepInputInNormalMode)
				{
					EditTextBox->SetIsReadOnly(true);
					ToggleCursorBlinkingOff();
				}
			}

			break;
		}
		case EUMEditableWidgetsFocusState::MultiLine:
		{
			if (const TSharedPtr<SMultiLineEditableTextBox> MultiTextBox =
					ActiveMultiLineEditableTextBox.Pin())
			{
				MultiTextBox->ClearSelection();
				MultiTextBox->SetIsReadOnly(false);
				MultiTextBox->BeginSearch(FText::GetEmpty()); // Retrieve blinking
				if (bKeepInputInNormalMode)
				{
					MultiTextBox->SetIsReadOnly(true);
					ToggleCursorBlinkingOff();
				}
			}
			break;
		}
	}
}

void UVimTextEditorSubsystem::ToggleCursorBlinkingOff()
{
	TSharedRef<FVimInputProcessor> Processor = FVimInputProcessor::Get();
	FSlateApplication&			   SlateApp = FSlateApplication::Get();

	Processor->SimulateKeyPress(SlateApp, EKeys::Left, FUMInputHelpers::GetShiftDownModKeys());
	if (DoesActiveEditableHasAnyTextSelected())
	{
		// Simply compensate by moving one time to the right (opposite direction)
		Processor->SimulateKeyPress(SlateApp, EKeys::Right /*No Shift Down*/);
	}
	// else we're at the beginning of the input, and we can simply do nothing
	// as the blinking stopped by this point.
}

bool UVimTextEditorSubsystem::IsEditableTextWithDefaultBuffer()
{
	switch (EditableWidgetsFocusState)
	{
		case EUMEditableWidgetsFocusState::None:
			break; // Theoretically not reachable

		case EUMEditableWidgetsFocusState::SingleLine:
			if (const auto EditTextBox = ActiveEditableTextBox.Pin())
				return IsDefaultEditableBuffer(EditTextBox->GetText().ToString());

		case EUMEditableWidgetsFocusState::MultiLine:
			if (const TSharedPtr<SMultiLineEditableTextBox> MultiTextBox =
					ActiveMultiLineEditableTextBox.Pin())
				return IsDefaultEditableBuffer(MultiTextBox->GetText().ToString());
	}
	return false;
}

void UVimTextEditorSubsystem::InsertAndAppend(
	FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	const FKey					   InKey = InKeyEvent.GetKey();
	const bool					   bIsShiftDown = InKeyEvent.IsShiftDown();
	TSharedRef<FVimInputProcessor> InputProcessor = FVimInputProcessor::Get();
	const bool					   bIsVisualMode = CurrentVimMode == EVimMode::Visual;

	if (InKey == EKeys::I)
	{
		if (bIsShiftDown)
		{
			if (bIsVisualMode || IsCurrentLineInMultiEmpty())
				ClearTextSelection(false); // Simply clear and insert
			else
			{
				// Go to the beginning of the line.
				GotoStartOrEnd(SlateApp, FUMInputHelpers::GetKeyEventFromKey(EKeys::G, false /*ShiftDown*/));

				// See explanation below ("By default...")
				InputProcessor->SimulateKeyPress(SlateApp, EKeys::Left);
			}
		}
		else
		{
			if (bIsVisualMode)
				return; // Do nothing. AFAIK that's Vim behavior.
			else if (IsCurrentLineInMultiEmpty())
				ClearTextSelection(false); // Simply clear and insert in place.
			else
				// By default we're keeping the cursor to the right of the curr
				// selected character. So we need to simulate one time left to
				// mimic insert behavior (which puts the cursor *before* the
				// currently selected character).
				InputProcessor->SimulateKeyPress(SlateApp, EKeys::Left);
		}
	}
	else // [a/A]ppend
	{
		if (bIsShiftDown)
		{
			if (bIsVisualMode)
				ClearTextSelection(false); // Simply clear and insert
			else
			{
				// Go to the end of the line.
				GotoStartOrEnd(SlateApp, FUMInputHelpers::GetKeyEventFromKey(EKeys::G, true /*ShiftDown*/));
			}
		}
		else
		{
			if (bIsVisualMode)
				return; // Do nothing. AFAIK that's Vim behavior.

			// Else, just goto Insert Mode:
			// Default behavior is in the correct cursor location already.
		}
	}
	InputProcessor->SetVimMode(SlateApp, EVimMode::Insert);
}

// TODO: Update practice to conform with new methods for Multiline
void UVimTextEditorSubsystem::GotoStartOrEnd(
	FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	if (IsEditableTextWithDefaultBuffer())
		return;

	const bool bIsMultiline = EditableWidgetsFocusState == EUMEditableWidgetsFocusState::MultiLine;

	if (InKeyEvent.IsShiftDown()) // Shift Down (i.e. Shift + G -> aka GoTo End)
	{
		if (bIsMultiline)
			HandleGoToEndMultiLine(SlateApp);

		else // Single-Line
			HandleGoToEndSingleLine(SlateApp);
	}
	else // gg -> GoTo Start
	{
		if (bIsMultiline)
			HandleGoToStartMultiLine(SlateApp);

		else // Single-Line
			HandleGoToStartSingleLine(SlateApp);
	}
}

void UVimTextEditorSubsystem::HandleGoToEndMultiLine(FSlateApplication& SlateApp)
{
	const TSharedPtr<SMultiLineEditableTextBox> MultiTextBox =
		ActiveMultiLineEditableTextBox.Pin();
	if (!MultiTextBox.IsValid())
		return; // Bail if invalid Multiline Widget

	if (CurrentVimMode == EVimMode::Visual)
		HandleVisualModeGoToStartOrEndMultiLine(SlateApp, false /*Go-To-End*/);

	else // Normal Mode
	{
		MultiTextBox->GoTo(ETextLocation::EndOfDocument);
		SetCursorSelectionToDefaultLocation(SlateApp /*RightAlign*/);
	}

	// TODO:
	// There are a few bugs that we should take care of:
	// 1. In Normal & Visual Mode, when trying to go up after going to end, we're
	// skipping the one before last line.
}

void UVimTextEditorSubsystem::HandleGoToStartMultiLine(FSlateApplication& SlateApp)
{
	const TSharedPtr<SMultiLineEditableTextBox> MultiTextBox =
		ActiveMultiLineEditableTextBox.Pin();
	if (!MultiTextBox.IsValid())
		return;

	if (CurrentVimMode == EVimMode::Visual)
		HandleVisualModeGoToStartOrEndMultiLine(SlateApp, true /*Go-To-Start*/);

	else
	{
		MultiTextBox->GoTo(ETextLocation::BeginningOfDocument);
		FString CurrTextLine;
		SetCursorSelectionToDefaultLocation(SlateApp, false /*Left Align*/);
	}
}

void UVimTextEditorSubsystem::HandleVisualModeGoToStartOrEndMultiLine(FSlateApplication& SlateApp, bool bGoToStart)
{
	TSharedRef<FVimInputProcessor> InputProc = FVimInputProcessor::Get();
	const FModifierKeysState	   ShiftDownMod = FUMInputHelpers::GetShiftDownModKeys();
	const FKey					   NavDir = bGoToStart ? EKeys::Up : EKeys::Down;

	// We should stop when the selection isn't changing anymore, which is
	// an indication that we've reached the end of the document.
	// Until then, we can Shift + Down to continue selecting lines downwards.
	FString SelectedText = "";
	GetSelectedText(SelectedText);
	int32 SelectionSize = 0;
	do
	{
		SelectionSize = SelectedText.Len();
		InputProc->SimulateKeyPress(SlateApp, NavDir, ShiftDownMod);
		GetSelectedText(SelectedText);
	}
	while (SelectionSize != SelectedText.Len());
}

void UVimTextEditorSubsystem::HandleGoToEndSingleLine(FSlateApplication& SlateApp)
{
	const bool bIsVisualMode = CurrentVimMode == EVimMode::Visual;

	FVimInputProcessor::Get()->SimulateKeyPress(SlateApp, EKeys::End,
		// If Visual Mode, we should simulate shift down to append
		// to selection new text to the current selection
		bIsVisualMode ? FUMInputHelpers::GetShiftDownModKeys() : FModifierKeysState());

	if (!bIsVisualMode)
		SetCursorSelectionToDefaultLocation(SlateApp);
}

void UVimTextEditorSubsystem::HandleGoToStartSingleLine(FSlateApplication& SlateApp)
{
	const bool bIsVisualMode = CurrentVimMode == EVimMode::Visual;

	TSharedRef<FVimInputProcessor> InputProc = FVimInputProcessor::Get();
	InputProc->SimulateKeyPress(SlateApp, EKeys::Home,
		// If Visual Mode, we should simulate shift down to append
		// to selection new text to the current selection
		bIsVisualMode
			? FUMInputHelpers::GetShiftDownModKeys()
			: FModifierKeysState());

	if (!bIsVisualMode)
		InputProc->SimulateKeyPress(
			SlateApp, EKeys::Right, FUMInputHelpers::GetShiftDownModKeys());
}

void UVimTextEditorSubsystem::SetCursorSelectionToDefaultLocation(FSlateApplication& SlateApp, bool bAlignCursorRight)
{
	TSharedRef<FVimInputProcessor> InputProcessor = FVimInputProcessor::Get();
	const FModifierKeysState	   ModKeys = FUMInputHelpers::GetShiftDownModKeys();
	if (bAlignCursorRight)
	{ // Right Align: for most cases
		InputProcessor->SimulateKeyPress(SlateApp, EKeys::Left);
		InputProcessor->SimulateKeyPress(SlateApp, EKeys::Right, ModKeys);
	}
	else // Left Align: for end of document scenarios
	{
		InputProcessor->SimulateKeyPress(SlateApp, EKeys::Right);
		InputProcessor->SimulateKeyPress(SlateApp, EKeys::Left, ModKeys);
	}
}

bool UVimTextEditorSubsystem::DoesActiveEditableHasAnyTextSelected()
{
	switch (EditableWidgetsFocusState)
	{
		case EUMEditableWidgetsFocusState::None:
			break; // Theoretically not reachable

		case EUMEditableWidgetsFocusState::SingleLine:
			if (const auto EditTextBox = ActiveEditableTextBox.Pin())
				return EditTextBox->AnyTextSelected();

		case EUMEditableWidgetsFocusState::MultiLine:
			if (const TSharedPtr<SMultiLineEditableTextBox> MultiTextBox =
					ActiveMultiLineEditableTextBox.Pin())
				return MultiTextBox->AnyTextSelected();
	}
	return false;
}

bool UVimTextEditorSubsystem::GetActiveEditableTextContent(FString& OutText)
{
	switch (EditableWidgetsFocusState)
	{
		case EUMEditableWidgetsFocusState::None:
			break; // Theoretically not reachable

		case EUMEditableWidgetsFocusState::SingleLine:
			if (const auto EditTextBox = ActiveEditableTextBox.Pin())
			{
				OutText = EditTextBox->GetText().ToString();
				return true;
			}

		case EUMEditableWidgetsFocusState::MultiLine:
			if (const TSharedPtr<SMultiLineEditableTextBox> MultiTextBox =
					ActiveMultiLineEditableTextBox.Pin())
			{
				OutText = MultiTextBox->GetText().ToString();
				return true;
			}
	}
	return false;
}

bool UVimTextEditorSubsystem::GetSelectedText(FString& OutText)
{
	switch (EditableWidgetsFocusState)
	{
		case EUMEditableWidgetsFocusState::None:
			break; // Theoretically not reachable

		case EUMEditableWidgetsFocusState::SingleLine:
			if (const auto EditTextBox = ActiveEditableTextBox.Pin())
			{
				OutText = EditTextBox->GetSelectedText().ToString();
				return true;
			}

		case EUMEditableWidgetsFocusState::MultiLine:
			if (const TSharedPtr<SMultiLineEditableTextBox> MultiTextBox =
					ActiveMultiLineEditableTextBox.Pin())
			{
				OutText = MultiTextBox->GetSelectedText().ToString();
				return true;
			}
	}
	return false;
}

bool UVimTextEditorSubsystem::SetActiveEditableTextContent(const FText& InText)
{
	switch (EditableWidgetsFocusState)
	{
		case EUMEditableWidgetsFocusState::None:
			break; // Theoretically not reachable

		case EUMEditableWidgetsFocusState::SingleLine:
			if (const auto EditTextBox = ActiveEditableTextBox.Pin())
			{
				EditTextBox->SetText(InText);
				return true;
			}

		case EUMEditableWidgetsFocusState::MultiLine:
			if (const TSharedPtr<SMultiLineEditableTextBox> MultiTextBox =
					ActiveMultiLineEditableTextBox.Pin())
			{
				MultiTextBox->SetText(InText);
				return true;
			}
	}
	return false;
}

bool UVimTextEditorSubsystem::RetrieveActiveEditableCursorBlinking()
{
	switch (EditableWidgetsFocusState)
	{
		case EUMEditableWidgetsFocusState::None:
			break; // Theoretically not reachable

		case EUMEditableWidgetsFocusState::SingleLine:
			if (const auto EditTextBox = ActiveEditableTextBox.Pin())
			{
				EditTextBox->BeginSearch(FText::GetEmpty());
				return true;
			}

		case EUMEditableWidgetsFocusState::MultiLine:
			if (const TSharedPtr<SMultiLineEditableTextBox> MultiTextBox =
					ActiveMultiLineEditableTextBox.Pin())
			{
				MultiTextBox->BeginSearch(FText::GetEmpty());
				return true;
			}
	}
	return false;
}

bool UVimTextEditorSubsystem::SelectAllActiveEditableText()
{
	switch (EditableWidgetsFocusState)
	{
		case EUMEditableWidgetsFocusState::None:
			break; // Theoretically not reachable

		case EUMEditableWidgetsFocusState::SingleLine:
			if (const auto EditTextBox = ActiveEditableTextBox.Pin())
			{
				EditTextBox->SelectAllText();
				return true;
			}

		case EUMEditableWidgetsFocusState::MultiLine:
			if (const TSharedPtr<SMultiLineEditableTextBox> MultiTextBox =
					ActiveMultiLineEditableTextBox.Pin())
			{
				MultiTextBox->SelectAllText();

				// Useful functions
				// MultiTextBox->InsertTextAtCursor();
				// MultiTextBox->Refresh();
				// MultiTextBox->ScrollTo();
				return true;
			}
	}
	return false;
}

bool UVimTextEditorSubsystem::IsCurrentLineEmpty()
{
	switch (EditableWidgetsFocusState)
	{
		case EUMEditableWidgetsFocusState::None:
			break; // Theoretically not reachable

		case EUMEditableWidgetsFocusState::SingleLine:
			return IsCurrentLineInSingleEmpty();

		case EUMEditableWidgetsFocusState::MultiLine:
			return IsCurrentLineInMultiEmpty();
	}
	return false;
}

bool UVimTextEditorSubsystem::IsCurrentLineInSingleEmpty()
{
	if (const auto EditTextBox = ActiveEditableTextBox.Pin())
	{
		FString Text = EditTextBox->GetText().ToString();
		// return Text.IsEmpty() || IsDefaultEditableBuffer(Text);
		return Text.IsEmpty();
	}
	return false;
}

bool UVimTextEditorSubsystem::IsCurrentLineInMultiEmpty()
{
	if (const TSharedPtr<SMultiLineEditableTextBox> MultiTextBox =
			ActiveMultiLineEditableTextBox.Pin())
	{
		FString CurrTextLine;
		MultiTextBox->GetCurrentTextLine(CurrTextLine);
		// Also we might want to check if default buffer.
		return CurrTextLine.IsEmpty();
	}
	return false;
}

void UVimTextEditorSubsystem::DebugMultiLineCursorLocation(bool bIsPreNavigation, bool bIgnoreDelay)
{
	const TSharedPtr<SMultiLineEditableTextBox> MultiTextBox =
		ActiveMultiLineEditableTextBox.Pin();
	if (!MultiTextBox.IsValid())
		return;

	const FString NavigationContext = bIsPreNavigation
		? "Pre-Navigation"
		: "Post-Navigation";

	FTextLocation TextLocation = MultiTextBox->GetCursorLocation();
	if (!TextLocation.IsValid())
		return;
	int32 LineIndex = TextLocation.GetLineIndex();
	int32 CharOffset = TextLocation.GetOffset();

	// Debug if current line is empty
	FString CurrLineText;
	MultiTextBox->GetCurrentTextLine(CurrLineText);
	FString CurrLineEmptyDebug = CurrLineText.IsEmpty() ? "TRUE" : "FALSE";

	// Debug if the cursor is aligned to the Left or to the Right
	FString AlignedRightDebug = IsCursorAlignedRight(FSlateApplication::Get()) ? "TRUE" : "FALSE";

	Logger.Print(FString::Printf(TEXT("MultiLine %s Location:\nLine Index: %d\nChar Offset: %d\nis Empty Line: %s Content: %s\nis Cursor Aligned Right: %s"),
					 *NavigationContext, LineIndex,
					 CharOffset, *CurrLineEmptyDebug, *CurrLineText, *AlignedRightDebug),
		ELogVerbosity::Log, true);
}

// TODO: Refactor this function (i.e. break it down to smaller functions) and
// fix bugs when navigating to and from end of document.
bool UVimTextEditorSubsystem::NavigateUpDownMultiLine(FSlateApplication& SlateApp, const FKey& InKeyDir)
{
	const TSharedPtr<SMultiLineEditableTextBox> MultiTextBox =
		ActiveMultiLineEditableTextBox.Pin();
	if (!MultiTextBox.IsValid())
		return false;

	TSharedRef<FVimInputProcessor> InputProc = FVimInputProcessor::Get();
	const bool					   bMoveUp = InKeyDir == EKeys::Up;

	// Get current cursor location to determine how we should navigate
	FTextLocation TextLocation = MultiTextBox->GetCursorLocation();
	if (!TextLocation.IsValid())
		return false;
	const int32 CurrLineIndex = TextLocation.GetLineIndex();
	const int32 StartCharOffset = TextLocation.GetOffset();
	FString		StartLineText;
	MultiTextBox->GetCurrentTextLine(StartLineText);
	const bool bStartLineEmpty = StartLineText.IsEmpty();

	ClearTextSelection();

	const int32 NewLineIndex = bMoveUp
		? CurrLineIndex - 1	 // Previous line index
		: CurrLineIndex + 1; // Next line index
	MultiTextBox->GoTo(FTextLocation(NewLineIndex, 0));

	// Check if we've entered an empty line to determine proper movement
	FString NewLineText;
	MultiTextBox->GetCurrentTextLine(NewLineText);
	if (NewLineText.IsEmpty())
	{ // Empty Line Flow:
		// This sequence of right then left+Shift here is important
		// to both highlight the cursor yet stay in the empty line itself
		// for further navigation
		// TODO: Refactor this
		if (IsMultiLineCursorAtEndOfDocument(SlateApp, MultiTextBox.ToSharedRef()))
			SetCursorSelectionToDefaultLocation(SlateApp,
				// false /*Left Align*/); // Right Align isn't working here too
				true /*Right Align*/);
		else
			SetCursorSelectionToDefaultLocation(SlateApp,
				false /*Left Align*/);
	}
	else // Non-empty lines
	{
		if (StartCharOffset > NewLineText.Len())
		{
			InputProc->SimulateKeyPress(SlateApp, EKeys::End);
			SetCursorSelectionToDefaultLocation(SlateApp);
		}
		else
		{
			MultiTextBox->GoTo(FTextLocation(NewLineIndex, StartCharOffset));
			if (bStartLineEmpty)
				InputProc->SimulateKeyPress(SlateApp, EKeys::Right, FUMInputHelpers::GetShiftDownModKeys());
			else
				SetCursorSelectionToDefaultLocation(SlateApp);
		}
	}
	return true;
}

bool UVimTextEditorSubsystem::IsMultiLineCursorAtEndOfDocument()
{
	if (const TSharedPtr<SMultiLineEditableTextBox> MultiTextBox =
			ActiveMultiLineEditableTextBox.Pin())
	{
		return IsMultiLineCursorAtEndOfDocument(FSlateApplication::Get(), MultiTextBox.ToSharedRef());
	}
	return false;
}

bool UVimTextEditorSubsystem::IsMultiLineCursorAtEndOfDocument(FSlateApplication& SlateApp, const TSharedRef<SMultiLineEditableTextBox> InMultiLine)
{
	// Get current text location
	FTextLocation StartTextLocation = InMultiLine->GetCursorLocation();
	if (!StartTextLocation.IsValid())
		return false;

	TSharedRef<FVimInputProcessor> InputProc = FVimInputProcessor::Get();

	// First, we want to simulate down once to see if there are other lines
	// in the document. If there are, we're clearly not at the end of the doc.
	InputProc->SimulateKeyPress(SlateApp, EKeys::Down, ModKeysShiftDown);
	FTextLocation NewTextLocation = InMultiLine->GetCursorLocation();
	if (NewTextLocation.GetLineIndex() > StartTextLocation.GetLineIndex())
	{
		Logger.Print(FString::Printf(TEXT("Not at end of document -> more lines to go\nStart Index: %d\nNext Index: %d"),
						 StartTextLocation.GetLineIndex(),
						 NewTextLocation.GetLineIndex()),
			true);
		// We're not at the end, we can revert to the origin position and return.
		InputProc->SimulateKeyPress(SlateApp, EKeys::Up, ModKeysShiftDown);
		return false;
	}

	// At this point we know we're at the last line of the document.
	// Before checking if we're also at the last char in that line, we should
	// check if this line is empty, because if so the cursor alignment is set
	// in a specific way for highlighting.
	if (IsCurrentLineInMultiEmpty())
	{
		Logger.Print("At End-of-Document - Last line is empty.", true);
		return true;
	}

	// The last line isn't empty, so we should just check if we're at the last
	// char at this line (i.e. the end of the doc)
	FString LineText;
	InMultiLine->GetCurrentTextLine(LineText);
	return LineText.Len() == StartTextLocation.GetOffset();
}

bool UVimTextEditorSubsystem::IsMultiLineCursorAtBeginningOfDocument()
{
	const TSharedPtr<SMultiLineEditableTextBox> MultiTextBox =
		ActiveMultiLineEditableTextBox.Pin();
	if (!MultiTextBox.IsValid())
		return false;

	// Get current text location
	FTextLocation StartTextLocation = MultiTextBox->GetCursorLocation();
	if (!StartTextLocation.IsValid())
		return false;

	return StartTextLocation.GetLineIndex() == 0
		&& StartTextLocation.GetOffset() == 0;
}

bool UVimTextEditorSubsystem::IsMultiLineCursorAtEndOrBeginningOfLine()
{
	return IsMultiLineCursorAtBeginningOfLine()
		|| IsMultiLineCursorAtEndOfLine();
}

bool UVimTextEditorSubsystem::IsMultiLineCursorAtEndOfLine()
{
	const TSharedPtr<SMultiLineEditableTextBox> MultiTextBox =
		ActiveMultiLineEditableTextBox.Pin();
	if (!MultiTextBox.IsValid())
		return false;

	if (IsCurrentLineInMultiEmpty())
		return true;

	// Get current text location
	FTextLocation StartTextLocation = MultiTextBox->GetCursorLocation();
	if (!StartTextLocation.IsValid())
		return false;

	// We're at end if the last char is highlighed (i.e. len == currLocation)
	FString CurrLineText;
	MultiTextBox->GetCurrentTextLine(CurrLineText);

	return CurrLineText.Len() == StartTextLocation.GetOffset();
}

bool UVimTextEditorSubsystem::IsSingleLineCursorAtBeginningOfLine()
{
	if (const auto EditTextBox = ActiveEditableTextBox.Pin())
	{
		FSlateApplication&			   SlateApp = FSlateApplication::Get();
		TSharedRef<FVimInputProcessor> InputProc = FVimInputProcessor::Get();

		if (EditTextBox->AnyTextSelected())
		{
			FString OriginSelText = EditTextBox->GetSelectedText().ToString();
			InputProc->SimulateKeyPress(SlateApp, EKeys::Left, ModKeysShiftDown);
			FString NewSelText = EditTextBox->GetSelectedText().ToString();
			bool	bIsAtBeginningOfLine = OriginSelText.Len() == NewSelText.Len();
			if (!bIsAtBeginningOfLine)
				InputProc->SimulateKeyPress(SlateApp, EKeys::Right, ModKeysShiftDown);
			return bIsAtBeginningOfLine;
		}
		else
		{
			InputProc->SimulateKeyPress(SlateApp, EKeys::Left, ModKeysShiftDown);
			FString NewSelText = EditTextBox->GetSelectedText().ToString();
			bool	bIsAtBeginningOfLine = !EditTextBox->AnyTextSelected();
			if (!bIsAtBeginningOfLine)
				InputProc->SimulateKeyPress(SlateApp, EKeys::Right, ModKeysShiftDown);
			return bIsAtBeginningOfLine;
		}
	}
	return false;
}

bool UVimTextEditorSubsystem::IsCursorAtBeginningOfLine()
{
	switch (EditableWidgetsFocusState)
	{
		case EUMEditableWidgetsFocusState::None:
			break; // Theoretically not reachable

		case EUMEditableWidgetsFocusState::SingleLine:
			return IsSingleLineCursorAtBeginningOfLine();

		case EUMEditableWidgetsFocusState::MultiLine:
			return IsMultiLineCursorAtBeginningOfLine();
	}
	return false;
}

bool UVimTextEditorSubsystem::IsMultiLineCursorAtBeginningOfLine()
{
	const TSharedPtr<SMultiLineEditableTextBox> MultiTextBox =
		ActiveMultiLineEditableTextBox.Pin();
	if (!MultiTextBox.IsValid())
		return false;

	// Get current text location
	FTextLocation StartTextLocation = MultiTextBox->GetCursorLocation();
	if (!StartTextLocation.IsValid())
		return false;
	const int32 CursorOffset = StartTextLocation.GetOffset();
	// NOTE:
	// In Visual Mode we should be allowed to go to the 0th offset location
	// thus the boundary changes to 0 instead of 1.
	const int32 TargetLimit =
		CurrentVimMode == EVimMode::Visual
			// If no text selected, we should also only compare against 0,
			// as we're not yet aligned to the right with highlighting
			|| !MultiTextBox->AnyTextSelected()
		? 0
		: 1;

	// NOTE:
	// Because (in most cases) we align the cursor to the right,
	// we should compensate (i.e. offset) by +1 to properly check if
	// we're at the beginning of the line (as we will always be +1 of it)
	return CursorOffset == TargetLimit;
}

// If we have 2 character selected, we really don't care too much about the
// cursor location, but if we have 1 character selected, it it essential to
// know the character selected to determine how we can preserve highlighting
// correctly.
bool UVimTextEditorSubsystem::IsCursorAlignedRight(FSlateApplication& SlateApp)
{
	FString OriginSelText;
	if (!GetSelectedText(OriginSelText))
		return false;

	if (IsCursorAtEndOfLine(SlateApp))
		return true;

	const TSharedRef<FVimInputProcessor> VimProc = FVimInputProcessor::Get();
	VimProc->SimulateKeyPress(SlateApp, EKeys::Right, FUMInputHelpers::GetShiftDownModKeys());

	FString NewSelText;
	if (!GetSelectedText(NewSelText))
		return false;

	// Revert: return to origin selection
	VimProc->SimulateKeyPress(SlateApp, EKeys::Left, FUMInputHelpers::GetShiftDownModKeys());

	return NewSelText.Len() > OriginSelText.Len();
}

bool UVimTextEditorSubsystem::IsCursorAtEndOfLine(FSlateApplication& SlateApp)
{
	switch (EditableWidgetsFocusState)
	{
		case EUMEditableWidgetsFocusState::None:
			break; // Theoretically not reachable

		case EUMEditableWidgetsFocusState::SingleLine:
			return IsCursorAtEndOfLineSingle(SlateApp);

		case EUMEditableWidgetsFocusState::MultiLine:
			return IsCursorAtEndOfLineMulti(SlateApp);
	}
	return false;
}

bool UVimTextEditorSubsystem::IsCursorAtEndOfLineSingle(FSlateApplication& SlateApp)
{
	const auto EditTextBox = ActiveEditableTextBox.Pin();
	if (!EditTextBox.IsValid())
		return false;

	const FString Text = EditTextBox->GetText().ToString();
	if (Text.IsEmpty())
		return true;

	TSharedRef<FVimInputProcessor> InputProc = FVimInputProcessor::Get();
	const FModifierKeysState	   ModKeys = FUMInputHelpers::GetShiftDownModKeys();

	if (EditTextBox->AnyTextSelected())
	{
		FString OriginSelText = EditTextBox->GetSelectedText().ToString();

		InputProc->SimulateKeyPress(SlateApp, EKeys::Right, ModKeys);
		FString NewSelText = EditTextBox->GetSelectedText().ToString();
		if (NewSelText.Len() > OriginSelText.Len())
		{
			// We've managed to move, thus not at end.
			// Revert to origin position.
			InputProc->SimulateKeyPress(SlateApp, EKeys::Left, ModKeys);
			return false;
		}
		// We haven't moved, thus we're at the end of the line.
		return true;
	}
	else // Initially, no text selected
	{
		InputProc->SimulateKeyPress(SlateApp, EKeys::Right, ModKeys);
		FString NewSelText = EditTextBox->GetSelectedText().ToString();
		if (EditTextBox->AnyTextSelected())
		{
			// We've managed to move, thus not at end.
			// Revert to origin position.
			InputProc->SimulateKeyPress(SlateApp, EKeys::Left, ModKeys);
			return false;
		}
		// We haven't moved, thus we're at the end of the line.
		return true;
	}
}

bool UVimTextEditorSubsystem::IsCursorAtEndOfLineMulti(FSlateApplication& SlateApp)
{
	const TSharedPtr<SMultiLineEditableTextBox> MultiTextBox =
		ActiveMultiLineEditableTextBox.Pin();
	if (!MultiTextBox.IsValid())
		return false;

	if (IsMultiLineCursorAtEndOfDocument(
			FSlateApplication::Get(), MultiTextBox.ToSharedRef()))
		return true;

	// Needed?
	// if (IsCurrentLineInMultiEmpty())
	// 	return true;

	FTextLocation CursorLocation = MultiTextBox->GetCursorLocation();
	if (!CursorLocation.IsValid())
		return false;

	FString LineText;
	MultiTextBox->GetCurrentTextLine(LineText);
	return LineText.Len() == CursorLocation.GetOffset();
}

EUMSelectionState UVimTextEditorSubsystem::GetSelectionState()
{
	FString SelectedText;
	if (!GetSelectedText(SelectedText))
		return EUMSelectionState::None;

	else if (SelectedText.IsEmpty())
		return EUMSelectionState::None;

	else if (SelectedText.Len() == 1)
		return EUMSelectionState::OneChar;

	return EUMSelectionState::ManyChars;
}

void UVimTextEditorSubsystem::SwitchInsertToNormalMultiLine(FSlateApplication& SlateApp)
{
	if (IsMultiLineCursorAtBeginningOfDocument())
		SetCursorSelectionToDefaultLocation(SlateApp,
			false /*Must Align Cursor to the Left*/);

	else if (IsMultiLineCursorAtBeginningOfLine())
	{
		if (IsCurrentLineInMultiEmpty())
			SetCursorSelectionToDefaultLocation(SlateApp,
				IsMultiLineCursorAtEndOfDocument() /*Left-or-Right*/);

		else // Non-Empty line, we can shift down to the right to prop align.
			FVimInputProcessor::Get()->SimulateKeyPress(
				SlateApp, EKeys::Right, ModKeysShiftDown);
	}
	else
		SetCursorSelectionToDefaultLocation(SlateApp);
}

void UVimTextEditorSubsystem::DeleteNormalMode(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	if (IsCurrentLineEmpty())
		return;

	ClearTextSelection(false /*Insert*/);

	TSharedRef<FVimInputProcessor> VimProc = FVimInputProcessor::Get();
	VimProc->SimulateKeyPress(SlateApp, EKeys::Left, ModKeysShiftDown);
	VimProc->SimulateKeyPress(SlateApp, EKeys::Delete);

	if (!IsCurrentLineEmpty()
		&& !IsCursorAtEndOfLine(SlateApp))
		VimProc->SimulateKeyPress(SlateApp, EKeys::Right);

	ClearTextSelection(true /*Normal*/);

	SetNormalModeCursor();
}

void UVimTextEditorSubsystem::ShiftDeleteNormalMode(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	if (IsCurrentLineEmpty() || IsCursorAtBeginningOfLine())
		return;

	ClearTextSelection(false /*Insert*/);

	TSharedRef<FVimInputProcessor> VimProc = FVimInputProcessor::Get();
	VimProc->SimulateKeyPress(SlateApp, EKeys::Left);
	VimProc->SimulateKeyPress(SlateApp, EKeys::Left, ModKeysShiftDown);
	VimProc->SimulateKeyPress(SlateApp, EKeys::Delete);

	if (!IsCurrentLineEmpty()
		&& !IsCursorAtEndOfLine(SlateApp))
		VimProc->SimulateKeyPress(SlateApp, EKeys::Right);

	ClearTextSelection(true /*Normal*/);

	SetNormalModeCursor();
}

void UVimTextEditorSubsystem::BindCommands()
{
	TSharedRef<FVimInputProcessor> VimInputProcessor = FVimInputProcessor::Get();
	VimInputProcessor->OnVimModeChanged.AddUObject(
		this, &UVimTextEditorSubsystem::OnVimModeChanged);

	TWeakObjectPtr<UVimTextEditorSubsystem> WeakTextSubsystem =
		MakeWeakObjectPtr(this);

	//				~ HJKL Navigate Pins & Nodes ~
	//
	// H: Go to Left Pin / Node:
	VimInputProcessor->AddKeyBinding_Sequence(
		EUMBindingContext::TextEditing,
		{ EKeys::H },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::HandleVimTextNavigation);

	// J: Go Down to Next Pin:
	VimInputProcessor->AddKeyBinding_Sequence(
		EUMBindingContext::TextEditing,
		{ EKeys::J },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::HandleVimTextNavigation);

	// K: Go Up to Previous Pin:
	VimInputProcessor->AddKeyBinding_Sequence(
		EUMBindingContext::TextEditing,
		{ EKeys::K },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::HandleVimTextNavigation);

	// L: Go to Right Pin / Node:
	VimInputProcessor->AddKeyBinding_Sequence(
		EUMBindingContext::TextEditing,
		{ EKeys::L },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::HandleVimTextNavigation);
	//
	//				~ HJKL Navigate Pins & Nodes ~

	// [i]nsert & [a]ppend + [I]nsert & [A]ppend (start / end of line)
	//
	VimInputProcessor->AddKeyBinding_KeyEvent(
		EUMBindingContext::TextEditing,
		{ EKeys::I },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::InsertAndAppend);

	VimInputProcessor->AddKeyBinding_KeyEvent(
		EUMBindingContext::TextEditing,
		{ EKeys::A },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::InsertAndAppend);

	VimInputProcessor->AddKeyBinding_KeyEvent(
		EUMBindingContext::TextEditing,
		{ FInputChord(EModifierKey::Shift, EKeys::I) },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::InsertAndAppend);

	VimInputProcessor->AddKeyBinding_KeyEvent(
		EUMBindingContext::TextEditing,
		{ FInputChord(EModifierKey::Shift, EKeys::A) },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::InsertAndAppend);
	//
	// [i]nsert & [a]ppend + [I]nsert & [A]ppend (start / end of line)

	// gg & Shift+g -> Goto to End & Start of line
	VimInputProcessor->AddKeyBinding_KeyEvent(
		EUMBindingContext::TextEditing,
		{ EKeys::G, EKeys::G },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::GotoStartOrEnd);

	VimInputProcessor->AddKeyBinding_KeyEvent(
		EUMBindingContext::TextEditing,
		{ FInputChord(EModifierKey::Shift, EKeys::G) },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::GotoStartOrEnd);

	VimInputProcessor->AddKeyBinding_KeyEvent(
		EUMBindingContext::TextEditing,
		{ EKeys::X },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::DeleteNormalMode,
		EVimMode::Normal);

	VimInputProcessor->AddKeyBinding_KeyEvent(
		EUMBindingContext::TextEditing,
		{ FInputChord(EModifierKey::Shift, EKeys::X) },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::ShiftDeleteNormalMode,
		EVimMode::Normal);
}

/**
 * Known (unsolved) issues:
 * 1) Given the following rows of text:
 * "I'm a short tex(t)"
 * "I'm actually a longer t(e)xt"
 * When trying to navigate between the (e) surrounded with parenthesis to the (t)
 * above it, in Vim, when we try to go down again we expect our cursor to
 * highlight (e), but without some custom tracking, in Unreal this is not what
 * we'll get. The cursor will highlight the closest bottom character instead of
 * smartly knowing from where we came.
 *
 * 2)
 */
