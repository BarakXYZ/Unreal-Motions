// VimInputProcessor.h
#pragma once

#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include "UMBufferVisualizer.h"

#include "UMLogger.h"
#include "UMKeyChordTrieNode.h"

class SBufferVisualizer;

UENUM(BlueprintType)
enum class EVimMode : uint8
{
	Normal	   UMETA(DisplayName = "Normal"),
	Insert	   UMETA(DisplayName = "Insert"),
	Visual	   UMETA(DisplayName = "Visual"),
	VisualLine UMETA(DisplayName = "Visual Line"),

	// Used mainly for binding to comply with any of the above modes
	// (except Insert of course which will naturally be ignored)
	Any UMETA(DisplayName = "Any"),
};

UENUM(BlueprintType)
enum class EUMBindingContext : uint8
{
	TextEditing	   UMETA(DisplayName = "Text Editing"), // Both Multi & Single
	GraphEditor	   UMETA(DisplayName = "Graph Editor"),
	NodeNavigation UMETA(DisplayName = "Node Navigation"),
	TreeView	   UMETA(DisplayName = "Tree View"), // All type of list views
	Viewport	   UMETA(DisplayName = "Viewport"),	 // Level Editor
	Generic		   UMETA(DisplayName = "Generic"),	 // Default fallback
};

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
 * @note Called after mode changes through SetVimMode()
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnVimModeChanged, const EVimMode);

DECLARE_MULTICAST_DELEGATE_TwoParams(FUMOnKeyUpEvent,
	FSlateApplication& /* SlateApp */, const FKeyEvent& /* InKeyEvent */);

DECLARE_MULTICAST_DELEGATE_TwoParams(FUMOnKeyDown, FSlateApplication&, const FKeyEvent&);

class FVimInputProcessor : public IInputProcessor
{
public:
	/**
	 * Constructor for the input preprocessor
	 * @note Registers default key bindings through OnUMPreProcessorInputInit delegate
	 */
	FVimInputProcessor();

	/** Destructor */
	~FVimInputProcessor();

	/**
	 * Returns the singleton instance of the input preprocessor
	 * @return Shared reference to the input preprocessor instance.
	 */
	static TSharedRef<FVimInputProcessor> Get();

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

	/** Key up input */
	virtual bool HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override;

	/**
	 * Processes the current key sequence through the key binding trie structure
	 * @param SlateApp - Reference to the Slate application instance
	 * @param InKeyEvent - The key event to process
	 * @return True if the sequence was processed or is a valid partial match, false if no match was found
	 */
	bool ProcessKeySequence(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	/**
	 * Tries to traverse the trie for the given context and Vim mode using the CurrentSequence.
	 * @param InContext - The context in which to try a match
	 * @param InVimMode - The Vim mode for which to try a match
	 * @param OutNode - The node matched if partial/full success
	 * @return True if there's at least a partial match, false otherwise
	 */
	bool TraverseTrieForContextAndVim(EUMBindingContext InContext, EVimMode InVimMode, TSharedPtr<FKeyChordTrieNode>& OutNode) const;

public:
	/**
	 * Changes the current Vim editing mode and broadcasts the change through the
	 * mode change delegate
	 * @param NewMode - The new Vim mode to switch to (Normal, Insert, or Visual)
	 */
	void SetVimMode(FSlateApplication& SlateApp, const EVimMode NewMode, bool bResetCurrSequence = true, bool bBroadcastModeChange = true);

	/**
	 * Delayed override edition
	 */
	void SetVimMode(FSlateApplication& SlateApp, const EVimMode NewMode, const float Delay);

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

	// ~~~~~~~~~~~~~  Multi-Context Support  ~~~~~~~~~~~~~
	/**
	 * Set the current context (e.g. called from OnFocusChanged or wherever you decide).
	 */
	void SetCurrentContext(EUMBindingContext NewContext)
	{
		CurrentContext = NewContext;
	}

	EUMBindingContext GetCurrentContext() const
	{
		return CurrentContext;
	}

	/////////////////////////////////////////////////////////////////////////
	// ~ Add Key Bindings ~
	//

	/**
	 * High-level function to get or create the trie root for a given context and Vim mode.
	 */
	TSharedPtr<FKeyChordTrieNode> GetOrCreateTrieRoot(EUMBindingContext Context, EVimMode VimMode = EVimMode::Any);

	/**
	 * Low-level helper: finds or creates a trie node (descendant of the given root) for a sequence.
	 * Finds or creates a trie node for the given input sequence
	 * @param Sequence - Array of input chords representing the key sequence
	 * @return Pointer to the found or created trie node
	 */
	TSharedPtr<FKeyChordTrieNode> FindOrCreateTrieNode(
		TSharedPtr<FKeyChordTrieNode> Root,
		const TArray<FInputChord>&	  Sequence);

	/**
	 */
	TSharedPtr<FKeyChordTrieNode> FindOrCreateTrieNode(const TArray<FInputChord>& Sequence);

	//
	// 1) No-Parameter Bindings
	//

	/**
	 * Adds a key binding with no parameters
	 * @param Context - The context in which this binding should be active
	 * @param Sequence - Array of input chords that trigger the callback
	 * @param Callback - Function to execute when the sequence is matched
	 * @param VimModes - Array of Vim modes in which this binding should be active (defaults to {Any})
	 */
	void AddKeyBinding_NoParam(
		EUMBindingContext		   Context,
		const TArray<FInputChord>& Sequence,
		TFunction<void()>		   Callback,
		const TArray<EVimMode>&	   VimModes = { EVimMode::Any });

	/**
	 * Adds a key binding with no parameters using a weak pointer to an object
	 * @param Context - The context in which this binding should be active
	 * @param Sequence - Array of input chords that trigger the callback
	 * @param WeakObj - Weak pointer to the object containing the member function
	 * @param MemberFunc - Member function to call when the sequence is matched
	 * @param VimModes - Array of Vim modes in which this binding should be active (defaults to {Any})
	 */
	template <typename ObjectType>
	void AddKeyBinding_NoParam(
		EUMBindingContext		   Context,
		const TArray<FInputChord>& Sequence,
		TWeakPtr<ObjectType>	   WeakObj,
		void (ObjectType::*MemberFunc)(),
		const TArray<EVimMode>& VimModes = { EVimMode::Any })
	{
		AddKeyBinding_NoParam(
			Context,
			Sequence,
			[this, WeakObj, MemberFunc]() {
				if (TSharedPtr<ObjectType> Shared = WeakObj.Pin())
					(Shared.Get()->*MemberFunc)();
				else
					DebugInvalidWeakPtr(EUMKeyBindingCallbackType::NoParam);
			},
			VimModes);
	}

	/**
	 * Adds a key binding with no parameters using a weak object pointer
	 * @param Context - The context in which this binding should be active
	 * @param Sequence - Array of input chords that trigger the callback
	 * @param WeakObj - Weak object pointer to the UObject containing the member function
	 * @param MemberFunc - Member function to call when the sequence is matched
	 * @param VimModes - Array of Vim modes in which this binding should be active (defaults to {Any})
	 */
	template <typename ObjectType>
	void AddKeyBinding_NoParam(
		EUMBindingContext		   Context,
		const TArray<FInputChord>& Sequence,
		TWeakObjectPtr<ObjectType> WeakObj,
		void (ObjectType::*MemberFunc)(),
		const TArray<EVimMode>& VimModes = { EVimMode::Any })
	{
		AddKeyBinding_NoParam(
			Context,
			Sequence,
			[this, WeakObj, MemberFunc]() {
				if (WeakObj.IsValid())
					(WeakObj.Get()->*MemberFunc)();
				else
					DebugInvalidWeakPtr(EUMKeyBindingCallbackType::NoParam);
			},
			VimModes);
	}

	/**
	 * Adds a key binding with no parameters using a raw pointer
	 * @param Context - The context in which this binding should be active
	 * @param Sequence - Array of input chords that trigger the callback
	 * @param Obj - Raw pointer to the object containing the member function
	 * @param MemberFunc - Member function to call when the sequence is matched
	 * @param VimModes - Array of Vim modes in which this binding should be active (defaults to {Any})
	 */
	template <typename ObjectType>
	void AddKeyBinding_NoParam(
		EUMBindingContext		   Context,
		const TArray<FInputChord>& Sequence,
		ObjectType*				   Obj,
		void (ObjectType::*MemberFunc)(),
		const TArray<EVimMode>& VimModes = { EVimMode::Any })
	{
		AddKeyBinding_NoParam(
			Context,
			Sequence,
			[this, Obj, MemberFunc]() {
				if (Obj)
				{
					(Obj->*MemberFunc)();
				}
				else
				{
					// TODO: add logger here
					// DebugInvalidRawPointer(EUMKeyBindingCallbackType::NoParam);
				}
			},
			VimModes);
	}

	//
	// 2) KeyEvent Parameter Bindings
	//

	/**
	 * Adds a key binding that receives the Slate application and key event
	 * @param Context - The context in which this binding should be active
	 * @param Sequence - Array of input chords that trigger the callback
	 * @param Callback - Function to execute when the sequence is matched
	 * @param VimModes - Array of Vim modes in which this binding should be active (defaults to {Any})
	 */
	void AddKeyBinding_KeyEvent(
		EUMBindingContext											   Context,
		const TArray<FInputChord>&									   Sequence,
		TFunction<void(FSlateApplication& SlateApp, const FKeyEvent&)> Callback,
		const TArray<EVimMode>&										   VimModes = { EVimMode::Any });

	/**
	 * Adds a key binding for a static function that receives the Slate application and key event.
	 * This overload is designed for static member functions or free functions.
	 *
	 * @tparam ObjectType - Ignored in this static overload but kept for symmetry with the weak pointer version.
	 * @param Context - The context in which this binding should be active
	 * @param Sequence - Array of input chords that trigger the callback.
	 * @param MemberFunc - Static function to call when the sequence is matched.
	 * @param VimModes - Array of Vim modes in which this binding should be active (defaults to {Any})
	 */
	template <typename ObjectType>
	void AddKeyBinding_KeyEvent(
		EUMBindingContext		   Context,
		const TArray<FInputChord>& Sequence,
		void (*MemberFunc)(FSlateApplication&, const FKeyEvent&),
		const TArray<EVimMode>& VimModes = { EVimMode::Any })
	{
		AddKeyBinding_KeyEvent(
			Context,
			Sequence,
			[MemberFunc](FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) {
				MemberFunc(SlateApp, InKeyEvent);
			},
			VimModes);
	}

	/**
	 * Adds a key binding for a member function that receives the Slate
	 * application and key event.
	 * This overload supports weak pointers, allowing for safe binding to member
	 * functions of objects that may be destroyed.
	 *
	 * @tparam ObjectType - The type of the object containing the member function.
	 * @param Context - The context in which this binding should be active
	 * @param Sequence - Array of input chords that trigger the callback.
	 * @param WeakObj - Weak pointer to the object containing the member function.
	 * @param MemberFunc - Member function to call when the sequence is matched.
	 * @param VimModes - Array of Vim modes in which this binding should be active (defaults to {Any})
	 */
	template <typename ObjectType>
	void AddKeyBinding_KeyEvent(
		EUMBindingContext		   Context,
		const TArray<FInputChord>& Sequence,
		TWeakPtr<ObjectType>	   WeakObj,
		void (ObjectType::*MemberFunc)(
			FSlateApplication& SlateApp, const FKeyEvent&),
		const TArray<EVimMode>& VimModes = { EVimMode::Any })
	{
		AddKeyBinding_KeyEvent(
			Context,
			Sequence,
			[this, WeakObj, MemberFunc](
				FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) {
				if (TSharedPtr<ObjectType> Shared = WeakObj.Pin())
					(Shared.Get()->*MemberFunc)(SlateApp, InKeyEvent);
				else
					DebugInvalidWeakPtr(EUMKeyBindingCallbackType::KeyEventParam);
			},
			VimModes);
	}

	/**
	 * Adds a key binding that receives the Slate application and key event using a weak object pointer
	 * @param Context - The context in which this binding should be active
	 * @param Sequence - Array of input chords that trigger the callback
	 * @param WeakObj - Weak object pointer to the UObject containing the member function
	 * @param MemberFunc - Member function to call when the sequence is matched
	 * @param VimModes - Array of Vim modes in which this binding should be active (defaults to {Any})
	 */
	template <typename ObjectType>
	void AddKeyBinding_KeyEvent(
		EUMBindingContext		   Context,
		const TArray<FInputChord>& Sequence,
		TWeakObjectPtr<ObjectType> WeakObj,
		void (ObjectType::*MemberFunc)(
			FSlateApplication& SlateApp, const FKeyEvent&),
		const TArray<EVimMode>& VimModes = { EVimMode::Any })
	{
		AddKeyBinding_KeyEvent(
			Context,
			Sequence,
			[this, WeakObj, MemberFunc](
				FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) {
				if (WeakObj.IsValid())
					(WeakObj.Get()->*MemberFunc)(SlateApp, InKeyEvent);
				else
					DebugInvalidWeakPtr(EUMKeyBindingCallbackType::KeyEventParam);
			},
			VimModes);
	}

	//
	// 3) Sequence Parameter Bindings
	//

	/**
	 * Adds a key binding that receives the Slate application and input chord sequence
	 * @param Context - The context in which this binding should be active
	 * @param Sequence - Array of input chords that trigger the callback
	 * @param Callback - Function to execute when the sequence is matched
	 * @param VimModes - Array of Vim modes in which this binding should be active (defaults to {Any})
	 */
	void AddKeyBinding_Sequence(
		EUMBindingContext		   Context,
		const TArray<FInputChord>& Sequence,
		TFunction<void(
			FSlateApplication& SlateApp, const TArray<FInputChord>&)>
								Callback,
		const TArray<EVimMode>& VimModes = { EVimMode::Any });

	/**
	 * Adds a key binding that receives the Slate application and input chord sequence using a weak pointer
	 * @param Context - The context in which this binding should be active
	 * @param Sequence - Array of input chords that trigger the callback
	 * @param WeakObj - Weak pointer to the object containing the member function
	 * @param MemberFunc - Member function to call when the sequence is matched
	 * @param VimModes - Array of Vim modes in which this binding should be active (defaults to {Any})
	 */
	template <typename ObjectType>
	void AddKeyBinding_Sequence(
		EUMBindingContext		   Context,
		const TArray<FInputChord>& Sequence,
		TWeakPtr<ObjectType>	   WeakObj,
		void (ObjectType::*MemberFunc)(
			FSlateApplication& SlateApp, const TArray<FInputChord>&),
		const TArray<EVimMode>& VimModes = { EVimMode::Any })
	{
		AddKeyBinding_Sequence(
			Context,
			Sequence,
			[this, WeakObj, MemberFunc](
				FSlateApplication& SlateApp, const TArray<FInputChord>& Arr) {
				if (TSharedPtr<ObjectType> Shared = WeakObj.Pin())
					(Shared.Get()->*MemberFunc)(SlateApp, Arr);
				else
					DebugInvalidWeakPtr(EUMKeyBindingCallbackType::SequenceParam);
			},
			VimModes);
	}

	/**
	 * Adds a key binding that receives the Slate application and input chord sequence using a weak object pointer
	 * @param Context - The context in which this binding should be active
	 * @param Sequence - Array of input chords that trigger the callback
	 * @param WeakObj - Weak object pointer to the UObject containing the member function
	 * @param MemberFunc - Member function to call when the sequence is matched
	 * @param VimModes - Array of Vim modes in which this binding should be active (defaults to {Any})
	 */
	template <typename ObjectType>
	void AddKeyBinding_Sequence(
		EUMBindingContext		   Context,
		const TArray<FInputChord>& Sequence,
		TWeakObjectPtr<ObjectType> WeakObj,
		void (ObjectType::*MemberFunc)(
			FSlateApplication& SlateApp, const TArray<FInputChord>&),
		const TArray<EVimMode>& VimModes = { EVimMode::Any })
	{
		AddKeyBinding_Sequence(
			Context,
			Sequence,
			[this, WeakObj, MemberFunc](
				FSlateApplication& SlateApp, const TArray<FInputChord>& Arr) {
				if (WeakObj.IsValid())
					(WeakObj.Get()->*MemberFunc)(SlateApp, Arr);
				else
					DebugInvalidWeakPtr(EUMKeyBindingCallbackType::SequenceParam);
			},
			VimModes);
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

	static FKeyEvent GetKeyEventFromKey(const FKey& InKey, bool bIsShiftDown);

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

	/**
	 * Simulates a key input using ProcessKeyDownEvent (SlateApplication).
	 * Will toggle native handling to process the key correctly.
	 * @param SlateApp - Reference to the Slate application instance
	 * @param SimulatedKey - The key to simulate
	 * @param ModifierKeys - Which modifiers should be simulated
	 */
	static void SimulateKeyPress(
		FSlateApplication& SlateApp, const FKey& SimulatedKey,
		const FModifierKeysState& ModifierKeys = FModifierKeysState(), bool bSetNativeInputHandling = true);

	// Buffer Visualizer:
	void CheckCreateBufferVisualizer(
		FSlateApplication& SlateApp, const FKey& InKey);

	void UpdateBufferAndVisualizer(const FKey& InKey);

	// Possess: Bind the object's member function to the delegate
	template <typename UserClass>
	void Possess(UserClass* InObject, void (UserClass::*InMethod)(FSlateApplication&, const FKeyEvent&))
	{
		if (!PossessedObjects.Contains(InObject))
		{
			// Store the binding so it can be unbound later
			auto Binding = Delegate_OnKeyDown.AddUObject(InObject, InMethod);
			// Save the binding handle for later unbinding
			PossessedObjects.Add(InObject, Binding);
		}
	}

	void Unpossess(UObject* InObject);

	int32 GetCountBuffer();

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

	virtual bool HandleMouseButtonUpEvent(
		FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;

	virtual bool HandleMouseMoveEvent(
		FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;

	/////////////////////////////////////////////////////////////////////////////
	// ~ Member Variables ~
	//

	/** Input processing state */
private:
	/* Deprecated */
	TSharedPtr<FKeyChordTrieNode> TrieRoot = nullptr; // Root node (unique)

	// Map of roots, one per context and vim mode
	TMap<EUMBindingContext, TMap<EVimMode, TSharedPtr<FKeyChordTrieNode>>> ContextTrieRoots;

	// Which context are we currently in?
	EUMBindingContext CurrentContext = EUMBindingContext::Generic;

	/** Static instance management */
	static bool bNativeInputHandling;

	/** Buffer */
	TArray<FInputChord>			  CurrentSequence;	// Current Input Sequence
	TWeakPtr<SUMBufferVisualizer> BufferVisualizer; // Pointer to the visualizer
	FString						  CurrentBuffer;	// Sequence as a String buffer

	/** Count Command */
	bool bIsCounting{ false };
	bool bRequestFollowupReset{ false };

public:
	/** Event delegates */
	FOnVimModeChanged  OnVimModeChanged;
	FUMOnCountPrefix   OnCountPrefix;
	FUMOnResetSequence OnResetSequence;
	FUMOnKeyDown	   Delegate_OnKeyDown;
	FUMOnKeyUpEvent	   Delegate_OnKeyUpEvent;

	// Map to track objects and their binding handles
	TMap<UObject*, FDelegateHandle> PossessedObjects;

	/** Logging configuration */
	EUMLogMethod UMHelpersLogMethod{ EUMLogMethod::PrintToScreen };
	bool		 bVisualLog{ true };
	bool		 bConsoleLog{ false };
	FUMLogger	 Logger;

	FTimerHandle TimerHandle_LinearPress;

	FString		CountBuffer;
	const int32 MIN_REPEAT_COUNT = 1;
	const int32 MAX_REPEAT_COUNT = 999;

	/** Input mode state */
	static EVimMode VimMode;
};

// End of VimInputProcessor.h
