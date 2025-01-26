#include "UMFocuserEditorSubsystem.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Input/Events.h"
#include "Logging/LogVerbosity.h"
#include "UMInputHelpers.h"
#include "UMLogger.h"
#include "Editor.h"
#include "Widgets/Docking/SDockTab.h"
#include "UMSlateHelpers.h"
#include "UMWindowNavigatorEditorSubsystem.h"
#include "UMFocusVisualizer.h"
#include "UMConfig.h"
#include "UMFocusHelpers.h"

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

	FUMInputHelpers::OnSimulateRightClick.AddUObject(
		this, &UUMFocuserEditorSubsystem::UpdateWidgetForActiveTab);

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
	Logger.Print("On Focus Changed", ELogVerbosity::Log, true);
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

		// Only draws focus if Nomad Tab
		DrawFocusForNomadTab(NewWidget.ToSharedRef());
	}

	ValidateFocusedWidget(); // Make sure our currently focused widget is valid

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
				TryRegisterWidgetWithTab(PrevActiveTab.ToSharedRef());
			}

			if (const TSharedPtr<SDockTab> NewActiveTab = WeakNewActiveTab.Pin())
			{
				const TSharedRef<SDockTab> NewTabRef = NewActiveTab.ToSharedRef();
				TryRegisterWidgetWithTab(NewTabRef); // Right?? Why not

				if (NewTabRef->GetVisualTabRole() != ETabRole::MajorTab)
				{
					TryRegisterMinorWithParentMajorTab(NewTabRef);
					TryActivateLastWidgetInTab(NewTabRef);
					VisualizeParentDockingTabStack(NewTabRef);
					return;
				}

				if (NewTabRef->GetTabRole() == ETabRole::NomadTab)
					TryActivateLastWidgetInTab(NewTabRef);
				else
					TryFocusLastActiveMinorForMajorTab(NewTabRef);
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

	if (PrevActiveTab.IsValid()
		&& PrevActiveTab->GetTabRole() == ETabRole::NomadTab)
	{
		TryRegisterWidgetWithTab(PrevActiveTab.ToSharedRef());
	}

	if (!NewActiveTab.IsValid())
		return;

	if (NewActiveTab->GetVisualTabRole() != ETabRole::MajorTab)
		return; // Minor Tabs are handled automatically by OnActiveTabChanged

	const TSharedRef<SDockTab> TabRef = NewActiveTab.ToSharedRef();

	// if Nomad Tab, we need to manually call the TryActivate as it isn't picked
	// up by OnActiveTabChanged
	if (NewActiveTab->GetTabRole() == ETabRole::NomadTab)
	{
		// Logger.Print("Nomad Tab Only", ELogVerbosity::Warning, true);

		TryActivateLastWidgetInTab(TabRef);
		TryRegisterWidgetWithTab(TabRef, 0.05); // Right?? Why not
		// VisualizeParentDockingTabStack(TabRef, ETabRole::NomadTab);
		return;
	}

	// 1. Check if there's a registered Minor Tab to this Major Tab.
	if (TryFocusLastActiveMinorForMajorTab(TabRef))
		return;

	TSharedRef<SWidget> TabContent = TabRef->GetContent();
	// 2. Try to focus on the first Minor Tab (if it exists).
	if (TryFocusFirstFoundMinorTab(TabContent))
		return;

	////////////////
	// Deprecated?
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
		// FUMFocusVisualizer::Get()->DrawDebugOutlineOnWidget(InTab->GetContent());

		// const TSharedPtr<SWidget> Parent = FUMSlateHelpers::GetAssociatedParentSplitterChild(NewWidget.ToSharedRef());

		// if (Parent.IsValid())
		// 	FUMFocusVisualizer::Get()->DrawDebugOutlineOnWidget(Parent.ToSharedRef());

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

	// We need to manually activate in order to bring the window to the front.
	// Until this line, UE can handle focus pretty well natively.
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
			// ??
			// if (NewWindow.IsValid()
			// 	&& NewWindow->GetContent()->HasFocusedDescendants())
			// 	return;

			const auto ActiveMajorTab = FUMSlateHelpers::GetActiveMajorTab();
			if (!ActiveMajorTab.IsValid())
				return;

			const auto PinTrackedMajorTab = TrackedActiveMajorTab.Pin();

			if (PinTrackedMajorTab.IsValid()
				&& PinTrackedMajorTab->GetId() != ActiveMajorTab->GetId())
			{
				TrackedActiveMajorTab = ActiveMajorTab;
				OnActiveTabChanged(PinTrackedMajorTab, ActiveMajorTab);
				return;
			}
			OnActiveTabChanged(nullptr, ActiveMajorTab);
		},
		0.01f,
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
			const TSharedRef<SDockTab> TabRef = LastActiveMinorTab.ToSharedRef();
			ActivateTab(TabRef);
			Logger.Print(FString::Printf(TEXT("Last Active Minor Tab found: %s"),
							 *TabRef->GetTabLabel().ToString()),
				ELogVerbosity::Verbose, bVisLogTabFocusFlow);

			TryActivateLastWidgetInTab(TabRef);
			// if (TabRef->GetTabRole() != ETabRole::NomadTab)
			VisualizeParentDockingTabStack(TabRef);
			return true;
		}
	}
	return false;
}

bool UUMFocuserEditorSubsystem::TryFocusFirstFoundMinorTab(TSharedRef<SWidget> InContent)
{
	TSharedPtr<SWidget> FirstMinorTabAsWidget;
	if (FUMSlateHelpers::TraverseFindWidget(
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
	if (FUMSlateHelpers::TraverseFindWidget(
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
	const TSharedRef<SWidget> InWidget, const TSharedRef<SDockTab> InTab)
{
	if (FUMSlateHelpers::DoesWidgetResideInTab(InTab, InWidget))
	{
		LastActiveWidgetByTabId.Add(InTab->GetId(), InWidget);

		Log_TryRegisterWidgetWithTab(InTab, InWidget);
		return true;
	}
	Log_TryRegisterWidgetWithTab(InTab, nullptr);
	return false;
}

bool UUMFocuserEditorSubsystem::TryRegisterWidgetWithTab(
	const TSharedRef<SDockTab> InTab)
{
	for (const TWeakPtr<SWidget>& WidgetWeakPtr : RecentlyUsedWidgets)
	{
		if (const TSharedPtr<SWidget> WidgetPtr = WidgetWeakPtr.Pin())
		{
			// if found the most recent widget that lives in the new tab
			const TSharedRef<SWidget> WidgetRef = WidgetPtr.ToSharedRef();
			if (FUMSlateHelpers::DoesWidgetResideInTab(InTab, WidgetRef))
			{
				LastActiveWidgetByTabId.Add(InTab->GetId(), WidgetWeakPtr);

				Log_TryRegisterWidgetWithTab(InTab, WidgetRef);
				return true;
			}
		}
	}
	Log_TryRegisterWidgetWithTab(InTab, nullptr);
	return false;
}

void UUMFocuserEditorSubsystem::TryRegisterWidgetWithTab(
	const TSharedRef<SDockTab> InTab, float Delay)
{
	TWeakPtr<SDockTab>		  WeakTab = InTab;
	TSharedRef<FTimerManager> TimerManager = GEditor->GetTimerManager();
	TimerManager->ClearTimer(TimerHandle_TryRegisterWidgetWithTab);

	TimerManager->SetTimer(
		TimerHandle_TryRegisterWidgetWithTab,
		[this, WeakTab]() {
			if (const TSharedPtr<SDockTab> TabPtr = WeakTab.Pin())
			{
				const TSharedRef<SDockTab> InTab = TabPtr.ToSharedRef();
				TryRegisterWidgetWithTab(InTab);
			}
		},
		Delay, false);
}

void UUMFocuserEditorSubsystem::Log_TryRegisterWidgetWithTab(const TSharedRef<SDockTab> InTab, const TSharedPtr<SWidget> InRegisteredWidget)
{
	const bool bVisLog = false;

	if (InRegisteredWidget.IsValid())
	{
		Logger.Print(
			FString::Printf(
				TEXT("Most recent widget in tab '%s' is '%s'"),
				*InTab->GetTabLabel().ToString(),
				*InRegisteredWidget->GetTypeAsString()),
			ELogVerbosity::Verbose, bVisLog);
	}
	else
	{
		Logger.Print(FString::Printf(
						 TEXT("Could NOT register any Widget with Tab: %s"),
						 *InTab->GetTabLabel().ToString()),
			ELogVerbosity::Error, bVisLog);
	}
}

bool UUMFocuserEditorSubsystem::TryActivateLastWidgetInTab(
	const TSharedRef<SDockTab> InTab)
{
	// Logger.Print("TryActivateLastWidgetInTab.", ELogVerbosity::Log, true);

	// NOTE:
	// if this tab has any focus, it means the user had probably clicked on
	// it manually via the mouse or something. And so, we want to keep the focus
	// as is, as he had an intention to draw focus to a specific widget.
	if (InTab->GetContent()->HasFocusedDescendants())
	{
		// TEST
		if (const auto Widget = TrackedActiveWidget.Pin())
		{
			const TSharedRef<SWidget> WidgetRef = Widget.ToSharedRef();
			DrawFocusForNomadTab(WidgetRef);
			TryRegisterWidgetWithTab(WidgetRef, InTab);
		}
		// Log_TryActivateLastWidgetInTab(InTab, nullptr, true);
		return true;
	}

	if (IsFirstEncounter(InTab))
		return FirstEncounterDefaultInit(InTab);

	// Try find the last active registered widget
	const uint64 TabId = InTab->GetId();
	if (TWeakPtr<SWidget>* FoundWidget = LastActiveWidgetByTabId.Find(TabId))
	{
		if (const TSharedPtr<SWidget> Widget = FoundWidget->Pin())
		{
			// It looks like we won't be able to pull focus properly on some
			// tabs without this delay. Especially Nomad Tabs.
			FUMFocusHelpers::SetWidgetFocusWithDelay(
				Widget.ToSharedRef(),
				TimerHandle_TryActivateLastWidgetInTab, 0.025f, true);

			// Log_TryActivateLastWidgetInTab(InTab, Widget);
			if (InTab->GetTabRole() == ETabRole::NomadTab)
				FUMFocusVisualizer::Get()->DrawDebugOutlineOnWidget(Widget.ToSharedRef());
			return true;
		}
	}
	// Log_TryActivateLastWidgetInTab(InTab, nullptr);

	return false; // We don't wany any other fallback?
}

void UUMFocuserEditorSubsystem::Log_TryActivateLastWidgetInTab(const TSharedRef<SDockTab> InTab, const TSharedPtr<SWidget> FoundWidget, const bool bTabHadFocus)
{
	if (bTabHadFocus)
	{
		Logger.Print("TryActivateLastWidgetInTab: Tab had focus.",
			ELogVerbosity::Log, true);
		return;
	}

	if (FoundWidget.IsValid())
	{
		Logger.Print(FString::Printf(TEXT("Found Widget: %s in Tab: %s"),
						 *FoundWidget->GetTypeAsString(),
						 *InTab->GetTabLabel().ToString()),
			ELogVerbosity::Verbose, true);
	}
	else
	{
		Logger.Print(FString::Printf(TEXT("Could NOT Find any Widget in Tab: %s"),
						 *InTab->GetTabLabel().ToString()),
			ELogVerbosity::Error, true);
	}
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

bool UUMFocuserEditorSubsystem::IsFirstEncounter(const TSharedRef<SDockTab> InTab)
{
	static TSet<uint64> TabEncounterTrackingMap;

	const uint64 TabId = InTab->GetId();
	if (TabEncounterTrackingMap.Contains(TabId))
		return false; // Already tracked and seen before

	TabEncounterTrackingMap.Add(TabId);
	return true; // First time seen; add for tracking and return true
}

bool UUMFocuserEditorSubsystem::FirstEncounterDefaultInit(
	const TSharedRef<SDockTab> InTab)
{
	static const TMap<FString, FString> DefaultWidgetTypeByTabLabel{
		// Nomad Tabs
		{ "Content Browser", "SAssetTileView" },
		{ "Editor Preferences", "SEditableText" },
		{ "Project Settings", "SEditableText" },
		{ "Output Log", "SEditableText" },
		{ "Widget Reflector", "SToolBarButtonBlock" },

		// Level Editor
		{ "Outliner", "SSceneOutlinerTreeView" },
		{ "Viewport", "SViewport" },

		// Blueprint Editor
		{ "Palette", "SEditableText" },
		// { "Hierarchy", "STreeView< TSharedPtr<FHierarchyModel> >" },
		// { "Hierarchy", "STreeView" },
		{ "Hierarchy", "SEditableText" },
		{ "Details", "SEditableText" },
		{ "My Blueprint", "SEditableText" },
		{ "Event Graph", "SGraphPanel" },
		{ "Construction Script", "SGraphPanel" },
		{ "Components", "SSubobjectEditorDragDropTree" },
		{ "Compiler Results", "SDockingTabStack" },
	};

	const FString TabLabel = FUMSlateHelpers::GetCleanTabLabel(InTab);
	if (const FString* DefaultType = DefaultWidgetTypeByTabLabel.Find(TabLabel))
	{
		TSharedPtr<SWidget> FoundDefaultWidget;
		if (!FUMSlateHelpers::TraverseFindWidget(
				InTab->GetContent(), FoundDefaultWidget, *DefaultType))
			return false;

		const TSharedRef<SWidget> WidgetRef = FoundDefaultWidget.ToSharedRef();
		TryRegisterWidgetWithTab(WidgetRef, InTab);
		FUMFocusHelpers::SetWidgetFocusWithDelay(WidgetRef,
			TimerHandle_GenericSetWidgetFocusWithDelay, 0.025f, true);

		Log_FirstEncounterDefaultInit(InTab, FoundDefaultWidget);
		return true;
	}

	Log_FirstEncounterDefaultInit(InTab, nullptr);
	TryFocusFirstFoundSearchBox(InTab->GetContent()); // Default temp
	return false;
}

void UUMFocuserEditorSubsystem::Log_FirstEncounterDefaultInit(const TSharedRef<SDockTab> InTab, const TSharedPtr<SWidget> FoundWidget)
{
	const bool bVisLog = true;

	if (FoundWidget.IsValid())
	{
		Logger.Print(FString::Printf(
						 TEXT("Initialized first encountered Tab{%s} Widget{%s}"),
						 *InTab->GetTabLabel().ToString(),
						 *FoundWidget->GetTypeAsString()),
			ELogVerbosity::Verbose, bVisLog);
	}
	else
	{
		Logger.Print(FString::Printf(
						 TEXT("Could NOT initialize first encountered Tab{%s}"),
						 *InTab->GetTabLabel().ToString()),
			ELogVerbosity::Error, bVisLog);
	}
}

bool UUMFocuserEditorSubsystem::DrawFocusForNomadTab(
	const TSharedRef<SWidget> AssociatedWidget)
{
	if (FUMSlateHelpers::IsCurrentlyActiveTabNomad())
	{
		const TSharedPtr<SWidget> Parent =
			FUMSlateHelpers::GetAssociatedParentSplitterChild(AssociatedWidget);

		if (Parent.IsValid())
		{
			FUMFocusVisualizer::Get()->DrawDebugOutlineOnWidget(Parent.ToSharedRef());
			return true;
		}
	}
	return false;
}

bool UUMFocuserEditorSubsystem::TryBringFocusToActiveTab()
{
	const TSharedPtr<SDockTab> MajorTab = FUMSlateHelpers::GetActiveMajorTab();
	if (!MajorTab.IsValid())
	{
		Logger.Print("Invalid Major Tab!", ELogVerbosity::Error, true);
		return false;
	}

	const TSharedRef<SDockTab> TabRef = MajorTab.ToSharedRef();
	if (TabRef->GetTabRole() == ETabRole::NomadTab)
		TryActivateLastWidgetInTab(TabRef);
	else
		TryFocusLastActiveMinorForMajorTab(TabRef);

	Logger.Print("TryBringFocusToActiveTab: Success!",
		ELogVerbosity::Verbose, true);
	return true;
}

void UUMFocuserEditorSubsystem::ValidateFocusedWidget()
{
	TSharedRef<FTimerManager> TimerManager = GEditor->GetTimerManager();
	TimerManager->ClearTimer(TimerHandle_ValidateFocusedWidget);

	TimerManager->SetTimer(
		TimerHandle_ValidateFocusedWidget,
		[this]() {
			FSlateApplication&		  SlateApp = FSlateApplication::Get();
			const TSharedPtr<SWidget> FocusedWidget = SlateApp.GetUserFocusedWidget(0);

			if (!FocusedWidget.IsValid())
				TryBringFocusToActiveTab();
		},
		0.2f,
		false);
}

void UUMFocuserEditorSubsystem::UpdateWidgetForActiveTab()
{
	const TSharedPtr<SDockTab> ActiveMajorTab = FUMSlateHelpers::GetActiveMajorTab();

	if (!ActiveMajorTab.IsValid())
		return;

	const TSharedRef<SDockTab> MajorTabRef = ActiveMajorTab.ToSharedRef();
	if (ActiveMajorTab->GetTabRole() == ETabRole::NomadTab)
	{
		TryRegisterWidgetWithTab(MajorTabRef);
	}
	else
	{
		if (const auto ActiveMinorTab = FUMSlateHelpers::GetActiveMinorTab())
		{
			const TSharedRef<SDockTab> MinorTabRef = ActiveMinorTab.ToSharedRef();
			TryRegisterMinorWithParentMajorTab(MinorTabRef);
			TryRegisterWidgetWithTab(MinorTabRef);
		}
	}
}
