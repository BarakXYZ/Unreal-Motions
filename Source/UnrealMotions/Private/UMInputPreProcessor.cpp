#include "UMInputPreProcessor.h"
#include "UMHelpers.h"
#include "StatusBarSubsystem.h"
#include "Editor.h"
// #include "WidgetDrawerConfig.h"

FUMInputPreProcessor::FUMInputPreProcessor()
{
	InitializeKeyBindings();
	// UStatusBarSubsystem* StatusBarSubsystem = GEditor->GetEditorSubsystem<UStatusBarSubsystem>();

	// FWidgetDrawerConfig MyDrawerConfig("VIM");
	// MyDrawerConfig.ButtonText = FText::FromString("My Custom Notifier!");
	// // MyDrawerConfig.Icon = FSlateIcon(); // Optional: If you have a custom icon
	// MyDrawerConfig.ToolTipText = FText::FromString("Click to see My Custom Notifier Panel");
	// StatusBarSubsystem->RegisterDrawer(
	// 	FName("MainMenu.StatusBar"),
	// 	MoveTemp(MyDrawerConfig),
	// 	5);
}
FUMInputPreProcessor::~FUMInputPreProcessor()
{
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
}

bool FUMInputPreProcessor::HandleKeyDownEvent(
	FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	SwitchVimModes(InKeyEvent);
	if (VimMode != EVimMode::Normal)
		return false; // Pass on the handling to Unreal native.

	// Process the key input using the trie
	const FKey& KeyPressed = InKeyEvent.GetKey();

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

// if (VimMode == EVimMode::Normal)
// {
// 	if (KeyPressed == EKeys::H)
// 	{
// 		FKeyEvent LeftKeyEvent(
// 			EKeys::Left,
// 			FModifierKeysState(),
// 			0,	   // User index
// 			false, // Is repeat
// 			0,	   // Character code
// 			0	   // Key code
// 		);
// 		App.ProcessKeyDownEvent(LeftKeyEvent);
// 	}
// 	else if (KeyPressed == EKeys::J)
// 	{
// 		FKeyEvent DownKeyEvent(
// 			EKeys::Down,
// 			FModifierKeysState(),
// 			0,	   // User index
// 			false, // Is repeat
// 			0,	   // Character code
// 			0	   // Key code
// 		);
// 		App.ProcessKeyDownEvent(DownKeyEvent);
// 	}
// 	else if (KeyPressed == EKeys::K)
// 	{
// 		FKeyEvent UpKeyEvent(
// 			EKeys::Up,
// 			FModifierKeysState(),
// 			0,	   // User index
// 			false, // Is repeat
// 			0,	   // Character code
// 			0	   // Key code
// 		);
// 		App.ProcessKeyDownEvent(UpKeyEvent);
// 	}
// 	else if (KeyPressed == EKeys::L)
// 	{
// 		FKeyEvent RightKeyEvent(
// 			EKeys::Right,
// 			FModifierKeysState(),
// 			0,	   // User index
// 			false, // Is repeat
// 			0,	   // Character code
// 			0	   // Key code
// 		);
// 		App.ProcessKeyDownEvent(RightKeyEvent);
// 	}
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
