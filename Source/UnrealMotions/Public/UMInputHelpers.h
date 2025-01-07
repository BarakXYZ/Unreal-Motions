#pragma once

#include "CoreMinimal.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWidget.h"
#include "InputCoreTypes.h"

class FUMInputHelpers
{
public:
	FUMInputHelpers();
	~FUMInputHelpers();

	static const TSharedPtr<FUMInputHelpers> Get();

	static void SimulateClickOnWidget(
		FSlateApplication& SlateApp, const TSharedRef<SWidget> Widget,
		const FKey& EffectingButton, bool bIsDoubleClick = false);

	static bool AreMouseButtonsPressed(const TSet<FKey>& InButtons);

	static bool GetArrowKeyFromVimKey(const FKey& InVimKey, FKey& OutArrowKey);

	static bool GetUINavigationFromVimKey(
		const FKey& InVimKey, EUINavigation& OutUINavigation);

	/**
	 * @brief Maps Vim navigation keys (HJKL) to corresponding UI nav events.
	 *
	 * @details Creates a navigation event that:
	 * - Translates H,J,K,L to Left,Down,Up,Right
	 * - Handles shift modifier for selection
	 * - Sets appropriate navigation genesis
	 *
	 * @param InKeyEvent The original key event to map
	 * @param OutNavigationEvent The resulting navigation event
	 * @param bIsShiftDown Whether shift is being held
	 * @return true if mapping was successful, false otherwise
	 */
	static bool GetNavigationEventFromVimKey(const FKeyEvent& InKeyEvent,
		FNavigationEvent& OutNavigationEvent, bool bIsShiftDown);

	// static void PrintConfiguredHotkeysCommandsName(const TArray<FInputChord>& InChordsToCheck);

	static const TSharedPtr<FUMInputHelpers> InputHelpers;
};
