#include "VimGraphEditorSubsystem.h"
#include "BlueprintEditor.h"
#include "EdGraphNode_Comment.h"
#include "UMConfig.h"
#include "GraphEditorModule.h"
#include "Kismet2/BlueprintEditorUtils.h"
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

// DEFINE_LOG_CATEGORY_STATIC(LogVimGraphEditorSubsystem, NoLogging, All); // Prod
DEFINE_LOG_CATEGORY_STATIC(LogVimGraphEditorSubsystem, Log, All); // Dev

#define LOCTEXT_NAMESPACE "VimGraphEditorSubsystem"

bool UVimGraphEditorSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	TSharedRef<FUMConfig> Config = FUMConfig::Get();
	// if Vim is enabled, TabNavigator must also be enabled.
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
		{ EKeys::SpaceBar, EKeys::G, EKeys::D },
		WeakGraphSubsystem,
		&UVimGraphEditorSubsystem::DebugEditor);
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

#undef LOCTEXT_NAMESPACE
