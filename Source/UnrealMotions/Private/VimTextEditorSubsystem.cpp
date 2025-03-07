#include "VimTextEditorSubsystem.h"
#include "Framework/Application/SlateApplication.h"
#include "UMSlateHelpers.h"
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
		TextBox->SetIsReadOnly(CurrentVimMode == EVimMode::Normal);

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

						FModifierKeysState ModKeysNone(false, false, false, false, false, false, false, false, false);
						FModifierKeysState ModKeysShiftDown(
							true, true, /* Shift Down */
							false, false, false, false, false, false, false);

						// TODO: in visual mode we must track our cursor location
						// manually and understand if we're to the right or to
						// the left of the selection to know which keys we should
						// simulate.

						// if any text selected; break from the selection and end
						// up in the same place by simulating 1 arrow key in the
						// direction of our cursor relative to the selection
						// if (EditTextBox->AnyTextSelected())
						// 	FVimInputProcessor::SimulateKeyPress(SlateApp, FKey(EKeys::Left), ModKeysNone); // temp left

						// // Continue in both cases to simulate 1 arrow key to the
						// // opposite direction & another 1 arrow key (shift-down)
						// // to the same direction.
						// FVimInputProcessor::SimulateKeyPress(SlateApp, FKey(EKeys::Right), ModKeysNone); // Temp right

						FVimInputProcessor::SimulateKeyPress(SlateApp, FKey(EKeys::Left), ModKeysShiftDown); // temp left

						// DEPRECATED?
						// If no text selected: Select to the right.
						// We're deducing our location by checking if any text
						// was selected, as there will be no text selected only
						// if we're at the beginning of the line. If so, we can
						// simply simulate to the right to select the first char.
						if (!EditTextBox->AnyTextSelected())
						{
							FVimInputProcessor::SimulateKeyPress(SlateApp, FKey(EKeys::Right), ModKeysShiftDown);
						}
					},
					0.025f, false);

				// EditTextBox->BeginSearch(FText::FromString(" "), ESearchCase::IgnoreCase, true);
				// EditTextBox->GoTo(ETextLocation::EndOfLine);
				// EditTextBox->AdvanceSearch();
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
	if (CurrentVimMode == EVimMode::Normal)
	{
		// It looks like we can clear text selection to always end up in the
		// expected cursor position (left or right to the selection).
		// The flow should be as follows if in Normal mode for the clear to
		// actually take place:
		// FVimInputProcessor::Get()->SetVimMode(FSlateApplication::Get(), EVimMode::Insert);
		// EditTextBox->ClearSelection();
		// FVimInputProcessor::Get()->SetVimMode(FSlateApplication::Get(), EVimMode::Normal);
		// If not switching first to insert, it seems like the clear won't take
		// place. Figure this out.

		ClearTextSelection();
		TSharedRef<FVimInputProcessor> Input = FVimInputProcessor::Get();
		// Input->SimulateKeyPress(SlateApp);
	}
}

void UVimTextEditorSubsystem::ClearTextSelection()
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
				EditTextBox->ClearSelection();
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

	// Simulate clear selection to check where our cursor is at:
	VimInputProcessor->AddKeyBinding_NoParam(
		EUMContextBinding::TextEditing,
		// { FInputChord(EModifierKey::Alt, EKeys::M) },
		{ EKeys::SpaceBar, EKeys::M, EKeys::M, EKeys::M },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::ClearTextSelection);
}
