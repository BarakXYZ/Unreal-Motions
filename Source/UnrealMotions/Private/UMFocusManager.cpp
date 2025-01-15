#include "UMFocusManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
// #include "GenericPlatform/GenericWindow.h"
#include "GenericPlatform/GenericWindow.h"
#include "Input/Events.h"
#include "Logging/LogVerbosity.h"
#include "UMLogger.h"
#include "Editor.h"
#include "Widgets/Docking/SDockTab.h"
#include "UMSlateHelpers.h"
#include "UMWindowsNavigationManager.h"
#include "UMInputHelpers.h"
#include "UMFocusVisualizer.h"

// DEFINE_LOG_CATEGORY_STATIC(LogUMFocusManager, NoLogging, All); // Prod
DEFINE_LOG_CATEGORY_STATIC(LogUMFocusManager, Log, All); // Dev
FUMLogger FUMFocusManager::Logger(&LogUMFocusManager);

using Log = FUMLogger;
using Sym = EUMLogSymbol;

const TSharedPtr<FUMFocusManager> FUMFocusManager::FocusManager =
	MakeShared<FUMFocusManager>();

FUMFocusManager::FUMFocusManager()
{
	FCoreDelegates::OnPostEngineInit.AddRaw(
		this, &FUMFocusManager::RegisterSlateEvents);
}

FUMFocusManager::~FUMFocusManager()
{
	TSharedRef<FGlobalTabmanager> GTM = FGlobalTabmanager::Get();
	GTM->OnActiveTabChanged_Unsubscribe(DelegateHandle_OnActiveTabChanged);
	GTM->OnTabForegrounded_Unsubscribe(DelegateHandle_OnTabForegrounded);

	if (GEditor && GEditor->IsTimerManagerValid())
	{
		GEditor->GetTimerManager()->ClearAllTimersForObject(this);
	}
}

const TSharedPtr<FUMFocusManager>& FUMFocusManager::Get()
{
	return FocusManager;
}

void FUMFocusManager::RegisterSlateEvents()
{
	FSlateApplication& SlateApp = FSlateApplication::Get();

	SlateApp.OnFocusChanging()
		// .AddRaw(this, &FUMFocusManager::OnFocusChanged);
		.AddRaw(this, &FUMFocusManager::OnFocusChangedV2);

	SlateApp.OnWindowBeingDestroyed().AddRaw(
		this, &FUMFocusManager::OnWindowBeingDestroyed);

	////////////////////////////////////////////////////////////////////////
	//						~ Tabs Delegates ~							  //
	//																	  //
	TSharedRef<FGlobalTabmanager> GTM = FGlobalTabmanager::Get();

	DelegateHandle_OnActiveTabChanged = GTM->OnActiveTabChanged_Subscribe(
		FOnActiveTabChanged::FDelegate::CreateRaw(
			// this, &FUMFocusManager::OnActiveTabChanged));
			this, &FUMFocusManager::OnActiveTabChangedV2));

	// This seems to trigger in some cases where OnActiveTabChanged won't.
	// Keeping this as a double check.
	DelegateHandle_OnTabForegrounded = GTM->OnTabForegrounded_Subscribe(
		FOnActiveTabChanged::FDelegate::CreateRaw(
			// this, &FUMFocusManager::OnTabForegrounded));
			this, &FUMFocusManager::OnTabForegroundedV2));
	//																	  //
	//						~ Tabs Delegates ~							  //
	////////////////////////////////////////////////////////////////////////

	FUMWindowsNavigationManager::Get().OnWindowChanged.AddSP(
		FocusManager.ToSharedRef(), &FUMFocusManager::HandleOnWindowChanged);
}

void FUMFocusManager::DetectWidgetType(const TSharedRef<SWidget> InWidget)
{
	const FString WidgetType = InWidget->GetTypeAsString();
	if (FUMSlateHelpers::IsValidTreeViewType(WidgetType))
	{
		// Set enum... + ?
	}
	else if (FUMSlateHelpers::IsValidEditableText(WidgetType))
	{
		// Set enum... + ?
	}
	else
	{
		// Default enum... + ?
	}
}

void FUMFocusManager::OnFocusChanged(const FFocusEvent& FocusEvent, const FWeakWidgetPath& OldWidgetPath,
	const TSharedPtr<SWidget>& OldWidget, const FWidgetPath& NewWidgetPath,
	const TSharedPtr<SWidget>& NewWidget)
{
	// Logger.ToggleLogging(true);
	if (!OldWidget.IsValid() || !NewWidget.IsValid())
		return;

	PrevWidget = OldWidget;

	FSlateApplication& SlateApp = FSlateApplication::Get();
	// SlateApp.FindWidgetWindow();
	// SlateApp.FindMenuInWidgetPath();
	FWidgetPath WidgetPath;
	if (SlateApp.FindPathToWidget(NewWidget.ToSharedRef(), WidgetPath))
		// Logger.Print(WidgetPath.ToString(), ELogVerbosity::Verbose);

		DetectWidgetType(NewWidget.ToSharedRef());

	// Globally define rules for this LogCategory:
	// 1. Should all messages print as notification?
	// 2. Should all messages need to be ignored?
	// 3. The symbol will deduce the color of the message (screen)
	// 4.
	// Log::Print("Same widget, do not update", Sym::Info);

	// Clear if there's a current running timer as we gonna assign a newer widget
	TSharedRef<FTimerManager> TimerManager = GEditor->GetTimerManager();
	if (TimerManager->IsTimerActive(TimerHandleNewWidgetTracker))
		TimerManager->ClearTimer(TimerHandleNewWidgetTracker);

	// Because we really only care on what was the last active widget in a certain tab, maybe we can just listen to the OnActiveTabChanged and register what was the last active widget???? Big.
	TWeakPtr<SWidget> WeakNewWidget = NewWidget; // Safely store in a WeakPtr

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
			TrackActiveWindow();

			const TSharedPtr<SWidget> PinnedNewWidget = WeakNewWidget.Pin();
			if (!PinnedNewWidget.IsValid())
				return;

			if (ShouldFilterNewWidget(PinnedNewWidget.ToSharedRef()))
			{
				bHasFilteredAnIncomingNewWidget = true;
				return;
			}

			if (const TSharedPtr<SWidget> PinnedActiveWidget = ActiveWidget.Pin())
			{
				if (PinnedActiveWidget->GetId() == PinnedNewWidget->GetId())
				{
					Logger.Print("OnFocusedChanged: Same widget, skip...");
					return;
				}
			}

			Logger.Print(FString::Printf(
				TEXT("OnFocusedChanged: New widget %s, register..."),
				*PinnedNewWidget->GetType().ToString()));

			ActiveWidget = PinnedNewWidget; // New Widget found - track it.

			if (ActiveMinorTab.IsValid())
			{
				const TSharedPtr<SDockTab>& MinorTab = ActiveMinorTab.Pin();
				if (MinorTab.IsValid())
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
					Logger.Print("OnFocusChanged: No global or local Minor Tab found. Can't register...", ELogVerbosity::Error, true);
			}
		},
		// We want to be pretty quick and expressive with this delay becauase
		// the user might change to a new widget and immediately jump to a new
		// minor tab. In this case we will need to track the widget quickly before
		// jumping, and if we have a large delay, it will invalidate the old timer
		// which will discard the widget tracking for the (now) old panel.
		0.025f, false);
	// 0.1f, false); // Potentially too slow
}

// NOTE: there's an incorrect use of the delegate being called internally by UE.
// It specifies the delegate as firstly the NewActive, then OldTab. But in reality
// calls the OldTab, then NewTab. I wrote more about that in the BugNotes folder,
// but anyways, I've flipped the params to match what's actually being passed.
void FUMFocusManager::OnActiveTabChanged(
	TSharedPtr<SDockTab> PrevActiveTab, TSharedPtr<SDockTab> NewActiveTab)
{
	FTimerHandle TimerHandle;
	GEditor->GetTimerManager()->SetTimer(
		TimerHandle,
		[this, PrevActiveTab, NewActiveTab]() { DebugPrevAndNewMinorTabsMajorTabs(PrevActiveTab, NewActiveTab); },
		0.025f, false);

	TSharedRef<FTimerManager> TimerManager = GEditor->GetTimerManager();
	if (TimerManager->IsTimerActive(TimerHandleNewMinorTabTracker))
		TimerManager->ClearTimer(TimerHandleNewMinorTabTracker);

	TWeakPtr<SDockTab> WeakNewTab = NewActiveTab;

	TimerManager->SetTimer(
		TimerHandleNewMinorTabTracker,
		[this, WeakNewTab]() {
			if (bIsTabForegrounding)
				return;

			Logger.Print("OnActiveTabChanged");
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
	// i.e. Unreal will mark tabs as new active ones while we're really
	// still just foregrounding. So we need to determine if we're still
	// processing the foregrounding operation or have landed.
	// NOTE:
	// It looks like when we detach (drag-from) Major Tab and then land somewhere;
	// our focus will behave unexpectedly - focusing on the tab in the back
	// of the tab we've just dragged. This feels a bit odd. So we need to track
	// which tab we're currently dragging around so we can retain focus on it
	// manually.

	FSlateApplication& SlateApp = FSlateApplication::Get();

	// We can also potentially check to see if the foregrounding is drag by
	// looking at the pressed mouse buttons, but IsDragDropping() seems to do
	// the job and we really don't want to rely on PressedMouseButtons too much.
	// const TSet<FKey>&  PressedButtons = SlateApp.GetPressedMouseButtons();
	if (SlateApp.IsDragDropping())
	{
		// Bypass the misinformed OnActiveTabChanged in order to not assign
		// new active widgets (as we're really just dragging a tab around)
		bIsTabForegrounding = true;

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
			Logger.Print(FString::Printf(
				TEXT("OnTabForegrounded: Dragging %s"), *LogTab.ToString()));
		// else
		// 	Logger.Print("OnTabForeGrounded: Dummy Check");

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
	Logger.Print("OnTabForegrounded: Finished");

	// Now we just want to check which tab should be forwarded to set as current.
	// *if TabForegrounding flag is true; we want to pass the manually tracked tab
	// (i.e. we're probably dragging tabs around).
	if (bIsTabForegrounding && ForegroundedProcessedTab.IsValid())
		SetCurrentTab(ForegroundedProcessedTab.Pin().ToSharedRef());
	// *else we want to pass the NewActiveTab (param)
	// (i.e. we're probably cycling or directly clicking on tabs).
	else if (NewActiveTab.IsValid())
		SetCurrentTab(NewActiveTab.ToSharedRef());
	else
		Logger.Print("OnTabForegrounded: No valid new tab.",
			ELogVerbosity::Error);

	// OnActiveTabChanged should be safe to continue processing from now on
	// though it might be healthy to even delay this mark a bit (like by 25ms or
	// something) to insure we don't bring focus to a none-desired tab.
	bIsTabForegrounding = false;
	bIsDummyForegroundingCallbackCheck = false;
}

void FUMFocusManager::SetCurrentTab(const TSharedRef<SDockTab> NewTab)
{
	// LogTabParentWindow(NewTab);

	PreSetCurrentTab();

	if (NewTab->GetVisualTabRole() == ETabRole::MajorTab)
		SetActiveMajorTab(NewTab);
	else
		SetActiveMinorTab(NewTab);
}

void FUMFocusManager::PreSetCurrentTab()
{
	// if something unuseful stole our focus, we want to bring focus to our last
	// tracked meaningful widget.
	if (bBypassAutoFocusLastActiveWidget) // When navigation between panel tabs
	{
		Logger.Print("PreSetCurrentTab: BypassAutoFocusLastActiveWidget");
		bHasFilteredAnIncomingNewWidget = false;
		bBypassAutoFocusLastActiveWidget = false;
	}
	else if (bHasFilteredAnIncomingNewWidget)
	{
		Logger.Print("PreSetCurrentTab: Try focus last active widget");
		bHasFilteredAnIncomingNewWidget = false;
		TryFocusLastActiveWidget();
	}
}

void FUMFocusManager::SetActiveMajorTab(const TSharedRef<SDockTab> InMajorTab)
{
	// Check if our currently set Major Tab match the incoming new one
	if (const TSharedPtr<SDockTab>& PinnedLastMajorTab = ActiveMajorTab.Pin())
		if (PinnedLastMajorTab == InMajorTab)
			return; // They match, we can skip.

	// Set new Major Tab & activate it. Not sure if activation is needed, has it
	// should already be active by this point. TODO: Check if activation is needed
	LogTabChange(TEXT("Major"), ActiveMajorTab, InMajorTab);
	ActiveMajorTab = InMajorTab;
	InMajorTab->ActivateInParent(ETabActivationCause::SetDirectly);

	// if we have any user focus, we don't need to remap our user's focus to
	// something else. He had probably clicked and focused on something
	// manually with the mouse.
	if (!(DoesTabHaveFocus(InMajorTab)
			&& DoesActiveMinorTabResidesInMajorTab(InMajorTab)))
		FindTabWellAndActivateForegroundedTab(InMajorTab);
}

void FUMFocusManager::SetActiveMinorTab(const TSharedRef<SDockTab> InMinorTab)
{
	// Check if our currently set Minor Tab match the incoming new one
	if (const TSharedPtr<SDockTab>& PinnedLastMinorTab = ActiveMinorTab.Pin())
		if (PinnedLastMinorTab == InMinorTab)
			return; // They match, we can skip.

	// Set new Minor Tab & activate it. Not sure if activation is needed, has it
	// should already be active by this point. TODO: Check if activation is needed
	LogTabChange(TEXT("Minor"), ActiveMinorTab, InMinorTab);
	ActiveMinorTab = InMinorTab;
	InMinorTab->ActivateInParent(ETabActivationCause::SetDirectly);

	VisualizeParentDockingTabStack(InMinorTab);

	// Register this tab's TabWell with the current Active Major Tab parent.
	RegisterTabWellWithActiveMajorTab(InMinorTab);

	if (DoesTabHaveFocus(InMinorTab))
		return; // Skip if already focused (usually if user manually clicked)

	// Check if there's an associated last widget we can focus on
	// I think we should only really do that if the TabWell operation goes
	// well (TabWell ;))
	TryFocusOnLastActiveWidgetInMinorTab(InMinorTab);
}

bool FUMFocusManager::DoesTabHaveFocus(const TSharedRef<SDockTab> InTab)
{
	// Skip widget focusing if there's already any focus (e.g.
	// user had manually clicked with the mouse)
	if (InTab->GetContent()->HasAnyUserFocusOrFocusedDescendants())
	{
		Logger.Print("DoesTabHaveFocus: Minor had focus... Skipping.", ELogVerbosity::Warning);
		return true;
	}
	return false;
}

bool FUMFocusManager::VisualizeParentDockingTabStack(
	const TSharedRef<SDockTab> InTab)
{
	TWeakPtr<SWidget> DockingTabStack;
	if (FUMSlateHelpers::GetParentDockingTabStackAsWidget(
			InTab->GetContent(), DockingTabStack))
	{
		if (const auto PinDocking = DockingTabStack.Pin())
		{
			FUMFocusVisualizer::DrawDebugOutlineOnWidget(
				PinDocking.ToSharedRef());
			return true;
		}
	}
	return false;
}

bool FUMFocusManager::RegisterTabWellWithActiveMajorTab(
	const TSharedRef<SDockTab> InBaseMinorTab)
{
	if (const TSharedPtr<SDockTab> PinnedLastMajorTab = ActiveMajorTab.Pin())
	{
		if (DoesMinorTabResidesInMajorTab(
				InBaseMinorTab, PinnedLastMajorTab.ToSharedRef()))
		{
			if (const TSharedPtr<SWidget> TabWell =
					InBaseMinorTab->GetParentWidget())
			{
				LastActiveTabWellByMajorTabId.Add(
					PinnedLastMajorTab->GetId(), TabWell);
				Log::NotifySuccess(
					FText::FromString(FString::Printf(TEXT("TabWell registered with MajorTab: %s"),
						*PinnedLastMajorTab->GetTabLabel().ToString())),
					bLog);
				return true;
			}
		}
		else
			Log::NotifySuccess(
				FText::FromString("Can't register TabWell: Minor tab does not reside in Major Tab"),
				bLog);
	}
	else
		Log::NotifySuccess(
			FText::FromString("Can't register TabWell: No valid Major Tab"),
			bLog);
	return false;
}

bool FUMFocusManager::TryFocusOnLastActiveWidgetInMinorTab(
	const TSharedRef<SDockTab> InTab)
{
	FSlateApplication& SlateApp = FSlateApplication::Get();
	if (const TWeakPtr<SWidget>* LastWidget =
			LastActiveWidgetByMinorTabId.Find(InTab->GetId()))
	{
		if (const auto& PinnedLastWidget = LastWidget->Pin())
		{
			SlateApp.SetAllUserFocus(
				PinnedLastWidget.ToSharedRef(), EFocusCause::Navigation);
			Log::NotifySuccess(
				FText::FromString(FString::Printf(TEXT("Widget Found: %s"),
					*LastWidget->Pin()->GetTypeAsString())),
				bLog);
			return true;
		}
	}
	else
	{
		Log::NotifySuccess(FText::FromString("SetCurrentTab: Can't find associated valid widget."));
		//  TODO: Find something more useful?
		SlateApp.SetAllUserFocus(
			InTab->GetContent(), EFocusCause::Navigation);
	}
	return false;
}

bool FUMFocusManager::DoesActiveMinorTabResidesInMajorTab(
	const TSharedRef<SDockTab> InMajorTab)
{
	const TSharedRef<FGlobalTabmanager>& GTM = FGlobalTabmanager::Get();
	if (const TSharedPtr<SDockTab> GlobalMinorTab = GTM->GetActiveTab())
	{
		if (const TSharedPtr<FTabManager>
				TabManager = GlobalMinorTab->GetTabManagerPtr())
		{
			if (const TSharedPtr<SDockTab>& ParentMajorTab =
					GTM->GetMajorTabForTabManager(TabManager.ToSharedRef()))
			{
				if (ParentMajorTab->GetId() == InMajorTab->GetId())
				{
					Logger.Print("Active Minor Tab Resides in Major Tab");
					return true;
				}
				Logger.Print("Active Minor Tab does NOT Resides in Major Tab",
					ELogVerbosity::Warning);
				return false;
			}
		}
	}
	return false;
}

bool FUMFocusManager::FindTabWellAndActivateForegroundedTab(
	const TSharedRef<SDockTab> InMajorTab)
{
	if (const TWeakPtr<SWidget>* TabWell =
			LastActiveTabWellByMajorTabId.Find(InMajorTab->GetId()))
	{
		if (const auto& PinnedTabWell = TabWell->Pin())
			if (FChildren* Tabs = PinnedTabWell->GetChildren())
			{
				for (int32 i{ 0 }; i < Tabs->Num(); ++i)
				{
					TSharedRef<SDockTab> MinorTab =
						StaticCastSharedRef<SDockTab>(Tabs->GetChildAt(i));
					if (MinorTab->IsForeground()) // Get the latest tab in TabWell
					{
						Logger.Print(FString::Printf(TEXT("FindTabWellAndActivate: Activate Minor Tab: %s"), *MinorTab->GetTabLabel().ToString()),
							ELogVerbosity::Verbose);

						// Activate the frontmost Minor Tab in the TabWell
						MinorTab->ActivateInParent(
							ETabActivationCause::SetDirectly);

						// if the window has changed the GlobalTabmanager
						// won't automatically alert us for the change, thus
						// we need to call this manually.
						if (HasWindowChanged())
						{
							Log::NotifySuccess(
								FText::FromString("Window Changed - Manually Calling OnActiveTabChanged"), bLog);
							OnActiveTabChanged(nullptr, MinorTab);
						}
						return true;
					}
				}
			}
	}
	// It will be good to look at the different generic winodws the users may be
	// in Unreal and understand what will he probably want to be focused.
	// For example in Preferences it's the SearchBox while in Blueprint Editor
	// it's the Editor Graph, etc.
	// We can have some light rules on how we want to set the user's focus
	// if there's no specific TabWell associated with the Major Tab (first time)
	// Lookup all the TabWells and bring focus to the first one + tab in it.
	Logger.Print("FindTabWellAndActivate: No TabWell found with Active MajorTab",
		ELogVerbosity::Error, true);
	// FText::FromString("No TabWell found with Active Major Tab"));

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
	return false;
}

bool FUMFocusManager::HasWindowChanged()
{
	const auto& SlateApp = FSlateApplication::Get();

	TSharedPtr<SWindow> SysActiveWin = SlateApp.GetActiveTopLevelRegularWindow();
	if (!SysActiveWin.IsValid())
		return false;

	const auto& PinnedActiveWindow = ActiveWindow.Pin();
	if (!PinnedActiveWindow.IsValid()
		|| !SysActiveWin->GetTitle().EqualTo(PinnedActiveWindow->GetTitle()))
	{
		ActiveWindow = SysActiveWin;
		return true;
	}

	return false;
}

void FUMFocusManager::HandleOnWindowChanged(
	const TSharedPtr<SWindow> PrevWindow,
	const TSharedPtr<SWindow> NewWindow)
{
	FGlobalTabmanager::Get();

	if (NewWindow.IsValid())
	{
		Logger.Print(FString::Printf(TEXT("New Window: %s"), *NewWindow->GetTitle().ToString()), ELogVerbosity::Verbose);

		ActivateWindow(NewWindow.ToSharedRef());

		if (TryFocusFirstFoundMinorTab(NewWindow->GetContent()))
			// if (TryFocusFirstFoundMinorTab(NewWindow.ToSharedRef()))
			return;

		if (TryFocusFirstFoundSearchBox(NewWindow->GetContent()))
			// if (TryFocusFirstFoundSearchBox(NewWindow.ToSharedRef()))
			return;
		// FTimerHandle TimerHandle;
		// GEditor->GetTimerManager()->SetTimer(
		// 	TimerHandle,
		// 	[this]() {
		// 		TSharedPtr<SDockTab> FrontmostMajorTab;
		// 		if (FUMSlateHelpers::GetFrontmostForegroundedMajorTab(FrontmostMajorTab))
		// 		{
		// 			OnTabForegroundedV2(FrontmostMajorTab, nullptr);
		// 		}
		// 	},
		// 	0.5f, false);

		TSharedPtr<SDockTab> FrontmostMajorTab;
		if (FUMSlateHelpers::GetFrontmostForegroundedMajorTab(FrontmostMajorTab))
		{
			OnTabForegroundedV2(FrontmostMajorTab, nullptr);
		}
	}
}

bool FUMFocusManager::TryFocusLastActiveWidget()
{
	if (ActiveWidget.IsValid())
	{
		if (const TSharedPtr<SWidget>& Widget = ActiveWidget.Pin())
			if (!Widget->HasAnyUserFocus())
			{
				FSlateApplication::Get().SetAllUserFocus(
					Widget, EFocusCause::Navigation);
			}
		return true;
	}
	return false;
}

bool FUMFocusManager::TryGetFrontmostMajorTab()
{
	TSharedPtr<SDockTab> FrontmostMajorTab;
	if (!FUMSlateHelpers::GetFrontmostForegroundedMajorTab(FrontmostMajorTab))
		return false;
	ActiveMajorTab = FrontmostMajorTab;
	return true;
}

void FUMFocusManager::TrackActiveWindow()
{
	const TSharedPtr<SWindow> NewActiveWindow =
		FSlateApplication::Get().GetActiveTopLevelRegularWindow();
	if (NewActiveWindow.IsValid())
		ActiveWindow = NewActiveWindow;
}

bool FUMFocusManager::ShouldFilterNewWidget(TSharedRef<SWidget> InWidget)
{
	static const TSet<FName> WidgetsToFilterByType{
		"SDockingTabStack",
		"SWindow",
		/* To be (potentially) continued */
	};
	return WidgetsToFilterByType.Contains(InWidget->GetType());
}

bool FUMFocusManager::DoesMinorTabResidesInMajorTab(
	const TSharedRef<SDockTab> MinorTab, const TSharedRef<SDockTab> MajorTab)
{
	const TSharedRef<FGlobalTabmanager>& GTM = FGlobalTabmanager::Get();

	if (const TSharedPtr<FTabManager>
			TabManager = MinorTab->GetTabManagerPtr())

		if (const TSharedPtr<SDockTab>& ParentMajorTab =
				GTM->GetMajorTabForTabManager(TabManager.ToSharedRef()))

			if (ParentMajorTab->GetId() == MajorTab->GetId())
				return true;

	Logger.Print(
		FString::Printf(TEXT("Minor Tab %s does not reside in Major Tab %s"),
			*MinorTab->GetTabLabel().ToString(),
			*MajorTab->GetTabLabel().ToString()));

	return false;
}

bool FUMFocusManager::RemoveActiveMajorTab()
{
	static const FString LevelEditorType{ "LevelEditor" };

	if (const TSharedPtr<SDockTab> MajorTab =
			FUMSlateHelpers::GetActiveMajorTab())
		// Removing the LevelEditor will cause closing the entire editor which is
		// a bit too much.
		if (!MajorTab->GetLayoutIdentifier().ToString().Equals(LevelEditorType))
		{
			Logger.Print(FString::Printf(TEXT("Try remove Major Tab: %s"), *MajorTab->GetTabLabel().ToString()), ELogVerbosity::Verbose, true);
			MajorTab->RemoveTabFromParent();
			FocusNextFrontmostWindow();
			return true;
		}

	// Fallback to the active Minor Tab
	// if (const TSharedPtr<SDockTab> MinorTab =
	// FUMSlateHelpers::GetActiveMinorTab())
	// {
	// 	Logger.Print(FString::Printf(TEXT("Try remove Minor Tab: %s"),
	// 					 *MinorTab->GetTabLabel().ToString()),
	// 		ELogVerbosity::Verbose, true);
	// 	MinorTab->RemoveTabFromParent();
	// 	FocusNextFrontmostWindow();
	// 	return true;
	// }

	Logger.Print("Can't find any non-level-editor Major Tabs to remove",
		ELogVerbosity::Verbose, true);
	return false;
}

bool FUMFocusManager::FocusNextFrontmostWindow()
{
	FSlateApplication&			SlateApp = FSlateApplication::Get();
	TArray<TSharedRef<SWindow>> Wins;

	SlateApp.GetAllVisibleWindowsOrdered(Wins);
	for (int32 i{ Wins.Num() - 1 }; i > 0; --i)
	{
		if (!Wins[i]->GetTitle().IsEmpty())
		{
			ActivateWindow(Wins[i]);
			return true; // Found the next window
		}
	}

	// If no windows were found; try to bring focus to the root window
	if (TSharedPtr<SWindow> RootWin = FGlobalTabmanager::Get()->GetRootWindow())
	{
		ActivateWindow(RootWin.ToSharedRef());
		return true;
	}
	return false;
}

// NOTE: Kept here some commented methods for future reference.
void FUMFocusManager::ActivateWindow(const TSharedRef<SWindow> Window)
{
	FSlateApplication& SlateApp = FSlateApplication::Get();
	Window->BringToFront(true);
	TSharedRef<SWidget>			   WinContent = Window->GetContent();
	FWindowDrawAttentionParameters DrawParams(
		EWindowDrawAttentionRequestType::UntilActivated);
	// Window->DrawAttention(DrawParams);
	// Window->ShowWindow();
	// Window->FlashWindow(); // Cool way to visually indicate activated wins!

	// I was a bit worried about this, but actually it seems that without this
	// we will have a weird focusing bug. So this actually seems to work pretty
	// well.
	SlateApp.ClearAllUserFocus();

	// TODO:
	// NOTE:
	// This is really interesting. It may help to soildfy focus and what we
	// actually want! Like pass in the MajorTab->MinorTab->*Widget*
	// I'm thinking maybe something like find major tab in window function ->
	// Then we have the major, we can do the check, get the minor, get the widget
	// and pass it in **before drawing attention**!
	// Window->SetWidgetToFocusOnActivate();

	// This will focus the window content, which isn't really useful. And it
	// looks like ->DrawAttention() Seems to do a better job!
	// SlateApp.SetAllUserFocus(
	// 	WinContent, EFocusCause::Navigation);

	Window->DrawAttention(DrawParams); // Seems to work well!

	FocusManager->Logger.Print(FString::Printf(
		TEXT("Activating Window: %s"), *Window->GetTitle().ToString()));
}

void FUMFocusManager::ActivateNewInvokedTab(
	FSlateApplication& SlateApp, const TSharedPtr<SDockTab> NewTab)
{
	SlateApp.ClearAllUserFocus(); // NOTE: In order to actually draw focus

	if (!NewTab.IsValid())
		return;

	// TArray<TSharedRef<SWindow>> Wins;
	// SlateApp.GetAllVisibleWindowsOrdered(Wins);
	// if (Wins.IsEmpty())
	// 	return;
	// FUMFocusManager::ActivateWindow(Wins.Last());

	if (TSharedPtr<SWindow> Win = NewTab->GetParentWindow())
		FUMFocusManager::ActivateWindow(Win.ToSharedRef());

	FocusManager->ActivateTab(NewTab.ToSharedRef());
}

void FUMFocusManager::LogTabChange(const FString& TabType, const TWeakPtr<SDockTab>& CurrentTab, const TSharedRef<SDockTab>& NewTab)
{
	FString NewTabLog = FString::Printf(TEXT("New: %s (ID: %d)\n"),
		*NewTab->GetTabLabel().ToString(), NewTab->GetId());

	FString PrevTabLog = CurrentTab.IsValid()
		? FString::Printf(TEXT("Previous: %s (ID: %d)\n"), *CurrentTab.Pin()->GetTabLabel().ToString(), CurrentTab.Pin()->GetId())
		: TEXT("None");

	const FString LogStr = FString::Printf(
		TEXT("Set New %s Tab:\n%s%s"), *TabType, *NewTabLog, *PrevTabLog);

	Logger.Print(LogStr, ELogVerbosity::Verbose);
}

void FUMFocusManager::LogTabParentWindow(const TSharedRef<SDockTab> InTab)
{
	const TSharedPtr<SWindow> TabParentWindow = InTab->GetParentWindow();
	if (!TabParentWindow.IsValid())
	{
		Logger.Print("SetCurrentTab: Invalid Tab Window", ELogVerbosity::Error);
	}
	else
	{
		Logger.Print(FString::Printf(
			TEXT("SetCurrentTab: Tab=%s, Window=%s"),
			*InTab->GetTabLabel().ToString(),
			*TabParentWindow->GetTitle().ToString()));
	}
}

void FUMFocusManager::ToggleVisualLog(bool bIsVisualLog)
{
	bLog = bIsVisualLog;
}

void FUMFocusManager::DebugGlobalTabManagerTracking()
{
	FSlateApplication& SlateApp = FSlateApplication::Get();
	if (const TSharedPtr<SWidget> FocusedWidget =
			SlateApp.GetUserFocusedWidget(0))
	{
		TSharedRef<FGlobalTabmanager>
			GTM = FGlobalTabmanager::Get();
		// GTM->GetTabManagerForMajorTab();  // Potentially interesting Minor Tabs
		// GTM->DrawAttentionToTabManager(); // Triggers Minor Tabs?!

		if (const TSharedPtr<SDockTab> CurrMinorTab = GTM->GetActiveTab())
		{
			if (const TSharedPtr<FTabManager> TabManager =
					CurrMinorTab->GetTabManagerPtr())
			{
				if (const TSharedPtr<SDockTab> MajorTab =
						GTM->GetMajorTabForTabManager(TabManager.ToSharedRef()))
				{
					Logger.Print(
						FString::Printf(TEXT("Widget Name: %s | Minor Tab: %s | Major Tab: %s"),
							*FocusedWidget->GetTypeAsString(),
							*CurrMinorTab->GetTabLabel().ToString(),
							*MajorTab->GetTabLabel().ToString()),
						ELogVerbosity::Verbose, true);
				}
			}
		}
	}
}

void FUMFocusManager::DebugPrevAndNewMinorTabsMajorTabs(
	TSharedPtr<SDockTab> PrevActiveTab, TSharedPtr<SDockTab> NewActiveTab)
{
	TSharedRef<FGlobalTabmanager>
		GTM = FGlobalTabmanager::Get();

	const FString DefaultLog = "was Destroyed";

	// Handle Previous Tab:
	FString LogPrevMinorTab = DefaultLog;
	FString LogPrevMajorTab = DefaultLog;
	FString LogPrevWidget = DefaultLog;

	if (PrevActiveTab.IsValid())
	{
		LogPrevMinorTab = PrevActiveTab->GetTabLabel().ToString();

		if (const TSharedPtr<FTabManager> PrevActiveTabManager =
				PrevActiveTab->GetTabManagerPtr())
		{
			if (const TSharedPtr<SDockTab> PrevMajorActiveTab =
					GTM->GetMajorTabForTabManager(
						PrevActiveTabManager.ToSharedRef()))
			{
				LogPrevMajorTab = PrevMajorActiveTab->GetTabLabel().ToString();
			}
		}
	}
	if (const TSharedPtr<SWidget> PrevActiveWidget = PrevWidget.Pin())
	{
		LogPrevWidget = PrevActiveWidget->GetTypeAsString();
	}

	// Handle New Tab:
	FString LogNewMinorTab = DefaultLog;
	FString LogNewMajorTab = DefaultLog;
	FString LogNewWidget = DefaultLog;

	if (NewActiveTab.IsValid())
	{
		LogNewMinorTab = NewActiveTab->GetTabLabel().ToString();

		if (const TSharedPtr<FTabManager> NewActiveTabManager =
				NewActiveTab->GetTabManagerPtr())
		{
			if (const TSharedPtr<SDockTab> NewMajorActiveTab =
					GTM->GetMajorTabForTabManager(
						NewActiveTabManager.ToSharedRef()))
			{
				LogNewMajorTab = NewMajorActiveTab->GetTabLabel().ToString();
			}
		}
	}
	FSlateApplication& SlateApp = FSlateApplication::Get();
	if (const TSharedPtr<SWidget> CurrWidget = SlateApp.GetUserFocusedWidget(0))
	{
		LogNewWidget = CurrWidget->GetTypeAsString();
	}

	const FString LogPrevious =
		FString::Printf(
			TEXT("Prev Minor Tab: %s | Prev Major Tab: %s | Prev Widget: %s"),
			*LogPrevMinorTab,
			*LogPrevMajorTab,
			*LogPrevWidget);

	const FString LogNew =
		FString::Printf(
			TEXT("New Minor Tab: %s | New Major Tab: %s | New Widget: %s"),
			*LogNewMinorTab,
			*LogNewMajorTab,
			*LogNewWidget);

	Logger.Print((LogPrevious + '\n' + LogNew), ELogVerbosity::Verbose, true);
}

void FUMFocusManager::OnFocusChangedV2(const FFocusEvent& FocusEvent, const FWeakWidgetPath& OldWidgetPath,
	const TSharedPtr<SWidget>& OldWidget, const FWidgetPath& NewWidgetPath,
	const TSharedPtr<SWidget>& NewWidget)
{
	const FString LogFunc = "OnFocusChanged:\n";
	FString		  LogOldWidget = "Invalid";
	FString		  LogNewWidget = "Invalid";

	if (OldWidget.IsValid() && !ShouldFilterNewWidget(OldWidget.ToSharedRef()))
	{
		PrevWidget = OldWidget;
		LogOldWidget = OldWidget->GetTypeAsString();
	}

	if (NewWidget.IsValid() && !ShouldFilterNewWidget(NewWidget.ToSharedRef()))
	{
		ActiveWidget = NewWidget;
		RecordWidgetUse(NewWidget.ToSharedRef());
		LogNewWidget = NewWidget->GetTypeAsString();
	}

	Logger.Print(
		LogFunc + FString::Printf(TEXT("Old Widget: %s\nNew Widget: %s"), *LogOldWidget, *LogNewWidget),
		ELogVerbosity::Verbose, bVisLogTabFocusFlow);
}

void FUMFocusManager::OnActiveTabChangedV2(
	TSharedPtr<SDockTab> PrevActiveTab, TSharedPtr<SDockTab> NewActiveTab)
{
	const FString LogFunc = "OnActiveTabChanged:\n";
	// Logger.Print(LogFunc, ELogVerbosity::Verbose, true);

	if (!GEditor->IsValidLowLevelFast() || !GEditor->IsTimerManagerValid())
		return;

	const TWeakPtr<SDockTab> WeakPrevActiveTab = PrevActiveTab;
	const TWeakPtr<SDockTab> WeakNewActiveTab = NewActiveTab;

	TSharedRef<FTimerManager> TimerManager = GEditor->GetTimerManager();
	TimerManager->ClearTimer(TimerHandle_OnActiveTabChanged);

	TimerManager->SetTimer(
		TimerHandle_OnActiveTabChanged,
		[this, WeakPrevActiveTab, WeakNewActiveTab]() {
			if (const auto PrevActiveTab = WeakPrevActiveTab.Pin())
			{
				// NOTE:
				// Intuitively we might think we need to register the Prev
				// Widget with the PrevActiveTab, but the flow of actions
				// unfolds by firstly making the tab switch focus, then the
				// widgets. This happens because when we activate a tab, our
				// OnFocusChanged event won't acatually trigger! That means
				// our ActiveWidget is still associated and lives inside the
				// previously active tab, and should be registered with him.
				// NOTE:
				// I'm not sure actually (LOL), perhaps it depends on if we
				// go through the foregrounding or not? Anyway, we can pass both
				// Prev & Active widgets for safety, I don't think there's any
				// harm in doing so.
				// if (!TryRegisterWidgetWithTab(
				// 		ActiveWidget, PrevActiveTab.ToSharedRef()))
				// 	TryRegisterWidgetWithTab(
				// 		PrevWidget, PrevActiveTab.ToSharedRef());
				TryRegisterWidgetWithTab(PrevActiveTab.ToSharedRef());
			}

			if (const auto NewActiveTab = WeakNewActiveTab.Pin())
			{
				const auto NewTabRef = NewActiveTab.ToSharedRef();
				TryRegisterMinorWithParentMajorTab(NewTabRef);
				TryActivateLastWidgetInTab(NewTabRef);
				VisualizeParentDockingTabStack(NewTabRef);
			}
		},
		0.025,
		false);
}

void FUMFocusManager::OnTabForegroundedV2(
	TSharedPtr<SDockTab> NewActiveTab, TSharedPtr<SDockTab> PrevActiveTab)
{
	const FString LogFunc = "OnTabForegrounded:\n";
	// Logger.Print(LogFunc, ELogVerbosity::Verbose, true);
	// DebugPrevAndNewMinorTabsMajorTabs(NewActiveTab, PrevActiveTab);

	if (NewActiveTab.IsValid())
	{
		if (NewActiveTab->GetVisualTabRole() == ETabRole::MajorTab)
		{
			const TSharedRef<SDockTab> TabRef = NewActiveTab.ToSharedRef();
			// 1. Check if there's a registered Minor Tab to this Major Tab.
			if (TryFocusLastActiveMinorForMajorTab(TabRef))
				return;

			TSharedRef<SWidget> TabContent = TabRef->GetContent();
			// 2. Try to focus on the first Minor Tab (if it exists).
			if (TryFocusFirstFoundMinorTab(TabContent))
				return;

			// 3. Try to focus on the first SearchBox (if it exists).
			if (TryFocusFirstFoundSearchBox(TabContent))
				return;

			// 4. Focus on the Tab's Content (default fallback).
			FocusTabContent(TabRef);
		}
		else // Minor Tab
		{
		}
	}
}

bool FUMFocusManager::TryFocusLastActiveMinorForMajorTab(
	TSharedRef<SDockTab> InMajorTab)
{
	if (const TWeakPtr<SDockTab>* LastActiveMinorTabPtr =
			LastActiveMinorTabByMajorTabId.Find(InMajorTab->GetId()))
	{
		if (const TSharedPtr<SDockTab> LastActiveMinorTab =
				LastActiveMinorTabPtr->Pin())
		{
			ActivateTab(LastActiveMinorTab.ToSharedRef());
			Logger.Print(FString::Printf(TEXT("Last Active Minor Tab found: %s"),
							 *LastActiveMinorTab->GetTabLabel().ToString()),
				ELogVerbosity::Verbose, bVisLogTabFocusFlow);

			TryActivateLastWidgetInTab(LastActiveMinorTab.ToSharedRef());
			return true;
		}
	}
	return false;
}

bool FUMFocusManager::TryFocusFirstFoundMinorTab(TSharedRef<SWidget> InContent)
{
	TWeakPtr<SWidget> FirstMinorTabAsWidget;
	if (FUMSlateHelpers::TraverseWidgetTree(
			InContent, FirstMinorTabAsWidget, "SDockTab"))
	{
		const TSharedPtr<SDockTab> FirstMinorTab =
			StaticCastSharedPtr<SDockTab>(FirstMinorTabAsWidget.Pin());

		// TODO:
		// Test without activating the tab itself?
		ActivateTab(FirstMinorTab.ToSharedRef());
		Logger.Print("Minor Tab Found: Fallback to focus first Minor Tab.",
			ELogVerbosity::Warning, bVisLogTabFocusFlow);

		TryActivateLastWidgetInTab(FirstMinorTab.ToSharedRef());
		return true;
	}
	return false;
}

bool FUMFocusManager::TryFocusFirstFoundSearchBox(
	TSharedRef<SWidget> InContent)
{
	// FSlateApplication& SlateApp = FSlateApplication::Get();
	TWeakPtr<SWidget> EditableTextAsWidget;
	if (FUMSlateHelpers::TraverseWidgetTree(
			// NewActiveTab->GetContent(), SearchBoxAsWidget, "SSearchBox"))
			InContent, EditableTextAsWidget, "SEditableText"))
	{
		// SlateApp.ClearAllUserFocus();
		// SlateApp.SetAllUserFocus(EditableTextAsWidget.Pin(),
		// 	EFocusCause::Navigation);

		FTimerHandle TimerHandle;
		GEditor->GetTimerManager()->SetTimer(
			TimerHandle,
			[EditableTextAsWidget]() {
				if (const auto Editable = EditableTextAsWidget.Pin())
				{
					FSlateApplication& SlateApp = FSlateApplication::Get();
					SlateApp.ClearAllUserFocus();
					SlateApp.SetAllUserFocus(Editable,
						EFocusCause::Navigation);
				}
			},
			0.1f, false);

		Logger.Print("Fallback to first EditableText.",
			ELogVerbosity::Warning, true);
		return true;
	}
	return false;
}

void FUMFocusManager::FocusTabContent(TSharedRef<SDockTab> InTab)
{
	FSlateApplication& SlateApp = FSlateApplication::Get();
	// SlateApp.ClearAllUserFocus();
	SlateApp.SetAllUserFocus(InTab->GetContent(),
		EFocusCause::Navigation);

	Logger.Print("Fallback to Tab's Content.",
		ELogVerbosity::Warning, bVisLogTabFocusFlow);
}

bool FUMFocusManager::TryRegisterMinorWithParentMajorTab(
	const TSharedRef<SDockTab> InMinorTab)
{
	TSharedRef<FGlobalTabmanager> GTM = FGlobalTabmanager::Get();

	// Check if has a valid Tab Manager
	if (const TSharedPtr<FTabManager> MinorTabManager =
			InMinorTab->GetTabManagerPtr())
	{
		// Check if has a valid Major Tab parent
		if (const TSharedPtr<SDockTab> ParentMajorTab =
				GTM->GetMajorTabForTabManager(MinorTabManager.ToSharedRef()))
		{
			// Associate this Minor Tab as the last active for this Major Tab
			LastActiveMinorTabByMajorTabId.Add(
				ParentMajorTab->GetId(), InMinorTab);

			Logger.Print(
				FString::Printf(
					TEXT("Registered Minor Tab: %s with Major Tab: %s"),
					*InMinorTab->GetTabLabel().ToString(),
					*ParentMajorTab->GetTabLabel().ToString()),
				ELogVerbosity::Verbose, bVisLogTabFocusFlow);
			return true;
		}
	}
	return false;
}

bool FUMFocusManager::TryRegisterWidgetWithTab(
	const TWeakPtr<SWidget> InWidget, const TSharedRef<SDockTab> InTab)
{
	if (const TSharedPtr<SWidget> Widget = InWidget.Pin())
	{
		if (FUMSlateHelpers::DoesWidgetResideInTab(InTab, Widget.ToSharedRef()))
		{
			LastActiveWidgetByTabId.Add(InTab->GetId(), InWidget);
			// TODO: Debug this
			Logger.Print(FString::Printf(
				TEXT("Registered Widget: %s with Tab: %s"),
				*Widget->GetTypeAsString(),
				*InTab->GetTabLabel().ToString()));
			return bVisLogTabFocusFlow;
		}
	}
	return false;
}

bool FUMFocusManager::TryRegisterWidgetWithTab(const TSharedRef<SDockTab> InTab)
{
	for (const TWeakPtr<SWidget>& WidgetWeakPtr : RecentlyUsedWidgets)
	{
		if (const TSharedPtr<SWidget> WidgetPtr = WidgetWeakPtr.Pin())
		{
			// if found the most recent widget that lives in the new tab
			if (FUMSlateHelpers::DoesWidgetResideInTab(InTab, WidgetPtr.ToSharedRef()))
			{
				LastActiveWidgetByTabId.Add(InTab->GetId(), WidgetWeakPtr);

				Logger.Print(
					FString::Printf(
						TEXT("Most recent widget in tab '%s' is '%s'"),
						*InTab->GetTabLabel().ToString(),
						*WidgetPtr->GetTypeAsString()),
					ELogVerbosity::Verbose, bVisLogTabFocusFlow);
				return true;
			}
		}
	}
	Logger.Print(FString::Printf(
					 TEXT("Could NOT register any recent Widget with Tab: %s"),
					 *InTab->GetTabLabel().ToString()),
		ELogVerbosity::Error, bVisLogTabFocusFlow);
	return false;
}

bool FUMFocusManager::TryActivateLastWidgetInTab(
	const TSharedRef<SDockTab> InTab)
{
	// return false; // NOTE: TEST

	// if this tab has any focus, it means the user had probably clicked on
	// it manually via the mouse or something.
	if (InTab->GetContent()->HasFocusedDescendants())
	{
		TryRegisterWidgetWithTab(ActiveWidget, InTab);
		return true;
	}

	FSlateApplication& SlateApp = FSlateApplication::Get();
	// Clearing before drawing focus seems to solidfy the end focus result,
	// especially for things like SGraphPanel allowing solid focus on Nodes.
	SlateApp.ClearAllUserFocus();
	TSharedRef<FTimerManager> TimerManager = GEditor->GetTimerManager();
	// We wanna clear previous timer to mitigate a potential loop
	TimerManager->ClearTimer(TimerHandle_FocusTabContentFallback);

	// Try find a registered last active widget
	if (TWeakPtr<SWidget>* FoundWidget =
			LastActiveWidgetByTabId.Find(InTab->GetId()))
	{
		if (const TSharedPtr<SWidget> Widget = FoundWidget->Pin())
		{
			SlateApp.SetAllUserFocus(Widget, EFocusCause::Navigation);
			Logger.Print(FString::Printf(TEXT("Found Widget: %s in Tab: %s"),
							 *Widget->GetTypeAsString(),
							 *InTab->GetTabLabel().ToString()),
				ELogVerbosity::Verbose, bVisLogTabFocusFlow);

			return true;
		}
	}
	TWeakPtr<SDockTab> WeakInTab = InTab; // Store as weak for safety

	TimerManager->SetTimer(
		TimerHandle_FocusTabContentFallback,
		[this, WeakInTab]() {
			if (const auto& InTab = WeakInTab.Pin())
			{
				FSlateApplication::Get().SetAllUserFocus(
					InTab->GetContent(), EFocusCause::Navigation);
				Logger.Print("Widget not found in Tab. Fallback focus Tab's Content", ELogVerbosity::Warning, bVisLogTabFocusFlow);
			}
		},
		0.2f, false);

	return false; // I guess return false if it's a fallback?
}

void FUMFocusManager::ActivateTab(const TSharedRef<SDockTab> InTab)
{
	InTab->ActivateInParent(ETabActivationCause::UserClickedOnTab);
	InTab->DrawAttention();
	// FGlobalTabmanager::Get()->SetActiveTab(InTab);
}

void FUMFocusManager::RecordWidgetUse(TSharedRef<SWidget> InWidget)
{
	// Convert to TWeakPtr so we don’t keep it alive forever
	TWeakPtr<SWidget> WeakWidget = InWidget;

	// Iterate through the array
	for (int32 i = 0; i < RecentlyUsedWidgets.Num();)
	{
		if (const auto& Widget = RecentlyUsedWidgets[i].Pin())
		{
			// Check if this is the widget we want to promote
			if (InWidget->GetId() == Widget->GetId())
			{
				// Remove it from the current position and reinsert at the front
				RecentlyUsedWidgets.RemoveAt(i);
				RecentlyUsedWidgets.Insert(WeakWidget, 0);
				return; // Done, so we can exit early
			}
			else
			{
				// Move to the next entry
				++i;
			}
		}
		else
		{
			// Remove invalid widget (stale reference)
			RecentlyUsedWidgets.RemoveAt(i);
			// Do not increment i because the array has shifted
		}
	}

	// If we didn’t find the widget, add it to the front
	RecentlyUsedWidgets.Insert(WeakWidget, 0);

	// Enforce size limit
	const int32 MaxSize = 10; // or whatever size you want
	if (RecentlyUsedWidgets.Num() > MaxSize)
	{
		RecentlyUsedWidgets.SetNum(MaxSize);
	}
}

FString FUMFocusManager::TabRoleToString(ETabRole InTabRole)
{
	switch (InTabRole)
	{
		case ETabRole::MajorTab:
			return "MajorTab";
		case ETabRole::PanelTab:
			return "PanelTab";
		case ETabRole::NomadTab:
			return "NomadTab";
		case ETabRole::DocumentTab:
			return "DocumentTab";
		case ETabRole::NumRoles:
			return "NumRoles";
		default:
			return "Unknown";
	}
}

void FUMFocusManager::OnWindowBeingDestroyed(const SWindow& Window)
{
	if (Window.IsRegularWindow())
	{
		// NOTE:
		// We want to have a small delay to only start searching after the window
		// is completey destroyed. Otherwise we will still catch that same winodw
		// that is being destroyed.
		FTimerHandle TimerHandle;
		GEditor->GetTimerManager()->SetTimer(
			TimerHandle,
			[this]() {
				TSharedPtr<SDockTab>
					NewMajorTab;
				if (FUMSlateHelpers::GetFrontmostForegroundedMajorTab(NewMajorTab))
				{
					NewMajorTab->ActivateInParent(ETabActivationCause::UserClickedOnTab);
					OnTabForegroundedV2(NewMajorTab, nullptr);
				}
			},
			0.1f, false);
	}
	// Logger.Print("Window being destroyed is not Regular");
}
