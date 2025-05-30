#include "UMInputHelpers.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "Types/SlateEnums.h"
#include "UMSlateHelpers.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SViewport.h"
#include "VimInputProcessor.h"
#include "UMFocusHelpers.h"

// DEFINE_LOG_CATEGORY_STATIC(LogUMInputHelpers, NoLogging, All); // Prod
DEFINE_LOG_CATEGORY_STATIC(LogUMInputHelpers, Log, All); // Dev
FUMLogger FUMInputHelpers::Logger(&LogUMInputHelpers, true);

FUMOnSimulateRightClick FUMInputHelpers::OnSimulateRightClick;

void FUMInputHelpers::SimulateClickOnWidget(
	FSlateApplication& SlateApp, const TSharedRef<SWidget> Widget,
	const FKey& EffectingButton, float Delay, bool bIsDoubleClick,
	bool bIsShiftDown, bool bIsCtrlDown)
{
	TWeakPtr<SWidget> WeakWidget = Widget;
	FTimerHandle	  TimerHandle;
	GEditor->GetTimerManager()->SetTimer(
		TimerHandle,
		[&SlateApp, WeakWidget, EffectingButton, bIsDoubleClick, bIsShiftDown, bIsCtrlDown]() {
			if (TSharedPtr<SWidget> Widget = WeakWidget.Pin())
				FUMInputHelpers::SimulateClickOnWidget(
					SlateApp,
					Widget.ToSharedRef(),
					EffectingButton, bIsDoubleClick, bIsShiftDown, bIsCtrlDown);
		},
		Delay, false);
}

void FUMInputHelpers::SimulateClickOnWidget(
	FSlateApplication& SlateApp, const TSharedRef<SWidget> Widget,
	const FKey& EffectingButton, bool bIsDoubleClick,
	bool bIsShiftDown, bool bIsCtrlDown)
{
	FWidgetPath WidgetPath;
	if (!SlateApp.FindPathToWidget(Widget, WidgetPath))
	{
		Logger.Print("SimulateClickOnWidget: Failed to find widget path!",
			ELogVerbosity::Warning);
		return;
	}

	TSharedPtr<SWindow> ActiveWindow = SlateApp.GetActiveTopLevelRegularWindow();
	if (!ActiveWindow.IsValid())
	{
		Logger.Print("SimulateClickOnWidget: No active window found!",
			ELogVerbosity::Warning);
		return;
	}

	// Get the Native OS Window
	TSharedPtr<FGenericWindow> NativeWindow = ActiveWindow->GetNativeWindow();
	if (!NativeWindow.IsValid())
	{
		Logger.Print("SimulateClickOnWidget: Native window is invalid!",
			ELogVerbosity::Warning);
		return;
	}

	// Get the geometry of the widget to determine its position
	const FGeometry& WidgetGeometry = Widget->GetCachedGeometry();
	const FVector2D	 WidgetScreenPosition = WidgetGeometry.GetAbsolutePosition();
	const FVector2D	 WidgetSize = WidgetGeometry.GetLocalSize();
	const FVector2D	 WidgetCenter = WidgetScreenPosition + (WidgetSize * 0.5f);

	// Save the original cursor position that we will go back to after clicking
	const FVector2D OriginalCursorPosition = SlateApp.GetCursorPos();

	SlateApp.GetPlatformApplication()->Cursor->Show(false); // Hide micro flicker
	SlateApp.SetCursorPos(WidgetCenter);					// Move to the widget's center

	// Set Modifier Key State
	FModifierKeysState ModKeys(
		bIsShiftDown, bIsShiftDown,
		bIsCtrlDown, bIsCtrlDown,
		false, false, false, false, false);

	FPointerEvent MouseDownEvent( // Construct the click Pointer Event
		0,						  // PointerIndex
		WidgetCenter,			  // Current cursor position
		WidgetCenter,			  // Last cursor position
		TSet<FKey>(),			  // No buttons pressed
		EffectingButton,
		0.0f, // WheelDelta
		ModKeys);

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
	SlateApp.GetPlatformApplication()->Cursor->Show(true); // Reveal
}

void FUMInputHelpers::SimulateRightClick(
	FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	TSharedPtr<SWidget> FocusedWidget = SlateApp.GetUserFocusedWidget(0);
	if (!FocusedWidget.IsValid())
		return;

	Logger.Print("Simulate Right-Click", ELogVerbosity::Verbose, true);
	OnSimulateRightClick.Broadcast();

	// Will fetch and assign the item to Focused Widget (or not if not list view)
	FUMSlateHelpers::GetSelectedTreeViewItemAsWidget(SlateApp, FocusedWidget,
		TOptional<TSharedPtr<SListView<TSharedPtr<ISceneOutlinerTreeItem>>>>());

	// Will remain the original focused widget if not list
	FUMInputHelpers::SimulateClickOnWidget(
		SlateApp, FocusedWidget.ToSharedRef(), EKeys::RightMouseButton);

	FUMFocusHelpers::TryFocusFuturePopupMenu(SlateApp);
}

void FUMInputHelpers::SimulateClickAtPosition(
	FSlateApplication& SlateApp, const FVector2D& ClickPosition,
	const FKey& EffectingButton, float Delay, bool bIsDoubleClick,
	bool bIsShiftDown, bool bIsCtrlDown)
{
	FTimerHandle TimerHandle;
	GEditor->GetTimerManager()->SetTimer(
		TimerHandle,
		[&SlateApp, ClickPosition, EffectingButton, bIsDoubleClick, bIsShiftDown, bIsCtrlDown]() {
			FUMInputHelpers::SimulateClickAtPosition(
				SlateApp, ClickPosition,
				EffectingButton, bIsDoubleClick, bIsShiftDown, bIsCtrlDown);
		},
		Delay, false);
}

void FUMInputHelpers::SimulateClickAtPosition(
	FSlateApplication& SlateApp, const FVector2D& ClickPosition,
	const FKey& EffectingButton, bool bIsDoubleClick,
	bool bIsShiftDown, bool bIsCtrlDown)
{
	TSharedPtr<SWindow> ActiveWindow = SlateApp.GetActiveTopLevelRegularWindow();
	if (!ActiveWindow.IsValid())
	{
		Logger.Print("SimulateClickAtPosition: No active window found!", ELogVerbosity::Warning);
		return;
	}

	// Get the native OS window
	TSharedPtr<FGenericWindow> NativeWindow = ActiveWindow->GetNativeWindow();
	if (!NativeWindow.IsValid())
	{
		Logger.Print("SimulateClickAtPosition: Native window is invalid!", ELogVerbosity::Warning);
		return;
	}

	// Save the original cursor position to restore after clicking
	const FVector2D OriginalCursorPosition = SlateApp.GetCursorPos();

	SlateApp.GetPlatformApplication()->Cursor->Show(false); // Hide cursor for micro flicker
	SlateApp.SetCursorPos(ClickPosition);					// Move to the specified click position

	// Set modifier key state
	FModifierKeysState ModKeys(
		bIsShiftDown, bIsShiftDown, // LeftShift, RightShift
		bIsCtrlDown, bIsCtrlDown,	// LeftControl, RightControl
		false, false, false, false, false);

	// Construct the pointer event for the click
	FPointerEvent MouseDownEvent(
		0,			   // PointerIndex
		ClickPosition, // Current cursor position
		ClickPosition, // Last cursor position
		TSet<FKey>(),  // No additional buttons pressed
		EffectingButton,
		0.0f, // WheelDelta
		ModKeys);

	if (bIsDoubleClick)
	{
		SlateApp.ProcessMouseButtonDoubleClickEvent(NativeWindow, MouseDownEvent);
		SlateApp.ProcessMouseButtonUpEvent(MouseDownEvent); // May be needed to complete the double-click
	}
	else
	{
		SlateApp.ProcessMouseButtonDownEvent(NativeWindow, MouseDownEvent);
		SlateApp.ProcessMouseButtonUpEvent(MouseDownEvent);
	}

	// Restore original cursor position and show the cursor again
	SlateApp.SetCursorPos(OriginalCursorPosition);
	SlateApp.GetPlatformApplication()->Cursor->Show(true);
}

void FUMInputHelpers::ToggleRightClickPress(
	FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	// if (const TSharedPtr<SViewport> Viewport = SlateApp.GetGameViewport())
	// {
	// 	// Viewport->OnMouseButtonDown();
	// 	TWeakPtr<ISlateViewport> IView = Viewport->GetViewportInterface();
	// 	if (const auto View = IView.Pin())
	// 	{
	// 		if (const auto AsWidget = View->GetWidget().Pin())
	// 		{
	// 			const auto Geometry = AsWidget->GetCachedGeometry();
	// 			View->OnMouseButtonDown(Geometry, FPointerEvent(0, Geometry.AbsolutePosition, Geometry.AbsolutePosition, TSet<FKey>(), EKeys::RightMouseButton, 0.0f, FModifierKeysState()));
	// 		}
	// 	}
	// }

	if (GetActiveEditorViewportClient())
	{
		InputKey(GetActiveEditorViewportClient(), EKeys::K, EInputEvent::IE_Repeat);
		Logger.Print("ToggleRightClickPress", ELogVerbosity::Verbose, true);
	}
}

FEditorViewportClient* FUMInputHelpers::GetActiveEditorViewportClient()
{
	if (GEditor && GEditor->GetActiveViewport())
	{
		return static_cast<FEditorViewportClient*>(GEditor->GetActiveViewport()->GetClient());
	}
	return nullptr;
}

void FUMInputHelpers::OrbitAroundPivot(
	FEditorViewportClient* Client, float DeltaYaw, float DeltaPitch)
{
	if (!Client)
	{
		return;
	}

	// Get the current pivot (focus) and the current camera rotation & location
	const FVector  Pivot = Client->GetLookAtLocation();
	const FRotator CurrentRot = Client->GetViewRotation();
	const FVector  CurrentLoc = Client->GetViewLocation();

	// Distance from camera to pivot
	const float DistanceToPivot = (CurrentLoc - Pivot).Size();

	// New rotation = old rotation + the deltas
	FRotator NewRotation = CurrentRot;
	NewRotation.Yaw += DeltaYaw;
	NewRotation.Pitch += DeltaPitch;

	// Constrain pitch if you want to avoid flipping
	NewRotation.Pitch = FMath::ClampAngle(NewRotation.Pitch, -89.9f, 89.9f);

	// Convert rotation to a direction vector
	const FVector NewDirection = NewRotation.Vector();

	// Orbit: place the camera so it remains the same distance from the pivot
	const FVector NewLocation = Pivot - (NewDirection * DistanceToPivot);

	// Apply back to the viewport
	Client->SetViewLocation(NewLocation);
	Client->SetViewRotation(NewRotation);
}

void FUMInputHelpers::RotateCameraInPlace(
	FEditorViewportClient* Client, float DeltaYaw, float DeltaPitch)
{
	if (!Client)
	{
		return;
	}

	// Get the current camera rotation & location
	FRotator CurrentRot = Client->GetViewRotation();
	// We do NOT alter the location, so just store it
	FVector CurrentLoc = Client->GetViewLocation();

	// Adjust rotation
	CurrentRot.Yaw += DeltaYaw;
	CurrentRot.Pitch += DeltaPitch;

	// Optional: clamp pitch to avoid flipping
	CurrentRot.Pitch = FMath::ClampAngle(CurrentRot.Pitch, -89.9f, 89.9f);

	// Apply updated rotation; keep the same location
	Client->SetViewLocation(CurrentLoc);
	Client->SetViewRotation(CurrentRot);
}

bool FUMInputHelpers::InputKey(
	FEditorViewportClient* InViewportClient,
	FKey				   Key,
	EInputEvent			   Event)
{
	FKeyEvent KeyEvent;

	if (Event == IE_Pressed || Event == IE_Repeat)
	{
		float Step = 5.0f; // degrees per key press

		if (Key == EKeys::H)
		{
			// OrbitAroundPivot(InViewportClient, -Step, 0.0f);
			RotateCameraInPlace(InViewportClient, -Step, 0.0f);
			return true;
		}
		else if (Key == EKeys::L)
		{
			// OrbitAroundPivot(InViewportClient, Step, 0.0f);
			RotateCameraInPlace(InViewportClient, Step, 0.0f);
			return true;
		}
		else if (Key == EKeys::K)
		{
			// OrbitAroundPivot(InViewportClient, 0.0f, Step);
			RotateCameraInPlace(InViewportClient, 0.0f, Step);
			return true;
		}
		else if (Key == EKeys::J)
		{
			// OrbitAroundPivot(InViewportClient, 0.0f, -Step);
			RotateCameraInPlace(InViewportClient, 0.0f, -Step);
			return true;
		}
	}

	return false; // let other handlers process keys we didn’t handle
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
	const FModifierKeysState ModKeysShift(
		bIsShiftDown, bIsShiftDown,
		false, false, false, false, false, false, false);

	EUINavigation UINav;
	if (GetUINavigationFromVimKey(InKeyEvent.GetKey(), UINav))
	{
		FNavigationEvent NavEvent(ModKeysShift, 0, UINav,
			ENavigationGenesis::Keyboard);
		OutNavigationEvent = NavEvent;
		return true;
	}
	return false;
}

bool FUMInputHelpers::MapVimToArrowNavigation(
	const FKeyEvent& InKeyEvent, FKeyEvent& OutKeyEvent, bool bIsShiftDown)
{
	const FModifierKeysState ModKeysState(
		bIsShiftDown, bIsShiftDown,
		false, false, /* Control */
		// NOTE:
		// Simulate alt down for seems to help with escaping some widgets
		// that persist on keeping attention (e.g. Content Browser file tree)
		// So until I'll notice any harm in constant alt down; I'm keeping this.
		true, true,	  /* Alt */
		false, false, /* Commands */
		false /* Caps Locks */);

	FKey MappedKey;
	if (GetArrowKeyFromVimKey(InKeyEvent.GetKey(), MappedKey))
	{
		OutKeyEvent = FKeyEvent(
			MappedKey,
			ModKeysState,
			0,	   // User index
			false, // Is repeat
			0,	   // Character code
			0	   // Key code
		);
		return true;
	}
	return false;
}

void FUMInputHelpers::Enter(
	FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	static const FName ButtonType{ "SButton" };

	TSharedPtr<SWidget> FocusedWidget = SlateApp.GetUserFocusedWidget(0);
	if (!FocusedWidget)
		return;

	// Logger.Print(FocusedWidget->GetWidgetClass().GetWidgetType().ToString());
	const FName FocusedWidgetType =
		FocusedWidget->GetWidgetClass().GetWidgetType();

	if (FocusedWidgetType.IsEqual(ButtonType))
	{
		FUMFocusHelpers::ClickSButton(SlateApp, FocusedWidget.ToSharedRef());
		return;
	}
	// ** Not sure this is even needed :0 **
	// Will fetch and assign the item to Focused Widget (or not if not list view)
	// if (FUMSlateHelpers::GetSelectedTreeViewItemAsWidget(SlateApp, FocusedWidget,
	// 		TOptional<TSharedPtr<SListView<TSharedPtr<ISceneOutlinerTreeItem>>>>()))
	// {
	// 	// Lists requires Double-Click to actually open
	// 	FUMInputHelpers::SimulateClickOnWidget(
	// 		SlateApp, FocusedWidget.ToSharedRef(),
	// 		EKeys::LeftMouseButton, true /* Double-Click */);
	// 	return;
	// }

	// ** Not sure this is even needed :0 **
	// All other widgets can be a normal Single-Click
	// FUMInputHelpers::SimulateClickOnWidget(SlateApp, FocusedWidget.ToSharedRef(),
	// 	EKeys::LeftMouseButton, false /* Single-Click */);

	// I think now we know how to properly simulate the Enter and this is working.
	// if (FocusedWidgetType.IsEqual("SComboRow<TSharedPtr<FString>>"))
	// "SComboListType"
	if (FocusedWidget->GetTypeAsString().Equals("SComboListType"))
	{
		// Let focuser know we're about to lose focus to something unuseful and
		// future schedule focus on something useful
	}
	else
	{
		Logger.Print(FString::Printf(TEXT("NOPE: %s"), *FocusedWidget->GetTypeAsString()), ELogVerbosity::Warning, true);
	}

	FVimInputProcessor::Get()->SimulateKeyPress(SlateApp, FKey(EKeys::Enter));
}

bool FUMInputHelpers::GetDigitFromKey(const FKey& InKey, int32& OutDigit,
	int32 MinClamp, int32 MaxClamp)
{
	static const TMap<FKey, int32> KeyToDigit = {
		// { EKeys::Zero, 0 },  // Isn't useful and also collides with 0 JumpStart
		{ EKeys::One, 1 },
		{ EKeys::Two, 2 },
		{ EKeys::Three, 3 },
		{ EKeys::Four, 4 },
		{ EKeys::Five, 5 },
		{ EKeys::Six, 6 },
		{ EKeys::Seven, 7 },
		{ EKeys::Eight, 8 },
		{ EKeys::Nine, 9 },
	};

	const int32* FoundDigit = KeyToDigit.Find(InKey);
	if (!FoundDigit)
		return false;

	int32 Digit = *FoundDigit;

	// Apply clamping if specified
	if (MinClamp > 0 || MaxClamp > 0)
	{
		// If only min is specified, use the found number as max
		const int32 EffectiveMax = (MaxClamp > 0) ? MaxClamp : Digit;
		// If only max is specified, use 0 as min
		const int32 EffectiveMin = (MinClamp > 0) ? MinClamp : 0;

		OutDigit = FMath::Clamp(Digit, EffectiveMin, EffectiveMax);
	}
	else
		OutDigit = Digit;

	return true;
}

bool FUMInputHelpers::GetStrDigitFromKey(const FKey& InKey, FString& OutStr,
	int32 MinClamp, int32 MaxClamp)
{
	int32 Digit;
	if (GetDigitFromKey(InKey, Digit, MinClamp, MaxClamp))
	{
		OutStr = FString::FromInt(Digit);
		return true;
	}
	return false;
}

FInputChord FUMInputHelpers::GetChordFromKeyEvent(
	const FKeyEvent& InKeyEvent)
{
	const FModifierKeysState& ModState = InKeyEvent.GetModifierKeys();
	return FInputChord(
		InKeyEvent.GetKey(),	  // The key
		ModState.IsShiftDown(),	  // bShift
		ModState.IsControlDown(), // bCtrl
		ModState.IsAltDown(),	  // bAlt
		ModState.IsCommandDown()  // bCmd
	);
}

void FUMInputHelpers::ReleaseMouseButton(const FKey KeyToRelease)
{
	FSlateApplication& SlateApp = FSlateApplication::Get();

	const FVector2D CurrPos = SlateApp.GetCursorPos();
	FPointerEvent	MouseUpEvent(
		  0,				   // PointerIndex
		  CurrPos,			   // ScreenSpacePosition
		  CurrPos,			   // LastScreenSpacePosition
		  TSet<FKey>(),		   // No buttons pressed now
		  KeyToRelease,		   // EffectingButton (button being released)
		  0.0f,				   // WheelDelta
		  FModifierKeysState() // ModifierKeys
	  );
	SlateApp.ProcessMouseButtonUpEvent(MouseUpEvent);
}

void FUMInputHelpers::MoveMouseButtonToPosition(
	FVector2D TargetPosition)
{
	FSlateApplication& SlateApp = FSlateApplication::Get();

	SlateApp.SetCursorPos(TargetPosition);

	// Or move to with the pointer thingy?
}

void FUMInputHelpers::SimulateMouseMoveToPosition(FSlateApplication& SlateApp, const FVector2D TargetPosition)
{
	SlateApp.ProcessMouseMoveEvent(
		FPointerEvent(
			0,
			TargetPosition,
			SlateApp.GetCursorPos(),
			TSet<FKey>(),
			FKey(),
			0,
			FModifierKeysState()));
}

// TODO:
// This should really return a delegate or some way to know when the delays are
// done to explicitly let the caller know when it's safe to call more actions!
// Already had a problem where a timer was set to 0.05f in the caller (which is
// bad because they we're racing and not in sync - both set to 0.05f).
// Essentially, to caller should always make sure to call actions *after* the
// delays of this function are finished (so best via some delegate calling?)
bool FUMInputHelpers::DragAndReleaseWidgetAtPosition(
	const TSharedRef<SWidget> InWidget,
	const FVector2f			  TargetPosition)
{
	FSlateApplication& SlateApp = FSlateApplication::Get();

	// Calculate the active widget center, which is where we will position our
	// cursor, to then start the drag.
	FVector2f WidgetCenterPos =
		FUMSlateHelpers::GetWidgetCenterScreenSpacePosition(InWidget);

	if (!SimulateMousePressAtPosition(WidgetCenterPos))
		return false;

	// Now we want to simulate the move event (to simulate dragging)
	TriggerDragInPlace();

	// We need a slight delay to let the drag adjust...
	// This seems to be important; else it won't catch up and break.
	TSharedRef<FTimerManager> TimerManager = GEditor->GetTimerManager();

	FTimerHandle TimerHandle_MoveMouseToTabWell;
	TimerManager->SetTimer(
		TimerHandle_MoveMouseToTabWell,
		[&SlateApp, TargetPosition]() {
			SlateApp.SetCursorPos(FVector2D(TargetPosition));
		},
		0.025f,
		false);

	// DEBUG: Return here to debug drag to position (pre-release)
	// SlateApp.GetPlatformApplication()->Cursor->Show(true);
	// return false;
	// DEBUG: Return here to debug drag to position (pre-release)

	// Release the tab in the new TabWell
	FTimerHandle   TimerHandle_ReleaseMouseButton;
	FTimerDelegate Delegate_ReleaseMouseButton;
	Delegate_ReleaseMouseButton.BindStatic(
		&ReleaseMouseButton, EKeys::LeftMouseButton);

	TimerManager->SetTimer(
		TimerHandle_ReleaseMouseButton,
		Delegate_ReleaseMouseButton,
		0.05f,
		false);

	return true;
}

bool FUMInputHelpers::DragAndReleaseWidgetAtPosition(
	const TSharedRef<SWidget> InWidget,
	const TArray<FVector2f>&  TargetPositions,
	const float				  MoveOffsetDelay)
{
	if (TargetPositions.IsEmpty())
		return false;

	FSlateApplication& SlateApp = FSlateApplication::Get();

	// Calculate the active widget center, which is where we will position our
	// cursor, to then start the drag.
	FVector2f WidgetCenterPos =
		FUMSlateHelpers::GetWidgetCenterScreenSpacePosition(InWidget);

	if (!SimulateMousePressAtPosition(WidgetCenterPos))
		return false;

	// Now we want to simulate the move event (to simulate dragging)
	TriggerDragInPlace();

	// We need a slight delay to let the drag adjust...
	// This seems to be important; else it won't catch up and break.
	TSharedRef<FTimerManager> TimerManager = GEditor->GetTimerManager();

	const int32 PosNum = TargetPositions.Num();
	const float TotalOffsetDelay = PosNum * MoveOffsetDelay;

	for (int32 i = 0; i < PosNum; ++i)
	{
		const FVector2f TargetPosition = TargetPositions[i];
		FTimerHandle	TimerHandle;

		TimerManager->SetTimer(
			TimerHandle,
			[TargetPosition, &SlateApp]() {
				SlateApp.SetCursorPos(FVector2D(TargetPosition));
			},
			(i * MoveOffsetDelay) + MoveOffsetDelay,
			false);
	}

	// DEBUG: Return here to debug drag to position (pre-release)
	// SlateApp.GetPlatformApplication()->Cursor->Show(true);
	// return false;
	// DEBUG: Return here to debug drag to position (pre-release)

	// Release the tab in the new TabWell
	FTimerHandle   TimerHandle_ReleaseMouseButton;
	FTimerDelegate Delegate_ReleaseMouseButton;
	Delegate_ReleaseMouseButton.BindStatic(
		&ReleaseMouseButton, EKeys::LeftMouseButton);

	TimerManager->SetTimer(
		TimerHandle_ReleaseMouseButton,
		Delegate_ReleaseMouseButton,
		TotalOffsetDelay + MoveOffsetDelay,
		false);

	return true;
}

bool FUMInputHelpers::SimulateMousePressAtPosition(
	const FVector2f TargetPosition,
	const FKey		MouseButtonToSimulate)
{
	FSlateApplication& SlateApp = FSlateApplication::Get();

	// We will need the Native Window as a parameter for the MouseDown Simulation
	TSharedPtr<FGenericWindow> GenericActiveWindow =
		FUMSlateHelpers::GetGenericActiveTopLevelWindow();
	if (!GenericActiveWindow.IsValid())
		return false;

	// Move to the center of the tab we want to drag and store current position
	SlateApp.SetCursorPos(FVector2d(TargetPosition));
	const FVector2f CurrMousePos = SlateApp.GetCursorPos(); // Current Pos

	FPointerEvent MousePressEvent(
		0,
		0,
		CurrMousePos,
		CurrMousePos,
		TSet<FKey>({ MouseButtonToSimulate }),
		MouseButtonToSimulate,
		0,
		FModifierKeysState());

	// Simulate Press
	SlateApp.ProcessMouseButtonDownEvent(GenericActiveWindow, MousePressEvent);
	return true;
}

void FUMInputHelpers::TriggerDragInPlace()
{
	FSlateApplication& SlateApp = FSlateApplication::Get();

	// Dragging horizontally by 10 units just to trigger the drag at all.
	// 10 seems to be around the minimum to trigger the dragging.
	// TODO: Test on more screen resolutions and on MacOS
	const FVector2f MouseCurrPos = SlateApp.GetCursorPos();
	FVector2f		EndPosition = MouseCurrPos + FVector2f(10.0f, 0.0f);

	FPointerEvent MouseMoveEvent(
		0,									  // PointerIndex
		EndPosition,						  // ScreenSpacePosition (Goto)
		MouseCurrPos,						  // LastScreenSpacePosition (Prev)
		TSet<FKey>{ EKeys::LeftMouseButton }, // Currently pressed buttons
		EKeys::Invalid,						  // No new button pressed
		0.0f,								  // WheelDelta
		FModifierKeysState()				  // ModifierKeys
	);

	// Simulate Move (drag)
	SlateApp.ProcessMouseMoveEvent(MouseMoveEvent);
}

void FUMInputHelpers::SimulateScrollWidget(
	FSlateApplication& SlateApp, const TSharedRef<SWidget> Widget,
	FVector2f CursorLocation, float WheelDelta,
	bool bIsShiftDown, bool bIsCtrlDown)
{
	FModifierKeysState ModKeys(
		bIsShiftDown, bIsShiftDown,
		bIsCtrlDown, bIsCtrlDown,
		false, false, false, false, false);

	FPointerEvent PointerEvent(
		0,				/* PointerIndex     = */
		CursorLocation, /* ScreenSpacePos   = */
		CursorLocation, /* LastScreenSpacePos = */
		TSet<FKey>(),	/* PressedButton    = */
		FKey(),			/* Effecting Button */
		WheelDelta,
		ModKeys);

	Widget->OnMouseWheel(Widget->GetCachedGeometry(), PointerEvent);
}

FModifierKeysState FUMInputHelpers::GetShiftDownModKeys()
{
	return FModifierKeysState(true, true, false, false, false, false, false, false, false);
}
FModifierKeysState FUMInputHelpers::GetCtrlDownModKeys()
{
	return FModifierKeysState(false, false, true, true, false, false, false, false, false);
}

FKeyEvent FUMInputHelpers::GetKeyEventFromKey(
	const FKey& InKey, bool bIsShiftDown)
{
	const FModifierKeysState ModKeys(bIsShiftDown, bIsShiftDown,
		false, false, false, false, false, false, false);

	return FKeyEvent(
		InKey,
		ModKeys,
		0, false, 0, 0);
}

bool FUMInputHelpers::IsKeyEventModifierOnly(const FKeyEvent& InKeyEvent)
{
	const int32 IntChar = InKeyEvent.GetCharacter();
	if (InKeyEvent.GetModifierKeys().AnyModifiersDown() && IntChar == 0)
		return true; // Ignore Modifiers as standalones (i.e. solo shift/ctrl etc)
	return false;
}

TCHAR FUMInputHelpers::GetCharFromKeyEvent(const FKeyEvent& InKeyEvent)
{
	// Get the raw character from the key event
	TCHAR Character = InKeyEvent.GetCharacter();

	// Handle special cases for number keys when Shift is pressed
	if (InKeyEvent.IsShiftDown())
	{
		// Map for shifted number keys (US keyboard layout)
		switch (Character)
		{
			case '1':
				return '!';
			case '2':
				return '@';
			case '3':
				return '#';
			case '4':
				return '$';
			case '5':
				return '%';
			case '6':
				return '^';
			case '7':
				return '&';
			case '8':
				return '*';
			case '9':
				return '(';
			case '0':
				return ')';
			case '-':
				return '_';
			case '=':
				return '+';
			case '[':
				return '{';
			case ']':
				return '}';
			case '\\':
				return '|';
			case ';':
				return ':';
			case '\'':
				return '"';
			case ',':
				return '<';
			case '.':
				return '>';
			case '/':
				return '?';
			case '`':
				return '~';
			default:
				// For alphabetic characters, keep them uppercase when Shift is down
				if (FChar::IsAlpha(Character))
				{
					return FChar::ToUpper(Character);
				}
				return Character;
		}
	}
	else
	{
		// No Shift - lowercase alphabetic characters, keep others as is
		return FChar::IsAlpha(Character) ? FChar::ToLower(Character) : Character;
	}
}
