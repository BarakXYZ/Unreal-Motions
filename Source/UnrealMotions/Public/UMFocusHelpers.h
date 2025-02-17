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
	static void HandleWidgetExecutionWithDelay(FSlateApplication& SlateApp, const TSharedRef<SWidget> InWidget, FTimerHandle* TimerHandle = nullptr, const float Delay = 0.015f);

	static bool TryFocusPopupMenu(FSlateApplication& SlateApp);
	static void TryFocusFuturePopupMenu(FSlateApplication& SlateApp, FTimerHandle* TimerHandle = nullptr, const float Delay = 0.15f);

	static bool BringFocusToPopupMenu(FSlateApplication& SlateApp, TSharedRef<SWidget> WinContent);

	static bool TryFocusSubMenu(FSlateApplication& SlateApp, const TSharedRef<SWindow> ParentMenuWindow);
	static void TryFocusFutureSubMenu(FSlateApplication& SlateApp, const TSharedRef<SWindow> ParentMenuWindow, FTimerHandle* TimerHandle = nullptr, const float Delay = 0.15f);

	static void LogWidgetType(const TSharedRef<SWidget> InWidget);

	static void ClickOnWidget(FSlateApplication& SlateApp, const TSharedRef<SWidget> InWidget);
	static void ClickSButton(FSlateApplication& SlateApp, const TSharedRef<SWidget> InWidget);
	static void ClickSCheckBox(FSlateApplication& SlateApp, const TSharedRef<SWidget> InWidget);
	static void ClickSDockTab(FSlateApplication& SlateApp, const TSharedRef<SWidget> InWidget);
	static void ClickSPropertyValueWidget(FSlateApplication& SlateApp, const TSharedRef<SWidget> InWidget);

	static void ClickSNodeSPin(FSlateApplication& SlateApp, const TSharedRef<SWidget> InWidget);
	static void ClickSNode(FSlateApplication& SlateApp, const TSharedRef<SWidget> InWidget);
	static void ClickSPin(FSlateApplication& SlateApp, const TSharedRef<SWidget> InWidget);

	static FUMLogger Logger;
};
