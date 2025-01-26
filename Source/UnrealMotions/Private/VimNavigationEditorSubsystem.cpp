#include "VimNavigationEditorSubsystem.h"
#include "Framework/Docking/TabManager.h"
#include "Input/Events.h"
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
#include "UMFocusHelpers.h"

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
	if (HintOverlayData.IsDisplayed())
		ResetHintMarkers(); // Reset existing Hint Markers (if any)

	const TSharedPtr<SWindow> ActiveWindow = SlateApp.GetActiveTopLevelRegularWindow();
	if (!ActiveWindow.IsValid())
		return;

	TArray<TSharedPtr<SWidget>> InteractableWidgets;
	if (!CollectInteractableWidgets(InteractableWidgets))
		return; // Will return false if no widgets were found.

	const int32 NumWidgets = InteractableWidgets.Num();

	// Create Hint Marker Labels
	const TArray<FString> Labels = GenerateLabels(NumWidgets);

	// Create the Hint Marker Widgets
	TArray<TSharedRef<SUMHintMarker>> HintMarkers;
	HintMarkers.Reserve(NumWidgets);

	for (int32 i = 0; i < NumWidgets; ++i)
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
	}

	if (!BuildHintTrie(Labels, HintMarkers)) // Build the Trie from these markers
		return;

	// Add all Hint Markers to the screen
	TSharedRef<SUMHintOverlay> HintOverlay = SNew(SUMHintOverlay, MoveTemp(HintMarkers));

	ActiveWindow->AddOverlaySlot(999)[HintOverlay];

	HintOverlayData = FHintOverlayData(HintOverlay, ActiveWindow, true);

	// We want to possess our processor to temporarily handle any input manually.
	FVimInputProcessor::Get()->Possess(this,
		&UVimNavigationEditorSubsystem::ProcessHintInput);

	Logger.Print(FString::Printf(TEXT("Created %d Hint Markers!"), NumWidgets), ELogVerbosity::Verbose, true);
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

TArray<FString> UVimNavigationEditorSubsystem::GenerateLabels(int32 NumLabels)
{
	if (NumLabels <= 0)
		return TArray<FString>(); // No labels

	// Default characters. May want to play with different combos.
	// More Options to try: "fghjklrueidcvm" or "asdfgqwertzxcvb"
	const FString Alphabet = TEXT("FGHJKLRUEIDC");
	// const FString Alphabet = TEXT("AFGHJKLRUEIDC");
	// const FString Alphabet = TEXT("ASDFGQWERTZXCVB");

	// -----------------------------
	// The BFS-like expansion:
	// -----------------------------
	// Start with an array = [""].
	// We'll pop from the front (via `offset`) and expand into N new labels
	// by prepending each character in Alphabet.  Then we continue until
	// we have at least NumLabels beyond the current offset (or else
	// the array has only 1 element).
	//
	// After BFS finishes, we extract exactly NumLabels from that array,
	// sort them, then reverse each string. (Mimicing Vimium's approach)

	TArray<FString> BFS;
	BFS.Reserve(NumLabels * 2); // Just an optimization
	BFS.Add(TEXT(""));			// Start from an empty string

	int32 Offset = 0;
	while (((BFS.Num() - Offset) < NumLabels) || (BFS.Num() == 1))
	{
		// "Pop" one string from the BFS queue.
		FString Current = BFS[Offset++];
		// Expand by prepending each allowed character.
		for (int32 i = 0; i < Alphabet.Len(); ++i)
		{
			// In Vimium's code, they do `ch + oldString` (string concatenation).
			// That “builds” the Label in reverse. We’ll also do that so we can
			// replicate the final sort+reverse step exactly.
			const TCHAR C = Alphabet[i];
			BFS.Add(FString(1, &C) + Current);
		}
	}

	// Now slice out exactly NumLabels from [offset.. offset+NumLabels).
	TArray<FString> Sliced;
	Sliced.Reserve(NumLabels);
	for (int32 i = 0; i < NumLabels; ++i)
	{
		Sliced.Add(BFS[Offset + i]);
	}

	// Sort them lexicographically (still reversed).
	// Vimium does `hints.sort()` in JavaScript, which is a plain
	// lexicographical sort.
	Sliced.Sort([](const FString& A, const FString& B) {
		return A < B; // Standard ascending lex-order compare
	});

	// Finally, reverse each string (since BFS built them backwards).
	// Vimium does: `hints.map(str => str.reverse())`.
	// In C++, we can do:
	for (FString& Label : Sliced)
	{
		// std::reverse style approach:
		int32 i = 0;
		int32 j = Label.Len() - 1;
		while (i < j)
		{
			TCHAR Temp = Label[i];
			Label[i] = Label[j];
			Label[j] = Temp;
			i++;
			j--;
		}
	}
	return Sliced;
}

bool UVimNavigationEditorSubsystem::BuildHintTrie(
	const TArray<FString>&					 HintLabels,
	const TArray<TSharedRef<SUMHintMarker>>& HintMarkers)
{
	// Reset the entire Trie first (if it already existed).
	RootHintNode.Reset();

	// Create a fresh root node:
	RootHintNode = MakeShared<FUMHintWidgetTrieNode>();

	// We'll reuse this pointer for traversal:
	TSharedPtr<FUMHintWidgetTrieNode> TraversalNode;

	// For each Label (i.e. 'A', 'JK', 'HH', 'HF', etc.)
	for (int32 i = 0; i < HintLabels.Num(); ++i)
	{
		const FString&					 Label = HintLabels[i];
		const TSharedRef<SUMHintMarker>& Marker = HintMarkers[i];

		TSharedPtr<FUMHintWidgetTrieNode> CurrentNode = RootHintNode;

		// The root node also accumulates *all* markers (so it includes
		// every label's marker for partial matches at the very start).
		CurrentNode->HintMarkers.Add(Marker);

		// For each character in the hint string (i.e. 'JK' -> 'J', 'K')
		for (TCHAR Char : HintLabels[i])
		{
			// Map this TCHAR to an FKey
			const uint32 KeyCode = static_cast<uint32>(Char);
			const FKey	 MappedKey = FInputKeyManager::Get().GetKeyFromCodes(KeyCode, KeyCode);

			if (!CheckCharToKeyConversion(Char, MappedKey))
				return false;

			const FInputChord InputChord(MappedKey);

			// If the child node doesn't exist, create it:
			if (!CurrentNode->Children.Contains(InputChord))
			{
				TSharedPtr<FUMHintWidgetTrieNode> NewNode = MakeShared<FUMHintWidgetTrieNode>();
				CurrentNode->Children.Add(InputChord, NewNode);
			}

			// Move into the child node
			TSharedPtr<FUMHintWidgetTrieNode> ChildNode = CurrentNode->Children[InputChord];

			// This ChildNode also accumulates the same marker for partial
			// matching (visualization)
			ChildNode->HintMarkers.Add(Marker);

			// Advance
			CurrentNode = ChildNode;
		}

		// When we've consumed all characters in a Label, mark the final node as
		// terminal (e.g. 'JK' -> 'J' -> [non-terminal] ... 'K' -> [terminal])
		CurrentNode->bIsTerminal = true;
		// By definition, that node now has exactly one unique marker for
		// that label, but it may also have an array with size 1.
	}
	return true;
}

bool UVimNavigationEditorSubsystem::CheckCharToKeyConversion(
	const TCHAR InChar, const FKey& InKey)
{
	if (InKey.IsValid())
	{
		Logger.Print(FString::Printf(TEXT("Mapped TCHAR '%c' to an FKey: %s"),
						 InChar, *InKey.GetFName().ToString()),
			ELogVerbosity::Verbose);
		return true;
	}
	else
	{
		Logger.Print(FString::Printf(TEXT("Failed to map TCHAR '%c' to an FKey"),
						 InChar),
			ELogVerbosity::Error, true);
		return false;
	}
}

void UVimNavigationEditorSubsystem::ProcessHintInput(
	FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	const FInputChord InputChord = FUMInputHelpers::GetChordFromKeyEvent(InKeyEvent);
	// If there's no current node, assume we start at the root
	if (!CurrentHintNode.IsValid())
		CurrentHintNode = RootHintNode;

	if (!CurrentHintNode.IsValid())
	{
		ResetHintMarkers();
		return; // Reset if no valid Trie.
	}

	// Check if there's a child for this chord
	TSharedPtr<FUMHintWidgetTrieNode>* ChildPtr = CurrentHintNode->Children.Find(InputChord);
	if (!ChildPtr || !ChildPtr->IsValid())
	{
		ResetHintMarkers(); // No match for this chord -> reset
		return;
	}

	CurrentHintNode = *ChildPtr; // Move to child

	// If this node is terminal, we've spelled out an entire label
	if (CurrentHintNode->bIsTerminal)
	{
		// Because each label is unique, there should be exactly 1 marker
		// in the array that is truly the final label's marker.
		const TArray<TWeakPtr<SUMHintMarker>>& HintMarkers = CurrentHintNode->HintMarkers;
		if (HintMarkers.Num() != 1) // Check if we really have only 1
		{
			Logger.Print("Node marked as terminal, yet doesn't contain exactly 1 Node!", ELogVerbosity::Error, true);
		}
		else
		{
			if (const TSharedPtr<SUMHintMarker> HM = HintMarkers[0].Pin())
			{
				Logger.Print("Node marked as terminal, contains exactly 1 Node!", ELogVerbosity::Verbose, true);

				// Focus internal widget
				if (const TSharedPtr<SWidget> Widget = HM->TargetWidgetWeak.Pin())
				{
					// SlateApp.SetAllUserFocus(Widget, EFocusCause::Navigation);
					// FUMInputHelpers::SimulateClickOnWidget(SlateApp, Widget.ToSharedRef(), FKey(EKeys::LeftMouseButton));
					FUMFocusHelpers::HandleWidgetExecution(SlateApp, Widget.ToSharedRef());
				}
			}
		}
		ResetHintMarkers(); // Done - reset all Hint Markers (Overlay) && Trie
	}
	else
		// Visualize partial matches by showing all the markers in this node
		VisualizeHints(CurrentHintNode);
}

void UVimNavigationEditorSubsystem::VisualizeHints(TSharedPtr<FUMHintWidgetTrieNode> Node)
{
	if (!Node.IsValid())
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
}

void UVimNavigationEditorSubsystem::ResetHintMarkers()
{
	CurrentHintNode.Reset(); // Remove all references
	RootHintNode.Reset();	 // Destroy Trie

	// Remove the overlay from the window (if it was added)
	if (const TSharedPtr<SWindow> Win = HintOverlayData.AssociatedWindow.Pin())
	{
		if (const TSharedPtr<SUMHintOverlay> HintOverlay = HintOverlayData.HintOverlay.Pin())
		{
			Win->RemoveOverlaySlot(HintOverlay.ToSharedRef());
		}
	}
	HintOverlayData.Reset();
	FVimInputProcessor::Get()->Unpossess(this);
}

void UVimNavigationEditorSubsystem::OnVimModeChanged(const EVimMode NewVimMode)
{
	if (HintOverlayData.IsDisplayed() && NewVimMode == EVimMode::Normal)
		ResetHintMarkers();
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
