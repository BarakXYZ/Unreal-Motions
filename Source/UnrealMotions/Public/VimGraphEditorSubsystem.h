#pragma once

#include "BlueprintEditor.h"
#include "Framework/Application/SlateApplication.h"
#include "Input/Events.h"
#include "SNodePanel.h"
#include "UMLogger.h"
#include "SGraphPanel.h"
#include "VimInputProcessor.h"
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
	void AddNodeToPin(FSlateApplication& SlateApp, UEdGraphPin* InPin, UEdGraphNode* ParentNode, TSharedRef<SGraphPanel> GraphPanel, bool bIsAppendingNode);
	void AddNodeToPin(FSlateApplication& SlateApp, const TSharedRef<SGraphPin> InPin);
	void Log_AddNodeToPin(bool bIsAppendingNode);

	void ZoomGraph(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	// void

	// Natively Shift + Delete deletes and keep the nodes connected
	// "Zoom to Graph Extents" is also useful to zoom out to see all nodes.

	void ZoomToFit(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	void HandleVimNodeNavigation(FSlateApplication& SlateApp, const TArray<FInputChord>& InSequence);

	void HandleGraphPanelPanning(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);
	void StopGraphPanelPanning(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	/**
	 * Shifts the GraphPanel's view so that the specified Node is within
	 * a "safe zone" inside the panel. The safe zone is a percentage of
	 * the panel’s size (e.g. 0.2f => 20% from each side).
	 */
	void AdjustViewIfNodeOutOfBounds(
		const TSharedRef<SGraphPanel> InGraphPanel,
		const TSharedRef<SGraphNode>  InGraphNode,
		float						  SafeMarginPercent = 0.2f);

	UEdGraphPin* GetFirstOrLastLinkedPinFromPin(const TSharedRef<SGraphPanel> GraphPanel, UEdGraphPin* InPin, EEdGraphPinDirection TargetDir);

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
	void DebugNodeAndPinsTypes();

	FBlueprintEditor*		 GetBlueprintEditor(const UEdGraph* ActiveGraphObj);
	TSharedPtr<SGraphEditor> GetGraphEditor(const TSharedRef<SWidget> InWidget);

	UEdGraphNode* FindEdgeNodeInChain(
		const TArray<UEdGraphNode*>& SelectedNodes, bool bFindFirstNode);

	UEdGraphNode* FindFarthestNode(
		const TArray<UEdGraphNode*>&					  Candidates,
		const TMap<UEdGraphNode*, TArray<UEdGraphNode*>>& OutgoingConnections,
		bool											  bFindFirstNode);

	void MoveConnectedNodesToRight(UEdGraphNode* StartNode, float OffsetX);

	void HandleOnContextBindingChanged(EUMContextBinding NewContext, const TSharedRef<SWidget> NewWidget);

	void HandleOnGraphChanged(const FEdGraphEditAction& InAction);
	void HandleOnSelectionChanged(const FGraphPanelSelectionSet& GraphPanelSelectionSet);
	void UnhookFromActiveGraphPanel();
	void DeleteNode(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	void ProcessNodeClick(FSlateApplication& SlateApp, const TSharedRef<SWidget> InWidget);

	void AddNodeToHighlightedPin(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	void ClickOnInteractableWithinPin(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	void HighlightPinForSelectedNode(
		FSlateApplication&			  SlateApp,
		const TSharedRef<SGraphPanel> GraphPanel,
		const TSharedRef<SGraphNode>  SelectedNode);

	void HighlightPinForSelectedNode(
		FSlateApplication&			  SlateApp,
		const TSharedRef<SGraphPanel> GraphPanel);

	bool TryGetNearestLinkedPin(
		const TArray<UEdGraphPin*>& InSortedObjPins, UEdGraphPin*& OutPin,
		int32 TrackedPinIndex, EEdGraphPinDirection FollowDir);

	bool TryGetParallelPin(
		const TArray<TSharedRef<SWidget>> InWidgetPins,
		int32 BasePinIndex, EEdGraphPinDirection TargetDir,
		TSharedPtr<SGraphPin>& OutPinWidget, bool bShouldMatchExactIndex = false);

	void OnVimModeChanged(const EVimMode NewVimMode);

	FUMLogger					Logger;
	int32						NodeCounter;
	EVimMode					PreviousVimMode{ EVimMode::Insert }, CurrentVimMode{ EVimMode::Insert };
	EUMContextBinding			CurrentContext{ EUMContextBinding::Generic };
	FDelegateHandle				DelegateHandle_OnKeyUpEvent;
	FTimerHandle				TimerHandle_GraphPanning;
	TArray<FKey>				PressedPanningKeys;
	FVector2D					CurrentPanelOffset;
	const TMap<FKey, FVector2D> PanOffsetByMotion{
		{ EKeys::H, FVector2D(-1.0, 0.0) },
		{ EKeys::J, FVector2D(0.0, 1.0) },
		{ EKeys::K, FVector2D(0.0, -1.0) },
		{ EKeys::L, FVector2D(1.0, 0.0) },
	};

	// We’ll store the original positions of any nodes we shift
	TMap<UEdGraphNode*, FVector2D> ShiftedNodesOriginalPositions;

	// We’ll track whether we’re currently in a “shifted” state
	// so we know whether to revert on menu close.
	bool bNodesWereShifted = false;

	struct FGraphSelectionTracker
	{
		TWeakPtr<SGraphPanel>				 GraphPanel;
		TWeakPtr<SGraphNode>				 GraphNode;
		TWeakPtr<SGraphPin>					 GraphPin;
		TArray<TWeakObjectPtr<UEdGraphNode>> VisitedNodesStack;
		static UVimGraphEditorSubsystem*	 VimGraphOwner;

		FGraphSelectionTracker() = default;
		FGraphSelectionTracker(
			TWeakPtr<SGraphPanel> InGraphPanel,
			TWeakPtr<SGraphNode>  InGraphNode,
			TWeakPtr<SGraphPin>	  InGraphPin);

		bool IsValid();
		bool IsTrackedNodeSelected();

		/**
		 * Handles node selection logic in both Visual and Normal modes.
		 *
		 * In Visual mode, we track visited nodes in a stack-like structure:
		 *   - Forward moves (to a new node) push that node onto the stack and select it.
		 *   - Backward moves (to the node beneath the top of the stack) pop the old top node and deselect it.
		 *
		 * In Normal mode, we simply single-select the new node.
		 *
		 * @param NewNode      The node we're moving to.
		 * @param OldNode      The node we're moving from.
		 * @param InGraphPanel The active SGraphPanel for selection management.
		 */
		void HandleNodeSelection(UEdGraphNode* NewNode, UEdGraphNode* OldNode,
			const TSharedRef<SGraphPanel> GraphPanel);
	};

	FGraphSelectionTracker GraphSelectionTracker;

	FDelegateHandle					  OnGraphChangedHandler;
	SGraphEditor::FOnSelectionChanged OnSelectionChangedOriginDelegate;
	TWeakPtr<SGraphPanel>			  ActiveGraphPanel;
};
