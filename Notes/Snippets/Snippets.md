	// UWorld* EditorWorld = nullptr;

	// // Example: Get the “editor world context” if one is available.
	// if (GEditor)
	// {
	// 	// This returns the first world used by the Editor (the level you see open).
	// 	FWorldContext& EditorWorldContext = GEditor->GetEditorWorldContext();
	// 	EditorWorld = EditorWorldContext.World();
	// }

	// if (EditorWorld)
	// {
	// 	FTimerHandle TimerHandle;
	// 	EditorWorld->GetTimerManager().SetTimer(
	// 		TimerHandle,
	// 		FTimerDelegate::CreateLambda([this, &SlateApp]() {
	// 		}),
	// 		3.0f, // Delay
	// 		false // bLoop
	// 	);
	// }



// Timer
		FTimerHandle TimerHandle;
		GEditor->GetTimerManager()->SetTimer(
			TimerHandle,
			[Outliner, NewItem]() {
				Outliner->SetSelection(NewItem, ESelectInfo::OnNavigation);
			},
			0.01f, // 10ms delay
			false  // Do not loop
		);

// Next tick
		GEditor->GetTimerManager()->SetTimerForNextTick(
			[Outliner, NewItem]() {
				Outliner->SetSelection(NewItem, ESelectInfo::OnNavigation);
			});


// Outliner methods
	// Outliner->GetNumItemsSelected();
	// Outliner->FlashHighlightOnItem();
	// Outliner->GetRootItems();
	// Outliner->ItemFromWidget();
	// Outliner->GetOutlinerPtr();
	// Outliner->GetScrollWidget();
	// Outliner->GetSelectedItems();
	// Outliner->RequestNavigateToItem();
	// Outliner->GetRootItems();
	// Outliner->GetNumGeneratedChildren();  // Only visible ones (not all)
	// Outliner->RequestScrollIntoView(Outliner->GetSelectedItems()[0]);
	// Outliner->ScrollToTop();
	// Outliner->ScrollToBottom();

	int32 NumSelected = Outliner->GetNumItemsSelected();
	int32 NumChildren = Outliner->GetItems().Num();
	int32 NumRootChildren = Outliner->GetRootItems().Num();


	// MAP
	// TSet<TSharedPtr<ISceneOutlinerTreeItem, ESPMode::ThreadSafe>,
	// 	DefaultKeyFuncs<TSharedPtr<ISceneOutlinerTreeItem, ESPMode::ThreadSafe>>>
	// 	ExpandedItems;
	// Outliner->GetExpandedItems(ExpandedItems);
	// ExpandedItems.Find(FirstSel);

	// Helpful for Vertical box?
	// Find the index of the focused widget in its parent's children
	// int32 WidgetIndex = FindWidgetIndexInParent(FocusedWidget.ToSharedRef());
	// if (WidgetIndex == INDEX_NONE)
	// 	return;

