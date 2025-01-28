#pragma once

#include "CoreMinimal.h"
#include "Input/Events.h"
#include "SUMHintMarker.h"

/**
 * Trie node for hint markers. Manages a map of child nodes keyed by an InputChord,
 * and accumulates an array of *all* markers in the sub-tree.
 * If this node is terminal,
 * that means it corresponds to exactly one specific label and thus has exactly
 * one unique marker.
 */
struct FUMHintWidgetTrieNode : public TSharedFromThis<FUMHintWidgetTrieNode>
{
	/** For each FInputChord "edge," which child node do we go to? */
	TMap<FInputChord, TSharedPtr<FUMHintWidgetTrieNode>> Children;

	/**
	 * We need a reference to the Parent Node for when the user is pressing
	 * "BackSpace", which means we need to climb up 1 node (to the parent )
	 */
	TWeakPtr<FUMHintWidgetTrieNode> Parent;

	/**
	 * All hint markers in this node’s sub-tree. That is:
	 * - If bIsTerminal == true, then at least one of these markers is the
	 *   unique marker for the label that ends here.
	 * - If this node is just a prefix node, it might have multiple markers
	 *   (for multiple possible completions).
	 */
	TArray<TWeakPtr<SUMHintMarker>> HintMarkers;

	/** Indicates that this node corresponds to the end of a valid label string. */
	bool bIsTerminal = false;
};
