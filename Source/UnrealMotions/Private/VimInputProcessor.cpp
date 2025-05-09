// VimInputProcessor.cpp
#include "VimInputProcessor.h"
#include "Editor.h"
#include "Engine/GameInstance.h"
#include "Framework/Application/SlateApplication.h"
#include "UMInputHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogUMInputPreProcessor, Log, All); // Dev

EVimMode FVimInputProcessor::VimMode{ EVimMode::Insert };

bool FVimInputProcessor::bNativeInputHandling{ false };

FVimInputProcessor::FVimInputProcessor()
{
	Logger = FUMLogger(&LogUMInputPreProcessor);

	RegisterDefaultKeyBindings();
}
FVimInputProcessor::~FVimInputProcessor()
{
	ContextTrieRoots.Empty();
}

TSharedRef<FVimInputProcessor> FVimInputProcessor::Get()
{
	static const TSharedRef<FVimInputProcessor> InputPreProcessor =
		MakeShared<FVimInputProcessor>();
	return InputPreProcessor;
}

void FVimInputProcessor::Tick(
	const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor)
{
	// if (TSharedPtr<SWidget> FocusedWidget = SlateApp.GetUserFocusedWidget(0))
	// 	FUMLogger::AddDebugMessage(FocusedWidget->GetTypeAsString());
}

TSharedPtr<FKeyChordTrieNode> FVimInputProcessor::GetOrCreateTrieRoot(EUMBindingContext Context, EVimMode InVimMode)
{
	// Try to find an existing map for the context
	TMap<EVimMode, TSharedPtr<FKeyChordTrieNode>>* VimMap = ContextTrieRoots.Find(Context);
	if (VimMap)
	{
		// Try to find an existing root for the specific Vim mode
		if (TSharedPtr<FKeyChordTrieNode>* FoundRoot = VimMap->Find(InVimMode))
		{
			return *FoundRoot;
		}
		// Otherwise, create a new one for this Vim mode
		TSharedPtr<FKeyChordTrieNode> NewRoot = MakeShared<FKeyChordTrieNode>();
		VimMap->Add(InVimMode, NewRoot);
		return NewRoot;
	}
	// If no map for the context exists, create one
	TMap<EVimMode, TSharedPtr<FKeyChordTrieNode>> NewVimMap;
	TSharedPtr<FKeyChordTrieNode>				  NewRoot = MakeShared<FKeyChordTrieNode>();
	NewVimMap.Add(InVimMode, NewRoot);
	ContextTrieRoots.Add(Context, NewVimMap);
	return NewRoot;
}

TSharedPtr<FKeyChordTrieNode> FVimInputProcessor::FindOrCreateTrieNode(
	TSharedPtr<FKeyChordTrieNode> Root,
	const TArray<FInputChord>&	  Sequence)
{
	TSharedPtr<FKeyChordTrieNode> Current = Root;

	for (const FInputChord& Key : Sequence)
	{
		// If a child doesn’t exist for this chord, create one
		if (!Current->Children.Contains(Key))
		{
			Current->Children.Add(Key, MakeShared<FKeyChordTrieNode>());
		}

		// Move down to the child
		Current = Current->Children[Key];
	}

	return Current;
}

TSharedPtr<FKeyChordTrieNode> FVimInputProcessor::FindOrCreateTrieNode(const TArray<FInputChord>& Sequence)
{
	// Default to Generic context and Any Vim mode
	TSharedPtr<FKeyChordTrieNode> Root = GetOrCreateTrieRoot(EUMBindingContext::Generic, EVimMode::Any);
	return FindOrCreateTrieNode(Root, Sequence);
}

bool FVimInputProcessor::TraverseTrieForContextAndVim(EUMBindingContext InContext, EVimMode InVimMode, TSharedPtr<FKeyChordTrieNode>& OutNode) const
{
	OutNode = nullptr;

	// If there's no trie roots for this context, no match is possible
	if (!ContextTrieRoots.Contains(InContext))
		return false;

	const TMap<EVimMode, TSharedPtr<FKeyChordTrieNode>>& VimMap = ContextTrieRoots[InContext];

	// If there's no trie root for this Vim mode in the context, no match
	if (!VimMap.Contains(InVimMode))
		return false;

	// Get the root node for this context and Vim mode
	TSharedPtr<FKeyChordTrieNode> Root = VimMap[InVimMode];
	TSharedPtr<FKeyChordTrieNode> CurrentNode = Root;

	// Walk the sequence
	for (const FInputChord& SequenceKey : CurrentSequence)
	{
		if (!CurrentNode->Children.Contains(SequenceKey))
			return false; // Cannot find the specific sequence

		CurrentNode = CurrentNode->Children[SequenceKey];
	}

	// If we got here, we at least have a partial match
	OutNode = CurrentNode;
	return true;
}

// Process the current Vim Key Sequence
bool FVimInputProcessor::ProcessKeySequence(
	FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	// Add this key to CurrentSequence
	CurrentSequence.Add(FUMInputHelpers::GetChordFromKeyEvent(InKeyEvent));

	// Logger.Print(FString::Printf(TEXT("Current Sequence Num: %d"), CurrentSequence.Num()), true);

	// Traverse the trie to find Partial / Full Matches in the following order:
	//    1. (CurrentContext, VimMode) ->
	//    2. (CurrentContext, Any)     ->
	//    3. (Generic, VimMode)        ->
	//    4. (Generic, Any)
	TSharedPtr<FKeyChordTrieNode> MatchedNode;
	bool						  bMatchFound = false;

	if (TraverseTrieForContextAndVim(CurrentContext, VimMode, MatchedNode))
		bMatchFound = true;

	else if (TraverseTrieForContextAndVim(CurrentContext, EVimMode::Any, MatchedNode))
		bMatchFound = true;

	else if (TraverseTrieForContextAndVim(EUMBindingContext::Generic, VimMode, MatchedNode))
		bMatchFound = true;

	else if (TraverseTrieForContextAndVim(EUMBindingContext::Generic, EVimMode::Any, MatchedNode))
		bMatchFound = true;

	// If no Partial-Match was found; Reset and return false.
	if (!bMatchFound)
	{
		ResetSequence(SlateApp);
		return false;
	}

	// If we have a matched node, see if it has a callback
	if (MatchedNode
		&& MatchedNode->CallbackType != EUMKeyBindingCallbackType::None)
	{
		const int32 CountPrefix = GetCountBuffer();
		switch (MatchedNode->CallbackType)
		{
			case EUMKeyBindingCallbackType::NoParam:
				if (MatchedNode->NoParamCallback)
					for (int32 i{ 0 }; i < CountPrefix; ++i)
						MatchedNode->NoParamCallback();
				break;

			case EUMKeyBindingCallbackType::KeyEventParam:
				if (MatchedNode->KeyEventCallback)
					for (int32 i{ 0 }; i < CountPrefix; ++i)
						MatchedNode->KeyEventCallback(SlateApp, InKeyEvent);
				break;

			case EUMKeyBindingCallbackType::SequenceParam:
				if (MatchedNode->SequenceCallback)
					for (int32 i{ 0 }; i < CountPrefix; ++i)
						MatchedNode->SequenceCallback(SlateApp, CurrentSequence);
				break;

			default:
				break;
		}

		// If we found a full match, reset sequence after invoking
		ResetSequence(SlateApp);
		return true;
	}

	// If no callback yet, but partial match is valid => keep building
	return true;
}

void FVimInputProcessor::ResetSequence(FSlateApplication& SlateApp)
{
	// Logger.Print("Reset Input Sequence -> Vim Proc", true);
	OnResetSequence.Broadcast();
	CountBuffer.Empty();
	CurrentSequence.Empty();
	CurrentBuffer.Empty();
	bIsCounting = false;
	ResetBufferVisualizer(SlateApp);
}

void FVimInputProcessor::ResetBufferVisualizer(FSlateApplication& SlateApp)
{
	if (BufferVisualizer.IsValid())
	{
		TSharedPtr<SWindow> ParentWindow =
			SlateApp.FindWidgetWindow(
				BufferVisualizer.Pin().ToSharedRef());
		if (ParentWindow.IsValid())
			ParentWindow->RemoveOverlaySlot(BufferVisualizer.Pin().ToSharedRef());
	}
}

// 1) No-Parameter
void FVimInputProcessor::AddKeyBinding_NoParam(
	EUMBindingContext		   Context,
	const TArray<FInputChord>& Sequence,
	TFunction<void()>		   Callback,
	EVimMode				   InVimMode)
{
	TSharedPtr<FKeyChordTrieNode> Root = GetOrCreateTrieRoot(Context, InVimMode);
	TSharedPtr<FKeyChordTrieNode> Node = FindOrCreateTrieNode(Root, Sequence);

	Node->NoParamCallback = MoveTemp(Callback);
	Node->CallbackType = EUMKeyBindingCallbackType::NoParam;
}

// 2) Single FKeyEvent param
void FVimInputProcessor::AddKeyBinding_KeyEvent(
	EUMBindingContext											   Context,
	const TArray<FInputChord>&									   Sequence,
	TFunction<void(FSlateApplication& SlateApp, const FKeyEvent&)> Callback,
	EVimMode													   InVimMode)
{
	TSharedPtr<FKeyChordTrieNode> Root = GetOrCreateTrieRoot(Context, InVimMode);
	TSharedPtr<FKeyChordTrieNode> Node = FindOrCreateTrieNode(Root, Sequence);

	Node->KeyEventCallback = MoveTemp(Callback);
	Node->CallbackType = EUMKeyBindingCallbackType::KeyEventParam;
}

// 3) TArray<FInputChord> param
void FVimInputProcessor::AddKeyBinding_Sequence(
	EUMBindingContext		   Context,
	const TArray<FInputChord>& Sequence,
	TFunction<void(FSlateApplication& SlateApp,
		const TArray<FInputChord>&)>
			 Callback,
	EVimMode InVimMode)
{
	TSharedPtr<FKeyChordTrieNode> Root = GetOrCreateTrieRoot(Context, InVimMode);
	TSharedPtr<FKeyChordTrieNode> Node = FindOrCreateTrieNode(Root, Sequence);

	Node->SequenceCallback = MoveTemp(Callback);
	Node->CallbackType = EUMKeyBindingCallbackType::SequenceParam;
}

void FVimInputProcessor::RegisterDefaultKeyBindings()
{
	// For convenience, ensure the Generic root exists (for default Vim mode Any)
	GetOrCreateTrieRoot(EUMBindingContext::Generic, EVimMode::Any);

	// Example default key bindings in Generic:
	AddKeyBinding_KeyEvent(
		EUMBindingContext::Generic,
		{ EKeys::I },
		[this](FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) {
			SwitchVimModes(SlateApp, InKeyEvent);
		},
		EVimMode::Any);

	AddKeyBinding_KeyEvent(
		EUMBindingContext::Generic,
		{ FInputChord(EModifierKey::Shift, EKeys::V) },
		[this](FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) {
			SwitchVimModes(SlateApp, InKeyEvent);
		},
		EVimMode::Any);

	AddKeyBinding_KeyEvent(
		EUMBindingContext::Generic,
		{ EKeys::V },
		[this](FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) {
			SwitchVimModes(SlateApp, InKeyEvent);
		},
		EVimMode::Any);
}

bool FVimInputProcessor::HandleKeyDownEvent(
	FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	DebugKeyEvent(InKeyEvent);

	// NOTE:
	// When we call SlateApp.ProcessKeyDownEvent(), it will trigger another
	// HandleKeyDownEvent call.
	// We need to return false during that second pass to let Unreal Engine
	// process the simulated key event.
	// Otherwise, the simulated key press won't be handled by the engine.
	// This mechanism is implemented as we sometimes need to simulate the native
	// UE handling of keys while maintaining our Vim state; Practically
	// allowing us to simulate native UE strokes from our Vim state.
	if (bNativeInputHandling)
	{
		bNativeInputHandling = false; // Resume manual handling from next event
		return false;
	}

	// We give an exception to the Escape key to be handled at this stage.
	if (IsSimulateEscapeKey(SlateApp, InKeyEvent))
		return true;

	// In case any outside class is currently possessing our Vim Processor to
	// handle input manually (e.g. Vimium implementation in the Vim Navigation
	// Subsystem); We'll return true and broadcast the InKeyEvent to them for
	// manual processing.
	if (Delegate_OnKeyDown.IsBound())
	{
		Delegate_OnKeyDown.Broadcast(SlateApp, InKeyEvent);
		return true;
	}

	if (ShouldSwitchVimMode(SlateApp, InKeyEvent)) // Return true and reset
		return true;

	if (VimMode == EVimMode::Insert)
	{
		// Have the option to handle some thing manually even in Insert mode?
		// For example 'Enter' in MultiLines behaving very annoyingliy?

		return false; // Pass the Input handling to Unreal native mode.
	}

	if (TrackCountPrefix(SlateApp, InKeyEvent)) // Return if currently counting.
		return true;

	const FKey InKey = InKeyEvent.GetKey();

	if (InKey.IsModifierKey()) // Simply ignore
		return true;		   // or false?

	// TODO: Add a small timer until actually showing the buffer &
	// (retrigger timer upon keystrokes)
	CheckCreateBufferVisualizer(SlateApp, InKey);
	UpdateBufferAndVisualizer(InKey);

	return ProcessKeySequence(SlateApp, InKeyEvent);
}

// NOTE:
// Keep an eye on this handler. Returning Up is important to when we're simulating
// things. Currently we're always returning Up, which seems to work fine.
// The key reminder here is that if we're not returning Up, we will always keep
// our keys Down, which cause issues.
bool FVimInputProcessor::HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	Delegate_OnKeyUpEvent.Broadcast(SlateApp, InKeyEvent);
	// Logger.Print("Key up", true);

	// Seems to work fine.
	// We're essentially passing the handling to Unreal by returning false.
	return false;
}

bool FVimInputProcessor::ShouldSwitchVimMode(
	FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	// TODO:
	// Give the user an option to configure entering Vim Modes via either Escape
	// or Shift+Escape to keep the native Escape as expected.
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		SetVimMode(SlateApp, EVimMode::Normal);
		ResetSequence(SlateApp);
		return true;
	}
	return false;
}

bool FVimInputProcessor::HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
{
	// Logger.Print("Mouse Button Down!", ELogVerbosity::Log, true);
	// return false;
	// TSharedPtr<SWidget> FocusedWidget = SlateApp.GetUserFocusedWidget(0);
	// if (FocusedWidget.IsValid())
	// {
	// 	const FSlateWidgetClassData ClassData = FocusedWidget->GetWidgetClass();
	// 	const FName					WidgetType = ClassData.GetWidgetType();
	// 	FSlateAttributeDescriptor	AttDesc = ClassData.GetAttributeDescriptor();
	// 	// AttDesc.DefaultSortOrder();
	// 	FString WidgetName = FocusedWidget->ToString();
	// 	FString DebugStr = FString::Printf(TEXT("Widget Type: %s, Widget Name: %s"), *WidgetType.ToString(), *WidgetName);
	//
	// 	return false;
	// }
	return false;
}

bool FVimInputProcessor::HandleMouseButtonUpEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
{
	// Logger.Print("Mouse Button Up!", ELogVerbosity::Log, true);
	return false;
}

bool FVimInputProcessor::HandleMouseMoveEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
{
	// Logger.Print("Mouse Button Move!", ELogVerbosity::Log, true);
	return false;
}

bool FVimInputProcessor::TrackCountPrefix(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	// Skip processing if either:
	// 1. This isn't the first key and we're not already in counting mode
	// 2. Any modifier keys are pressed (since modifiers can also be symbols)
	if ((!CurrentSequence.IsEmpty() && !bIsCounting)
		|| InKeyEvent.GetModifierKeys().AnyModifiersDown())
		return false;

	FString OutStr;
	// Check if the current key is a digit
	if (FUMInputHelpers::GetStrDigitFromKey(InKeyEvent.GetKey(), OutStr))
	{
		// If this is the first key in the sequence, start counting mode
		// Example: User pressed "4" which could start "4h" command
		if (CurrentSequence.IsEmpty())
		{
			bIsCounting = true;
			OnCountPrefix.Broadcast(OutStr);
			CountBuffer += OutStr;
			return true;
		}
		// Reject additional digits after initial input
		// Example sequence: If user types "374oc3":
		// 1. The "374" will be ignored.
		// 2. Only "oc3" will be processed as a valid command
		// 3. The "3" in "oc3" is treated as a direct keystroke,
		//    not as part of the count buffer
		// NOTE: Some commands will only check the direct keystroke
		// for count values, ignoring the count buffer entirely
		// and right after a valid command we will anyway reset all buffers,
		// which means we end up fresh & clean.
		return false;
	}
	return false;
}

void FVimInputProcessor::SwitchVimModes(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	const FKey& KeyPressed = InKeyEvent.GetKey();

	if (KeyPressed == EKeys::Escape)
		SetVimMode(SlateApp, EVimMode::Normal);

	if (VimMode == EVimMode::Normal)
	{
		if (KeyPressed == EKeys::I)
			SetVimMode(SlateApp, EVimMode::Insert);
		if (KeyPressed == EKeys::V)
			SetVimMode(SlateApp, EVimMode::Visual);
	}
}

bool FVimInputProcessor::IsSimulateEscapeKey(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	// NOTE:
	// Because Escape is used to *enter* Vim Normal mode; We'll use Shift+Escape
	// to simulate normal native Escape mode (which is sometimes needed in the
	// UE editor).
	// We may want to allow the user to reverse this and have Escape stay native,
	// while using Shift+Escape to enter Vim Mode. This should be a config option.
	if (InKeyEvent.IsShiftDown() && InKeyEvent.GetKey() == EKeys::Escape)
	{
		static const FKey Escape = EKeys::Escape;
		SimulateKeyPress(SlateApp, Escape); // Native Escape key simulation.

		// Logger.Print("Simulate Escape");
		return true;
	}
	return false;
}

void FVimInputProcessor::SetVimMode(FSlateApplication& SlateApp, const EVimMode NewMode, bool bResetCurrSequence, bool bBroadcastModeChange)
{
	if (VimMode != NewMode)
	{
		VimMode = NewMode;
		Logger.Print(UEnum::GetValueAsString(NewMode));
	}

	if (bResetCurrSequence)
		ResetSequence(SlateApp);

	if (bBroadcastModeChange)
		OnVimModeChanged.Broadcast(NewMode);
}

void FVimInputProcessor::SetVimMode(FSlateApplication& SlateApp, const EVimMode NewMode, const float Delay)
{
	FTimerHandle TimerHandle;
	GEditor->GetTimerManager()->SetTimer(
		TimerHandle,
		[this, &SlateApp, NewMode]() {
			SetVimMode(SlateApp, NewMode);
		},
		Delay, false);
}

FOnVimModeChanged& FVimInputProcessor::GetOnVimModeChanged()
{
	return OnVimModeChanged;
}

void FVimInputProcessor::RegisterOnVimModeChanged(TFunction<void(const EVimMode)> Callback)
{
	OnVimModeChanged.AddLambda(Callback);
}

void FVimInputProcessor::UnregisterOnVimModeChanged(const void* CallbackOwner)
{
	OnVimModeChanged.RemoveAll(CallbackOwner);
}

FKeyEvent FVimInputProcessor::GetKeyEventFromKey(const FKey& InKey, bool bIsShiftDown)
{
	const FModifierKeysState ModKeys(bIsShiftDown, bIsShiftDown,
		false, false, false, false, false, false, false);

	return FKeyEvent(
		InKey,
		ModKeys,
		0, false, 0, 0);
}

void FVimInputProcessor::SimulateMultiTabPresses(FSlateApplication& SlateApp, int32 TimesToRepeat)
{
	static const FKeyEvent TabEvent(
		FKey(EKeys::Tab),
		FModifierKeysState(),
		0, 0, 0, 0);

	for (int32 i{ 0 }; i < TimesToRepeat; ++i)
	{
		bNativeInputHandling = true;
		SlateApp.ProcessKeyDownEvent(TabEvent);
	}
}

void FVimInputProcessor::ToggleNativeInputHandling(const bool bNativeHandling)
{
	bNativeInputHandling = bNativeHandling;
}

void FVimInputProcessor::SimulateKeyPress(
	FSlateApplication& SlateApp, const FKey& SimulatedKey,
	const FModifierKeysState& ModifierKeys, bool bSetNativeInputHandling)
{
	const FKeyEvent SimulatedEvent(
		SimulatedKey,
		ModifierKeys,
		0 /*UserIndex*/, false /*bIsRepeat*/, 0, 0);

	bNativeInputHandling = bSetNativeInputHandling;
	SlateApp.ProcessKeyDownEvent(SimulatedEvent);
	SlateApp.ProcessKeyUpEvent(SimulatedEvent);
}

void FVimInputProcessor::CheckCreateBufferVisualizer(FSlateApplication& SlateApp, const FKey& InKey)
{
	// We probably want to show the buffer not only upon Leaderkey actually
	// if (InKey == EKeys::SpaceBar && CurrentBuffer.IsEmpty())
	if (CurrentBuffer.IsEmpty())
	{
		CurrentBuffer = ""; // Start a new buffer
		if (!BufferVisualizer.IsValid())
		{
			TSharedPtr<SWindow> ActiveWindow = SlateApp.GetActiveTopLevelRegularWindow();
			if (ActiveWindow.IsValid() && ActiveWindow->HasOverlay())
			{
				TSharedPtr<SUMBufferVisualizer> NewVisualizer =
					SNew(SUMBufferVisualizer);
				ActiveWindow->AddOverlaySlot()
					[NewVisualizer.ToSharedRef()];
				// Set visualizer to ignore mouse input; constructor won't suffice
				NewVisualizer->SetVisibility(EVisibility::HitTestInvisible);
				BufferVisualizer = NewVisualizer; // Store as weak pointer
			}
		}
	}
}

void FVimInputProcessor::UpdateBufferAndVisualizer(const FKey& InKey)
{
	// if it's empty; we just want to start the prefix with the pure name
	if (CurrentBuffer.IsEmpty())
	{
		CurrentBuffer = InKey.GetDisplayName().ToString();
	}
	else // if it's not empty; also append "+" to have a nice visualization
	{
		CurrentBuffer += " + " + InKey.GetDisplayName().ToString();
	}

	// if the Buffer Slate Widget is still valid; update it to the latest buffer
	if (const TSharedPtr<SUMBufferVisualizer> PinBufVis = BufferVisualizer.Pin())
	{
		PinBufVis->UpdateBuffer(CurrentBuffer);
	}
}

void FVimInputProcessor::Unpossess(UObject* InObject)
{
	if (PossessedObjects.Contains(InObject))
	{
		// Retrieve the binding handle and remove it from the delegate
		Delegate_OnKeyDown.Remove(PossessedObjects[InObject]);

		// Remove the object from the map
		PossessedObjects.Remove(InObject);
	}
}

int32 FVimInputProcessor::GetCountBuffer()
{
	if (!CountBuffer.IsEmpty())
	{
		return FMath::Clamp(
			FCString::Atoi(*CountBuffer),
			MIN_REPEAT_COUNT,
			MAX_REPEAT_COUNT);
	}
	return MIN_REPEAT_COUNT;
}

void FVimInputProcessor::DebugInvalidWeakPtr(EUMKeyBindingCallbackType CallbackType)
{
	FString Log = "Invalid";

	switch (CallbackType)
	{
		case EUMKeyBindingCallbackType::None:
			Log = "Invalid Weakptr _None";
			break;
		case EUMKeyBindingCallbackType::NoParam:
			Log = "Invalid Weakptr _NoParam";
			break;
		case EUMKeyBindingCallbackType::KeyEventParam:
			Log = "Invalid Weakptr _KeyEvent";
			break;
		case EUMKeyBindingCallbackType::SequenceParam:
			Log = "Invalid Weakptr _SequenceParam";
			break;
	}

	Logger.Print(Log, ELogVerbosity::Error, true);
}

void FVimInputProcessor::DebugKeyEvent(const FKeyEvent& InKeyEvent)
{
	FString EventPath = "NONE";
	if (InKeyEvent.GetEventPath())
		EventPath = InKeyEvent.GetEventPath()->ToString();

	// Causing crash
	// FString WinTitle = InKeyEvent.GetWindow()->GetTitle().ToString();
	// InKeyEvent.GetPlatformUserId().Get();

	const FString LogStr = FString::Printf(
		TEXT("Key: %s\nShift: %s, Ctrl: %s, Alt: %s, Cmd: %s\nCharCode: %d, KeyCode: %d\nUserIndex: %d\nIsRepeat: %s\nIsPointerEvent: %s\nEventPath: %s\nInputDeviceID: %d\nIsNativeInputHandling: %s\nCurrent Sequence Num: %d"),
		*InKeyEvent.GetKey().ToString(),
		InKeyEvent.IsShiftDown() ? TEXT("true") : TEXT("false"),
		InKeyEvent.IsControlDown() ? TEXT("true") : TEXT("false"),
		InKeyEvent.IsAltDown() ? TEXT("true") : TEXT("false"),
		InKeyEvent.IsCommandDown() ? TEXT("true") : TEXT("false"),
		InKeyEvent.GetCharacter(),
		InKeyEvent.GetKeyCode(),
		InKeyEvent.GetUserIndex(),
		InKeyEvent.IsRepeat() ? TEXT("true") : TEXT("false"),
		InKeyEvent.IsPointerEvent() ? TEXT("true") : TEXT("false"),
		*EventPath,
		InKeyEvent.GetInputDeviceId().GetId(),
		bNativeInputHandling ? TEXT("true") : TEXT("false"),
		CurrentSequence.Num());

	Logger.Print(LogStr, true);
}
