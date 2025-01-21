#include "UMFocuserEditorSubsystem.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Input/Events.h"
#include "Logging/LogVerbosity.h"
#include "UMLogger.h"
#include "Editor.h"
#include "Widgets/Docking/SDockTab.h"
#include "UMSlateHelpers.h"
#include "UMWindowNavigatorEditorSubsystem.h"
#include "UMFocusVisualizer.h"
#include "UMConfig.h"

// DEFINE_LOG_CATEGORY_STATIC(UMFocuserEditorSubsystem, NoLogging, All); // Prod
DEFINE_LOG_CATEGORY_STATIC(UMFocuserEditorSubsystem, Log, All); // Dev

bool UUMFocuserEditorSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	TSharedRef<FUMConfig> Config = FUMConfig::Get();

	// If Vim is enabled, we must enable the Focuser to also exist
	return Config->IsFocuserEnabled() || Config->IsVimEnabled();
}

void UUMFocuserEditorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Logger = FUMLogger(&UMFocuserEditorSubsystem);

	FCoreDelegates::OnPostEngineInit.AddUObject(
		this, &UUMFocuserEditorSubsystem::RegisterSlateEvents);

	Super::Initialize(Collection);
}

void UUMFocuserEditorSubsystem::Deinitialize()
{
	TSharedRef<FGlobalTabmanager> GTM = FGlobalTabmanager::Get();
	GTM->OnActiveTabChanged_Unsubscribe(DelegateHandle_OnActiveTabChanged);
	GTM->OnTabForegrounded_Unsubscribe(DelegateHandle_OnTabForegrounded);

	if (GEditor && GEditor->IsTimerManagerValid())
		GEditor->GetTimerManager()->ClearAllTimersForObject(this);

	Super::Deinitialize();
}

void UUMFocuserEditorSubsystem::RegisterSlateEvents()
{
	FSlateApplication& SlateApp = FSlateApplication::Get();

	SlateApp.OnFocusChanging()
		.AddUObject(this, &UUMFocuserEditorSubsystem::OnFocusChanged);

	SlateApp.OnWindowBeingDestroyed().AddUObject(
		this, &UUMFocuserEditorSubsystem::OnWindowBeingDestroyed);

	////////////////////////////////////////////////////////////////////////
	//						~ Tabs Delegates ~							  //
	//																	  //
	TSharedRef<FGlobalTabmanager> GTM = FGlobalTabmanager::Get();

	DelegateHandle_OnActiveTabChanged = GTM->OnActiveTabChanged_Subscribe(
		FOnActiveTabChanged::FDelegate::CreateUObject(
			this, &UUMFocuserEditorSubsystem::OnActiveTabChanged));

	// This seems to trigger in some cases where OnActiveTabChanged won't.
	// Keeping this as a double check.
	DelegateHandle_OnTabForegrounded = GTM->OnTabForegrounded_Subscribe(
		FOnActiveTabChanged::FDelegate::CreateUObject(
			this, &UUMFocuserEditorSubsystem::OnTabForegrounded));
	//																	  //
	//						~ Tabs Delegates ~							  //
	////////////////////////////////////////////////////////////////////////

	// Listen to OnWindowChanged from the Window Navigator
	// (if it was enabled in the config file):
	if (const UUMWindowNavigatorEditorSubsystem* WindowNavigator =
			GEditor->GetEditorSubsystem<UUMWindowNavigatorEditorSubsystem>())
	{
		WindowNavigator->OnWindowChanged.AddUObject(
			this, &UUMFocuserEditorSubsystem::HandleOnWindowChanged);
	}
}

void UUMFocuserEditorSubsystem::CheckWindowChanged()
{
	TSharedPtr<SWindow>		  OptNewWindow;
	const TSharedPtr<SWindow> TrackedWindow = TrackedActiveWindow.Pin();
	if (FUMSlateHelpers::CheckReplaceIfWindowChanged(
			TrackedWindow, OptNewWindow))
	{
		HandleOnWindowChanged(TrackedWindow, OptNewWindow);
		TrackedActiveWindow = OptNewWindow;

		// Logger.Print("Window Changed", ELogVerbosity::Log, true);
	}
}

void UUMFocuserEditorSubsystem::OnFocusChanged(const FFocusEvent& FocusEvent, const FWeakWidgetPath& OldWidgetPath,
	const TSharedPtr<SWidget>& OldWidget, const FWidgetPath& NewWidgetPath,
	const TSharedPtr<SWidget>& NewWidget)
{
	CheckWindowChanged();

	const FString LogFunc = "OnFocusChanged:\n";
	FString		  LogOldWidget = "Invalid";
	FString		  LogNewWidget = "Invalid";

	if (OldWidget.IsValid() && !ShouldFilterNewWidget(OldWidget.ToSharedRef()))
	{
		TrackedPrevWidget = OldWidget;
		LogOldWidget = OldWidget->GetTypeAsString();
	}

	if (NewWidget.IsValid() && !ShouldFilterNewWidget(NewWidget.ToSharedRef()))
	{
		TrackedActiveWidget = NewWidget;
		RecordWidgetUse(NewWidget.ToSharedRef());
		LogNewWidget = NewWidget->GetTypeAsString();
	}

	Logger.Print(
		LogFunc + FString::Printf(TEXT("Old Widget: %s\nNew Widget: %s"), *LogOldWidget, *LogNewWidget),
		ELogVerbosity::Verbose, bVisLogTabFocusFlow);
}

void UUMFocuserEditorSubsystem::OnActiveTabChanged(
	TSharedPtr<SDockTab> PrevActiveTab, TSharedPtr<SDockTab> NewActiveTab)
{
	const FString LogFunc = "OnActiveTabChanged:\n";
	Logger.Print(LogFunc, ELogVerbosity::Verbose, true);

	if (!GEditor->IsValidLowLevelFast() || !GEditor->IsTimerManagerValid())
		return;

	const TWeakPtr<SDockTab> WeakPrevActiveTab = PrevActiveTab;
	const TWeakPtr<SDockTab> WeakNewActiveTab = NewActiveTab;

	TSharedRef<FTimerManager> TimerManager = GEditor->GetTimerManager();
	TimerManager->ClearTimer(TimerHandle_OnActiveTabChanged);

	TimerManager->SetTimer(
		TimerHandle_OnActiveTabChanged,
		[this, WeakPrevActiveTab, WeakNewActiveTab]() {
			if (const TSharedPtr<SDockTab> PrevActiveTab = WeakPrevActiveTab.Pin())
			{
				// NOTE:
				// Intuitively we might think we need to register the Prev
				// Widget with the PrevActiveTab, but the flow of actions
				// unfolds by firstly making the tab switch focus, then the
				// widgets. This happens because when we activate a tab, our
				// OnFocusChanged event won't acatually trigger! That means
				// our ActiveWidget is still associated and lives inside the
				// previously active tab, and should be registered with it.
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

			if (const TSharedPtr<SDockTab> NewActiveTab = WeakNewActiveTab.Pin())
			{
				const TSharedRef<SDockTab> NewTabRef = NewActiveTab.ToSharedRef();
				if (NewActiveTab->GetVisualTabRole() != ETabRole::MajorTab)
					TryRegisterMinorWithParentMajorTab(NewTabRef);

				TryActivateLastWidgetInTab(NewTabRef);
				VisualizeParentDockingTabStack(NewTabRef);
			}
		},
		0.025,
		false);
}

void UUMFocuserEditorSubsystem::OnTabForegrounded(
	TSharedPtr<SDockTab> NewActiveTab, TSharedPtr<SDockTab> PrevActiveTab)
{
	const FString LogFunc = "OnTabForegrounded:\n";
	Logger.Print(LogFunc, ELogVerbosity::Verbose, true);

	// DebugPrevAndNewMinorTabsMajorTabs(NewActiveTab, PrevActiveTab);

	// FUMSlateHelpers::LogTab(NewActiveTab.ToSharedRef());

	if (PrevActiveTab.IsValid() && PrevActiveTab->GetTabRole() == ETabRole::NomadTab)
	{
		TryRegisterWidgetWithTab(PrevActiveTab.ToSharedRef());
		// OnActiveTabChanged(PrevActiveTab, NewActiveTab);
		// return;
	}

	if (!NewActiveTab.IsValid())
		return;

	if (NewActiveTab->GetVisualTabRole() != ETabRole::MajorTab)
		return; // Minor Tabs are handled automatically by OnActiveTabChanged

	const TSharedRef<SDockTab> TabRef = NewActiveTab.ToSharedRef();

	// ??
	if (NewActiveTab->GetTabRole() == ETabRole::NomadTab)
	{
		// OnActiveTabChanged(PrevActiveTab, NewActiveTab);
		// TryRegisterWidgetWithTab(NewActiveTab.ToSharedRef());
		Logger.Print("Nomad Tab Only?", ELogVerbosity::Warning, true);
		TryActivateLastWidgetInTab(TabRef);
		VisualizeParentDockingTabStack(TabRef, ETabRole::NomadTab);

		return;
	}

	// 1. Check if there's a registered Minor Tab to this Major Tab.
	if (TryFocusLastActiveMinorForMajorTab(TabRef))
		return;

	TSharedRef<SWidget> TabContent = TabRef->GetContent();
	// 2. Try to focus on the first Minor Tab (if it exists).
	if (TryFocusFirstFoundMinorTab(TabContent))
		return;

	// I think that if we're here; it's almost certainly a Nomad Tab.
	Logger.Print("Nomad Tab Only?", ELogVerbosity::Warning, true);
	return;

	// const TSharedPtr<SDockTab> TrackedMajorTab = TrackedActiveMajorTab.Pin();
	// if (TrackedMajorTab.IsValid() && TrackedMajorTab->GetId() == TabRef->GetId())
	// {
	// 	Logger.Print("Nomad Tab same as previous", ELogVerbosity::Warning, true);
	// 	return;
	// }
	// TrackedActiveMajorTab = NewActiveTab;
	// OnActiveTabChanged(PrevActiveTab, NewActiveTab);
	// return;

	// Experimental
	// if (TabRef->GetTabRole() == ETabRole::NomadTab)
	// 	if (TryActivateLastWidgetInTab(TabRef))
	// 		return;

	// 3. Try to focus on the first SearchBox (if it exists).
	if (TryFocusFirstFoundSearchBox(TabContent))
		return;

	// 4. Focus on the Tab's Content (default fallback).
	FocusTabContent(TabRef);
}

void UUMFocuserEditorSubsystem::DetectWidgetType(
	const TSharedRef<SWidget> InWidget)
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

bool UUMFocuserEditorSubsystem::VisualizeParentDockingTabStack(
	const TSharedRef<SDockTab> InTab, const ETabRole TabRole)
{
	// This is temporary; we will probably want something more fancy in the future
	if (TabRole == ETabRole::NomadTab)
	{
		FUMFocusVisualizer::Get()->DrawDebugOutlineOnWidget(InTab->GetContent());
		return true;
	}

	// Logger.Print("Try Draw Debug Outline", ELogVerbosity::Log, true);
	TWeakPtr<SWidget> DockingTabStack;
	if (FUMSlateHelpers::GetParentDockingTabStackAsWidget(
			InTab->GetContent(), DockingTabStack))
	{
		if (const auto PinDocking = DockingTabStack.Pin())
		{
			// Logger.Print("Draw Debug Outline", ELogVerbosity::Verbose, true);
			FUMFocusVisualizer::Get()->DrawDebugOutlineOnWidget(
				PinDocking.ToSharedRef());
			return true;
		}
	}
	return false;
}

void UUMFocuserEditorSubsystem::HandleOnWindowChanged(
	const TSharedPtr<SWindow> PrevWindow,
	const TSharedPtr<SWindow> NewWindow)
{
	if (!NewWindow.IsValid())
		return;

	Logger.Print(FString::Printf(TEXT("OnWindowChanged: New Window: %s"),
		*NewWindow->GetTitle().ToString()));

	// We need to manually activate in order to bring the window to the front
	// Until this line, Unreal can handle most of the things well natively.
	FUMSlateHelpers::ActivateWindow(NewWindow.ToSharedRef());

	// This section helps to make sure we'll have focus in case the new window
	// started with no focus while moving to a different window.
	// What does that mean?
	// Well, if we drag-off a Major Tab from a TabWell (making a new window
	// out of it), our dragged-from window will have no focus. Thus when we
	// try to return to it, we don't have any focus to come back to.
	// This is why we manually search for the frontmost Major Tab again and
	// try to focus it (if there's no focus descendants).
	TSharedRef<FTimerManager> TimerManager = GEditor->GetTimerManager();
	TimerManager->ClearTimer(TimerHandle_OnWindowChanged);
	TimerManager->SetTimer(
		TimerHandle_OnWindowChanged,
		[this, NewWindow]() {
			// if (NewWindow.IsValid()
			// 	&& NewWindow->GetContent()->HasFocusedDescendants())
			// 	return;

			const TSharedPtr<SDockTab> ActiveMajorTab =
				FUMSlateHelpers::GetActiveMajorTab();
			if (!ActiveMajorTab.IsValid())
				return;

			const TSharedPtr<SDockTab> TrackedMajorTab = TrackedActiveMajorTab.Pin();
			// OnTabForegrounded(ActiveMajorTab, nullptr);

			// Why not OnActiveTabChanged to actually register stuff?
			if (TrackedMajorTab.IsValid()
				&& TrackedMajorTab->GetId() != ActiveMajorTab->GetId())
			{
				TrackedActiveMajorTab = ActiveMajorTab;
				OnActiveTabChanged(TrackedMajorTab, ActiveMajorTab);
				return;
			}
			OnActiveTabChanged(nullptr, ActiveMajorTab);
		},
		0.1f,
		false);
	return;
}

bool UUMFocuserEditorSubsystem::ShouldFilterNewWidget(TSharedRef<SWidget> InWidget)
{
	static const TSet<FName> WidgetsToFilterByType{
		"SDockingTabStack",
		"SWindow",
		/* To be (potentially) continued */
	};
	return WidgetsToFilterByType.Contains(InWidget->GetType());
}

bool UUMFocuserEditorSubsystem::RemoveActiveMajorTab()
{
	static const FString LevelEditorType{ "LevelEditor" };

	if (const TSharedPtr<SDockTab> MajorTab =
			FUMSlateHelpers::GetActiveMajorTab())
		// Removing the LevelEditor will cause closing the entire editor which is
		// a bit too much.
		if (!MajorTab->GetLayoutIdentifier().ToString().Equals(LevelEditorType))
		{
			// Logger.Print(FString::Printf(TEXT("Try remove Major Tab: %s"), *MajorTab->GetTabLabel().ToString()), ELogVerbosity::Verbose, true);
			MajorTab->RemoveTabFromParent();
			FocusNextFrontmostWindow();
			return true;
		}
	return false;
}

bool UUMFocuserEditorSubsystem::FocusNextFrontmostWindow()
{
	FSlateApplication&			SlateApp = FSlateApplication::Get();
	TArray<TSharedRef<SWindow>> Wins;

	SlateApp.GetAllVisibleWindowsOrdered(Wins);
	for (int32 i{ Wins.Num() - 1 }; i > 0; --i)
	{
		if (!Wins[i]->GetTitle().IsEmpty())
		{
			FUMSlateHelpers::ActivateWindow(Wins[i]);
			return true; // Found the next window
		}
	}

	// If no windows were found; try to bring focus to the root window
	if (TSharedPtr<SWindow> RootWin = FGlobalTabmanager::Get()->GetRootWindow())
	{
		FUMSlateHelpers::ActivateWindow(RootWin.ToSharedRef());
		return true;
	}
	return false;
}

void UUMFocuserEditorSubsystem::ActivateNewInvokedTab(
	FSlateApplication& SlateApp, const TSharedPtr<SDockTab> NewTab)
{
	SlateApp.ClearAllUserFocus(); // NOTE: In order to actually draw focus

	if (!NewTab.IsValid())
		return;

	// TArray<TSharedRef<SWindow>> Wins;
	// SlateApp.GetAllVisibleWindowsOrdered(Wins);
	// if (Wins.IsEmpty())
	// 	return;
	// FUMSlateHelpers::ActivateWindow(Wins.Last());

	if (TSharedPtr<SWindow> Win = NewTab->GetParentWindow())
		FUMSlateHelpers::ActivateWindow(Win.ToSharedRef());

	ActivateTab(NewTab.ToSharedRef());
}

bool UUMFocuserEditorSubsystem::TryFocusLastActiveMinorForMajorTab(
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

bool UUMFocuserEditorSubsystem::TryFocusFirstFoundMinorTab(TSharedRef<SWidget> InContent)
{
	TSharedPtr<SWidget> FirstMinorTabAsWidget;
	if (FUMSlateHelpers::TraverseWidgetTree(
			InContent, FirstMinorTabAsWidget, "SDockTab"))
	{
		const TSharedPtr<SDockTab> FirstMinorTab =
			StaticCastSharedPtr<SDockTab>(FirstMinorTabAsWidget);

		ActivateTab(FirstMinorTab.ToSharedRef());
		Logger.Print("Minor Tab Found: Fallback to focus first Minor Tab.",
			ELogVerbosity::Warning, bVisLogTabFocusFlow);

		TryActivateLastWidgetInTab(FirstMinorTab.ToSharedRef());
		return true;
	}
	return false;
}

bool UUMFocuserEditorSubsystem::TryFocusFirstFoundSearchBox(
	TSharedRef<SWidget> InContent)
{
	// FSlateApplication& SlateApp = FSlateApplication::Get();
	TSharedPtr<SWidget> EditableTextAsWidget;
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
				if (EditableTextAsWidget.IsValid())
				{
					FSlateApplication& SlateApp = FSlateApplication::Get();
					SlateApp.ClearAllUserFocus();
					SlateApp.SetAllUserFocus(EditableTextAsWidget,
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

void UUMFocuserEditorSubsystem::FocusTabContent(TSharedRef<SDockTab> InTab)
{
	FSlateApplication& SlateApp = FSlateApplication::Get();
	// SlateApp.ClearAllUserFocus();
	SlateApp.SetAllUserFocus(InTab->GetContent(),
		EFocusCause::Navigation);

	Logger.Print("Fallback to Tab's Content.",
		ELogVerbosity::Warning, bVisLogTabFocusFlow);
}

bool UUMFocuserEditorSubsystem::TryRegisterMinorWithParentMajorTab(
	const TSharedRef<SDockTab> InMinorTab)
{
	TSharedRef<FGlobalTabmanager> GTM = FGlobalTabmanager::Get();

	// Check if the tab has a valid Tab Manager
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

bool UUMFocuserEditorSubsystem::TryRegisterWidgetWithTab(
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

bool UUMFocuserEditorSubsystem::TryRegisterWidgetWithTab(const TSharedRef<SDockTab> InTab)
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

bool UUMFocuserEditorSubsystem::TryActivateLastWidgetInTab(
	const TSharedRef<SDockTab> InTab)
{
	// return false; // NOTE: TEST

	// Logger.Print("TryActivateLastWidgetInTab.", ELogVerbosity::Log, true);
	// if this tab has any focus, it means the user had probably clicked on
	// it manually via the mouse or something. In the case, we don't want to
	// override the focus with the last registered one.
	if (InTab->GetContent()->HasFocusedDescendants())
	{
		TryRegisterWidgetWithTab(TrackedActiveWidget, InTab);
		// Logger.Print("TryActivateLastWidgetInTab: Tab had focus.", ELogVerbosity::Warning, true);
		return true;
	}

	FSlateApplication& SlateApp = FSlateApplication::Get();
	// Clearing before drawing focus seems to solidfy the end focus result,
	// especially for things like SGraphPanel allowing solid focus on Nodes.
	SlateApp.ClearAllUserFocus();
	TSharedRef<FTimerManager> TimerManager = GEditor->GetTimerManager();
	// We wanna clear previous timer to mitigate a potential loop
	TimerManager->ClearTimer(TimerHandle_FocusTabContentFallback);
	TimerManager->ClearTimer(TimerHandle_TryActivateLastWidgetInTab);

	// Try find a registered last active widget
	if (TWeakPtr<SWidget>* FoundWidget =
			LastActiveWidgetByTabId.Find(InTab->GetId()))
	{
		if (const TSharedPtr<SWidget> Widget = FoundWidget->Pin())
		{
			// It looks like we won't be able to pull focus properly on some
			// tabs without this delay. Especially Nomad Tabs.
			TimerManager->SetTimer(
				TimerHandle_TryActivateLastWidgetInTab,
				[&SlateApp, FoundWidget]() {
					if (const TSharedPtr<SWidget> Widget = FoundWidget->Pin())
						SlateApp.SetAllUserFocus(Widget, EFocusCause::Navigation);
				},
				0.025f, false);

			// SlateApp.SetAllUserFocus(Widget, EFocusCause::Navigation);
			Logger.Print(FString::Printf(TEXT("Found Widget: %s in Tab: %s"),
							 *Widget->GetTypeAsString(),
							 *InTab->GetTabLabel().ToString()),
				ELogVerbosity::Verbose, true);

			return true;
		}
	}

	// Registered widget was not found Fallback:
	// if (TryFocusFirstFoundListView(InTab))
	// 	return true;

	if (TryFocusFirstFoundSearchBox(InTab->GetContent()))
		return true;

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

void UUMFocuserEditorSubsystem::ActivateTab(const TSharedRef<SDockTab> InTab)
{
	InTab->ActivateInParent(ETabActivationCause::UserClickedOnTab);
	InTab->DrawAttention();
	// FGlobalTabmanager::Get()->SetActiveTab(InTab);
}

void UUMFocuserEditorSubsystem::RecordWidgetUse(TSharedRef<SWidget> InWidget)
{
	// Convert to TWeakPtr so we don’t keep it alive after it is destroyed.
	TWeakPtr<SWidget> WeakWidget = InWidget;

	for (int32 i = 0; i < RecentlyUsedWidgets.Num();)
	{
		if (const TSharedPtr<SWidget>& Widget = RecentlyUsedWidgets[i].Pin())
		{
			// Check if this is the widget we want to promote
			if (InWidget->GetId() == Widget->GetId())
			{
				// Remove it from the current position and reinsert at the front
				RecentlyUsedWidgets.RemoveAt(i);
				RecentlyUsedWidgets.Insert(WeakWidget, 0);
				return; // Widget found, return.
			}
			++i;
		}
		else
		// Remove invalid widget (stale reference)
		// Do not increment i because the array has shifted
		{
			RecentlyUsedWidgets.RemoveAt(i);
			Logger.Print("Removed invalid widget from recording.", ELogVerbosity::Warning);
		}
	}

	// If we didn’t find the widget, add it to the front
	RecentlyUsedWidgets.Insert(WeakWidget, 0);

	// Enforce size limit
	const int32 MaxSize = 10;
	if (RecentlyUsedWidgets.Num() > MaxSize)
		RecentlyUsedWidgets.SetNum(MaxSize); // Shrink the array if it exceeds Max
}

FString UUMFocuserEditorSubsystem::TabRoleToString(ETabRole InTabRole)
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

void UUMFocuserEditorSubsystem::OnWindowBeingDestroyed(const SWindow& Window)
{
	if (!Window.IsRegularWindow())
		return; // Ignoring all none-regular windows, like notification windows.

	// NOTE:
	// We want to have a small delay to only start searching after the window
	// is completey destroyed. Otherwise we will still catch that same winodw
	// that is being destroyed.
	TSharedRef<FTimerManager> TimerManager = GEditor->GetTimerManager();
	TimerManager->ClearTimer(TimerHandle_OnWindowBeingDestroyed);
	TimerManager->SetTimer(
		TimerHandle_OnWindowBeingDestroyed,
		[this]() {
			if (const TSharedPtr<SDockTab> ActiveMajorTab =
					FUMSlateHelpers::GetActiveMajorTab())
			{
				ActiveMajorTab->ActivateInParent(
					ETabActivationCause::UserClickedOnTab);
				OnTabForegrounded(ActiveMajorTab, nullptr);
			}
		},
		0.1f, false);

	// Logger.Print("Window being destroyed is not Regular");
}

void UUMFocuserEditorSubsystem::DebugPrevAndNewMinorTabsMajorTabs(
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
	if (const TSharedPtr<SWidget> PrevActiveWidget = TrackedPrevWidget.Pin())
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
