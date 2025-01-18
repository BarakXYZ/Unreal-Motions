#pragma once

#include "EditorViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWidget.h"
#include "InputCoreTypes.h"
#include "UMLogger.h"

class FUMInputHelpers
{
public:
	static void SimulateClickOnWidget(
		FSlateApplication& SlateApp, const TSharedRef<SWidget> Widget,
		const FKey& EffectingButton, bool bIsDoubleClick = false);

	static void SimulateRightClick(
		FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	static void ToggleRightClickPress(
		FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	static FEditorViewportClient* GetActiveEditorViewportClient();

	static void OrbitAroundPivot(
		FEditorViewportClient* Client, float DeltaYaw, float DeltaPitch);

	static void RotateCameraInPlace(
		FEditorViewportClient* Client, float DeltaYaw, float DeltaPitch);

	static bool InputKey(
		FEditorViewportClient* InViewportClient,
		FKey				   Key,
		EInputEvent			   Event);

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

	/**
	 * @brief Maps Vim movement keys to corresponding arrow key events.
	 *
	 * @details Translates HJKL keys to arrow key events while preserving:
	 * - Modifier key states
	 * - Other key event properties
	 *
	 * @param InKeyEvent Original key event
	 * @param OutKeyEvent Mapped arrow key event
	 * @return true if mapping was successful, false if key wasn't mappable
	 */
	static bool MapVimToArrowNavigation(
		const FKeyEvent& InKeyEvent, FKeyEvent& OutKeyEvent, bool bIsShiftDown);

	static void Enter(
		FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	/**
	 * Converts a numeric key press to its string representation with optional value clamping
	 * @param InKey - The key to convert
	 * @param OutStr - Output parameter for the string representation
	 * @param MinClamp - Optional minimum value to clamp the digit to (0 means no min clamping)
	 * @param MaxClamp - Optional maximum value to clamp the digit to (0 means no max clamping)
	 * @return True if the key was a numeric key (0-9)
	 */
	static bool GetStrDigitFromKey(const FKey& InKey, FString& OutStr,
		int32 MinClamp = 0, int32 MaxClamp = 0);

	/**
	 * Creates an FInputChord from a key event, capturing modifier key states
	 * @param InKeyEvent - The key event to convert
	 * @return FInputChord containing the key and modifier state information
	 */
	static FInputChord GetChordFromKeyEvent(const FKeyEvent& InKeyEvent);

	static void ReleaseMouseButtonAtCurrentPos(
		const FKey KeyToRelease = EKeys::LeftMouseButton);

	static void MoveMouseButtonToPosition(const FVector2D TargetPosition);

	static FUMLogger Logger;
};
