#include "VimTextEditorSubsystem.h"
#include "Framework/Application/SlateApplication.h"
#include "UMSlateHelpers.h"
#include "VimInputProcessor.h"
#include "Widgets/InvalidateWidgetReason.h"
#include "UMInputHelpers.h"
#include "Editor.h"
#include "UMConfig.h"

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

	switch (CurrentVimMode)
	{
		case EVimMode::Insert:
		{
			UpdateEditables();
			break;
		}
		case EVimMode::Normal:
		{
			UpdateEditables();
			break;
		}
		case EVimMode::Visual:
		{
			UpdateEditables();
			break;
		}
	}
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

	// TODO: Might wanna do an early return if we're on Insert mode.

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

void UVimTextEditorSubsystem::UpdateEditables()
{
	switch (EditableWidgetsFocusState)
	{
		case EUMEditableWidgetsFocusState::None:
			break;

		case EUMEditableWidgetsFocusState::SingleLine:
			SetCursorSingle();
			ToggleReadOnlySingle();
			Logger.Print("Handle Single-Line");
			break;

		case EUMEditableWidgetsFocusState::MultiLine:
			SetCursorMulti();
			ToggleReadOnlyMulti();
			break;
	}
}

void UVimTextEditorSubsystem::ToggleReadOnlySingle()
{
	if (const TSharedPtr<SEditableTextBox> TextBox = ActiveEditableTextBox.Pin())
	{
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

void UVimTextEditorSubsystem::ToggleReadOnlyMulti()
{
	if (const TSharedPtr<SMultiLineEditableTextBox> MultiTextBox =
			ActiveMultiLineEditableTextBox.Pin())
	{
		MultiTextBox->SetIsReadOnly(CurrentVimMode == EVimMode::Normal);
		// MultiTextBox->SetTextBoxBackgroundColor(
		// 	FSlateColor(FLinearColor::Black));
		// MultiTextBox->SetReadOnlyForegroundColor(
		// 	FSlateColor(FLinearColor::White));

		// NOTE:
		// We set this dummy GoTo to refresh the cursor blinking to ON / OFF
		MultiTextBox->GoTo(MultiTextBox->GetCursorLocation());
	}
}

void UVimTextEditorSubsystem::SetCursorSingle()
{
	if (const auto EditTextBox = ActiveEditableTextBox.Pin())
	{
		const FString& Text = EditTextBox->GetText().ToString();

		// In Insert mode:
		// 1. We clear the buffer if default setup ("  ")
		// 2. Or simply just clear any previous selection.
		if (CurrentVimMode == EVimMode::Insert)
		{
			if (IsDefaultEditableBuffer(Text)) // (1) Clear default buffer if any
			{
				EditTextBox->SetText(FText::GetEmpty());	 // Clear
				EditTextBox->BeginSearch(FText::GetEmpty()); // Retrieve blinking
			}
			else
				EditTextBox->ClearSelection(); // (2) Clear previous selection
		}
		else if (CurrentVimMode == EVimMode::Normal || CurrentVimMode == EVimMode::Visual)
		{
			// 1) If empty; set default dummy buffer ("  ") for visualization & select it:
			if (Text.IsEmpty())
			{
				EditTextBox->SetText(FText::FromString("  "));

				EditTextBox->SelectAllText();
				// NOTE:
				// There's a slight delay that occurs sometimes when focusing
				// for example on the finder in the Content Browser.
				// This delay is important to set the text properly.
				FTimerHandle TimerHandle;
				GEditor->GetTimerManager()->SetTimer(
					TimerHandle, [this]() {
						if (const TSharedPtr<SEditableTextBox> EditTextBox =
								ActiveEditableTextBox.Pin())
							EditTextBox->SelectAllText();
					},
					0.025f, false);
			}

			// 2) There's 1 custom CHAR in buffer; so we can just select all.
			else if (Text.Len() == 1)
				EditTextBox->SelectAllText();

			// 3) There are multiple custom CHARS, potentially words, etc.
			else
			{
				// Again delaying the processing as it seems to be needed in order
				// to process correctly
				FTimerHandle TimerHandle;
				GEditor->GetTimerManager()->SetTimer(
					TimerHandle,
					[this]() {
						const TSharedPtr<SEditableTextBox> EditTextBox =
							ActiveEditableTextBox.Pin();
						if (!EditTextBox.IsValid())
							return;

						FSlateApplication& SlateApp = FSlateApplication::Get();

						// This is important in order to mitigate a potential
						// Stack Overflow that seems to occur in Preferences
						// SearchBox
						if (SlateApp.IsProcessingInput())
							return;

						EditTextBox->ClearSelection();

						// New method -> always keep cursor to the right of the
						// selection.
						FVimInputProcessor::SimulateKeyPress(SlateApp, FKey(EKeys::Left));
						FVimInputProcessor::SimulateKeyPress(SlateApp, FKey(EKeys::Right), FUMInputHelpers::GetShiftDownModKeys());
					},
					0.025f, false);
			}
		}
	}
}

void UVimTextEditorSubsystem::SetCursorMulti()
{
	if (const TSharedPtr<SMultiLineEditableTextBox> MultiTextBox =
			ActiveMultiLineEditableTextBox.Pin())
	{
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
	Logger.Print("Handle Vim Text Navigation", ELogVerbosity::Verbose, true);
	if (CurrentVimMode != EVimMode::Insert)
	{
		const bool bIsVisualMode = CurrentVimMode == EVimMode::Visual;

		if (IsEditableTextWithDefaultBuffer())
			return; // Early return to preserve the default buffer selection.

		TSharedRef<FVimInputProcessor> Input = FVimInputProcessor::Get();
		FKey						   ArrowKeyToSimulate;
		FUMInputHelpers::GetArrowKeyFromVimKey(InSequence.Last().Key, ArrowKeyToSimulate);
		FModifierKeysState ModKeysShiftDown = FUMInputHelpers::GetShiftDownModKeys();
		FKey			   Left = EKeys::Left;
		FKey			   Right = EKeys::Right;

		// NOTE:
		// We're processing and simulating in a very specifc order because we need
		// to have a determinstic placement for our cursor at any given time.
		// We want to keep our cursor essentially always to the right of each
		// character so we can handle beginning & end of line properly to maintain
		// correct selection.
		// Another small note is that simulating keys seems a bit loosy-goosy
		// sometimes (for example, it seems like if we had another simulate left
		// when going left, it won't really effect the selection?) But this combo
		// below seems to work well for all tested cases.
		if (ArrowKeyToSimulate == Left)
		{
			if (!bIsVisualMode)
			{
				ClearTextSelection();
				Input->SimulateKeyPress(SlateApp, Left);
				Input->SimulateKeyPress(SlateApp, Left);
				Input->SimulateKeyPress(SlateApp, Right, ModKeysShiftDown);
			}
			else
			{
				Input->SimulateKeyPress(SlateApp, Left, ModKeysShiftDown);
			}
		}
		else if (ArrowKeyToSimulate == Right)
		{
			if (!bIsVisualMode)
			{
				ClearTextSelection();
				Input->SimulateKeyPress(SlateApp, Right);
				Input->SimulateKeyPress(SlateApp, Left);
			}
			Input->SimulateKeyPress(SlateApp, Right, ModKeysShiftDown);
		}

		// Up & Down: for now we can process them although maybe we
		// souldn't in Single-Line Editable? Not sure what's a better UX.
		else
		{
			if (bIsVisualMode)
				Input->SimulateKeyPress(SlateApp, ArrowKeyToSimulate, ModKeysShiftDown);
			else if (EditableWidgetsFocusState == EUMEditableWidgetsFocusState::MultiLine) // Multi-Line Normal Mode
			{
				ClearTextSelection();
				Input->SimulateKeyPress(SlateApp, ArrowKeyToSimulate);
				Input->SimulateKeyPress(SlateApp, Left);
				Input->SimulateKeyPress(SlateApp, Right, ModKeysShiftDown);
			}
			else // Single-line -> Just go up or down
				Input->SimulateKeyPress(SlateApp, ArrowKeyToSimulate);
		}
	}
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
				FTimerHandle			  TimerHandle;
				TSharedRef<FTimerManager> TimerManager = GEditor->GetTimerManager();

				// Magic sequence - I've tried other combinations but this seems
				// to be the only combo to properly clears the selection from
				// Normal Mode (read-only)
				EditTextBox->ClearSelection();
				EditTextBox->SetIsReadOnly(false);
				EditTextBox->BeginSearch(FText::GetEmpty()); // Retrieve blinking
				if (bKeepInputInNormalMode)
				{
					EditTextBox->SetIsReadOnly(true);
					ToggleCursorBlinkingOff(EditTextBox.ToSharedRef());
				}

				// To remove blinking again:
				// TimerManager->SetTimer(
				// 	TimerHandle,
				// 	[]() {
				// 		FVimInputProcessor::Get()->SimulateKeyPress(FSlateApplication::Get(), EKeys::Left);
				// 	},
				// 	0.25f, false);
			}

			break;
		}
		case EUMEditableWidgetsFocusState::MultiLine:
		{
			if (const TSharedPtr<SMultiLineEditableTextBox> MultiTextBox =
					ActiveMultiLineEditableTextBox.Pin())
				MultiTextBox->ClearSelection();

			break;
		}
	}
}

void UVimTextEditorSubsystem::ToggleCursorBlinkingOff(TSharedRef<SEditableTextBox> InEditableTextBox)
{
	FModifierKeysState ModKeysNone(false, false, false, false, false, false, false, false, false);
	FModifierKeysState ModKeysShiftDown(
		true, true, /* Shift Down */
		false, false, false, false, false, false, false);

	TSharedRef<FVimInputProcessor> Processor = FVimInputProcessor::Get();
	FSlateApplication&			   SlateApp = FSlateApplication::Get();

	Processor->SimulateKeyPress(SlateApp, EKeys::Left, ModKeysShiftDown);
	if (InEditableTextBox->AnyTextSelected())
	{
		// Logger.Print("Has Text Selected", ELogVerbosity::Log, true);
		// Simply compensate by moving one time to the right (opposite direction)
		Processor->SimulateKeyPress(SlateApp, EKeys::Right /*No Shift Down*/);
	}
	// else we're at the beginning of the input, and we can simply do nothing
	// as the blinking stopped by this point.

	// Keeping this for fast access in case we need some timers going
	// FTimerHandle				   TimerHandle;
	// TSharedRef<FTimerManager>	   TimerManager = GEditor->GetTimerManager();
	// TimerManager->SetTimer(
	// 	TimerHandle,
	// 	[]() {

	// 	},
	// 	0.025f, false);
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
			if (bIsVisualMode)
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

			// By default we're keeping the cursor to the right of the currently
			// selected character. So we need to simulate one time to the left to
			// mimic insert behavior (which puts the cursor *before* the currently
			// selected character).
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

void UVimTextEditorSubsystem::GotoStartOrEnd(
	FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	if (IsEditableTextWithDefaultBuffer())
		return;

	const FKey					   InKey = InKeyEvent.GetKey();
	const bool					   bIsShiftDown = InKeyEvent.IsShiftDown();
	TSharedRef<FVimInputProcessor> InputProcessor = FVimInputProcessor::Get();
	const bool					   bIsVisualMode = CurrentVimMode == EVimMode::Visual;

	// TODO: Add some more logic for Multi-Line Editable Text

	if (bIsShiftDown)
	{
		InputProcessor->SimulateKeyPress(SlateApp, EKeys::End,
			bIsVisualMode ? FUMInputHelpers::GetShiftDownModKeys() : FModifierKeysState());
	}
	else
	{
		InputProcessor->SimulateKeyPress(SlateApp, EKeys::Home,
			bIsVisualMode ? FUMInputHelpers::GetShiftDownModKeys() : FModifierKeysState());
	}
	if (!bIsVisualMode)
		SetCursorSelectionToDefaultLocation(SlateApp);
}

void UVimTextEditorSubsystem::SetCursorSelectionToDefaultLocation(FSlateApplication& SlateApp)
{
	TSharedRef<FVimInputProcessor> InputProcessor = FVimInputProcessor::Get();
	InputProcessor->SimulateKeyPress(SlateApp, EKeys::Left);
	InputProcessor->SimulateKeyPress(SlateApp, EKeys::Right, FUMInputHelpers::GetShiftDownModKeys());
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
		EUMContextBinding::TextEditing,
		// { FInputChord(EModifierKey::Shift, EKeys::H) },
		{ EKeys::H },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::HandleVimTextNavigation);

	// J: Go Down to Next Pin:
	VimInputProcessor->AddKeyBinding_Sequence(
		EUMContextBinding::TextEditing,
		// { FInputChord(EModifierKey::Shift, EKeys::J) },
		{ EKeys::J },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::HandleVimTextNavigation);

	// K: Go Up to Previous Pin:
	VimInputProcessor->AddKeyBinding_Sequence(
		EUMContextBinding::TextEditing,
		// { FInputChord(EModifierKey::Shift, EKeys::K) },
		{ EKeys::K },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::HandleVimTextNavigation);

	// L: Go to Right Pin / Node:
	VimInputProcessor->AddKeyBinding_Sequence(
		EUMContextBinding::TextEditing,
		// { FInputChord(EModifierKey::Shift, EKeys::L) },
		{ EKeys::L },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::HandleVimTextNavigation);
	//
	//				~ HJKL Navigate Pins & Nodes ~

	// [i]nsert & [a]ppend + [I]nsert & [A]ppend (start / end of line)
	//
	VimInputProcessor->AddKeyBinding_KeyEvent(
		EUMContextBinding::TextEditing,
		{ EKeys::I },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::InsertAndAppend);

	VimInputProcessor->AddKeyBinding_KeyEvent(
		EUMContextBinding::TextEditing,
		{ EKeys::A },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::InsertAndAppend);

	VimInputProcessor->AddKeyBinding_KeyEvent(
		EUMContextBinding::TextEditing,
		{ FInputChord(EModifierKey::Shift, EKeys::I) },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::InsertAndAppend);

	VimInputProcessor->AddKeyBinding_KeyEvent(
		EUMContextBinding::TextEditing,
		{ FInputChord(EModifierKey::Shift, EKeys::A) },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::InsertAndAppend);
	//
	// [i]nsert & [a]ppend + [I]nsert & [A]ppend (start / end of line)

	// gg & Shift+g -> Goto to End & Start of line
	VimInputProcessor->AddKeyBinding_KeyEvent(
		EUMContextBinding::TextEditing,
		{ EKeys::G, EKeys::G },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::GotoStartOrEnd);

	VimInputProcessor->AddKeyBinding_KeyEvent(
		EUMContextBinding::TextEditing,
		{ FInputChord(EModifierKey::Shift, EKeys::G) },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::GotoStartOrEnd);
}

// TODO:
// Vim Navigation in Single-Line Editable Text - V
// Proper handling for [i]nsert and [a]ppend - V
// Go to end and start of input Single-Line - V
// Go to end and start of input Multi-Line -
// Vim Navigation in Multi-Line Editable Text -
