#include "VimTextEditorSubsystem.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/InvalidateWidgetReason.h"
#include "UMInputHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogVimTextEditorSubsystem, Log, All);

void UVimTextEditorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Logger.SetLogCategory(&LogVimTextEditorSubsystem);

	InputPP = FUMInputPreProcessor::Get().ToWeakPtr();
	BindCommands();
	Super::Initialize(Collection);

	FCoreDelegates::OnPostEngineInit
		.AddUObject(this, &UVimTextEditorSubsystem::RegisterSlateEvents);
}

void UVimTextEditorSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UVimTextEditorSubsystem::BindCommands()
{
	using VimTextSub = UVimTextEditorSubsystem;
	if (TSharedPtr<FUMInputPreProcessor> Input = InputPP.Pin())
	{
		Input->OnVimModeChanged.AddUObject(
			this, &VimTextSub::OnVimModeChanged);
	}
}

void UVimTextEditorSubsystem::OnVimModeChanged(const EVimMode NewVimMode)
{
	EVimMode PreviousMode = CurrentVimMode;
	CurrentVimMode = NewVimMode;

	FSlateApplication&		  SlateApp = FSlateApplication::Get();
	const TSharedPtr<SWidget> FocusedWidget = SlateApp.GetUserFocusedWidget(0);

	switch (CurrentVimMode)
	{
		case EVimMode::Insert:
		{
			bShouldReadOnlyText = false;
			ToggleReadOnly(bIsActiveEditableMultiLine, false);
			break;
		}
		case EVimMode::Normal:
		case EVimMode::Visual:
		{
			bShouldReadOnlyText = true;
			ToggleReadOnly(bIsActiveEditableMultiLine, true);
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

	if (NewWidget.IsValid())
	{
		if (NewWidget->GetType().IsEqual(EditableTextType))
		{
			ActiveEditableGeneric = NewWidget; // Track this widget for later use
			bIsActiveEditableMultiLine = false;
			//
			TSharedPtr<SEditableText> EditableText =
				StaticCastSharedPtr<SEditableText>(NewWidget);
			ActiveEditableText = EditableText;

			Logger.Print("VimTextEditor: SEditableText Found", ELogVerbosity::Verbose);
		}
		else if (NewWidget->GetType().IsEqual(MultiEditableTextType))
		{
			ActiveEditableGeneric = NewWidget; // Track this widget for later use
			bIsActiveEditableMultiLine = true;

			TSharedPtr<SMultiLineEditableText> MultiEditableText =
				StaticCastSharedPtr<SMultiLineEditableText>(NewWidget);
			ActiveMultiLineEditableText = MultiEditableText;

			Logger.Print("VimTextEditor: SMultiLineEditableText Found", ELogVerbosity::Verbose);
		}
		else
		{
			// Logger.Print("Could not find Editable or Multi Text"
			// 		+ NewWidget->GetWidgetClass().GetWidgetType().ToString(),
			// 	ELogVerbosity::Warning, true);
			return;
		}

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

		if (bIsActiveEditableMultiLine)
		{
			// if (Parent->GetType().IsEqual(MultiEditableTextBoxType))
			if (Parent->GetWidgetClass().GetWidgetType().IsEqual("SBorder"))
			{
				TSharedPtr<SMultiLineEditableTextBox> MultiTextBox =
					StaticCastSharedPtr<SMultiLineEditableTextBox>(Parent);
				ActiveMultiLineEditableTextBox = MultiTextBox;
			}
		}
		else // Single Editable Text
		{
			// if (Parent->GetType().IsEqual(EditableTextBoxType))
			if (Parent->GetWidgetClass().GetWidgetType().IsEqual("SBorder"))
			{
				TSharedPtr<SEditableTextBox> TextBox =
					StaticCastSharedPtr<SEditableTextBox>(Parent);
				ActiveEditableTextBox = TextBox;
			}
		}
		ToggleReadOnly(bIsActiveEditableMultiLine, bShouldReadOnlyText);
		// Logger.Print(Parent->GetWidgetClass().GetWidgetType().ToString(),
		// ELogVerbosity::Verbose, true);
		return;
	}
	else
	{
		Logger.Print("OnFocusedChanged: Widget Focused is Invalid",
			ELogVerbosity::Error, true);
	}
}

void UVimTextEditorSubsystem::ToggleReadOnly(bool bIsMultiLine, bool bIsReadOnly)
{
	if (bIsMultiLine)
	{
		if (const TSharedPtr<SMultiLineEditableTextBox> MultiTextBox =
				ActiveMultiLineEditableTextBox.Pin())
		{
			MultiTextBox->SetIsReadOnly(bIsReadOnly);
			MultiTextBox->SetTextBoxBackgroundColor(
				FSlateColor(FLinearColor::Black));
			MultiTextBox->SetReadOnlyForegroundColor(
				FSlateColor(FLinearColor::White));

			// NOTE:
			// We set this dummy goto to refresh the cursor blinking to ON / OFF
			MultiTextBox->GoTo(MultiTextBox->GetCursorLocation());
		}
	}
	else
	{
		if (const TSharedPtr<SEditableTextBox> TextBox =
				ActiveEditableTextBox.Pin())
		{
			TextBox->SetIsReadOnly(bIsReadOnly);
			TextBox->SetTextBoxBackgroundColor(
				FSlateColor(FLinearColor::Black));
			TextBox->SetReadOnlyForegroundColor(
				FSlateColor(FLinearColor::White));

			// TextBox->OnNavigation();

			// TextBox->SetTextBlockStyle();
			// TextBox->SetStyle();

			// Won't influence the "Active" blue outline look
			// FSlateApplication::Get().SetKeyboardFocus(TextBox, EFocusCause::Navigation);

			if (const auto EditTextBox = ActiveEditableText.Pin())
			{
				FNavigationEvent NavEvent;
				FKeyEvent		 InKeyEvent(FKey(EKeys::Right), FModifierKeysState(), 0, 0, 0, 0);
				FUMInputHelpers::GetNavigationEventFromVimKey(
					InKeyEvent, NavEvent, true);
				// EditTextBox->OnNavigation(EditTextBox->GetCachedGeometry(), NavEvent);
				// EditTextBox->OnNavigation(EditTextBox->GetCachedGeometry(), NavEvent);
				// EditTextBox->OnNavigation(EditTextBox->GetCachedGeometry(), NavEvent);

				// EditTextBox->OnPreviewKeyDown(EditTextBox->GetCachedGeometry(), InKeyEvent);
				// EditTextBox->OnPreviewKeyDown(EditTextBox->GetCachedGeometry(), InKeyEvent);
				// EditTextBox->OnPreviewKeyDown(EditTextBox->GetCachedGeometry(), InKeyEvent);
				// Most of the down and up are protected for the non Box
			}
			FKeyEvent InKeyEvent(
				FKey(EKeys::Right), FModifierKeysState(), 0, 0, 0, 0);
			// TextBox->OnPreviewKeyDown(TextBox->GetCachedGeometry(), InKeyEvent);
			// TextBox->OnKeyDown(TextBox->GetCachedGeometry(), InKeyEvent);
			// TextBox->OnKeyUp(TextBox->GetCachedGeometry(), InKeyEvent);
			// TextBox->OnFinishedKeyInput();
			// TextBox->OnKeyDown(TextBox->GetCachedGeometry(), InKeyEvent);
			// TextBox->OnKeyUp(TextBox->GetCachedGeometry(), InKeyEvent);
			// TextBox->OnFinishedKeyInput();

			// NOTE:
			// Unlike the MultiLineEditable we don't have GetCursorLocation
			// so we can't really know where we were for the GoTo() *~*
			// So the hack I found to mitigate the blinking vs. non-blinking
			// cursor not changing upon turnning IsReadOnly ON / OFF is to just
			// BeginSearch with an empty FText (LOL).
			// This seems to keep the cursor in the same location while fixing
			// aligning the blinking cursor with the current mode <3
			// TextBox->BeginSearch(FText::GetEmpty());
		}
	}
}
