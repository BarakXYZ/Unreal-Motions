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
	TMap<FInputChord, FKeyChordTrieNode*> Children;

	// Which callback type is set for this node?
	EUMKeyBindingCallbackType CallbackType = EUMKeyBindingCallbackType::None;

	// Up to three callback function pointers
	TFunction<void()> NoParamCallback;

	TFunction<void(
		FSlateApplication& SlateApp, const FKeyEvent&)>
		KeyEventCallback;

	TFunction<void(
		FSlateApplication& SlateApp, const TArray<FInputChord>&)>
		SequenceCallback;

	~FKeyChordTrieNode()
	{
		for (auto& Pair : Children)
			delete Pair.Value;
	}
};
