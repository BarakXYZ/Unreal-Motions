#include "VimGraphEditorSubsystem.h"
#include "BlueprintEditor.h"
#include "EdGraphNode_Comment.h"
#include "Framework/Application/SlateApplication.h"
#include "UMConfig.h"
#include "GraphEditorModule.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UMSlateHelpers.h"
#include "VimEditorSubsystem.h"
#include "VimInputProcessor.h"
#include "UMEditorHelpers.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "EdGraphSchema_K2_Actions.h"
#include "EdGraph/EdGraph.h"
#include "BlueprintNodeSpawner.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "Kismet/KismetSystemLibrary.h"
#include "BlueprintNodeBinder.h"
#include "SGraphPanel.h"
#include "UMSlateHelpers.h"
#include "UMInputHelpers.h"
#include "KismetPins/SGraphPinExec.h"

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

	// FBlueprintEditor* CurrentBPEditor =
	// 	static_cast<FBlueprintEditor*>(
	// 		FModuleManager::Get().GetModulePtr<FKismetEditorModule>("Kismet")->GetCurrentBlueprintEditor().Get());

	// if (CurrentBPEditor)
	// {
	// 	CurrentBPEditor->GetFocusedGraph();
	// 	CurrentBPEditor->GetToolkitName();
	// }

	// return nullptr;

	BindVimCommands();
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

	VimInputProcessor->AddKeyBinding_NoParam(
		EUMContextBinding::GraphEditor,
		{ EKeys::SpaceBar, EKeys::G, EKeys::D },
		WeakGraphSubsystem,
		&UVimGraphEditorSubsystem::DebugEditor);

	VimInputProcessor->AddKeyBinding_KeyEvent(
		EUMContextBinding::GraphEditor,
		{ EKeys::A },
		WeakGraphSubsystem,
		&UVimGraphEditorSubsystem::AddNode);

	VimInputProcessor->AddKeyBinding_KeyEvent(
		EUMContextBinding::GraphEditor,
		{ EKeys::I },
		WeakGraphSubsystem,
		&UVimGraphEditorSubsystem::AddNode);
}

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

void UVimGraphEditorSubsystem::AddNode(
	FSlateApplication& SlateApp,
	const FKeyEvent&   InKeyEvent)
{
	// Logger.Print("From Graph <3", ELogVerbosity::Log, true);

	const TSharedPtr<SGraphPanel> GraphPanel = TryGetActiveGraphPanel(SlateApp);
	if (!GraphPanel.IsValid())
		return;

	// In node graph, users cannot drag pins from a zoom level below -5.
	// it will only allow dragging nodes at this zoom level.
	// Thus we want to early return here & potentially notify the user.
	// NOTE: Using GetZoomText() and not GetZoomAmount() (which seems more
	// straight forward) because GetZoomAmount() seems to always return 0 *~*
	if (!IsValidZoom(GraphPanel->GetZoomText().ToString()))
		return;
	// Optionally we can ZoomToFit if not valid zoom, but idk; it's a diff UX
	// and will also require a delay, etc.
	// GraphPanel->ZoomToFit(true);

	const TArray<UEdGraphNode*>
		SelNodes = GraphPanel->GetSelectedGraphNodes();
	if (SelNodes.IsEmpty())
		return;

	const FKey InKey = InKeyEvent.GetKey();

	const UEdGraphNode* NodeObj = InKey == FKey(EKeys::I)
		? FindEdgeNodeInChain(SelNodes, true)	// first
		: FindEdgeNodeInChain(SelNodes, false); // last

	if (!NodeObj)
		return;

	const TSharedPtr<SGraphNode> NodeWidget =
		GraphPanel->GetNodeWidgetFromGuid(NodeObj->NodeGuid);

	if (!NodeWidget.IsValid())
		return;

	TArray<TSharedRef<SWidget>> AllPins;
	NodeWidget->GetPins(AllPins);

	if (AllPins.IsEmpty())
		return;

	for (const auto& Pin : AllPins)
	{
		if (!Pin->GetTypeAsString().Equals("SGraphPinExec"))
			continue;

		const EEdGraphPinDirection TargetPinType =
			InKey == FKey(EKeys::I) ? EGPD_Input : EGPD_Output;

		const TSharedRef<SGraphPinExec> AsGraphPin =
			StaticCastSharedRef<SGraphPinExec>(Pin);

		// Get the direction of the pin
		UEdGraphPin* EdGraphPin = AsGraphPin->GetPinObj();
		if (!EdGraphPin || EdGraphPin->Direction != TargetPinType)
			continue; // Skip if it's an input pin

		// EdGraphPin->LinkedTo;

		// GraphPanel->GetGraphObj()->AddOnGraphChangedHandler();
		// UEdGraph* Graph = GraphPanel->GetGraphObj();
		// if (!Graph)
		// 	return;

		const FVector2D OriginCurPos = SlateApp.GetCursorPos();

		// Switch to Insert mode to immediately type in the Node Menu
		FVimInputProcessor::Get()->SetVimMode(SlateApp, EVimMode::Insert);

		// Store the current number of nodes we have so we can compare it
		// after the menu window has closed to see if a new node was created.
		NodeCounter = GraphPanel->GetChildren()->Num();
		TWeakPtr<SGraphNode> WeakGraphNode = NodeWidget;

		// TODO:
		// Check if is connected to any node. If so, we will need to move it
		// and the entire following chain it is connected to - to the right
		// before inserting or appending a new node

		// TODO: We only need to do the nodes offseting if we have any connected nodes
		// to the direction we're append or inserting (etc.)
		FVector2f CursorOffset;
		FVector2f NodesOffset;
		if (InKey == FKey(EKeys::I))
		{
			CursorOffset = FVector2f(-150.0f, 0.0f);
			NodesOffset = FVector2f(150.0f, 0.0f);
		}
		else
		{
			CursorOffset = FVector2f(150.0f, 0.0f);
			NodesOffset = FVector2f(-150.0f, 0.0f);
		}
		MoveConnectedNodesToRight(NodeWidget->GetNodeObj(), NodesOffset.X);
		FUMInputHelpers::DragAndReleaseWidgetAtPosition(
			Pin,
			(Pin->GetCachedGeometry().LocalToAbsolute(CursorOffset)));

		FTimerHandle TimerHandle;
		GEditor->GetTimerManager()->SetTimer(
			TimerHandle,
			[this, &SlateApp, OriginCurPos, WeakGraphNode]() {
				SlateApp.SetCursorPos(OriginCurPos);
				const TSharedPtr<SWindow> MenuWin = SlateApp.GetActiveTopLevelWindow(); // Should be the menu

				if (!MenuWin.IsValid())
					return;

				MenuWin->GetOnWindowClosedEvent().AddLambda(
					[this, &SlateApp, WeakGraphNode](const TSharedRef<SWindow>& Window) {
						FTimerHandle TimerHandle;
						GEditor->GetTimerManager()->SetTimer(
							TimerHandle,
							[this, &SlateApp, WeakGraphNode]() {
								OnNodeCreationMenuClosed(SlateApp, WeakGraphNode);
								Logger.Print("Menu Closed!", ELogVerbosity::Log, true);
							},
							0.05f, false);
					});
			},
			0.05f, false);
		// SlateApp.SetCursorPos(OriginCurPos);
		break;
	}
}

void UVimGraphEditorSubsystem::OnNodeCreationMenuClosed(FSlateApplication& SlateApp, TWeakPtr<SGraphNode> AssociatedNode)
{
	if (const TSharedPtr<SGraphNode> GraphNode = AssociatedNode.Pin())
	{
		const TSharedPtr<SGraphPanel> GraphPanel = GraphNode->GetOwnerPanel();
		if (!GraphPanel.IsValid())
			return;

		if (NodeCounter < GraphPanel->GetChildren()->Num())
		{
			Logger.Print("New node was created!", ELogVerbosity::Log, true);
			// GraphPanel->SelectionManager.SetSelectionSet()

			FUMInputHelpers::SimulateClickOnWidget(
				FSlateApplication::Get(),
				GraphNode.ToSharedRef(),
				FKey(EKeys::LeftMouseButton), false,
				true /* Shift Down (add to selection)*/);

			GraphPanel->StraightenConnections();

			FUMInputHelpers::SimulateClickOnWidget(
				FSlateApplication::Get(),
				GraphNode.ToSharedRef(),
				FKey(EKeys::LeftMouseButton), false, false,
				true /* Ctrl Down (remove from selection) */);
		}
		else
			Logger.Print("No new nodes were created...", ELogVerbosity::Log, true);

		// Switch to Normal mode to immediately (probably preferred)
		FVimInputProcessor::Get()->SetVimMode(SlateApp, EVimMode::Normal);
	}
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

#undef LOCTEXT_NAMESPACE
