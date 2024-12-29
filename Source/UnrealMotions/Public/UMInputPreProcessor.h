#pragma once

#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include "UMBufferVisualizer.h"

#include "UMHelpers.h"
#include "WidgetDrawerConfig.h"

class SBufferVisualizer;

UENUM(BlueprintType)
enum class EVimMode : uint8
{
	Normal UMETA(DisplayName = "Normal"),
	Insert UMETA(DisplayName = "Insert"),
	Visual UMETA(DisplayName = "Visual"),
};

// Trie Node Structure
enum class EUMCallbackType : uint8
{
	None,
	NoParam,
	KeyEventParam,
	SequenceParam
};

struct FTrieNode
{
	TMap<FInputChord, FTrieNode*> Children;

	// Which callback type is set for this node?
	EUMCallbackType CallbackType = EUMCallbackType::None;

	// Up to three callback function pointers
	TFunction<void()> NoParamCallback;

	TFunction<void(
		FSlateApplication& SlateApp, const FKeyEvent&)>
		KeyEventCallback;

	TFunction<void(
		FSlateApplication& SlateApp, const TArray<FInputChord>&)>
		SequenceCallback;

	~FTrieNode()
	{
		for (auto& Pair : Children)
			delete Pair.Value;
	}
};

DECLARE_MULTICAST_DELEGATE(FOnUMPreProcessorInputInit);
DECLARE_MULTICAST_DELEGATE_OneParam(FUMOnCountPrefix, FString /* New Prefix */);
DECLARE_MULTICAST_DELEGATE(FUMOnResetSequence);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnVimModeChanged, const EVimMode);

class FUMInputPreProcessor : public IInputProcessor
{
public:
	FUMInputPreProcessor();
	~FUMInputPreProcessor();

	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp,
		TSharedRef<ICursor> Cursor) override;

	virtual bool HandleKeyDownEvent(
		FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override;

	virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;

	static TSharedPtr<FUMInputPreProcessor> Get();
	static void								Initialize();
	static bool								IsInitialized();

	bool IsVimSwitch(const FKeyEvent& InKeyEvent);
	void SwitchVimModes(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);
	bool TrackCountPrefix(const FKeyEvent& InKeyEvent);

	void SetMode(EVimMode NewMode);

	void InitializeKeyBindings();

	void DebugInvalidWeakPtr(EUMCallbackType CallbackType);

	/////////////////////////////////////////////////////////////////////////
	// ~ Add Key Bindings ~
	//

	/**
	 * Finds or creates a trie node for the given input sequence
	 * @param Sequence - Array of input chords representing the key sequence
	 * @return Pointer to the found or created trie node
	 */
	FTrieNode* FindOrCreateTrieNode(const TArray<FInputChord>& Sequence);

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
					DebugInvalidWeakPtr(EUMCallbackType::NoParam);
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
					DebugInvalidWeakPtr(EUMCallbackType::NoParam);
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
					DebugInvalidWeakPtr(EUMCallbackType::KeyEventParam);
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
					DebugInvalidWeakPtr(EUMCallbackType::KeyEventParam);
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
					DebugInvalidWeakPtr(EUMCallbackType::SequenceParam);
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
					DebugInvalidWeakPtr(EUMCallbackType::SequenceParam);
			});
	}

	/////////////////////////////////////////////////////////////////////////

	// Process the current sequence and execute callbacks if matched
	bool ProcessKeySequence(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	// Reset the input sequence
	void ResetSequence(FSlateApplication& SlateApp);
	void ResetBufferVisualizer(FSlateApplication& SlateApp);

	FOnVimModeChanged& GetOnVimModeChanged();

	/** Registers a callback for Vim mode changes */
	void RegisterOnVimModeChanged(TFunction<void(const EVimMode)> Callback);

	/** Unregisters a specific callback */
	void UnregisterOnVimModeChanged(const void* CallbackOwner);

	static FInputChord GetChordFromKeyEvent(const FKeyEvent& InKeyEvent);

	static bool GetStrDigitFromKey(const FKey& InKey, FString& OutStr);

	void SimulateMultipleTabEvent(FSlateApplication& SlateApp, int32 TimesToRepeat);

	void DebugKeyEvent(const FKeyEvent& InKeyEvent);

	bool IsSimulateEscapeKey(
		FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	static void ToggleNativeInputHandling(const bool bNativeHandling);

	// Trie Root Node
	FTrieNode* TrieRoot = nullptr;

	// Current Input Sequence
	TArray<FInputChord> CurrentSequence;

public:
	static TSharedPtr<FUMInputPreProcessor> InputPreProcessor;
	static FOnUMPreProcessorInputInit		OnUMPreProcessorInputInit;

	TWeakPtr<FUMInputPreProcessor> WeakInputPreProcessor;
	static bool					   bNativeInputHandling;
	EUMHelpersLogMethod			   UMHelpersLogMethod{ EUMHelpersLogMethod::PrintToScreen };
	EVimMode					   VimMode{ EVimMode::Insert };

	TWeakPtr<SUMBufferVisualizer> BufferVisualizer; // Pointer to the visualizer
	FString						  CurrentBuffer;	// Current buffer contents

	// Delegates
	FOnVimModeChanged  OnVimModeChanged;
	FUMOnCountPrefix   OnCountPrefix;
	FUMOnResetSequence OnResetSequence;

	bool bVisualLog{ true };
	bool bConsoleLog{ false };
};
