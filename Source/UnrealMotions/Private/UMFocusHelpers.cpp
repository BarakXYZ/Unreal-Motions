#include "UMFocusHelpers.h"
#include "Editor.h"
#include "UMSlateHelpers.h"

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
