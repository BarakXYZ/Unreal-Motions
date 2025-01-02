		// Same for all
		// EFlowDirectionPreference FlowDir = ListView->GetFlowDirectionPreference();

		// Same for all
		// FSlateWidgetPersistentState PerState = ListView->GetPersistentState();

		// Just vertical for everything :(
		// if (ListView->Private_GetOrientation() == EOrientation::Orient_Vertical)
		// 	FUMHelpers::NotifySuccess(FText::FromString("Vertical Orientation"), bVisualLog);
		// if (ListView->Private_GetOrientation() == EOrientation::Orient_Horizontal)
		// 	FUMHelpers::NotifySuccess(FText::FromString("Horizontal Orientation"), bVisualLog);

		// Another method, but it's more visual than actually selecting.
		// Is visually selecting correctly at least!
		// TSharedPtr<ISceneOutlinerTreeItem, ESPMode::ThreadSafe> StartItem;
		// if (bIsShiftDown)
		// 	StartItem = IsAnchorToTheLeft
		// 		? SelItems.Last()
		// 		: SelItems[0];
		// else
		// 	StartItem = IsAnchorToTheLeft
		// 		? SelItems[0]
		// 		: SelItems.Last();

		// ListView->ClearSelection();
		// ListView->SetSelection(StartItem, ESelectInfo::OnNavigation);
		// ListView->Private_SelectRangeFromCurrentTo(GotoItem);
		// return;

	// I think that's the way to get the currently sel item
	// ListView->Private_HasSelectorFocus(Item);

	int32 Cur{ 0 };
	for (const auto& Item : ListItems)
	{
		// if (ListView->Private_IsItemSelectableOrNavigable(Item))
		// {
		// 	// if (!ListView->Private_DoesItemHaveChildren(Cur))
		// 	FilteredList.Add(Item);
		// }
		// else
		// 	FUMHelpers::NotifySuccess(
		// 		FText::FromString("Item is not selectable or navigable!"));
		// ++Cur;
		FilteredList.Add(Item);
	}


