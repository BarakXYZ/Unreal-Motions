#pragma once
#include "UMLogger.h"
#include "Framework/Application/SlateApplication.h"

class FUMFocusHelpers
{
public:
	static bool FocusNearestInteractableWidget(
		const TSharedRef<SWidget> StartWidget);

	/** Will clear the old timer handle before starting a new one to mitigate
	 * potential loops */
	static void SetWidgetFocusWithDelay(const TSharedRef<SWidget> InWidget,
		FTimerHandle& TimerHandle, const float Delay, const bool bClearUserFocus);

	static bool HandleWidgetExecution(FSlateApplication& SlateApp, const TSharedRef<SWidget> InWidget);

	static void TryFocusFuturePopupMenu(FSlateApplication& SlateApp, FTimerHandle& TimerHandle, const float Delay = 0.025f);

	static void LogWidgetType(const TSharedRef<SWidget> InWidget);

	static FUMLogger Logger;
};
