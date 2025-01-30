#pragma once

#include "CoreMinimal.h"
#include "Input/Events.h"

enum class EUMKeyBindingCallbackType : uint8
{
	None,
	NoParam,
	KeyEventParam,
	SequenceParam
};

struct FKeyChordTrieNode
{
	// Now use TSharedPtr instead of raw pointers
	TMap<FInputChord, TSharedPtr<FKeyChordTrieNode>> Children;

	// Which callback type is set for this node?
	EUMKeyBindingCallbackType CallbackType = EUMKeyBindingCallbackType::None;

	// Callback functions
	TFunction<void()> NoParamCallback;

	TFunction<void(FSlateApplication& SlateApp, const FKeyEvent&)> KeyEventCallback;

	TFunction<void(FSlateApplication& SlateApp, const TArray<FInputChord>&)> SequenceCallback;

	// No manual destructor needed – TSharedPtr handles cleanup automatically
	// ~FKeyChordTrieNode() = default;
};
