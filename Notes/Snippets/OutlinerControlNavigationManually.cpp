#include "VimEditorSubsystem.h"
#include "Brushes/SlateColorBrush.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Templates/SharedPointer.h"
#include "UMHelpers.h"
#include "UMInputPreProcessor.h"
#include "Widgets/Docking/SDockTab.h"
#include "UMWindowsNavigationManager.h"
#include "UMTabsNavigationManager.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STreeView.h"
#include "Editor/SceneOutliner/Public/SSceneOutliner.h"
// #include "Editor/SceneOutliner/Public/ISceneOutlinerTreeItem.h"
#include "ISceneOutlinerTreeItem.h"
#include "Folder.h"
#include "SceneOutlinerStandaloneTypes.h"
#include "FolderTreeItem.h"

// TODO: if none is activated and we're in a valid widget we should just try to focus on the first element or something
void UVimEditorSubsystem::ProcessVimNavigationInput(
	FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	static const TSet<FName> ValidWidgetNavigationTypes{
		"SAssetTileView",
		"SSceneOutlinerTreeView",
		"STreeView",
		"SSubobjectEditorDragDropTree"
	};

	TSharedPtr<SWidget> FocusedWidget = SlateApp.GetUserFocusedWidget(0);
	if (!FocusedWidget.IsValid())
	{
		FUMHelpers::NotifySuccess(FText::FromString("Focused widget NOT valid"));
		return;
	}

	if (!ValidWidgetNavigationTypes.Find(FocusedWidget->GetType())
		&& !ValidWidgetNavigationTypes.Find(
			FName(FocusedWidget->GetTypeAsString().Left(9))))
	{
		FUMHelpers::NotifySuccess(FText::FromString(
			FString::Printf(TEXT("Not a valid type: %s"),
				*FocusedWidget->GetTypeAsString())));
		// FUMHelpers::NotifySuccess(FText::FromString(FocusedWidget->GetTypeAsString().Left(9)));
		return;
	}

	// TSharedPtr<SSceneOutlinerTreeView> Outliner =
	// 	StaticCastSharedPtr<SSceneOutlinerTreeView>(FocusedWidget);
	TSharedPtr<SListView<TSharedPtr<ISceneOutlinerTreeItem>>> Outliner =
		StaticCastSharedPtr<SListView<
			TSharedPtr<ISceneOutlinerTreeItem>>>(FocusedWidget);
	if (!Outliner.IsValid())
	{
		FUMHelpers::NotifySuccess(FText::FromString("Not Outliner"));
		return;
	}
	// FUMHelpers::NotifySuccess(
	// 	FText::FromString(FString::Printf(TEXT("Widget Type: %s"),
	// 		*FocusedWidget->GetTypeAsString())));

	TArray<TSharedPtr<ISceneOutlinerTreeItem>> SelectedItems =
		Outliner->GetSelectedItems();

	if (SelectedItems.IsEmpty())
	{
		// FUMHelpers::NotifySuccess(FText::FromString("Non-Selected Items"));
		return;
	}
	TSharedPtr<ISceneOutlinerTreeItem, ESPMode::ThreadSafe> FirstSel = SelectedItems[0];
	TSharedPtr<ISceneOutlinerTreeItem, ESPMode::ThreadSafe> LastSel = SelectedItems.Last();

	TArrayView<const TSharedPtr<ISceneOutlinerTreeItem, ESPMode::ThreadSafe>>
		Arr = Outliner->GetItems(); // GetRootItems doesn't seem work...

	int32 WidgetIndex{ INDEX_NONE };
	int32 Cursor{ 0 };
	for (const TSharedPtr<ISceneOutlinerTreeItem, ESPMode::ThreadSafe>& Item : Arr)
	{
		if (Item.IsValid() && Item == FirstSel)
		{
			WidgetIndex = Cursor;
			break;
		}
		++Cursor;
	}

	// Update the widget index based on the navigation input
	if (InKeyEvent.GetKey() == EKeys::H || InKeyEvent.GetKey() == EKeys::K)
	{
		WidgetIndex -= 1; // Navigate left or up
	}
	else if (InKeyEvent.GetKey() == EKeys::J || InKeyEvent.GetKey() == EKeys::L)
	{
		WidgetIndex += 1; // Navigate right or down
	}

	// Ensure the new index is within bounds
	if (Arr.IsValidIndex(WidgetIndex))
	{
		TSharedPtr<ISceneOutlinerTreeItem, ESPMode::ThreadSafe> NewItem =
			Arr[WidgetIndex];
		Outliner->SetSelection(NewItem, ESelectInfo::OnNavigation);
		// NewItem->GetID();
		// if (NewItem->GetRootObject() != FFolder::GetInvalidRootObject())
		// 	FUMHelpers::NotifySuccess(FText::FromString("Folder!"));
		// Will crash on some classes like Content Browser and BP outliner
		if (NewItem->IsA<FFolderTreeItem>())
		{
			FUMHelpers::NotifySuccess(FText::FromString("Folder!"));
		}

		// NOTE: Unreal automatically deselects folder items,
		// so we need to reapply the selection to ensure it stays selected.
		FTimerHandle TimerHandle;
		GEditor->GetTimerManager()->SetTimer(
			TimerHandle,
			[Outliner, NewItem]() {
				if (Outliner && NewItem.IsValid()
					&& !Outliner->IsItemSelected(NewItem))
					Outliner->SetSelection(NewItem, ESelectInfo::OnNavigation);
			},
			0.01f, // 10ms delay
			false  // Do not loop
		);
		// DebugTreeItem(NewItem, WidgetIndex);
		return;
	}
	FUMHelpers::NotifySuccess(FText::FromString("Cannot move to item."));
	return;

	FKeyEvent OutKeyEvent;
	MapVimToArrowNavigation(InKeyEvent, OutKeyEvent);

	int32 Count = MIN_REPEAT_COUNT;
	if (!CountBuffer.IsEmpty())
	{
		Count = FMath::Clamp(
			FCString::Atoi(*CountBuffer),
			MIN_REPEAT_COUNT,
			MAX_REPEAT_COUNT);
	}

	for (int32 i{ 0 }; i < Count; ++i)
	{
		FUMInputPreProcessor::ToggleNativeInputHandling(true);
		SlateApp.ProcessKeyDownEvent(OutKeyEvent);
		// FUMInputPreProcessor::SimulateKeyPress(
		// SlateApp, FKey(EKeys::BackSpace));  // BackSpace might be useful
	}
}
