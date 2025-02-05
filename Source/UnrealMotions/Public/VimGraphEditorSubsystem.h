#pragma once

#include "BlueprintEditor.h"
#include "Input/Events.h"
#include "UMLogger.h"
#include "SGraphPanel.h"
#include "EditorSubsystem.h"
#include "VimGraphEditorSubsystem.generated.h"

/**
 *
 */
UCLASS()
class UNREALMOTIONS_API UVimGraphEditorSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * Controlled via the Unreal Motions config; should or shouldn't create the
	 * subsystem at all.
	 */
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	/**
	 * @param Collection The subsystem collection being initialized
	 */
	virtual void Initialize(FSubsystemCollectionBase& Collction) override;

	virtual void Deinitialize() override;

	void BindVimCommands();

	void AddNode(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	void ZoomGraph(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	// void

	// Natively Shift + Delete deletes and keep the nodes connected
	// "Zoom to Graph Extents" is also useful to zoom out to see all nodes.

	void ZoomToFit(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	void OnNodeCreationMenuClosed(
		FSlateApplication& SlateApp,
		UEdGraphPin* DraggedFromPin, bool bIsAppendingNode);

	bool GetPinToLinkedPinDelta(
		const TSharedRef<SGraphNode> InNode, UEdGraphPin* InPin,
		const TSharedRef<SGraphNode> LinkedToNode, double& Delta);

	bool GetNewNodeAlignedPosition();

	FVector2D GetAlignedNodePositionX(
		const TSharedRef<SGraphNode> OriginNode,
		const TSharedRef<SGraphNode> LinkedNode,
		bool						 bIsAppendingNode);

	void CollectDownstreamNodes(
		UEdGraphNode*		 StartNode,
		TSet<UEdGraphNode*>& OutNodes,
		bool				 bIsAppending);

	void ShiftNodesForSpace(
		const TSharedPtr<SGraphPanel>& GraphPanel,
		const TSet<UEdGraphNode*>&	   NodesToMove,
		float						   ShiftAmountX);

	void RevertShiftedNodes(const TSharedPtr<SGraphPanel>& GraphPanel);

	bool IsWasNewNodeCreated();

	bool IsValidZoom(const FString InZoomLevelStr);

	bool ConnectNewNodeToPrevConnection(
		UEdGraphPin* OriginPin, const TSharedRef<SGraphNode> NewNode);

	void DebugEditor();

	FBlueprintEditor*		 GetBlueprintEditor(const UEdGraph* ActiveGraphObj);
	TSharedPtr<SGraphEditor> GetGraphEditor(const TSharedRef<SWidget> InWidget);

	UEdGraphNode* FindEdgeNodeInChain(
		const TArray<UEdGraphNode*>& SelectedNodes, bool bFindFirstNode);

	UEdGraphNode* FindFarthestNode(
		const TArray<UEdGraphNode*>&					  Candidates,
		const TMap<UEdGraphNode*, TArray<UEdGraphNode*>>& OutgoingConnections,
		bool											  bFindFirstNode);

	const TSharedPtr<SGraphPanel> TryGetActiveGraphPanel(FSlateApplication& SlateApp);

	void MoveConnectedNodesToRight(UEdGraphNode* StartNode, float OffsetX);

	FUMLogger Logger;
	int32	  NodeCounter;

	// We’ll store the original positions of any nodes we shift
	TMap<UEdGraphNode*, FVector2D> ShiftedNodesOriginalPositions;

	// We’ll track whether we’re currently in a “shifted” state
	// so we know whether to revert on menu close.
	bool bNodesWereShifted = false;
};
