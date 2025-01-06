#include "UMFocusManager.h"
#include "Framework/Docking/TabManager.h"
#include "Input/Events.h"
#include "UMHelpers.h"
#include "Editor.h"
#include "UMTabsNavigationManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "UMSlateHelpers.h"

const TSharedPtr<FUMFocusManager> FUMFocusManager::FocusManager =
	MakeShared<FUMFocusManager>();

FUMFocusManager::FUMFocusManager()
{
	FCoreDelegates::OnPostEngineInit.AddRaw(
		this, &FUMFocusManager::RegisterSlateEvents);
}

FUMFocusManager::~FUMFocusManager()
{
}

const TSharedPtr<FUMFocusManager>& FUMFocusManager::Get()
{
	return FocusManager;
}

void FUMFocusManager::RegisterSlateEvents()
{
	FSlateApplication& SlateApp = FSlateApplication::Get();

	SlateApp.OnFocusChanging()
		.AddRaw(this, &FUMFocusManager::OnFocusChanged);

	////////////////////////////////////////////////////////////////////////
	//						~ Tabs Delegates ~
	//
	TSharedRef<FGlobalTabmanager> GTM = FGlobalTabmanager::Get();

	GTM->OnActiveTabChanged_Subscribe(
		FOnActiveTabChanged::FDelegate::CreateRaw(
			this, &FUMFocusManager::OnActiveTabChanged));

	// This seems to trigger in some cases where OnActiveTabChanged won't.
	// Keeping this as a double check.
	GTM->OnTabForegrounded_Subscribe(
		FOnActiveTabChanged::FDelegate::CreateRaw(
			this, &FUMFocusManager::OnTabForegrounded));
	//
	////////////////////////////////////////////////////////////////////////
}

void FUMFocusManager::OnFocusChanged(const FFocusEvent& FocusEvent, const FWeakWidgetPath& OldWidgetPath,
	const TSharedPtr<SWidget>& OldWidget, const FWidgetPath& NewWidgetPath,
	const TSharedPtr<SWidget>& NewWidget)
{
	TSharedRef<FTimerManager> TimerManager = GEditor->GetTimerManager();
	if (TimerManager->IsTimerActive(TimerHandleNewWidgetTracker))
		TimerManager->ClearTimer(TimerHandleNewWidgetTracker);

	TWeakPtr<SWidget> WeakNewWidget = NewWidget; // For safety as it may be removed

	// NOTE:
	// We want to have a very small delay to insure tab delegates will register
	// and update before this new widget assignment is performed.
	// We can't just call this from the tab registration after its done because
	// the OnFocusChanged will be called at times where the tab delegates won't.
	// And in these cases, we still need to register and track the newly active
	// widget.
	TimerManager->SetTimer(
		TimerHandleNewWidgetTracker,
		[this, WeakNewWidget]() {
			FUMHelpers::NotifySuccess(
				FText::FromString("Focus Manager On Focused Changed"), bVisualLog);

			if (!WeakNewWidget.IsValid() || WeakNewWidget == ActiveWidget)
				return; // return if match the currently tracked widget

			// New Widget found:
			ActiveWidget = WeakNewWidget;

			if (ActiveMinorTab.IsValid())
			{
				const TSharedPtr<SDockTab>& MinorTab = ActiveMinorTab.Pin();
				if (MinorTab)
				{
					const uint64& TabId = MinorTab->GetId();
					if (LastActiveWidgetByMinorTabId.FindRef(TabId) != ActiveWidget)
						LastActiveWidgetByMinorTabId.Add(TabId, ActiveWidget);
				}
			}
			else // Fallback to the globally active Minor Tab
			{
				if (const TSharedPtr<SDockTab>& NewMinorTab =
						FGlobalTabmanager::Get()->GetActiveTab())
				{
					const uint64& NewTabId = NewMinorTab->GetId();
					if (LastActiveWidgetByMinorTabId.FindRef(NewTabId) != ActiveWidget)
						LastActiveWidgetByMinorTabId.Add(NewTabId, ActiveWidget);

					ActiveMinorTab = NewMinorTab;
				}
				else
					FUMHelpers::NotifySuccess(
						FText::FromString("No active Minor (Global) Tab found."),
						bVisualLog);
			}
		},
		// We want to be pretty quick and expressive with this delay becauase
		// the user might change to a new widget and immediately jump to a new
		// minor tab. In this case we will need to track the widget quickly before
		// jumping, and if we have a large delay, it will invalidate the old timer
		// which will discard the widget tracking for the old panel.
		0.025f, false);
	// 0.1f, false);  // Potentially too slow
}

// NOTE: there's an incorrect use of the delegate being called internally by UE.
// It specifies the delegate as firstly the NewActive, then OldTab. But in reality
// calls the OldTab, then NewTab. I wrote more about that in the BugNotes folder,
// but anyways, I've flipped the params to match what's actually being passed.
void FUMFocusManager::OnActiveTabChanged(
	TSharedPtr<SDockTab> PrevActiveTab, TSharedPtr<SDockTab> NewActiveTab)
{
	TSharedRef<FTimerManager> TimerManager = GEditor->GetTimerManager();
	if (TimerManager->IsTimerActive(TimerHandleNewMinorTabTracker))
		TimerManager->ClearTimer(TimerHandleNewMinorTabTracker);

	TWeakPtr<SDockTab> WeakNewTab = NewActiveTab;

	TimerManager->SetTimer(
		TimerHandleNewMinorTabTracker,
		[this, WeakNewTab]() {
			if (bIsTabForegrounding)
				return;

			FUMHelpers::NotifySuccess(FText::FromString("OnActiveTabChanged"), bVisualLog);
			if (!WeakNewTab.IsValid())
				return;

			// NOTE: We first need to assign the new Major Tab to ensure proper
			// tracking order (MajorTab->MinorTab->NewWidget)
			// TODO: Debug this further
			const TSharedPtr<FTabManager> TabManager =
				WeakNewTab.Pin()->GetTabManagerPtr();
			if (TabManager.IsValid())
			{
				const TSharedPtr<SDockTab> MajorTab =
					FGlobalTabmanager::Get()->GetMajorTabForTabManager(
						TabManager.ToSharedRef());

				if (MajorTab.IsValid())
					SetCurrentTab(MajorTab.ToSharedRef());
			}

			// Then we set the Minor Tab
			SetCurrentTab(WeakNewTab.Pin().ToSharedRef());
		},
		0.0125f,
		false);
}

void FUMFocusManager::OnTabForegrounded(
	TSharedPtr<SDockTab> NewActiveTab, TSharedPtr<SDockTab> PrevActiveTab)
{
	// NOTE:
	// We want to distinguish between foregrounding in transit and foregrounding
	// that has landed. While in transit the behavior seems to be a bit wonky
	// i.e. Unreal will mark tabs as new active ones while we're currently really
	// still just foregrounding. So we need to determine if we're still
	// processing the foreground operation.
	// NOTE:
	// It looks like when we detach (drag-from) Major Tab and then land somewhere;
	// our focus will behave a bit unexpectedly focusing on the tab in the back
	// of the tab we've just dragged. This feels a bit odd. So we need to track
	// which tab we're currently dragging around so we can retain focus on it
	// manually.

	FSlateApplication& SlateApp = FSlateApplication::Get();

	// The IsDragDropping() func seems to do the job without this + it's better
	// not to rely on the pressed buttons too much because we may add in the near
	// future functionality to detch and relocate tabs via keyboard.
	// const TSet<FKey>&  PressedButtons = SlateApp.GetPressedMouseButtons();

	if (SlateApp.IsDragDropping())
	{
		bIsTabForegrounding = true; // Bypass the misinformed OnActiveTabChanged

		// It seems to sometimes think it's processing a certain incorrect tab,
		// but it will eventually settle on the correct one. So we can store
		// that as a reference which will end up setting the correct tab.
		FText LogTab = FText::FromString("Invalid Tab");
		if (NewActiveTab.IsValid())
		{
			LogTab = NewActiveTab->GetTabLabel();
			ForegroundedProcessedTab = NewActiveTab;
		}
		if (!bIsDummyForegroundingCallbackCheck)
			FUMHelpers::NotifySuccess(
				FText::FromString(FString::Printf(TEXT("OnTabForegrounded - Processing: %s"), *LogTab.ToString())), bVisualLog);
		// else
		// 	FUMHelpers::NotifySuccess(FText::FromString("Dummy Check"));
		// Set false to allow regular logging.
		bIsDummyForegroundingCallbackCheck = false;

		// NOTE:
		// Since sometimes the delegate seems to not trigger again when the
		// foregrounding is finished - we have to set this timer to verify
		// manually if we've already completed the foregrounding operation.
		TSharedRef<FTimerManager> TimerManager = GEditor->GetTimerManager();
		if (!TimerManager->IsTimerActive(TimerHandleTabForegrounding))
			TimerManager->SetTimer(
				TimerHandleTabForegrounding,
				[this, TimerManager]() {
					TimerManager->ClearTimer(TimerHandleTabForegrounding);

					// When calling the function again, we don't want to
					// influence which tab should be focused next, but just to
					// verify we finish the operation at all. Thus we pass
					// null pointers as the params (which will be ignored by the
					// default null check) and mark a flag for logging purposes
					// and potentially additional functionalities.
					if (bIsTabForegrounding)
					{
						bIsDummyForegroundingCallbackCheck = true;
						OnTabForegrounded(nullptr, nullptr);
					}
				},
				0.15f, false);
		return;
	}

	FUMHelpers::NotifySuccess(
		FText::FromString("OnTabForegrounded - Finished"), bVisualLog);

	// Not we just want to check which tab should be forwarded for set as current.
	// *if TabForegrounding flag is true; we want to pass the manually tracked tab
	// (We're probably dragging tabs around).
	// *else we want to pass the NewActiveTab (param)
	// (We're probably cycling or directly clicking on tabs).
	if (bIsTabForegrounding && ForegroundedProcessedTab.IsValid())
		SetCurrentTab(ForegroundedProcessedTab.Pin().ToSharedRef());
	else if (NewActiveTab.IsValid())
		SetCurrentTab(NewActiveTab.ToSharedRef());
	else
		FUMHelpers::NotifySuccess(
			FText::FromString("No valid new tab found in OnTabForegrounded"),
			bVisualLog);

	// OnActiveTabChanged should be safe to continue processing from now on
	// though it might be healthy to even delay this mark a bit (like by 25ms or
	// something) to insure we don't bring focus to a none-desired tab.
	bIsTabForegrounding = false;
	bIsDummyForegroundingCallbackCheck = false;
}

void FUMFocusManager::SetCurrentTab(const TSharedRef<SDockTab> NewTab)
{
	auto& SlateApp = FSlateApplication::Get();

	if (NewTab->GetVisualTabRole() == ETabRole::MajorTab)
	{
		if (ActiveMajorTab.IsValid() && ActiveMajorTab == NewTab)
			return; // Not a new Major Tab

		LogTabChange(TEXT("Major"), ActiveMajorTab, NewTab);
		ActiveMajorTab = NewTab;

		// if we have any user focus, we don't need to remap our user's focus to
		// something else. He had probably clicked and focused on something
		// already manually with the mouse.
		if (NewTab->GetContent()->HasAnyUserFocusOrFocusedDescendants())
		{
			FUMHelpers::NotifySuccess(
				FText::FromString("Major tab already had focus. Skipping."),
				bVisualLog);
			return;
		}
		// In case we do not have any focus:
		// Lookup the associated TabWell and activate the most recent tab in it.
		else if (const TWeakPtr<SWidget>* TabWell =
					 LastActiveTabWellByMajorTabId.Find(NewTab->GetId()))
		{
			if (TabWell->IsValid() && TabWell->Pin().IsValid())
				if (FChildren* Tabs = TabWell->Pin()->GetChildren())
				{
					for (int32 i{ 0 }; i < Tabs->Num(); ++i)
					{
						TSharedRef<SDockTab> Tab =
							StaticCastSharedRef<SDockTab>(Tabs->GetChildAt(i));
						if (Tab->IsForeground()) // Get the latest tab in TabWell
						{
							FUMHelpers::NotifySuccess(
								FText::FromString("Focused MinorTab in TabWell"));
							Tab->ActivateInParent(ETabActivationCause::SetDirectly);
							return;
						}
					}
				}
		}
		// Lookup all the TabWells and bring focus to the first one + tab in it.
		else
		{
			// TWeakPtr<SWidget> DockingArea;
			// if (!FUMSlateHelpers::TraverseWidgetTree(
			// 		NewTab->GetContent(), DockingArea, "SDockingArea"))
			// 	return false;

			// Next stop will be the splitter within it, which also includes all.
			// TWeakPtr<SWidget> Splitter;
			// if (!FUMSlateHelpers::TraverseWidgetTree(
			// 		NewTab->GetContent(), Splitter, "SSplitter"))
			// 	// Or check inside the DockingArea specifically?
			// 	return;

			// // Collect all SDockTab(s)
			// TArray<TWeakPtr<SWidget>>
			// 	FoundPanelTabs;
			// if (FUMSlateHelpers::TraverseWidgetTree(
			// 		Splitter.Pin(), FoundPanelTabs, "STabWell"))
			// {
			// }
		}
	}
	else // Minor Tab
	{
		if (ActiveMinorTab.IsValid() && ActiveMinorTab == NewTab)
			return; // Not a new Minor Tab

		LogTabChange(TEXT("Minor"), ActiveMinorTab, NewTab);
		ActiveMinorTab = NewTab;

		// Register this tab's TabWell with the current Active Major Tab parent.
		if (ActiveMajorTab.IsValid() && ActiveMajorTab.Pin().IsValid())
			LastActiveTabWellByMajorTabId.Add(
				ActiveMajorTab.Pin()->GetId(), NewTab->GetParentWidget());
		// Not sure why we wouldn't have a valid ref really:
		// But we may want to handle this scenario and grab a valid Major Tab
		else
			FUMHelpers::NotifySuccess(
				FText::FromString("Can't regiser TabWell: No valid Major Tab"));

		// Check if there's an associated last widget we can focus on
		if (const TWeakPtr<SWidget>* LastWidget =
				LastActiveWidgetByMinorTabId.Find(NewTab->GetId()))
		{
			if (LastWidget->IsValid() && LastWidget->Pin().IsValid())
			{
				SlateApp.SetAllUserFocus(
					LastWidget->Pin().ToSharedRef(), EFocusCause::Navigation);
				FUMHelpers::NotifySuccess(
					FText::FromString(FString::Printf(TEXT("Widget Found: %s"),
						*LastWidget->Pin()->GetTypeAsString())),
					bVisualLog);
			}
		}
	}
}

void FUMFocusManager::LogTabChange(const FString& TabType, const TWeakPtr<SDockTab>& CurrentTab, const TSharedRef<SDockTab>& NewTab)
{
	FString PrevTabLog = CurrentTab.IsValid()
		? FString::Printf(TEXT("Previous: %s (ID: %d)\n"), *CurrentTab.Pin()->GetTabLabel().ToString(), CurrentTab.Pin()->GetId())
		: TEXT("None");

	FString NewTabLog = FString::Printf(TEXT("New: %s (ID: %d)\n"),
		*NewTab->GetTabLabel().ToString(), NewTab->GetId());

	const FString LogStr = FString::Printf(
		TEXT("Set New %s Tab:\n%s%s"), *TabType, *PrevTabLog, *NewTabLog);

	UE_LOG(LogTemp, Log, TEXT("%s"), *LogStr);
	FUMHelpers::NotifySuccess(FText::FromString(LogStr), bVisualLog);
}
