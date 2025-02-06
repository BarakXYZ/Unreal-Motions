#include "UMFocusHelpers.h"
#include "Editor.h"
#include "Input/Events.h"
#include "Logging/LogVerbosity.h"
#include "SGraphNode.h"
#include "SGraphPin.h"
#include "SGraphPanel.h"
#include "EdGraph/EdGraphNode.h"
#include "Templates/SharedPointer.h"
#include "UMInputHelpers.h"
#include "UMSlateHelpers.h"
#include "VimGraphEditorSubsystem.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Docking/SDockTab.h"
#include "VimInputProcessor.h"
#include "VimGraphEditorSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogUMFocusHelpers, Log, All); // Dev
FUMLogger FUMFocusHelpers::Logger(&LogUMFocusHelpers);

bool FUMFocusHelpers::FocusNearestInteractableWidget(
	const TSharedRef<SWidget> StartWidget)
{
	uint64				IgnoreWidgetId = StartWidget->GetId();
	TSharedPtr<SWidget> FoundWidget;
	TSharedPtr<SWidget> Parent = StartWidget->GetParentWidget();

	while (Parent.IsValid())
	{
		if (FUMSlateHelpers::TraverseFindWidget(Parent.ToSharedRef(), FoundWidget,
				FUMSlateHelpers::GetInteractableWidgetTypes(), IgnoreWidgetId))
		{
			FTimerHandle TimerHandle;
			SetWidgetFocusWithDelay(FoundWidget.ToSharedRef(),
				TimerHandle, 0.025f, true);
			// FSlateApplication::Get().SetAllUserFocus(
			// 	FoundWidget, EFocusCause::Navigation);
			return true;
		}

		// Update the ignore parent ID, as we've just traversed all its children.
		IgnoreWidgetId = Parent->GetId();
		Parent = Parent->GetParentWidget(); // Climb up to the next parent
	}
	return false;
}

void FUMFocusHelpers::SetWidgetFocusWithDelay(const TSharedRef<SWidget> InWidget,
	FTimerHandle& TimerHandle, const float Delay, const bool bClearUserFocus)
{
	TSharedRef<FTimerManager> TimerManager = GEditor->GetTimerManager();
	TimerManager->ClearTimer(TimerHandle);
	TWeakPtr<SWidget> WeakWidget = InWidget;

	TimerManager->SetTimer(
		TimerHandle,
		[WeakWidget, bClearUserFocus]() {
			if (const TSharedPtr<SWidget> Widget = WeakWidget.Pin())
			{
				FSlateApplication& SlateApp = FSlateApplication::Get();
				if (bClearUserFocus)
					SlateApp.ClearAllUserFocus();

				SlateApp.SetAllUserFocus(Widget, EFocusCause::Navigation);
			}
		},
		Delay,
		false);
}

bool FUMFocusHelpers::HandleWidgetExecution(FSlateApplication& SlateApp, const TSharedRef<SWidget> InWidget)
{
	const TMap<FString, TFunction<void(FSlateApplication&, TSharedRef<SWidget>)>> WidgetExecMap = {
		{ "SButton", &FUMFocusHelpers::ClickSButton },
		{ "SCheckBox", &FUMFocusHelpers::ClickSCheckBox },
		{ "SPropertyValueWidget", &FUMFocusHelpers::ClickSPropertyValueWidget },
		{ "SDockTab", &FUMFocusHelpers::ClickSDockTab },
		{ "SGraphNodeK2Event", &FUMFocusHelpers::ClickSNodeSPin },
		{ "SGraphNodeK2Default", &FUMFocusHelpers::ClickSNodeSPin },
		{ "SLevelOfDetailBranchNode", &FUMFocusHelpers::ClickSNodeSPin },
	};

	// SlateApp.SetAllUserFocus(InWidget, EFocusCause::Navigation);

	const FString ClassType = InWidget->GetWidgetClass().GetWidgetType().ToString();
	const FString WidgetType = InWidget->GetTypeAsString();
	LogWidgetType(InWidget);

	if (auto* ExecFuncClass = WidgetExecMap.Find(ClassType))
	{
		(*ExecFuncClass)(SlateApp, InWidget);
		return true;
	}
	else if (auto* ExecFuncWidget = WidgetExecMap.Find(WidgetType))
	{
		(*ExecFuncWidget)(SlateApp, InWidget);
		return true;
	}
	// TODO: Refactor to an array of startswith? or send over the
	// SLevelOfDetailBranchNode from the caller?
	else if (WidgetType.StartsWith("SGraphPin"))
	{
		ClickSPin(SlateApp, InWidget);
		return true;
	}

	// Generic set focus & press
	SlateApp.SetAllUserFocus(InWidget, EFocusCause::Navigation);
	FVimInputProcessor::Get()->SimulateKeyPress(SlateApp, FKey(EKeys::Enter));
	return false;
}

void FUMFocusHelpers::HandleWidgetExecutionWithDelay(FSlateApplication& SlateApp, const TSharedRef<SWidget> InWidget, FTimerHandle* TimerHandle, const float Delay)
{
	TSharedRef<FTimerManager> TimerManager = GEditor->GetTimerManager();
	FTimerHandle			  LocalTimerHandle;
	FTimerHandle&			  TimerHandleRef =
		TimerHandle ? *TimerHandle : LocalTimerHandle;

	if (TimerHandle)
		TimerManager->ClearTimer(TimerHandleRef);

	TWeakPtr<SWidget> WeakWidget = InWidget;

	TimerManager->SetTimer(
		TimerHandleRef,
		[&SlateApp, WeakWidget]() {
			if (const TSharedPtr<SWidget> Widget = WeakWidget.Pin())
				HandleWidgetExecution(SlateApp, Widget.ToSharedRef());
		},
		Delay, false);
}

void FUMFocusHelpers::ClickSButton(FSlateApplication& SlateApp, const TSharedRef<SWidget> InWidget)
{
	// Pulling focus first to trigger widget registration
	SlateApp.SetAllUserFocus(InWidget, EFocusCause::Navigation);
	TSharedRef<SButton> AsButton = StaticCastSharedRef<SButton>(InWidget);
	AsButton->SimulateClick();

	// TODO: Check if it popped a menu that we can focus?
	if (const TSharedPtr<SWindow> ActiveWin = SlateApp.FindWidgetWindow(InWidget))
	{
		if (ActiveWin->IsRegularWindow())
			TryFocusFuturePopupMenu(SlateApp);
		else
		{
			TryFocusFutureSubMenu(SlateApp, ActiveWin.ToSharedRef());
		}
	}
}

void FUMFocusHelpers::ClickSCheckBox(FSlateApplication& SlateApp, const TSharedRef<SWidget> InWidget)
{
	SlateApp.SetAllUserFocus(InWidget, EFocusCause::Navigation);
	TSharedRef<SCheckBox> AsCheckBox = StaticCastSharedRef<SCheckBox>(InWidget);
	AsCheckBox->ToggleCheckedState();

	TryFocusFuturePopupMenu(SlateApp);
}

void FUMFocusHelpers::ClickSDockTab(FSlateApplication& SlateApp, const TSharedRef<SWidget> InWidget)
{
	TSharedRef<SDockTab> AsDockTab = StaticCastSharedRef<SDockTab>(InWidget);
	AsDockTab->ActivateInParent(ETabActivationCause::SetDirectly);
}

void FUMFocusHelpers::ClickSPropertyValueWidget(FSlateApplication& SlateApp, const TSharedRef<SWidget> InWidget)
{
	TSharedPtr<SWidget> FoundButton;
	// Sometimes they might have an SButton inside that we should operate on
	if (FUMSlateHelpers::TraverseFindWidget(InWidget, FoundButton, "SButton"))
	{
		ClickSButton(SlateApp, FoundButton.ToSharedRef());
		return;
	}
	FUMInputHelpers::SimulateClickOnWidget(SlateApp, InWidget, FKey(EKeys::LeftMouseButton));
}

void FUMFocusHelpers::ClickSNodeSPin(FSlateApplication& SlateApp, const TSharedRef<SWidget> InWidget)
{
	static const TArray<FString> NodeAndPinTypes = { "SGraphPin", "SGraphNode" };
	// We need to skip the first parent, as it shares both widgets (Pin & Node):
	const TSharedPtr<SWidget> BaseWidget = InWidget->GetParentWidget();
	if (!BaseWidget.IsValid())
		return;
	const TSharedRef<SWidget> BaseRef = BaseWidget.ToSharedRef();

	// Search for both types and get the nearest one. Whichever is found first
	// is the actual intended target widget.
	TSharedPtr<SWidget> FoundWidget;
	if (FUMSlateHelpers::TraverseFindWidgetUpwards(BaseRef, FoundWidget, NodeAndPinTypes))
	{
		// Logger.Print("Found either SGraphPin | SGraphNode",
		// 	ELogVerbosity::Log, true);

		if (FoundWidget->GetTypeAsString().StartsWith("SGraphPin"))
			ClickSPin(SlateApp, FoundWidget.ToSharedRef());
		else
			ClickSNode(SlateApp, FoundWidget.ToSharedRef());
	}
}

void FUMFocusHelpers::ClickSNode(FSlateApplication& SlateApp, const TSharedRef<SWidget> InWidget)
{
	Logger.Print("Found SGraphNode", ELogVerbosity::Log, true);

	TSharedRef<SGraphNode> AsGraphNode = // We can safely cast to Base SGraphNode
		StaticCastSharedRef<SGraphNode>(InWidget);

	UEdGraphNode* AsNodeObj = AsGraphNode->GetNodeObj();
	if (!AsNodeObj)
		return;

	const TSharedPtr<SGraphPanel> GraphPanel = AsGraphNode->GetOwnerPanel();
	if (!GraphPanel.IsValid())
		return;

	// We wanna focus the Graph first to draw focus to the entire Minor Tab
	// (just selecting the Node is not enough)
	SlateApp.SetAllUserFocus(GraphPanel, EFocusCause::Navigation);
	GraphPanel->SelectionManager.SelectSingleNode(AsNodeObj);
}

void FUMFocusHelpers::ClickSPin(FSlateApplication& SlateApp, const TSharedRef<SWidget> InWidget)
{
	Logger.Print("Found SGraphPin", ELogVerbosity::Log, true);

	TSharedRef<SGraphPin> GraphPin = StaticCastSharedRef<SGraphPin>(InWidget);
	if (UVimGraphEditorSubsystem* GraphSub =
			GEditor->GetEditorSubsystem<UVimGraphEditorSubsystem>())
		GraphSub->AddNodeToPin(SlateApp, GraphPin);
}

bool FUMFocusHelpers::TryFocusPopupMenu(FSlateApplication& SlateApp)
{
	// const TSharedPtr<SWindow> ActiveWindow = SlateApp.GetActiveTopLevelWindow();
	const TSharedPtr<SWindow> ActiveWindow = SlateApp.GetActiveTopLevelRegularWindow();

	if (!ActiveWindow.IsValid())
		return false;

	const TArray<TSharedRef<SWindow>> Wins = ActiveWindow->GetChildWindows();

	if (Wins.IsEmpty())
		return false;

	for (const auto& Win : Wins)
	{
		FChildren* Children = Win->GetChildren();
		if (!Children || Children->Num() == 0)
		{
			continue;
		}

		const TSharedRef<SWidget> FirstChild = Children->GetChildAt(0);
		if (!FirstChild->GetTypeAsString().Equals(
				"MenuStackInternal::SMenuContentWrapper"))
		{
			continue;
		}

		// const TArray<TSharedRef<SWindow>> DeeperChildWins = Win->GetChildWindows();
		// if (!DeeperChildWins.IsEmpty())
		// {
		// 	Logger.Print("Menu Window also has Childrens!", ELogVerbosity::Log, true);
		// }

		BringFocusToPopupMenu(SlateApp, Win->GetContent());

		Logger.Print("Menu Window found!",
			ELogVerbosity::Verbose, true);
		return true;
	}

	Logger.Print("Menu Window was NOT found!",
		ELogVerbosity::Warning, true);
	return false;
}

void FUMFocusHelpers::TryFocusFuturePopupMenu(FSlateApplication& SlateApp,
	FTimerHandle* TimerHandle, const float Delay)
{
	TSharedRef<FTimerManager> TimerManager = GEditor->GetTimerManager();
	FTimerHandle			  LocalTimerHandle;
	FTimerHandle&			  TimerHandleRef =
		TimerHandle ? *TimerHandle : LocalTimerHandle;

	if (TimerHandle)
		TimerManager->ClearTimer(TimerHandleRef);

	TimerManager->SetTimer(
		TimerHandleRef,
		[&SlateApp]() {
			TryFocusPopupMenu(SlateApp);
		},
		Delay, false);
}

bool FUMFocusHelpers::BringFocusToPopupMenu(FSlateApplication& SlateApp, TSharedRef<SWidget> WinContent)
{
	// Lookup the first focusable widget in that menu (WIP)
	TSet<FString> LookUpTypes = {
		"SButton",

		// This seems to work and be more generic I think?
		"SComboListType",

		// TODO:
		// This seems to really just ignore any type of "selection" support.
		// Can't get it to receive non-mouse navigation in any why shape or form.
		// Have tried to even focus on the inner border and stuff, nothing works.
		// So potentially try to send a pull-request to UE to fix this?
		"SPlacementAssetMenuEntry",
	};
	TSharedPtr<SWidget> FoundWidget;
	if (FUMSlateHelpers::TraverseFindWidget(
			WinContent, FoundWidget, LookUpTypes))
	{
		// Clearing seems to help for when jumping from an EditableT
		// SlateApp.ClearAllUserFocus();
		SlateApp.SetAllUserFocus(FoundWidget, EFocusCause::Navigation);

		Logger.Print(FString::Printf(TEXT("Widget In Menu Window found: %s"), *FoundWidget->GetTypeAsString()),
			ELogVerbosity::Verbose, true);

		return true;
	}
	return false;
}

bool FUMFocusHelpers::TryFocusSubMenu(FSlateApplication& SlateApp, const TSharedRef<SWindow> ParentMenuWindow)
{
	const TArray<TSharedRef<SWindow>> ChildWins = ParentMenuWindow->GetChildWindows();
	if (ChildWins.IsEmpty())
		return false;

	// We really just want to first window
	return BringFocusToPopupMenu(SlateApp, ChildWins[0]->GetContent());
}

void FUMFocusHelpers::TryFocusFutureSubMenu(FSlateApplication& SlateApp, const TSharedRef<SWindow> ParentMenuWindow, FTimerHandle* TimerHandle, const float Delay)
{
	TSharedRef<FTimerManager> TimerManager = GEditor->GetTimerManager();
	FTimerHandle			  LocalTimerHandle;
	FTimerHandle&			  TimerHandleRef =
		TimerHandle ? *TimerHandle : LocalTimerHandle;

	if (TimerHandle)
		TimerManager->ClearTimer(TimerHandleRef);

	const TWeakPtr<SWindow> WeakParentMenuWin = ParentMenuWindow;

	TimerManager->SetTimer(
		TimerHandleRef,
		[&SlateApp, WeakParentMenuWin]() {
			if (const TSharedPtr<SWindow> ParentWin = WeakParentMenuWin.Pin())
				TryFocusSubMenu(SlateApp, ParentWin.ToSharedRef());
		},
		Delay, false);
}

void FUMFocusHelpers::LogWidgetType(const TSharedRef<SWidget> InWidget)
{
	const FString ClassType = InWidget->GetWidgetClass().GetWidgetType().ToString();
	const FString WidgetType = InWidget->GetTypeAsString();

	Logger.Print(FString::Printf(TEXT("Widget Class Type: %s | Specific: %s"),
					 *ClassType, *WidgetType),
		ELogVerbosity::Log, true);
}
