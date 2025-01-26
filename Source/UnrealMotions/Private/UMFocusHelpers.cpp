#include "UMFocusHelpers.h"
#include "Editor.h"
#include "Input/Events.h"
#include "Logging/LogVerbosity.h"
#include "UMSlateHelpers.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Docking/SDockTab.h"

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
	TMap<FString, TFunction<void(FSlateApplication&, TSharedRef<SWidget>)>> WidgetExecMap = {

		{ "SButton", [](FSlateApplication& SlateApp, TSharedRef<SWidget> InWidget) {
			 TSharedRef<SButton> AsButton = StaticCastSharedRef<SButton>(InWidget);
			 AsButton->SimulateClick();

			 // TODO: Check if it popped a menu that we can focus?
			 FTimerHandle TimerHandle;
			 TryFocusFuturePopupMenu(SlateApp, TimerHandle, 0.025f);
		 } },

		{ "SCheckBox", [](FSlateApplication& SlateApp, TSharedRef<SWidget> InWidget) {
			 SlateApp.SetAllUserFocus(InWidget, EFocusCause::Navigation);
			 TSharedRef<SCheckBox> AsCheckBox = StaticCastSharedRef<SCheckBox>(InWidget);
			 AsCheckBox->ToggleCheckedState();

			 // TODO: Check if it popped a menu that we can focus?
			 FTimerHandle TimerHandle;
			 TryFocusFuturePopupMenu(SlateApp, TimerHandle, 0.025f);
		 } },

		{ "SDockTab", [](FSlateApplication& SlateApp, TSharedRef<SWidget> InWidget) {
			 TSharedRef<SDockTab> AsDockTab = StaticCastSharedRef<SDockTab>(InWidget);
			 AsDockTab->ActivateInParent(ETabActivationCause::SetDirectly);
		 } },

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

	// Generic set focus
	SlateApp.SetAllUserFocus(InWidget, EFocusCause::Navigation);
	return false;
}

void FUMFocusHelpers::TryFocusFuturePopupMenu(FSlateApplication& SlateApp,
	FTimerHandle& TimerHandle, const float Delay)
{
	TSharedRef<FTimerManager> TimerManager = GEditor->GetTimerManager();
	TimerManager->ClearTimer(TimerHandle);

	TimerManager->SetTimer(
		TimerHandle,
		[&SlateApp]() {
			const TSharedPtr<SWindow> ActiveWindow = SlateApp.GetActiveTopLevelWindow();

			const TArray<TSharedRef<SWindow>> Wins = ActiveWindow->GetChildWindows();

			if (Wins.IsEmpty())
				return;

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
					// Logger.Print(FString::Printf(TEXT("Child 0 Type is: %s"),
					// 				 *FirstChild->GetTypeAsString()),
					// 	ELogVerbosity::Log, true);
					continue;
				}

				// Logger.Print("Menu Window found!",
				// 	ELogVerbosity::Verbose, true);

				// Lookup the first button in that menu and pull focus on it.
				TSharedPtr<SWidget> FoundButton;
				if (FUMSlateHelpers::TraverseFindWidget(
						Win->GetContent(), FoundButton, "SButton"))
				{
					// SlateApp.ClearAllUserFocus();
					SlateApp.SetAllUserFocus(FoundButton, EFocusCause::Navigation);
				}
				return;
			}
			// Logger.Print("Menu Window was NOT found!",
			// 	ELogVerbosity::Log);
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
