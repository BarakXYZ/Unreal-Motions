#include "UMInputPreProcessor.h"
#include "Editor.h"
#include "Engine/GameInstance.h"
#include "Framework/Application/SlateApplication.h"
#include "StatusBarSubsystem.h"

// #include "WidgetDrawerConfig.h"

DEFINE_LOG_CATEGORY_STATIC(LogUMInputPreProcessor, Log, All); // Dev
//
TSharedPtr<FUMInputPreProcessor>
	FUMInputPreProcessor::InputPreProcessor{ nullptr };

EVimMode FUMInputPreProcessor::VimMode{ EVimMode::Insert };

FOnUMPreProcessorInputInit FUMInputPreProcessor::OnUMPreProcessorInputInit;

FOnRequestVimModeChange FUMInputPreProcessor::OnRequestVimModeChange;

bool FUMInputPreProcessor::bNativeInputHandling{ false };

FUMInputPreProcessor::FUMInputPreProcessor()
{
	Logger = FUMLogger(&LogUMInputPreProcessor);

	OnUMPreProcessorInputInit.AddLambda(
		[this]() { RegisterDefaultKeyBindings(); });

	OnRequestVimModeChange.AddLambda(
		[this](FSlateApplication& SlateApp, const EVimMode NewMode) {
			SetMode(SlateApp, NewMode);
		});
}
FUMInputPreProcessor::~FUMInputPreProcessor()
{
}

void FUMInputPreProcessor::Initialize()
{
	if (!InputPreProcessor)
	{
		InputPreProcessor = MakeShared<FUMInputPreProcessor>();
		OnUMPreProcessorInputInit.Broadcast();
	}
}

bool FUMInputPreProcessor::IsInitialized()
{
	return InputPreProcessor.IsValid();
}

TSharedPtr<FUMInputPreProcessor> FUMInputPreProcessor::Get()
{
	// Always ensure initialization first
	Initialize();
	return InputPreProcessor;
}

void FUMInputPreProcessor::Tick(
	const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor)
{
	// if (TSharedPtr<SWidget> FocusedWidget = SlateApp.GetUserFocusedWidget(0))
	// 	FUMLogger::AddDebugMessage(FocusedWidget->GetTypeAsString());
}

// Process the current key sequence
bool FUMInputPreProcessor::ProcessKeySequence(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	// 1) Add this key to current sequence
	CurrentSequence.Add(GetChordFromKeyEvent(InKeyEvent));

	// 2) Traverse the trie
	FKeyChordTrieNode* CurrentNode = TrieRoot;
	for (const FInputChord& SequenceKey : CurrentSequence)
	{
		if (!CurrentNode->Children.Contains(SequenceKey))
		{
			// No match found
			ResetSequence(SlateApp);
			return false;
		}
		CurrentNode = CurrentNode->Children[SequenceKey];
	}

	// 3) Found a node, check for a callback
	if (CurrentNode->CallbackType != EUMKeyBindingCallbackType::None)
	{
		switch (CurrentNode->CallbackType)
		{
			case EUMKeyBindingCallbackType::NoParam:
				if (CurrentNode->NoParamCallback)
					CurrentNode->NoParamCallback();
				break;

			case EUMKeyBindingCallbackType::KeyEventParam:
				if (CurrentNode->KeyEventCallback)
					CurrentNode->KeyEventCallback(SlateApp, InKeyEvent);
				break;

			case EUMKeyBindingCallbackType::SequenceParam:
				if (CurrentNode->SequenceCallback)
					CurrentNode->SequenceCallback(SlateApp, CurrentSequence);
				break;

			default:
				break;
		}

		// 4) Reset sequence after invoking the callback
		ResetSequence(SlateApp);
		return true;
	}

	// Partial match is still valid; do not reset
	return true;
}

void FUMInputPreProcessor::ResetSequence(FSlateApplication& SlateApp)
{
	OnResetSequence.Broadcast();
	CurrentSequence.Empty();
	CurrentBuffer.Empty();
	bIsCounting = false;
	ResetBufferVisualizer(SlateApp);
	// FUMLogger::NotifySuccess(FText::FromString("Reset Sequence"));
}

void FUMInputPreProcessor::ResetBufferVisualizer(FSlateApplication& SlateApp)
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

FKeyChordTrieNode* FUMInputPreProcessor::FindOrCreateTrieNode(
	const TArray<FInputChord>& Sequence)
{
	FKeyChordTrieNode* Current = TrieRoot;
	for (const FInputChord& Key : Sequence)
	{
		if (!Current->Children.Contains(Key))
		{
			Current->Children.Add(Key, new FKeyChordTrieNode());
		}
		Current = Current->Children[Key];
	}
	return Current;
}

// 1) No-Parameter
void FUMInputPreProcessor::AddKeyBinding_NoParam(
	const TArray<FInputChord>& Sequence,
	TFunction<void()>		   Callback)
{
	FKeyChordTrieNode* Node = FindOrCreateTrieNode(Sequence);
	Node->NoParamCallback = MoveTemp(Callback);
	Node->CallbackType = EUMKeyBindingCallbackType::NoParam;
}

// 2) Single FKeyEvent param
void FUMInputPreProcessor::AddKeyBinding_KeyEvent(
	const TArray<FInputChord>&									   Sequence,
	TFunction<void(FSlateApplication& SlateApp, const FKeyEvent&)> Callback)
{
	FKeyChordTrieNode* Node = FindOrCreateTrieNode(Sequence);
	Node->KeyEventCallback = MoveTemp(Callback);
	Node->CallbackType = EUMKeyBindingCallbackType::KeyEventParam;
}

// 3) TArray<FInputChord> param
void FUMInputPreProcessor::AddKeyBinding_Sequence(
	const TArray<FInputChord>&												 Sequence,
	TFunction<void(FSlateApplication& SlateApp, const TArray<FInputChord>&)> Callback)
{
	FKeyChordTrieNode* Node = FindOrCreateTrieNode(Sequence);
	Node->SequenceCallback = MoveTemp(Callback);
	Node->CallbackType = EUMKeyBindingCallbackType::SequenceParam;
}

void FUMInputPreProcessor::RegisterDefaultKeyBindings()
{
	// Create the root node
	TrieRoot = new FKeyChordTrieNode();
	// WeakInputPreProcessor = InputPreProcessor.ToWeakPtr();

	AddKeyBinding_KeyEvent(
		{ EKeys::I },
		[this](FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) {
			SwitchVimModes(SlateApp, InKeyEvent);
		});

	AddKeyBinding_KeyEvent(
		{ FInputChord(EModifierKey::Shift, EKeys::V) },
		[this](FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) {
			SwitchVimModes(SlateApp, InKeyEvent);
		});
	AddKeyBinding_KeyEvent(
		{ EKeys::V },
		[this](FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) {
			SwitchVimModes(SlateApp, InKeyEvent);
		});
}

void FUMInputPreProcessor::TestLinearInput(FSlateApplication& SlateApp)
{
	TSharedRef<FTimerManager> TimerManager = GEditor->GetTimerManager();

	TimerManager->SetTimer(
		TimerHandle_LinearPress,
		[this, &SlateApp]() {
			Logger.Print("LINEAR INPUT", ELogVerbosity::Warning);
			// TestLinearInput(SlateApp);
		},
		0.02f, true);
}

bool FUMInputPreProcessor::HandleKeyDownEvent(
	FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	// TEST Linear Input:
	// if (InKeyEvent.GetKey() == EKeys::LeftBracket)
	// {
	// 	TestLinearInput(SlateApp);
	// }

	// return false;
	// DebugKeyEvent(InKeyEvent);

	// NOTE:
	// When we call SlateApp.ProcessKeyDownEvent(), it will trigger another
	// HandleKeyDownEvent call.
	// We need to return false during that second pass to let Unreal Engine
	// process the simulated key event.
	// Otherwise, the simulated key press won't be handled by the engine.
	if (bNativeInputHandling)
	{
		// FUMLogger::NotifySuccess(FText::FromString(
		// 	"HandleKeyDownEvent Dummy"));

		bNativeInputHandling = false; // Resume manual handling from next event
		return false;
	}

	// FUMLogger::NotifySuccess(FText::FromString("HandleKeyDownEvent Prod"));
	if (IsSimulateEscapeKey(SlateApp, InKeyEvent))
		return true;

	if (ShouldSwitchVimMode(SlateApp, InKeyEvent)) // Return true and reset
		return true;

	if (VimMode == EVimMode::Insert) // Default
		return false;				 // Pass on the handling to Unreal native.

	if (TrackCountPrefix(SlateApp, InKeyEvent)) // Return if currently counting.
		return true;

	FKey KeyPressed = InKeyEvent.GetKey();

	if (KeyPressed.IsModifierKey()) // Simply ignore
		return true;				// or false?

	// If leader key is pressed, show the buffer visualizer
	if (KeyPressed == EKeys::SpaceBar && CurrentBuffer.IsEmpty())
	{
		CurrentBuffer = ""; // Start a new buffer
		if (!BufferVisualizer.IsValid())
		{
			TSharedPtr<SWindow> ActiveWindow = SlateApp.GetActiveTopLevelWindow();
			if (ActiveWindow.IsValid())
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

	// Update buffer and visualizer
	if (CurrentBuffer.IsEmpty())
		CurrentBuffer = KeyPressed.GetDisplayName().ToString();
	else
		CurrentBuffer += " + " + KeyPressed.GetDisplayName().ToString();

	if (BufferVisualizer.IsValid())
	{
		BufferVisualizer.Pin()->UpdateBuffer(CurrentBuffer);
	}

	return ProcessKeySequence(SlateApp, InKeyEvent);
	// return true;
}

// TODO: Check this behavior
// I think we must return up for the keys we're simulating!
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
bool FUMInputPreProcessor::HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	// TEST Linear Input:
	// if (InKeyEvent.GetKey() == EKeys::LeftBracket)
	// {
	// 	GEditor->GetTimerManager()->ClearTimer(TimerHandle_LinearPress);
	// }

	OnMouseButtonUpAlertTabForeground.Broadcast();
	// FUMLogger::NotifySuccess(FText::FromString("Key up!"));
	// return true;

	// if (VimMode == EVimMode::Normal)
	// 	return true; // ??  Not sure

	return false; // Seems to work fine for now.
}

bool FUMInputPreProcessor::ShouldSwitchVimMode(
	FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		SetMode(SlateApp, EVimMode::Normal);
		ResetSequence(SlateApp);
		return true;
	}
	// else if(InKeyEvent.GetKey() == EKeys::I && )
	return false;
}

bool FUMInputPreProcessor::HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
{
	// return false;
	TSharedPtr<SWidget> FocusedWidget = SlateApp.GetUserFocusedWidget(0);
	if (FocusedWidget.IsValid())
	{
		const FSlateWidgetClassData ClassData = FocusedWidget->GetWidgetClass();
		const FName					WidgetType = ClassData.GetWidgetType();
		FSlateAttributeDescriptor	AttDesc = ClassData.GetAttributeDescriptor();
		// AttDesc.DefaultSortOrder();
		FString WidgetName = FocusedWidget->ToString();
		FString DebugStr = FString::Printf(TEXT("Widget Type: %s, Widget Name: %s"), *WidgetType.ToString(), *WidgetName);
		// FUMLogger::NotifySuccess(FText::FromString(DebugStr));

		return false;
	}
	return false;
}

bool FUMInputPreProcessor::TrackCountPrefix(
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
	if (GetStrDigitFromKey(InKeyEvent.GetKey(), OutStr))
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

void FUMInputPreProcessor::SwitchVimModes(
	FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	const FKey& KeyPressed = InKeyEvent.GetKey();

	if (KeyPressed == EKeys::Escape)
		SetMode(SlateApp, EVimMode::Normal);

	if (VimMode == EVimMode::Normal)
	{
		if (KeyPressed == EKeys::I)
			SetMode(SlateApp, EVimMode::Insert);
		if (KeyPressed == EKeys::V)
			SetMode(SlateApp, EVimMode::Visual);
	}
}

bool FUMInputPreProcessor::IsSimulateEscapeKey(
	FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.IsShiftDown() && InKeyEvent.GetKey() == EKeys::Escape)
	{
		FUMLogger::NotifySuccess(FText::FromString("Simulate Escape"));
		static const FKeyEvent EscapeEvent(
			FKey(EKeys::Escape),
			FModifierKeysState(),
			0, 0, 0, 0);

		bNativeInputHandling = true;
		SlateApp.ProcessKeyDownEvent(EscapeEvent);
		return true;
	}
	return false;
}

void FUMInputPreProcessor::SetMode(FSlateApplication& SlateApp, const EVimMode NewMode)
{
	if (VimMode != NewMode)
	{
		VimMode = NewMode;
		FUMLogger::NotifySuccess(
			FText::FromString(UEnum::GetValueAsString(NewMode)), bVisualLog);
	}
	ResetSequence(SlateApp);
	OnVimModeChanged.Broadcast(NewMode);
}

FOnVimModeChanged& FUMInputPreProcessor::GetOnVimModeChanged()
{
	return OnVimModeChanged;
}

void FUMInputPreProcessor::RegisterOnVimModeChanged(TFunction<void(const EVimMode)> Callback)
{
	OnVimModeChanged.AddLambda(Callback);
}

void FUMInputPreProcessor::UnregisterOnVimModeChanged(const void* CallbackOwner)
{
	OnVimModeChanged.RemoveAll(CallbackOwner);
}

FKeyEvent FUMInputPreProcessor::GetKeyEventFromKey(
	const FKey& InKey, bool bIsShiftDown)
{
	const FModifierKeysState ModKeys(bIsShiftDown, bIsShiftDown,
		false, false, false, false, false, false, false);

	return FKeyEvent(
		InKey,
		ModKeys,
		0, false, 0, 0);
}

FInputChord FUMInputPreProcessor::GetChordFromKeyEvent(
	const FKeyEvent& InKeyEvent)
{
	const FModifierKeysState& ModState = InKeyEvent.GetModifierKeys();
	return FInputChord(
		InKeyEvent.GetKey(),	  // The key
		ModState.IsShiftDown(),	  // bShift
		ModState.IsControlDown(), // bCtrl
		ModState.IsAltDown(),	  // bAlt
		ModState.IsCommandDown()  // bCmd
	);
}

void FUMInputPreProcessor::SimulateMultiTabPresses(
	FSlateApplication& SlateApp, int32 TimesToRepeat)
{
	FUMLogger::NotifySuccess();
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

void FUMInputPreProcessor::ToggleNativeInputHandling(const bool bNativeHandling)
{
	bNativeInputHandling = bNativeHandling;
}

void FUMInputPreProcessor::SimulateKeyPress(
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

bool FUMInputPreProcessor::GetStrDigitFromKey(const FKey& InKey, FString& OutStr,
	int32 MinClamp, int32 MaxClamp)
{
	static const TMap<FKey, FString> KeyToStringDigits = {
		{ EKeys::Zero, TEXT("0") },
		{ EKeys::One, TEXT("1") },
		{ EKeys::Two, TEXT("2") },
		{ EKeys::Three, TEXT("3") },
		{ EKeys::Four, TEXT("4") },
		{ EKeys::Five, TEXT("5") },
		{ EKeys::Six, TEXT("6") },
		{ EKeys::Seven, TEXT("7") },
		{ EKeys::Eight, TEXT("8") },
		{ EKeys::Nine, TEXT("9") },
	};

	const FString* FoundStr = KeyToStringDigits.Find(InKey);
	if (!FoundStr)
	{
		return false;
	}

	// Convert found string to number for clamping
	int32 NumValue = FCString::Atoi(**FoundStr);

	// Apply clamping if specified
	if (MinClamp > 0 || MaxClamp > 0)
	{
		// If only min is specified, use the found number as max
		const int32 EffectiveMax = (MaxClamp > 0) ? MaxClamp : NumValue;
		// If only max is specified, use 0 as min
		const int32 EffectiveMin = (MinClamp > 0) ? MinClamp : 0;

		NumValue = FMath::Clamp(NumValue, EffectiveMin, EffectiveMax);
	}

	// Convert back to string
	OutStr = FString::FromInt(NumValue);
	return true;
}

void FUMInputPreProcessor::DebugInvalidWeakPtr(EUMKeyBindingCallbackType CallbackType)
{
	switch (CallbackType)
	{
		case EUMKeyBindingCallbackType::None:
			FUMLogger::NotifySuccess(
				FText::FromString("Invalid Weakptr _None"));
			break;
		case EUMKeyBindingCallbackType::NoParam:
			FUMLogger::NotifySuccess(
				FText::FromString("Invalid Weakptr _NoParam"));
			break;
		case EUMKeyBindingCallbackType::KeyEventParam:
			FUMLogger::NotifySuccess(
				FText::FromString("Invalid Weakptr _KeyEvent"));
			break;
		case EUMKeyBindingCallbackType::SequenceParam:
			FUMLogger::NotifySuccess(
				FText::FromString("Invalid Weakptr _SequenceParam"));
			break;
	}
}

void FUMInputPreProcessor::DebugKeyEvent(const FKeyEvent& InKeyEvent)
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

	FUMLogger::NotifySuccess(FText::FromString(LogStr));
	UE_LOG(LogTemp, Warning, TEXT("%s"), *LogStr);
}
