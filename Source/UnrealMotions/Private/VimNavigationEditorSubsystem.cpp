#include "VimNavigationEditorSubsystem.h"
#include "Framework/Docking/TabManager.h"
#include "Misc/StringFormatArg.h"
#include "Types/SlateEnums.h"
#include "UMInputHelpers.h"
#include "UMSlateHelpers.h"
#include "VimInputProcessor.h"
#include "Widgets/Docking/SDockTab.h"
#include "UMFocusVisualizer.h"
#include "UMConfig.h"
#include "SUMHintMarker.h"
#include "SUMHintOverlay.h"

DEFINE_LOG_CATEGORY_STATIC(LogVimNavigationEditorSubsystem, Log, All); // Dev

bool UVimNavigationEditorSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return FUMConfig::Get()->IsVimEnabled();
}

/**
 */
void UVimNavigationEditorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Logger = FUMLogger(&LogVimNavigationEditorSubsystem);

	BindVimCommands();

	Super::Initialize(Collection);
}

void UVimNavigationEditorSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UVimNavigationEditorSubsystem::NavigatePanelTabs(
	FSlateApplication& SlateApp, const FKeyEvent& InKey)
{
	// Get the desired navigation from the in Vim Key
	EUINavigation NavDirection;
	if (!FUMInputHelpers::GetUINavigationFromVimKey(InKey.GetKey(), NavDirection))
		return;

	FKey ArrowKey;
	if (!FUMInputHelpers::GetArrowKeyFromVimKey(InKey.GetKey(), ArrowKey))
		return;

	// Window to operate on: keep an eye of this. This should be aligned with
	// the currently focused window.
	const TSharedPtr<SWindow> Win = SlateApp.GetActiveTopLevelRegularWindow();
	if (!Win.IsValid())
		return;

	const TSharedRef<FGlobalTabmanager> GTM = FGlobalTabmanager::Get();
	const TSharedPtr<SDockTab>			ActiveMinorTab = GTM->GetActiveTab();
	if (!ActiveMinorTab.IsValid())
		return;
	const TSharedRef<SDockTab> MinorTabRef = ActiveMinorTab.ToSharedRef();

	// Getting the parent window of the minor tab directly seems to always
	// return an invalid parent window so fetching the window from SlateApp.
	// Anyway, this is our way to verify we only proceed actually focused
	// panel tabs, as Unreal official "Active Tab" tracking can be misaligned
	// with Nomad Tabs. We may need to actually check our Major Tab first and see
	// if it's a Nomad Tab to not even go this route.
	if (!FUMSlateHelpers::DoesTabResideInWindow(Win.ToSharedRef(), MinorTabRef))
		return;
	// Nomad tabs handling?

	TWeakPtr<SWidget> DockingTabStack;
	if (FUMSlateHelpers::GetParentDockingTabStackAsWidget(MinorTabRef, DockingTabStack))
	{
		const auto& PinDockingTab = DockingTabStack.Pin();
		if (!PinDockingTab.IsValid())
			return;

		// Bring focus to the Docking Tab Stack to achieve the built-in
		// panel navigation when it is focused.
		SlateApp.SetAllUserFocus(PinDockingTab, EFocusCause::Navigation);

		// For some reason the Editor Viewport wants arrows *~* try to mitigate
		if (ActiveMinorTab->GetLayoutIdentifier().ToString().Equals("LevelEditorViewport"))
		{
			FVimInputProcessor::SimulateKeyPress(SlateApp, ArrowKey);
		}
		else // Others does not want arrows! *~*
		{
			// Create a widget path for the active widget
			FWidgetPath WidgetPath;
			if (SlateApp.FindPathToWidget(
					// CurrMinorTab.ToSharedRef(), WidgetPath))
					PinDockingTab.ToSharedRef(), WidgetPath))
			{
				// Craft and process the event
				FReply Reply =
					FReply::Handled().SetNavigation(
						NavDirection,
						ENavigationGenesis::Keyboard,
						ENavigationSource::FocusedWidget);

				FSlateApplication::Get().ProcessReply(
					WidgetPath, // CurrentEventPath
					Reply,		// TheReply
					nullptr,	// WidgetsUnderMouse (not needed here)
					nullptr,	// InMouseEvent (optional)
					0			// UserIndex
				);
			}
		}
	}
}

void UVimNavigationEditorSubsystem::FlashHintMarkers(
	FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	const TSharedPtr<SWindow> ActiveWindow = SlateApp.GetActiveTopLevelRegularWindow();
	if (!ActiveWindow.IsValid())
		return;

	TArray<TSharedPtr<SWidget>> InteractableWidgets;
	if (!CollectInteractableWidgets(InteractableWidgets))
		return;

	const int32 WidgetsNum = InteractableWidgets.Num();

	// Create Hint Marker Labels
	// FString AllowedCharacters = TEXT("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
	// More Options to try: "fghjklrueidcvm" or "asdfgqwertzxcvb"
	// TODO: There are duplicate markers (i.e. A, AA) need to be fixed.
	const FString	DefaultChars = TEXT("AFGHJKLRUEIDC");
	TArray<FString> Labels;
	GenerateLabels(WidgetsNum, DefaultChars, Labels);

	// Create the Hint Marker Widgets
	TArray<TSharedRef<SUMHintMarker>> HintMarkers;
	for (int32 i = 0; i < WidgetsNum; ++i)
	{
		const TSharedRef<SWidget> WidgetRef = InteractableWidgets[i].ToSharedRef();
		TSharedRef<SUMHintMarker> HintMarker =
			SNew(SUMHintMarker)
				.TargetWidget(WidgetRef)
				.InWidgetLocalPosition(FUMSlateHelpers::GetWidgetLocalPositionInWindow(WidgetRef, ActiveWindow.ToSharedRef()))
				.MarkerText(FText::FromString(Labels[i]))
				.BackgroundColor(FSlateColor(FLinearColor::Yellow)) // Wrap color
				.TextColor(FSlateColor(FLinearColor::Black));

		HintMarkers.Add(HintMarker);
		// Logger.Print(Labels[i]);
	}

	// Add all Hint Markers to the screen
	TSharedRef<SUMHintOverlay> HintOverlay = SNew(SUMHintOverlay, MoveTemp(HintMarkers));
	// TSharedRef<SUMHintOverlay> HintOverlay = SNew(SUMHintOverlay, HintMarkers);

	ActiveWindow->AddOverlaySlot(999)[HintOverlay];

	HintOverlayData = FHintOverlayData(HintOverlay, ActiveWindow);

	Logger.Print(FString::Printf(TEXT("Created %d Hint Markers!"), WidgetsNum), ELogVerbosity::Verbose, true);
}

bool UVimNavigationEditorSubsystem::CollectInteractableWidgets(
	TArray<TSharedPtr<SWidget>>& OutWidgets)
{
	// Get & validate the currently focused widget (from which we will traverse)
	FSlateApplication&		  SlateApp = FSlateApplication::Get();
	const TSharedPtr<SWidget> FocusedWidget = SlateApp.GetUserFocusedWidget(0);
	if (!FocusedWidget.IsValid())
		return false;

	// return FUMSlateHelpers::TraverseFindWidgetUpwards(FocusedWidget.ToSharedRef(),
	// 	OutWidgets, FUMSlateHelpers::GetInteractableWidgetTypes(),
	// 	true /* Traverse down once before up */);

	// TEST
	const TSharedPtr<SWindow> ActiveWindow = SlateApp.GetActiveTopLevelRegularWindow();
	if (!ActiveWindow.IsValid())
		return false;

	return FUMSlateHelpers::TraverseFindWidget(ActiveWindow.ToSharedRef(),
		OutWidgets, FUMSlateHelpers::GetInteractableWidgetTypes());
}

void UVimNavigationEditorSubsystem::GenerateLabels(
	int32			 NumLabels,
	const FString&	 AllowedCharacters,
	TArray<FString>& OutLabels)
{
	const FString DefaultVimiumCharacters = TEXT("fghjklrueidc");
	OutLabels.Reset();
	OutLabels.Reserve(NumLabels);

	// Decide which set of characters to use -- default if none provided.
	FString Alphabet = AllowedCharacters;
	if (Alphabet.IsEmpty())
	{
		Alphabet = DefaultVimiumCharacters;
	}

	const int32 Base = Alphabet.Len();
	if (Base == 0)
	{
		// You could log an error or just return.
		UE_LOG(LogTemp, Warning, TEXT("AllowedCharacters is empty. Cannot generate labels."));
		return;
	}

	// Generate each label
	for (int32 i = 0; i < NumLabels; ++i)
	{
		// We’ll do "Excel columns" style using 1-based index.
		int32	CurrentIndex = i + 1;
		FString Label;

		do
		{
			// Subtract 1 so that:
			//    1 -> remainder 0
			//    2 -> remainder 1
			//    ...
			//    Base -> remainder (Base - 1)
			CurrentIndex -= 1;
			const int32 Remainder = CurrentIndex % Base;
			const TCHAR Char = Alphabet[Remainder];

			// Prepend this character to our label
			Label.InsertAt(0, Char);

			// Move to the next "digit"
			CurrentIndex /= Base;
		}
		while (CurrentIndex > 0);

		OutLabels.Add(Label);
	}
}

void UVimNavigationEditorSubsystem::BuildHintTrie(
	const TArray<FString>&					 HintLabels,
	const TArray<TSharedPtr<SUMHintMarker>>& HintMarkers)
{
	RootHintNode = new FUMHintWidgetTrieNode();

	for (int32 i = 0; i < HintLabels.Num(); ++i)
	{
		CurrentHintNode = RootHintNode;

		for (const TCHAR& Char : HintLabels[i])
		{
			// Convert the TCHAR to uint32 for KeyCode
			uint32 KeyCode = static_cast<uint32>(Char);

			// Get the FKey from the KeyCode and CharCode
			FKey MappedKey = FInputKeyManager::Get().GetKeyFromCodes(KeyCode, KeyCode);

			if (MappedKey.IsValid())
			{
				// Create the input chord
				FInputChord InputChord;
				InputChord.Key = MappedKey;

				// Traverse or create nodes in the trie
				if (!CurrentHintNode->Children.Contains(InputChord))
				{
					CurrentHintNode->Children.Add(InputChord, new FUMHintWidgetTrieNode());
				}
				CurrentHintNode = CurrentHintNode->Children[InputChord];
			}
			else
			{
				Logger.Print(FString::Printf(TEXT("Failed to map TCHAR '%c' to a valid FKey"), Char),
					ELogVerbosity::Error, true);
			}
		}

		// Mark as terminal and add the hint marker
		CurrentHintNode->bIsTerminal = true;
		CurrentHintNode->HintMarkers.Add(HintMarkers[i]);
	}
}

void UVimNavigationEditorSubsystem::ProcessHintInput(const FInputChord& InputChord)
{
	if (!CurrentHintNode)
		CurrentHintNode = RootHintNode; // Start at root if no active node

	if (CurrentHintNode->Children.Contains(InputChord))
	{
		CurrentHintNode = CurrentHintNode->Children[InputChord];

		if (CurrentHintNode->bIsTerminal)
		{
			// Trigger the actions for all hints in this node
			for (const TWeakPtr<SUMHintMarker>& Marker : CurrentHintNode->HintMarkers)
			{
				if (const TSharedPtr<SUMHintMarker> HM = Marker.Pin())
				{
					// ExecuteWidgetAction(Marker.Pin());
				}
			}

			ResetHintState(); // Reset the trie traversal
		}
		else
		{
			// Update visualization for all potential matches
			VisualizeHints(CurrentHintNode);
		}
	}
	else
	{
		// Invalid input, reset or handle gracefully
		ResetHintState();
	}
}

void UVimNavigationEditorSubsystem::VisualizeHints(FUMHintWidgetTrieNode* Node)
{
	if (!Node)
		return;

	// Visualize all hint markers in the current node
	for (const TWeakPtr<SUMHintMarker>& Marker : Node->HintMarkers)
	{
		if (const TSharedPtr<SUMHintMarker> HM = Marker.Pin())
		{
			// TODO:
			// HM.VisualizeHint();
		}
	}

	// Optionally, visualize markers in child nodes (recursive or iterative)
	// for (auto& Pair : Node->Children)
	// {
	//     VisualizeHints(Pair.Value);
	// }
}

void UVimNavigationEditorSubsystem::ResetHintState()
{
	CurrentHintNode = nullptr;
	// RootHintNode = nullptr; // Right?

	if (const TSharedPtr<SWindow> Win = HintOverlayData.AssociatedWindow.Pin())
	{
		if (const TSharedPtr<SUMHintOverlay> HintOverlay = HintOverlayData.HintOverlay.Pin())
		{
			Win->RemoveOverlaySlot(HintOverlay.ToSharedRef());
		}
	}
	HintOverlayData.Reset();
}

void UVimNavigationEditorSubsystem::OnVimModeChanged(const EVimMode NewVimMode)
{
	if (HintOverlayData.IsDisplayed() && NewVimMode == EVimMode::Normal)
		ResetHintState();
}

void UVimNavigationEditorSubsystem::BindVimCommands()
{
	TSharedRef<FVimInputProcessor> VimInputProcessor = FVimInputProcessor::Get();

	// Input PreProcessor Listeners
	// VimInputProcessor->OnResetSequence.AddUObject(
	// 	this, &UVimNavigationEditorSubsystem::OnResetSequence);

	// VimInputProcessor->OnCountPrefix.AddUObject(
	// 	this, &UVimNavigationEditorSubsystem::OnCountPrefix);

	VimInputProcessor->OnVimModeChanged.AddUObject(
		this, &UVimNavigationEditorSubsystem::OnVimModeChanged);

	TWeakObjectPtr<UVimNavigationEditorSubsystem> WeakNavigationSubsystem =
		MakeWeakObjectPtr(this);

	VimInputProcessor->AddKeyBinding_KeyEvent(
		{ FInputChord(EModifierKey::Control, EKeys::H) },
		WeakNavigationSubsystem,
		&UVimNavigationEditorSubsystem::NavigatePanelTabs);

	VimInputProcessor->AddKeyBinding_KeyEvent(
		{ FInputChord(EModifierKey::Control, EKeys::J) },
		WeakNavigationSubsystem,
		&UVimNavigationEditorSubsystem::NavigatePanelTabs);

	VimInputProcessor->AddKeyBinding_KeyEvent(
		{ FInputChord(EModifierKey::Control, EKeys::K) },
		WeakNavigationSubsystem,
		&UVimNavigationEditorSubsystem::NavigatePanelTabs);

	VimInputProcessor->AddKeyBinding_KeyEvent(
		{ FInputChord(EModifierKey::Control, EKeys::L) },
		WeakNavigationSubsystem,
		&UVimNavigationEditorSubsystem::NavigatePanelTabs);

	VimInputProcessor->AddKeyBinding_KeyEvent(
		{ EKeys::F },
		WeakNavigationSubsystem,
		&UVimNavigationEditorSubsystem::FlashHintMarkers);
}
