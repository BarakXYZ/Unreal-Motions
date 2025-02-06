#include "VimGraphEditorSubsystem.h"
#include "BlueprintEditor.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraphNode_Comment.h"
#include "Framework/Application/SlateApplication.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UMConfig.h"
#include "GraphEditorModule.h"
#include "UMSlateHelpers.h"
#include "VimInputProcessor.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "EdGraphSchema_K2_Actions.h"
#include "BlueprintNodeSpawner.h"
#include "K2Node_CallFunction.h"
#include "Kismet/KismetSystemLibrary.h"
#include "BlueprintNodeBinder.h"
#include "SGraphPanel.h"
#include "UMSlateHelpers.h"
#include "UMInputHelpers.h"
#include "KismetPins/SGraphPinExec.h"
#include "SGraphPin.h"
#include "UMEditorHelpers.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "GraphEditor.h"
#include "UMFocuserEditorSubsystem.h"

// DEFINE_LOG_CATEGORY_STATIC(LogVimGraphEditorSubsystem, NoLogging, All); // Prod
DEFINE_LOG_CATEGORY_STATIC(LogVimGraphEditorSubsystem, Log, All); // Dev

#define LOCTEXT_NAMESPACE "VimGraphEditorSubsystem"

bool UVimGraphEditorSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	TSharedRef<FUMConfig> Config = FUMConfig::Get();
	return Config->IsVimEnabled();
}

void UVimGraphEditorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Logger = FUMLogger(&LogVimGraphEditorSubsystem);

	BindVimCommands();

	// Start listening when Context Binding is changed directly
	// I wonder about this, it's cute. But is it really needed?
	// FCoreDelegates::OnPostEngineInit.AddLambda([this]() {
	// 	if (UUMFocuserEditorSubsystem* Focuser =
	// 			GEditor->GetEditorSubsystem<UUMFocuserEditorSubsystem>())
	// 	{
	// 		Focuser->OnBindingContextChanged.AddUObject(
	// 			this, &UVimGraphEditorSubsystem::HandleOnContextBindingChanged);
	// 	}
	// });

	Super::Initialize(Collection);
}

void UVimGraphEditorSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UVimGraphEditorSubsystem::BindVimCommands()
{
	TSharedRef<FVimInputProcessor> VimInputProcessor = FVimInputProcessor::Get();

	TWeakObjectPtr<UVimGraphEditorSubsystem> WeakGraphSubsystem =
		MakeWeakObjectPtr(this);

	/* Append after the last selected node */
	VimInputProcessor->AddKeyBinding_KeyEvent(
		EUMContextBinding::GraphEditor,
		{ EKeys::A },
		WeakGraphSubsystem,
		&UVimGraphEditorSubsystem::AddNode);

	/* Insert before the last selected node */
	VimInputProcessor->AddKeyBinding_KeyEvent(
		EUMContextBinding::GraphEditor,
		{ EKeys::I },
		WeakGraphSubsystem,
		&UVimGraphEditorSubsystem::AddNode);

	/* Append after last node in the chain */
	VimInputProcessor->AddKeyBinding_KeyEvent(
		EUMContextBinding::GraphEditor,
		{ FInputChord(EModifierKey::Shift, EKeys::A) },
		WeakGraphSubsystem,
		&UVimGraphEditorSubsystem::AddNode);

	/* Insert before the last node in the chain */
	VimInputProcessor->AddKeyBinding_KeyEvent(
		EUMContextBinding::GraphEditor,
		{ FInputChord(EModifierKey::Shift, EKeys::I) },
		WeakGraphSubsystem,
		&UVimGraphEditorSubsystem::AddNode);

	/** Zoom-In -> Ctrl + '=' ('+') */
	VimInputProcessor->AddKeyBinding_KeyEvent(
		EUMContextBinding::GraphEditor,
		{ FInputChord(EModifierKey::Control, EKeys::Equals) },
		WeakGraphSubsystem,
		&UVimGraphEditorSubsystem::ZoomGraph);

	/** Zoom-Out -> Ctrl + '-' */
	VimInputProcessor->AddKeyBinding_KeyEvent(
		EUMContextBinding::GraphEditor,
		{ FInputChord(EModifierKey::Control, EKeys::Hyphen) },
		WeakGraphSubsystem,
		&UVimGraphEditorSubsystem::ZoomGraph);

	/** Zoom-to-Fit, center selected nodes -> 'zz' */
	VimInputProcessor->AddKeyBinding_KeyEvent(
		EUMContextBinding::GraphEditor,
		// { FInputChord(EModifierKey::Control, EKeys::Hyphen) },
		{ EKeys::Z, EKeys::Z },
		WeakGraphSubsystem,
		&UVimGraphEditorSubsystem::ZoomToFit);

	/** Zoom-to-Fit, all nodes in graph -> Shift+'Z' */
	VimInputProcessor->AddKeyBinding_KeyEvent(
		EUMContextBinding::GraphEditor,
		{ FInputChord(EModifierKey::Shift, EKeys::Z) },
		WeakGraphSubsystem,
		&UVimGraphEditorSubsystem::ZoomToFit);

	/** Debug */
	VimInputProcessor->AddKeyBinding_NoParam(
		EUMContextBinding::GraphEditor,
		{ EKeys::SpaceBar, EKeys::D, EKeys::G },
		WeakGraphSubsystem,
		&UVimGraphEditorSubsystem::DebugNodeAndPinsTypes);

	// About the pin motions:
	// Upon the user focusing a node via Hint Markers we have access to the
	// future selected node, thus we can setup the pin visualization easily.
	// We have the "AddOnGraphChangedHandler" to know when nodes are deleted
	// or added. Also, we can start listening to OnMouseButtonDown from the
	// VimProcessor and things like that when we're entering the GraphEditor
	// context. Also the OnKeyButtonDown is available, etc.
	// Other than that though, we then user tries to navigate pins via HJKL
	// we can simply upon the event check we have everything we need (i.e.
	// 1 selected node we know we're operating on, etc.) so there isn't a real
	// need to know about focus changing inside the node graph too much.
	// It will be the most robust to build the HJKL event as independent as
	// possible and to not rely on too many outside delegates.
	//
	// Thinking more about this, we might still need to rely partially
	// one SelectionChanged. We can make it more robust by listening
	// to OnFocusChanged while we're on the GraphEditor context.
	// I think the OnSelectionChanged will be needed because we kind
	// of have to know when there are no nodes selected vs. any.
	// In these cases, we will have to hide the highlight overlay
	// for the pins. If we can think of a cleaner architecture
	// it will be great; but that's what I currently have.
	//
	// Maybe............ we can also have the following flow:
	// let's say we have a node with 6 inputs and 3 outputs.
	// In this case, when we press 'A' to append (and the
	// node is actually selected) we will have hint markers
	// popping for each of the output nodes with numbers 1-3.
	// So the user can easily complete 1-3 to select to which
	// pin he wants to append. Similarly for 'I' to insert,
	// it will pop 6 hint markers (1-6) for the 6 input pins
	// of the node. So i1, i2, i3... we send the cursor to
	// pull a string to insert a new node for the correct
	// pin index!
	// That actually sounds really cool, simple and also
	// fast and nice UX. Because needing to go to each
	// pin does sounds kind of weird honestly.
	// That way HJKL just navigates the nodes themselves
	// and we don't have to worry about highlighting and
	// navigating pins which feels really intense!
	// PROBLEM SOLVED!

	// Selection + Move HJKL
	VimInputProcessor->AddKeyBinding_KeyEvent(
		EUMContextBinding::GraphEditor,
		// { FInputChord(EModifierKey::Shift, EKeys::H) },
		{ EKeys::H },
		WeakGraphSubsystem,
		&UVimGraphEditorSubsystem::HandleVimNodeNavigation);

	VimInputProcessor->AddKeyBinding_KeyEvent(
		EUMContextBinding::GraphEditor,
		// { FInputChord(EModifierKey::Shift, EKeys::J) },
		{ EKeys::J },
		WeakGraphSubsystem,
		&UVimGraphEditorSubsystem::HandleVimNodeNavigation);

	VimInputProcessor->AddKeyBinding_KeyEvent(
		EUMContextBinding::GraphEditor,
		// { FInputChord(EModifierKey::Shift, EKeys::K) },
		{ EKeys::K },
		WeakGraphSubsystem,
		&UVimGraphEditorSubsystem::HandleVimNodeNavigation);

	VimInputProcessor->AddKeyBinding_KeyEvent(
		EUMContextBinding::GraphEditor,
		// { FInputChord(EModifierKey::Shift, EKeys::L) },
		{ EKeys::L },
		WeakGraphSubsystem,
		&UVimGraphEditorSubsystem::HandleVimNodeNavigation);

	// TODO:
	// 2. gg & G to go to focus first or last node in a chain
	// 3. Figure out movement inside the graph panel (wasd?)
	// 4. Movement between nodes & pins
	// 5. Finalize graph appending and insertion edge cases
}

void UVimGraphEditorSubsystem::AddNode(
	FSlateApplication& SlateApp,
	const FKeyEvent&   InKeyEvent)
{
	const TSharedPtr<SGraphPanel> GraphPanel = TryGetActiveGraphPanel(SlateApp);
	if (!GraphPanel.IsValid())
		return;

	Logger.Print(FString::Printf(TEXT("Zoom Amount (New): %f"), GraphPanel->GetZoomAmount()), ELogVerbosity::Log, true);

	// In node graph, users cannot drag pins from a zoom level below -5.
	// it will only allow dragging nodes at this zoom level.
	// Thus we want to early return here & potentially notify the user.
	if (GraphPanel->GetZoomAmount() <= 0.25f)
		return;
	// Optionally we can ZoomToFit if not valid zoom, but idk; it's a diff UX
	// and will also require a delay, etc.
	// GraphPanel->ZoomToFit(true);

	const TArray<UEdGraphNode*>
		SelNodes = GraphPanel->GetSelectedGraphNodes();
	if (SelNodes.IsEmpty())
		return;

	const FKey InKey = InKeyEvent.GetKey();
	const bool bIsAppendingNode = InKey == FKey(EKeys::A);

	UEdGraphNode* NodeObj = InKeyEvent.IsShiftDown()
		// Shift + [I] / [A] -> Get the first or last node in the entire chain.
		? FindEdgeNodeInChain(SelNodes, bIsAppendingNode)

		// [I] / [A] -> Use the last selected node.
		: SelNodes[SelNodes.Num() - 1];

	if (!NodeObj)
		return;

	const TSharedPtr<SGraphNode> NodeWidget = // Get widget representation.
		GraphPanel->GetNodeWidgetFromGuid(NodeObj->NodeGuid);
	if (!NodeWidget.IsValid())
		return;

	// Identify the pin we care about (Out pin if appending, In pin if inserting)
	// We'll also see if that pin is connected to anything to decide if we shift
	UEdGraphPin* OriginPinObj = nullptr;

	TArray<TSharedRef<SWidget>> AllPins;
	NodeWidget->GetPins(AllPins);

	// Get either the Input ([I]nsert) or Output ([A]ppend) pin.
	const EEdGraphPinDirection TargetPinType =
		bIsAppendingNode ? EGPD_Output : EGPD_Input;

	for (const auto& Pin : AllPins)
	{
		if (!Pin->GetTypeAsString().Equals("SGraphPinExec"))
			continue;

		// Cast to the specific widget representation of this pin object
		// (as the array of pins is fetching them as SWidgets)
		const TSharedRef<SGraphPin> AsGraphPin =
			StaticCastSharedRef<SGraphPin>(Pin);

		// Verify the direction of the pin matches our TargetPinType (In | Out)
		UEdGraphPin* EdGraphPin = AsGraphPin->GetPinObj();
		if (!EdGraphPin || EdGraphPin->Direction != TargetPinType)
			continue; // Skip if there's a mismatch

		OriginPinObj = EdGraphPin;
		break;
	}

	if (!OriginPinObj) // Check if any node was actually found
		return;

	/** Final Pin found, drag a line from it and break afterwards. */
	AddNodeToPin(SlateApp, OriginPinObj, NodeObj, GraphPanel.ToSharedRef(), bIsAppendingNode);
}

void UVimGraphEditorSubsystem::OnNodeCreationMenuClosed(
	FSlateApplication& SlateApp,
	UEdGraphPin* DraggedFromPin, bool bIsAppendingNode)
{
	FVimInputProcessor::Get()->SetVimMode(SlateApp, EVimMode::Normal);

	if (!DraggedFromPin)
	{
		Logger.Print("Dragged From Pin is Invalid", ELogVerbosity::Error, true);
		return;
	}

	const TSharedPtr<SGraphPanel> GraphPanel = TryGetActiveGraphPanel(SlateApp);
	if (!GraphPanel.IsValid())
	{
		Logger.Print("Graph Panel is Invalid", ELogVerbosity::Error, true);
		return;
	}

	if (NodeCounter == GraphPanel->GetChildren()->Num())
	{
		Logger.Print("No new Nodes were created...", ELogVerbosity::Log, true);
		// Revert if we had shifted
		RevertShiftedNodes(GraphPanel);
		return;
	}

	Logger.Print("New node was created!", ELogVerbosity::Log, true);
	// GraphPanel->SelectionManager.SetSelectionSet()

	// Get and neatly reposition the newly created to node.
	TArray<UEdGraphNode*> SelNodes = GraphPanel->GetSelectedGraphNodes();
	if (SelNodes.Num() != 1 || !SelNodes[0]) // Should have exactly 1 node
		return;
	const TSharedPtr<SGraphNode> NewNode =
		GraphPanel->GetNodeWidgetFromGuid(SelNodes[0]->NodeGuid);
	if (!NewNode.IsValid())
		return;
	const TSharedRef<SGraphNode> NewNodeRef = NewNode.ToSharedRef();

	// TODO: Refactor this
	TSet<UEdGraphNode*> NodesToShift;
	for (const auto& Pair : ShiftedNodesOriginalPositions)
	{
		if (Pair.Key)
			NodesToShift.Add(Pair.Key);
	}

	// Move all the nodes by the width of the new node
	if (!NodesToShift.IsEmpty())
		ShiftNodesForSpace(GraphPanel, NodesToShift, NewNode->GetDesiredSize().X);

	// Get the origin node ~~~~
	UEdGraphNode* NewNodeObj = NewNode->GetNodeObj();
	if (!NewNodeObj)
		return;

	// NOTE:
	// This is currently not the greatest way to get the target pin because
	// we may be operating on input / output pins with different indexes.
	// For when we will start to do that - we will need to preserve the index
	// of the input / output pin that we've been dragging from to get that
	// specifically (we can also maybe try to store its ID? we have FindPinById
	// I think)
	// UEdGraphPin* NewPin =
	// 	NewNodeObj->FindPinByPredicate([bIsAppendingNode](UEdGraphPin* Pin) {
	// 		return Pin->Direction == (bIsAppendingNode ? EGPD_Input : EGPD_Output);
	// 	});
	// if (!NewPin->HasAnyConnections())
	// 	return;
	// // NewNode->FindWidgetForPin(NewPin);
	// if (!NewPin->LinkedTo[0])
	// 	return;
	// TSharedPtr<SGraphNode> DraggedFromNode =
	// 	GraphPanel->GetNodeWidgetFromGuid(NewPin->LinkedTo[0]->GetOwningNode()->NodeGuid);
	// if (!DraggedFromNode.IsValid())
	// 	return;
	// Get the origin node ^^^^^

	// Get the Dragged From Node:
	// Simply passing it by a weak ptr doesn't seem to always work - especially
	// when new macros are created in the graph. Thus we fetch it like that:
	const TSharedPtr<SGraphNode> DraggedFromNode =
		GraphPanel->GetNodeWidgetFromGuid(
			DraggedFromPin->GetOwningNode()->NodeGuid);
	if (!DraggedFromNode.IsValid())
		return;

	// Align the X position of the Origin and new node
	SNodePanel::SNode::FNodeSet MovedNodes;
	NewNode->MoveTo(
		GetAlignedNodePositionX(DraggedFromNode.ToSharedRef(),
			NewNodeRef, bIsAppendingNode),
		MovedNodes);

	// Add the previous node to the selection and align the Y position too.
	GraphPanel->SelectionManager.SetNodeSelection(DraggedFromNode->GetNodeObj(), true);
	GraphPanel->StraightenConnections();
	GraphPanel->SelectionManager.SelectSingleNode(SelNodes[0]);

	// When inserting we also want to potentially connect the new node to any
	// existing nodes previous to our origin node (it will be disconnected
	// by default)
	if (bIsAppendingNode)
		return;

	ConnectNewNodeToPrevConnection(DraggedFromPin, NewNodeRef);
}

bool UVimGraphEditorSubsystem::ConnectNewNodeToPrevConnection(
	UEdGraphPin* OriginPin, const TSharedRef<SGraphNode> NewNode)
{
	if (!OriginPin->HasAnyConnections())
		return false;

	// Get the pin the origin node is currently connected to
	UEdGraphPin* PreviousLinkedPin = OriginPin->LinkedTo[0];
	if (!PreviousLinkedPin)
		return false;

	// Check if we're connected to an additional node (other than the NewNode)
	// if we're just connected to the NewNode, we're good to go and we can exit
	// This check is needed for when we have 0 connections when inserting a node.
	if (PreviousLinkedPin->GetOwningNode() == NewNode->GetNodeObj())
		return true;

	// TODO: I think we can refactor this and use the appropriate fetching methods
	// Unreal provides for finding Pins.
	TArray<TSharedRef<SWidget>> Pins;
	NewNode->GetPins(Pins);
	UEdGraphPin* NewNodePinObj = nullptr;
	for (const auto& Pin : Pins)
	{
		if (!Pin->GetTypeAsString().Equals("SGraphPinExec"))
			continue;

		// Cast to the specific widget representation of this pin object
		// (as the array of pins is fetching them as SWidgets)
		const TSharedRef<SGraphPin> AsGraphPin =
			StaticCastSharedRef<SGraphPin>(Pin);

		// Verify the direction of the pin matches our TargetPinType (In | Out)
		UEdGraphPin* EdGraphPin = AsGraphPin->GetPinObj();
		if (!EdGraphPin || EdGraphPin->Direction != EGPD_Input)
			continue; // Skip if there's a mismatch

		NewNodePinObj = EdGraphPin;
		break;
	}
	if (!NewNodePinObj)
		return false; // Pin wasn't found

	PreviousLinkedPin->BreakLinkTo(OriginPin);
	PreviousLinkedPin->MakeLinkTo(NewNodePinObj);
	return true;
}

FVector2D UVimGraphEditorSubsystem::GetAlignedNodePositionX(
	const TSharedRef<SGraphNode> OriginNode,
	const TSharedRef<SGraphNode> LinkedNode,
	bool						 bIsAppendingNode)
{
	FVector2D OldNodePos = OriginNode->GetPosition();
	FVector2D OldNodeSize = OriginNode->GetDesiredSize();
	FVector2D NewNodeSize = LinkedNode->GetDesiredSize();
	float	  NodeGap = 35.0f;

	return bIsAppendingNode
		? FVector2D(OldNodePos.X + OldNodeSize.X + NodeGap, OldNodePos.Y)
		: FVector2D(OldNodePos.X - (NewNodeSize.X + NodeGap), OldNodePos.Y);
}

bool UVimGraphEditorSubsystem::GetPinToLinkedPinDelta(
	const TSharedRef<SGraphNode> InNode, UEdGraphPin* InPin,
	const TSharedRef<SGraphNode> LinkedToNode, double& Delta)
{
	// Check this pin is connected to exactly one node (that can be fetched)
	if (InPin->LinkedTo.Num() != 1)
		return false;

	// NOTE:
	// We can potentially have an override in the future (if needed) and fetch
	// the new node manually from here. In our case we already have a ref; so
	// it's not really needed
	// InPin->LinkedTo[0]->GetOwningNode();

	TSharedPtr<SGraphPin> OriginPinWidget =
		InNode->FindWidgetForPin(InPin);

	TSharedPtr<SGraphPin> LinkedPinWidget =
		LinkedToNode->FindWidgetForPin(InPin->LinkedTo[0]);

	if (!OriginPinWidget.IsValid() || !LinkedPinWidget.IsValid())
		return false;

	Delta = OriginPinWidget->GetNodeOffset().Y - LinkedPinWidget->GetNodeOffset().Y;
	return true;
}

const TSharedPtr<SGraphPanel> UVimGraphEditorSubsystem::TryGetActiveGraphPanel(FSlateApplication& SlateApp)
{
	const TSharedPtr<SWidget> FocusedWidget = SlateApp.GetUserFocusedWidget(0);
	return (FocusedWidget.IsValid()
			   && FocusedWidget->GetTypeAsString().Equals("SGraphPanel"))
		? StaticCastSharedPtr<SGraphPanel>(FocusedWidget)
		: nullptr;
}

UEdGraphNode* UVimGraphEditorSubsystem::FindEdgeNodeInChain(
	const TArray<UEdGraphNode*>& SelectedNodes, bool bFindFirstNode)
{
	if (SelectedNodes.IsEmpty())
		return nullptr;

	TMap<UEdGraphNode*, TArray<UEdGraphNode*>> OutgoingConnections;
	TSet<UEdGraphNode*>						   InputNodes;

	// Build the connectivity graph
	for (UEdGraphNode* Node : SelectedNodes)
	{
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin->Direction == EGPD_Output)
			{
				for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
				{
					if (LinkedPin && SelectedNodes.Contains(LinkedPin->GetOwningNode()))
					{
						OutgoingConnections.FindOrAdd(Node).Add(LinkedPin->GetOwningNode());
						InputNodes.Add(LinkedPin->GetOwningNode());
					}
				}
			}
		}
	}

	// Find candidates: Either source nodes (for first) or sink nodes (for last)
	TArray<UEdGraphNode*> Candidates;
	for (UEdGraphNode* Node : SelectedNodes)
	{
		bool bIsCandidate = bFindFirstNode
			? !InputNodes.Contains(Node)		   // First: No incoming connections
			: !OutgoingConnections.Contains(Node); // Last: No outgoing connections

		if (bIsCandidate)
			Candidates.Add(Node);
	}

	// If there's only one candidate, return it immediately
	if (Candidates.Num() == 1)
		return Candidates[0];

	// Otherwise, traverse to find the deepest node
	return FindFarthestNode(Candidates, OutgoingConnections, bFindFirstNode);
}

// Traverses and finds the deepest node from given candidates
UEdGraphNode* UVimGraphEditorSubsystem::FindFarthestNode(
	const TArray<UEdGraphNode*>&					  Candidates,
	const TMap<UEdGraphNode*, TArray<UEdGraphNode*>>& OutgoingConnections,
	bool											  bFindFirstNode)
{
	UEdGraphNode* EdgeNode = nullptr;
	int			  MaxDepth = -1;

	for (UEdGraphNode* StartNode : Candidates)
	{
		TSet<UEdGraphNode*> Visited;
		int					Depth = 0;
		UEdGraphNode*		CurrentNode = StartNode;

		while (OutgoingConnections.Contains(CurrentNode) && OutgoingConnections[CurrentNode].Num() > 0)
		{
			Depth++;
			Visited.Add(CurrentNode);
			CurrentNode = OutgoingConnections[CurrentNode][0]; // Follow the first link
		}

		if (Depth > MaxDepth)
		{
			MaxDepth = Depth;
			EdgeNode = CurrentNode;
		}
	}

	return EdgeNode;
}

bool UVimGraphEditorSubsystem::IsValidZoom(const FString InZoomLevelStr)
{
	TArray<FString> Parsed;
	InZoomLevelStr.ParseIntoArrayWS(Parsed); // Splits by whitespace

	if (Parsed.Num() > 1) // Ensure we have at least two parts: "Zoom" and the number
	{
		float ZoomLevel = FCString::Atof(*Parsed[1]); // Convert the second part to float
		if (ZoomLevel < -5.0f)
			return false;

		Logger.Print(FString::Printf(TEXT("Zoom Amount: %f"), ZoomLevel), ELogVerbosity::Warning, true);
	}
	else
		return false;

	Logger.Print(FString::Printf(TEXT("Zoom Amount: %s"), *InZoomLevelStr), ELogVerbosity::Log, true);
	return true;
}

/** Deprecated but potentially some snippet here are interesting? */
void UVimGraphEditorSubsystem::MoveConnectedNodesToRight(UEdGraphNode* StartNode, float OffsetX)
{
	if (!StartNode)
		return;

	TSet<UEdGraphNode*>	  VisitedNodes;
	TQueue<UEdGraphNode*> NodeQueue;

	NodeQueue.Enqueue(StartNode);
	VisitedNodes.Add(StartNode);

	while (!NodeQueue.IsEmpty())
	{
		UEdGraphNode* CurrentNode;
		NodeQueue.Dequeue(CurrentNode);

		CurrentNode->NodePosX += OffsetX; // Move current node

		// Enqueue connected nodes
		CurrentNode->ForEachNodeDirectlyConnectedToOutputs([&](UEdGraphNode* ConnectedNode) {
			if (!VisitedNodes.Contains(ConnectedNode))
			{
				VisitedNodes.Add(ConnectedNode);
				NodeQueue.Enqueue(ConnectedNode);
			}
		});
	}
}

// TODO: We want to also collect nodes that are linked to pins other
// that just the main exec. For example in a for loop macro, we will want to
// shift the array it is connected to too.
// We will basically need to check if a certain pin has multiple connections(?)
// and collect them all(?)
void UVimGraphEditorSubsystem::CollectDownstreamNodes(
	UEdGraphNode*		 StartNode,
	TSet<UEdGraphNode*>& OutNodes,
	bool				 bIsAppending)
{
	if (!StartNode)
	{
		return;
	}

	// A simple BFS or DFS approach
	TQueue<UEdGraphNode*> Queue;
	Queue.Enqueue(StartNode);

	while (!Queue.IsEmpty())
	{
		UEdGraphNode* Current = nullptr;
		Queue.Dequeue(Current);

		if (!Current || OutNodes.Contains(Current))
		{
			continue;
		}

		// Mark visited
		OutNodes.Add(Current);

		// For "append", we look at output pins; for "insert",
		// we might also want to move everything downstream,
		// including the "current" node itself. So logic can differ.
		for (UEdGraphPin* Pin : Current->Pins)
		{
			if (!Pin)
				continue;

			// If appending, we want to follow outgoing edges
			// If inserting, we might do the same, except also
			// treat the node itself as "to be moved".
			if (bIsAppending && Pin->Direction == EGPD_Output)
			{
				for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
				{
					if (LinkedPin && LinkedPin->GetOwningNode())
					{
						Queue.Enqueue(LinkedPin->GetOwningNode());
					}
				}
			}
			else if (!bIsAppending && Pin->Direction == EGPD_Output)
			{
				// For insert, we also gather downstream:
				for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
				{
					if (LinkedPin && LinkedPin->GetOwningNode())
					{
						Queue.Enqueue(LinkedPin->GetOwningNode());
					}
				}
			}
		}
	}
}

void UVimGraphEditorSubsystem::ShiftNodesForSpace(
	const TSharedPtr<SGraphPanel>& GraphPanel,
	const TSet<UEdGraphNode*>&	   NodesToMove,
	float						   ShiftAmountX)
{
	ShiftedNodesOriginalPositions.Empty(); // Clear from previous usage

	// We'll collect them into an SNodePanel::SNode::FNodeSet
	// which MoveTo() uses for collision checks, etc.
	SNodePanel::SNode::FNodeSet NodeFilter;

	for (UEdGraphNode* GraphNode : NodesToMove)
	{
		if (!GraphNode)
			continue;

		// Get the SGraphNode so we can call MoveTo
		const TSharedPtr<SGraphNode> SNode =
			GraphPanel->GetNodeWidgetFromGuid(GraphNode->NodeGuid);

		if (!SNode.IsValid())
			continue;

		// Cache its original position
		const FVector2D OriginalPos = SNode->GetPosition();
		ShiftedNodesOriginalPositions.Add(GraphNode, OriginalPos);

		// lol this is not how you use the filter :)
		// We add it to the filter so we can move them as a group
		// NodeFilter.Add(SNode);
	}

	// Now apply the shift
	for (const auto& Pair : ShiftedNodesOriginalPositions)
	{
		if (UEdGraphNode* GraphNode = Pair.Key)
		{
			const TSharedPtr<SGraphNode> SNode =
				GraphPanel->GetNodeWidgetFromGuid(GraphNode->NodeGuid);
			if (!SNode.IsValid())
				continue;

			// Current position
			const FVector2D CurPos = Pair.Value;
			// Shift it
			FVector2D NewPos(CurPos.X + ShiftAmountX, CurPos.Y);

			// SNodePanel::SNode interface
			SNode->MoveTo(NewPos, NodeFilter, /*bMarkDirty=*/true);
			// Logger.Print("Try move node!", ELogVerbosity::Verbose, true);
		}
	}

	bNodesWereShifted = !ShiftedNodesOriginalPositions.IsEmpty();
}

void UVimGraphEditorSubsystem::RevertShiftedNodes(
	const TSharedPtr<SGraphPanel>& GraphPanel)
{
	if (!bNodesWereShifted)
		return;

	SNodePanel::SNode::FNodeSet NodeFilter;

	// Move them back
	for (auto& Pair : ShiftedNodesOriginalPositions)
	{
		if (UEdGraphNode* GraphNode = Pair.Key)
		{
			const TSharedPtr<SGraphNode> SNode =
				GraphPanel->GetNodeWidgetFromGuid(GraphNode->NodeGuid);
			if (!SNode.IsValid())
				continue;

			// This is not how you use it!
			// NodeFilter.Add(SNode);
		}
	}

	for (auto& Pair : ShiftedNodesOriginalPositions)
	{
		if (UEdGraphNode* GraphNode = Pair.Key)
		{
			const TSharedPtr<SGraphNode> SNode =
				GraphPanel->GetNodeWidgetFromGuid(GraphNode->NodeGuid);
			if (!SNode.IsValid())
				continue;

			const FVector2D OriginalPos = Pair.Value;
			SNode->MoveTo(OriginalPos, NodeFilter, /*bMarkDirty=*/true);
		}
	}

	// Cleanup
	ShiftedNodesOriginalPositions.Empty();
	bNodesWereShifted = false;
}

void UVimGraphEditorSubsystem::ZoomGraph(
	FSlateApplication& SlateApp,
	const FKeyEvent&   InKeyEvent)
{
	Logger.Print("My Custom Zoom", ELogVerbosity::Verbose, true);

	const TSharedPtr<SGraphPanel> GraphPanel = TryGetActiveGraphPanel(SlateApp);
	if (!GraphPanel.IsValid())
		return;

	const TSharedRef<SGraphPanel> GraphPanelRef = GraphPanel.ToSharedRef();

	TArray<UEdGraphNode*> SelNodes = GraphPanel->GetSelectedGraphNodes();
	FVector2f			  CursorPos;
	if (SelNodes.IsEmpty())
		CursorPos = FUMSlateHelpers::GetWidgetCenterScreenSpacePosition(GraphPanelRef);
	else
	{
		TSharedPtr<SGraphNode> SelNode = GraphPanel->GetNodeWidgetFromGuid(SelNodes[0]->NodeGuid);
		if (SelNode.IsValid())
			CursorPos = FUMSlateHelpers::GetWidgetCenterScreenSpacePosition(SelNode.ToSharedRef());
		else
			CursorPos = FUMSlateHelpers::GetWidgetCenterScreenSpacePosition(GraphPanelRef);
	}

	// Decide the wheel delta based on the key pressed:
	// (+ for zoom in, - for zoom out).
	const float WheelDelta = InKeyEvent.GetKey() == EKeys::Equals ? +1.0f : -1.0f;

	FUMInputHelpers::SimulateScrollWidget(SlateApp, GraphPanelRef, CursorPos, WheelDelta, false /*Shift not down */, true /* Control Down */);
}

void UVimGraphEditorSubsystem::ZoomToFit(
	FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	const TSharedPtr<SGraphPanel> GraphPanel = TryGetActiveGraphPanel(SlateApp);
	if (!GraphPanel.IsValid())
		return;

	if (InKeyEvent.IsShiftDown())
	{
		GraphPanel->ZoomToFit(false /* All nodes */);
		return;
	}

	// NOTE:
	// The ZoomToFit doesn't retain zoom level, which is pretty annoying.
	// We're creating some custom logic to center nodes without losing zoom.

	UEdGraph* GraphObj = GraphPanel->GetGraphObj();
	if (!GraphObj)
		return;

	TSharedPtr<SGraphEditor> GraphEditor = SGraphEditor::FindGraphEditorForGraph(GraphObj);
	if (!GraphEditor.IsValid())
		return;

	// GraphEditor->OnSelectionChanged;

	// Gather the currently selected nodes
	const TArray<UEdGraphNode*> SelNodes = GraphPanel->GetSelectedGraphNodes();

	if (SelNodes.Num() == 0)
		return; // Nothing selected, so nothing to center

	float MinX = FLT_MAX;
	float MinY = FLT_MAX;
	float MaxX = -FLT_MAX;
	float MaxY = -FLT_MAX;

	FSlateRect NodeBounds;
	if (!GraphPanel->GetBoundsForSelectedNodes(NodeBounds))
		return;

	MinX = FMath::Min(MinX, NodeBounds.Left);
	MinY = FMath::Min(MinY, NodeBounds.Top);
	MaxX = FMath::Max(MaxX, NodeBounds.Right);
	MaxY = FMath::Max(MaxY, NodeBounds.Bottom);

	const float CenterX = (MinX + MaxX) * 0.5f;
	const float CenterY = (MinY + MaxY) * 0.5f;
	FVector2D	DesiredCenter(CenterX, CenterY);

	// Get the panel size in *local/pixel* space
	FVector2D PanelSize = GraphPanel->GetCachedGeometry().GetLocalSize();

	// Get current zoom info from your blueprint editor
	FVector2D OldViewLocation;
	float	  OldZoomAmount = 1.0f;
	GraphEditor->GetViewLocation(OldViewLocation, OldZoomAmount);

	// Convert half the panel size to graph space (divide by zoom)
	FVector2D HalfPanelInGraphSpace = (PanelSize * 0.5f) / OldZoomAmount;

	// Shift the bounding-box center back by half the panel so that "Center"
	// ends up in the middle of the screen.
	FVector2D DesiredCameraTopLeft = FVector2D(CenterX, CenterY) - HalfPanelInGraphSpace;

	// Move the camera (view)
	GraphEditor->SetViewLocation(DesiredCameraTopLeft, OldZoomAmount);
}

/** DEPRECATED -> Keeping this for reference (for some code snippets) */
void UVimGraphEditorSubsystem::DebugEditor()
{
	return;

	IAssetEditorInstance* Editor = FUMEditorHelpers::GetLastActiveEditor();
	if (!Editor)
		return;

	Logger.Print(FString::Printf(TEXT("Editor Name: %s"),
					 *Editor->GetEditorName().ToString()),
		ELogVerbosity::Verbose, true);

	if (!Editor->GetEditorName().IsEqual("BlueprintEditor"))
		return;

	// FAssetEditorToolkit* Toolkit =
	// 	static_cast<FAssetEditorToolkit*>(Editor);
	// if (!Toolkit)
	// 	return;

	// Attempt to cast Editor to IBlueprintEditor
	IBlueprintEditor* BlueprintEditorInterface =
		StaticCast<IBlueprintEditor*>(Editor);
	if (!BlueprintEditorInterface)
		return;

	// Cast to FBlueprintEditor for full access
	FBlueprintEditor* BlueprintEditor =
		StaticCast<FBlueprintEditor*>(BlueprintEditorInterface);

	if (!BlueprintEditor)
		return;

	// BlueprintEditor->SetViewLocation(FVector2D(0.0f, 0.0f), 1);
	FVector2D CurrViewLocation;
	float	  CurrZoomAmount;
	BlueprintEditor->GetViewLocation(CurrViewLocation, CurrZoomAmount);

	Logger.Print(FString::Printf(TEXT("View Location: (%f, %f), Zoom: %f"),
					 CurrViewLocation.X, CurrViewLocation.Y, CurrZoomAmount),
		ELogVerbosity::Log, true);

	UEdGraph* Graph = BlueprintEditor->GetFocusedGraph();

	TArray<TObjectPtr<UEdGraphNode>> Nodes = Graph->Nodes;
	if (Nodes.IsEmpty())
		return;

	TObjectPtr<UEdGraphNode> SelNode = Nodes[0];
	BlueprintEditor->AddToSelection(SelNode);
	// Calculate the position for the new node
	const float NodeSpacing = 300.0f; // Spacer
	FVector2D	NewNodePos(
		  SelNode->NodePosX + NodeSpacing, SelNode->NodePosY);

	BlueprintEditor->GetFocusedGraph();

	// TArray<UEdGraphPin*> Pins = SelNode->GetAllPins();
	// for (const auto& Pin : Pins)
	// {
	// 	Pin->MakeLinkTo();
	// }

	// Create a node spawner for the PrintString function
	UBlueprintNodeSpawner* PrintStringSpawner =
		UBlueprintNodeSpawner::Create(
			UK2Node_CallFunction::StaticClass(), GetTransientPackage());
	if (!PrintStringSpawner)
		return;

	// Get the Blueprint object from the editor
	// UBlueprint* Blueprint = BlueprintEditor->GetBlueprintObj();
	// if (!Blueprint || !Graph)
	// 	return;

	IBlueprintNodeBinder::FBindingSet Bindings; // Create an empty binding set

	// Set the function to PrintString
	UK2Node_CallFunction* PrintStringNode =
		Cast<UK2Node_CallFunction>(PrintStringSpawner->Invoke(
			Graph, Bindings, NewNodePos));

	if (!PrintStringNode)
		return;

	PrintStringNode->SetFromFunction(UKismetSystemLibrary::StaticClass()->FindFunctionByName(TEXT("PrintString")));

	// Finalize node placement
	// PrintStringNode->NodePosX = NewNodePos.X;
	// PrintStringNode->NodePosY = NewNodePos.Y;
	// PrintStringNode->AllocateDefaultPins();

	// FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BlueprintEditor->GetBlueprintObj());

	return;

	for (const auto& Node : Nodes)
	{
		// Logger.Print(FString::Printf(TEXT("Node Name: %s"),
		// 				 // *Node->GetFullName()),
		// 				 *Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString()),
		// 	ELogVerbosity::Log, true);

		// Node->SetEnabledState(ENodeEnabledState::Enabled);
		// Node->SetEnabledState(ENodeEnabledState::Disabled);
	}

	return;

	// Logger.Print(TEXT("Successfully cast to IBlueprintEditor"), ELogVerbosity::Verbose, true);

	Logger.Print(FString::Printf(TEXT("Number of Selected Nodes: %d"),
					 BlueprintEditorInterface->GetNumberOfSelectedNodes()),
		ELogVerbosity::Log, true);

	UEdGraphNode_Comment* CommentNode = NewObject<UEdGraphNode_Comment>(Graph);

	if (CommentNode)
	{
		// Set the comment text
		CommentNode->NodeComment = "Hi Babe <3";
		// Add the node to the graph
		Graph->AddNode(CommentNode, true, false);
	}
	else
	{
		Logger.Print(TEXT("Failed to cast to IBlueprintEditor"), ELogVerbosity::Warning, true);
	}
}

FBlueprintEditor* UVimGraphEditorSubsystem::GetBlueprintEditor(const UEdGraph* ActiveGraphObj)
{
	if (!ActiveGraphObj)
		return nullptr;

	// Get the Blueprint that owns the graph
	UBlueprint* Blueprint =
		FBlueprintEditorUtils::FindBlueprintForGraph(ActiveGraphObj);
	if (!Blueprint)
		return nullptr;

	UAssetEditorSubsystem* AssetEditorSubsystem =
		GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	if (!AssetEditorSubsystem)
		return nullptr;

	// Find the open editor for a specific asset
	IAssetEditorInstance* EditorInstance =
		AssetEditorSubsystem->FindEditorForAsset(Blueprint, /*bFocusIfOpen=*/false);

	return StaticCast<FBlueprintEditor*>(EditorInstance);
}

/** DEPRECATED: Keeping this for reference, but theres a better method ->
 *
 * TSharedPtr<SGraphEditor> GraphEditor =
 * SGraphEditor::FindGraphEditorForGraph(GraphObj);
 * if (!GraphEditor.IsValid())
 *		return;
 */
TSharedPtr<SGraphEditor> UVimGraphEditorSubsystem::GetGraphEditor(const TSharedRef<SWidget> InWidget)
{
	TSharedPtr<SWidget> FoundGraphEditor;
	if (FUMSlateHelpers::TraverseFindWidgetUpwards(
			InWidget, FoundGraphEditor, "SGraphEditor"))
	{
		TSharedPtr<SGraphEditor> AsGraphEditor =
			StaticCastSharedPtr<SGraphEditor>(FoundGraphEditor);
		return AsGraphEditor;
	}
	return nullptr;
}

void UVimGraphEditorSubsystem::HandleOnContextBindingChanged(
	EUMContextBinding NewContext, const TSharedRef<SWidget> NewWidget)
{
	UnhookFromActiveGraphPanel();

	if (NewContext == EUMContextBinding::GraphEditor
		&& NewWidget->GetTypeAsString().Equals("SGraphPanel"))
	{
		TSharedRef<SGraphPanel> GraphPanel = StaticCastSharedRef<SGraphPanel>(NewWidget);

		ActiveGraphPanel = GraphPanel;

		// Store the original Delegate
		OnSelectionChangedOriginDelegate =
			GraphPanel->SelectionManager.OnSelectionChanged;

		// NOTE: This can be fragile if someone else is wrapping around this
		// (i.e. overriding this delegate). Might need to re-think this.
		// Another method might be listening to mouse-click & keyboard events
		// like crazy. Not sure what is better. Will test this out for a bit
		// and see if it behaves ok.
		// Create a new one that calls both the old delegate and our custom code
		GraphPanel->SelectionManager.OnSelectionChanged =
			SGraphEditor::FOnSelectionChanged::CreateLambda(
				[this](const FGraphPanelSelectionSet& NewSelection) {
					// First, call the old one (if it’s still bound)
					OnSelectionChangedOriginDelegate.ExecuteIfBound(NewSelection);
					// Then do your stuff
					HandleOnSelectionChanged(NewSelection);
				});

		///////////////////////////////////////////////////////
		// Other listener:
		UEdGraph* GraphObj = GraphPanel->GetGraphObj();
		if (!GraphObj)
			return;

		OnGraphChangedHandler = GraphObj->AddOnGraphChangedHandler(
			FOnGraphChanged::FDelegate::CreateUObject(
				this, &UVimGraphEditorSubsystem::HandleOnGraphChanged));
	}
}

void UVimGraphEditorSubsystem::HandleOnGraphChanged(const FEdGraphEditAction& InAction)
{
	Logger.Print("On Graph Changed!", ELogVerbosity::Log, true);
}

void UVimGraphEditorSubsystem::HandleOnSelectionChanged(
	const FGraphPanelSelectionSet& GraphPanelSelectionSet)
{
	Logger.Print("On Selection Changed!", ELogVerbosity::Log, true);
}

void UVimGraphEditorSubsystem::UnhookFromActiveGraphPanel()
{
	// If we still have a valid GraphPanel
	if (TSharedPtr<SGraphPanel> GraphPanel = ActiveGraphPanel.Pin())
	{
		// Restore the original selection-changed delegate
		// (or unbind entirely if you prefer)
		GraphPanel->SelectionManager.OnSelectionChanged = OnSelectionChangedOriginDelegate;

		// Remove our OnGraphChanged handler if we had one
		if (UEdGraph* GraphObj = GraphPanel->GetGraphObj())
		{
			GraphObj->RemoveOnGraphChangedHandler(OnGraphChangedHandler);
		}
	}

	// Reset local references
	OnSelectionChangedOriginDelegate.Unbind();
	ActiveGraphPanel.Reset();
	OnGraphChangedHandler.Reset();
}

void UVimGraphEditorSubsystem::HandleVimNodeNavigation(
	FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	const TSharedPtr<SGraphPanel> GraphPanel = TryGetActiveGraphPanel(SlateApp);
	if (!GraphPanel.IsValid())
		return;

	TArray<UEdGraphNode*> SelNodes = GraphPanel->GetSelectedGraphNodes();
	if (SelNodes.IsEmpty() || !SelNodes[0])
		return;

	TArray<UEdGraphPin*> PinObjs = SelNodes[0]->GetAllPins();
	if (PinObjs.IsEmpty())
		return;

	TSharedPtr<SGraphNode> GraphNode =
		GraphPanel->GetNodeWidgetFromGuid(SelNodes[0]->NodeGuid);

	TArray<TSharedRef<SWidget>> Pins;
	GraphNode->GetPins(Pins);
	if (Pins.IsEmpty())
		return;
}

void UVimGraphEditorSubsystem::DebugNodeAndPinsTypes()
{
	FSlateApplication&			  SlateApp = FSlateApplication::Get();
	const TSharedPtr<SGraphPanel> GraphPanel = TryGetActiveGraphPanel(SlateApp);
	if (!GraphPanel.IsValid())
		return;

	UEdGraph* GraphObj = GraphPanel->GetGraphObj();
	if (!GraphObj)
		return;

	TArray<TObjectPtr<UEdGraphNode>> Nodes = GraphObj->Nodes;
	for (const auto& Node : Nodes)
	{
		if (!Node)
			continue;

		TSharedPtr<SGraphNode> GraphNode = GraphPanel->GetNodeWidgetFromGuid(Node->NodeGuid);
		if (!GraphNode.IsValid())
			return;

		// For Nodes: Chop 10 chars left (SGraphNode)
		// For Pins: Chop 9 chars left (SGraphPin)
		// Potentially I can only chop 9 and get both?
		// Or: just add them all to the set. It will still be O(1), just a bit
		// longer in terms of types.
		//
		GraphNode->GetType();
		const FString WidgetType = GraphNode->GetTypeAsString();
		const FString ClassType = GraphNode->GetWidgetClass().GetWidgetType().ToString();

		Logger.Print(FString::Printf(TEXT("Widget Type: %s | Class Type: %s"),
						 *WidgetType, *ClassType),
			ELogVerbosity::Log, true);
	}
}

void UVimGraphEditorSubsystem::AddNodeToPin(
	FSlateApplication& SlateApp, UEdGraphPin* InPin,
	UEdGraphNode* ParentNode, TSharedRef<SGraphPanel> GraphPanel,
	bool bIsAppendingNode)
{
	// --------------------------
	// 1) Collect nodes to shift
	// --------------------------
	TSet<UEdGraphNode*> NodesToShift;

	if (bIsAppendingNode)
	{
		// For appending; shift all downstream nodes (-> on the OutPin chain).
		// But we do NOT shift this node itself in that scenario.
		for (UEdGraphPin* LinkedPin : InPin->LinkedTo)
		{
			if (LinkedPin && LinkedPin->GetOwningNode())
			{
				CollectDownstreamNodes(LinkedPin->GetOwningNode(), NodesToShift, /*bIsAppending=*/true);
			}
		}
	}
	else
	{
		// For inserting, we shift the "current" node plus anything downstream.
		CollectDownstreamNodes(ParentNode, NodesToShift, /*bIsAppending=*/false);
	}

	// If there's something to move, shift them.
	// For example, +300 for shifting “to the right” or -300 if you want left.
	// You can customize.
	if (NodesToShift.Num() > 0)
	{
		Logger.Print("Nodes to Shift is above 0", ELogVerbosity::Verbose, true);
		ShiftNodesForSpace(GraphPanel, NodesToShift, /*ShiftAmountX=*/35.f);
	}
	else
		Logger.Print("Nodes to Shift is 0", ELogVerbosity::Error, true);

	const FVector2D OriginCurPos = SlateApp.GetCursorPos();

	// Switch to Insert mode to immediately type in the Node Menu
	FVimInputProcessor::Get()->SetVimMode(SlateApp, EVimMode::Insert);

	// Store the current number of nodes we have so we can compare it
	// after the menu window has closed to see if a new node was created.
	NodeCounter = GraphPanel->GetChildren()->Num();

	TSharedPtr<SGraphNode> ParentGraphNode =
		GraphPanel->GetNodeWidgetFromGuid(ParentNode->NodeGuid);
	if (!ParentGraphNode.IsValid())
		return;
	TSharedPtr<SGraphPin> GraphPin = ParentGraphNode->FindWidgetForPin(InPin);
	if (!GraphPin.IsValid())
		return;
	const TSharedRef<SGraphPin> PinRef = GraphPin.ToSharedRef();

	// NOTE: Only when appending we need an offset for cursor, because when
	// inserting a node, we firstly move all the nodes to the right, thus, the
	// offset is created organically just by moving the nodes.
	// Other than that, all we need is to get either the top left or top right
	// abs of the widget depending on if we're inserting or appending.
	FVector2f OffA(35.0f, 0);
	FVector2f CurOffset = bIsAppendingNode
		? (FUMSlateHelpers::GetWidgetTopRightScreenSpacePosition(PinRef, OffA))
		: (FUMSlateHelpers::GetWidgetTopLeftScreenSpacePosition(PinRef));

	FUMInputHelpers::DragAndReleaseWidgetAtPosition(PinRef, CurOffset);

	FTimerHandle TimerHandle;
	GEditor->GetTimerManager()->SetTimer(
		TimerHandle,
		[this, &SlateApp, OriginCurPos, InPin, bIsAppendingNode]() {
			SlateApp.SetCursorPos(OriginCurPos); // Restore cursor position

			const TSharedPtr<SWindow> MenuWin = // Get the Popup Menu Win
				SlateApp.GetActiveTopLevelWindow();

			if (!MenuWin.IsValid())
				return;

			// Listen to when this window is closing and evaluate the users
			// action; Was a new node added?
			MenuWin->GetOnWindowClosedEvent().AddLambda(
				[this, &SlateApp, InPin, bIsAppendingNode](const TSharedRef<SWindow>& Window) {
					FTimerHandle TimerHandle;
					GEditor->GetTimerManager()->SetTimer(
						TimerHandle,
						[this, &SlateApp,
							InPin, bIsAppendingNode]() {
							OnNodeCreationMenuClosed(
								SlateApp,
								InPin, bIsAppendingNode);
							Logger.Print("Menu Closed",
								ELogVerbosity::Log, true);
						},
						0.05f, false);
				});
		},
		0.05f, false);
}

void UVimGraphEditorSubsystem::AddNodeToPin(
	FSlateApplication& SlateApp, const TSharedRef<SGraphPin> InPin)
{
	Logger.Print("Add Node to Pin", ELogVerbosity::Log, true);
	UEdGraphPin* PinObj = InPin->GetPinObj();
	if (!PinObj)
		return;

	UEdGraphNode* ParentNode = PinObj->GetOwningNode();
	if (!ParentNode)
		return;

	const TSharedPtr<SGraphPanel> GraphPanel = TryGetActiveGraphPanel(SlateApp);
	if (!GraphPanel.IsValid())
		return;

	bool bIsAppendingNode = PinObj->Direction == EGPD_Output;

	AddNodeToPin(SlateApp, PinObj, ParentNode, GraphPanel.ToSharedRef(), bIsAppendingNode);
}

#undef LOCTEXT_NAMESPACE
