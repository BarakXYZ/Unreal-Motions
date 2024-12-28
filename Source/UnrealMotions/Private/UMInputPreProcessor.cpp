#include "UMInputPreProcessor.h"
#include "UMHelpers.h"
#include "StatusBarSubsystem.h"
#include "Editor.h"
#include "UObject/UnrealNames.h"
// #include "WidgetDrawerConfig.h"

TSharedPtr<FUMInputPreProcessor>
	FUMInputPreProcessor::InputPreProcessor = nullptr;

FUMInputPreProcessor::FUMInputPreProcessor()
{
	InitializeKeyBindings();
}
FUMInputPreProcessor::~FUMInputPreProcessor()
{
}

void FUMInputPreProcessor::Initialize()
{
	if (!InputPreProcessor)
	{
		InputPreProcessor = MakeShared<FUMInputPreProcessor>();
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
bool FUMInputPreProcessor::ProcessKeySequence(const FKey& Key)
{
	// Add the key to the current sequence
	CurrentSequence.Add(Key);

	// Traverse the trie to check for matches
	FTrieNode* CurrentNode = TrieRoot;
	for (const FKey& SequenceKey : CurrentSequence)
	{
		if (!CurrentNode->Children.Contains(SequenceKey))
		{
			// No match found; reset sequence
			ResetSequence();
			return false;
		}
		CurrentNode = CurrentNode->Children[SequenceKey];
	}

	// If a callback exists at this node, execute it
	if (CurrentNode->Callback)
	{
		CurrentNode->Callback();
		ResetSequence(); // Reset the sequence after executing the callback
		return true;
	}

	// Allow partial matches
	return true;
}

// Reset the input sequence
void FUMInputPreProcessor::ResetSequence()
{
	CurrentSequence.Empty();
	CurrentBuffer.Empty();
	if (BufferVisualizer.IsValid())
	{
		TSharedPtr<SWindow> ParentWindow =
			FSlateApplication::Get().FindWidgetWindow(
				BufferVisualizer.Pin().ToSharedRef());
		if (ParentWindow.IsValid())
			ParentWindow->RemoveOverlaySlot(BufferVisualizer.Pin().ToSharedRef());
	}
	// FUMHelpers::NotifySuccess(FText::FromString("Reset Sequence"));
}

// Initialize Key Bindings in the Trie
void FUMInputPreProcessor::InitializeKeyBindings()
{
	// Create the root node
	TrieRoot = new FTrieNode();

	// Add the "space + j + n" binding
	AddKeyBinding(
		{ EKeys::SpaceBar, EKeys::J, EKeys::N },
		this,
		&FUMInputPreProcessor::Callback_JumpNotification);

	// Add more bindings as needed
	AddKeyBinding(
		{ EKeys::SpaceBar, EKeys::K },
		[]() {
			UE_LOG(LogTemp, Log, TEXT("Callback triggered for Space + K!"));
		});

	AddKeyBinding(
		{ EKeys::H },
		this,
		&FUMInputPreProcessor::GoLeft);

	AddKeyBinding(
		{ EKeys::J },
		this,
		&FUMInputPreProcessor::GoDown);

	AddKeyBinding(
		{ EKeys::K },
		this,
		&FUMInputPreProcessor::GoUp);

	AddKeyBinding(
		{ EKeys::L },
		this,
		&FUMInputPreProcessor::GoRight);
}

template <typename ObjectType>
void FUMInputPreProcessor::AddKeyBinding(const TArray<FKey>& Sequence, ObjectType* Object, void (ObjectType::*MemberFunction)())
{
	AddKeyBinding(Sequence, [Object, MemberFunction]() {
		(Object->*MemberFunction)();
	});
}

void FUMInputPreProcessor::AddKeyBinding(const TArray<FKey>& Sequence, TFunction<void()> Callback)
{
	// Start from the root of the trie
	FTrieNode* CurrentNode = TrieRoot;

	// Traverse or create nodes for the sequence
	for (const FKey& Key : Sequence)
	{
		if (!CurrentNode->Children.Contains(Key))
		{
			CurrentNode->Children.Add(Key, new FTrieNode());
		}
		CurrentNode = CurrentNode->Children[Key];
	}

	// Assign the callback to the last node
	CurrentNode->Callback = MoveTemp(Callback);
}

void FUMInputPreProcessor::Callback_JumpNotification()
{
	FUMHelpers::NotifySuccess(FText::FromString("Jump Notification <3"));
	GoDown();
}

bool FUMInputPreProcessor::HandleKeyDownEvent(
	FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	// InKeyEvent.GetKeyCodea
	if (bIgnoreKeyDown)
	{
		// FUMHelpers::NotifySuccess(FText::FromString("HandleKeyDownEvent Dummy"));
		bIgnoreKeyDown = false;
		return false; // We have to pass false in order for unreal to actually
					  // process the fake event! Need to still work on this.
	}
	// FUMHelpers::NotifySuccess(FText::FromString("HandleKeyDownEvent Prod"));
	// if (InKeyEvent.GetKey() == EKeys::J)
	// {
	// 	GoDown();
	// 	return true;
	// }
	// else if (InKeyEvent.GetKey() == EKeys::K)
	// {
	// 	bIgnoreKeyDown = true;
	// 	GoUp();
	// 	return true;
	// }
	// else if (InKeyEvent.GetKey() == EKeys::H)
	// {
	// 	bIgnoreKeyDown = true;
	// 	GoLeft();
	// 	return true;
	// }
	// else if (InKeyEvent.GetKey() == EKeys::L)
	// {
	// 	bIgnoreKeyDown = true;
	// 	GoRight();
	// 	return true;
	// }

	SwitchVimModes(InKeyEvent);
	if (VimMode != EVimMode::Normal)
		return false; // Pass on the handling to Unreal native.

	// Process the key input using the trie
	// const FKey& KeyPressed = InKeyEvent.GetKey();
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

	// Check for sequence completion
	ProcessKeySequence(KeyPressed);

	// NavigateTo(InKeyEvent);

	// FUMHelpers::NotifySuccess();
	return true;
}

void FUMInputPreProcessor::SwitchVimModes(const FKeyEvent& InKeyEvent)
{
	const FKey& KeyPressed = InKeyEvent.GetKey();
	if (KeyPressed == EKeys::Escape && InKeyEvent.IsShiftDown())
	{
		SetMode(EVimMode::Normal);
	}
	else if (VimMode == EVimMode::Normal && KeyPressed == EKeys::I)
	{
		SetMode(EVimMode::Insert);
	}
	else if (VimMode == EVimMode::Normal && KeyPressed == EKeys::V)
	{
		SetMode(EVimMode::Visual);
	}
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

void FUMInputPreProcessor::NavigateTo(const FKeyEvent& InKeyEvent)
{
	FSlateApplication& App = FSlateApplication::Get();
	const FKey&		   InKey = InKeyEvent.GetKey();
	FKey			   OutKey; // Change to FKey instead of EKeys

	// Compare against the key name instead of direct FKey comparison
	if (InKey == EKeys::H)
		OutKey = FKey(EKeys::Left); // Create FKey from EKeys
	else if (InKey == EKeys::J)
		OutKey = FKey(EKeys::Down);
	else if (InKey == EKeys::K)
		OutKey = FKey(EKeys::Up);
	else if (InKey == EKeys::L)
		OutKey = FKey(EKeys::Right);
	else
		return;

	// Create new key event using the same structure as InKeyEvent
	FKeyEvent MappedKeyEvent(
		OutKey,
		InKeyEvent.GetModifierKeys(), // Copy modifier state from input event
		InKeyEvent.GetUserIndex(),
		InKeyEvent.IsRepeat(),
		InKeyEvent.GetCharacter(),
		InKeyEvent.GetKeyCode());
	App.ProcessKeyDownEvent(MappedKeyEvent);
}

void FUMInputPreProcessor::GoLeft()
{
	bIgnoreKeyDown = true;
	FKeyEvent DownKeyEvent(
		EKeys::Left,
		FModifierKeysState(),
		0,	   // User index
		false, // Is repeat
		0,	   // Character code
		0	   // Key code
	);
	FSlateApplication::Get().ProcessKeyDownEvent(DownKeyEvent);
}

void FUMInputPreProcessor::GoDown()
{
	bIgnoreKeyDown = true;
	FKeyEvent DownKeyEvent(
		EKeys::Down,
		FModifierKeysState(),
		0,	   // User index
		false, // Is repeat
		0,	   // Character code
		0	   // Key code
	);
	FSlateApplication::Get().ProcessKeyDownEvent(DownKeyEvent);
}

void FUMInputPreProcessor::GoUp()
{
	bIgnoreKeyDown = true;
	FKeyEvent DownKeyEvent(
		EKeys::Up,
		FModifierKeysState(),
		0,	   // User index
		false, // Is repeat
		0,	   // Character code
		0	   // Key code
	);
	FSlateApplication::Get().ProcessKeyDownEvent(DownKeyEvent);
}

void FUMInputPreProcessor::GoRight()
{
	bIgnoreKeyDown = true;
	FKeyEvent DownKeyEvent(
		EKeys::Right,
		FModifierKeysState(),
		0,	   // User index
		false, // Is repeat
		0,	   // Character code
		0	   // Key code
	);
	FSlateApplication::Get().ProcessKeyDownEvent(DownKeyEvent);
	// FUMHelpers::NotifySuccess(FText::FromString("Go Right!"));
}

// 	else if (KeyPressed == EKeys::D && KeyEvent.IsControlDown())
// 	{
// 		for (int32 i = 0; i < 6; ++i)
// 		{
// 			FKeyEvent DownKeyEvent(
// 				EKeys::Down,
// 				FModifierKeysState(),
// 				0,	   // User index
// 				false, // Is repeat
// 				0,	   // Character code
// 				0	   // Key code
// 			);
// 			App.ProcessKeyDownEvent(DownKeyEvent);
// 		}
// 	}
// 	else if (KeyPressed == EKeys::U && KeyEvent.IsControlDown())
// 	{
// 		for (int32 i = 0; i < 6; ++i)
// 		{
// 			FKeyEvent UpKeyEvent(
// 				EKeys::Up,
// 				FModifierKeysState(),
// 				0,	   // User index
// 				false, // Is repeat
// 				0,	   // Character code
// 				0	   // Key code
// 			);
// 			App.ProcessKeyDownEvent(UpKeyEvent);
// 		}
// 	}
// }
