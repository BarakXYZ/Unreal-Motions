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

	static FUMLogger Logger;
};
