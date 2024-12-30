#include "VimEditorSubsystem.h"
#include "Brushes/SlateColorBrush.h"
#include "Chaos/Interface/PhysicsInterfaceWrapperShared.h"
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

DEFINE_LOG_CATEGORY_STATIC(LogVimEditorSubsystem, Log, All); // Development
static constexpr int32 MIN_REPEAT_COUNT = 1;
static constexpr int32 MAX_REPEAT_COUNT = 999;

TMap<FKey, FKey> UVimEditorSubsystem::VimToArrowKeys = {
	{ EKeys::H, FKey(EKeys::Left) }, { EKeys::J, FKey(EKeys::Down) },
	{ EKeys::K, FKey(EKeys::Up) }, { EKeys::L, FKey(EKeys::Right) }
};

void UVimEditorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	const FConfigFile& ConfigFile = FUMHelpers::ConfigFile;
	FString			   OutLog = "Vim Editor Subsystem Initialized: ";

	FString TestGetStr;
	bool	bStartVim = true;

	if (!ConfigFile.IsEmpty())
	{
		ConfigFile.GetBool(*FUMHelpers::VimSection, TEXT("bStartVim"), bStartVim);

		// TODO: Remove to a more general place? Debug
		ConfigFile.GetBool(*FUMHelpers::DebugSection, TEXT("bVisualLog"), bVisualLog);
	}

	ToggleVim(bStartVim);
	OutLog += bStartVim ? "Enabled by Config." : "Disabled by Config.";
	FUMHelpers::NotifySuccess(FText::FromString(OutLog), bVisualLog);

	VimSubWeak = this;
	InputPP = FUMInputPreProcessor::Get().ToWeakPtr();
	BindCommands();
	Super::Initialize(Collection);
}

void UVimEditorSubsystem::Deinitialize()
{
	ToggleVim(false);
	Super::Deinitialize();
}

void UVimEditorSubsystem::OnResetSequence()
{
	CountBuffer.Empty();
}
void UVimEditorSubsystem::OnCountPrefix(FString AddedCount)
{
	CountBuffer += AddedCount; // Build the buffer: 1 + 7 + 3 (173);
}

void UVimEditorSubsystem::ToggleVim(bool bEnable)
{
	FString OutLog = bEnable ? "Start Vim: " : "Stop Vim: ";

	if (FSlateApplication::IsInitialized())
	{
		if (bEnable)
		{
			// TODO:
		}
		else
		{
			if (PreInputKeyDownDelegateHandle.IsValid())
			{
				FSlateApplication::Get().OnApplicationPreInputKeyDownListener().Remove(PreInputKeyDownDelegateHandle);
				PreInputKeyDownDelegateHandle.Reset(); // Clear the handle to avoid reuse
				OutLog += "Delegate resetted successfully.";
			}
			else
				OutLog += "Delegate wasn't valid / bound. Skipping.";
		}
	}

	FUMHelpers::NotifySuccess(FText::FromString(OutLog), bVisualLog);
}

bool UVimEditorSubsystem::MapVimToArrowNavigation(
	const FKeyEvent& InKeyEvent, FKeyEvent& OutKeyEvent)
{
	const FKey* MappedKey = VimToArrowKeys.Find(InKeyEvent.GetKey());
	if (!MappedKey)
		return false;
	OutKeyEvent = FKeyEvent(
		*MappedKey,
		InKeyEvent.GetModifierKeys(),
		0,	   // User index
		false, // Is repeat
		0,	   // Character code
		0	   // Key code
	);
	return true;
}

void UVimEditorSubsystem::ProcessVimNavigationInput(
	FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
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

void UVimEditorSubsystem::DeleteItem(
	FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	// EKeys::Delete;
	FKeyEvent DeleteEvent(
		FKey(EKeys::Delete),
		FModifierKeysState(),
		0, 0, 0, 0);
	FUMInputPreProcessor::ToggleNativeInputHandling(true);
	// SlateApp.ProcessKeyDownEvent(DeleteEvent);

	// UWorld* EditorWorld = nullptr;

	// // Example: Get the “editor world context” if one is available.
	// if (GEditor)
	// {
	// 	FUMHelpers::NotifySuccess(FText::FromString("GEDITOR"));
	// 	// This returns the first world used by the Editor (the level you see open).
	// 	FWorldContext& EditorWorldContext = GEditor->GetEditorWorldContext();
	// 	EditorWorld = EditorWorldContext.World();
	// }

	// if (EditorWorld)
	// {
	// 	FUMHelpers::NotifySuccess(FText::FromString("EditorWorld"));
	// 	FTimerHandle TimerHandle;
	// 	EditorWorld->GetTimerManager().SetTimer(
	// 		TimerHandle,
	// 		FTimerDelegate::CreateLambda([this, &SlateApp]() {
	// 			// This is your timer callback, runs once after 3 seconds in the Editor world
	// 			SimulateMultipleTabEvent(SlateApp, 2);
	// 		}),
	// 		3.0f, // Delay
	// 		false // bLoop
	// 	);
	// }
	SlateApp.ProcessKeyDownEvent(DeleteEvent); // Will just block the entire process until the delete window is handled, thus not really helping.
}

void UVimEditorSubsystem::OpenWidgetReflector(
	FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	static const FName REFLECTOR = "WidgetReflector";

	TSharedPtr<SDockTab> RefTab = // Invoke the Widget Reflector tab
		FGlobalTabmanager::Get()->TryInvokeTab(FTabId(REFLECTOR));
	ActivateNewInvokedTab(FSlateApplication::Get(), RefTab); // For proper focus

	// FGlobalTabmanager::Get()->GetTabManagerForMajorTab(RefTab)->GetPrivateApi().GetLiveDockAreas();
	TSharedPtr<SWindow> RefWin = RefTab->GetParentWindow();
	if (!RefWin)
		return;

	// Fetching the first (maybe only) instance of this (parent of our target)
	TWeakPtr<SWidget> FoundWidget;
	if (!FUMTabsNavigationManager::TraverseWidgetTree(
			RefWin, FoundWidget, "SToolBarButtonBlock"))
		return;

	// Locate the button within it
	if (FUMTabsNavigationManager::TraverseWidgetTree(
			FoundWidget.Pin(), FoundWidget, "SButton"))
	{
		TSharedPtr<SButton> Button =
			StaticCastSharedPtr<SButton>(FoundWidget.Pin());
		if (!Button)
			return;
		Button->SimulateClick(); // Will spawn a fetchable Menu Box (&& SWindow)

		// Get this window so we can search for the final buttons within it
		TArray<TSharedRef<SWindow>> ChildRefWins = RefWin->GetChildWindows();
		if (ChildRefWins.Num() == 0)
			return;

		// Remove the annoying SearchBox that is taking focus from HJKL
		if (FUMTabsNavigationManager::TraverseWidgetTree(
				ChildRefWins[0], FoundWidget, "SSearchBox"))
		{
			// Won't work
			// TSharedPtr<SHorizontalBox> ParentBox =
			// 	StaticCastSharedPtr<SHorizontalBox>(
			// 		FoundWidget.Pin()->GetParentWidget());
			// if (ParentBox)
			// {
			// 	// ParentBox->RemoveSlot(FoundWidget.Pin().ToSharedRef());
			// 	// ParentBox->SetEnabled(false);
			// }

			// Works but still catch 1 key!
			TSharedPtr<SSearchBox> SearchBox =
				StaticCastSharedPtr<SSearchBox>(FoundWidget.Pin());

			// Maybe can try to clear the input from a reference?
			// The problem is that it gets the characters from somewhere else
			// the actual handling does work but the first key will always be
			// received because it's being sent from outside.
			// Need to understand from where it's getting the input.

			SearchBox->SetOnKeyCharHandler(
				FOnKeyChar::CreateUObject(
					this, &UVimEditorSubsystem::HandleDummyKeyChar));

			SearchBox->SetOnKeyDownHandler(
				FOnKeyDown::CreateUObject(
					this, &UVimEditorSubsystem::HandleDummyKeyDown));
		}

		// Find all menu entry buttons in the found window
		TArray<TWeakPtr<SWidget>> FoundButtons;
		if (!FUMTabsNavigationManager::TraverseWidgetTree(
				ChildRefWins[0], FoundButtons, "SMenuEntryButton"))
			return;

		if (FoundButtons.Num() == 0 || !FoundButtons[1].IsValid())
			return;
		TSharedPtr<SButton> EntryButton = // SMenuEntry inherits from SButton
			StaticCastSharedPtr<SButton>(FoundButtons[1].Pin());
		if (!EntryButton.IsValid())
			return;

		bool bIsImmediatelyTriggered = InKeyEvent.IsShiftDown();

		// We need a slight delay before drawing focus to this new button
		FTimerHandle TimerHandle;
		GEditor->GetTimerManager()->SetTimer(
			TimerHandle,
			[EntryButton, bIsImmediatelyTriggered]() {
				FSlateApplication& SlateApp = FSlateApplication::Get();
				SlateApp.ClearAllUserFocus();
				if (bIsImmediatelyTriggered)
					EntryButton->SimulateClick();
				// TODO: This is fragile - let's add a manual focus visualizer
				SlateApp.SetAllUserFocus(EntryButton, EFocusCause::Navigation);
			},
			0.01f, // 10ms delay seems to be enough (need to check on mac)
			false  // Do not loop
		);
		FUMHelpers::NotifySuccess(FText::AsNumber(FoundButtons.Num()));
	}
}

void UVimEditorSubsystem::OpenOutputLog()
{
	static const FName LOG = "OutputLog";
	ActivateNewInvokedTab(FSlateApplication::Get(),
		FGlobalTabmanager::Get()->TryInvokeTab(FTabId(LOG)));
}

void UVimEditorSubsystem::OpenContentBrowser(
	FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	static FString DEFAULT_TAB_INDEX = "1";
	FString		   ContentBrowserId = "ContentBrowserTab";
	FString		   TabNum;
	if (FUMInputPreProcessor::GetStrDigitFromKey(
			InKeyEvent.GetKey(), TabNum, 1, 4)) // Valid tabs are only 1-4
		ContentBrowserId += TabNum;
	else
		ContentBrowserId += DEFAULT_TAB_INDEX;

	ActivateNewInvokedTab(SlateApp,
		FGlobalTabmanager::Get()->TryInvokeTab(FName(*ContentBrowserId)));
}

void UVimEditorSubsystem::ActivateNewInvokedTab(
	FSlateApplication& SlateApp, const TSharedPtr<SDockTab> NewTab)
{
	SlateApp.ClearAllUserFocus(); // NOTE: In order to actually draw focus

	if (TSharedPtr<SWindow> Win = NewTab->GetParentWindow())
		FUMWindowsNavigationManager::ActivateWindow(Win.ToSharedRef());
	NewTab->ActivateInParent(ETabActivationCause::SetDirectly);
}

void UVimEditorSubsystem::RemoveActiveMajorTab()
{
	if (!FUMTabsNavigationManager::RemoveActiveMajorTab())
		return;
	FUMWindowsNavigationManager::FocusNextFrontmostWindow();
}

void UVimEditorSubsystem::BindCommands()
{
	using VimSub = UVimEditorSubsystem;

	if (!InputPP.IsValid())
		return;
	FUMInputPreProcessor& Input = *InputPP.Pin();

	// Listeners
	Input.OnResetSequence.AddUObject(
		this, &VimSub::OnResetSequence);

	Input.OnCountPrefix.AddUObject(
		this, &VimSub::OnCountPrefix);

	// ~ Commands ~ //
	//
	// Delete item - Simulate the Delete key (WIP)
	Input.AddKeyBinding_KeyEvent(
		{ EKeys::X },
		VimSubWeak, &VimSub::DeleteItem);

	Input.AddKeyBinding_KeyEvent(
		{ EKeys::SpaceBar, EKeys::O, EKeys::W },
		VimSubWeak, &VimSub::OpenWidgetReflector);

	Input.AddKeyBinding_KeyEvent(
		{ EKeys::SpaceBar, EKeys::O, FInputChord(EModifierKey::Shift, EKeys::W) },
		VimSubWeak, &VimSub::OpenWidgetReflector);

	Input.AddKeyBinding_NoParam(
		{ EKeys::SpaceBar, EKeys::O, EKeys::O },
		VimSubWeak, &VimSub::OpenOutputLog);

	//** Open Content Browsers 1-4 */
	Input.AddKeyBinding_KeyEvent(
		{ EKeys::SpaceBar, EKeys::O, EKeys::C, EKeys::One },
		VimSubWeak, &VimSub::OpenContentBrowser);
	Input.AddKeyBinding_KeyEvent(
		{ EKeys::SpaceBar, EKeys::O, EKeys::C, EKeys::Two },
		VimSubWeak, &VimSub::OpenContentBrowser);
	Input.AddKeyBinding_KeyEvent(
		{ EKeys::SpaceBar, EKeys::O, EKeys::C, EKeys::Three },
		VimSubWeak, &VimSub::OpenContentBrowser);
	Input.AddKeyBinding_KeyEvent(
		{ EKeys::SpaceBar, EKeys::O, EKeys::C, EKeys::Four },
		VimSubWeak, &VimSub::OpenContentBrowser);

	Input.AddKeyBinding_NoParam(
		{ FInputChord(EModifierKey::Control, EKeys::W) },
		VimSubWeak, &VimSub::RemoveActiveMajorTab);

	//  Move HJKL
	Input.AddKeyBinding_KeyEvent(
		{ EKeys::H },
		VimSubWeak, &VimSub::ProcessVimNavigationInput);
	Input.AddKeyBinding_KeyEvent(
		{ EKeys::J },
		VimSubWeak, &VimSub::ProcessVimNavigationInput);
	Input.AddKeyBinding_KeyEvent(
		{ EKeys::K },
		VimSubWeak, &VimSub::ProcessVimNavigationInput);
	Input.AddKeyBinding_KeyEvent(
		{ EKeys::L },
		VimSubWeak, &VimSub::ProcessVimNavigationInput);

	// Selection + Move HJKL
	Input.AddKeyBinding_KeyEvent(
		{ FInputChord(EModifierKey::Shift, EKeys::H) },
		VimSubWeak, &VimSub::ProcessVimNavigationInput);
	Input.AddKeyBinding_KeyEvent(
		{ FInputChord(EModifierKey::Shift, EKeys::J) },
		VimSubWeak, &VimSub::ProcessVimNavigationInput);
	Input.AddKeyBinding_KeyEvent(
		{ FInputChord(EModifierKey::Shift, EKeys::K) },
		VimSubWeak, &VimSub::ProcessVimNavigationInput);
	Input.AddKeyBinding_KeyEvent(
		{ FInputChord(EModifierKey::Shift, EKeys::L) },
		VimSubWeak, &VimSub::ProcessVimNavigationInput);
}
