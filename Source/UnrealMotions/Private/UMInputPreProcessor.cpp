#include "UMInputPreProcessor.h"
#include "Editor.h"
#include "Engine/GameInstance.h"
#include "UMHelpers.h"
#include "StatusBarSubsystem.h"
// #include "WidgetDrawerConfig.h"

TSharedPtr<FUMInputPreProcessor>
	FUMInputPreProcessor::InputPreProcessor = nullptr;

FOnUMPreProcessorInputInit FUMInputPreProcessor::OnUMPreProcessorInputInit;

bool FUMInputPreProcessor::bNativeInputHandling = { false };

FUMInputPreProcessor::FUMInputPreProcessor()
{
	OnUMPreProcessorInputInit.AddLambda(
		[this]() { InitializeKeyBindings(); });
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
}

// Process the current key sequence
bool FUMInputPreProcessor::ProcessKeySequence(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	// 1) Add this key to current sequence
	CurrentSequence.Add(GetChordFromKeyEvent(InKeyEvent));

	// 2) Traverse the trie
	FTrieNode* CurrentNode = TrieRoot;
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
	if (CurrentNode->CallbackType != EUMCallbackType::None)
	{
		switch (CurrentNode->CallbackType)
		{
			case EUMCallbackType::NoParam:
				if (CurrentNode->NoParamCallback)
					CurrentNode->NoParamCallback();
				break;

			case EUMCallbackType::KeyEventParam:
				if (CurrentNode->KeyEventCallback)
					CurrentNode->KeyEventCallback(SlateApp, InKeyEvent);
				break;

			case EUMCallbackType::SequenceParam:
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

// Reset the input sequence
void FUMInputPreProcessor::ResetSequence(FSlateApplication& SlateApp)
{
	OnResetSequence.Broadcast();
	CurrentSequence.Empty();
	CurrentBuffer.Empty();
	ResetBufferVisualizer(SlateApp);
	// FUMHelpers::NotifySuccess(FText::FromString("Reset Sequence"));
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

FTrieNode* FUMInputPreProcessor::FindOrCreateTrieNode(
	const TArray<FInputChord>& Sequence)
{
	FTrieNode* Current = TrieRoot;
	for (const FInputChord& Key : Sequence)
	{
		if (!Current->Children.Contains(Key))
		{
			Current->Children.Add(Key, new FTrieNode());
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
	FTrieNode* Node = FindOrCreateTrieNode(Sequence);
	Node->NoParamCallback = MoveTemp(Callback);
	Node->CallbackType = EUMCallbackType::NoParam;
}

// 2) Single FKeyEvent param
void FUMInputPreProcessor::AddKeyBinding_KeyEvent(
	const TArray<FInputChord>&									   Sequence,
	TFunction<void(FSlateApplication& SlateApp, const FKeyEvent&)> Callback)
{
	FTrieNode* Node = FindOrCreateTrieNode(Sequence);
	Node->KeyEventCallback = MoveTemp(Callback);
	Node->CallbackType = EUMCallbackType::KeyEventParam;
}

// 3) TArray<FInputChord> param
void FUMInputPreProcessor::AddKeyBinding_Sequence(
	const TArray<FInputChord>&												 Sequence,
	TFunction<void(FSlateApplication& SlateApp, const TArray<FInputChord>&)> Callback)
{
	FTrieNode* Node = FindOrCreateTrieNode(Sequence);
	Node->SequenceCallback = MoveTemp(Callback);
	Node->CallbackType = EUMCallbackType::SequenceParam;
}

// Initialize Key Bindings in the Trie
void FUMInputPreProcessor::InitializeKeyBindings()
{
	// Create the root node
	TrieRoot = new FTrieNode();
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
}

bool FUMInputPreProcessor::HandleKeyDownEvent(
	FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	// NOTE:
	// When we call SlateApp.ProcessKeyDownEvent(), it will trigger another
	// HandleKeyDownEvent call.
	// We need to return false during that second pass to let Unreal Engine
	// process the simulated key event.
	// Otherwise, the simulated key press won't be handled by the engine.
	if (bNativeInputHandling)
	{
		// FUMHelpers::NotifySuccess(FText::FromString(
		// 	"HandleKeyDownEvent Dummy"));

		bNativeInputHandling = false; // Resume manual handling from next event
		// DebugKeyEvent(InKeyEvent);
		return false;
	}

	// FUMHelpers::NotifySuccess(FText::FromString("HandleKeyDownEvent Prod"));
	if (IsSimulateEscapeKey(SlateApp, InKeyEvent))
		return true;

	if (IsVimSwitch(InKeyEvent)) // On switching modes, return true
		return true;

	if (VimMode == EVimMode::Insert) // Default
		return false;				 // Pass on the handling to Unreal native.

	if (TrackCountPrefix(InKeyEvent)) // User is currently counting, return.
		return true;

	FKey KeyPressed = InKeyEvent.GetKey();
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

	if (KeyPressed.IsModifierKey()) // Simply ignore
		return true;				// or false?

	// if (VimMode == EVimMode::Visual)
	// {
	// 	// FUMHelpers::NotifySuccess(FText::FromString("Visual Process"));
	// 	FModifierKeysState VisualModKeysState(
	// 		true, true, // Always shift
	// 		false, false, false, false, false, false, false);
	// 	FKeyEvent VisualProcessesdKeyEvent(
	// 		InKeyEvent.GetKey(),
	// 		VisualModKeysState,
	// 		InKeyEvent.GetUserIndex(),
	// 		InKeyEvent.IsRepeat(),
	// 		InKeyEvent.GetCharacter(),
	// 		InKeyEvent.GetKeyCode());
	// 	return ProcessKeySequence(SlateApp, VisualProcessesdKeyEvent);
	// 	// return true;
	// }

	return ProcessKeySequence(SlateApp, InKeyEvent);
	// return true;
}

bool FUMInputPreProcessor::IsVimSwitch(const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		SetMode(EVimMode::Normal);
		return true;
	}
	return false;
}

bool FUMInputPreProcessor::TrackCountPrefix(const FKeyEvent& InKeyEvent)
{
	FString OutStr;
	if (!GetStrDigitFromKey(InKeyEvent.GetKey(), OutStr))
		return false;
	OnCountPrefix.Broadcast(OutStr); // Build the buffer: 1 + 7 + 3 (173);
	return true;
}

void FUMInputPreProcessor::SwitchVimModes(
	FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	const FKey& KeyPressed = InKeyEvent.GetKey();

	if (KeyPressed == EKeys::Escape)
		SetMode(EVimMode::Normal);

	if (VimMode == EVimMode::Normal)
	{
		if (KeyPressed == EKeys::I)
			SetMode(EVimMode::Insert);
		if (KeyPressed == EKeys::V)
			SetMode(EVimMode::Visual);
	}
}

bool FUMInputPreProcessor::IsSimulateEscapeKey(
	FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.IsShiftDown() && InKeyEvent.GetKey() == EKeys::Escape)
	{
		FUMHelpers::NotifySuccess(FText::FromString("Simulate Escape"));
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

void FUMInputPreProcessor::SetMode(EVimMode NewMode)
{
	if (VimMode != NewMode)
	{
		VimMode = NewMode;
		FUMHelpers::NotifySuccess(
			FText::FromString(UEnum::GetValueAsString(NewMode)), bVisualLog);
	}
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

void FUMInputPreProcessor::SimulateMultipleTabEvent(
	FSlateApplication& SlateApp, int32 TimesToRepeat)
{
	FUMHelpers::NotifySuccess();
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

bool FUMInputPreProcessor::HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
{
	// TSharedPtr<SWidget> FocusedWidget = SlateApp.GetUserFocusedWidget(0);
	// if (FocusedWidget.IsValid())
	// {
	// 	FString WidgetName = FocusedWidget->ToString();
	// 	FUMHelpers::NotifySuccess(FText::FromString(WidgetName));
	// 	return false;
	// }
	return false;
}

bool FUMInputPreProcessor::GetStrDigitFromKey(const FKey& InKey, FString& OutStr)
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
	if (FoundStr)
	{
		OutStr = *FoundStr;
		return true;
	}
	return false;
}

void FUMInputPreProcessor::DebugInvalidWeakPtr(EUMCallbackType CallbackType)
{
	switch (CallbackType)
	{
		case EUMCallbackType::None:
			FUMHelpers::NotifySuccess(
				FText::FromString("Invalid Weakptr _None"));
			break;
		case EUMCallbackType::NoParam:
			FUMHelpers::NotifySuccess(
				FText::FromString("Invalid Weakptr _NoParam"));
			break;
		case EUMCallbackType::KeyEventParam:
			FUMHelpers::NotifySuccess(
				FText::FromString("Invalid Weakptr _KeyEvent"));
			break;
		case EUMCallbackType::SequenceParam:
			FUMHelpers::NotifySuccess(
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

	FUMHelpers::NotifySuccess(FText::FromString(LogStr));
	UE_LOG(LogTemp, Warning, TEXT("%s"), *LogStr);
}
