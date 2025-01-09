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

using Log = FUMLogger;
using Sym = EUMLogSymbol;

const TSharedPtr<FUMFocusManager> FUMFocusManager::FocusManager =
	MakeShared<FUMFocusManager>();

FUMFocusManager::FUMFocusManager()
	: Logger(&LogUMFocusManager)
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

	FUMWindowsNavigationManager::Get().OnWindowChanged.AddSP(
		FocusManager.ToSharedRef(), &FUMFocusManager::HandleOnWindowChanged);
}

void FUMFocusManager::OnFocusChanged(const FFocusEvent& FocusEvent, const FWeakWidgetPath& OldWidgetPath,
	const TSharedPtr<SWidget>& OldWidget, const FWidgetPath& NewWidgetPath,
	const TSharedPtr<SWidget>& NewWidget)
{
	// Logger.ToggleLogging(true);
	Logger.Print("OnFocusChanged");

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
			// Log::NotifySuccess(
			// 	FText::FromString("Focus Manager On Focused Changed"), bLog);

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
	if (bIsTabForegrounding && ForegroundedProcessedTab.IsValid()
		&& ForegroundedProcessedTab.Pin().IsValid())
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
		FUMFocusVisualizer::DrawDebugOutlineOnWidget(
			DockingTabStack.Pin().ToSharedRef());
		return true;
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

void FUMFocusManager::HandleOnWindowChanged(const TSharedPtr<SWindow> NewWindow)
{
	if (NewWindow.IsValid())
	{
		// Log::NotifySuccess(FText::FromString("New Window"), bLog);
		Log::NotifySuccess(FText::FromString("New Window"));
		// TryFocusLastActiveWidget();
		TSharedPtr<SDockTab> FrontmostMajorTab;
		// TODO: Debug what this spits when not drawing focus actively to the win
		if (FUMSlateHelpers::GetFrontmostForegroundedMajorTab(FrontmostMajorTab))
		{
			// TODO:
			// Check if for some magical reason it now will draw and call the Global
			// Tab Manager delegate correctly.
			//
			// FSlateApplication::Get().ClearAllUserFocus();
			// FSlateApplication::Get().ClearKeyboardFocus();
			if (const auto& TabWell =
					LastActiveTabWellByMajorTabId.Find(
						FrontmostMajorTab->GetId()))
			{
				if (const auto& PinTabWell = TabWell->Pin())
				{
					if (const auto& MinorTab =
							LastActiveMinorTabByTabWellId.Find(
								PinTabWell->GetId()))
					{
						if (const auto& PinMinorTab = MinorTab->Pin())
						{
							PinMinorTab->ActivateInParent(
								ETabActivationCause::UserClickedOnTab);
							Log::NotifySuccess(
								FText::FromString(FString::Printf(TEXT("New Window: Found Minor Tab - %s"),
									*PinMinorTab->GetTabLabel().ToString())));
							return;
						}
						Log::NotifySuccess(
							FText::FromString(FString::Printf(TEXT("Cannot find MinorTab '%s' by TabWell ID. Major Tab: %s"),
								*MinorTab->Pin()->GetTabLabel().ToString(),
								*FrontmostMajorTab->GetTabLabel().ToString())));
					}
				}
			}
			else
			{
				Log::NotifySuccess(
					FText::FromString("Cannot find TabWell by Major Tab ID"));
				// Find TabWells and focus on the first one!
			}

			// FrontmostMajorTab->DrawAttention();
			// Log::NotifySuccess(
			// 	FText::FromString(FString::Printf(TEXT("New Window: Found Frontmost Major Tab - %s"),
			// 		*FrontmostMajorTab->GetTabLabel().ToString())));
			// FrontmostMajorTab->ActivateInParent(ETabActivationCause::SetDirectly);
			// FGlobalTabmanager::Get()->SetActiveTab(FrontmostMajorTab);
			// FGlobalTabmanager::Get()->DrawAttention(FrontmostMajorTab.ToSharedRef());
			// FGlobalTabmanager::Get()->TryInvokeTab(FrontmostMajorTab->GetLayoutIdentifier());
			// FGlobalTabmanager::Get()->DrawAttentionToTabManager(FrontmostMajorTab->GetTabManagerPtr().ToSharedRef());

			// FrontmostMajorTab->ActivateInParent(ETabActivationCause::SetDirectly);

			// Latest production
			// OnActiveTabChanged(nullptr, FrontmostMajorTab);
			// Log::NotifySuccess(
			// 	FText::FromString(
			// 		FString::Printf(TEXT("New Window: New Focused Major - %s"),
			// 			*FrontmostMajorTab->GetTabLabel().ToString())),
			// 	bLog);
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

	if (const TSharedPtr<SDockTab>& MajorTab = FocusManager->ActiveMajorTab.Pin())
		// Removing the LevelEditor will cause closing the entire editor which is
		// a bit too much.
		if (!MajorTab->GetLayoutIdentifier().ToString().Equals(LevelEditorType))
		{
			MajorTab->RemoveTabFromParent();
			return true;
		}
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

	// TODO: For some reason it looks like there's still a bug where we allow
	// association of widgets with panel tabs that aren't their actual panel!
	// So we need to have greater safeguards on this.

	// This will focus the window content, which isn't really useful. And it
	// looks like ->DrawAttention() Seems to do a better job!
	// SlateApp.SetAllUserFocus(
	// 	WinContent, EFocusCause::Navigation);

	Window->DrawAttention(DrawParams); // Seems to work well!

	Log::AddDebugMessage(FString::Printf(
		TEXT("Activating Window: %s"), *Window->GetTitle().ToString()));
	FocusManager->HandleOnWindowChanged(Window);
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
