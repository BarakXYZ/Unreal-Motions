#include "VimTextEditorSubsystem.h"
#include "Async/TaskTrace.h"
#include "Framework/Application/SlateApplication.h"
#include "GenericPlatform/GenericApplication.h"
#include "VimInputProcessor.h"
#include "UMInputHelpers.h"
#include "Editor.h"
#include "UMConfig.h"
#include "VimTextEditorUtils.h"
#include "UMSlateHelpers.h"
#include "VimTextTypes.h"

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

	OnSingleLineEditableKeyDown.BindUObject(
		this, &UVimTextEditorSubsystem::OnSingleLineKeyDown);

	// Handle KeyDown in Multis in a custom way to intercept and process 'Enter'
	// manually.
	OnMultiLineEditableKeyDown.BindUObject(
		this, &UVimTextEditorSubsystem::OnMultiLineKeyDown);
}

void UVimTextEditorSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UVimTextEditorSubsystem::OnVimModeChanged(const EVimMode NewVimMode)
{
	PreviousVimMode = CurrentVimMode;
	CurrentVimMode = NewVimMode;
	FSlateApplication& SlateApp = FSlateApplication::Get();

	if (NewVimMode == EVimMode::Visual)
		TrackVisualModeStartLocation(SlateApp);

	// We should skip this when initializing as our editable is still stablizing.
	if (!bIsEditableInit)
	{
		RefreshActiveEditable(SlateApp);
		AssignEditableBorder();
		HandleEditableUX();
	}
}

void UVimTextEditorSubsystem::RefreshActiveEditable(FSlateApplication& SlateApp)
{
	// Store origin location to come back to as we auto-jump to
	// the end of the document
	FTextLocation OriginLocation;
	if (!GetCursorLocation(SlateApp, OriginLocation))
		return;

	// if we were aligned left, we want to normalize the GoTo
	// location 1 time to the right to properly select the character
	// in Normal Mode.
	const bool bWasCursorAlignedLeft = !IsCursorAlignedRight(SlateApp);

	// SetText to cache the most up-to-date input properly
	// This is needed to navigate the text in its most recent form.
	FString CurrTextContent;
	GetActiveEditableTextContent(CurrTextContent);
	SetActiveEditableText(FText::FromString(CurrTextContent));

	// GoTo to the original location we've started at.
	GoToTextLocation(SlateApp,
		FTextLocation(OriginLocation.GetLineIndex(),
			bWasCursorAlignedLeft ? OriginLocation.GetOffset() + 1 // Normalize
								  : OriginLocation.GetOffset()));
}

void UVimTextEditorSubsystem::AssignEditableBorder(bool bAssignDefaultBorder, const EVimMode OptVimModeOverride)
{
	const EVimMode VimMode =
		bAssignDefaultBorder
		? EVimMode::Any /*OnFocusLost*/
		: OptVimModeOverride == EVimMode::Any
		? CurrentVimMode
		: OptVimModeOverride;

	switch (EditableWidgetsFocusState)
	{
		case EUMEditableWidgetsFocusState::SingleLine:
		{
			if (const auto TextBox = ActiveEditableTextBox.Pin())
				TextBox->SetBorderImage(&GetBorderBrush(VimMode));
			return;
		}
		case EUMEditableWidgetsFocusState::MultiLine:
		{
			if (const TSharedPtr<SMultiLineEditableTextBox> MultiTextBox =
					ActiveMultiLineEditableTextBox.Pin())
				MultiTextBox->SetBorderImage(&GetBorderBrush(VimMode));
			return;
		}
		default:
			break;
	}
}

bool UVimTextEditorSubsystem::TrackVisualModeStartLocation(FSlateApplication& SlateApp)
{
	return GetCursorLocation(SlateApp, StartCursorLocationVisualMode);
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
	static bool bAlreadyProcessingFocusChange = false;
	// Guard against recursion (stack overflow issues)
	if (bAlreadyProcessingFocusChange)
		return;

	// TODO: Might wanna do an early return if we're in Insert mode.

	// Logger.ToggleLogging(true);
	if (!NewWidget.IsValid())
		return;

	const TSharedRef<SWidget> NewWidgetRef = NewWidget.ToSharedRef();
	Logger.Print("On Focus Changed -> Editable");
	// Check if Single-Line Editable Text & Track
	if (NewWidget->GetType().ToString().Equals(SingleEditableTextType))
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
	else if (NewWidget->GetType().ToString().Equals(MultiEditableTextType))
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

	// Flag we're not free to digest any new focus change until finish processing
	bAlreadyProcessingFocusChange = true;

	// This is another important flag to properly initialize SingleLines
	bIsEditableInit = true;

	// NOTE:
	// At first encounter, single lines seems to have trouble with the first
	// keystroke input, requiring 2 strokes to actually input. This is annoying
	// so we're trying to mitigate this with some syntatic dummy stroke to
	// end up with 1 regular stroke instead of 2.
	bIsFirstSingleLineKeyStroke = true;

	if (EditableWidgetsFocusState == EUMEditableWidgetsFocusState::SingleLine)
	{
		if (Parent->GetWidgetClass().GetWidgetType().IsEqual("SBorder"))
		{
			// I feel like it's a better UX to enter Insert mode for SingleLines
			// as the user probably expect quick instant typing XP
			FVimInputProcessor::Get()->SetVimMode(FSlateApplication::Get(), EVimMode::Insert);

			TSharedPtr<SEditableTextBox> TextBox =
				StaticCastSharedPtr<SEditableTextBox>(Parent);
			ActiveEditableTextBox = TextBox;

			TextBox->SetOnKeyDownHandler(OnSingleLineEditableKeyDown);

			FText HintText = TextBox->GetHintText();
			if (!HintText.IsEmpty())
				DefaultHintText = HintText;

			// I think that for SingleLine it's a better UX to leave these off
			// TextBox->SetSelectAllTextWhenFocused(false);
			// TextBox->SetSelectAllTextOnCommit(false);
			Logger.Print("Set Active Editable Text Box");
		}
	}
	else // Multi-Line
	{
		if (Parent->GetWidgetClass().GetWidgetType().IsEqual("SBorder"))
		{
			TSharedPtr<SMultiLineEditableTextBox> MultiTextBox =
				StaticCastSharedPtr<SMultiLineEditableTextBox>(Parent);
			ActiveMultiLineEditableTextBox = MultiTextBox;

			const TSharedPtr<SMultiLineEditableText>
				MultiText = GetMultilineEditableFromBox(MultiTextBox.ToSharedRef());
			if (MultiText.IsValid())
			{
				MultiText->SetClearTextSelectionOnFocusLoss(false);
			}

			// NOTE:
			// In one hand this solves the annoying unexpected behavior of Enter
			// in MultiLine text by intercepting the Enter key and sending a
			// proper \n (break-line) instead of weird commit / select all.
			// On the other hand, we do expect Enter to commit in Console Multis.
			// So, either we'll need to detect when we're at a console vs. an
			// editable, or...
			// Are there any other places like Console that need this commision?
			bIsCurrMultiLineChildOfConsole = IsMultiChildOfConsole(MultiTextBox.ToSharedRef());
			MultiTextBox->SetOnKeyDownHandler(OnMultiLineEditableKeyDown);

			FText HintText = MultiTextBox->GetHintText();
			if (!HintText.IsEmpty())
				DefaultHintText = HintText;

			Logger.Print("Set Active MultiEditable Text Box");
		}
	}

	SetEditableUnifiedStyle();
	AssignEditableBorder();
	HandleEditableUX(); // We early return if Non-Editables, so this is safe.

	// After finishing all setup, we're now free to digest a new focus change
	bAlreadyProcessingFocusChange = false;
}

void UVimTextEditorSubsystem::SetEditableUnifiedStyle(const float InDelay)
{
	FTimerHandle TimerHandle;
	GEditor->GetTimerManager()->SetTimer(
		TimerHandle,
		[this]() {
			SetEditableUnifiedStyle();
		},
		InDelay, false);
}
void UVimTextEditorSubsystem::SetEditableUnifiedStyle()
{
	switch (EditableWidgetsFocusState)
	{
		case EUMEditableWidgetsFocusState::SingleLine:
		{
			if (const auto TextBox = ActiveEditableTextBox.Pin())
				SetEditableUnifiedStyleSingleLine(TextBox.ToSharedRef());
			break;
		}
		case EUMEditableWidgetsFocusState::MultiLine:
		{
			if (const TSharedPtr<SMultiLineEditableTextBox> MultiTextBox =
					ActiveMultiLineEditableTextBox.Pin())
				SetEditableUnifiedStyleMultiLine(MultiTextBox.ToSharedRef());
			break;
		}
		default:
			break;
	}
}

void UVimTextEditorSubsystem::SetEditableUnifiedStyleSingleLine(
	const TSharedRef<SEditableTextBox> InTextBox)
{
	InTextBox->SetTextBoxBackgroundColor(
		FSlateColor(FLinearColor::Black));
	InTextBox->SetForegroundColor(FSlateColor(FLinearColor::White));
	InTextBox->SetReadOnlyForegroundColor(FSlateColor(FLinearColor::White));
}

void UVimTextEditorSubsystem::SetEditableUnifiedStyleMultiLine(
	const TSharedRef<SMultiLineEditableTextBox> InMultiTextBox)
{
	// Currently not needed
}

void UVimTextEditorSubsystem::ToggleReadOnly(bool bNegateCurrentState, bool bHandleBlinking)
{
	switch (EditableWidgetsFocusState)
	{
		case EUMEditableWidgetsFocusState::None:
			break;

		case EUMEditableWidgetsFocusState::SingleLine:
			ToggleReadOnlySingle(bNegateCurrentState, bHandleBlinking);
			break;

		case EUMEditableWidgetsFocusState::MultiLine:
			ToggleReadOnlyMulti(bNegateCurrentState, bHandleBlinking);
			break;
	}
}

void UVimTextEditorSubsystem::ToggleReadOnlySingle(bool bNegateCurrentState, bool bHandleBlinking)
{
	if (const TSharedPtr<SEditableTextBox> TextBox = ActiveEditableTextBox.Pin())
	{
		if (bNegateCurrentState)
			TextBox->SetIsReadOnly(!TextBox->IsReadOnly());
		else
			TextBox->SetIsReadOnly(CurrentVimMode != EVimMode::Insert);

		// NOTE:
		// Unlike the MultiLineEditable we don't have GetCursorLocation
		// so we can't really know where we were for the GoTo() *~*
		// So the hack I found to mitigate the blinking vs. non-blinking
		// cursor not changing upon turnning IsReadOnly ON / OFF is to just
		// BeginSearch with an empty FText (LOL).
		// This seems to keep the cursor in the same location while aligning
		// the blinking cursor with the current mode (Normal vs. Insert) :0
		// NOTE:
		// Because this clears the selection, we the bHandleBlinking param
		// to bypass this behavior in cases where we only need a pure ReadOnly
		// change but keep the selection in place.
		if (bHandleBlinking)
			TextBox->BeginSearch(FText::GetEmpty());
	}
}

void UVimTextEditorSubsystem::ToggleReadOnlyMulti(bool bNegateCurrentState, bool bHandleBlinking)
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

		// NOTE:
		// We set this dummy GoTo to refresh the cursor blinking to ON / OFF
		// NOTE:
		// Because this clears the selection, we the bHandleBlinking param
		// to bypass this behavior in cases where we only need a pure ReadOnly
		// change but keep the selection in place.
		if (bHandleBlinking)
			MultiTextBox->GoTo(MultiTextBox->GetCursorLocation());
	}
}

const FSlateRoundedBoxBrush& UVimTextEditorSubsystem::GetBorderBrush(EVimMode InVimMode)
{
	static const FLinearColor BaseColor = FLinearColor::White;
	static const float		  OutlineWidth = 1.0f;
	static const float		  CornerRoundness = 8.0f;
	static UTexture2D*		  T_BorderDefault = LoadObject<UTexture2D>(
		   nullptr,
		   TEXT("/Engine/EngineResources/Black.Black"));

	// Define color map for different modes
	static const TMap<EVimMode, FLinearColor> ModeColors = {
		{ EVimMode::Any, FLinearColor(0.1f, 0.1f, 0.1f, 1.0f) },	 // Grey
		{ EVimMode::Normal, FLinearColor(0.0f, 0.5f, 1.0f, 1.0f) },	 // Cyan
		{ EVimMode::Visual, FLinearColor(1.0f, 0.25f, 1.0f, 1.0f) }, // Purple
		{ EVimMode::Insert, FLinearColor(0.75f, 0.5f, 0.0f, 1.0f) }	 // Orange
	};

	// Define and initialize brushes only once
	static TMap<EVimMode, FSlateRoundedBoxBrush> ModeBrushes;
	if (ModeBrushes.Num() == 0)
	{
		for (const auto& Pair : ModeColors)
		{
			ModeBrushes.Add(Pair.Key, FSlateRoundedBoxBrush(BaseColor, CornerRoundness, Pair.Value, OutlineWidth));
		}
	}

	// Get the appropriate brush and set resource
	FSlateRoundedBoxBrush* Brush = ModeBrushes.Find(InVimMode);
	if (Brush)
	{
		Brush->SetResourceObject(T_BorderDefault);
		return *Brush;
	}

	// Fallback to default brush
	ModeBrushes[EVimMode::Any].SetResourceObject(T_BorderDefault);
	return ModeBrushes[EVimMode::Any];
}

bool UVimTextEditorSubsystem::TrySetHintTextForVimMode()
{
	switch (CurrentVimMode)
	{
		case EVimMode::Insert:
			return SetActiveEditableHintText(InsertModeHintText,
				true /* Only set Hint Text if editable had one initially */);

		case EVimMode::Visual:
		case EVimMode::Normal:
			return SetActiveEditableHintText(NormalModeHintText,
				true /* Only set Hint Text if editable had one initially */);

		default:
			return false;
	}
}

void UVimTextEditorSubsystem::SetDefaultBuffer()
{
	// 1) If empty; set default dummy buffer ("  ") for visualization & select it
	SetActiveEditableText(FText::FromString("  "));
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
				SetActiveEditableText(FText::FromString("  "));
				SelectAllActiveEditableText();
			}
		},
		0.025f, false);
}

void UVimTextEditorSubsystem::HandleInsertMode(const FString& InCurrentText)
{
	Logger.Print("HandleInsertMode", true);
	// In Insert mode:
	// 1. We clear the buffer if default setup ("  ")
	// 2. Or simply just clear any previous selection.
	if (IsDefaultEditableBuffer(InCurrentText)) // Clear default buffer if any
	{
		SetActiveEditableText(FText::GetEmpty()); // Clear
		RetrieveActiveEditableCursorBlinking();
	}
	else
		ClearTextSelection(false /*Insert*/); // (2) Clear previous selection

	TrySetHintTextForVimMode();
	ToggleReadOnly();
}

void UVimTextEditorSubsystem::HandleNormalModeOneChar()
{
	ToggleReadOnly();
	SelectAllActiveEditableText();
}

void UVimTextEditorSubsystem::HandleNormalModeMultipleChars()
{
	Logger.Print("HandleNormalModeMultipleChars", true);
	FSlateApplication&			   SlateApp = FSlateApplication::Get();
	TSharedRef<FVimInputProcessor> VimProc = FVimInputProcessor::Get();
	// This seems to be needed for first contact with MultiLine
	// when navigating from outer panels and when the Multi has
	// more than 1 character.
	ForceFocusActiveEditable(SlateApp);

	switch (GetSelectionState())
	{
		case (EUMSelectionState::None):
			break; // Continue with the flow at the bottom

		case (EUMSelectionState::OneChar):
			Logger.Print("One Char", true);
			ToggleReadOnly();
			return; // No need to do anything as we're already selecting 1 char

		case (EUMSelectionState::ManyChars):
		{
			Logger.Print("Many Chars", true);
			if ((!IsCurrentLineInMultiEmpty()
					|| EditableWidgetsFocusState == EUMEditableWidgetsFocusState::SingleLine)
				&& !IsCursorAlignedRight(SlateApp))
			{
				Logger.Print("Many Chars Inner", true);
				ClearTextSelection();
				ToggleReadOnly();

				VimProc->SimulateKeyPress(SlateApp, EKeys::Right, ModShiftDown);

				return;
			}
		}
	}
	Logger.Print("None || Cannot GetSelectionState", true);

	ClearTextSelection();
	ToggleReadOnly();

	// Align cursor left or right depending on it's location
	if (EditableWidgetsFocusState == EUMEditableWidgetsFocusState::MultiLine)
		SwitchInsertToNormalMultiLine(SlateApp);
	else // Align cursor to the right in all other scenarios
	{
		// NOTE:
		// We're bypassing SetCursorToDefualtLocation on first time init for
		// SingleLine Editables because there's seems to be a focus issue
		// where simulating right / left will jump to a different widget
		// throwing the entire focus out. This resolves after a few ms when the
		// focus and editable settles down, but until then, we need this bypass.
		if (!bIsEditableInit)
			SetCursorSelectionToDefaultLocation(SlateApp);
	}
}

void UVimTextEditorSubsystem::HandleEditableUX()
{
	FString TextContent;
	if (!GetActiveEditableTextContent(TextContent))
		return;

	if (CurrentVimMode == EVimMode::Insert)
		HandleInsertMode(TextContent);

	// ~ Normal & Visual Mode: ~

	else if (TextContent.IsEmpty())
	{
		ToggleReadOnly();

		// if the current editable has no default Hint Text, it's a
		// strong sign we're within a MultiLine Editable Text.
		// In this case, a better UX will be to opt for the Default Buffer
		// (i.e. "  ") and cursor like visualization (select all).
		if (!SetActiveEditableHintText(
				NormalModeHintText,
				true /*Check if had default buffer*/))
			SetDefaultBuffer(); // Default buffer alternative if return false
	}

	else if (TextContent.Len() == 1) // 1 Char; Simply select all.
		HandleNormalModeOneChar();

	else // Multi Chars
		HandleNormalModeMultipleChars();

	bIsEditableInit = false;
}

bool UVimTextEditorSubsystem::ForceFocusActiveEditable(FSlateApplication& SlateApp)
{
	switch (EditableWidgetsFocusState)
	{
		case EUMEditableWidgetsFocusState::SingleLine:
		{
			if (const auto EditTextBox = ActiveEditableTextBox.Pin())
			{
				if (EditTextBox->HasAnyUserFocusOrFocusedDescendants())
					return true;
				SlateApp.SetAllUserFocus(EditTextBox, EFocusCause::Mouse);
				return true;
			}
			break;
		}
		case EUMEditableWidgetsFocusState::MultiLine:
		{
			if (const TSharedPtr<SMultiLineEditableTextBox> MultiTextBox =
					ActiveMultiLineEditableTextBox.Pin())
			{
				TSharedPtr<SMultiLineEditableText> MultiText = GetMultilineEditableFromBox(MultiTextBox.ToSharedRef());
				if (!MultiText.IsValid())
					return false;

				if (MultiText->HasAnyUserFocusOrFocusedDescendants())
					return true;
				SlateApp.SetAllUserFocus(MultiText, EFocusCause::Mouse);
				return true;
			}
			break;
		}
		default:
			break;
	}
	return false;
}

void UVimTextEditorSubsystem::OnEditableFocusLost()
{
	ResetEditableHintText(true /*Clear Tracked Hint Text for next run*/);
	AssignEditableBorder(true /*Assign Default Border -> Focus Lost*/);
	FVimInputProcessor::Get()->Unpossess(this); // In case aborting while replace

	switch (EditableWidgetsFocusState)
	{
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
		default:
			break;
	}
}

void UVimTextEditorSubsystem::ResetEditableHintText(bool bResetTrackedHintText)
{
	switch (EditableWidgetsFocusState)
	{
		case EUMEditableWidgetsFocusState::None:
			break; // Theoretically not reachable

		case EUMEditableWidgetsFocusState::SingleLine:
			if (const auto EditTextBox = ActiveEditableTextBox.Pin())
			{
				// Reset Hint Text to Default
				if (!DefaultHintText.IsEmpty())
					EditTextBox->SetHintText(DefaultHintText);
			}
			break;

		case EUMEditableWidgetsFocusState::MultiLine:
			if (const TSharedPtr<SMultiLineEditableTextBox> MultiTextBox =
					ActiveMultiLineEditableTextBox.Pin())
			{
				// Reset Hint Text to Default
				if (!DefaultHintText.IsEmpty())
					MultiTextBox->SetHintText(DefaultHintText);
			}
			break;
	}

	if (bResetTrackedHintText)
		DefaultHintText = FText::GetEmpty();
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

void UVimTextEditorSubsystem::HandleVimTextNavigation(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	if (CurrentVimMode == EVimMode::Insert
		|| IsEditableTextWithDefaultBuffer()) // Preserve default buffer sel
		return;

	// DebugMultiLineCursorLocation(true /*Pre-Navigation*/);

	FKey ArrowKeyToSimulate;
	FUMInputHelpers::GetArrowKeyFromVimKey(InKeyEvent.GetKey(), ArrowKeyToSimulate);

	// NOTE:
	// We're processing and simulating in a very specifc order because we need
	// to have a determinstic placement for our cursor at any given time.
	// We want to keep our cursor essentially always to the right of each
	// character so we can handle beginning & end of line properly to maintain
	// correct selection. Some edge cases like end of document and such will
	// require custom alignment (left-align) for proper selection.

	if (ArrowKeyToSimulate == EKeys::Left)
		HandleLeftNavigation(SlateApp);
	else if (ArrowKeyToSimulate == EKeys::Right)
		HandleRightNavigation(SlateApp);

	else
	{
		if (EditableWidgetsFocusState == EUMEditableWidgetsFocusState::MultiLine)
			HandleUpDownMultiLine(SlateApp, ArrowKeyToSimulate);

		else // Single-line -> Just go up or down
			FVimInputProcessor::Get()->SimulateKeyPress(
				SlateApp, ArrowKeyToSimulate);
	}

	// DebugMultiLineCursorLocation(false /*Post-Navigation*/);
}

void UVimTextEditorSubsystem::HandleRightNavigation(FSlateApplication& SlateApp)
{
	if (IsCurrentLineEmpty())
		return;

	TSharedRef<FVimInputProcessor> Input = FVimInputProcessor::Get();
	const FKey					   Right = EKeys::Right;

	if (EditableWidgetsFocusState == EUMEditableWidgetsFocusState::MultiLine
		&& IsMultiLineCursorAtEndOfLine())
		return; // Shouldn't continue navigation per Vim rules

	if (CurrentVimMode == EVimMode::Visual)
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
				// Logger.Print("Once Char Right Visual", true);
				// We will need to align right to retain the anchor
				// selection and also highlight the next left right.
				// Thus, we're breaking, then aligning right, then
				// moving to the next char.
				if (!IsCursorAlignedRight(SlateApp))
				{
					Logger.Print("Aligned Left", true);
					ClearTextSelection();
					Input->SimulateKeyPress(SlateApp, Right, ModShiftDown);
				}
				break;
			}
			default:
				break;
		}
	}
	else // Normal Mode
	{
		ClearTextSelection();							// Break from curr sel
		Input->SimulateKeyPress(SlateApp, Right);		// GoTo next char
		Input->SimulateKeyPress(SlateApp, EKeys::Left); // Prep right alignment
	}
	Input->SimulateKeyPress(SlateApp, Right, ModShiftDown); // Sel
}

void UVimTextEditorSubsystem::HandleLeftNavigation(FSlateApplication& SlateApp)
{
	if (IsCurrentLineEmpty())
		return;

	TSharedRef<FVimInputProcessor> Input = FVimInputProcessor::Get();
	FKey						   Left = EKeys::Left;

	if (EditableWidgetsFocusState == EUMEditableWidgetsFocusState::MultiLine
		&& IsMultiLineCursorAtBeginningOfLine())
		return; // Shouldn't continue navigation per Vim rules

	if (CurrentVimMode == EVimMode::Visual)
	{
		switch (GetSelectionState())
		{
			case (EUMSelectionState::None):
			{
				// Logger.Print("NONE", true);
				SetCursorSelectionToDefaultLocation(SlateApp);
				break;
			}
			case (EUMSelectionState::OneChar):
			{
				// Logger.Print("ONE CHAR", true);
				// We will need to align left to retain the anchor
				// selection and also highlight the next left char.
				// Thus, we're breaking, then aligning left, then
				// moving to the next char.
				if (IsCursorAlignedRight(SlateApp))
				{
					ClearTextSelection();
					Input->SimulateKeyPress(SlateApp, Left, ModShiftDown);
				}
				break;
			}
			default:
				break;
		}
		Input->SimulateKeyPress(SlateApp, Left, ModShiftDown);
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
	return; // Is this function even needed?

	TSharedRef<FVimInputProcessor> Processor = FVimInputProcessor::Get();
	FSlateApplication&			   SlateApp = FSlateApplication::Get();

	Processor->SimulateKeyPress(SlateApp, EKeys::Left, ModShiftDown);
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
	TSharedRef<FVimInputProcessor> InputProc = FVimInputProcessor::Get();
	const bool					   bIsVisualMode = CurrentVimMode == EVimMode::Visual;

	if (InKey == EKeys::I)
	{ // [i/I]nsert
		if (bIsShiftDown)
		{
			if (bIsVisualMode || IsCurrentLineInMultiEmpty())
				ClearTextSelection(false); // Simply clear and insert
			else
				// Go to the beginning of the line.
				InputProc->SimulateKeyPress(SlateApp, EKeys::Home);
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
				InputProc->SimulateKeyPress(SlateApp, EKeys::Left);
		}
	}
	else // [a/A]ppend
	{
		if (bIsShiftDown)
		{
			if (bIsVisualMode)
				ClearTextSelection(false); // Simply clear and insert
			else
				// Go to the end of the line.
				InputProc->SimulateKeyPress(SlateApp, EKeys::End);
		}
		else
		{
			if (bIsVisualMode)
				return; // Do nothing. AFAIK that's Vim's behavior.

			// Else, just enter Insert Mode:
			// Default behavior is in the correct cursor location already.
		}
	}
	InputProc->SetVimMode(SlateApp, EVimMode::Insert);
}

// TODO: Update practice to conform with new methods for Multiline
void UVimTextEditorSubsystem::GoToStartOrEnd(
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
			HandleGoToStartOrEndSingleLine(SlateApp, true /*GoTo End*/);
	}
	else // gg -> GoTo Start
	{
		if (bIsMultiline)
			HandleGoToStartMultiLine(SlateApp);

		else // Single-Line
			HandleGoToStartOrEndSingleLine(SlateApp, false /*GoTo Start*/);
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
		if (IsCurrentLineInMultiEmpty(MultiTextBox.ToSharedRef()))
			SetCursorSelectionToDefaultLocation(SlateApp, false /*Left Align*/);
		else
			SetCursorSelectionToDefaultLocation(SlateApp, true /*Right Align*/);
	}
}

void UVimTextEditorSubsystem::HandleVisualModeGoToStartOrEndMultiLine(FSlateApplication& SlateApp, bool bGoToStart)
{
	const TSharedPtr<SMultiLineEditableTextBox> MultiTextBox =
		ActiveMultiLineEditableTextBox.Pin();
	if (!MultiTextBox.IsValid())
		return;

	FTextLocation CurrentTextLocation = MultiTextBox->GetCursorLocation();
	if (!CurrentTextLocation.IsValid()
		|| !StartCursorLocationVisualMode.IsValid())
		return;

	FTextLocation GoToLocation;
	if (bGoToStart)
		GoToLocation = FTextLocation(0, 0);
	else
	{
		TSharedPtr<SMultiLineEditableText> MultiLineText = GetMultilineEditableFromBox(MultiTextBox.ToSharedRef());
		if (!MultiLineText.IsValid())
			return;

		const int32 LineCount = MultiLineText->GetTextLineCount();
		FString		TextAtLastLine;
		MultiLineText->GetTextLine(LineCount > 0 ? LineCount - 1 : LineCount, TextAtLastLine);
		const int32 LastCharOffset = TextAtLastLine.Len();
		GoToLocation = FTextLocation(LineCount, LastCharOffset);
	}
	// DebugSelectStartToEnd(StartCursorLocationVisualMode, GoToLocation);
	SelectTextInRange(SlateApp, StartCursorLocationVisualMode, GoToLocation);
}

void UVimTextEditorSubsystem::HandleGoToStartOrEndSingleLine(FSlateApplication& SlateApp, bool bGoToEnd)
{
	if (CurrentVimMode == EVimMode::Visual)
	{
		if (!DoesActiveEditableHasAnyTextSelected())
			HandleEditableUX();

		// Lambda to choose the correct navigation based on bGoToEnd.
		auto Navigate = [&]() {
			if (bGoToEnd)
				HandleRightNavigation(SlateApp);
			else
				HandleLeftNavigation(SlateApp);
		};

		// Loop until the selection length remains unchanged.
		FString PrevSelectedText;
		FString NewSelectedText;
		do
		{
			if (!GetSelectedText(PrevSelectedText))
				return;

			Navigate();

			if (!GetSelectedText(NewSelectedText))
				return;
		}
		while (NewSelectedText.Len() != PrevSelectedText.Len());
	}
	else // Normal Mode
	{
		TSharedRef<FVimInputProcessor> InputProc = FVimInputProcessor::Get();
		InputProc->SimulateKeyPress(SlateApp, bGoToEnd ? EKeys::End : EKeys::Home);

		if (bGoToEnd)
			SetCursorSelectionToDefaultLocation(SlateApp);
		else
			InputProc->SimulateKeyPress(SlateApp, EKeys::Right, ModShiftDown);
	}
}

void UVimTextEditorSubsystem::HandleGoToStartOrEndOfLine(FSlateApplication& SlateApp, bool bGoToEnd)
{
	TSharedRef<FVimInputProcessor> InputProc = FVimInputProcessor::Get();
	const auto					   Navigate = bGoToEnd
							? &UVimTextEditorSubsystem::HandleRightNavigation
							: &UVimTextEditorSubsystem::HandleLeftNavigation;
	int32						   CurrOffset = INDEX_NONE;
	int32						   PrevOffset = INDEX_NONE;
	FTextLocation				   CurrentTextLocation;
	do
	{
		PrevOffset = CurrOffset;
		if (!GetCursorLocation(SlateApp, CurrentTextLocation))
			return;
		CurrOffset = CurrentTextLocation.GetOffset();
		(this->*Navigate)(SlateApp);
	}
	while (CurrOffset != PrevOffset);
}

void UVimTextEditorSubsystem::JumpUpOrDown(FSlateApplication& SlateApp, const TArray<FInputChord>& InSequence)
{
	if (const TSharedPtr<SMultiLineEditableTextBox> MultiTextBox =
			ActiveMultiLineEditableTextBox.Pin())
	{
		const FKey NavUpOrDownKey = InSequence.Last().Key == EKeys::U
			? EKeys::Up
			: EKeys::Down;

		const int32 NumOfLinesToJump = 6;

		for (int32 i{ 0 }; i < NumOfLinesToJump; ++i)
			HandleUpDownMultiLine(SlateApp, NavUpOrDownKey);
	}
}

void UVimTextEditorSubsystem::SetCursorSelectionToDefaultLocation(FSlateApplication& SlateApp, bool bAlignCursorRight)
{
	TSharedRef<FVimInputProcessor> InputProcessor = FVimInputProcessor::Get();
	if (bAlignCursorRight)
	{ // Right Align: for most cases
		InputProcessor->SimulateKeyPress(SlateApp, EKeys::Left);
		InputProcessor->SimulateKeyPress(SlateApp, EKeys::Right, ModShiftDown);
	}
	else
	// Left Align: for some cases (end of document and proper maintance of
	// anchor char selection)
	{
		InputProcessor->SimulateKeyPress(SlateApp, EKeys::Right);
		InputProcessor->SimulateKeyPress(SlateApp, EKeys::Left, ModShiftDown);
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

bool UVimTextEditorSubsystem::GetActiveEditableTextContent(FString& OutText, const bool bIfMultiCurrLine)
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
				if (bIfMultiCurrLine)
					MultiTextBox->GetCurrentTextLine(OutText);
				else
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
		case EUMEditableWidgetsFocusState::SingleLine:
			if (const TSharedPtr<SEditableTextBox> EditTextBox = ActiveEditableTextBox.Pin())
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
		default:
			break;
	}
	return false;
}

bool UVimTextEditorSubsystem::GetSelectionRange(FSlateApplication& SlateApp, FTextSelection& OutSelectionRange)
{
	switch (EditableWidgetsFocusState)
	{
		case EUMEditableWidgetsFocusState::SingleLine:
			return GetSelectionRangeSingleLine(SlateApp, OutSelectionRange);

		case EUMEditableWidgetsFocusState::MultiLine:
			return GetSelectionRangeMultiLine(SlateApp, OutSelectionRange);

		default:
			break;
	}
	return false;
}

bool UVimTextEditorSubsystem::GetSelectionRangeSingleLine(FSlateApplication& SlateApp, FTextSelection& OutSelectionRange)
{
	if (const TSharedPtr<SEditableTextBox> EditTextBox = ActiveEditableTextBox.Pin())
	{
		if (!EditTextBox->AnyTextSelected())
			return false;

		const bool bIsCursorAlignedRight = IsCursorAlignedRight(SlateApp);

		const int32 SelectedTextLen = EditTextBox->GetSelectedText().ToString().Len();

		FTextLocation EndTextLocation; // i.e. Current Text Location
		GetCursorLocation(SlateApp, EndTextLocation);

		const FTextLocation BeginningTextLocation =
			bIsCursorAlignedRight
			? FTextLocation(0, EndTextLocation.GetOffset() - SelectedTextLen)
			: FTextLocation(0, EndTextLocation.GetOffset() + SelectedTextLen);

		OutSelectionRange = FTextSelection(BeginningTextLocation, EndTextLocation);
		return true;
	}
	return false;
}

bool UVimTextEditorSubsystem::GetSelectionRangeMultiLine(FSlateApplication& SlateApp, FTextSelection& OutSelectionRange)
{
	if (const TSharedPtr<SMultiLineEditableTextBox> MultiTextBox =
			ActiveMultiLineEditableTextBox.Pin())
	{
		if (!MultiTextBox->AnyTextSelected())
			return false;

		const TSharedPtr<SMultiLineEditableText> MultiText = GetMultilineEditableFromBox(MultiTextBox.ToSharedRef());
		if (MultiText.IsValid())
		{
			OutSelectionRange = MultiText->GetSelection();
			return true;
		}
	}
	return false;
}

void UVimTextEditorSubsystem::DebugSelectionRange(const FTextSelection& InSelectionRange)
{
	// NOTE:
	// We're directly fetching using LocationA & LocationB because internally
	// Unreal will retrieve the End & Beginning location by calculating who's
	// bigger. This is really not what we want because we care about true begin
	// and end position in Vim.
	const FTextLocation Beg = InSelectionRange.LocationA;
	const FTextLocation End = InSelectionRange.LocationB;

	Logger.Print(FString::Printf(TEXT("Selection Range:\nStart Location: Line(%d), Offset(%d)\nEnd Location: Line(%d), Offset(%d)"), Beg.GetLineIndex(), Beg.GetOffset(), End.GetLineIndex(), End.GetOffset()), true);
}

bool UVimTextEditorSubsystem::SetActiveEditableText(const FText& InText)
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

bool UVimTextEditorSubsystem::SetActiveEditableHintText(const FText& InText, bool bConsiderDefaultHintText)
{
	if (bConsiderDefaultHintText && DefaultHintText.IsEmpty())
		return false;

	switch (EditableWidgetsFocusState)
	{
		case EUMEditableWidgetsFocusState::None:
			break; // Theoretically not reachable

		case EUMEditableWidgetsFocusState::SingleLine:
			if (const auto EditTextBox = ActiveEditableTextBox.Pin())
			{
				EditTextBox->SetHintText(InText);
				return true;
			}

		case EUMEditableWidgetsFocusState::MultiLine:
			if (const TSharedPtr<SMultiLineEditableTextBox> MultiTextBox =
					ActiveMultiLineEditableTextBox.Pin())
			{
				MultiTextBox->SetHintText(InText);
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

void UVimTextEditorSubsystem::SelectAllActiveEditableText()
{
	switch (EditableWidgetsFocusState)
	{
		case EUMEditableWidgetsFocusState::None:
			break; // Theoretically not reachable

		case EUMEditableWidgetsFocusState::SingleLine:
			if (const auto EditTextBox = ActiveEditableTextBox.Pin())
				EditTextBox->SelectAllText();

		case EUMEditableWidgetsFocusState::MultiLine:
			if (const TSharedPtr<SMultiLineEditableTextBox> MultiTextBox =
					ActiveMultiLineEditableTextBox.Pin())
				MultiTextBox->SelectAllText();
	}
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
		return IsCurrentLineInMultiEmpty(MultiTextBox.ToSharedRef());

	return false;
}

bool UVimTextEditorSubsystem::IsCurrentLineInMultiEmpty(const TSharedRef<SMultiLineEditableTextBox> InMultiTextBox)
{
	FString CurrTextLine;
	InMultiTextBox->GetCurrentTextLine(CurrTextLine);
	// Also we might want to check if default buffer.
	return CurrTextLine.IsEmpty();
}

// TODO: Refactor this function (i.e. break it down to smaller functions)
bool UVimTextEditorSubsystem::HandleUpDownMultiLine(FSlateApplication& SlateApp, const FKey& InKeyDir)
{
	if (CurrentVimMode == EVimMode::Visual)
		return HandleUpDownMultiLineVisualMode(SlateApp, InKeyDir);
	else // Normal Mode
		return HandleUpDownMultiLineNormalMode(SlateApp, InKeyDir);
}

bool UVimTextEditorSubsystem::HandleUpDownMultiLineNormalMode(FSlateApplication& SlateApp, const FKey& InKeyDir)
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

	// Check if we've entered an empty line to determine proper navigation
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

bool UVimTextEditorSubsystem::HandleUpDownMultiLineVisualMode(FSlateApplication& SlateApp, const FKey& InKeyDir)
{
	TSharedRef<FVimInputProcessor> InputProc = FVimInputProcessor::Get();

	const TSharedPtr<SMultiLineEditableTextBox> MultiTextBox =
		ActiveMultiLineEditableTextBox.Pin();
	if (!MultiTextBox.IsValid())
		return false;

	FTextLocation CurrentTextLocation = MultiTextBox->GetCursorLocation();
	if (!CurrentTextLocation.IsValid()
		|| !StartCursorLocationVisualMode.IsValid())
		return false;

	const FTextLocation GoToTextLocation(
		CurrentTextLocation.GetLineIndex() + (InKeyDir == EKeys::Up ? -1 : 1),
		CurrentTextLocation.GetOffset());

	TSharedPtr<SMultiLineEditableText> MultiLine = GetMultilineEditableFromBox(MultiTextBox.ToSharedRef());

	return SelectTextInRange(SlateApp,
		StartCursorLocationVisualMode, GoToTextLocation);
}

void UVimTextEditorSubsystem::AlignCursorToIndex(FSlateApplication& SlateApp, int32 CurrIndex, int32 AlignToIndex, bool bAlignRight)
{
	if (CurrIndex == AlignToIndex)
	{
		SetCursorSelectionToDefaultLocation(SlateApp, bAlignRight);
		return;
	}

	bool bIsCursorToTheRight = CurrIndex > AlignToIndex;
	bool bIsCursorAlignedRight = IsCursorAlignedRight(SlateApp);
	if (bIsCursorAlignedRight)
	{
		if (bIsCursorAlignedRight) // Aligned right and already to the right.
			return;
	}
	else if (!bIsCursorAlignedRight) // Aligned left and already to the left.
		return;

	TSharedRef<FVimInputProcessor> VimProc = FVimInputProcessor::Get();

	const FKey	NavDir = bIsCursorToTheRight ? EKeys::Left : EKeys::Right;
	const int32 Delta = bIsCursorToTheRight // Bigger num - smaller num index
		? CurrIndex - AlignToIndex
		: AlignToIndex - CurrIndex;
	int32		i = 0;
	while (i < Delta)
	{
		VimProc->SimulateKeyPress(SlateApp, NavDir);
		++i;
	}
	ClearTextSelection();
	SetCursorSelectionToDefaultLocation(SlateApp, bAlignRight);
}

bool UVimTextEditorSubsystem::IsMultiLineCursorAtEndOfDocument(const bool bConsiderLastLineAsEnd)
{
	if (const TSharedPtr<SMultiLineEditableTextBox> MultiTextBox =
			ActiveMultiLineEditableTextBox.Pin())
	{
		return IsMultiLineCursorAtEndOfDocument(FSlateApplication::Get(), MultiTextBox.ToSharedRef(), bConsiderLastLineAsEnd);
	}
	return false;
}

bool UVimTextEditorSubsystem::IsMultiLineCursorAtEndOfDocument(
	FSlateApplication&							SlateApp,
	const TSharedRef<SMultiLineEditableTextBox> InMultiLine,
	const bool									bConsiderLastLineAsEnd)
{
	// Get current text location
	FTextLocation StartTextLocation = InMultiLine->GetCursorLocation();
	if (!StartTextLocation.IsValid())
	{
		// Logger.Print("Cannot determine if at End-of-Document - Invalid Cursor Location", true);
		return false;
	}

	TSharedRef<FVimInputProcessor> InputProc = FVimInputProcessor::Get();

	// First, we want to simulate down once to see if there are other lines
	// in the document. If there are, we're clearly not at the end of the doc.
	InputProc->SimulateKeyPress(SlateApp, EKeys::Down, ModShiftDown);
	FTextLocation NewTextLocation = InMultiLine->GetCursorLocation();
	if (NewTextLocation.GetLineIndex() > StartTextLocation.GetLineIndex())
	{
		// Logger.Print(FString::Printf(TEXT("Not at end of document -> more lines to go\nStart Index: %d\nNext Index: %d"),
		// 				 StartTextLocation.GetLineIndex(),
		// 				 NewTextLocation.GetLineIndex()),
		// 	true);
		// We're not at the end, we can revert to the origin position and return.
		InputProc->SimulateKeyPress(SlateApp, EKeys::Up, ModShiftDown);
		return false;
	}
	else if (bConsiderLastLineAsEnd)
		return true;

	// At this point we know we're at the last line of the document.
	// Before checking if we're also at the last char in that line, we should
	// check if this line is empty, because if so the cursor alignment is set
	// in a specific way for highlighting.
	if (IsCurrentLineInMultiEmpty())
	{
		// Logger.Print("At End-of-Document - Last line is empty.", true);
		return true;
	}

	// The last line isn't empty, so we should just check if we're at the last
	// char at this line (i.e. the end of the doc)
	FString LineText;
	InMultiLine->GetCurrentTextLine(LineText);
	// Logger.Print(FString::Printf(TEXT("Line Text Length: %d\nStart Text Location Offset: %d"), LineText.Len(), StartTextLocation.GetOffset()), true);
	// It looks like we will need to compensate with +1 for some reason. Keep
	// an eye on this.
	return LineText.Len() == StartTextLocation.GetOffset() + 1;
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

bool UVimTextEditorSubsystem::SelectTextInRange(
	FSlateApplication&	 SlateApp,
	const FTextLocation& StartLocation,
	const FTextLocation& EndLocation,
	bool				 bJumpToStart,
	bool				 bIgnoreSetCursorToDefault)
{
	TSharedRef<FVimInputProcessor> InputProc = FVimInputProcessor::Get();

	const int32 StartLineIndex = StartLocation.GetLineIndex();
	const int32 StartOffset = StartLocation.GetOffset();
	const int32 EndLineIndex = EndLocation.GetLineIndex();
	const int32 EndOffset = EndLocation.GetOffset();

	if (StartLocation == EndLocation)
	{
		GoToTextLocation(SlateApp, StartLocation);
		HandleEditableUX();
		return true;
	}

	const bool bShouldAlignRight = StartLineIndex < EndLineIndex
		|| (StartLineIndex == EndLineIndex
			&& StartOffset < EndOffset);

	if (bJumpToStart) // vs. moving from where we're currently at
		GoToTextLocation(SlateApp, StartLocation);
	const bool bIsAtBeginningOfLine = IsMultiLineCursorAtBeginningOfLine();

	if (bShouldAlignRight)
		if (bIsAtBeginningOfLine || bIgnoreSetCursorToDefault)
			InputProc->SimulateKeyPress(SlateApp, EKeys::Right, ModShiftDown);
		else
			SetCursorSelectionToDefaultLocation(SlateApp);
	else // Align Left
	{
		if (bIsAtBeginningOfLine)
			SetCursorSelectionToDefaultLocation(SlateApp, false /*LeftAlign*/);
		else
			InputProc->SimulateKeyPress(SlateApp, EKeys::Left, ModShiftDown);
	}

	FTextLocation CurrentTextLocation; // We gonna mutate this and pass it around
	if (!GetCursorLocation(SlateApp, CurrentTextLocation))
		return false;

	// Vertical Selection: Up or Down (Moving between lines)
	if (!SelectTextToLineIndex(SlateApp, CurrentTextLocation, EndLineIndex))
		return false;

	// Horizontal Selection: Left or Right (Moving between characters)
	return SelectTextToOffset(SlateApp, CurrentTextLocation, EndOffset);
}

bool UVimTextEditorSubsystem::SelectTextToLineIndex(FSlateApplication& SlateApp, FTextLocation& CurrentTextLocation, const int32 EndLineIndex)
{
	// Only in MultiLines we want to perform the Up & Down selection
	if (EditableWidgetsFocusState != EUMEditableWidgetsFocusState::MultiLine)
		return true; // Should return true as this isn't an error for SingleLine

	const TSharedRef<FVimInputProcessor> InputProc = FVimInputProcessor::Get();

	int32	   CurrLineIndex = CurrentTextLocation.GetLineIndex();
	const bool bShouldGoUp = CurrLineIndex > EndLineIndex;
	const FKey NavDirVertical = bShouldGoUp ? EKeys::Up : EKeys::Down;
	int32	   PrevLineIndex = INDEX_NONE;

	while (CurrLineIndex != EndLineIndex && CurrLineIndex != PrevLineIndex)
	{
		PrevLineIndex = CurrLineIndex;

		InputProc->SimulateKeyPress(SlateApp, NavDirVertical, ModShiftDown);

		if (!GetCursorLocation(SlateApp, CurrentTextLocation))
			return false;
		CurrLineIndex = CurrentTextLocation.GetLineIndex();
	}
	return true;
}

bool UVimTextEditorSubsystem::SelectTextToOffset(FSlateApplication& SlateApp, FTextLocation& CurrentTextLocation, const int32 EndOffset)
{
	int32	   CurrOffset = CurrentTextLocation.GetOffset();
	const bool bShouldGoRight = CurrOffset < EndOffset;
	const auto Navigate = bShouldGoRight
		? &UVimTextEditorSubsystem::HandleRightNavigation
		: &UVimTextEditorSubsystem::HandleLeftNavigation;
	int32	   PrevOffset = INDEX_NONE;

	while (CurrOffset != EndOffset && CurrOffset != PrevOffset)
	{
		PrevOffset = CurrOffset;

		(this->*Navigate)(SlateApp);

		if (!GetCursorLocation(SlateApp, CurrentTextLocation))
			return false;
		CurrOffset = CurrentTextLocation.GetOffset();
		// Logger.Print(FString::Printf(TEXT("Current Offset: %d\nPrevious Offset: %d\nTarget End Offset: %d"), CurrOffset, PrevOffset, EndOffset), true);
	}
	return true;
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
			InputProc->SimulateKeyPress(SlateApp, EKeys::Left, ModShiftDown);
			FString NewSelText = EditTextBox->GetSelectedText().ToString();
			bool	bIsAtBeginningOfLine = OriginSelText.Len() == NewSelText.Len();
			if (!bIsAtBeginningOfLine)
				InputProc->SimulateKeyPress(SlateApp, EKeys::Right, ModShiftDown);
			return bIsAtBeginningOfLine;
		}
		else
		{
			InputProc->SimulateKeyPress(SlateApp, EKeys::Left, ModShiftDown);
			FString NewSelText = EditTextBox->GetSelectedText().ToString();
			bool	bIsAtBeginningOfLine = !EditTextBox->AnyTextSelected();
			if (!bIsAtBeginningOfLine)
				InputProc->SimulateKeyPress(SlateApp, EKeys::Right, ModShiftDown);
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

bool UVimTextEditorSubsystem::IsMultiLineCursorAtBeginningOfLine(bool bForceZeroValidation)
{
	const TSharedPtr<SMultiLineEditableTextBox> MultiTextBox =
		ActiveMultiLineEditableTextBox.Pin();
	if (!MultiTextBox.IsValid())
		return false;

	// Get current text location
	FTextLocation StartTextLocation = MultiTextBox->GetCursorLocation();
	if (!StartTextLocation.IsValid())
		return false;
	const int32		   CursorOffset = StartTextLocation.GetOffset();
	FSlateApplication& SlateApp = FSlateApplication::Get();
	// NOTE:
	// In Visual Mode we should be allowed to go to the 0th offset location
	// thus the boundary changes to 0 instead of 1.
	// We also need to take in consideration right vs. left cursor alignment
	// as if we're aligned right we should keep our target limit to 1 to properly
	// keep the first char in the line selected.
	const int32 TargetLimit =
		bForceZeroValidation
			|| (CurrentVimMode == EVimMode::Visual
				&& !IsCursorAlignedRight(SlateApp))
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
	FString	   OriginSelText = "";
	const bool bWasAnyTextSelected = GetSelectedText(OriginSelText);
	if (!bWasAnyTextSelected)
	{
		// Logger.Print("Cursor Aligned Right: No Text Selected", true);
		return true;
	}

	const TSharedRef<FVimInputProcessor> VimProc = FVimInputProcessor::Get();
	VimProc->SimulateKeyPress(SlateApp, EKeys::Right, ModShiftDown);

	FString	   NewSelText;
	const bool bPostAnyTextSelected = GetSelectedText(NewSelText);
	if (!bPostAnyTextSelected)
	{
		// Logger.Print("Cursor Aligned Left: Initially some text was selected, then none when simulated to the Right", true);

		// If we initially had text selected, but after moving right we have
		// no selection, this means our cursor was aligned to the left side of
		// the selection.
		// Example: Given text "abc" with "b" selected:
		// Left-aligned:  a[b]c -> moving right breaks selection -> ab|c
		// Right-aligned: ab[c] -> moving right keeps selection -> ab[c]
		// Revert: return to origin selection

		VimProc->SimulateKeyPress(SlateApp, EKeys::Left, ModShiftDown);
		return false; // Aligned Left
	}

	// In all other scenarios we know some text was selected, and we still have
	// text selected after simulating to the right, so we can compare if we have
	// more or less text selected to determine if we're aligned to the left
	// or to the right.

	const int32 NewSelTextLen = NewSelText.Len();
	const int32 OriginSelTextLen = OriginSelText.Len();
	if (NewSelTextLen != OriginSelTextLen)
	{
		// if any mutations were introduced - revert: return to origin selection
		VimProc->SimulateKeyPress(SlateApp, EKeys::Left, ModShiftDown);
	}
	const bool bIsCursorAlignedRight = NewSelTextLen >= OriginSelTextLen;
	// const FString CursorAlignmentDebug = bIsCursorAlignedRight
	// 	? "Cursor Aligned Right: New Selection is Bigger than Prev Selection."
	// 	: "Cursor Aligned Left: New Selection is Smaller than Prev Selection.";
	// Logger.Print(CursorAlignmentDebug, true);
	return bIsCursorAlignedRight;
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

	if (EditTextBox->AnyTextSelected())
	{
		FString OriginSelText = EditTextBox->GetSelectedText().ToString();

		InputProc->SimulateKeyPress(SlateApp, EKeys::Right, ModShiftDown);
		FString NewSelText = EditTextBox->GetSelectedText().ToString();
		int32	NewSelLen = NewSelText.Len();
		if (NewSelLen > OriginSelText.Len()
			// if == 0 it also means we've managed to move, because we had text
			// selected, and if we're at the end and simulating right, we should
			// continue to have some text selected (i.e. retain our selection)
			|| NewSelLen == 0)
		{
			// We've managed to move, thus not at end.
			// Revert to origin position.
			InputProc->SimulateKeyPress(SlateApp, EKeys::Left, ModShiftDown);
			return false;
		}
		// We haven't moved, thus we're at the end of the line.
		return true;
	}
	else // Initially, no text selected
	{
		InputProc->SimulateKeyPress(SlateApp, EKeys::Right, ModShiftDown);
		FString NewSelText = EditTextBox->GetSelectedText().ToString();
		if (EditTextBox->AnyTextSelected())
		{
			// We've managed to move, thus not at end.
			// Revert to origin position.
			InputProc->SimulateKeyPress(SlateApp, EKeys::Left, ModShiftDown);
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
	if (IsMultiLineCursorAtBeginningOfLine())
	{
		if (IsCurrentLineInMultiEmpty())
			SetCursorSelectionToDefaultLocation(SlateApp,
				IsMultiLineCursorAtEndOfDocument() /*Left-or-Right*/);

		else // Non-Empty line, we can shift down to the right to prop align.
			FVimInputProcessor::Get()->SimulateKeyPress(
				SlateApp, EKeys::Right, ModShiftDown);
	}
	else
		SetCursorSelectionToDefaultLocation(SlateApp);
}

void UVimTextEditorSubsystem::Delete(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	if (IsCurrentLineEmpty())
		return;

	DeleteCurrentSelection(SlateApp, true /* Yank Currently Selected Text*/);

	TSharedRef<FVimInputProcessor> VimProc = FVimInputProcessor::Get();
	if (!IsCurrentLineEmpty()
		&& !IsCursorAtEndOfLine(SlateApp))
		VimProc->SimulateKeyPress(SlateApp, EKeys::Right);

	ClearTextSelection(true /*Normal*/);

	if (CurrentVimMode == EVimMode::Visual)
		VimProc->SetVimMode(SlateApp, EVimMode::Normal);
	else
		HandleEditableUX();
}

void UVimTextEditorSubsystem::ShiftDeleteNormalMode(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	if (IsCurrentLineEmpty() || IsCursorAtBeginningOfLine())
		return;

	ClearTextSelection(false /*Insert*/);

	TSharedRef<FVimInputProcessor> VimProc = FVimInputProcessor::Get();
	VimProc->SimulateKeyPress(SlateApp, EKeys::Left);
	VimProc->SimulateKeyPress(SlateApp, EKeys::Left, ModShiftDown);
	VimProc->SimulateKeyPress(SlateApp, EKeys::Delete);

	if (!IsCurrentLineEmpty()
		&& !IsCursorAtEndOfLine(SlateApp))
		VimProc->SimulateKeyPress(SlateApp, EKeys::Right);

	ClearTextSelection(true /*Normal*/);

	HandleEditableUX();
}

void UVimTextEditorSubsystem::DeleteUpOrDown(FSlateApplication& SlateApp, const TArray<FInputChord>& InSequence)
{
	const TSharedPtr<SMultiLineEditableTextBox> MultiTextBox =
		ActiveMultiLineEditableTextBox.Pin();
	if (!MultiTextBox.IsValid())
		return;

	// Get the MultiLine itself to quick access line count
	const TSharedPtr<SMultiLineEditableText> MultiLine = GetMultilineEditableFromBox(MultiTextBox.ToSharedRef());
	if (!MultiLine.IsValid())
		return;

	FTextLocation TextLocation = MultiLine->GetCursorLocation();
	if (!TextLocation.IsValid())
		return;
	const int32 LineIndex = TextLocation.GetLineIndex();
	const int32 LineCount = MultiLine->GetTextLineCount();

	// Logger.Print(FString::Printf(TEXT("Line Index: %d\nLine Count: %d"), LineIndex, LineCount), true);

	const bool bIsUp = InSequence.Last().Key == EKeys::K;
	if (bIsUp && LineIndex == 0)
		return; // Invalid: At the first line
	else if (!bIsUp && LineIndex + 1 == LineCount)
		return; // Invalid: At the last line.

	FString TextGetter;
	FString TextToYank;
	if (bIsUp)
	{
		HandleUpDownMultiLineNormalMode(SlateApp, EKeys::Up);
		GetCursorLocation(SlateApp, TextLocation);
	}

	// Prepare lines for yanking
	MultiLine->GetTextLine(TextLocation.GetLineIndex(), TextGetter);
	TextToYank.Append(TextGetter + '\n');
	MultiLine->GetTextLine(TextLocation.GetLineIndex() + 1, TextGetter);
	TextToYank.Append(TextGetter);

	DeleteLine(SlateApp, FKeyEvent());
	DeleteLine(SlateApp, FKeyEvent());
	YankData.SetData(TextToYank, EUMYankType::Linewise); // Manually yank lines
}

void UVimTextEditorSubsystem::ActionToMotion(FSlateApplication& SlateApp, const TArray<FInputChord>& InSequence)
{
	if (InSequence.Num() < 2)
		return; // We expect at least 2 elements (e.g. dw, de, etc.)

	const FKey Action = InSequence[0].Key; // i.e. Delete / Change / Yank

	// Create a sub-array skipping the first element (d)
	// (Should be done before switching to Visual Mode as switching Vim Modes
	// resets the InSequence ref)
	TArray<FInputChord> SubSequence;
	for (int32 i = 1; i < InSequence.Num(); i++)
		SubSequence.Add(InSequence[i]);

	FTextLocation OriginTextLocation;
	if (!GetCursorLocation(SlateApp, OriginTextLocation))
		return;
	FTextLocation						 NewTextLocation;
	const TSharedRef<FVimInputProcessor> InputProc = FVimInputProcessor::Get();
	InputProc->SetVimMode(SlateApp, EVimMode::Visual);

	// Pass the newly created array containing the pure word motions (dw vs. w)
	NavigateWordMotion(SlateApp, SubSequence);

	if (!GetCursorLocation(SlateApp, NewTextLocation))
		return;

	if (NewTextLocation != OriginTextLocation)
	{
		if (NewTextLocation.GetLineIndex() != OriginTextLocation.GetLineIndex())
		{
			if (!GoToTextLocation(SlateApp, OriginTextLocation))
				return;
			FString TextLine;
			GetActiveEditableTextContent(TextLine, true /*CurrLine*/);
			SelectTextToOffset(SlateApp, OriginTextLocation, TextLine.Len());
		}
		else if (SubSequence[0].Key == EKeys::W)
			HandleLeftNavigation(SlateApp);
		else if (SubSequence[0].Key == EKeys::G)
			HandleRightNavigation(SlateApp);
	}

	if (Action == EKeys::D)
		Delete(SlateApp, FKeyEvent());

	else if (Action == EKeys::C)
		ChangeVisualMode(SlateApp, FKeyEvent());

	else if (Action == EKeys::Y)
		YankCharacter(SlateApp, TArray<FInputChord>());
}

void UVimTextEditorSubsystem::AppendNewLine(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	// Only in MultiLine we want to simulate \n (break line)
	const bool bIsMultiLine = EditableWidgetsFocusState == EUMEditableWidgetsFocusState::MultiLine;

	TSharedRef<FVimInputProcessor> VimProc = FVimInputProcessor::Get();
	VimProc->SetVimMode(SlateApp, EVimMode::Insert);

	if (InKeyEvent.IsShiftDown())
	{
		VimProc->SimulateKeyPress(SlateApp, EKeys::Home);

		if (bIsMultiLine)
		{
			VimProc->SimulateKeyPress(SlateApp, EKeys::Enter, ModShiftDown);
			VimProc->SimulateKeyPress(SlateApp, EKeys::Up);
		}
	}
	else
	{
		VimProc->SimulateKeyPress(SlateApp, EKeys::End);
		// NOTE:
		// Because we're not actually pressing shift, for some reason, this isn't
		// working. We aren't able to *simulate* shift down with enter. Not sure
		// why. So created this helper function to insert \n after we go to the
		// end-of-the-line, which essentially achieves the same goal of going
		// down a line.
		// VimProc->SimulateKeyPress(SlateApp, EKeys::Enter, ModShiftDown);
		if (bIsMultiLine)
			AppendBreakMultiLine();
	}
}

bool UVimTextEditorSubsystem::AppendBreakMultiLine()
{
	const TSharedPtr<SMultiLineEditableTextBox> MultiTextBox =
		ActiveMultiLineEditableTextBox.Pin();
	if (!MultiTextBox.IsValid())
		return false;

	MultiTextBox->InsertTextAtCursor(FText::FromString("\n"));
	return true;
}

bool UVimTextEditorSubsystem::InsertTextAtCursor(FSlateApplication& SlateApp, const FText& InText)
{
	switch (EditableWidgetsFocusState)
	{
		case EUMEditableWidgetsFocusState::SingleLine:
			return InsertTextAtCursorSingleLine(SlateApp, InText);

		case EUMEditableWidgetsFocusState::MultiLine:
			return InsertTextAtCursorMultiLine(SlateApp, InText);

		default:
			break;
	}
	return false;
}

bool UVimTextEditorSubsystem::InsertTextAtCursorSingleLine(FSlateApplication& SlateApp, const FText& InText)
{
	if (const auto EditTextBox = ActiveEditableTextBox.Pin())
	{
		TSharedRef<SEditableTextBox> TextBoxRef = EditTextBox.ToSharedRef();
		const int32					 CursorOffset = GetCursorLocationSingleLine(SlateApp, TextBoxRef);
		Logger.Print(FString::Printf(TEXT("Cursor Offset: %d"), CursorOffset), true);

		FString CurrText = EditTextBox->GetText().ToString();
		if (CursorOffset > CurrText.Len())
			return false;

		const FString InTextStr = InText.ToString();
		CurrText.InsertAt(CursorOffset, InTextStr);
		EditTextBox->SetText(FText::FromString(CurrText));
		return true;
	}
	return false;
}

bool UVimTextEditorSubsystem::InsertTextAtCursorMultiLine(FSlateApplication& SlateApp, const FText& InText)
{
	if (const TSharedPtr<SMultiLineEditableTextBox> MultiTextBox =
			ActiveMultiLineEditableTextBox.Pin())
	{
		const TSharedPtr<SMultiLineEditableText> MultiText = GetMultilineEditableFromBox(MultiTextBox.ToSharedRef());
		if (!MultiText.IsValid())
			return false;

		const bool bIsInitReadOnly = MultiText->IsTextReadOnly();
		if (bIsInitReadOnly)
			MultiTextBox->SetIsReadOnly(false);

		MultiTextBox->InsertTextAtCursor(InText);
		// Revert ReadOnly state to origin or stay off if originall none.
		MultiTextBox->SetIsReadOnly(bIsInitReadOnly);

		return true;
	}
	return false;
}

void UVimTextEditorSubsystem::DeleteLine(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	FString CurrentLine;
	if (GetActiveEditableTextContent(CurrentLine, true /*Only Current Line*/))
		YankData.SetData(CurrentLine, EUMYankType::Linewise);
	else
		return;

	switch (EditableWidgetsFocusState)
	{
		case EUMEditableWidgetsFocusState::None:
			break; // Theoretically not reachable

		case EUMEditableWidgetsFocusState::SingleLine:
			DeleteLineNormalModeSingle(SlateApp, InKeyEvent);

		case EUMEditableWidgetsFocusState::MultiLine:
			DeleteLineNormalModeMulti(SlateApp, InKeyEvent);
	}
}

void UVimTextEditorSubsystem::DeleteLineVisualMode(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	DeleteLine(SlateApp, InKeyEvent);
	FVimInputProcessor::Get()->SetVimMode(SlateApp, EVimMode::Normal);
}

void UVimTextEditorSubsystem::DeleteLineNormalModeSingle(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	DeleteLineSingle(SlateApp, InKeyEvent);
	HandleEditableUX();
}

void UVimTextEditorSubsystem::DeleteLineSingle(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	const auto EditTextBox = ActiveEditableTextBox.Pin();
	if (!EditTextBox.IsValid())
		return;

	EditTextBox->SetText(FText::GetEmpty());
}

void UVimTextEditorSubsystem::DeleteLineNormalModeMulti(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	const TSharedPtr<SMultiLineEditableTextBox> MultiTextBox =
		ActiveMultiLineEditableTextBox.Pin();
	if (!MultiTextBox.IsValid())
		return;

	// For later comparison with the new line to determine if we can preserve
	// character index.
	FTextLocation OriginLocation = MultiTextBox->GetCursorLocation();
	if (!OriginLocation.IsValid())
		return;

	DeleteLineMulti(SlateApp, FKeyEvent());

	// Check if we can keep the same cursor index or (second best alternative)
	// if we should opt to go to the end of the line.
	FString NewTextLine;
	MultiTextBox->GetCurrentTextLine(NewTextLine);
	int32 NewLineLen = NewTextLine.Len();
	if (NewLineLen >= OriginLocation.GetOffset())
		MultiTextBox->GoTo(OriginLocation);
	else
	{
		FTextLocation NewGoToLocation(OriginLocation.GetLineIndex(), NewLineLen);
		MultiTextBox->GoTo(NewGoToLocation);
	}

	HandleEditableUX(); // Highlight current char properly in Normal Mode
}

void UVimTextEditorSubsystem::DeleteLineMulti(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	const TSharedPtr<SMultiLineEditableTextBox> MultiTextBox =
		ActiveMultiLineEditableTextBox.Pin();
	if (!MultiTextBox.IsValid())
		return;

	TSharedRef<FVimInputProcessor> VimProc = FVimInputProcessor::Get();
	ClearTextSelection(false /*Insert*/);

	DeleteCurrentLineContent(SlateApp);

	// If at the end of the document, we can't normally delete the line. We have
	// to firstly go Up and to the Right(End), and then perform the deletion.
	if (IsMultiLineCursorAtEndOfDocument(SlateApp, MultiTextBox.ToSharedRef()))
	{
		VimProc->SimulateKeyPress(SlateApp, EKeys::Up);
		VimProc->SimulateKeyPress(SlateApp, EKeys::End);
	}

	VimProc->SimulateKeyPress(SlateApp, EKeys::Delete); // Delete the line itself
}

void UVimTextEditorSubsystem::DeleteCurrentLineContent(FSlateApplication& SlateApp)
{
	TSharedRef<FVimInputProcessor> VimProc = FVimInputProcessor::Get();
	VimProc->SimulateKeyPress(SlateApp, EKeys::Home);
	VimProc->SimulateKeyPress(SlateApp, EKeys::End, ModShiftDown);
	VimProc->SimulateKeyPress(SlateApp, EKeys::Delete); // Delete selection
}

void UVimTextEditorSubsystem::DeleteCurrentSelection(FSlateApplication& SlateApp, const bool bYankSelection)
{
	// if paste with shift; Don't copy the replaced text, else copy.
	if (bYankSelection)
		YankCurrentlySelectedText();

	TSharedRef<FVimInputProcessor> VimProc = FVimInputProcessor::Get();
	ToggleReadOnly(true, false /*Don't Clear Selection*/);
	VimProc->SimulateKeyPress(SlateApp, EKeys::Delete);
	ToggleReadOnly(true, true /*Stop Blinking*/);
}

void UVimTextEditorSubsystem::ChangeEntireLine(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	switch (EditableWidgetsFocusState)
	{
		case EUMEditableWidgetsFocusState::None:
			return; // Theoretically not reachable

		case EUMEditableWidgetsFocusState::SingleLine:
			DeleteLineSingle(SlateApp, InKeyEvent);
			break;

		case EUMEditableWidgetsFocusState::MultiLine:
			ChangeEntireLineMulti(SlateApp, InKeyEvent);
			break;
	}
	FVimInputProcessor::Get()->SetVimMode(SlateApp, EVimMode::Insert);
}

void UVimTextEditorSubsystem::ChangeEntireLineMulti(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	DeleteLineMulti(SlateApp, InKeyEvent); // Also goes to insert mode
	if (GetMultiLineCount() == 1)		   // Multi's with 1 line are G2G
		return;

	bool bAtEnd = IsMultiLineCursorAtEndOfDocument(true /*Last Line as End*/);

	AppendBreakMultiLine();
	if (!bAtEnd)
		FVimInputProcessor::Get()->SimulateKeyPress(SlateApp, EKeys::Up);
}

int32 UVimTextEditorSubsystem::GetMultiLineCount()
{
	if (const TSharedPtr<SMultiLineEditableTextBox> MultiTextBox =
			ActiveMultiLineEditableTextBox.Pin())
	{
		const TSharedPtr<SMultiLineEditableText> MultiLineText = GetMultilineEditableFromBox(MultiTextBox.ToSharedRef());

		if (MultiLineText.IsValid())
			return MultiLineText->GetTextLineCount();
	}
	return INDEX_NONE;
}

void UVimTextEditorSubsystem::DeleteToEndOfLine(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	ChangeToEndOfLine(SlateApp, InKeyEvent);
	FVimInputProcessor::Get()->SetVimMode(SlateApp, EVimMode::Normal);
}

void UVimTextEditorSubsystem::DeleteInsideWord(FSlateApplication& SlateApp, const TArray<FInputChord>& InSequence)
{
	if (SelectInsideWord(SlateApp))
	{
		DeleteCurrentSelection(SlateApp, true /*Yank Deleted Text*/);
		FVimInputProcessor::Get()->SetVimMode(SlateApp, EVimMode::Normal);
	}
}

void UVimTextEditorSubsystem::ChangeToEndOfLine(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	TSharedRef<FVimInputProcessor> InputProc = FVimInputProcessor::Get();

	// We wanna simulate one time to the left is we're aligned right to properly
	// be inserted at the correct offset.
	if (IsCursorAlignedRight(SlateApp)
		&& !IsMultiLineCursorAtBeginningOfLine(true /*Zero Validation Offset*/))
		InputProc->SimulateKeyPress(SlateApp, EKeys::Left);

	InputProc->SetVimMode(SlateApp, EVimMode::Insert);
	InputProc->SimulateKeyPress(SlateApp, EKeys::End, ModShiftDown);
	InputProc->SimulateKeyPress(SlateApp, EKeys::Delete);
}

void UVimTextEditorSubsystem::ChangeVisualMode(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	TSharedRef<FVimInputProcessor> InputProc = FVimInputProcessor::Get();
	DeleteCurrentSelection(SlateApp, true /*Yank Deleted Text*/);
	InputProc->SetVimMode(SlateApp, EVimMode::Insert);
}

void UVimTextEditorSubsystem::ChangeInsideWord(FSlateApplication& SlateApp, const TArray<FInputChord>& InSequence)
{
	if (SelectInsideWord(SlateApp))
		ChangeVisualMode(SlateApp, FKeyEvent());
}

FUMStringInfo UVimTextEditorSubsystem::GetFStringInfo(const FString& InputString)
{
	FUMStringInfo Info;

	// Handle an empty string.
	if (InputString.IsEmpty())
	{
		Info.LineCount = 0;
		Info.LastCharIndex = INDEX_NONE;
		return Info;
	}

	// Split the string into lines using "\n" as the delimiter.
	// Setting bCullEmpty to false ensures that empty lines are also kept.
	TArray<FString> Lines;
	InputString.ParseIntoArray(Lines, TEXT("\n"), /*bCullEmpty=*/false);
	Info.LineCount = Lines.Num();

	// Find the index of the last character that is not a newline or carriage return.
	// This loop goes backwards over the original string.
	Info.LastCharIndex = INDEX_NONE;
	for (int32 i = InputString.Len() - 1; i >= 0; --i)
	{
		if (InputString[i] != '\n' && InputString[i] != '\r')
		{
			Info.LastCharIndex = i;
			break;
		}
	}

	return Info;
}

void UVimTextEditorSubsystem::AddDebuggingText(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	TSharedRef<FVimInputProcessor> InputProc = FVimInputProcessor::Get();
	const FText					   DebugContent = FText::FromString(
		   "ABCD\nEFGH\nIJKL\nMNOP\nQRST\nUVW\nXYZ");
	SetActiveEditableText(DebugContent);
	SetCursorSelectionToDefaultLocation(SlateApp);
}

TSharedPtr<SMultiLineEditableText> UVimTextEditorSubsystem::GetMultilineEditableFromBox(const TSharedRef<SMultiLineEditableTextBox> InMultiLineTextBox)
{
	TSharedPtr<SWidget> FoundMultiLine;
	if (!FUMSlateHelpers::TraverseFindWidget(InMultiLineTextBox->GetContent(), FoundMultiLine, MultiEditableTextType))
		return nullptr;

	return StaticCastSharedPtr<SMultiLineEditableText>(FoundMultiLine);
}

TSharedPtr<SEditableText> UVimTextEditorSubsystem::GetSingleEditableFromBox(const TSharedRef<SEditableTextBox> InEditableTextBox)
{
	TSharedPtr<SWidget> FoundSingleLine;
	if (!FUMSlateHelpers::TraverseFindWidget(InEditableTextBox->GetContent(), FoundSingleLine, SingleEditableTextType))
		return nullptr;

	return StaticCastSharedPtr<SEditableText>(FoundSingleLine);
}

void UVimTextEditorSubsystem::VimCommandSelectAll(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	// NOTE:
	// The delays are essential for proper selection because we're switching to
	// visual mode.
	TSharedRef<FVimInputProcessor> InputProc = FVimInputProcessor::Get();

	switch (EditableWidgetsFocusState)
	{
		case EUMEditableWidgetsFocusState::None:
			break; // Theoretically not reachable

		case EUMEditableWidgetsFocusState::SingleLine:
			if (const auto EditTextBox = ActiveEditableTextBox.Pin())
			{
				InputProc->SimulateKeyPress(SlateApp, EKeys::Home);
				InputProc->SetVimMode(SlateApp, EVimMode::Visual);
				FTimerHandle TimerHandle;
				GEditor->GetTimerManager()->SetTimer(
					TimerHandle,
					[this, &SlateApp]() {
						TSharedRef<FVimInputProcessor> InputProc = FVimInputProcessor::Get();
						InputProc->SimulateKeyPress(SlateApp, EKeys::End, ModShiftDown);
					},
					0.025f, false);
			}

		case EUMEditableWidgetsFocusState::MultiLine:
			if (const TSharedPtr<SMultiLineEditableTextBox> MultiTextBox =
					ActiveMultiLineEditableTextBox.Pin())
			{
				MultiTextBox->GoTo(FTextLocation(0, 0));
				InputProc->SetVimMode(SlateApp, EVimMode::Visual);
				HandleVisualModeGoToStartOrEndMultiLine(SlateApp, false /*End*/);
				FTimerHandle TimerHandle;
				GEditor->GetTimerManager()->SetTimer(
					TimerHandle,
					[this, &SlateApp]() {
						TSharedRef<FVimInputProcessor> InputProc = FVimInputProcessor::Get();
						HandleUpDownMultiLineVisualMode(SlateApp, EKeys::Down);
					},
					0.025f, false);
			}
	}
}

void UVimTextEditorSubsystem::DebugSelectStartToEnd(const FTextLocation& StartLocation, const FTextLocation& GoToLocation)
{
	Logger.Print(
		FString::Printf(
			TEXT("Start Line Index: %d\n Start Offset: %d\n GoTo Line Index: %d\n GoTo Offset: %d"),
			StartLocation.GetLineIndex(), StartLocation.GetOffset(), GoToLocation.GetLineIndex(), GoToLocation.GetOffset()),
		true);
}

void UVimTextEditorSubsystem::NavigateW(
	FSlateApplication& SlateApp, const TArray<FInputChord>& InSequence)
{
	// 'w' – forward word (small word).
	NavigateWord(SlateApp, false,
		&FVimTextEditorUtils::FindNextWordBoundary);
}

void UVimTextEditorSubsystem::NavigateBigW(
	FSlateApplication& SlateApp, const TArray<FInputChord>& InSequence)
{
	// 'W' – forward word (big word).
	NavigateWord(SlateApp, true,
		&FVimTextEditorUtils::FindNextWordBoundary);
}

void UVimTextEditorSubsystem::NavigateB(
	FSlateApplication& SlateApp, const TArray<FInputChord>& InSequence)
{
	// 'b' – backward word (small word).
	NavigateWord(SlateApp, false,
		&FVimTextEditorUtils::FindPreviousWordBoundary);
}

void UVimTextEditorSubsystem::NavigateBigB(
	FSlateApplication& SlateApp, const TArray<FInputChord>& InSequence)
{
	// 'B' – backward word (big word).
	NavigateWord(SlateApp, true,
		&FVimTextEditorUtils::FindPreviousWordBoundary);
}

void UVimTextEditorSubsystem::NavigateE(FSlateApplication& SlateApp, const TArray<FInputChord>& InSequence)
{
	// 'e' – forward to end of word (small word)
	NavigateWord(SlateApp, false,
		&FVimTextEditorUtils::FindNextWordEnd);
}

void UVimTextEditorSubsystem::NavigateBigE(FSlateApplication& SlateApp, const TArray<FInputChord>& InSequence)
{
	// 'E' – forward to end of word (big word)
	NavigateWord(SlateApp, true,
		&FVimTextEditorUtils::FindNextWordEnd);
}

void UVimTextEditorSubsystem::NavigateGE(FSlateApplication& SlateApp, const TArray<FInputChord>& InSequence)
{
	// 'ge' – backward to end of word (small word)
	NavigateWord(SlateApp, false,
		&FVimTextEditorUtils::FindPreviousWordEnd);
}

void UVimTextEditorSubsystem::NavigateGBigE(FSlateApplication& SlateApp, const TArray<FInputChord>& InSequence)
{
	// 'gE' – backward to end of word (big word)
	NavigateWord(SlateApp, true,
		&FVimTextEditorUtils::FindPreviousWordEnd);
}

void UVimTextEditorSubsystem::NavigateWordMotion(
	FSlateApplication&		   SlateApp,
	const TArray<FInputChord>& InSequence)
{
	// Determine if we're dealing with a big word (WORD) or small word (word)
	bool bIsBigWord = false;

	// Determine direction and position (beginning/end of word)
	bool bIsForward = true;
	bool bIsWordEnd = false;
	bool bIsGCommand = false;

	// Parse the sequence to determine the exact motion
	if (InSequence.Num() == 1)
	{
		// Single key commands: w, W, b, B, e, E
		const FInputChord& Chord = InSequence[0];

		if (Chord.Key == EKeys::W)
		{
			// w or W - forward to beginning of word
			bIsBigWord = Chord.bShift;
			bIsForward = true;
			bIsWordEnd = false;
		}
		else if (Chord.Key == EKeys::B)
		{
			// b or B - backward to beginning of word
			bIsBigWord = Chord.bShift;
			bIsForward = false;
			bIsWordEnd = false;
		}
		else if (Chord.Key == EKeys::E)
		{
			// e or E - forward to end of word
			bIsBigWord = Chord.bShift;
			bIsForward = true;
			bIsWordEnd = true;
		}
	}
	else if (InSequence.Num() == 2 && InSequence[0].Key == EKeys::G)
	{
		// 'g' commands: ge, gE
		bIsGCommand = true;
		const FInputChord& SecondChord = InSequence[1];

		if (SecondChord.Key == EKeys::E)
		{
			// ge or gE - backward to end of word
			bIsBigWord = SecondChord.bShift;
			bIsForward = false;
			bIsWordEnd = true;
		}
	}

	// Select the appropriate word boundary function
	auto WordBoundaryFunc = bIsForward
		? (bIsWordEnd
				  ? &FVimTextEditorUtils::FindNextWordEnd
				  : &FVimTextEditorUtils::FindNextWordBoundary)
		: (bIsWordEnd
				  ? &FVimTextEditorUtils::FindPreviousWordEnd
				  : &FVimTextEditorUtils::FindPreviousWordBoundary);

	// Call the navigation function with determined parameters
	NavigateWord(SlateApp, bIsBigWord, WordBoundaryFunc);
}

int32 UVimTextEditorSubsystem::GetCursorOffsetSingle()
{
	if (const auto EditTextBox = ActiveEditableTextBox.Pin())
	{
		// TODO:
		// Hypothetical function – implement according to your API.
		// return EditTextBox->GetCursorOffset();
	}
	return 0;
}

void UVimTextEditorSubsystem::SetCursorOffsetSingle(int32 NewOffset)
{
	if (const auto EditTextBox = ActiveEditableTextBox.Pin())
	{
		// TODO:
		// Hypothetical function – implement according to your API.
		// EditTextBox->SetCursorOffset(NewOffset);
	}
}

//------------------------------------------------------------------------------
// Main Navigation Functions
//------------------------------------------------------------------------------
// These functions calculate a new cursor position (as an absolute offset)
// and then update the editor accordingly (handling multi-line vs. single-line).

void UVimTextEditorSubsystem::NavigateWord(
	FSlateApplication& SlateApp,
	bool			   bBigWord,
	int32 (*FindWordBoundary)(
		const FString& Text, int32 CurrentPos, bool bBigWord))
{
	FString Text;
	if (!GetActiveEditableTextContent(Text))
		return;
	// Logger.Print(FString::Printf(TEXT("Editable Text: %s"), *Text), true);

	TSharedRef<FVimInputProcessor> VimProc = FVimInputProcessor::Get();
	FTextLocation				   OriginCursorLocation;
	if (!GetCursorLocation(SlateApp, OriginCursorLocation))
		return;
	// Logger.Print(FString::Printf(TEXT("Origin Offset: %d"), OriginCursorLocation.GetOffset()), true);

	int32 CurrentAbs = FVimTextEditorUtils::TextLocationToAbsoluteOffset(
		Text,
		FTextLocation(
			OriginCursorLocation.GetLineIndex(),
			OriginCursorLocation.GetOffset()
				- (IsCursorAlignedRight(SlateApp) ? 1 /*Comp-RightAlign*/ : 0)));

	// Logger.Print(FString::Printf(TEXT("Current Abs: %d"), CurrentAbs), true);

	const int32 NewAbs = (*FindWordBoundary)(Text, CurrentAbs, bBigWord);

	// Logger.Print(FString::Printf(TEXT("New Abs: %d"), NewAbs), true);

	FTextLocation NewLoc;
	FVimTextEditorUtils::AbsoluteOffsetToTextLocation(Text, NewAbs, NewLoc);
	if (!NewLoc.IsValid())
		return;
	// Logger.Print(FString::Printf(TEXT("Final GoTo Location:\nLine Index: %d\nOffset: %d"), NewLoc.GetLineIndex(), NewLoc.GetOffset()), true);

	if (CurrentVimMode == EVimMode::Visual)
	{
		SelectTextInRange(SlateApp, StartCursorLocationVisualMode, NewLoc);

		if (NewLoc.GetOffset() > 1 && IsCursorAlignedRight(SlateApp))
			VimProc->SimulateKeyPress(SlateApp, EKeys::Right, ModShiftDown);
	}
	else // Normal Mode
	{
		GoToTextLocation(SlateApp, NewLoc);
		if (NewAbs == Text.Len()) // We're at end, sim Left, then Shift+Right
			SetCursorSelectionToDefaultLocation(SlateApp /*Align Right*/);
		else
			VimProc->SimulateKeyPress(SlateApp, EKeys::Right, ModShiftDown);
	}
}
void UVimTextEditorSubsystem::SelectInsideWord()
{
	SelectInsideWord(FSlateApplication::Get());
}

bool UVimTextEditorSubsystem::SelectInsideWord(FSlateApplication& SlateApp)
{
	FString Text;
	if (!GetActiveEditableTextContent(Text))
		return false;

	TSharedRef<FVimInputProcessor> VimProc = FVimInputProcessor::Get();
	FTextLocation				   OriginCursorLocation;
	if (!GetCursorLocation(SlateApp, OriginCursorLocation))
		return false;

	int32 CurrentAbs = FVimTextEditorUtils::TextLocationToAbsoluteOffset(
		Text,
		FTextLocation(
			OriginCursorLocation.GetLineIndex(),
			OriginCursorLocation.GetOffset()
				- (IsCursorAlignedRight(SlateApp) ? 1 /*Comp-RightAlign*/ : 0)));

	TPair<int32, int32> WordBoundaries;
	if (!FVimTextEditorUtils::GetAbsWordBoundaries(Text, CurrentAbs, WordBoundaries, false))
		return false;

	// Get Beginning & End Text Locations
	FTextLocation WordStart;
	FVimTextEditorUtils::AbsoluteOffsetToTextLocation(Text, WordBoundaries.Key, WordStart);
	if (!WordStart.IsValid())
		return false;
	FTextLocation WordEnd;
	FVimTextEditorUtils::AbsoluteOffsetToTextLocation(Text, WordBoundaries.Value, WordEnd);
	if (!WordEnd.IsValid())
		return false;

	VimProc->SetVimMode(SlateApp, EVimMode::Visual);
	SelectTextInRange(SlateApp, WordStart, WordEnd,
		true, /* Jump to Start of Boundary */
		true /*Ignore Set Cursor to Default to Align properly*/);

	return true;
}

bool UVimTextEditorSubsystem::GoToTextLocation(FSlateApplication& SlateApp, const FTextLocation& InTextLocation)
{
	switch (EditableWidgetsFocusState)
	{
		case EUMEditableWidgetsFocusState::SingleLine:
			if (const auto EditTextBox = ActiveEditableTextBox.Pin())
			{
				const TSharedPtr<SEditableText> EditableText = GetSingleEditableFromBox(EditTextBox.ToSharedRef());
				if (EditableText.IsValid())
				{
					EditableText->GoTo(FTextLocation(0, InTextLocation.GetOffset()));
					return true;
				}
				return false;
			}
			break;

		case EUMEditableWidgetsFocusState::MultiLine:
			if (const TSharedPtr<SMultiLineEditableTextBox> MultiTextBox =
					ActiveMultiLineEditableTextBox.Pin())
			{
				MultiTextBox->GoTo(InTextLocation);
				return true;
			}
			break;

		default:
			return false;
	}
	return false;
}

/** DEPRECATED - Discovered that we have a built-in GoTo method also for Single
 * Line Editable Text */
void UVimTextEditorSubsystem::GoToTextLocationSingleLine(
	FSlateApplication&				   SlateApp,
	const TSharedRef<SEditableTextBox> InTextBox,
	const int32						   InGoToOffset)
{
	const TSharedRef<FVimInputProcessor> VimProc = FVimInputProcessor::Get();
	VimProc->SimulateKeyPress(SlateApp, EKeys::Home);

	// Logger.Print(FString::Printf(TEXT("GoTo Singe-Line Offset: %d"), InGoToOffset), true);

	for (int32 i{ 0 }; i < InGoToOffset; ++i)
		VimProc->SimulateKeyPress(SlateApp, EKeys::Right);
}

bool UVimTextEditorSubsystem::GetCursorLocation(FSlateApplication& SlateApp, FTextLocation& OutTextLocation)
{
	switch (EditableWidgetsFocusState)
	{
		case EUMEditableWidgetsFocusState::SingleLine:
			if (const auto EditTextBox = ActiveEditableTextBox.Pin())
			{
				OutTextLocation =
					FTextLocation(0, /*Single-Line is always 1 line and 0 index*/
						GetCursorLocationSingleLine(
							SlateApp, EditTextBox.ToSharedRef()));
				return true;
			}
			break;

		case EUMEditableWidgetsFocusState::MultiLine:
			if (const TSharedPtr<SMultiLineEditableTextBox> MultiTextBox =
					ActiveMultiLineEditableTextBox.Pin())
			{
				// MultiTextBox->SetOnKeyCharHandler();
				// MultiTextBox->SetOnKeyDownHandler();
				OutTextLocation = MultiTextBox->GetCursorLocation();
				return OutTextLocation.IsValid();
			}
			break;

		default:
			return false;
	}
	return false;
}

FReply UVimTextEditorSubsystem::OnSingleLineKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	Logger.Print("SingleLine KeyDown", true);

	const TSharedPtr<SEditableTextBox> TextBox = ActiveEditableTextBox.Pin();
	if (!TextBox.IsValid())
		return FReply::Unhandled();

	// Check if the pressed key is Enter/Return
	// This is needed for proper commision when we're in ReadOnly mode
	if (InKeyEvent.GetKey() == EKeys::Enter)
	{
		Logger.Print("Enter clicked", true);
		TextBox->SetIsReadOnly(false);
	}

	if (bIsFirstSingleLineKeyStroke
		&& !FUMInputHelpers::IsKeyEventModifierOnly(InKeyEvent))
	{
		Logger.Print("First Single Line Key Stroke", true);
		bIsFirstSingleLineKeyStroke = false;
		VerifySingleLineEditableFirstStroke(InKeyEvent, TextBox->GetText());
	}
	return FReply::Unhandled();
}

void UVimTextEditorSubsystem::VerifySingleLineEditableFirstStroke(const FKeyEvent& InKeyEvent, const FText& PreStrokeContent)
{
	FTimerHandle TimerHandle;
	GEditor->GetTimerManager()->SetTimer(
		TimerHandle,
		[this, InKeyEvent, PreStrokeContent]() {
			if (const TSharedPtr<SEditableTextBox> TextBox = ActiveEditableTextBox.Pin())
			{
				// If the text was not changed, it means our stroke did not went
				// through, thus we need to manually add it.
				if (TextBox->GetText().EqualTo(PreStrokeContent))
				{
					TextBox->SetText(FText::FromString(
						FString::Chr(FUMInputHelpers::GetCharFromKeyEvent(InKeyEvent))));
					RefreshActiveEditable(FSlateApplication::Get());
				}
			}
		},
		0.025f, false);
}

FReply UVimTextEditorSubsystem::OnMultiLineKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	// Logger.Print("MultiLine KeyDown", true);
	const TSharedPtr<SMultiLineEditableTextBox> MultiTextBox =
		ActiveMultiLineEditableTextBox.Pin();
	if (!MultiTextBox.IsValid())
		return FReply::Unhandled();

	// Check if the pressed key is Enter/Return
	if (!bIsCurrMultiLineChildOfConsole // For consoles, commit as usual
		&& InKeyEvent.GetKey() == EKeys::Enter)
	{
		if (InKeyEvent.IsControlDown()) // Commit text
		{
			MultiTextBox->SetIsReadOnly(false);
			return FReply::Unhandled();
		}
		else
		{
			MultiTextBox->InsertTextAtCursor(FText::FromString("\n"));
			return FReply::Handled();
		}
	}
	return FReply::Unhandled();
}

int32 UVimTextEditorSubsystem::GetCursorLocationSingleLine(FSlateApplication& SlateApp, const TSharedRef<SEditableTextBox> InTextBox)
{
	const FString InitialTextContent = InTextBox->GetText().ToString();
	if (InitialTextContent.IsEmpty())
	{
		// Logger.Print("GetCursorLocationSingleLine: 0 (Editable is Empty)", true);
		return 0;
	}

	const FString OriginSelectedText = InTextBox->AnyTextSelected()
		? InTextBox->GetSelectedText().ToString()
		: "";

	const int32 InitTextLen = InitialTextContent.Len();
	const int32 OriginSelTextLen = OriginSelectedText.Len();

	FKey	   MeasureDirection;
	FKey	   ReverseDirection;
	const bool bIsCursorAlignedRight = IsCursorAlignedRight(SlateApp);

	if (bIsCursorAlignedRight)
	{
		MeasureDirection = EKeys::End;
		ReverseDirection = EKeys::Left;
	}
	else
	{
		MeasureDirection = EKeys::Home;
		ReverseDirection = EKeys::Right;
	}

	const TSharedRef<FVimInputProcessor> VimProc = FVimInputProcessor::Get();
	// Select until the end or beginning of the line (via Home or End)
	VimProc->SimulateKeyPress(SlateApp, MeasureDirection, ModShiftDown);

	// Fetch the new selection to compare against the original selection.
	const FString NewSelectedText = InTextBox->GetSelectedText().ToString();
	const int32	  NewSelTextLen = NewSelectedText.Len();

	// Calculate the offset for Right & Left aligned cursors
	const int32 CursorOffset = bIsCursorAlignedRight
		? InitTextLen - NewSelTextLen + OriginSelTextLen
		: NewSelTextLen - OriginSelTextLen;

	// Reverse Offset to the Original Location to retain things as they were
	for (int32 i{ 0 }; i < (NewSelTextLen - OriginSelTextLen); ++i)
		VimProc->SimulateKeyPress(SlateApp, ReverseDirection, ModShiftDown);

	// Logger.Print(FString::Printf(TEXT("Single-Line Offset: %d\nNew Selected Text Length: %d\n Origin Selected Text Len: %d"), CursorOffset, NewSelTextLen, OriginSelTextLen), true);
	return CursorOffset;

	/*
	Right Align Flow:
	----------------
	Example 1:
	  Text:     abcd*efg
	  Position: ----^
	  - Total Text Length:       7
	  - Origin Selection Length: 1 (On 'd', default right-aligned)
	  - New Selection Length:    4 ('d' -> 'g', i.e. 'd + efg')
	  - Offset = Total(7) - NewSel(4) + OriginSel(1) = 4

	Example 2:
	  Text:     abcd*efghijkl
	  Position: ----^
	  - Total Text Length:       12
	  - Origin Selection Length: 1 (On 'd', default right-aligned)
	  - New Selection Length:    9 ('d' -> 'l', i.e. 'd + efghijkl')
	  - Offset = Total(12) - NewSel(9) + OriginSel(1) = 4

	Left Align Flow:
	---------------
	Example 1:
	  Text:     abcd*efg
	  Position: ----^
	  - Total Text Length:       7 (irrelevant)
	  - Origin Selection Length: 1 (On 'e', left-aligned)
	  - New Selection Length:    5 ('e' -> 'a', i.e. 'abcd + e')
	  - Offset = NewSel(5) - OriginSel(1) = 4

	Example 2:
	  Text:     abcd*efghijkl
	  Position: ----^
	  - Total Text Length:       12 (irrelevant)
	  - Origin Selection Length: 7 (On 'k' -> 'e', i.e. 'efghijk')
	  - New Selection Length:    11 ('k' -> 'a', i.e. 'abcd + efghijk')
	  - Offset = NewSel(11) - OriginSel(7) = 4
	*/
}

void UVimTextEditorSubsystem::JumpToEndOrStartOfLine(FSlateApplication& SlateApp,
	void (UVimTextEditorSubsystem::*HandleLeftOrRightNavigation)(FSlateApplication&))
{
	FTextLocation PrevTextLocation;
	FTextLocation CurrTextLocation;
	do
	{
		if (!GetCursorLocation(SlateApp, PrevTextLocation))
			return;
		(this->*HandleLeftOrRightNavigation)(SlateApp);
		if (!GetCursorLocation(SlateApp, CurrTextLocation))
			return;
	}
	while (CurrTextLocation != PrevTextLocation);
}

void UVimTextEditorSubsystem::JumpToStartOfLine(
	FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	JumpToEndOrStartOfLine(SlateApp, &UVimTextEditorSubsystem::HandleLeftNavigation);
}

void UVimTextEditorSubsystem::JumpToEndOfLine(
	FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	JumpToEndOrStartOfLine(SlateApp, &UVimTextEditorSubsystem::HandleRightNavigation);
}

void UVimTextEditorSubsystem::YankCurrentlySelectedText()
{
	FString SelectedText;
	if (GetSelectedText(SelectedText))
	{
		YankData.SetData(SelectedText, EUMYankType::Characterwise);
	}
}

void UVimTextEditorSubsystem::YankCharacter(FSlateApplication& SlateApp, const TArray<FInputChord>& InSequence)
{
	FString Text = "";
	GetSelectedText(Text);
	YankData.SetData(Text, EUMYankType::Characterwise);

	FTextSelection SelectionRange;
	if (GetSelectionRange(SlateApp, SelectionRange))
	{
		// DebugSelectionRange(SelectionRange);

		// Per Vim rules; After yanking, we should jump to the leftmost selected
		// text available.
		GoToTextLocation(SlateApp, SelectionRange.GetBeginning());

		TSharedRef<FVimInputProcessor> VimProc = FVimInputProcessor::Get();
		// We wanna sim one time to the right to compensate and align right.
		VimProc->SimulateKeyPress(SlateApp, EKeys::Right);
		VimProc->SetVimMode(SlateApp, EVimMode::Normal); // Visual -> Normal
	}
}

void UVimTextEditorSubsystem::YankLine(FSlateApplication& SlateApp, const TArray<FInputChord>& InSequence)
{
	FString Text = "";
	if (GetActiveEditableTextContent(Text, true /*GetCurrLine if Multi*/))
		YankData.SetData(Text, EUMYankType::Linewise);
}

void UVimTextEditorSubsystem::Paste(FSlateApplication& SlateApp, const TArray<FInputChord>& InSequence)
{
	switch (YankData.GetType())
	{
		case EUMYankType::Characterwise:
			HandlePasteCharacterwise(SlateApp, InSequence);
			break;

		case EUMYankType::Linewise:
			HandlePasteLinewise(SlateApp, InSequence);
			break;

		default:
			break;
	}
}

void UVimTextEditorSubsystem::HandlePasteCharacterwise(FSlateApplication& SlateApp, const TArray<FInputChord>& InSequence)
{
	const TSharedRef<FVimInputProcessor> InputProc = FVimInputProcessor::Get();
	const FText							 TextToPaste = FText::FromString(YankData.GetText(EditableWidgetsFocusState));

	switch (CurrentVimMode)
	{
		case EVimMode::Normal:
		{
			HandlePasteCharacterwiseNormalMode(SlateApp, InSequence, TextToPaste, InputProc);
			break;
		}
		case EVimMode::Visual:
		{
			HandlePasteCharacterwiseVisualMode(SlateApp, InSequence, TextToPaste, InputProc);
			break;
		}
		default:
			break;
	}
}

void UVimTextEditorSubsystem::HandlePasteCharacterwiseNormalMode(FSlateApplication& SlateApp, const TArray<FInputChord>& InSequence, const FText& TextToPaste, const TSharedRef<FVimInputProcessor> InputProc)
{
	const FInputChord LastChord = InSequence.Last();
	const bool		  bIsShiftDown = LastChord.bShift;

	ClearTextSelection(false);

	if (bIsShiftDown) // Insert *before* the currently selected char
		InputProc->SimulateKeyPress(SlateApp, EKeys::Left);

	InsertTextAtCursor(SlateApp, TextToPaste);
	ToggleReadOnly();
	SetCursorSelectionToDefaultLocation(SlateApp);
}

void UVimTextEditorSubsystem::HandlePasteCharacterwiseVisualMode(FSlateApplication& SlateApp, const TArray<FInputChord>& InSequence, const FText& TextToPaste, const TSharedRef<FVimInputProcessor> InputProc)
{
	DeleteCurrentSelection(SlateApp, true /*Yank Deleted Text*/);
	ClearTextSelection(false);
	InsertTextAtCursor(SlateApp, TextToPaste);
	InputProc->SetVimMode(SlateApp, EVimMode::Normal);
}

// TODO: Fix bug in end of document Visual Mode nav
void UVimTextEditorSubsystem::HandlePasteLinewise(FSlateApplication& SlateApp, const TArray<FInputChord>& InSequence)
{
	const TSharedRef<FVimInputProcessor> InputProc = FVimInputProcessor::Get();
	const FInputChord					 LastChord = InSequence.Last();
	const bool							 bIsShiftDown = LastChord.bShift;

	AppendNewLine(SlateApp,
		FUMInputHelpers::GetKeyEventFromKey(FKey(), bIsShiftDown));

	const FString TextToPaste = YankData.GetText(EditableWidgetsFocusState);

	InsertTextAtCursor(SlateApp, FText::FromString(TextToPaste));

	InputProc->SetVimMode(SlateApp, EVimMode::Normal);

	// In SingleLine, the reasonable UX, I think, is to go to the end-of-the-line
	// if pasting a Linewise.
	// For MultiLine, it'll always be false (i.e. go-to-start of line)
	// because we're actually appending new lines.
	const bool bIsSingleLine = EditableWidgetsFocusState == EUMEditableWidgetsFocusState::SingleLine;
	const bool bGoToEnd = bIsSingleLine && !bIsShiftDown;
	HandleGoToStartOrEndSingleLine(SlateApp, bGoToEnd);

	InputProc->SetVimMode(SlateApp, EVimMode::Normal);
}

bool UVimTextEditorSubsystem::IsMultiChildOfConsole(const TSharedRef<SMultiLineEditableTextBox> InMultiBox)
{
	const FString ConsoleType = "SConsoleInputBox";

	TSharedPtr<SWidget> LookupWidget = InMultiBox;
	while (LookupWidget->IsParentValid())
	{
		LookupWidget = LookupWidget->GetParentWidget();
		if (LookupWidget.IsValid()
			&& LookupWidget->GetTypeAsString().Equals(ConsoleType))
			return true;
	}
	return false;
}

void UVimTextEditorSubsystem::YankInsideWord(FSlateApplication& SlateApp, const TArray<FInputChord>& InSequence)
{
	if (SelectInsideWord(SlateApp))
		YankCharacter(SlateApp, InSequence);
}

void UVimTextEditorSubsystem::ReplaceCharacter(FSlateApplication& SlateApp, const TArray<FInputChord>& InSequence)
{
	const TSharedRef<FVimInputProcessor> VimProc = FVimInputProcessor::Get();
	AssignEditableBorder(false, EVimMode::Insert); // Sim Insert border
	VimProc->Possess(this, &UVimTextEditorSubsystem::ReplaceCharacterSingle);
}

void UVimTextEditorSubsystem::ReplaceCharacterSingle(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	if (FUMInputHelpers::IsKeyEventModifierOnly(InKeyEvent))
		return;

	const TCHAR Char = InKeyEvent.GetCharacter();
	if (FChar::IsPrint(Char)) // Ignore and abort if none-printable like Escape.
	{
		DeleteCurrentSelection(SlateApp, false /*Don't yank*/);

		FString CharStr = FString::Chr(Char);
		if (!InKeyEvent.IsShiftDown())
			CharStr = CharStr.ToLower();
		InsertTextAtCursor(SlateApp, FText::FromString(CharStr));

		ToggleReadOnly();
		SetCursorSelectionToDefaultLocation(SlateApp);
	}

	AssignEditableBorder(); // Return to default per Vim Mode border
	FVimInputProcessor::Get()->Unpossess(this);
}

void UVimTextEditorSubsystem::BeginFindChar(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	bFindPreviousChar = InKeyEvent.IsShiftDown();
	FVimInputProcessor::Get()->Possess(this, &UVimTextEditorSubsystem::HandleFindChar);
}

void UVimTextEditorSubsystem::HandleFindChar(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	// Early exit if only modifier keys were pressed
	if (FUMInputHelpers::IsKeyEventModifierOnly(InKeyEvent))
		return;

	// Get the character to find
	const TCHAR CharToFind = FUMInputHelpers::GetCharFromKeyEvent(InKeyEvent);

	// Ignore none-prinatble characters like Escape, BackSpace, etc.
	if (!FChar::IsPrint(CharToFind))
		Logger.Print("None Printable", true);

	else if (TryFindAndMoveToCursor(SlateApp, CharToFind))
		Logger.Print("Found Char", true);

	FVimInputProcessor::Get()->Unpossess(this); // Release
}

bool UVimTextEditorSubsystem::TryFindAndMoveToCursor(FSlateApplication& SlateApp, TCHAR CharToFind)
{
	// Get the current text, where we'll try to find the user inputted character.
	FString CurrentLineText;
	if (!GetActiveEditableTextContent(CurrentLineText, true /* CurrLine-Only */))
		return false;

	// Get the origin cursor location, where we'll start the searching from.
	FTextLocation OriginCursorLocation;
	if (!GetCursorLocation(SlateApp, OriginCursorLocation)
		|| !OriginCursorLocation.IsValid())
		return false;
	const int32 OriginCursorOffset = OriginCursorLocation.GetOffset();
	Logger.Print(FString::Printf(TEXT("Origin Cursor Offset: %d"), OriginCursorOffset), true);

	Logger.Print(FString::Printf(TEXT("Adjustments: %d"), GetOffsetAdjustmentForFind(SlateApp)), true);

	int32 FoundCharPos = CurrentLineText.Find(
		FString::Chr(CharToFind),
		ESearchCase::CaseSensitive,
		bFindPreviousChar ? ESearchDir::FromEnd : ESearchDir::FromStart,
		OriginCursorOffset + GetOffsetAdjustmentForFind(SlateApp));

	Logger.Print(FString::Printf(TEXT("Found Char Position: %d"), FoundCharPos), true);

	if (FoundCharPos != INDEX_NONE) // Move cursor if character was found
	{
		if (CurrentVimMode == EVimMode::Visual)
			return HandleFindAndMoveToCursorVisualMode(SlateApp, FoundCharPos, OriginCursorLocation);

		else
		{
			GoToTextLocation(SlateApp,
				FTextLocation(OriginCursorLocation.GetLineIndex(), FoundCharPos));
			// SetCursorSelectionToDefaultLocation(SlateApp);
			FVimInputProcessor::Get()->SimulateKeyPress(SlateApp, EKeys::Right, ModShiftDown);
		}
		return true;
	}

	Logger.Print("TryFindAndMoveToCursor: INDEX_NONE", true);
	return false;
}

int32 UVimTextEditorSubsystem::GetOffsetAdjustmentForFind(FSlateApplication& SlateApp)
{
	const bool bIsCursorAlignedRight = IsCursorAlignedRight(SlateApp);

	if (bIsCursorAlignedRight)
		return bFindPreviousChar
			// When aligned right, our cursor is after the selected character.
			// For backward search, we must offset -1 to avoid getting stuck
			// on the current char and properly find the previous character.
			? -1

			// In case we're trying to find the next character and are already
			// aligned right, we're primed for a proper search.
			: 0;

	else // Cursor aligned left:
		// In this case, we have the opposite scenario where when we're aligned
		// left and trying to search for the previous char, we're primed for a
		// proper search. And when we want to find the next char, we have to
		// offset by +1 to not get stuck on the currently selectec character.
		return bFindPreviousChar ? 0 : 1;
}

bool UVimTextEditorSubsystem::HandleFindAndMoveToCursorVisualMode(FSlateApplication& SlateApp, int32 FoundCharPos, FTextLocation OriginCursorLocation)
{
	// We need to work closely to our anchor point as it should dictate how we
	// should wrap the found character.

	FTextSelection TextSelection;
	if (!GetSelectionRange(SlateApp, TextSelection))
		return false;
	const int32 StartLineIndex = TextSelection.LocationA.GetLineIndex();
	const int32 EndLineIndex = TextSelection.LocationB.GetLineIndex();
	if (StartLineIndex < EndLineIndex)
	{
		// Aligned right by lines, thus finding a char won't change
		// the alignment and we're guaranteed to stay aligned right.

		// Should always add +1 to the found char position to properly
		// select it and align correctly.
		++FoundCharPos;
	}
	else if (StartLineIndex == EndLineIndex) // Need to check the found char pos.
	{
		// In this case, finding a char may change the alignment
		// and we should take in consideration the found char index
		// to see if our alignment is in face going to change.
		const int32 StartOffset = TextSelection.LocationA.GetOffset();
		if (StartOffset <= FoundCharPos)
			// We gonna end up being aligned right.
			++FoundCharPos;

		// Else we gonna end up being aligned left, thus we should not alter the
		// found char position.
	}
	// Else: Aligned left by lines, thus finding a char won't change
	// the alignment and we're guaranteed to stay aligned left ( Don't touch the
	// found char position)

	return SelectTextToOffset(SlateApp, OriginCursorLocation, FoundCharPos);
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
	VimInputProcessor->AddKeyBinding_KeyEvent(
		EUMBindingContext::TextEditing,
		{ EKeys::H },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::HandleVimTextNavigation);

	// J: Go Down to Next Pin:
	VimInputProcessor->AddKeyBinding_KeyEvent(
		EUMBindingContext::TextEditing,
		{ EKeys::J },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::HandleVimTextNavigation);

	// K: Go Up to Previous Pin:
	VimInputProcessor->AddKeyBinding_KeyEvent(
		EUMBindingContext::TextEditing,
		{ EKeys::K },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::HandleVimTextNavigation);

	// L: Go to Right Pin / Node:
	VimInputProcessor->AddKeyBinding_KeyEvent(
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
		&UVimTextEditorSubsystem::GoToStartOrEnd);

	VimInputProcessor->AddKeyBinding_KeyEvent(
		EUMBindingContext::TextEditing,
		{ FInputChord(EModifierKey::Shift, EKeys::G) },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::GoToStartOrEnd);

	VimInputProcessor->AddKeyBinding_Sequence(
		EUMBindingContext::TextEditing,
		{ FInputChord(EModifierKey::Control, EKeys::D) },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::JumpUpOrDown);

	VimInputProcessor->AddKeyBinding_Sequence(
		EUMBindingContext::TextEditing,
		{ FInputChord(EModifierKey::Control, EKeys::U) },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::JumpUpOrDown);

	VimInputProcessor->AddKeyBinding_KeyEvent(
		EUMBindingContext::TextEditing,
		{ EKeys::X },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::Delete,
		EVimMode::Any /* Handles deletion in both Normal & Visual Mode*/);

	VimInputProcessor->AddKeyBinding_KeyEvent(
		EUMBindingContext::TextEditing,
		{ EKeys::D },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::Delete,
		EVimMode::Visual /* Handles deletion only in Visual Mode*/);

	VimInputProcessor->AddKeyBinding_KeyEvent(
		EUMBindingContext::TextEditing,
		{ FInputChord(EModifierKey::Shift, EKeys::X) },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::ShiftDeleteNormalMode,
		EVimMode::Normal);

	// Delete current line
	VimInputProcessor->AddKeyBinding_KeyEvent(
		EUMBindingContext::TextEditing,
		{ EKeys::D, EKeys::D },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::DeleteLine,
		EVimMode::Normal);

	VimInputProcessor->AddKeyBinding_KeyEvent(
		EUMBindingContext::TextEditing,
		{ EKeys::D, FInputChord(EModifierKey::Shift, EKeys::Four /*Shift+$*/) },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::DeleteToEndOfLine,
		EVimMode::Normal);

	VimInputProcessor->AddKeyBinding_KeyEvent(
		EUMBindingContext::TextEditing,
		{ FInputChord(EModifierKey::Shift, EKeys::D) },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::DeleteToEndOfLine,
		EVimMode::Normal);

	VimInputProcessor->AddKeyBinding_KeyEvent(
		EUMBindingContext::TextEditing,
		{ FInputChord(EModifierKey::Shift, EKeys::D) },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::DeleteLineVisualMode,
		EVimMode::Visual);

	VimInputProcessor->AddKeyBinding_Sequence(
		EUMBindingContext::TextEditing,
		{ EKeys::D, EKeys::I, EKeys::W },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::DeleteInsideWord,
		EVimMode::Normal);

	VimInputProcessor->AddKeyBinding_Sequence(
		EUMBindingContext::TextEditing,
		{ EKeys::D, EKeys::K },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::DeleteUpOrDown,
		EVimMode::Normal);

	VimInputProcessor->AddKeyBinding_Sequence(
		EUMBindingContext::TextEditing,
		{ EKeys::D, EKeys::J },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::DeleteUpOrDown,
		EVimMode::Normal);

	// ============================================================================
	// DELETE COMMANDS (d + motion)
	// These bindings handle deletion operations combined with different motions
	// ============================================================================

	// Delete word (dw) - Deletes from cursor to start of next word
	VimInputProcessor->AddKeyBinding_Sequence(
		EUMBindingContext::TextEditing,
		{ FInputChord(EKeys::D), FInputChord(EKeys::W) },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::ActionToMotion,
		EVimMode::Normal);

	// Delete WORD (dW) - Deletes from cursor to start of next WORD (space-delimited)
	VimInputProcessor->AddKeyBinding_Sequence(
		EUMBindingContext::TextEditing,
		{ EKeys::D, FInputChord(EModifierKey::Shift, EKeys::W) },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::ActionToMotion,
		EVimMode::Normal);

	// Delete to end of word (de) - Deletes from cursor to end of current word
	VimInputProcessor->AddKeyBinding_Sequence(
		EUMBindingContext::TextEditing,
		{ EKeys::D, EKeys::E },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::ActionToMotion,
		EVimMode::Normal);

	// Delete to end of WORD (dE) - Deletes from cursor to end of current WORD
	VimInputProcessor->AddKeyBinding_Sequence(
		EUMBindingContext::TextEditing,
		{ EKeys::D, FInputChord(EModifierKey::Shift, EKeys::E) },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::ActionToMotion,
		EVimMode::Normal);

	// Delete backward to start of word (db) - Deletes from cursor back to start of word
	VimInputProcessor->AddKeyBinding_Sequence(
		EUMBindingContext::TextEditing,
		{ EKeys::D, EKeys::B },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::ActionToMotion,
		EVimMode::Normal);

	// Delete backward to start of WORD (dB) - Deletes from cursor back to start of WORD
	VimInputProcessor->AddKeyBinding_Sequence(
		EUMBindingContext::TextEditing,
		{ EKeys::D, FInputChord(EModifierKey::Shift, EKeys::B) },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::ActionToMotion,
		EVimMode::Normal);

	// Delete to end of previous sentence (dge) - Deletes from cursor to end of prev sentence
	VimInputProcessor->AddKeyBinding_Sequence(
		EUMBindingContext::TextEditing,
		{ EKeys::D, EKeys::G, EKeys::E },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::ActionToMotion,
		EVimMode::Normal);

	// Delete to end of previous SENTENCE (dgE) - Deletes from cursor to end of prev SENTENCE
	VimInputProcessor->AddKeyBinding_Sequence(
		EUMBindingContext::TextEditing,
		{ EKeys::D, EKeys::G, FInputChord(EModifierKey::Shift, EKeys::E) },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::ActionToMotion,
		EVimMode::Normal);

	// ============================================================================
	// CHANGE COMMANDS (c + motion)
	// These bindings handle change operations (delete + insert) with different motions
	// ============================================================================

	// Change word (cw) - Changes from cursor to start of next word
	VimInputProcessor->AddKeyBinding_Sequence(
		EUMBindingContext::TextEditing,
		{ FInputChord(EKeys::C), FInputChord(EKeys::W) },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::ActionToMotion,
		EVimMode::Normal);

	// Change WORD (cW) - Changes from cursor to start of next WORD (space-delimited)
	VimInputProcessor->AddKeyBinding_Sequence(
		EUMBindingContext::TextEditing,
		{ EKeys::C, FInputChord(EModifierKey::Shift, EKeys::W) },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::ActionToMotion,
		EVimMode::Normal);

	// Change to end of word (ce) - Changes from cursor to end of current word
	VimInputProcessor->AddKeyBinding_Sequence(
		EUMBindingContext::TextEditing,
		{ EKeys::C, EKeys::E },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::ActionToMotion,
		EVimMode::Normal);

	// Change to end of WORD (cE) - Changes from cursor to end of current WORD
	VimInputProcessor->AddKeyBinding_Sequence(
		EUMBindingContext::TextEditing,
		{ EKeys::C, FInputChord(EModifierKey::Shift, EKeys::E) },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::ActionToMotion,
		EVimMode::Normal);

	// Change backward to start of word (cb) - Changes from cursor back to start of word
	VimInputProcessor->AddKeyBinding_Sequence(
		EUMBindingContext::TextEditing,
		{ EKeys::C, EKeys::B },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::ActionToMotion,
		EVimMode::Normal);

	// Change backward to start of WORD (cB) - Changes from cursor back to start of WORD
	VimInputProcessor->AddKeyBinding_Sequence(
		EUMBindingContext::TextEditing,
		{ EKeys::C, FInputChord(EModifierKey::Shift, EKeys::B) },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::ActionToMotion,
		EVimMode::Normal);

	// Change to end of previous sentence (cge) - Changes from cursor to end of prev sentence
	VimInputProcessor->AddKeyBinding_Sequence(
		EUMBindingContext::TextEditing,
		{ EKeys::C, EKeys::G, EKeys::E },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::ActionToMotion,
		EVimMode::Normal);

	// Change to end of previous SENTENCE (cgE) - Changes from cursor to end of prev SENTENCE
	VimInputProcessor->AddKeyBinding_Sequence(
		EUMBindingContext::TextEditing,
		{ EKeys::C, EKeys::G, FInputChord(EModifierKey::Shift, EKeys::E) },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::ActionToMotion,
		EVimMode::Normal);

	// ============================================================================
	// YANK COMMANDS (y + motion)
	// These bindings handle yank (copy) operations combined with different motions
	// ============================================================================

	// Yank word (yw) - Yanks from cursor to start of next word
	VimInputProcessor->AddKeyBinding_Sequence(
		EUMBindingContext::TextEditing,
		{ FInputChord(EKeys::Y), FInputChord(EKeys::W) },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::ActionToMotion,
		EVimMode::Normal);

	// Yank WORD (yW) - Yanks from cursor to start of next WORD (space-delimited)
	VimInputProcessor->AddKeyBinding_Sequence(
		EUMBindingContext::TextEditing,
		{ EKeys::Y, FInputChord(EModifierKey::Shift, EKeys::W) },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::ActionToMotion,
		EVimMode::Normal);

	// Yank to end of word (ye) - Yanks from cursor to end of current word
	VimInputProcessor->AddKeyBinding_Sequence(
		EUMBindingContext::TextEditing,
		{ EKeys::Y, EKeys::E },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::ActionToMotion,
		EVimMode::Normal);

	// Yank to end of WORD (yE) - Yanks from cursor to end of current WORD
	VimInputProcessor->AddKeyBinding_Sequence(
		EUMBindingContext::TextEditing,
		{ EKeys::Y, FInputChord(EModifierKey::Shift, EKeys::E) },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::ActionToMotion,
		EVimMode::Normal);

	// Yank backward to start of word (yb) - Yanks from cursor back to start of word
	VimInputProcessor->AddKeyBinding_Sequence(
		EUMBindingContext::TextEditing,
		{ EKeys::Y, EKeys::B },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::ActionToMotion,
		EVimMode::Normal);

	// Yank backward to start of WORD (yB) - Yanks from cursor back to start of WORD
	VimInputProcessor->AddKeyBinding_Sequence(
		EUMBindingContext::TextEditing,
		{ EKeys::Y, FInputChord(EModifierKey::Shift, EKeys::B) },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::ActionToMotion,
		EVimMode::Normal);

	// Yank to end of previous sentence (yge) - Yanks from cursor to end of prev sentence
	VimInputProcessor->AddKeyBinding_Sequence(
		EUMBindingContext::TextEditing,
		{ EKeys::Y, EKeys::G, EKeys::E },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::ActionToMotion,
		EVimMode::Normal);

	// Yank to end of previous SENTENCE (ygE) - Yanks from cursor to end of prev SENTENCE
	VimInputProcessor->AddKeyBinding_Sequence(
		EUMBindingContext::TextEditing,
		{ EKeys::Y, EKeys::G, FInputChord(EModifierKey::Shift, EKeys::E) },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::ActionToMotion,
		EVimMode::Normal);

	// Append New Line (After & Before the current Line)
	//
	VimInputProcessor->AddKeyBinding_KeyEvent(
		EUMBindingContext::TextEditing,
		{ EKeys::O },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::AppendNewLine,
		EVimMode::Normal);

	VimInputProcessor->AddKeyBinding_KeyEvent(
		EUMBindingContext::TextEditing,
		{ FInputChord(EModifierKey::Shift, EKeys::O) },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::AppendNewLine,
		EVimMode::Normal);
	//
	// Append New Line (After & Before the current Line)

	//							~ Change ~
	//
	VimInputProcessor->AddKeyBinding_KeyEvent(
		EUMBindingContext::TextEditing,
		{ EKeys::C },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::ChangeVisualMode,
		EVimMode::Visual);

	// Not-Vim-related but I believe helps in some scenarios with the UX
	VimInputProcessor->AddKeyBinding_KeyEvent(
		EUMBindingContext::TextEditing,
		{ EKeys::BackSpace },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::ChangeVisualMode);

	VimInputProcessor->AddKeyBinding_KeyEvent(
		EUMBindingContext::TextEditing,
		{ FInputChord(EModifierKey::Shift, EKeys::C) },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::ChangeVisualMode,
		EVimMode::Visual);

	VimInputProcessor->AddKeyBinding_Sequence(
		EUMBindingContext::TextEditing,
		{ EKeys::C, EKeys::I, EKeys::W },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::ChangeInsideWord,
		EVimMode::Normal);

	VimInputProcessor->AddKeyBinding_KeyEvent(
		EUMBindingContext::TextEditing,
		{ EKeys::C, EKeys::C },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::ChangeEntireLine,
		EVimMode::Normal);

	VimInputProcessor->AddKeyBinding_KeyEvent(
		EUMBindingContext::TextEditing,
		{ FInputChord(EModifierKey::Shift, EKeys::C) },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::ChangeToEndOfLine,
		EVimMode::Normal);

	VimInputProcessor->AddKeyBinding_KeyEvent(
		EUMBindingContext::TextEditing,
		{ EKeys::C, FInputChord(EModifierKey::Shift, EKeys::Four /*Shift+$*/) },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::ChangeToEndOfLine,
		EVimMode::Normal);
	//
	//							~ Change ~

	// Add debugging text (content) to the current editable
	VimInputProcessor->AddKeyBinding_KeyEvent(
		EUMBindingContext::TextEditing,
		{ EKeys::SpaceBar, EKeys::A, EKeys::D },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::AddDebuggingText);

	VimInputProcessor->AddKeyBinding_KeyEvent(
		EUMBindingContext::TextEditing,
		{ FInputChord(EModifierKey::Control, EKeys::A) },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::VimCommandSelectAll);

	VimInputProcessor->AddKeyBinding_Sequence(
		EUMBindingContext::TextEditing,
		{ EKeys::W }, // 'w' for forward (small word)
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::NavigateW);

	VimInputProcessor->AddKeyBinding_Sequence(
		EUMBindingContext::TextEditing,
		{ FInputChord(EModifierKey::Shift, EKeys::W) }, // 'W' for forward (big word)
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::NavigateBigW);

	VimInputProcessor->AddKeyBinding_Sequence(
		EUMBindingContext::TextEditing,
		{ EKeys::B }, // 'b' for backward (small word)
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::NavigateB);

	VimInputProcessor->AddKeyBinding_Sequence(
		EUMBindingContext::TextEditing,
		{ FInputChord(EModifierKey::Shift, EKeys::B) }, // 'B' for backward (big word)
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::NavigateBigB);

	// E for next small word end
	VimInputProcessor->AddKeyBinding_Sequence(
		EUMBindingContext::TextEditing,
		{ EKeys::E },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::NavigateE);

	// E for next big word end
	VimInputProcessor->AddKeyBinding_Sequence(
		EUMBindingContext::TextEditing,
		{ FInputChord(EModifierKey::Shift, EKeys::E) },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::NavigateBigE);

	// ge for previous small word end
	VimInputProcessor->AddKeyBinding_Sequence(
		EUMBindingContext::TextEditing,
		{ EKeys::G, EKeys::E },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::NavigateGE);

	// gE for previous big word end
	VimInputProcessor->AddKeyBinding_Sequence(
		EUMBindingContext::TextEditing,
		{ EKeys::G, FInputChord(EModifierKey::Shift, EKeys::E) },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::NavigateGBigE);

	// Yanking / Pasting Related
	//
	VimInputProcessor->AddKeyBinding_Sequence(
		EUMBindingContext::TextEditing,
		{ EKeys::Y, EKeys::Y },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::YankLine,
		EVimMode::Normal /* Only available in Normal Mode */);

	VimInputProcessor->AddKeyBinding_Sequence(
		EUMBindingContext::TextEditing,
		{ EKeys::Y },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::YankCharacter,
		EVimMode::Visual /* Only available in Visual Mode */);

	VimInputProcessor->AddKeyBinding_Sequence(
		EUMBindingContext::TextEditing,
		{ EKeys::P },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::Paste);

	VimInputProcessor->AddKeyBinding_Sequence(
		EUMBindingContext::TextEditing,
		{ FInputChord(EModifierKey::Shift, EKeys::P) },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::Paste);

	VimInputProcessor->AddKeyBinding_Sequence(
		EUMBindingContext::TextEditing,
		{ EKeys::Y, EKeys::I, EKeys::W },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::YankInsideWord,
		EVimMode::Normal);

	//
	// Yanking / Pasting Related

	VimInputProcessor->AddKeyBinding_NoParam(
		EUMBindingContext::TextEditing,
		// Not V + I + W because it will collide with entering Visual Mode +
		// we're already restricted to visual mode for this so it's ok.
		{ EKeys::I, EKeys::W },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::SelectInsideWord,
		EVimMode::Visual);

	// Jump to Start of Line
	VimInputProcessor->AddKeyBinding_KeyEvent(
		EUMBindingContext::TextEditing,
		{ EKeys::Zero },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::JumpToStartOfLine);

	// TODO: Caret GoTo first char in line
	// Should be very similar to JumpToStart just with checking if we've reached
	// a whitespace or something in that nature?

	// Jump to End of Line
	VimInputProcessor->AddKeyBinding_KeyEvent(
		EUMBindingContext::TextEditing,
		{ FInputChord(EModifierKey::Shift, EKeys::Four) /* i.e. $ */ },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::JumpToEndOfLine);

	VimInputProcessor->AddKeyBinding_Sequence(
		EUMBindingContext::TextEditing,
		{ EKeys::R },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::ReplaceCharacter,
		EVimMode::Normal);

	VimInputProcessor->AddKeyBinding_KeyEvent(
		EUMBindingContext::TextEditing,
		{ EKeys::F },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::BeginFindChar);

	VimInputProcessor->AddKeyBinding_KeyEvent(
		EUMBindingContext::TextEditing,
		{ FInputChord(EModifierKey::Shift, EKeys::F) },
		WeakTextSubsystem,
		&UVimTextEditorSubsystem::BeginFindChar);
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

/**
 * Known (unsolved) issues:
 * 1) Given the following rows of text:
 * "I'm a short tex(t)"
 * "I'm actually a longer t(e)xt"
 * When trying to navigate between the (e) surrounded with parenthesis to the (t)
 * above it, in Vim, when we try to go down again we expect our cursor to
 * highlight (e), but without some custom tracking, in Unreal this is not what
 * we'll get. The cursor will highlight the closest bottom character instead of
 * intelligently knowing from where we came.
 *
 * 2) Trying to simulate BackSpace does not seem to work. In order to 'delete'
 * stuff it seems like only simulating the 'Delete' key works.
 *
 * 3) In Insert mode, the normal UE behavior is to select all text on enter
 * (rather than to go down a line).
 * I don't really want to intercept with the native insert mode stuff. But maybe
 * we should? It will definitely be a better UX (in my opinion). Maybe this
 * should be an option. The implementation of this isn't that obvious though.
 * So need to see...
 *
 * 4) The native MultiLineEditable widget does have a select text in range method
 * but we have our own version that is concerned with some Vim specific selection
 * rules and leverages some custom functionality that selects text in a more
 * reliable way (that is concerned with Vim specfics).
 *
 * 5) After inserting some characters to a MultiLine Editable Text, it looks like
 * we have to write (save) to get a correct representation of the current input?
 * (to navigate properly) For now, we're manually setting the text to enforce
 * the most recent edits with itself for proper navigation.
 */

// TODOS:
// 1. Can't go down in visual mode from the first line in MultLine Editables
// when the same line goes down inside itself:
/*
 * Example Input:
TEST TTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTT
Test)
*/
// 2. Trying to F select a node pin from an editable doesn't work?
// ciw motion doesn't work in the first character
// 3. Sometimes text won't retain it's final most updated form after typing some
// input and switching between Vim modes.
