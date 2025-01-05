#include "UMFocusManager.h"
#include "Framework/Docking/TabManager.h"
#include "UMHelpers.h"
#include "Editor.h"
#include "Widgets/Docking/SDockTab.h"

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

// OnTabForegrounded will kick-in and potentially be useful in cases where
// we drag tabs to other tabwells // detach them from tabwells. In these cases
// this event will be called while OnActiveTabChanged won't because the active
// tab isn't changed, it was just moved to a new window.
// But not sure how helpful is this because when we want to move between tabs
// we will anyway check the parent tab well. So...?
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
	// It looks like when we detach (drag-from) Major Tab and then land somewhere,
	// our focus will behave a bit unexpectedly focusing on the tab in the back
	// of the tab we've just dragged. This feels a bit odd. So we need to track
	// which tab we're currently dragging around so we can retain focus on it
	// manually. WIP.

	FSlateApplication& SlateApp = FSlateApplication::Get();

	// The IsDragDropping() func seems to do the job without this.
	// const TSet<FKey>&  PressedButtons = SlateApp.GetPressedMouseButtons();

	if (SlateApp.IsDragDropping())
	{
		bIsTabForegrounding = true; // Bypass the misinformed OnActiveTabChanged

		// It seems to sometimes think it's processing a certain incorrect tab,
		// but it will eventually settle on the correct one. So we can store
		// that as a reference which will end up with the correct tab.
		FText LogTab = FText::FromString("Invalid Tab");
		if (NewActiveTab.IsValid())
		{
			LogTab = NewActiveTab->GetTabLabel();
			ForegroundedProcessedTab = NewActiveTab;
		}
		FUMHelpers::NotifySuccess(
			FText::FromString(FString::Printf(TEXT("OnTabForegrounded - Processing: %s"), *LogTab.ToString())), bVisualLog);

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
}

void FUMFocusManager::SetCurrentTab(const TSharedRef<SDockTab> NewTab)
{
	// Find last active minor tab for major tab!

	if (NewTab->GetVisualTabRole() == ETabRole::MajorTab)
	{
		if (ActiveMajorTab.IsValid() && ActiveMajorTab == NewTab)
			return;

		LogTabChange(TEXT("Major"), ActiveMajorTab, NewTab);
		ActiveMajorTab = NewTab;

		// Start working with our maps
		if (LastActiveTabWellByMajorTabId.Find(NewTab->GetId()))
		{
			// Continue searching
		}
		else
		{
			// Reprocess
		}
	}
	else // Minor Tab
	{
		if (ActiveMinorTab.IsValid() && ActiveMinorTab == NewTab)
			return;

		LogTabChange(TEXT("Minor"), ActiveMinorTab, NewTab);
		ActiveMinorTab = NewTab;
	}
	// The focus seems to be retain ok? But we will probably anyway handle this
	// more specifically
	NewTab->ActivateInParent(ETabActivationCause::SetDirectly);
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
