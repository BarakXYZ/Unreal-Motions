// Traverse and print spawner names after fetching the tab manager

		TSharedPtr<SDockTab> MinorTab =
			StaticCastSharedPtr<SDockTab>(FoundMinorTab.Pin());

		TSharedPtr<FTabManager> TabManager = MinorTab->GetTabManagerPtr();
		if (!TabManager.IsValid())
			return;

		TArray<TWeakPtr<FTabSpawnerEntry>> Spawners =
			TabManager->CollectSpawners();

		for (const auto& Spawner : Spawners)
		{
			if (Spawner.IsValid())
				FUMHelpers::NotifySuccess(Spawner.Pin()->GetDisplayName());
		}



// Potential traverse optimization? 

		FChildren* SChilds = Splitter.Pin()->GetChildren();

		int32 NumOfChildren = SChilds->Num();
		if (NumOfChildren == 0)
			return;

		for (int32 i = 0; i < NumOfChildren; ++i)
		{
			const TSharedRef<SWidget>& Child = SChilds->GetChildAt(i);
			if (Child->GetVisibility() != EVisibility::Visible)
				continue;

			FName ChildType = Child->GetType();
			if (ChildType.IsEqual(TEXT("SDockingSplitter")))
			{
				// Find potentially multiple tabwells
				Child->GetChildren()->GetChildAt(0);
			}

			else if (ChildType.IsEqual(TEXT("SDockingTabStack")))
			{
				// Find 1 tabwell
			}
		}


				// because SDockingTabStack is private and we can't even
				// cast it to an SWidget
				TSharedPtr<SDockingTabStack> TabStack =
					DockTab->GetParentDockTabStack();
				// Same
				TSharedPtr<SDockingArea> DockArea =
					DockTab->GetDockArea();

