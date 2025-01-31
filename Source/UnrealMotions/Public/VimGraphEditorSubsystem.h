#pragma once

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

	void DebugEditor();

	void AddNode(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	void OnNodeCreationMenuClosed(FSlateApplication& SlateApp, TWeakPtr<SGraphNode> AssociatedNode);

	bool IsWasNewNodeCreated();

	bool IsValidZoom(const FString InZoomLevelStr);

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
};
