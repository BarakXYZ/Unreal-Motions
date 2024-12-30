#pragma once

#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include "UMBufferVisualizer.h"

#include "UMHelpers.h"
#include "UMKeyChordTrieNode.h"

class SBufferVisualizer;

UENUM(BlueprintType)
enum class EVimMode : uint8
{
	Normal UMETA(DisplayName = "Normal"),
	Insert UMETA(DisplayName = "Insert"),
	Visual UMETA(DisplayName = "Visual"),
};

/**
 * Delegate that broadcasts when the input preprocessor is initialized
 * @note Called after the singleton instance is created in Initialize()
 */
DECLARE_MULTICAST_DELEGATE(FOnUMPreProcessorInputInit);

/**
 * Delegate that broadcasts when a numeric prefix is detected in input sequence
 * @param New Prefix - The numeric prefix string that was just entered
 * @note Used for handling repeat counts in Vim-style commands
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FUMOnCountPrefix, FString);

/**
 * Delegate that broadcasts when the current input sequence is reset
 * @note Triggered when clearing sequence state, buffer, and visualizations
 */
DECLARE_MULTICAST_DELEGATE(FUMOnResetSequence);

/**
 * Delegate that broadcasts when the Vim editing mode changes
 * @param EVimMode - The new mode being switched to (Normal, Insert, or Visual)
 * @note Called after mode changes through SetMode()
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnVimModeChanged, const EVimMode);

class FUMInputPreProcessor : public IInputProcessor
{
public:
	/**
	 * Constructor for the input preprocessor
	 * @note Registers default key bindings through OnUMPreProcessorInputInit delegate
	 */
	FUMInputPreProcessor();

	/** Destructor */
	~FUMInputPreProcessor();

	/**
	 * Returns the singleton instance of the input preprocessor
	 * @return Shared pointer to the input preprocessor instance
	 * @note Creates the instance if it doesn't exist
	 */
	static TSharedPtr<FUMInputPreProcessor> Get();

	/**
	 * Creates the singleton instance if it doesn't exist
	 * @note Broadcasts OnUMPreProcessorInputInit after creation
	 */
	static void Initialize();

	/**
	 * Checks if the input preprocessor has been initialized
	 * @return True if the input preprocessor instance exists
	 */
	static bool IsInitialized();

	/**
	 * Overridden tick function from IInputProcessor
	 * @param DeltaTime - Time elapsed since last tick
	 * @param SlateApp - Reference to the Slate application instance
	 * @param Cursor - Reference to the cursor interface
	 */
	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp,
		TSharedRef<ICursor> Cursor) override;

private:
	/**
	 * Primary input handling function that processes keyboard input based on the
	 * current Vim mode.
	 * @param SlateApp - Reference to the Slate application instance
	 * @param InKeyEvent - The key event to process
	 * @return True if the key event was handled, false if the event should be passed to native input handling
	 */
	virtual bool HandleKeyDownEvent(
		FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override;

	/**
	 * Processes the current key sequence through the key binding trie structure
	 * @param SlateApp - Reference to the Slate application instance
	 * @param InKeyEvent - The key event to process
	 * @return True if the sequence was processed or is a valid partial match, false if no match was found
	 */
	bool ProcessKeySequence(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	/**
	 * Changes the current Vim editing mode and broadcasts the change through the
	 * mode change delegate
	 * @param NewMode - The new Vim mode to switch to (Normal, Insert, or Visual)
	 */
	void SetMode(FSlateApplication& SlateApp, const EVimMode NewMode);

	/**
	 * Determines if the current key event should trigger a Vim mode switch
	 * @param SlateApp - Reference to the Slate application instance
	 * @param InKeyEvent - The key event to evaluate
	 * @return True if the key event should trigger a mode switch (Escape key)
	 */
	bool ShouldSwitchVimMode(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	/**
	 * Handles the logic for switching between different Vim modes based on key input
	 * @param SlateApp - Reference to the Slate application instance
	 * @param InKeyEvent - The key event that triggered the mode switch
	 */
	void SwitchVimModes(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

public:
	/**
	 * Returns a reference to the delegate for Vim mode change notifications
	 * @return Reference to FOnVimModeChanged delegate that broadcasts mode changes
	 */
	FOnVimModeChanged& GetOnVimModeChanged();

	/** Registers a callback for Vim mode changes */
	void RegisterOnVimModeChanged(TFunction<void(const EVimMode)> Callback);

	/** Unregisters a specific callback */
	void UnregisterOnVimModeChanged(const void* CallbackOwner);

	/**
	 * Toggles between native and custom input handling
	 * @param bNativeHandling - Whether to enable native input handling
	 */
	static void ToggleNativeInputHandling(const bool bNativeHandling);

	/////////////////////////////////////////////////////////////////////////
	// ~ Add Key Bindings ~
	//

	/**
	 * Finds or creates a trie node for the given input sequence
	 * @param Sequence - Array of input chords representing the key sequence
	 * @return Pointer to the found or created trie node
	 */
	FKeyChordTrieNode* FindOrCreateTrieNode(const TArray<FInputChord>& Sequence);

	//
	// 1) No-Parameter Bindings
	//

	/**
	 * Adds a key binding with no parameters
	 * @param Sequence - Array of input chords that trigger the callback
	 * @param Callback - Function to execute when the sequence is matched
	 */
	void AddKeyBinding_NoParam(
		const TArray<FInputChord>& Sequence,
		TFunction<void()>		   Callback);

	/**
	 * Adds a key binding with no parameters using a weak pointer to an object
	 * @param Sequence - Array of input chords that trigger the callback
	 * @param WeakObj - Weak pointer to the object containing the member function
	 * @param MemberFunc - Member function to call when the sequence is matched
	 */
	template <typename ObjectType>
	void AddKeyBinding_NoParam(
		const TArray<FInputChord>& Sequence,
		TWeakPtr<ObjectType>	   WeakObj,
		void (ObjectType::*MemberFunc)())
	{
		AddKeyBinding_NoParam(
			Sequence,
			[this, WeakObj, MemberFunc]() {
				if (TSharedPtr<ObjectType> Shared = WeakObj.Pin())
					(Shared.Get()->*MemberFunc)();
				else
					DebugInvalidWeakPtr(EUMKeyBindingCallbackType::NoParam);
			});
	}

	/**
	 * Adds a key binding with no parameters using a weak object pointer
	 * @param Sequence - Array of input chords that trigger the callback
	 * @param WeakObj - Weak object pointer to the UObject containing the member function
	 * @param MemberFunc - Member function to call when the sequence is matched
	 */
	template <typename ObjectType>
	void AddKeyBinding_NoParam(
		const TArray<FInputChord>& Sequence,
		TWeakObjectPtr<ObjectType> WeakObj,
		void (ObjectType::*MemberFunc)())
	{
		AddKeyBinding_NoParam(
			Sequence,
			[this, WeakObj, MemberFunc]() {
				if (WeakObj.IsValid())
					(WeakObj.Get()->*MemberFunc)();
				else
					DebugInvalidWeakPtr(EUMKeyBindingCallbackType::NoParam);
			});
	}

	//
	// 2) KeyEvent Parameter Bindings
	//

	/**
	 * Adds a key binding that receives the Slate application and key event
	 * @param Sequence - Array of input chords that trigger the callback
	 * @param Callback - Function to execute when the sequence is matched
	 */
	void AddKeyBinding_KeyEvent(
		const TArray<FInputChord>&									   Sequence,
		TFunction<void(FSlateApplication& SlateApp, const FKeyEvent&)> Callback);

	/**
	 * Adds a key binding that receives the Slate application and key event using a weak pointer
	 * @param Sequence - Array of input chords that trigger the callback
	 * @param WeakObj - Weak pointer to the object containing the member function
	 * @param MemberFunc - Member function to call when the sequence is matched
	 */
	template <typename ObjectType>
	void AddKeyBinding_KeyEvent(
		const TArray<FInputChord>& Sequence,
		TWeakPtr<ObjectType>	   WeakObj,
		void (ObjectType::*MemberFunc)(
			FSlateApplication& SlateApp, const FKeyEvent&))
	{
		AddKeyBinding_KeyEvent(
			Sequence,
			[this, WeakObj, MemberFunc](
				FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) {
				if (TSharedPtr<ObjectType> Shared = WeakObj.Pin())
					(Shared.Get()->*MemberFunc)(SlateApp, InKeyEvent);
				else
					DebugInvalidWeakPtr(EUMKeyBindingCallbackType::KeyEventParam);
			});
	}

	/**
	 * Adds a key binding that receives the Slate application and key event using a weak object pointer
	 * @param Sequence - Array of input chords that trigger the callback
	 * @param WeakObj - Weak object pointer to the UObject containing the member function
	 * @param MemberFunc - Member function to call when the sequence is matched
	 */
	template <typename ObjectType>
	void AddKeyBinding_KeyEvent(
		const TArray<FInputChord>& Sequence,
		TWeakObjectPtr<ObjectType> WeakObj,
		void (ObjectType::*MemberFunc)(
			FSlateApplication& SlateApp, const FKeyEvent&))
	{
		AddKeyBinding_KeyEvent(
			Sequence,
			[this, WeakObj, MemberFunc](
				FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) {
				if (WeakObj.IsValid())
					(WeakObj.Get()->*MemberFunc)(SlateApp, InKeyEvent);
				else
					DebugInvalidWeakPtr(EUMKeyBindingCallbackType::KeyEventParam);
			});
	}

	//
	// 3) Sequence Parameter Bindings
	//

	/**
	 * Adds a key binding that receives the Slate application and input chord sequence
	 * @param Sequence - Array of input chords that trigger the callback
	 * @param Callback - Function to execute when the sequence is matched
	 */
	void AddKeyBinding_Sequence(
		const TArray<FInputChord>& Sequence,
		TFunction<void(
			FSlateApplication& SlateApp, const TArray<FInputChord>&)>
			Callback);

	/**
	 * Adds a key binding that receives the Slate application and input chord sequence using a weak pointer
	 * @param Sequence - Array of input chords that trigger the callback
	 * @param WeakObj - Weak pointer to the object containing the member function
	 * @param MemberFunc - Member function to call when the sequence is matched
	 */
	template <typename ObjectType>
	void AddKeyBinding_Sequence(
		const TArray<FInputChord>& Sequence,
		TWeakPtr<ObjectType>	   WeakObj,
		void (ObjectType::*MemberFunc)(
			FSlateApplication& SlateApp, const TArray<FInputChord>&))
	{
		AddKeyBinding_Sequence(
			Sequence,
			[this, WeakObj, MemberFunc](
				FSlateApplication& SlateApp, const TArray<FInputChord>& Arr) {
				if (TSharedPtr<ObjectType> Shared = WeakObj.Pin())
					(Shared.Get()->*MemberFunc)(SlateApp, Arr);
				else
					DebugInvalidWeakPtr(EUMKeyBindingCallbackType::SequenceParam);
			});
	}

	/**
	 * Adds a key binding that receives the Slate application and input chord sequence using a weak object pointer
	 * @param Sequence - Array of input chords that trigger the callback
	 * @param WeakObj - Weak object pointer to the UObject containing the member function
	 * @param MemberFunc - Member function to call when the sequence is matched
	 */
	template <typename ObjectType>
	void AddKeyBinding_Sequence(
		const TArray<FInputChord>& Sequence,
		TWeakObjectPtr<ObjectType> WeakObj,
		void (ObjectType::*MemberFunc)(
			FSlateApplication& SlateApp, const TArray<FInputChord>&))
	{
		AddKeyBinding_Sequence(
			Sequence,
			[this, WeakObj, MemberFunc](
				FSlateApplication& SlateApp, const TArray<FInputChord>& Arr) {
				if (WeakObj.IsValid())
					(WeakObj.Get()->*MemberFunc)(SlateApp, Arr);
				else
					DebugInvalidWeakPtr(EUMKeyBindingCallbackType::SequenceParam);
			});
	}

private:
	/**
	 * Initializes the default key bindings for the Vim-like input system
	 * @note Creates the root trie node and registers basic mode switching commands:
	 * - 'I' key for switching to Insert mode
	 * - 'Shift+V' for switching to Visual mode
	 */
	void RegisterDefaultKeyBindings();

	/////////////////////////////////////////////////////////////////////////

	/**
	 * Simulates multiple tab key presses programmatically
	 * @param SlateApp - Reference to the Slate application instance
	 * @param TimesToRepeat - Number of times to simulate the tab press
	 */
	void SimulateMultiTabPresses(FSlateApplication& SlateApp, int32 TimesToRepeat);

	/**
	 * Checks if the current key combination should simulate an escape key press
	 * @param SlateApp - Reference to the Slate application instance
	 * @param InKeyEvent - The key event to evaluate
	 * @return True if the key combination should simulate escape (Shift+Escape)
	 */
	bool IsSimulateEscapeKey(
		FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	/**
	 * Resets the current input sequence state and cleans up related resources
	 * @param SlateApp - Reference to the Slate application instance needed for visualizer cleanup
	 * @note Broadcasts reset event, clears current sequence and buffer, and removes buffer visualization
	 */
	void ResetSequence(FSlateApplication& SlateApp);
	/**
	 * Cleans up and removes the buffer visualization overlay
	 * @param SlateApp - Reference to the Slate application instance
	 */
	void ResetBufferVisualizer(FSlateApplication& SlateApp);

	/**
	 * Processes numeric keys for command repeat counts
	 * @param SlateApp - Reference to the Slate application instance
	 * @param InKeyEvent - The key event to evaluate
	 * @return True if processed as a count digit, considering:
	 *         - Must be first key or continue existing count
	 *         - No modifier keys allowed
	 *         - Must be numeric key (0-9)
	 */
	bool TrackCountPrefix(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

public:
	//////////////////////////////////////////////////////////////////////////
	//							~ Helpers ~

	/**
	 * Creates an FInputChord from a key event, capturing modifier key states
	 * @param InKeyEvent - The key event to convert
	 * @return FInputChord containing the key and modifier state information
	 */
	static FInputChord GetChordFromKeyEvent(const FKeyEvent& InKeyEvent);

	/**
	 * Converts a numeric key press to its string representation with optional value clamping
	 * @param InKey - The key to convert
	 * @param OutStr - Output parameter for the string representation
	 * @param MinClamp - Optional minimum value to clamp the digit to (0 means no min clamping)
	 * @param MaxClamp - Optional maximum value to clamp the digit to (0 means no max clamping)
	 * @return True if the key was a numeric key (0-9)
	 */
	static bool GetStrDigitFromKey(const FKey& InKey, FString& OutStr,
		int32 MinClamp = 0, int32 MaxClamp = 0);

	/**
	 * Displays debug information when a weak pointer becomes invalid
	 * @param CallbackType - The type of callback that contained the invalid ptr
	 */
	void DebugInvalidWeakPtr(EUMKeyBindingCallbackType CallbackType);

	/**
	 * Logs detailed information about a key event for debugging purposes
	 * @param InKeyEvent - The key event to analyze and log
	 * @note Outputs key state, modifiers, event path, and device information to both screen and log
	 */
	void DebugKeyEvent(const FKeyEvent& InKeyEvent);

	static void SimulateKeyPress(
		FSlateApplication& SlateApp, const FKey& SimulatedKey);

	//
	/////////////////////////////////////////////////////////////////////////

private:
	//						~ DEPRECATED ~
	/**
	 * Handles mouse button press events
	 * @param SlateApp - Reference to the Slate application instance
	 * @param MouseEvent - The mouse event to process
	 * @return False to pass through mouse events
	 */
	virtual bool HandleMouseButtonDownEvent(
		FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;

	/////////////////////////////////////////////////////////////////////////////
	// ~ Member Variables ~
	//

	/** Input processing state */
private:
	FKeyChordTrieNode* TrieRoot = nullptr; // Root node (unique)

	/** Static instance management */
	static TSharedPtr<FUMInputPreProcessor> InputPreProcessor;
	static FOnUMPreProcessorInputInit		OnUMPreProcessorInputInit;
	static bool								bNativeInputHandling;

	/** Input mode state */
	EVimMode VimMode{ EVimMode::Insert };

	/** Buffer */
	TArray<FInputChord>			  CurrentSequence;	// Current Input Sequence
	TWeakPtr<SUMBufferVisualizer> BufferVisualizer; // Pointer to the visualizer
	FString						  CurrentBuffer;	// Sequence as a String buffer

	/** Count Command */
	bool bIsCounting{ false };

public:
	/** Event delegates */
	FOnVimModeChanged  OnVimModeChanged;
	FUMOnCountPrefix   OnCountPrefix;
	FUMOnResetSequence OnResetSequence;

	/** Logging configuration */
	EUMHelpersLogMethod UMHelpersLogMethod{ EUMHelpersLogMethod::PrintToScreen };
	bool				bVisualLog{ true };
	bool				bConsoleLog{ false };
};
