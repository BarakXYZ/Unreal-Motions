#include "VimNavigationEditorSubsystem.h"
#include "Framework/Docking/TabManager.h"
#include "Input/Events.h"
#include "Misc/StringFormatArg.h"
#include "Types/SlateEnums.h"
#include "UMInputHelpers.h"
#include "UMSlateHelpers.h"
#include "UObject/ObjectMacros.h"
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

	// Create Hint Marker Labels (e.g. "HH", "HL", "S", etc.)
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
				.MarkerText(Labels[i]);

		HintMarkers.Add(HintMarker);
	}

	if (!BuildHintTrie(Labels, HintMarkers)) // Build the Trie from these markers
		return;

	// Add all Hint Markers to the screen
	TSharedRef<SUMHintOverlay> HintOverlay = SNew(SUMHintOverlay, MoveTemp(HintMarkers));

	// 99999 seems to be enough to place it above things like the Content
	// Browser (Drawer) too.
	ActiveWindow->AddOverlaySlot(99999)[HintOverlay];

	HintOverlayData = FHintOverlayData(HintOverlay, ActiveWindow, true);

	// We want to possess our processor to temporarily handle any input manually.
	FVimInputProcessor::Get()->Possess(this,
		&UVimNavigationEditorSubsystem::ProcessHintInput);

	Logger.Print(FString::Printf(TEXT("Created %d Hint Markers!"), NumWidgets), ELogVerbosity::Verbose, true);
}

void UVimNavigationEditorSubsystem::FlashHintMarkersMultiWindow(
	FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	// If we already have any overlays displayed, reset them first
	if (!PerWindowHintOverlayData.IsEmpty())
		ResetHintMarkersMultiWindow();

	// Collect interactable widgets from *all* visible windows:
	TArray<TArray<TSharedPtr<SWidget>>> InteractableWidgetsPerWindow;
	TArray<TSharedRef<SWindow>>			ParentWindows;
	if (!CollectInteractableWidgets(InteractableWidgetsPerWindow, ParentWindows))
		return; // No widgets found anywhere

	// Count total # of widgets across *all* windows
	int32 TotalNumWidgets = 0;
	for (const auto& WindowWidgets : InteractableWidgetsPerWindow)
		TotalNumWidgets += WindowWidgets.Num();

	// Create Hint Marker Labels (e.g. "HH", "HL", "S", etc.)
	const TArray<FString> AllLabels = GenerateLabels(TotalNumWidgets);

	// We'll create one big array of hint markers so we can build a single trie.
	TArray<TSharedRef<SUMHintMarker>> AllMarkers;
	AllMarkers.Reserve(TotalNumWidgets);

	// The label array is a single pool; we hand out slices to each window.
	int32 LabelIndex = 0;

	// For each window, create an overlay, fill it with markers
	for (int32 WindowIdx{ 0 }; WindowIdx < ParentWindows.Num(); ++WindowIdx)
	{
		const auto	ParentWin = ParentWindows[WindowIdx];
		const auto& ChildWidgets = InteractableWidgetsPerWindow[WindowIdx];

		// Build the markers for *this* window
		TArray<TSharedRef<SUMHintMarker>> WindowMarkers;
		WindowMarkers.Reserve(ChildWidgets.Num());

		for (int32 i = 0; i < ChildWidgets.Num(); ++i)
		{
			const TSharedPtr<SWidget>& WidgetPtr = ChildWidgets[i];
			if (!WidgetPtr.IsValid())
				continue;

			// Make a new hint marker
			TSharedRef<SUMHintMarker> HintMarker =
				SNew(SUMHintMarker)
					.TargetWidget(WidgetPtr.ToSharedRef())
					.InWidgetLocalPosition(
						FUMSlateHelpers::GetWidgetLocalPositionInWindow(
							WidgetPtr.ToSharedRef(), ParentWin))
					.MarkerText(AllLabels[LabelIndex]);

			WindowMarkers.Add(HintMarker);
			AllMarkers.Add(HintMarker);
			++LabelIndex;
		}

		// Create an overlay for this window
		const TSharedRef<SUMHintOverlay> HintOverlay =
			SNew(SUMHintOverlay, MoveTemp(WindowMarkers));

		// 99999 seems to be enough to place it above things like the Content
		// Browser (Drawer) too.
		ParentWin->AddOverlaySlot(99999)[HintOverlay];

		// Track this overlay in our array
		FHintOverlayData OverlayData(HintOverlay, ParentWin, true);
		PerWindowHintOverlayData.Add(OverlayData);
	}

	// Build one trie from all the (label, marker) pairs
	if (!BuildHintTrie(AllLabels, AllMarkers))
		return;

	// Possess the input processor so we can handle the typed input
	FVimInputProcessor::Get()->Possess(this,
		&UVimNavigationEditorSubsystem::ProcessHintInputMultiWindow);

	Logger.Print(
		FString::Printf(TEXT("Created %d Hint Markers across %d windows!"),
			TotalNumWidgets, ParentWindows.Num()),
		ELogVerbosity::Verbose, true);
}

bool UVimNavigationEditorSubsystem::CollectInteractableWidgets(
	TArray<TSharedPtr<SWidget>>& OutWidgets)
{
	// Get & validate the currently focused widget (from which we will traverse)
	FSlateApplication&		  SlateApp = FSlateApplication::Get();
	const TSharedPtr<SWidget> FocusedWidget = SlateApp.GetUserFocusedWidget(0);
	if (!FocusedWidget.IsValid())
		return false;

	const TSharedPtr<SWindow> ActiveWindow = SlateApp.GetActiveTopLevelRegularWindow();
	if (!ActiveWindow.IsValid())
		return false;

	return FUMSlateHelpers::TraverseFindWidget(ActiveWindow.ToSharedRef(),
		OutWidgets, FUMSlateHelpers::GetInteractableWidgetTypes());
}

bool UVimNavigationEditorSubsystem::CollectInteractableWidgets(
	TArray<TArray<TSharedPtr<SWidget>>>& OutWidgets,
	TArray<TSharedRef<SWindow>>&		 ParentWindows)
{
	// Get & validate the currently focused widget (from which we will traverse)
	FSlateApplication&		  SlateApp = FSlateApplication::Get();
	const TSharedPtr<SWidget> FocusedWidget = SlateApp.GetUserFocusedWidget(0);
	if (!FocusedWidget.IsValid())
		return false;

	TArray<TSharedRef<SWindow>> VisibleWindows;
	SlateApp.GetAllVisibleWindowsOrdered(VisibleWindows);

	// Fetch the Interactive Widgets that each windows has.
	for (const TSharedRef<SWindow>& Win : VisibleWindows)
	{
		TArray<TSharedPtr<SWidget>> InteractableWidgets;
		if (FUMSlateHelpers::TraverseFindWidget(Win,
				InteractableWidgets, FUMSlateHelpers::GetInteractableWidgetTypes()))
		{
			// Need to check Overlay Support to avoid errors!
			// if (Win->IsRegularWindow() && Win->HasOverlay())
			if (Win->IsRegularWindow()) // Seems to suffice too.
			{
				OutWidgets.Add(MoveTemp(InteractableWidgets));
				ParentWindows.Add(Win);
			}
		}
	}
	// Return true if OutWidgets contains at least one non-empty array
	return (!OutWidgets.IsEmpty() && !OutWidgets[0].IsEmpty());
}

TArray<FString> UVimNavigationEditorSubsystem::GenerateLabels(int32 NumLabels)
{
	if (NumLabels <= 0)
		return TArray<FString>(); // No labels

	// Default characters. May want to play with different combos.
	// More Options to try: "fghjklrueidcvm" or "asdfgqwertzxcvb"
	// const FString Alphabet = TEXT("FGHJKLRUEIDC");
	const FString Alphabet = TEXT("ASDFGHJKLWECP");
	// const FString Alphabet = TEXT("MCIEUWLKJHGFDSA");  // Reversed (TEST)

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
				// Parent reference to support climbing up on BackSpace
				NewNode->Parent = CurrentNode;
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

void UVimNavigationEditorSubsystem::ProcessHintInputBase(
	FSlateApplication&							SlateApp,
	const FKeyEvent&							InKeyEvent,
	TFunction<void()>							ResetHintMarkersFunc,
	TFunction<void(const TSharedRef<SWidget>&)> HandleWidgetExecutionFunc)
{
	const FInputChord Chord = FUMInputHelpers::GetChordFromKeyEvent(InKeyEvent);

	// If there's no current node, assume we start at the root
	if (!CurrentHintNode.IsValid())
		CurrentHintNode = RootHintNode;

	if (!CurrentHintNode.IsValid())
	{
		ResetHintMarkersFunc();
		return; // Reset if no valid Trie.
	}

	if (OnBackSpaceHintMarkers(InKeyEvent))
		return;

	// Check if there's a child for this chord
	TSharedPtr<FUMHintWidgetTrieNode>* ChildPtr = CurrentHintNode->Children.Find(Chord);
	if (!ChildPtr || !ChildPtr->IsValid())
	{
		ResetHintMarkersFunc(); // No match for this chord -> reset
		return;
	}

	CurrentHintNode = *ChildPtr; // Move to child

	// If this node is terminal, we've spelled out an entire label
	if (CurrentHintNode->bIsTerminal)
	{
		const TArray<TWeakPtr<SUMHintMarker>>& HintMarkers = CurrentHintNode->HintMarkers;
		if (HintMarkers.Num() != 1)
			Logger.Print("Terminal Node: Doesn't contain exactly 1 Node!",
				ELogVerbosity::Error, true);

		else if (const TSharedPtr<SUMHintMarker> HM = HintMarkers[0].Pin())
		{
			Logger.Print("Node marked as terminal, contains exactly 1 Node!", ELogVerbosity::Verbose, true);

			// Handle widget execution
			if (const TSharedPtr<SWidget> Widget = HM->TargetWidgetWeak.Pin())
				HandleWidgetExecutionFunc(Widget.ToSharedRef());
		}
		ResetHintMarkersFunc();
	}
	else
		// Visualize partial matches by showing all the markers in this node
		VisualizeHints(CurrentHintNode.ToSharedRef());
}

void UVimNavigationEditorSubsystem::ProcessHintInput(
	FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	ProcessHintInputBase(
		SlateApp,
		InKeyEvent,
		[this]() { ResetHintMarkers(); },
		[&SlateApp](const TSharedRef<SWidget>& Widget) {
			FUMFocusHelpers::HandleWidgetExecution(SlateApp, Widget);
		});
}

void UVimNavigationEditorSubsystem::ProcessHintInputMultiWindow(
	FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	ProcessHintInputBase(
		SlateApp,
		InKeyEvent,
		[this]() { ResetHintMarkersMultiWindow(); },
		[&SlateApp](const TSharedRef<SWidget>& Widget) {
			const TSharedPtr<SWindow> WidgetWin = SlateApp.FindWidgetWindow(Widget);
			if (WidgetWin.IsValid())
				FUMSlateHelpers::ActivateWindow(WidgetWin.ToSharedRef());

			FUMFocusHelpers::HandleWidgetExecutionWithDelay(SlateApp, Widget);
		});
}

void UVimNavigationEditorSubsystem::VisualizeHints(const TSharedRef<FUMHintWidgetTrieNode> Node)
{
	// Visualize all hint markers in the current node
	for (const TWeakPtr<SUMHintMarker>& Marker : Node->HintMarkers)
	{
		if (const TSharedPtr<SUMHintMarker> HM = Marker.Pin())
		{
			HM->VisualizePressedKey(true);
			PressedHintMarkers.Add(HM); // Track pressed hints
		}
	}
}

bool UVimNavigationEditorSubsystem::OnBackSpaceHintMarkers(const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == FKey(EKeys::BackSpace)
		&& !PressedHintMarkers.IsEmpty())
	{
		TArray<TWeakPtr<SUMHintMarker>> MarkersToRemove;

		for (const auto& WeakHintMarker : PressedHintMarkers)
		{
			if (const TSharedPtr<SUMHintMarker> HintMarker = WeakHintMarker.Pin())
			{
				HintMarker->VisualizePressedKey(false);
				if (HintMarker->PressedKeyIndex == 0)
					MarkersToRemove.Add(WeakHintMarker);
			}
			else // Remove invalid reference
				MarkersToRemove.Add(WeakHintMarker);
		}
		for (const auto& WeakHintMarker : MarkersToRemove)
			PressedHintMarkers.Remove(WeakHintMarker);

		// Walk backwards to the parent node
		if (const auto Parent = CurrentHintNode->Parent.Pin())
			CurrentHintNode = Parent;
		else
			CurrentHintNode = RootHintNode;

		return true;
	}
	return false;
}

void UVimNavigationEditorSubsystem::ResetHintMarkersCore()
{
	CurrentHintNode.Reset(); // Remove all references
	RootHintNode.Reset();	 // Destroy Trie
	PressedHintMarkers.Empty();
	FVimInputProcessor::Get()->Unpossess(this);
}

void UVimNavigationEditorSubsystem::ResetHintMarkers()
{
	ResetHintMarkersCore();
	// Remove the overlay from the window (if it was added)
	if (const TSharedPtr<SWindow> Win = HintOverlayData.AssociatedWindow.Pin())
	{
		if (const TSharedPtr<SUMHintOverlay> HintOverlay = HintOverlayData.HintOverlay.Pin())
		{
			Win->RemoveOverlaySlot(HintOverlay.ToSharedRef());
		}
	}
	HintOverlayData.Reset();
}

void UVimNavigationEditorSubsystem::ResetHintMarkersMultiWindow()
{
	ResetHintMarkersCore();
	// Remove each overlay from its window
	for (const FHintOverlayData& OverlayData : PerWindowHintOverlayData)
	{
		if (const TSharedPtr<SWindow> Win = OverlayData.AssociatedWindow.Pin())
		{
			if (const TSharedPtr<SUMHintOverlay> HintOverlay = OverlayData.HintOverlay.Pin())
			{
				Win->RemoveOverlaySlot(HintOverlay.ToSharedRef());
			}
		}
	}
	PerWindowHintOverlayData.Empty();
}

void UVimNavigationEditorSubsystem::OnVimModeChanged(const EVimMode NewVimMode)
{
	if (HintOverlayData.IsDisplayed() && NewVimMode == EVimMode::Normal)
	{
		ResetHintMarkers();
		ResetHintMarkersMultiWindow();
	}
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

	VimInputProcessor->AddKeyBinding_KeyEvent(
		{ FInputChord(EModifierKey::Shift, EKeys::F) },
		WeakNavigationSubsystem,
		&UVimNavigationEditorSubsystem::FlashHintMarkersMultiWindow);
}
