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

TSharedPtr<FKeyChordTrieNode> FVimInputProcessor::GetOrCreateTrieRoot(
	EUMBindingContext Context)
{
	// Try to find an existing root
	if (TSharedPtr<FKeyChordTrieNode>* FoundRoot = ContextTrieRoots.Find(Context))
	{
		return *FoundRoot;
	}

	// Otherwise, create a new one
	TSharedPtr<FKeyChordTrieNode> NewRoot = MakeShared<FKeyChordTrieNode>();
	ContextTrieRoots.Add(Context, NewRoot);
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

bool FVimInputProcessor::TraverseTrieForContext(EUMBindingContext InContext, TSharedPtr<FKeyChordTrieNode>& OutNode) const
{
	OutNode = nullptr;

	// If there's no trie root for this context yet, no match is possible
	if (!ContextTrieRoots.Contains(InContext))
	{
		return false;
	}

	// Get the root node of this specified context
	TSharedPtr<FKeyChordTrieNode> Root = ContextTrieRoots[InContext];
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

// Process the current key sequence
bool FVimInputProcessor::ProcessKeySequence(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	// 1) Add this key to current sequence
	CurrentSequence.Add(FUMInputHelpers::GetChordFromKeyEvent(InKeyEvent));

	// 2) Try to traverse the trie for the current context first
	TSharedPtr<FKeyChordTrieNode> MatchedNode;
	bool						  bContextHasPartialOrFull = TraverseTrieForContext(CurrentContext, MatchedNode);

	// 3) If that fails, fallback to Generic
	bool bUsedGeneric = false;
	if (!bContextHasPartialOrFull)
	{
		if (CurrentContext != EUMBindingContext::Generic)
		{
			bUsedGeneric = TraverseTrieForContext(EUMBindingContext::Generic, MatchedNode);
		}

		// If neither the current context nor the Generic context recognized it (even as partial), reset
		if (!bUsedGeneric)
		{
			ResetSequence(SlateApp);
			return false;
		}
	}

	// 4) If we have a matched node, see if it has a callback
	if (MatchedNode && MatchedNode->CallbackType != EUMKeyBindingCallbackType::None)
	{
		switch (MatchedNode->CallbackType)
		{
			case EUMKeyBindingCallbackType::NoParam:
				if (MatchedNode->NoParamCallback)
				{
					MatchedNode->NoParamCallback();
				}
				break;

			case EUMKeyBindingCallbackType::KeyEventParam:
				if (MatchedNode->KeyEventCallback)
				{
					MatchedNode->KeyEventCallback(SlateApp, InKeyEvent);
				}
				break;

			case EUMKeyBindingCallbackType::SequenceParam:
				if (MatchedNode->SequenceCallback)
				{
					MatchedNode->SequenceCallback(SlateApp, CurrentSequence);
				}
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
	OnResetSequence.Broadcast();
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
	TFunction<void()>		   Callback)
{
	TSharedPtr<FKeyChordTrieNode> Root = GetOrCreateTrieRoot(Context);
	TSharedPtr<FKeyChordTrieNode> Node = FindOrCreateTrieNode(Root, Sequence);

	Node->NoParamCallback = MoveTemp(Callback);
	Node->CallbackType = EUMKeyBindingCallbackType::NoParam;
}

// 2) Single FKeyEvent param
void FVimInputProcessor::AddKeyBinding_KeyEvent(
	EUMBindingContext											   Context,
	const TArray<FInputChord>&									   Sequence,
	TFunction<void(FSlateApplication& SlateApp, const FKeyEvent&)> Callback)
{
	TSharedPtr<FKeyChordTrieNode> Root = GetOrCreateTrieRoot(Context);
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
		Callback)
{
	TSharedPtr<FKeyChordTrieNode> Root = GetOrCreateTrieRoot(Context);
	TSharedPtr<FKeyChordTrieNode> Node = FindOrCreateTrieNode(Root, Sequence);

	Node->SequenceCallback = MoveTemp(Callback);
	Node->CallbackType = EUMKeyBindingCallbackType::SequenceParam;
}

void FVimInputProcessor::RegisterDefaultKeyBindings()
{
	// For convenience, ensure the Generic root exists
	GetOrCreateTrieRoot(EUMBindingContext::Generic);

	// Example default key bindings in Generic:
	AddKeyBinding_KeyEvent(
		EUMBindingContext::Generic,
		{ EKeys::I },
		[this](FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) {
			SwitchVimModes(SlateApp, InKeyEvent);
		});

	AddKeyBinding_KeyEvent(
		EUMBindingContext::Generic,
		{ FInputChord(EModifierKey::Shift, EKeys::V) },
		[this](FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) {
			SwitchVimModes(SlateApp, InKeyEvent);
		});

	AddKeyBinding_KeyEvent(
		EUMBindingContext::Generic,
		{ EKeys::V },
		[this](FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) {
			SwitchVimModes(SlateApp, InKeyEvent);
		});
}

bool FVimInputProcessor::HandleKeyDownEvent(
	FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	// DebugKeyEvent(InKeyEvent);

	// NOTE:
	// When we call SlateApp.ProcessKeyDownEvent(), it will trigger another
	// HandleKeyDownEvent call.
	// We need to return false during that second pass to let Unreal Engine
	// process the simulated key event.
	// Otherwise, the simulated key press won't be handled by the engine.
	if (bNativeInputHandling)
	{
		bNativeInputHandling = false; // Resume manual handling from next event
		return false;
	}

	// We give an exception to the Escape key to be handled at this stage.
	if (IsSimulateEscapeKey(SlateApp, InKeyEvent))
		return true;

	// Should handle here if possessed as we want to handle generic inputs
	// that may collide with other bindings (i.e. I for Insert, etc.);
	if (Delegate_OnKeyDown.IsBound())
	{
		Delegate_OnKeyDown.Broadcast(SlateApp, InKeyEvent);
		return true;
	}

	if (ShouldSwitchVimMode(SlateApp, InKeyEvent)) // Return true and reset
		return true;

	if (VimMode == EVimMode::Insert)
		return false; // Default: Pass on the handling to Unreal native.

	if (TrackCountPrefix(SlateApp, InKeyEvent)) // Return if currently counting.
		return true;

	const FKey InKey = InKeyEvent.GetKey();

	if (InKey.IsModifierKey()) // Simply ignore
		return true;		   // or false?

	// TODO: Add a small timer until actually showing the buffer
	// and retrigger the timer upon keystrokes
	CheckCreateBufferVisualizer(SlateApp, InKey);
	UpdateBufferAndVisualizer(InKey);

	return ProcessKeySequence(SlateApp, InKeyEvent);
	// return true;
}

// TODO:
// Check this behavior; I think we must return up for the keys we're simulating!
// That make sense, they essentially never return to be unpressed!
// We're sending their input to Unreal native (for example, arrow)
// but never actually sending the key-up, which makes them stay
// down and continuously press(?) need to check.
// We should also keep in mind that when trying ot navigate the viewport
// when Normal mode is on we can't expend smooth traveling because we will have
// hotkeys (like 'D') that are mapped to commands, which will block the navigation
// So really navigation only can happen smoothly while in Insert mode.
// Need to think about this more.
// We should about giving navigation using different method so let's think about
// that more. Maybe a specific mode desgined for navigation? TODO WIP OK THANK
bool FVimInputProcessor::HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	OnMouseButtonUpAlertTabForeground.Broadcast(); // Deprecated?

	Delegate_OnKeyUpEvent.Broadcast(SlateApp, InKeyEvent);
	// Logger.Print("Key up!");
	// return true;

	// if (VimMode == EVimMode::Normal)
	// 	return true; // ??  Not sure

	return false; // Seems to work fine for now.
}

bool FVimInputProcessor::ShouldSwitchVimMode(
	FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		SetVimMode(SlateApp, EVimMode::Normal);
		ResetSequence(SlateApp);
		return true;
	}
	// else if(InKeyEvent.GetKey() == EKeys::I && )
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

	// 	return false;
	// }
	return false;
}

bool FVimInputProcessor::HandleMouseButtonUpEvent(
	FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
{
	// Logger.Print("Mouse Button Up!", ELogVerbosity::Log, true);
	return false;
}

bool FVimInputProcessor::HandleMouseMoveEvent(
	FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
{
	// Logger.Print("Mouse Button Move!", ELogVerbosity::Log, true);
	return false;
}

bool FVimInputProcessor::TrackCountPrefix(
	FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
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

void FVimInputProcessor::SwitchVimModes(
	FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
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

bool FVimInputProcessor::IsSimulateEscapeKey(
	FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.IsShiftDown() && InKeyEvent.GetKey() == EKeys::Escape)
	{
		static const FKey Escape = EKeys::Escape;
		SimulateKeyPress(SlateApp, Escape);

		Logger.Print("Simulate Escape");
		return true;
	}
	return false;
}

void FVimInputProcessor::SetVimMode(FSlateApplication& SlateApp, const EVimMode NewMode)
{
	if (VimMode != NewMode)
	{
		VimMode = NewMode;
		Logger.Print(UEnum::GetValueAsString(NewMode));
	}
	ResetSequence(SlateApp);
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

FKeyEvent FVimInputProcessor::GetKeyEventFromKey(
	const FKey& InKey, bool bIsShiftDown)
{
	const FModifierKeysState ModKeys(bIsShiftDown, bIsShiftDown,
		false, false, false, false, false, false, false);

	return FKeyEvent(
		InKey,
		ModKeys,
		0, false, 0, 0);
}

void FVimInputProcessor::SimulateMultiTabPresses(
	FSlateApplication& SlateApp, int32 TimesToRepeat)
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
	const FModifierKeysState& ModifierKeys)
{
	const FKeyEvent SimulatedEvent(
		SimulatedKey,
		ModifierKeys,
		0, 0, 0, 0);

	bNativeInputHandling = true;
	SlateApp.ProcessKeyDownEvent(SimulatedEvent);
	SlateApp.ProcessKeyUpEvent(SimulatedEvent);
}

void FVimInputProcessor::CheckCreateBufferVisualizer(
	FSlateApplication& SlateApp, const FKey& InKey)
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

template <typename UserClass>
void FVimInputProcessor::Possess(UserClass* InObject, void (UserClass::*InMethod)(FSlateApplication&, const FKeyEvent&))
{
	if (!PossessedObjects.Contains(InObject))
	{
		// Store the binding so it can be unbound later
		auto Binding = Delegate_OnKeyDown.AddUObject(InObject, InMethod);

		// Save the binding handle for later unbinding
		PossessedObjects.Add(InObject, Binding);
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
		TEXT("Key: %s, Shift: %s, Ctrl: %s, Alt: %s, Cmd: %s, CharCode: %d, KeyCode: %d, UserIndex: %d, IsRepeat: %s, IsPointerEvent: %s, EventPath: %s, InputDeviceID: %d"),
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
		InKeyEvent.GetInputDeviceId().GetId());
}
