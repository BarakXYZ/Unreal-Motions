// FUMKeyEvent.h

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/Optional.h"
#include "ModifierKeysStateHash.h" // The header we made above
#include "UObject/ObjectMacros.h"
// #include "UMKeyEvent.generated.h"

/**
 * Custom event struct that mirrors FKeyEvent’s data
 * but is hashable and comparable for usage in TMap/TSet.
 */
// USTRUCT(BlueprintType)
struct FUMKeyEvent
{
	// GENERATED_BODY()

public:
	/** The actual key that was pressed or released. */
	UPROPERTY(BlueprintReadWrite, Category = "UMKeyEvent")
	FKey Key;

	/** The set of modifier keys held when the event occurred. */
	UPROPERTY(BlueprintReadWrite, Category = "UMKeyEvent")
	FModifierKeysState ModifierKeys;

	/** The user index (usually 0 for single-user, but can be >0 for multi-user scenarios). */
	UPROPERTY(BlueprintReadWrite, Category = "UMKeyEvent")
	int32 UserIndex = 0;

	/** True if this is an auto-repeated keystroke. */
	UPROPERTY(BlueprintReadWrite, Category = "UMKeyEvent")
	bool bIsRepeat = false;

	/** The character code, if any. (0 if not a character event) */
	UPROPERTY(BlueprintReadWrite, Category = "UMKeyEvent")
	uint32 CharacterCode = 0;

	/** The raw key code from the hardware (often used on Windows). */
	UPROPERTY(BlueprintReadWrite, Category = "UMKeyEvent")
	uint32 KeyCode = 0;

	/** Optional: The input device identifier from which this event originated. */
	// If you don’t need this, just omit it.
	FInputDeviceId InputDeviceId = FInputDeviceId::CreateFromInternalId(0);

public:
	/** Default constructor. */
	FUMKeyEvent() = default;

	/**
	 * Construct from an FKeyEvent. This makes it easy to convert engine events
	 * to your custom struct on the fly.
	 */
	FUMKeyEvent(const FKeyEvent& InEvent)
	{
		Key = InEvent.GetKey();
		ModifierKeys = InEvent.GetModifierKeys(); // FInputEvent method
		UserIndex = InEvent.GetUserIndex();		  // from FInputEvent
		bIsRepeat = InEvent.IsRepeat();			  // from FInputEvent
		CharacterCode = InEvent.GetCharacter();
		KeyCode = InEvent.GetKeyCode();
		// If you want to keep the device ID from FInputEvent:
		InputDeviceId = InEvent.GetInputDeviceId();
	}

	/** Equality operator to allow TMap lookups, etc. */
	bool operator==(const FUMKeyEvent& Other) const
	{
		return Key == Other.Key && ModifierKeys == Other.ModifierKeys && UserIndex == Other.UserIndex && bIsRepeat == Other.bIsRepeat && CharacterCode == Other.CharacterCode && KeyCode == Other.KeyCode && InputDeviceId == Other.InputDeviceId;
	}

	/** Optional: For convenience, you might also define operator!= */
	bool operator!=(const FUMKeyEvent& Other) const
	{
		return !(*this == Other);
	}

	/** Hash function so we can store these in TMap/TSet. */
	friend FORCEINLINE uint32 GetTypeHash(const FUMKeyEvent& InUMKeyEvent)
	{
		// Start with hashing the FKey
		// uint32 HashVal = ::GetTypeHash(InUMKeyEvent.Key);
		uint32 HashVal = GetTypeHash(InUMKeyEvent.Key);

		// Incorporate the modifier keys
		HashVal = HashCombine(HashVal, ::GetTypeHash(InUMKeyEvent.ModifierKeys));

		// UserIndex can be hashed directly
		HashVal = HashCombine(HashVal, ::GetTypeHash(InUMKeyEvent.UserIndex));

		// bIsRepeat is a bool, combine by shifting
		HashVal = HashCombine(HashVal, (InUMKeyEvent.bIsRepeat ? 0x1u : 0x0u));

		// CharacterCode, KeyCode
		HashVal = HashCombine(HashVal, ::GetTypeHash(InUMKeyEvent.CharacterCode));
		HashVal = HashCombine(HashVal, ::GetTypeHash(InUMKeyEvent.KeyCode));

		// If we are storing InputDeviceId, incorporate that:
		if (InUMKeyEvent.InputDeviceId.IsValid())
		{
			HashVal = HashCombine(HashVal, ::GetTypeHash(InUMKeyEvent.InputDeviceId.GetId()));
		}
		else
		{
			// Optionally combine something for invalid device ID
			HashVal = HashCombine(HashVal, 0);
		}

		return HashVal;
	}
};
