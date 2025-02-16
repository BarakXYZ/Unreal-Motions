#include "VimGraphEditorSubsystem.h"
#include "BlueprintEditor.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraphNode_Comment.h"
#include "Framework/Application/SlateApplication.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UMConfig.h"
#include "GraphEditorModule.h"
#include "UMFocusHelpers.h"
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
#include "VimNavigationEditorSubsystem.h"

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
	GraphSelectionTracker.VimGraphOwner = this;

	BindVimCommands();

	// Start listening when Context Binding is changed directly
	// I wonder about this, it's cute. But is it really needed?
	FCoreDelegates::OnPostEngineInit.AddLambda([this]() {
		if (UUMFocuserEditorSubsystem* Focuser =
				GEditor->GetEditorSubsystem<UUMFocuserEditorSubsystem>())
		{
			Focuser->OnBindingContextChanged.AddUObject(
				this, &UVimGraphEditorSubsystem::HandleOnContextBindingChanged);
		}
	});

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

	VimInputProcessor->OnVimModeChanged.AddUObject(
		this, &UVimGraphEditorSubsystem::OnVimModeChanged);

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

	// Some more thoughts in regards to moving between nodes:
	// I think it makes the most sense to move (pan around) inside the node graph
	// itself via HJKL. This gives us freedom to assign natural motions like
	// moving to next / previous node via 'b' & 'w' and also doesn't collide with
	// 'a' which is preserved for appending node. (HJKL is better than AWSD in
	// that case).
	// We can go to the above / below node via 'e' & 'q' (which feels natural
	// as users are used to this motion feel from moving inside the UE viewport)
	// And we can have 'Shift + e' & 'Shift + q' for potentiall moving between
	// entire rows of disconnected nodes.
	// We can start now by implementing 'b' & 'w' for previous & next node,
	// then do panning inside the panel via HJKL and see how things feel from
	// there.

	// H: Pan Left in Graph Panel
	VimInputProcessor->AddKeyBinding_KeyEvent(
		EUMContextBinding::GraphEditor,
		{ FInputChord(EModifierKey::Alt, EKeys::H) },
		WeakGraphSubsystem,
		&UVimGraphEditorSubsystem::HandleGraphPanelPanning);

	// J: Pan Down in Graph Panel
	VimInputProcessor->AddKeyBinding_KeyEvent(
		EUMContextBinding::GraphEditor,
		{ FInputChord(EModifierKey::Alt, EKeys::J) },
		WeakGraphSubsystem,
		&UVimGraphEditorSubsystem::HandleGraphPanelPanning);

	// K: Pan Up in Graph Panel
	VimInputProcessor->AddKeyBinding_KeyEvent(
		EUMContextBinding::GraphEditor,
		{ FInputChord(EModifierKey::Alt, EKeys::K) },
		WeakGraphSubsystem,
		&UVimGraphEditorSubsystem::HandleGraphPanelPanning);

	// L: Pan Right in Graph Panel
	VimInputProcessor->AddKeyBinding_KeyEvent(
		EUMContextBinding::GraphEditor,
		{ FInputChord(EModifierKey::Alt, EKeys::L) },
		WeakGraphSubsystem,
		&UVimGraphEditorSubsystem::HandleGraphPanelPanning);

	//				~ HJKL Navigate Pins & Nodes ~
	//
	// H: Go to Left Pin / Node:
	VimInputProcessor->AddKeyBinding_Sequence(
		EUMContextBinding::GraphEditor,
		// { FInputChord(EModifierKey::Shift, EKeys::H) },
		{ EKeys::H },
		WeakGraphSubsystem,
		&UVimGraphEditorSubsystem::HandleVimNodeNavigation);

	// J: Go Down to Next Pin:
	VimInputProcessor->AddKeyBinding_Sequence(
		EUMContextBinding::GraphEditor,
		// { FInputChord(EModifierKey::Shift, EKeys::J) },
		{ EKeys::J },
		WeakGraphSubsystem,
		&UVimGraphEditorSubsystem::HandleVimNodeNavigation);

	// K: Go Up to Previous Pin:
	VimInputProcessor->AddKeyBinding_Sequence(
		EUMContextBinding::GraphEditor,
		// { FInputChord(EModifierKey::Shift, EKeys::K) },
		{ EKeys::K },
		WeakGraphSubsystem,
		&UVimGraphEditorSubsystem::HandleVimNodeNavigation);

	// L: Go to Right Pin / Node:
	VimInputProcessor->AddKeyBinding_Sequence(
		EUMContextBinding::GraphEditor,
		// { FInputChord(EModifierKey::Shift, EKeys::L) },
		{ EKeys::L },
		WeakGraphSubsystem,
		&UVimGraphEditorSubsystem::HandleVimNodeNavigation);

	// 'b':
	// If currently in an output pin, go to same node's input pin. (1 move)
	// Else if currently in Input Pin, go to previous node's input pin. (2 moves)
	VimInputProcessor->AddKeyBinding_Sequence(
		EUMContextBinding::GraphEditor,
		{ EKeys::B },
		WeakGraphSubsystem,
		&UVimGraphEditorSubsystem::HandleVimNodeNavigation);

	// 'w':
	// Go to next node's input pin (no matter if currently in In|Out Pin)
	VimInputProcessor->AddKeyBinding_Sequence(
		EUMContextBinding::GraphEditor,
		{ EKeys::W },
		WeakGraphSubsystem,
		&UVimGraphEditorSubsystem::HandleVimNodeNavigation);

	// 'e':
	// If currently in an Input pin, go to same node's Output pin. (1 move)
	// Else if currently in an Output Pin; go to previous node's Output pin. (2 moves)
	VimInputProcessor->AddKeyBinding_Sequence(
		EUMContextBinding::GraphEditor,
		{ EKeys::E },
		WeakGraphSubsystem,
		&UVimGraphEditorSubsystem::HandleVimNodeNavigation);

	// 'ge':
	// Go to previous node's Output pin (no matter if currently in In|Out Pin)
	VimInputProcessor->AddKeyBinding_Sequence(
		EUMContextBinding::GraphEditor,
		{ EKeys::G, EKeys::E },
		WeakGraphSubsystem,
		&UVimGraphEditorSubsystem::HandleVimNodeNavigation);

	// gg: Go to first node in chain from pin.
	VimInputProcessor->AddKeyBinding_Sequence(
		EUMContextBinding::GraphEditor,
		{ EKeys::G, EKeys::G },
		WeakGraphSubsystem,
		&UVimGraphEditorSubsystem::HandleVimNodeNavigation);

	// Shift+G: Go to last node in chain from pin.
	VimInputProcessor->AddKeyBinding_Sequence(
		EUMContextBinding::GraphEditor,
		{ FInputChord(EModifierKey::Shift, EKeys::G) },
		WeakGraphSubsystem,
		&UVimGraphEditorSubsystem::HandleVimNodeNavigation);

	// gh: Go to first Pin in Node.
	VimInputProcessor->AddKeyBinding_Sequence(
		EUMContextBinding::GraphEditor,
		{ EKeys::G, EKeys::K },
		WeakGraphSubsystem,
		&UVimGraphEditorSubsystem::HandleVimNodeNavigation);

	// gj: Go to last Pin in Node.
	VimInputProcessor->AddKeyBinding_Sequence(
		EUMContextBinding::GraphEditor,
		{ EKeys::G, EKeys::J },
		WeakGraphSubsystem,
		&UVimGraphEditorSubsystem::HandleVimNodeNavigation);

	// Ctrl+u: Go 3 Pins Up.
	VimInputProcessor->AddKeyBinding_Sequence(
		EUMContextBinding::GraphEditor,
		{ FInputChord(EModifierKey::Control, EKeys::U) },
		WeakGraphSubsystem,
		&UVimGraphEditorSubsystem::HandleVimNodeNavigation);

	// Ctrl+d: Go 3 Pins Down.
	VimInputProcessor->AddKeyBinding_Sequence(
		EUMContextBinding::GraphEditor,
		{ FInputChord(EModifierKey::Control, EKeys::D) },
		WeakGraphSubsystem,
		&UVimGraphEditorSubsystem::HandleVimNodeNavigation);
	//
	//				~ HJKL Navigate Pins & Nodes ~

	/** Delete Node */
	VimInputProcessor->AddKeyBinding_KeyEvent(
		EUMContextBinding::GraphEditor,
		// { FInputChord(EModifierKey::Shift, EKeys::Delete) },
		{ EKeys::Delete },
		WeakGraphSubsystem,
		&UVimGraphEditorSubsystem::DeleteNode);

	VimInputProcessor->AddKeyBinding_KeyEvent(
		EUMContextBinding::GraphEditor,
		// { FInputChord(EModifierKey::Shift, EKeys::X) },
		{ EKeys::X },
		WeakGraphSubsystem,
		&UVimGraphEditorSubsystem::DeleteNode);

	/** 'Enter': Add a new node to the currently highlighted pin */
	VimInputProcessor->AddKeyBinding_KeyEvent(
		EUMContextBinding::GraphEditor,
		{ EKeys::Enter },
		WeakGraphSubsystem,
		&UVimGraphEditorSubsystem::AddNodeToHighlightedPin);

	/** Shift + 'Enter': Access the clickable within the pin field */
	VimInputProcessor->AddKeyBinding_KeyEvent(
		EUMContextBinding::GraphEditor,
		{ FInputChord(EModifierKey::Shift, EKeys::Enter) },
		WeakGraphSubsystem,
		&UVimGraphEditorSubsystem::ClickOnInteractableWithinPin);

	// TODO:
	// 1. gg & G to go to focus first or last node in a chain
	// 2. Finalize graph appending and insertion edge cases
	// 3. 'b' & 'w' for moving to previous & next node.
	// 4. Panel panning via HJKL
	// 5. Move 'up' & 'down' between nodes via 'q' & 'e' & Shift ...
	// 6. 'Enter' to pull from the currently highlighted node?
}

void UVimGraphEditorSubsystem::AddNode(
	FSlateApplication& SlateApp,
	const FKeyEvent&   InKeyEvent)
{
	const TSharedPtr<SGraphPanel> GraphPanel = FUMSlateHelpers::TryGetActiveGraphPanel(SlateApp);
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
	TArray<TSharedRef<SWidget>> AllPins;
	NodeWidget->GetPins(AllPins);

	// Get either the Input ([I]nsert) or Output ([A]ppend) pin.
	const EEdGraphPinDirection TargetPinType =
		bIsAppendingNode ? EGPD_Output : EGPD_Input;

	UEdGraphPin*				OriginPinObj = nullptr;
	TArray<TSharedRef<SWidget>> FoundPinWidgets;
	for (const auto& Pin : AllPins)
	{
		// Cast to the specific widget representation of this pin object
		// (as the array of pins is fetching them as SWidgets)
		const TSharedRef<SGraphPin> AsGraphPin =
			StaticCastSharedRef<SGraphPin>(Pin);

		// We can also access these conditionals from here:
		// AsGraphPin->IsPinVisibleAsAdvanced();
		// AsGraphPin->GetDirection();

		// Verify the direction of the pin matches our TargetPinType (In | Out)
		UEdGraphPin* EdGraphPin = AsGraphPin->GetPinObj();
		if (!EdGraphPin
			// || EdGraphPin->bHidden
			|| EdGraphPin->bAdvancedView // Check if this pin is visible
			|| EdGraphPin->Direction != TargetPinType)
			continue;

		FoundPinWidgets.Add(Pin);

		if (!OriginPinObj) // Store only the first pin found.
			OriginPinObj = EdGraphPin;
	}

	if (!OriginPinObj)
		return;
	else if (FoundPinWidgets.Num() > 1)
	{
		// Generate markers for the found pins (TODO: numbered widgets)
		if (UVimNavigationEditorSubsystem* NavSub =
				GEditor->GetEditorSubsystem<UVimNavigationEditorSubsystem>())
			NavSub->GenerateMarkersForWidgets(SlateApp, FoundPinWidgets, true);
	}
	else
	{
		// In case we have only 1 pin, we can directly just pull from it.
		AddNodeToPin(SlateApp, OriginPinObj, NodeObj, GraphPanel.ToSharedRef(), bIsAppendingNode);
	}
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

	const TSharedPtr<SGraphPanel> GraphPanel = FUMSlateHelpers::TryGetActiveGraphPanel(SlateApp);
	if (!GraphPanel.IsValid())
	{
		Logger.Print("Graph Panel is Invalid", ELogVerbosity::Error, true);
		return;
	}

	UEdGraph* GraphObj = GraphPanel->GetGraphObj();
	if (!GraphObj)
		return;
	// if (NodeCounter == GraphPanel->GetChildren()->Num())
	if (NodeCounter == GraphObj->Nodes.Num())
	{
		Logger.Print("No new Nodes were created...", ELogVerbosity::Log, true);
		// Revert if we had shifted
		RevertShiftedNodes(GraphPanel);
		HighlightPinForSelectedNode(SlateApp, GraphPanel.ToSharedRef());
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

	HighlightPinForSelectedNode(SlateApp, GraphPanel.ToSharedRef(), NewNodeRef);

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
	const TSharedPtr<SGraphPanel> GraphPanel = FUMSlateHelpers::TryGetActiveGraphPanel(SlateApp);
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
	const TSharedPtr<SGraphPanel> GraphPanel = FUMSlateHelpers::TryGetActiveGraphPanel(SlateApp);
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
	CurrentContext = NewContext;
	return;

	// DEPRECATED
	UnhookFromActiveGraphPanel(); // Deprecated?

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

// TODO: Work on diagonal motion support:
// Potentially an array of currently pressed keys, which will determine the
// offset.
void UVimGraphEditorSubsystem::HandleGraphPanelPanning(
	FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	// Update to the latest key and panning params
	const FKey NewKey = InKeyEvent.GetKey();
	if (PressedPanningKeys.Contains(NewKey))
		return; // No duplicates
	PressedPanningKeys.Add(NewKey);
	if (const FVector2D* PanelOffsetPtr = PanOffsetByMotion.Find(NewKey))
		CurrentPanelOffset += *PanelOffsetPtr;
	else
		return; // Invalid navigation key (won't ever really get here)

	if (DelegateHandle_OnKeyUpEvent.IsValid())
		return; // We're already bound; shouldn't bind multiple times.

	DelegateHandle_OnKeyUpEvent = // Store handle to unbind on key up.
		FVimInputProcessor::Get()->Delegate_OnKeyUpEvent.AddUObject(
			this, &UVimGraphEditorSubsystem::StopGraphPanelPanning);

	// Logger.Print("HandleGraphPanelPanning", ELogVerbosity::Log, true);

	GEditor->GetTimerManager()->SetTimer(
		TimerHandle_GraphPanning,
		[this, &SlateApp, InKeyEvent]() {
			const TSharedPtr<SGraphPanel> GraphPanel =
				FUMSlateHelpers::TryGetActiveGraphPanel(SlateApp);
			if (GraphPanel.IsValid())
			{
				SlateApp.SetAllUserFocus(GraphPanel, EFocusCause::Navigation);
				UEdGraph* GraphObj = GraphPanel->GetGraphObj();
				if (GraphObj)
				{
					const TSharedPtr<SGraphEditor> GraphEditor =
						SGraphEditor::FindGraphEditorForGraph(GraphObj);
					if (GraphEditor.IsValid())
					{
						FVector2D Location;
						float	  ZoomAmount;
						GraphEditor->GetViewLocation(Location, ZoomAmount);
						Location += CurrentPanelOffset; // Update location
						GraphEditor->SetViewLocation(Location, ZoomAmount);
						return; // Don't stop Panning
					}
				}
			}
			StopGraphPanelPanning(SlateApp, InKeyEvent);
		},
		0.001f, true /* Loop=true */);
}

void UVimGraphEditorSubsystem::StopGraphPanelPanning(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	int32 Pos = PressedPanningKeys.Find(InKeyEvent.GetKey());
	if (Pos != INDEX_NONE)
	{
		if (const FVector2D* PanelOffsetPtr = PanOffsetByMotion.Find(PressedPanningKeys[Pos]))
			CurrentPanelOffset -= *PanelOffsetPtr;
		PressedPanningKeys.RemoveAt(Pos);
	}
	if (!PressedPanningKeys.IsEmpty())
		return;

	if (DelegateHandle_OnKeyUpEvent.IsValid())
	{
		GEditor->GetTimerManager()->ClearTimer(TimerHandle_GraphPanning);

		FVimInputProcessor::Get()->Delegate_OnKeyUpEvent.Remove(
			DelegateHandle_OnKeyUpEvent);
		DelegateHandle_OnKeyUpEvent.Reset();
	}
}

void UVimGraphEditorSubsystem::HandleVimNodeNavigation(
	FSlateApplication& SlateApp, const TArray<FInputChord>& InSequence)
{
	// if no tracking available try get selected node and init pin highlight
	if (!GraphSelectionTracker.IsValid()
		|| !GraphSelectionTracker.IsTrackedNodeSelected())
	{
		TSharedPtr<SGraphPanel> GraphPanel = FUMSlateHelpers::TryGetActiveGraphPanel(SlateApp);
		if (!GraphPanel.IsValid())
			return;
		TArray<UEdGraphNode*> SelNodes = GraphPanel->GetSelectedGraphNodes();
		if (SelNodes.IsEmpty() || !SelNodes[0])
			return;
		TSharedPtr<SGraphNode> GraphNode = GraphPanel->GetNodeWidgetFromGuid(SelNodes[0]->NodeGuid);
		if (!GraphNode.IsValid())
			return;

		ProcessNodeClick(SlateApp, GraphNode.ToSharedRef());
		return;
	}

	TSharedPtr<SGraphPanel> GraphPanel = GraphSelectionTracker.GraphPanel.Pin();
	TSharedPtr<SGraphNode>	GraphNode = GraphSelectionTracker.GraphNode.Pin();
	UEdGraphNode*			TrackedNodeObj = GraphNode->GetNodeObj();
	if (!TrackedNodeObj)
		return;

	TSharedPtr<SGraphPin> GraphPin = GraphSelectionTracker.GraphPin.Pin();
	UEdGraphPin*		  PinObj = GraphPin->GetPinObj();
	if (!PinObj)
		return; // Invalid current Pin Object

	TArray<TSharedRef<SWidget>>	  Pins;
	TArray<TSharedRef<SGraphPin>> CurrentPinGroup;
	GraphNode->GetPins(Pins);

	// Remove (filter) non‐visible pins as they're not important or useful
	Pins.RemoveAll([](const TSharedRef<SWidget>& Pin) { return !Pin->GetVisibility().IsVisible(); });
	if (Pins.IsEmpty())
		return;

	// NOTE: Unlike Widget Pins array getter: ObjPins array comes unsorted!
	// Thus we're constructing the ObjPins array manually (sorted).
	TArray<UEdGraphPin*> ObjPins;
	for (const auto& Pin : Pins)
	{
		TSharedRef<SGraphPin> AsGraphPin = StaticCastSharedRef<SGraphPin>(Pin);
		if (GraphPin->GetDirection() == AsGraphPin->GetDirection())
			CurrentPinGroup.Add(AsGraphPin); // Collect associated group pins

		if (UEdGraphPin* ObjPin = AsGraphPin->GetPinObj())
			ObjPins.Add(ObjPin);
	}
	if (ObjPins.IsEmpty())
		return; // No useful obj pins found

	const TSharedRef<SGraphPin> GraphPinRef = GraphPin.ToSharedRef();
	int32						CurrPinIndex = Pins.Find(GraphPinRef);
	int32						GroupPinIndex = CurrentPinGroup.Find(GraphPinRef);
	if (CurrPinIndex == INDEX_NONE || GroupPinIndex == INDEX_NONE)
		return; // Pin not found in the current node's pins array

	// We should have at most 2 strokes:
	const int32			SeqNum = InSequence.Num();
	const FKey			Key1 = InSequence[0].Key;
	const FKey			Key2 = SeqNum > 1 ? InSequence[1].Key : EKeys::Section;
	const bool			bIsShiftDown = InSequence[SeqNum - 1].bShift;
	const bool			bIsCtrlDown = InSequence[SeqNum - 1].bCtrl;
	TSharedPtr<SWidget> TargetPin;

	// Lambda to handle the 'H' and 'L' cases:
	//
	// *For key 'H' we want:
	//   if curr pin is Input: Follow nearest link to a *New Node's ~ Output Pin*
	//   else is Output: Move inside the current node to the *Parallel Input Pin*
	//
	// *For key 'L' we want the mirror:
	//   if curr pin is Output: Follow nearest link to a *New Node's ~ Input Pin*
	//   else is Input: Move inside the current node to the *Parallel Output Pin*
	auto HandleLeftRightNavigation =
		[&](EEdGraphPinDirection TargetDir) -> bool {
		if (GraphPin->GetDirection() == TargetDir) // Move to a new Node & Pin
		{
			// Try find any linked pins if the current highlighted pin isn't
			// linked and fallback to the nearest linked one.
			if (PinObj->LinkedTo.IsEmpty() && !TryGetNearestLinkedPin(ObjPins, PinObj, CurrPinIndex, TargetDir))
				return false; // No links found in any of the other pins

			// Post-found linked pin to go to
			UEdGraphPin* NewPin = PinObj->LinkedTo[0];
			if (!NewPin)
				return false; // Invalid Linked Pin Object
			UEdGraphNode* NewOwningNode = NewPin->GetOwningNode();
			if (!NewOwningNode)
				return false; // Invalid New Owning Node Object
			TSharedPtr<SGraphNode> NewGraphNode =
				GraphPanel->GetNodeWidgetFromGuid(NewOwningNode->NodeGuid);
			if (!NewGraphNode.IsValid())
				return false; // Invalid New Owning Node Widget
			TSharedPtr<SGraphPin> NewGraphPin = NewGraphNode->FindWidgetForPin(NewPin);
			if (!NewGraphPin.IsValid())
				return false; // Invalid New Pin Widget

			// Update tracking params
			GraphSelectionTracker.GraphNode = NewGraphNode;
			GraphSelectionTracker.GraphPin = NewGraphPin;

			TSharedRef<SGraphPanel> GraphPanelRef = GraphPanel.ToSharedRef();
			// Select the new node we're moving to // Add to selection if visual?
			GraphSelectionTracker.HandleNodeSelection(NewOwningNode, TrackedNodeObj, GraphPanelRef);

			AdjustViewIfNodeOutOfBounds(GraphPanelRef, NewGraphNode.ToSharedRef());

			// Highlight the New Pin we're moving into:
			FUMInputHelpers::SimulateMouseMoveToPosition(SlateApp,
				FVector2D(FUMSlateHelpers::GetWidgetCenterScreenSpacePosition(NewGraphPin.ToSharedRef())));

			return true;
		}
		else // Move inside the same node; try grab parallel indexed pin:
		{
			TSharedPtr<SGraphPin> FoundPin;
			if (!TryGetParallelPin(Pins, CurrPinIndex, TargetDir, FoundPin))
				return false;

			// Highlight the New Pin we're moving into:
			FUMInputHelpers::SimulateMouseMoveToPosition(SlateApp,
				FVector2D(FUMSlateHelpers::GetWidgetCenterScreenSpacePosition(FoundPin.ToSharedRef())));

			// Update tracking params
			GraphSelectionTracker.GraphPin = FoundPin;
			return true;
		}
	};

	auto HandleUpDownNavigation =
		[&](int32 Direction) -> bool {
		if (!CurrentPinGroup.IsValidIndex(GroupPinIndex + Direction))
			return false; // Can't move: we're at the edge of the pin group.

		// Go the above or below Pin:
		TargetPin = CurrentPinGroup[GroupPinIndex + Direction];

		FUMInputHelpers::SimulateMouseMoveToPosition(SlateApp,
			FVector2D(FUMSlateHelpers::GetWidgetCenterScreenSpacePosition(TargetPin.ToSharedRef())));
		GraphSelectionTracker.GraphPin = StaticCastSharedPtr<SGraphPin>(TargetPin);
		return true;
	};

	TArray<FInputChord> DummyKeySequence;

	// Handle Left & Right Pin / Node Navigation:
	if (Key1 == FKey(EKeys::H))
		HandleLeftRightNavigation(EGPD_Input);
	else if (Key1 == FKey(EKeys::L))
		HandleLeftRightNavigation(EGPD_Output);

	// Handle Up & Down Pin Navigation:
	else if (Key1 == EKeys::J || Key1 == EKeys::K)
	{
		HandleUpDownNavigation(Key1 == EKeys::J ? 1 : -1);
	}
	else if (Key2 == EKeys::J || Key2 == EKeys::K)
	{
		DummyKeySequence.Add(EKeys::G);
		DummyKeySequence.Add(Key2);

		if (HandleUpDownNavigation(Key2 == EKeys::J ? 1 : -1))
			HandleVimNodeNavigation(SlateApp, DummyKeySequence);
		return;
	}
	// Handle Goto First ('gg') & Last Pin (Shift+G) / Node Navigation:
	else if ((Key1 == EKeys::G && bIsShiftDown) || Key2 == EKeys::G)
	{
		if (bIsShiftDown) // Simulate Shift+G
			DummyKeySequence.Add(FInputChord(EModifierKey::Shift, EKeys::G));
		else // Simulate gg
		{
			DummyKeySequence.Add(EKeys::G);
			DummyKeySequence.Add(EKeys::G);
		}
		EEdGraphPinDirection TargetDir = bIsShiftDown ? EGPD_Output : EGPD_Input;
		// I thought this method has issues but it seems to work great!
		if (HandleLeftRightNavigation(TargetDir))
			HandleVimNodeNavigation(SlateApp, DummyKeySequence);
		return;
	}
	else if (Key1 == EKeys::B || Key1 == EKeys::W)
	{
		DummyKeySequence.Add(Key1 == EKeys::B ? EKeys::H : EKeys::L);
		HandleVimNodeNavigation(SlateApp, DummyKeySequence);
		if (GraphPin->GetDirection() == EGPD_Input) // 2 moves if base is In
			HandleVimNodeNavigation(SlateApp, DummyKeySequence);
	}
	else if (Key1 == EKeys::E || Key2 == EKeys::E)
	{
		DummyKeySequence.Add(Key1 == EKeys::E ? EKeys::L : EKeys::H);
		HandleVimNodeNavigation(SlateApp, DummyKeySequence);
		if (GraphPin->GetDirection() == EGPD_Output) // 2 moves if base is Out
			HandleVimNodeNavigation(SlateApp, DummyKeySequence);
	}
	else if (bIsCtrlDown) // Ctrl+D & Ctrl+U
	{
		const int32 Direction = Key1 == EKeys::D ? 1 : -1;
		int32		Reps{ 3 };
		while (Reps > 0 && HandleUpDownNavigation(Direction))
		{
			GroupPinIndex += Direction;
			--Reps;
		}
	}
}

void UVimGraphEditorSubsystem::AdjustViewIfNodeOutOfBounds(
	const TSharedRef<SGraphPanel> InGraphPanel,
	const TSharedRef<SGraphNode>  InGraphNode,
	float						  SafeMarginPercent)
{
	UEdGraph* GraphObj = InGraphPanel->GetGraphObj();
	if (!GraphObj)
		return;

	TSharedPtr<SGraphEditor> GraphEditor = SGraphEditor::FindGraphEditorForGraph(GraphObj);
	if (!GraphEditor.IsValid())
		return;

	// Grab the panel size (in panel / widget space)
	const FVector2D PanelSize = InGraphPanel->GetCachedGeometry().GetLocalSize();
	if (PanelSize.X <= 0.f || PanelSize.Y <= 0.f)
		return; // Panel not fully initialized

	// Zoom + offset in graph space
	const float		ZoomAmount = InGraphPanel->GetZoomAmount();
	const FVector2D ViewOffset = InGraphPanel->GetViewOffset();

	// Convert that top-left to panel space:
	const FVector2D NodeTopLeftInPanel =
		(InGraphNode->GetPosition() - ViewOffset) * ZoomAmount;

	const FVector2D NodeSizeInPanel = InGraphNode->GetDesiredSize();
	const FVector2D NodeBottomRightInPanel = NodeTopLeftInPanel + NodeSizeInPanel;

	// Compute the margin in pixels based on the panel size
	const float MarginX = PanelSize.X * SafeMarginPercent;
	const float MarginY = PanelSize.Y * SafeMarginPercent;

	// Compute safe region in panel space
	const FVector2D SafeMin(MarginX, MarginY);
	const FVector2D SafeMax(PanelSize.X - MarginX, PanelSize.Y - MarginY);

	// Check how far the node is out-of-bounds
	FVector2D Delta(0.f, 0.f);

	// Check left and right
	if (NodeTopLeftInPanel.X < SafeMin.X)
	{
		Delta.X = NodeTopLeftInPanel.X - SafeMin.X;
	}
	else if (NodeBottomRightInPanel.X > SafeMax.X)
	{
		Delta.X = NodeBottomRightInPanel.X - SafeMax.X;
	}

	// Check top and bottom
	if (NodeTopLeftInPanel.Y < SafeMin.Y)
	{
		Delta.Y = NodeTopLeftInPanel.Y - SafeMin.Y;
	}
	else if (NodeBottomRightInPanel.Y > SafeMax.Y)
	{
		Delta.Y = NodeBottomRightInPanel.Y - SafeMax.Y;
	}

	// Adjust view if out-of-bounds:
	if (!Delta.IsNearlyZero())
	{
		// Convert panel-space “Delta” into graph-space
		const FVector2D GraphSpaceDelta = Delta / ZoomAmount;

		// Shift the view offset so the node is back inside the safe zone
		GraphEditor->SetViewLocation(ViewOffset + GraphSpaceDelta, ZoomAmount);
	}
}

// DEPRECATED?
UEdGraphPin* UVimGraphEditorSubsystem::GetFirstOrLastLinkedPinFromPin(const TSharedRef<SGraphPanel> GraphPanel, UEdGraphPin* InPin, EEdGraphPinDirection TargetDir)
{
	UEdGraphNode* OwningNode = InPin->GetOwningNode();
	if (!OwningNode)
		return InPin;

	TSharedPtr<SGraphNode> OwningNodeWidget = GraphPanel->GetNodeWidgetFromGuid(OwningNode->NodeGuid);
	if (!OwningNodeWidget.IsValid())
		return InPin;

	TArray<TSharedRef<SWidget>> WidgetPins;
	OwningNodeWidget->GetPins(WidgetPins);

	// Remove (filter) non‐visible pins as they're not important or useful
	WidgetPins.RemoveAll([](const TSharedRef<SWidget>& Pin) { return !Pin->GetVisibility().IsVisible(); });
	if (WidgetPins.IsEmpty())
		return InPin;

	// NOTE: Unlike Widget Pins array getter: ObjPins array comes unsorted!
	// Thus we're constructing the ObjPins array manually (sorted).
	TArray<UEdGraphPin*> ObjPins;
	for (const auto& Pin : WidgetPins)
	{
		TSharedRef<SGraphPin> AsGraphPin = StaticCastSharedRef<SGraphPin>(Pin);
		if (UEdGraphPin* ObjPin = AsGraphPin->GetPinObj())
			ObjPins.Add(ObjPin);
	}
	if (ObjPins.IsEmpty())
		return InPin; // No useful obj pins found

	int32 CurrPinIndex = ObjPins.Find(InPin);
	if (CurrPinIndex == INDEX_NONE)
		return InPin; // Pin not found in the current node's pins array

	if (InPin->Direction == TargetDir) // Find linked pin in a new node
	{
		if (InPin->LinkedTo.IsEmpty())
		{
			if (!TryGetNearestLinkedPin(ObjPins, InPin, CurrPinIndex, TargetDir)
				|| !InPin->LinkedTo[0])
				return InPin;
		}
		InPin = InPin->LinkedTo[0];
	}
	// Find parallel pin inside the same node
	else
	{
		TSharedPtr<SGraphPin> FoundPinWidget;
		if (!TryGetParallelPin(WidgetPins, CurrPinIndex, TargetDir, FoundPinWidget))
			return InPin;

		UEdGraphPin* CheckPin = FoundPinWidget->GetPinObj();
		if (!CheckPin)
			return InPin;
		InPin = CheckPin;
	}

	return GetFirstOrLastLinkedPinFromPin(GraphPanel, InPin, TargetDir);
}

bool UVimGraphEditorSubsystem::TryGetParallelPin(
	const TArray<TSharedRef<SWidget>> InWidgetPins,
	int32 BasePinIndex, EEdGraphPinDirection TargetDir,
	TSharedPtr<SGraphPin>& OutPinWidget, bool bShouldMatchExactIndex)
{
	TArray<TSharedRef<SGraphPin>> ParallelPins;
	for (const auto& Widget : InWidgetPins) // Collect all Parallel Pins
	{
		TSharedRef<SGraphPin> PinWidget = StaticCastSharedRef<SGraphPin>(Widget);
		if (PinWidget->GetDirection() == TargetDir)
			ParallelPins.Add(PinWidget);
	}
	if (ParallelPins.IsEmpty())
		return false; // No pins to navigate to.

	// Because the node's pins are ordered (all inputs first,
	// then outputs), we can compute the “relative index” as follows:
	// *For H (TargetDir == EGPD_Input):
	//     Current pin is output, so subtract total input count.
	// *For L (TargetDir == EGPD_Output):
	//     Current pin is input so its index is already relative.
	int32 RelativeIndex = 0;
	RelativeIndex = (TargetDir == EGPD_Input)
		? BasePinIndex - ParallelPins.Num() // Output to Input
		: BasePinIndex;						// Input to Output

	// if there isn't an exact parallel pin for the currently indexed pin:
	// Fallback to the last parallel pin.
	// This is needed only if the current index is above 0.
	// For 0, it will always return valid and grab the first one because the
	// array is guaranteed to be not empty.
	if (!ParallelPins.IsValidIndex(RelativeIndex))
		RelativeIndex = ParallelPins.Num() - 1;

	OutPinWidget = ParallelPins[RelativeIndex];
	return true;
}

bool UVimGraphEditorSubsystem::TryGetNearestLinkedPin(
	const TArray<UEdGraphPin*>& InSortedObjPins, UEdGraphPin*& OutPin,
	int32 TrackedPinIndex, EEdGraphPinDirection FollowDir)
{
	int32 SearchUp{ TrackedPinIndex - 1 }, // Try find upwards
		SearchDown{ TrackedPinIndex + 1 }, // Try find downwards
		TimeOut{ (InSortedObjPins.Num() / 2) + 1 };
	auto Pred = [&](int32 Index) {
		return (InSortedObjPins.IsValidIndex(Index)
			&& InSortedObjPins[Index]
			&& !InSortedObjPins[Index]->bAdvancedView
			&& InSortedObjPins[Index]->Direction == FollowDir
			&& !InSortedObjPins[Index]->LinkedTo.IsEmpty());
	};
	while (TimeOut > 0)
	{
		if (Pred(SearchUp))
		{
			OutPin = InSortedObjPins[SearchUp];
			return true;
		}
		else if (Pred(SearchDown))
		{
			OutPin = InSortedObjPins[SearchDown];
			return true;
		}
		--SearchUp;
		++SearchDown;
		--TimeOut;
	}
	return false; // No links found in any of the other pins
}

void UVimGraphEditorSubsystem::DebugNodeAndPinsTypes()
{
	FSlateApplication&			  SlateApp = FSlateApplication::Get();
	const TSharedPtr<SGraphPanel> GraphPanel = FUMSlateHelpers::TryGetActiveGraphPanel(SlateApp);
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
	// Log_AddNodeToPin(bIsAppendingNode);

	// Reset both tracking parameters for a clean start (these are also cleaned
	// in other places, but for safety, we want to clean here too)
	ShiftedNodesOriginalPositions.Empty();
	bNodesWereShifted = false;

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

	// Shift nodes if there are any
	if (NodesToShift.Num() > 0)
	{
		Logger.Print("Nodes to Shift is above 0", ELogVerbosity::Log, true);
		ShiftNodesForSpace(GraphPanel, NodesToShift, /*ShiftAmountX=*/35.f);
	}
	else
		Logger.Print("Nodes to Shift is 0", ELogVerbosity::Log, true);

	const FVector2D OriginCurPos = SlateApp.GetCursorPos();

	// Switch to Insert mode to immediately type in the Node Menu
	FVimInputProcessor::Get()->SetVimMode(SlateApp, EVimMode::Insert);

	// Store the current number of nodes we have so we can compare it
	// after the menu window has closed to see if a new node was created.
	UEdGraph* GraphObj = GraphPanel->GetGraphObj();
	if (!GraphObj)
		return;
	NodeCounter = GraphObj->Nodes.Num();
	// NodeCounter = GraphPanel->GetChildren()->Num();

	TSharedPtr<SGraphNode> ParentGraphNode =
		GraphPanel->GetNodeWidgetFromGuid(ParentNode->NodeGuid);
	if (!ParentGraphNode.IsValid())
		return;
	TSharedPtr<SGraphPin> GraphPin = ParentGraphNode->FindWidgetForPin(InPin);
	if (!GraphPin.IsValid())
		return;
	const TSharedRef<SGraphPin> PinRef = GraphPin.ToSharedRef();

	// We want to grab the pin image (that lives inside the pin itself) for the
	// optimal pulling point (for the cursor):
	TSharedPtr<SWidget> FoundPinImage;
	const FString		ImageType = "SLayeredImage";
	if (!FUMSlateHelpers::TraverseFindWidget(PinRef, FoundPinImage, ImageType))
		return;

	// NOTE: Only when appending we need an offset for cursor, because when
	// inserting a node, we firstly move all the nodes to the right, thus, the
	// offset is created organically just by moving the nodes.
	// Other than that, all we need is to get either the top left or top right
	// abs of the widget depending on if we're inserting or appending.
	FVector2f OffA(35.0f, 0);
	FVector2f CurOffset = bIsAppendingNode
		? (FUMSlateHelpers::GetWidgetTopRightScreenSpacePosition(PinRef, OffA))
		: (FUMSlateHelpers::GetWidgetTopLeftScreenSpacePosition(PinRef));

	FUMInputHelpers::DragAndReleaseWidgetAtPosition(FoundPinImage.ToSharedRef(), CurOffset);

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
	// Logger.Print("Add Node to Pin", ELogVerbosity::Log, true);

	UEdGraphPin* PinObj = InPin->GetPinObj();
	if (!PinObj)
		return;

	UEdGraphNode* ParentNode = PinObj->GetOwningNode();
	if (!ParentNode)
		return;

	const TSharedPtr<SGraphPanel> GraphPanel = FUMSlateHelpers::TryGetActiveGraphPanel(SlateApp);
	if (!GraphPanel.IsValid())
		return;

	bool bIsAppendingNode = PinObj->Direction == EGPD_Output;

	AddNodeToPin(SlateApp, PinObj, ParentNode, GraphPanel.ToSharedRef(), bIsAppendingNode);
}

void UVimGraphEditorSubsystem::Log_AddNodeToPin(bool bIsAppendingNode)
{
	Logger.Print(FString::Printf(TEXT("bIsAppendingNode: %s"),
					 bIsAppendingNode ? TEXT("TRUE") : TEXT("FALSE")),
		ELogVerbosity::Log, true);

	Logger.Print(FString::Printf(TEXT("Node Positions Stored: %d"),
					 ShiftedNodesOriginalPositions.Num()),
		ELogVerbosity::Log, true);
}

// TODO: Add Shift+X to delete previous node and keep connection.
void UVimGraphEditorSubsystem::DeleteNode(
	FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	const TSharedPtr<SGraphPanel> GraphPanel = FUMSlateHelpers::TryGetActiveGraphPanel(SlateApp);
	if (!GraphPanel.IsValid())
		return;

	TArray<UEdGraphNode*> SelNodes = GraphPanel->GetSelectedGraphNodes();
	if (SelNodes.IsEmpty())
		return;

	TArray<UEdGraphPin*> Pins = SelNodes[0]->GetAllPins();
	UEdGraphNode*		 FallbackNode = nullptr;
	for (const auto& Pin : Pins)
	{
		if (Pin->LinkedTo.IsEmpty())
			continue;

		UEdGraphNode* OwningNode = Pin->LinkedTo[0]->GetOwningNode();
		if (!OwningNode)
			continue;

		FallbackNode = OwningNode;
		break;
	}

	SelNodes[0]->DestroyNode();
	if (FallbackNode)
		GraphPanel->SelectionManager.SelectSingleNode(FallbackNode);
}

void UVimGraphEditorSubsystem::ProcessNodeClick(FSlateApplication& SlateApp, const TSharedRef<SWidget> InWidget)
{
	// Logger.Print("Found SGraphNode", ELogVerbosity::Log, true);

	TSharedRef<SGraphNode> GraphNode = // We can safely cast to Base SGraphNode
		StaticCastSharedRef<SGraphNode>(InWidget);

	UEdGraphNode* AsNodeObj = GraphNode->GetNodeObj();
	if (!AsNodeObj)
		return;

	const TSharedPtr<SGraphPanel> GraphPanel = GraphNode->GetOwnerPanel();
	if (!GraphPanel.IsValid())
		return;
	const TSharedRef<SGraphPanel> GraphPanelRef = GraphPanel.ToSharedRef();

	// We wanna focus the Graph first to draw focus to the entire Minor Tab
	// (just selecting the Node won't be enough if coming from a diff minor tab)
	SlateApp.SetAllUserFocus(GraphPanel, EFocusCause::Navigation);

	GraphPanel->SelectionManager.SelectSingleNode(AsNodeObj);
	HighlightPinForSelectedNode(SlateApp, GraphPanelRef, GraphNode);

	AdjustViewIfNodeOutOfBounds(GraphPanelRef, GraphNode);
}

// TODO: Look out to see if we want to also simulate pure Enter key
void UVimGraphEditorSubsystem::AddNodeToHighlightedPin(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	// if no tracking available try get selected node and init pin highlight?
	// Not sure if truly needed...
	if (!GraphSelectionTracker.IsValid()
		|| !GraphSelectionTracker.IsTrackedNodeSelected())
		return;
	TSharedPtr<SGraphPin> GraphPin = GraphSelectionTracker.GraphPin.Pin();
	AddNodeToPin(SlateApp, GraphPin.ToSharedRef());
}

void UVimGraphEditorSubsystem::ClickOnInteractableWithinPin(
	FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	if (!GraphSelectionTracker.IsValid()
		|| !GraphSelectionTracker.IsTrackedNodeSelected())
		return;

	TSharedPtr<SGraphPin> GraphPin = GraphSelectionTracker.GraphPin.Pin();
	FChildren*			  Children = GraphPin->GetChildren();
	if (!Children || Children->Num() == 0)
		return;
	TSharedRef<SWidget> FirstChild = Children->GetChildAt(0);
	TSharedPtr<SWidget> FoundWidget;
	if (!FUMSlateHelpers::TraverseFindWidget(FirstChild, FoundWidget, FUMSlateHelpers::GetInteractableWidgetTypes()))
		return;

	FUMFocusHelpers::HandleWidgetExecution(SlateApp, FoundWidget.ToSharedRef());
}

void UVimGraphEditorSubsystem::HighlightPinForSelectedNode(
	FSlateApplication&			  SlateApp,
	const TSharedRef<SGraphPanel> GraphPanel,
	const TSharedRef<SGraphNode>  SelectedNode)
{
	// Check if there's a tracked node available and if it's the curr sel one:
	if (GraphSelectionTracker.IsValid()
		&& GraphSelectionTracker.IsTrackedNodeSelected())
	{
		// Highlight the previous pin it has tracked for continuity:
		TSharedPtr<SGraphPin> GraphPin = GraphSelectionTracker.GraphPin.Pin();
		FUMInputHelpers::SimulateMouseMoveToPosition(SlateApp,
			FVector2D(FUMSlateHelpers::GetWidgetCenterScreenSpacePosition(GraphPin.ToSharedRef())));
		return;
	}

	// Else: Highlight first Pin in currently selected node (Pin Input 0)
	TArray<TSharedRef<SWidget>> Pins;
	SelectedNode->GetPins(Pins);
	if (Pins.IsEmpty())
		return;
	TSharedRef<SGraphPin> GraphPin = StaticCastSharedRef<SGraphPin>(Pins[0]);

	// Track Parameters:
	GraphSelectionTracker = FGraphSelectionTracker(GraphPanel, SelectedNode, GraphPin);

	// Highlight Pin:
	FUMInputHelpers::SimulateMouseMoveToPosition(SlateApp,
		FVector2D(FUMSlateHelpers::GetWidgetCenterScreenSpacePosition(GraphPin)));
}

void UVimGraphEditorSubsystem::HighlightPinForSelectedNode(
	FSlateApplication&			  SlateApp,
	const TSharedRef<SGraphPanel> GraphPanel)
{
	GraphPanel->GetSelectedGraphNodes();
	TArray<UEdGraphNode*> SelNodes = GraphPanel->GetSelectedGraphNodes();
	if (SelNodes.IsEmpty() || !SelNodes[0])
		return;
	TSharedPtr<SGraphNode> GraphNode = GraphPanel->GetNodeWidgetFromGuid(SelNodes[0]->NodeGuid);
	if (!GraphNode.IsValid())
		return;

	HighlightPinForSelectedNode(SlateApp, GraphPanel, GraphNode.ToSharedRef());
}

void UVimGraphEditorSubsystem::OnVimModeChanged(const EVimMode NewVimMode)
{
	PreviousVimMode = CurrentVimMode;
	CurrentVimMode = NewVimMode;

	if (CurrentContext != EUMContextBinding::GraphEditor)
		return;

	switch (CurrentVimMode)
	{
		case EVimMode::Normal:
		{
			GraphSelectionTracker.VisitedNodesStack.Empty();

			if (GraphSelectionTracker.IsValid() && GraphSelectionTracker.IsTrackedNodeSelected())
			{
				TSharedPtr<SGraphPanel> GraphPanel = GraphSelectionTracker.GraphPanel.Pin();
				TSharedPtr<SGraphNode>	GraphNode = GraphSelectionTracker.GraphNode.Pin();
				UEdGraphNode*			NodeObj = GraphNode->GetNodeObj();
				if (NodeObj)
					GraphPanel->SelectionManager.SelectSingleNode(NodeObj);
			}
			break;
		}
		// case EVimMode::Insert:
		// {
		// 	break;
		// }
		case EVimMode::Visual:
		{
			GraphSelectionTracker.VisitedNodesStack.Empty();

			if (GraphSelectionTracker.IsValid() && GraphSelectionTracker.IsTrackedNodeSelected())
			{
				TSharedPtr<SGraphNode> GraphNode = GraphSelectionTracker.GraphNode.Pin();
				UEdGraphNode*		   NodeObj = GraphNode->GetNodeObj();
				if (NodeObj)
					GraphSelectionTracker.VisitedNodesStack.Add(NodeObj);
			}
			else // Init tracking
			{
				FSlateApplication&			  SlateApp = FSlateApplication::Get();
				const TSharedPtr<SGraphPanel> GraphPanel = FUMSlateHelpers::TryGetActiveGraphPanel(SlateApp);
				if (!GraphPanel.IsValid())
					return;

				TArray<UEdGraphNode*> SelNodes = GraphPanel->GetSelectedGraphNodes();
				if (SelNodes.IsEmpty() || !SelNodes[0])
					return;

				GraphSelectionTracker.VisitedNodesStack.Add(SelNodes[0]);
				HighlightPinForSelectedNode(SlateApp, GraphPanel.ToSharedRef());
			}
			break;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
//				~ FGraphSelectionTracker Implementation ~ //
//
UVimGraphEditorSubsystem* UVimGraphEditorSubsystem::FGraphSelectionTracker::VimGraphOwner = nullptr;

UVimGraphEditorSubsystem::FGraphSelectionTracker::FGraphSelectionTracker(
	TWeakPtr<SGraphPanel> InGraphPanel,
	TWeakPtr<SGraphNode>  InGraphNode,
	TWeakPtr<SGraphPin>	  InGraphPin)
{
	GraphPanel = InGraphPanel;
	GraphNode = InGraphNode;
	GraphPin = InGraphPin;
}

bool UVimGraphEditorSubsystem::FGraphSelectionTracker::IsValid()
{
	return GraphPanel.IsValid() && GraphNode.IsValid() && GraphPin.IsValid();
}

bool UVimGraphEditorSubsystem::FGraphSelectionTracker::IsTrackedNodeSelected()
{
	if (TSharedPtr<SGraphNode> Node = GraphNode.Pin())
	{
		if (TSharedPtr<SGraphPanel> Panel = GraphPanel.Pin())
		{
			if (!Panel->HasAnyUserFocusOrFocusedDescendants())
				return false; // Verify that this panel is actually active;
							  // This is needed for when switching tabs.

			TArray<UEdGraphNode*> SelNodes = Panel->GetSelectedGraphNodes();
			if (!SelNodes.IsEmpty())
			{
				UEdGraphNode* NodeObj = Node->GetNodeObj();
				if (NodeObj)
				{
					int32 NodeIndex = SelNodes.Find(NodeObj);
					if (NodeIndex != INDEX_NONE)
						return true;
				}
				// return SelNodes[0] && NodeObj && SelNodes[0] == NodeObj;
			}
		}
	}
	return false;
}

void UVimGraphEditorSubsystem::FGraphSelectionTracker::HandleNodeSelection(
	UEdGraphNode*				  NewNode,
	UEdGraphNode*				  OldNode,
	const TSharedRef<SGraphPanel> InGraphPanel)
{
	if (!NewNode || !OldNode)
		return; // Early-out if either node is invalid.

	// If Visual Mode was active before we entered the graph;
	// ensure the current node (OldNode) is on the stack as our starting point.
	if (VisitedNodesStack.IsEmpty())
		VisitedNodesStack.Add(OldNode);

	if (VimGraphOwner->CurrentVimMode == EVimMode::Visual)
	{
		// If we already have more than one node in the stack;
		// we can detect backtracking.
		if (VisitedNodesStack.Num() > 1)
		{
			// The node that is one below the top of the stack is our potential
			// "backtrack" target.
			UEdGraphNode* PotentialBacktrackNode = VisitedNodesStack[VisitedNodesStack.Num() - 2].Get();

			if (NewNode == PotentialBacktrackNode)
			{
				// --- BACKWARD MOVE ---
				// User is going back to a previously visited node.
				// Remove the old top node from the stack and deselect it.
				VisitedNodesStack.Pop();
				InGraphPanel->SelectionManager.SetNodeSelection(OldNode, /*bSelect=*/false);
			}
			else
			{
				// --- FORWARD MOVE ---
				// User is moving to a new node, so push it on the stack and select it.
				VisitedNodesStack.Add(NewNode);
				InGraphPanel->SelectionManager.SetNodeSelection(NewNode, /*bSelect=*/true);
			}
		}
		else
		{
			// Stack is empty or has only one node -> definitely a forward move.
			VisitedNodesStack.Add(NewNode);
			InGraphPanel->SelectionManager.SetNodeSelection(NewNode, true);
		}
	}
	else
		// Normal mode (non-Visual):
		// simply select the new node, ignoring our stack.
		InGraphPanel->SelectionManager.SelectSingleNode(NewNode);
}

//
//				~ FGraphSelectionTracker Implementation ~ //
///////////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
