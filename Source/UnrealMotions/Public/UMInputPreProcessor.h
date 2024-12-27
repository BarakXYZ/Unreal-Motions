#pragma once

#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
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
struct FTrieNode
{
	TMap<FKey, FTrieNode*> Children; // Child nodes for each key
	TFunction<void()>	   Callback; // Callback function to execute when a sequence matches

	~FTrieNode()
	{
		// Clean up child nodes recursively
		for (auto& Pair : Children)
		{
			delete Pair.Value;
		}
	}
};

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

	void SwitchVimModes(const FKeyEvent& InKeyEvent);

	void SetMode(EVimMode NewMode);

	void Callback_JumpNotification();

	// TEST
	// Set up the trie with key bindings
	void InitializeKeyBindings();

	// Add a new binding
	void AddKeyBinding(const TArray<FKey>& Sequence, TFunction<void()> Callback);

	template <typename ObjectType>
	void AddKeyBinding(const TArray<FKey>& Sequence, ObjectType* Object,
		void (ObjectType::*MemberFunction)());

	// Process the current sequence and execute callbacks if matched
	bool ProcessKeySequence(const FKey& Key);

	// Reset the input sequence
	void ResetSequence();

	// Trie Root Node
	FTrieNode* TrieRoot = nullptr;

	// Current Input Sequence
	TArray<FKey> CurrentSequence;

private:
	EUMHelpersLogMethod UMHelpersLogMethod{ EUMHelpersLogMethod::PrintToScreen };
	EVimMode			VimMode{ EVimMode::Insert };

	FWidgetDrawerConfig			  MyDrawerConfig{ TEXT("VIM") };
	TWeakPtr<SUMBufferVisualizer> BufferVisualizer; // Pointer to the visualizer
	FString						  CurrentBuffer;	// Current buffer contents

	bool bVisualLog{ true };
	bool bConsoleLog{ false };
};
