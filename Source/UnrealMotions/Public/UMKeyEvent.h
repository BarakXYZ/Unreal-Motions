// FUMKeyEvent.h

#pragma once

#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "UObject/ObjectMacros.h"
#include "GenericPlatform/GenericApplication.h"
#include "Framework/Application/IInputProcessor.h"

/** Equality operator for FModifierKeysState (not provided by default). */
FORCEINLINE bool operator==(const FModifierKeysState& LHS, const FModifierKeysState& RHS)
{
	return LHS.IsShiftDown() == RHS.IsShiftDown() && LHS.IsAltDown() == RHS.IsAltDown() && LHS.IsControlDown() == RHS.IsControlDown() && LHS.IsCommandDown() == RHS.IsCommandDown() && LHS.AreCapsLocked() == RHS.AreCapsLocked();
	// (We typically ignore "IsRepeat()" in the standard FModifierKeysState
	//  because it’s not actually used by the engine to represent an ongoing
	//  'held' state.  But if you want to incorporate that, feel free.)
}

/** Hash function for FModifierKeysState. */
FORCEINLINE uint32 GetTypeHash(const FModifierKeysState& InState)
{
	// We’ll fold its booleans into a bitmask.
	uint8 Flags = 0;
	Flags |= (InState.IsShiftDown() ? 1 << 0 : 0);
	Flags |= (InState.IsAltDown() ? 1 << 1 : 0);
	Flags |= (InState.IsControlDown() ? 1 << 2 : 0);
	Flags |= (InState.IsCommandDown() ? 1 << 3 : 0);
	Flags |= (InState.AreCapsLocked() ? 1 << 4 : 0);

	return ::GetTypeHash(Flags);
}

/**
 * Custom event struct that is a minimal replica of FKeyEvent's data.
 * This struct provides hashable and comparable functionality for usage in TMap/TSet containers.
 * It stores the pressed key information along with any modifier keys that were active during the event.
 * Unlike FKeyEvent, this struct can be used as a key in TMap or stored in TSet.
 */
struct FUMKeyEvent
{

public:
	/** The actual key that was pressed or released. */
	UPROPERTY(BlueprintReadWrite, Category = "UMKeyEvent")
	FKey Key;

	/** The set of modifier keys held when the event occurred. */
	UPROPERTY(BlueprintReadWrite, Category = "UMKeyEvent")
	FModifierKeysState ModifierKeys; // Default to all false

public:
	/** Default constructor. */
	FUMKeyEvent() = default;

	/**
	 * Construct from an FKeyEvent. Copy the key and modifiers state.
	 */
	FUMKeyEvent(const FKeyEvent& InEvent)
	{
		Key = InEvent.GetKey();
		ModifierKeys = InEvent.GetModifierKeys(); // FInputEvent method
	}

	/**
	 * Construct from a modifier key and an FKeyEvent. Copy the key and modifiers state.
	 */
	FUMKeyEvent(const FInputChord& InputChord)
	{
		FKeyEvent				  KeyEvent;
		const FModifierKeysState& ModState = KeyEvent.GetModifierKeys();

		FInputChord Chord(
			KeyEvent.GetKey(),		  // The key
			ModState.IsShiftDown(),	  // bShift
			ModState.IsControlDown(), // bCtrl
			ModState.IsAltDown(),	  // bAlt
			ModState.IsCommandDown()  // bCmd
		);
		// Key = InputChord.Key;
		// ModifierKeys = TempModKeys;
	}

	/**
	 * Copy the key and keep all modifiers false.
	 */
	FUMKeyEvent(const FKey& InKey)
	{
		Key = InKey;
	}

	/** Equality operator to allow TMap lookups, etc. */
	bool operator==(const FUMKeyEvent& Other) const
	{
		return Key == Other.Key && ModifierKeys == Other.ModifierKeys;
	}

	/**
	 * Inequality operator for comparing two FUMKeyEvent instances.
	 * @param Other The FUMKeyEvent to compare against.
	 * @return True if the events are not equal, false otherwise.
	 */
	bool operator!=(const FUMKeyEvent& Other) const
	{
		return !(*this == Other);
	}

	/** Hash function so we can store these in TMap/TSet. */
	friend FORCEINLINE uint32 GetTypeHash(const FUMKeyEvent& InUMKeyEvent)
	{
		// Start with hashing the FKey
		uint32 HashVal = GetTypeHash(InUMKeyEvent.Key);

		// Incorporate the modifier keys
		HashVal = HashCombine(HashVal, ::GetTypeHash(InUMKeyEvent.ModifierKeys));

		return HashVal;
	}
};
