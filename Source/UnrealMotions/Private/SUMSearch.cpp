#include "SUMSearch.h"
#include "Framework/Application/SlateApplication.h"
#include "Editor.h"
#include "Input/Events.h"
#include "Internationalization/Regex.h"
#include "UMSlateHelpers.h"
// #include "Widgets/Input/SButton.h"
#include "VimInputProcessor.h"

DEFINE_LOG_CATEGORY_STATIC(LogUMSearch, Log, All); // Dev

void SUMSearch::Construct(const FArguments& InArgs)
{
	Logger = FUMLogger(&LogUMSearch);
	bCanSupportFocus = true;

	ChildSlot
		[SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
				.Padding(FMargin(4))
					[SAssignNew(SearchBox, SSearchBox)
							.HintText(NSLOCTEXT("UMSearch", "SearchHint", "Search..."))
							.OnTextChanged(this, &SUMSearch::OnSearchBoxTextChanged)
							.OnTextCommitted(this, &SUMSearch::OnSearchBoxTextCommitted)
							.OnKeyDownHandler(this, &SUMSearch::OnSearchBoxKeyDown)]];

	// SearchBox->SetOnKeyCharHandler(FOnKeyChar::CreateRaw(this, &SUMSearch::OnKeyChar));
	SearchBox->SetOnKeyCharHandler(FOnKeyChar::CreateSP(this, &SUMSearch::OnKeyChar));
}

void SUMSearch::Open()
{
	FSlateApplication& SlateApp = FSlateApplication::Get();
	// const TSharedPtr<SWindow> ActiveWindow = SlateApp.GetActiveTopLevelRegularWindow();
	// if (!ActiveWindow.IsValid())
	// 	return;

	TSharedPtr<SWindow> Window =
		SNew(SWindow)
			.Title(FText::FromString(TEXT("Search")))
			.AutoCenter(EAutoCenter::PreferredWorkArea)
			.ClientSize(FVector2D(300, 40))
			.SizingRule(ESizingRule::FixedSize)
			.IsPopupWindow(true)
			.FocusWhenFirstShown(true);

	TSharedRef<SUMSearch> Search = SNew(SUMSearch);
	if (!Search->GetAllTextBlocksInActiveTab())
		return;

	// Optional (WIP)
	Search->GetAllEditableTextsInActiveTab();

	Window->SetContent(Search);

	// Not really sure what's the deal with the modals. Going for
	// regular right now. Modals seems to behave really weird.
	// SlateApp.AddModalWindow(Window.ToSharedRef(), ActiveWindow, true);
	SlateApp.AddWindow(Window.ToSharedRef(), true);

	// Simply trying to focus on the SearchBox doesn't seem to be enough.
	// So have to go with the traverse and delay approach
	TSharedPtr<SWidget> FoundEditable;
	if (!FUMSlateHelpers::TraverseWidgetTree(
			Search, FoundEditable, "SEditableText"))
		return;

	// Not working
	// Window->SetWidgetToFocusOnActivate(FoundEditable);
	// FWindowDrawAttentionParameters AttentionParams(
	// 	EWindowDrawAttentionRequestType::UntilActivated);
	// Window->DrawAttention(AttentionParams);
	// Window->BringToFront(true);

	FTimerHandle TimerHandle;
	FUMSlateHelpers::SetWidgetFocusWithDelay(FoundEditable.ToSharedRef(), TimerHandle, 0.025f, false);
}

void SUMSearch::Close()
{
	FSlateApplication& SlateApp = FSlateApplication::Get();
	FVimInputProcessor::Get()->SetVimMode(SlateApp, EVimMode::Normal, 0.05f);

	if (SearchBox.IsValid())
	{
		TSharedPtr<SWindow> Window = SlateApp.FindWidgetWindow(AsShared());
		if (Window.IsValid())
			Window->RequestDestroyWindow();
	}
}

TSharedPtr<SSearchBox> SUMSearch::GetTextBox()
{
	return SearchBox;
}

bool SUMSearch::GetAllTextBlocksInActiveTab()
{
	const TSharedPtr<SDockTab> ActiveTab = FUMSlateHelpers::GetActiveMajorTab();
	if (!ActiveTab.IsValid())
		return false;

	const FString TextBlockType = "STextBlock";

	TArray<TSharedPtr<SWidget>> FoundTextBlocks;
	if (!FUMSlateHelpers::TraverseWidgetTree(
			ActiveTab->GetContent(), FoundTextBlocks, TextBlockType))
		return false;

	for (const auto& Text : FoundTextBlocks)
	{
		const TSharedPtr<STextBlock> TextBlock =
			StaticCastSharedPtr<STextBlock>(Text);

		TextBlocksByString.Add(TextBlock->GetText().ToString(), TextBlock);
	}

	return true;
}

bool SUMSearch::GetAllEditableTextsInActiveTab()
{
	const TSharedPtr<SDockTab> ActiveTab = FUMSlateHelpers::GetActiveMajorTab();
	if (!ActiveTab.IsValid())
		return false;

	const FString EditableType = "SEditableText";

	TArray<TSharedPtr<SWidget>> FoundEditableTexts;
	if (FUMSlateHelpers::TraverseWidgetTree(
			ActiveTab->GetContent(), FoundEditableTexts, EditableType))
	{
		for (const auto& Editable : FoundEditableTexts)
		{
			const TSharedPtr<SEditableText> EditableText =
				StaticCastSharedPtr<SEditableText>(Editable);

			EditableTextsByString.Add(EditableText->GetText().ToString(), EditableText);
		}
		return true;
	}
	return false;
}

// NOTE:
// Where doing some manual text processing because OnTextChanged is a very slow
// delegate. To get a realtime feel, we're processing some stuff via the KeyDown
// & CharDown events. We're still relying on OnTextChanged to give as the final
// most-true representation of the text as a fallback.
// The main limitation with OnSearchBoxKeyDown deletion is that we don't know
// the position (index) of the cursor, thus we can delete incorrect chars.
// We can potentially mitigate that with some fancy selection mechanism (simulate
// select to end or start and dechiper what is our current cursor pos).

/** Handling deletion and exiting from the widget */
FReply SUMSearch::OnSearchBoxKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey& InKey = InKeyEvent.GetKey();

	if (InKey == EKeys::BackSpace)
	{
		ProcessBackSpaceOnKeyDown();
		VisualizeMatchedTargets();
		return FReply::Handled();
	}
	else if (InKey == EKeys::Escape)
	{
		Close();
		return FReply::Handled();
	}

	// Logger.Print(FString::Printf(TEXT("OnKeyDown | InText: %s"), *SearchText),
	// 	ELogVerbosity::Log, true);
	return FReply::Unhandled();
}

void SUMSearch::ProcessBackSpaceOnKeyDown()
{
	// Get the currently selected text
	if (SearchBox->AnyTextSelected())
	{
		const FString SelText = SearchBox->GetSelectedText().ToString();

		// Remove the selected text from SearchText
		const int32 SelStart =
			SearchText.Find(SelText, ESearchCase::CaseSensitive);

		if (SelStart != INDEX_NONE)
		{
			// Remove the selected text from SearchText
			const int32 SelLength = SelText.Len();
			SearchText.RemoveAt(SelStart, SelLength);
		}
	}
	else
	{
		// TODO:
		// This isn't a great, and will be incorrect if the cursor isn't at the
		// very end of the line. Should work on that and determine the cursor pos.
		SearchText = SearchText.LeftChop(1);
	}
}

/** Getting the proper representaition of the Character Event */
FReply SUMSearch::OnKeyChar(const FGeometry& MyGeometry, const FCharacterEvent& InCharacterEvent)
{
	const TCHAR& TypedChar = InCharacterEvent.GetCharacter();

	if (!FChar::IsPrint(TypedChar))
		return FReply::Unhandled(); // Ignore unprintable characters

	SearchText += TypedChar;
	VisualizeMatchedTargets();

	Logger.Print(FString::Printf(TEXT("OnKeyChar | InText: %s"), *SearchText),
		ELogVerbosity::Log, true);

	return FReply::Unhandled();
}

/** Slow, but useful for getting the valid and reliable text representation */
void SUMSearch::OnSearchBoxTextChanged(const FText& InText)
{
	Logger.Print("On Text Changed!", ELogVerbosity::Log, true);
	SearchText = InText.ToString();
	VisualizeMatchedTargets();

	// VisualizeMatchedTargets(InText);
}

void SUMSearch::OnSearchBoxTextCommitted(const FText& InText, ETextCommit::Type CommitType)
{
	if (CommitType == ETextCommit::OnEnter)
	{
		FocusFirstFoundMatch();
		Close();
	}
}

// Doesn't seem to ever trigger
void SUMSearch::OnFocusLost(const FFocusEvent& InFocusEvent)
{
	// Logger.Print("On Focus Lost!", ELogVerbosity::Warning, true);
	Close();
}

void SUMSearch::OnFocusChanging(const FWeakWidgetPath& PreviousFocusPath, const FWidgetPath& NewWidgetPath, const FFocusEvent& InFocusEvent)
{
	// Logger.Print("On Focus Changing!", ELogVerbosity::Warning, true);

	// We need the delay to actually evaluate if still has focus
	TWeakPtr<SWidget> WeakSearch = AsShared();
	FTimerHandle	  TimerHandle;
	GEditor->GetTimerManager()->SetTimer(
		TimerHandle,
		[this, WeakSearch]() {
			if (const TSharedPtr<SWidget> Search = WeakSearch.Pin())
				if (!Search->HasFocusedDescendants())
					Close();
		},
		0.025f, false);
}

void SUMSearch::VisualizeMatchedTargets()
{
	FRegexPattern Pattern(SearchText);
	VisualizeMatchedTextBlocks(Pattern);
	VisualizeMatchedEditableTexts(Pattern);
}

// Temp setup, we should have more generic functions, this is repetative
void SUMSearch::VisualizeMatchedTextBlocks(const FRegexPattern& Pattern)
{
	for (const auto& KV : TextBlocksByString)
	{
		const FString& Candidate = KV.Key;
		FRegexMatcher  Matcher(Pattern, Candidate);

		if (const TSharedPtr<STextBlock> TextBlock = KV.Value.Pin())
		{
			if (Matcher.FindNext())
			{
				Logger.Print(FString::Printf(TEXT("Match found in: %s"), *Candidate), ELogVerbosity::Verbose, true);

				TextBlock->SetHighlightText(FText::FromString(SearchText));
			}
			else
			{
				TextBlock->SetHighlightText(FText::GetEmpty());
			}
		}
	}
}

// Temp setup, we should have more generic functions, this is repetative
void SUMSearch::VisualizeMatchedEditableTexts(const FRegexPattern& Pattern)
{
	for (const auto& KV : EditableTextsByString)
	{
		const FString& Candidate = KV.Key;
		FRegexMatcher  Matcher(Pattern, Candidate);

		if (const TSharedPtr<SEditableText> EditableText = KV.Value.Pin())
		{
			if (Matcher.FindNext())
			{
				Logger.Print(FString::Printf(TEXT("Match found in: %s"), *Candidate), ELogVerbosity::Verbose, true);

				EditableText->SelectAllText();
			}
			else
			{
				EditableText->ClearSelection();
			}
		}
	}
}

void SUMSearch::FocusFirstFoundMatch()
{
	FRegexPattern Pattern(SearchText);

	if (FocusFirstFoundTextBlock(Pattern))
		return;

	FocusFirstFoundEditableText(Pattern);
}

bool SUMSearch::FocusFirstFoundTextBlock(const FRegexPattern& Pattern)
{
	for (const auto& KV : TextBlocksByString)
	{
		const FString& Candidate = KV.Key;
		FRegexMatcher  Matcher(Pattern, Candidate);

		const TSharedPtr<STextBlock> TextBlock = KV.Value.Pin();
		if (!TextBlock.IsValid())
			continue;

		if (Matcher.FindNext())
		{
			Logger.Print(FString::Printf(TEXT("Match found in: %s"), *Candidate), ELogVerbosity::Verbose, true);

			// This for example solves the Output Log focus issue.
			// if (const TSharedPtr<SWidget> Button =
			// 		FUMSlateHelpers::FindNearstWidgetType(
			// 			TextBlock.ToSharedRef(), "SButton"))
			// {
			// 	const TSharedPtr<SButton> AsButton =
			// 		StaticCastSharedPtr<SButton>(Button);

			// 	AsButton->SimulateClick();
			// 	return;
			// }

			// Maybe?
			// FUMInputHelpers::SimulateClickOnWidget(
			// 	FSlateApplication::Get(),
			// 	TextBlock.ToSharedRef(), FKey(EKeys::LeftMouseButton));
			// return;

			// FUMSlateHelpers::DebugClimbUpFromWidget(TextBlock.ToSharedRef());

			// This can be better to have proper handling for different types:
			// SButton should be casted and click simulated, DockTab should be
			// invoked, etc.

			TextBlock->SetHighlightText(FText::FromString(SearchText));
			if (FUMSlateHelpers::FocusNearestInteractableWidget(TextBlock.ToSharedRef()))
			{
				return true;
			}
		}
		else
		{
			TextBlock->SetHighlightText(FText::GetEmpty());
		}
	}
	return false;
}

bool SUMSearch::FocusFirstFoundEditableText(const FRegexPattern& Pattern)
{
	for (const auto& KV : EditableTextsByString)
	{
		const FString& Candidate = KV.Key;
		FRegexMatcher  Matcher(Pattern, Candidate);

		const TSharedPtr<SEditableText> EditableText = KV.Value.Pin();
		if (!EditableText.IsValid())
			continue;

		if (Matcher.FindNext())
		{
			Logger.Print(FString::Printf(TEXT("Match found in: %s"), *Candidate), ELogVerbosity::Verbose, true);

			FTimerHandle TimerHandle;
			FUMSlateHelpers::SetWidgetFocusWithDelay(EditableText.ToSharedRef(),
				TimerHandle, 0.025f, true);
			return true;
		}
		else
		{
			EditableText->ClearSelection();
		}
	}
	return false;
}
