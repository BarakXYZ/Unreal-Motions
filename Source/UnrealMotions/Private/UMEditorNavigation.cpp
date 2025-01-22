#include "UMEditorNavigation.h"
#include "Framework/Docking/TabManager.h"
#include "Types/SlateEnums.h"
#include "UMInputHelpers.h"
#include "UMSlateHelpers.h"
#include "VimInputProcessor.h"
#include "Widgets/Docking/SDockTab.h"
#include "UMFocusVisualizer.h"

DEFINE_LOG_CATEGORY(LogUMEditorNavigation);
FUMLogger FUMEditorNavigation::Logger(&LogUMEditorNavigation);

void FUMEditorNavigation::NavigatePanelTabs(
	FSlateApplication& SlateApp, const FKeyEvent& InKey)
{
	// Get the desired navigation from the in Vim Key
	EUINavigation NavDirection;
	if (!FUMInputHelpers::GetUINavigationFromVimKey(InKey.GetKey(), NavDirection))
		return;

	FKey ArrowKey;
	if (!FUMInputHelpers::GetArrowKeyFromVimKey(InKey.GetKey(), ArrowKey))
		return;

	// Window to operate on: keep an eye of this. This should be aligned with
	// the currently focused window.
	const TSharedPtr<SWindow> Win = SlateApp.GetActiveTopLevelRegularWindow();
	if (!Win.IsValid())
		return;

	const TSharedRef<FGlobalTabmanager> GTM = FGlobalTabmanager::Get();
	const TSharedPtr<SDockTab>			ActiveMinorTab = GTM->GetActiveTab();
	if (!ActiveMinorTab.IsValid())
		return;
	const TSharedRef<SDockTab> MinorTabRef = ActiveMinorTab.ToSharedRef();

	// Getting the parent window of the minor tab directly seems to always
	// return an invalid parent window so fetching the window from SlateApp.
	// Anyway, this is our way to verify we only proceed actually focused
	// panel tabs, as Unreal official "Active Tab" tracking can be misaligned
	// with Nomad Tabs. We may need to actually check our Major Tab first and see
	// if it's a Nomad Tab to not even go this route.
	if (!FUMSlateHelpers::DoesTabResideInWindow(Win.ToSharedRef(), MinorTabRef))
		return;
	// Nomad tabs handling?

	TWeakPtr<SWidget> DockingTabStack;
	if (FUMSlateHelpers::GetParentDockingTabStackAsWidget(MinorTabRef, DockingTabStack))
	{
		const auto& PinDockingTab = DockingTabStack.Pin();
		if (!PinDockingTab.IsValid())
			return;

		// Bring focus to the Docking Tab Stack to achieve the built-in
		// panel navigation when it is focused.
		SlateApp.SetAllUserFocus(PinDockingTab, EFocusCause::Navigation);

		// For some reason the Editor Viewport wants arrows *~* try to mitigate
		if (ActiveMinorTab->GetLayoutIdentifier().ToString().Equals("LevelEditorViewport"))
		{
			FVimInputProcessor::SimulateKeyPress(SlateApp, ArrowKey);
		}
		else // Others does not want arrows! *~*
		{
			// Create a widget path for the active widget
			FWidgetPath WidgetPath;
			if (SlateApp.FindPathToWidget(
					// CurrMinorTab.ToSharedRef(), WidgetPath))
					PinDockingTab.ToSharedRef(), WidgetPath))
			{
				// Craft and process the event
				FReply Reply =
					FReply::Handled().SetNavigation(
						NavDirection,
						ENavigationGenesis::Keyboard,
						ENavigationSource::FocusedWidget);

				FSlateApplication::Get().ProcessReply(
					WidgetPath, // CurrentEventPath
					Reply,		// TheReply
					nullptr,	// WidgetsUnderMouse (not needed here)
					nullptr,	// InMouseEvent (optional)
					0			// UserIndex
				);
			}
		}
	}
}

void FUMEditorNavigation::NavigateNomadSplitterChildrens(
	FSlateApplication& SlateApp, const FKeyEvent& InKey)
{
	// Get the desired navigation from the in Vim Key
	EUINavigation NavDirection;
	if (!FUMInputHelpers::GetUINavigationFromVimKey(InKey.GetKey(), NavDirection))
		return;

	FKey ArrowKey;
	if (!FUMInputHelpers::GetArrowKeyFromVimKey(InKey.GetKey(), ArrowKey))
		return;

	const TSharedPtr<SDockTab> ActiveMajorTab = FUMSlateHelpers::GetActiveMajorTab();
	if (!ActiveMajorTab.IsValid())
		return;
	if (ActiveMajorTab->GetTabRole() == ETabRole::NomadTab)
	{
		// Logger.Print("Nomad Tab when trying to navigate panels",
		// 	ELogVerbosity::Log, true);

		TArray<TSharedPtr<SWidget>> FoundSplitters;
		if (!FUMSlateHelpers::TraverseWidgetTree(
				// ActiveMajorTab->GetContent(), FoundSplitters, "SSplitter"))
				ActiveMajorTab->GetContent(), FoundSplitters, "SSplitter"))
			return;

		// Filter Splitters:
		// We want to end up with only terminal splitters (leaf).
		// Parent splitters aren't useful because we really want the splitters
		// they contain.
		for (const auto& Splitter : FoundSplitters)
		{
		}

		for (int32 i{ 0 }; i < FoundSplitters.Num(); ++i)
		{
			if (!FoundSplitters.IsValidIndex(i + 1))
				break;

			// FUMSlateHelpers::DoesWidgetResideInWidget(FoundSplitters[i], FoundSplitters[i + 1]);
			// if (FoundSplitters[i])
		}

		Logger.Print(FString::Printf(TEXT("Found %d Splitters in Nomad Tab"),
						 FoundSplitters.Num()),
			ELogVerbosity::Log, true);

		if (FoundSplitters.Num() < 3)
			return;

		FUMFocusVisualizer::Get()->DrawDebugOutlineOnWidget(
			FoundSplitters[2].ToSharedRef());

		// SlateApp.SetAllUserFocus(FoundSplitters[2], EFocusCause::Navigation);
	}
}
