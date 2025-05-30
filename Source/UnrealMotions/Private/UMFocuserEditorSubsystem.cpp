#include "UMFocuserEditorSubsystem.h"
#include "Engine/TimerHandle.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Input/Events.h"
#include "Logging/LogVerbosity.h"
#include "UMInputHelpers.h"
#include "UMLogger.h"
#include "Editor.h"
#include "VimInputProcessor.h"
#include "Widgets/Docking/SDockTab.h"
#include "UMSlateHelpers.h"
#include "UMWindowNavigatorEditorSubsystem.h"
#include "UMFocusVisualizer.h"
#include "UMConfig.h"
#include "UMFocusHelpers.h"

// DEFINE_LOG_CATEGORY_STATIC(UMFocuserEditorSubsystem, NoLogging, All); // Prod
DEFINE_LOG_CATEGORY_STATIC(UMFocuserEditorSubsystem, Log, All); // Dev
EUMBindingContext UUMFocuserEditorSubsystem::CurrentContext{ EUMBindingContext::Generic };

bool UUMFocuserEditorSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	TSharedRef<FUMConfig> Config = FUMConfig::Get();

	// If Vim is enabled, we must enable the Focuser to also exist
	return Config->IsFocuserEnabled() || Config->IsVimEnabled();
}

void UUMFocuserEditorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Logger = FUMLogger(&UMFocuserEditorSubsystem);
	ListNavigationManager.Logger = &Logger;
	ListNavigationManager.FocuserSub = this;

	FCoreDelegates::OnPostEngineInit.AddUObject(
		this, &UUMFocuserEditorSubsystem::RegisterSlateEvents);

	FUMInputHelpers::OnSimulateRightClick.AddUObject(
		this, &UUMFocuserEditorSubsystem::UpdateWidgetForActiveTab);

	Super::Initialize(Collection);

	if (FUMConfig::Get()->IsVimEnabled())
		BindVimCommands();
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

bool UUMFocuserEditorSubsystem::CheckWindowChanged()
{
	TSharedPtr<SWindow>		  OptNewWindow;
	const TSharedPtr<SWindow> TrackedWindow = TrackedActiveWindow.Pin();
	if (FUMSlateHelpers::CheckReplaceIfWindowChanged(
			TrackedWindow, OptNewWindow))
	{
		// This will fix an issue when jumping to a window that doesn't contain
		// a popmenu. BUT; for popup menus, it won't be able to detect if a pop
		// up menu has focus (within the window) which is like a problem lol
		// TODO: FUMSlateHelpers::ActivateWindow should detect if a window has
		// a popup menu active to not make it poof!
		TWeakPtr<SWindow> WeakOptNewWindow = OptNewWindow;
		TWeakPtr<SWindow> WeakTrackedWindow = TrackedWindow;
		FTimerHandle	  TimerHandle;
		GEditor->GetTimerManager()->SetTimer(
			TimerHandle,
			[this, WeakOptNewWindow, WeakTrackedWindow]() {
				HandleOnWindowChanged(WeakTrackedWindow.Pin(), WeakOptNewWindow.Pin());
			},
			0.025f, false);

		// HandleOnWindowChanged(TrackedWindow, OptNewWindow);
		TrackedActiveWindow = OptNewWindow;
		return true;

		// Logger.Print("Window Changed", ELogVerbosity::Log, true);
	}
	return false;
}

void UUMFocuserEditorSubsystem::OnFocusChanged(const FFocusEvent& FocusEvent, const FWeakWidgetPath& OldWidgetPath,
	const TSharedPtr<SWidget>& OldWidget, const FWidgetPath& NewWidgetPath,
	const TSharedPtr<SWidget>& NewWidget)
{
	// Logger.Print("On Focus Changed", ELogVerbosity::Log, true);
	Log_OnFocusChanged(OldWidget, NewWidget);

	// This seems to throw off the focus when changing window with the new Vimium
	// method. So commenting this off. Not sure exactly how useful this is to
	// track this here?
	if (CheckWindowChanged())
	{
		Logger.Print("OnFocusChanged: Window Changed!", ELogVerbosity::Log, true);
	}

	if (OldWidget.IsValid() && !ShouldFilterNewWidget(OldWidget.ToSharedRef()))
	{
		TrackedPrevWidget = OldWidget; // Deprecated (not used)
	}

	// Delayed function that helps us verify widget validity.
	ValidateFocusedWidget();

	if (!NewWidget.IsValid())
		return;

	const TSharedRef<SWidget> NewWidgetRef = NewWidget.ToSharedRef();
	// Update context to determine which Trie should be used for hotkeys inputted
	UpdateBindingContext(NewWidgetRef);

	if (!ShouldFilterNewWidget(NewWidgetRef))
	{

		TrackedActiveWidget = NewWidget; // Deprecated (not used)

		// TODO: Refactor to a proper function. This list of widget can grow.
		// These are widgets that aren't useful for navigation and such, thus
		// we want to fallback to something more useful when they're detected.
		if (NewWidget->GetTypeAsString().Equals("SDetailTree"))
		{
			TryBringFocusToActiveTab();
			return;
		}

		RecordWidgetUse(NewWidgetRef);

		// Don't track if in a None-Regular Window (probably Menu Window)
		if (FUMSlateHelpers::DoesWidgetResidesInRegularWindow(
				FSlateApplication::Get(), NewWidgetRef))
			TryRegisterWidgetWithTab(); // It's useful to constantly update

		// Won't do anything for None-Nomad. This is needed for visualization.
		DrawFocusForNomadTab(NewWidgetRef);
	}
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
				TryRegisterWidgetWithTab(NewTabRef); // Right?

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

void UUMFocuserEditorSubsystem::UpdateBindingContext(const TSharedRef<SWidget> NewWidget)
{
	static TMap<FString, EUMBindingContext> ContextByWidgetType = {
		{ "SGraphPanel", EUMBindingContext::GraphEditor },

		{ "SEditableText", EUMBindingContext::TextEditing },
		{ "SMultiLineEditableText", EUMBindingContext::TextEditing },
	};

	// Cover both specific and base type as a fallback
	const FString SpecificType = NewWidget->GetTypeAsString();
	const FString BaseType = NewWidget->GetWidgetClass().GetWidgetType().ToString();

	// Firstly search by specific type
	if (EUMBindingContext* Context = ContextByWidgetType.Find(SpecificType))
	{
		if (CurrentContext != *Context)
		{
			Logger.Print(FString::Printf(TEXT("New Context found for type: %s"),
							 *SpecificType),
				ELogVerbosity::Verbose, true);
			CurrentContext = *Context;
			FVimInputProcessor::Get()->SetCurrentContext(CurrentContext);
			OnBindingContextChanged.Broadcast(CurrentContext, NewWidget);
		}
		return; // Early return instead of elseif to not hide prev var dec Cntxt
	}

	// Then by base type
	if (EUMBindingContext* Context = ContextByWidgetType.Find(BaseType))
	{
		if (CurrentContext != *Context)
		{
			CurrentContext = *Context;
			FVimInputProcessor::Get()->SetCurrentContext(CurrentContext);
			OnBindingContextChanged.Broadcast(CurrentContext, NewWidget);
		}
	}
	else // Fallback to generic
	{
		if (CurrentContext != EUMBindingContext::Generic)
		{
			FVimInputProcessor::Get()->SetCurrentContext(EUMBindingContext::Generic);
			CurrentContext = EUMBindingContext::Generic;
			OnBindingContextChanged.Broadcast(CurrentContext, NewWidget);
		}
	}
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

bool UUMFocuserEditorSubsystem::TryRegisterWidgetWithTab()
{
	// Maybe we can also go about this by trying to grab the active minor and
	// checking if it has any focus? Thus knowing if we need to Nomad?

	const TSharedPtr<SDockTab> ActiveMajorTab = FUMSlateHelpers::GetActiveMajorTab();
	if (!ActiveMajorTab.IsValid())
		return false;

	TSharedPtr<SDockTab> ActiveTab;
	if (ActiveMajorTab->GetTabRole() == ETabRole::NomadTab)
	{
		ActiveTab = ActiveMajorTab;
	}
	else
	{
		ActiveTab = FUMSlateHelpers::GetActiveMinorTab();
		if (!ActiveTab.IsValid())
			return false;
	}

	// Doesn't seem to be solid at all
	// TEST
	// const TSharedPtr<SDockTab> ActiveMinorTab = FUMSlateHelpers::GetActiveMinorTab();
	// if (ActiveMinorTab.IsValid()
	// 	&& ActiveMinorTab->GetContent()->HasAnyUserFocusOrFocusedDescendants()
	// 	&& ActiveMinorTab->IsForeground()
	// 	&& ActiveMinorTab->IsActive())
	// {
	// 	Logger.Print("We know to fetch the Minor Tab (CHEAPER)", ELogVerbosity::Verbose, true);
	// }
	// else
	// {
	// 	Logger.Print("We know to fetch the Nomad Tab (CHEAPER)", ELogVerbosity::Verbose, true);
	// }
	// TEST

	return TryRegisterWidgetWithTab(ActiveTab.ToSharedRef());
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
		TryRegisterWidgetWithTab(InTab);
		return true;
	}

	// TODO:
	// Issue: When we have no focus we need to first check if there's potentially
	// a Menu Popup window that is waiting for focus. If so, we want to focus it.
	if (IsFirstEncounter(InTab))
		return FirstEncounterDefaultInit(InTab);

	// Try find the last active registered widget
	const uint64 TabId = InTab->GetId();
	if (TWeakPtr<SWidget>* FoundWidget = LastActiveWidgetByTabId.Find(TabId))
	{
		if (const TSharedPtr<SWidget> Widget = FoundWidget->Pin())
		{
			if (FUMFocusHelpers::TryFocusPopupMenu(FSlateApplication::Get()))
				return true;

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

	// NOTE:
	// We fallback to the first interactive widget found.
	// Why is this needed:
	// We default to SGraphPanel for Event Graph tabs, but we can have many more
	// tabs with different titles that are indeed blueprints, and should have
	// SGraphPanel as the first fallback, but won't be recognized because
	// their names can be different. So we grab the first interactive widget,
	// which for blueprint tabs works well. Though need to keep an eye on this
	// method and see if it handles good fallbacks in the future.
	// Marking this as WIP.
	TSharedPtr<SWidget> FoundWidget;
	if (FUMSlateHelpers::TraverseFindWidget(InTab->GetContent(), FoundWidget, FUMSlateHelpers::GetInteractableWidgetTypes()))
	{
		FSlateApplication::Get().SetAllUserFocus(FoundWidget, EFocusCause::Navigation);
		return true;
	}
	// Last fallback to the content of the tab (which in most cases isn't very
	// useful, but that's the only thing left?)
	FSlateApplication::Get().SetAllUserFocus(InTab->GetContent(), EFocusCause::Navigation);
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
			else
				ListNavigationManager.TrackFocusedWidget(FocusedWidget.ToSharedRef());
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

void UUMFocuserEditorSubsystem::Log_OnFocusChanged(
	const TSharedPtr<SWidget> OldWidget,
	const TSharedPtr<SWidget> NewWidget)
{
	bool bVisLog = false;

	const FString LogFunc = "OnFocusChanged:\n";
	FString		  LogOldWidget = "Invalid";
	FString		  LogNewWidget = "Invalid";

	if (OldWidget.IsValid())
	{
		LogOldWidget = " Specific Type: " + OldWidget->GetTypeAsString();
		LogOldWidget = "\nClass Type: " + OldWidget->GetWidgetClass().GetWidgetType().ToString();
	}

	if (NewWidget.IsValid())
	{
		// FUMSlateHelpers::DebugClimbUpFromWidget(NewWidget.ToSharedRef());
		LogOldWidget = " Specific Type: " + NewWidget->GetTypeAsString();
		LogOldWidget = "\nClass Type: " + NewWidget->GetWidgetClass().GetWidgetType().ToString();
	}

	Logger.Print(
		LogFunc + FString::Printf(TEXT("Old Widget: %s\nNew Widget: %s"), *LogOldWidget, *LogNewWidget),
		ELogVerbosity::Verbose, true);
}

///////////////////////////////////////////////////////////////////////////////
//				~ FGraphSelectionTracker Implementation ~ //
UUMFocuserEditorSubsystem::FListNavigationManager::FTrackedInst::FTrackedInst(
	TWeakPtr<SWidget>  InWidget,
	TWeakPtr<SWindow>  InParentWindow,
	TWeakPtr<SDockTab> InParentMajorTab,
	bool			   InbParentNomadTab,
	TWeakPtr<SDockTab> InParentMinorTab)
{
	Widget = InWidget;
	ParentWindow = InParentWindow;
	ParentMajorTab = InParentMajorTab;
	bIsParentNomadTab = InbParentNomadTab;
	ParentMinorTab = InParentMinorTab;
}

bool UUMFocuserEditorSubsystem::FListNavigationManager::FTrackedInst::IsValid()
{
	return (Widget.IsValid() && ParentWindow.IsValid() && ParentMajorTab.IsValid()
		&& (bIsParentNomadTab || ParentMinorTab.IsValid()));
}

void UUMFocuserEditorSubsystem::FListNavigationManager::Push(
	TSharedRef<SWidget>	 InWidget,
	TSharedRef<SWindow>	 InParentWindow,
	TSharedRef<SDockTab> InParentMajorTab,
	bool				 InbParentNomadTab,
	TSharedPtr<SDockTab> InParentMinorTab)
{
	// if (!TrackedWidgets.IsEmpty())
	// {
	// 	for (int32 i = TrackedWidgets.Num() - 1; i >= 0; --i)
	// 	{
	// 		if (!TrackedWidgets[i].IsValid())
	// 		{
	// 			TrackedWidgets.RemoveAt(i); // Remove invalid widgets
	// 			continue;
	// 		}
	// 		else if (TrackedWidgets[i].Widget.Pin()->GetId() == InWidget->GetId())
	// 		{
	// 			TrackedWidgets.RemoveAt(i); // No duplicates
	// 			break;
	// 		}
	// 	}
	// }

	if (bHasJumpedToNewWidget)
	{
		bHasJumpedToNewWidget = false;
		return;
	}

	TrackedWidgets.RemoveAll([&](const FTrackedInst& Inst) {
		if (const TSharedPtr<SWidget> Widget = Inst.Widget.Pin())
		{
			return Widget->GetId() == InWidget->GetId();
		}
		return true;
	});

	while (TrackedWidgets.Num() > CAP) // Shrink the array if it exceeds Max
		TrackedWidgets.RemoveAt(TrackedWidgets.Num() - 1);

	// if our currently focused widget is a widget we've previously jumped to:
	// we want it to be behind the upcoming new widget (for continuity)
	if (CurrWidgetIndex != 0
		&& TrackedWidgets.IsValidIndex(CurrWidgetIndex)
		&& TrackedWidgets[CurrWidgetIndex].IsValid())
	{
		FTrackedInst Temp = TrackedWidgets[CurrWidgetIndex];
		TrackedWidgets.RemoveAt(CurrWidgetIndex);
		TrackedWidgets.Insert(Temp, 0);
	}

	TrackedWidgets.Insert(FTrackedInst(InWidget, InParentWindow, InParentMajorTab, InbParentNomadTab, InParentMinorTab), 0);
	CurrWidgetIndex = 0; // We're at the front when a new widget is added, right?
}

// TODO: Figure out what's going on with pop-up windows -> they seem to crash
void UUMFocuserEditorSubsystem::FListNavigationManager::TrackFocusedWidget(const TSharedRef<SWidget> NewWidget)
{
	TWeakPtr<SWidget>		  WeakNewWidget = NewWidget;
	TSharedRef<FTimerManager> TimerManager = GEditor->GetTimerManager();
	TimerManager->ClearTimer(TimerHandle_TrackFocusedWidget);
	FSlateApplication& SlateApp = FSlateApplication::Get();

	// TSharedPtr<SWindow> ParentWindow = SlateApp.FindWidgetWindow(NewWidget);
	TSharedPtr<SWindow> ParentWindow = SlateApp.GetActiveTopLevelRegularWindow();
	if (!ParentWindow.IsValid())
		return;

	TSharedRef<FGlobalTabmanager> GTM = FGlobalTabmanager::Get();
	TSharedPtr<SDockTab>		  ActiveMajorTab = FUMSlateHelpers::GetActiveMajorTab();
	if (!ActiveMajorTab.IsValid())
		return;
	TSharedPtr<SDockTab> ActiveMinorTab = nullptr;
	// if nomad tab, no Minor Tab can be fetched (Nomad Tabs doesn't have minors)
	bool bIsParentNomadTab = ActiveMajorTab->GetTabRole() == ETabRole::NomadTab;
	if (!bIsParentNomadTab)
		ActiveMinorTab = FUMSlateHelpers::GetActiveMinorTab();

	// Maybe should be checked?
	// FUMSlateHelpers::DoesWidgetResideInTab(
	// 	ActiveMajorTab.ToSharedRef(), NewWidget);

	// Track widget params:
	Push(NewWidget, ParentWindow.ToSharedRef(),
		ActiveMajorTab.ToSharedRef(), bIsParentNomadTab, ActiveMinorTab);
	// },
	// 0.3f /* Test */, false);
}

void UUMFocuserEditorSubsystem::FListNavigationManager::JumpListNavigation(
	FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	if (TrackedWidgets.IsEmpty())
		return;

	// Logger->Print(FString::Printf(TEXT("Current Index: %d"),
	// 				  CurrWidgetIndex),
	// 	ELogVerbosity::Log, true);

	const FKey InKey = InKeyEvent.GetKey();
	int32	   TempIndex = CurrWidgetIndex;

	if (InKey == FKey(EKeys::I)) // In
	{
		if (TempIndex <= 0)
		{
			// Logger->Print("Can't Jump: Index is at top-of-stack", ELogVerbosity::Warning);
			CurrWidgetIndex = 0;
			return; // We're at the top of the stack: No widgets to go into
		}

		--TempIndex;

		while (!TrackedWidgets[TempIndex].IsValid())
		{
			TrackedWidgets.RemoveAt(TempIndex);

			if (TempIndex <= 0 || TrackedWidgets.IsEmpty())
			{
				// Logger->Print("Can't Jump: Index is at top-of-stack", ELogVerbosity::Warning);
				CurrWidgetIndex = 0;
				return; // No widgets to go into
			}

			--TempIndex;
		}
	}
	else // Out
	{
		int32 NumOfWidgets = TrackedWidgets.Num() - 1; // 0 index

		if (TempIndex == NumOfWidgets)
			return; // We're at the bottom of the stack: No widgets to go out to
		else if (TempIndex > NumOfWidgets)
		{
			CurrWidgetIndex = NumOfWidgets;
			return; // We shouldn't really ever get here, but for safety.
		}

		++TempIndex;
		while (!TrackedWidgets[TempIndex].IsValid())
		{
			TrackedWidgets.RemoveAt(TempIndex);

			NumOfWidgets = TrackedWidgets.Num();
			if (NumOfWidgets == 0)
			{
				CurrWidgetIndex = 0;
				return; // Empty Stack: No widgets to go out to
			}

			--NumOfWidgets; // 0 index
			if (TempIndex >= NumOfWidgets)
			{
				CurrWidgetIndex = NumOfWidgets;
				return; // Bottom of the stack: No widgets to go out to
			}

			// We've removed a widget, so the array shrinked.
			// We don't need to touch the TempIndex as it will now point to
			// a new entry (if it exists)
		}
	}

	// We've found a valid widget to go In-Out-to
	FTrackedInst*		 JumpToWidget = &TrackedWidgets[TempIndex];
	TSharedPtr<SWindow>	 TrackedWindow = JumpToWidget->ParentWindow.Pin();
	TSharedPtr<SDockTab> TrackedMajorTab = JumpToWidget->ParentMajorTab.Pin();
	TSharedPtr<SDockTab> TrackedMinorTab = JumpToWidget->ParentMinorTab.Pin();
	TSharedPtr<SWidget>	 WidgetPtr = JumpToWidget->Widget.Pin();
	bool				 bIsParentNomadTab = JumpToWidget->bIsParentNomadTab;

	Logger->Print(FString::Printf(TEXT("Valid Widget Found: %s | New Index: %d"),
					  *WidgetPtr->GetTypeAsString(), TempIndex),
		ELogVerbosity::Verbose, true);

	TSharedPtr<SWindow>	 ActiveWindow = SlateApp.GetActiveTopLevelRegularWindow();
	TSharedPtr<SDockTab> ActiveMajorTab = FUMSlateHelpers::GetActiveMajorTab();
	TSharedPtr<SDockTab> ActiveMinorTab = FUMSlateHelpers::GetActiveMinorTab();
	if (!(ActiveWindow.IsValid() && ActiveMajorTab.IsValid() && ActiveMinorTab.IsValid()))
		return;

	// Activate the window of the new widget if it's not the current
	if (ActiveWindow->GetId() != TrackedWindow->GetId())
		TrackedWindow->BringToFront(true);

	// Activate the Major Tab of the new widget if it's not the current
	if (ActiveMajorTab->GetId() != TrackedMajorTab->GetId())
		TrackedMajorTab->ActivateInParent(ETabActivationCause::SetDirectly);

	// Activate the Minor Tab of the new widget if it's not the current
	// + Make sure the parent Major Tab of the new widget isn't Nomad
	else if (!bIsParentNomadTab /** Minor tab is nullptr if parent Maj=Nomad */
		&& (ActiveMinorTab->GetId() != TrackedMinorTab->GetId()))
		TrackedMinorTab->ActivateInParent(ETabActivationCause::SetDirectly);

	bHasJumpedToNewWidget = true; /* Bypass registration for jumped-to-widget */
	SlateApp.SetAllUserFocus(WidgetPtr, EFocusCause::Navigation);
	CurrWidgetIndex = TempIndex; // Update index
}
//
//				~ FGraphSelectionTracker Implementation ~ //
///////////////////////////////////////////////////////////////////////////////

void UUMFocuserEditorSubsystem::BindVimCommands()
{
	TSharedRef<FVimInputProcessor> VimInputProcessor = FVimInputProcessor::Get();

	TWeakObjectPtr<UUMFocuserEditorSubsystem> WeakFocuserSubsystem =
		MakeWeakObjectPtr(this);

	// /* Jump List Navigation: In */
	// VimInputProcessor->AddKeyBinding_KeyEvent(
	// 	EUMBindingContext::Generic,
	// 	{ FInputChord(EModifierKey::Control, EKeys::I) },
	// 	WeakFocuserSubsystem,
	// 	// &FListNavigationManager::JumpListNavigation);
	// 	[this](FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) {

	// 	});

	// /* Jump List Navigation: Out */
	VimInputProcessor->AddKeyBinding_KeyEvent(
		EUMBindingContext::Generic,
		{ FInputChord(EModifierKey::Control, EKeys::I) },
		// WeakFocuserSubsystem,
		[this](FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) {
			ListNavigationManager.JumpListNavigation(SlateApp, InKeyEvent);
		});

	// /* Jump List Navigation: Out */
	VimInputProcessor->AddKeyBinding_KeyEvent(
		EUMBindingContext::Generic,
		{ FInputChord(EModifierKey::Control, EKeys::O) },
		// WeakFocuserSubsystem,
		[this](FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) {
			ListNavigationManager.JumpListNavigation(SlateApp, InKeyEvent);
		});
}
