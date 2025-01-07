#include "UMInputHelpers.h"
#include "Types/SlateEnums.h"

const TSharedPtr<FUMInputHelpers> FUMInputHelpers::InputHelpers =
	MakeShared<FUMInputHelpers>();

FUMInputHelpers::FUMInputHelpers()
{
}

FUMInputHelpers::~FUMInputHelpers()
{
}

const TSharedPtr<FUMInputHelpers> FUMInputHelpers::Get()
{
	return InputHelpers;
}

void FUMInputHelpers::SimulateClickOnWidget(
	FSlateApplication& SlateApp, const TSharedRef<SWidget> Widget,
	const FKey& EffectingButton, bool bIsDoubleClick)
{

	FWidgetPath WidgetPath;
	if (!SlateApp.FindPathToWidget(Widget, WidgetPath))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("SimulateClickOnWidget: Failed to find widget path!"));
		return;
	}

	TSharedPtr<SWindow> ActiveWindow = SlateApp.GetActiveTopLevelRegularWindow();
	if (!ActiveWindow.IsValid())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("SimulateClickOnWidget: No active window found!"));
		return;
	}

	// Get the Native OS Window
	TSharedPtr<FGenericWindow> NativeWindow = ActiveWindow->GetNativeWindow();
	if (!NativeWindow.IsValid())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("SimulateClickOnWidget: Native window is invalid!"));
		return;
	}

	// Get the geometry of the widget to determine its position
	const FGeometry& WidgetGeometry = Widget->GetCachedGeometry();
	const FVector2D	 WidgetScreenPosition = WidgetGeometry.GetAbsolutePosition();
	const FVector2D	 WidgetSize = WidgetGeometry.GetLocalSize();
	const FVector2D	 WidgetCenter = WidgetScreenPosition + (WidgetSize * 0.5f);

	// Save the original cursor position that we will go back to after clicking
	const FVector2D OriginalCursorPosition = SlateApp.GetCursorPos();

	SlateApp.SetPlatformCursorVisibility(false); // Hides the micro mouse flicker
	SlateApp.SetCursorPos(WidgetCenter);		 // Move to the widget's center

	FPointerEvent MouseDownEvent( // Construct the click Pointer Event
		0,						  // PointerIndex
		WidgetCenter,			  // Current cursor position
		WidgetCenter,			  // Last cursor position
		TSet<FKey>(),			  // No buttons pressed
		EffectingButton,
		0.0f, // WheelDelta
		FModifierKeysState());

	if (bIsDoubleClick)
	{ // This will open things like assets, etc. (Tree view)
		SlateApp.ProcessMouseButtonDoubleClickEvent(NativeWindow, MouseDownEvent);
		SlateApp.ProcessMouseButtonUpEvent(MouseDownEvent); // needed?
	}
	else
	{ // Simulate click (Up & Down)
		SlateApp.ProcessMouseButtonDownEvent(NativeWindow, MouseDownEvent);
		SlateApp.ProcessMouseButtonUpEvent(MouseDownEvent);
	}

	// Move the cursor back to its original position & toggle visibility on
	SlateApp.SetCursorPos(OriginalCursorPosition);
	SlateApp.SetPlatformCursorVisibility(true);
}

bool FUMInputHelpers::AreMouseButtonsPressed(const TSet<FKey>& InButtons)
{
	const FSlateApplication& SlateApp = FSlateApplication::Get();
	const TSet<FKey>&		 PressedButtons = SlateApp.GetPressedMouseButtons();

	for (const FKey& Button : InButtons)
		if (PressedButtons.Contains(Button))
			return true;

	return false;
}

bool FUMInputHelpers::GetArrowKeyFromVimKey(const FKey& InVimKey, FKey& OutArrowKey)
{
	static const TMap<FKey, FKey> ArrowKeysByVimKeys = {
		{ EKeys::H, FKey(EKeys::Left) }, { EKeys::J, FKey(EKeys::Down) },
		{ EKeys::K, FKey(EKeys::Up) }, { EKeys::L, FKey(EKeys::Right) }
	};

	if (const FKey* Key = ArrowKeysByVimKeys.Find(InVimKey))
	{
		OutArrowKey = *Key;
		return true;
	}
	return false;
}

bool FUMInputHelpers::GetUINavigationFromVimKey(
	const FKey& InVimKey, EUINavigation& OutUINavigation)
{
	static const TMap<FKey, EUINavigation> UINavigationByVimKey = {
		{ EKeys::H, EUINavigation::Left }, { EKeys::J, EUINavigation::Down },
		{ EKeys::K, EUINavigation::Up }, { EKeys::L, EUINavigation::Right }
	};

	if (const EUINavigation* NavKey = UINavigationByVimKey.Find(InVimKey))
	{
		OutUINavigation = *NavKey;
		return true;
	}
	return false;
}

bool FUMInputHelpers::GetNavigationEventFromVimKey(
	const FKeyEvent&  InKeyEvent,
	FNavigationEvent& OutNavigationEvent, bool bIsShiftDown)
{
	const FModifierKeysState ModKeysShift(bIsShiftDown, bIsShiftDown,
		false, false, false, false, false, false, false);

	EUINavigation UINav;
	if (FUMInputHelpers::GetUINavigationFromVimKey(InKeyEvent.GetKey(), UINav))
	{
		FNavigationEvent NavEvent(ModKeysShift, 0, UINav,
			ENavigationGenesis::Keyboard);
		OutNavigationEvent = NavEvent;
		return true;
	}
	return false;
}

// void FUMInputHelpers::PrintConfiguredHotkeysCommandsName(
// 	const TArray<FInputChord>& InChordsToCheck)
// {
// 	if (InChordsToCheck.Num() == 0)
// 	{
// 		UE_LOG(LogTemp, Warning, TEXT("No input chords provided for checking hotkeys."));
// 		return;
// 	}

// 	const UInputSettings* InputSettings = GetDefault<InputSettings>();
// 	if (!InputSettings)
// 	{
// 		UE_LOG(LogTemp, Error, TEXT("Failed to get InputSettings."));
// 		return;
// 	}

// 	const TArray<FInputActionBinding>&	ActionMappings = InputSettings->GetActionMappings();
// 	const TArray<FInputAxisKeyBinding>& AxisMappings = InputSettings->GetAxisMappings();

// 	for (const FInputChord& Chord : InChordsToCheck)
// 	{
// 		bool bFound = false;

// 		// Check Action Mappings
// 		for (const FInputActionBinding& ActionMapping : ActionMappings)
// 		{
// 			if (ActionMapping.Key == Chord.Key && ActionMapping.bShift == Chord.bShift && ActionMapping.bCtrl == Chord.bCtrl && ActionMapping.bAlt == Chord.bAlt && ActionMapping.bCmd == Chord.bCmd)
// 			{
// 				UE_LOG(LogTemp, Log, TEXT("Key '%s' is bound to Action '%s'"),
// 					*Chord.Key.GetDisplayName().ToString(),
// 					*ActionMapping.ActionName.ToString());
// 				bFound = true;
// 			}
// 		}

// 		// Check Axis Mappings
// 		for (const FInputAxisKeyBinding& AxisMapping : AxisMappings)
// 		{
// 			if (AxisMapping.Key == Chord.Key)
// 			{
// 				UE_LOG(LogTemp, Log, TEXT("Key '%s' is bound to Axis '%s' with scale %f"),
// 					*Chord.Key.GetDisplayName().ToString(),
// 					*AxisMapping.AxisName.ToString(),
// 					AxisMapping.Scale);
// 				bFound = true;
// 			}
// 		}

// 		if (!bFound)
// 		{
// 			UE_LOG(LogTemp, Warning, TEXT("Key '%s' is not bound to any action or axis."),
// 				*Chord.Key.GetDisplayName().ToString());
// 		}
// 	}
// }
