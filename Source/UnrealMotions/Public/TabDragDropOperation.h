#pragma once

#include "CoreMinimal.h"
#include "Input/DragAndDrop.h"

class FTabDragDropOperation : public FDragDropOperation
{
public:
	DRAG_DROP_OPERATOR_TYPE(FTabDragDropOperation, FDragDropOperation)

	// The tab being dragged
	TSharedPtr<class SDockTab> DraggedTab;

	// Factory method to create the drag-drop operation
	static TSharedRef<FTabDragDropOperation> New(TSharedPtr<SDockTab> InDraggedTab)
	{
		TSharedRef<FTabDragDropOperation> Operation = MakeShareable(new FTabDragDropOperation());
		Operation->DraggedTab = InDraggedTab;
		Operation->Construct();
		return Operation;
	}

	// Optionally override OnDragged for custom behavior
	virtual void OnDragged(const FDragDropEvent& DragDropEvent) override
	{
		// You can update the decorator position or perform other logic
		FDragDropOperation::OnDragged(DragDropEvent);
	}

	// Optionally override OnDrop to handle the drop action
	virtual void OnDrop(bool bDropWasHandled, const FPointerEvent& MouseEvent) override
	{
		if (!bDropWasHandled)
		{
			// Logic to handle when the drop wasn't processed
		}
	}
};
