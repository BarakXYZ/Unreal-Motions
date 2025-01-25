#pragma once

#include "Input/Events.h"
#include "SUMHintMarker.h"

struct FUMHintWidgetTrieNode
{
	TMap<FInputChord, FUMHintWidgetTrieNode*> Children;

	/** Links to all Hint Markers in this node */
	TArray<TWeakPtr<SUMHintMarker>> HintMarkers;

	/** Mark if this node represents a complete hint */
	bool bIsTerminal = false;

	~FUMHintWidgetTrieNode()
	{
		for (auto& Pair : Children)
			delete Pair.Value;
	}
};
