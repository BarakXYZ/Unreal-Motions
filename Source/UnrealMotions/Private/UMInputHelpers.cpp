#include "UMInputHelpers.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "Types/SlateEnums.h"
#include "UMSlateHelpers.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SViewport.h"

// DEFINE_LOG_CATEGORY_STATIC(LogUMInputHelpers, NoLogging, All); // Prod
DEFINE_LOG_CATEGORY_STATIC(LogUMInputHelpers, Log, All); // Dev
FUMLogger FUMInputHelpers::Logger(&LogUMInputHelpers, true);

void FUMInputHelpers::SimulateClickOnWidget(
	FSlateApplication& SlateApp, const TSharedRef<SWidget> Widget,
	const FKey& EffectingButton, bool bIsDoubleClick)
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

void FUMInputHelpers::SimulateRightClick(
	FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	TSharedPtr<SWidget> FocusedWidget = SlateApp.GetUserFocusedWidget(0);
	if (!FocusedWidget.IsValid())
		return;

	// Will fetch and assign the item to Focused Widget (or not if not list view)
	FUMSlateHelpers::GetSelectedTreeViewItemAsWidget(SlateApp, FocusedWidget,
		TOptional<TSharedPtr<SListView<TSharedPtr<ISceneOutlinerTreeItem>>>>());

	// Will remain the original focused widget if not list
	FUMInputHelpers::SimulateClickOnWidget(
		SlateApp, FocusedWidget.ToSharedRef(), EKeys::RightMouseButton);
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
		Logger.Print("Test", ELogVerbosity::Verbose, true);
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
		false, false, false, false, false, false, false);

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

	TSharedPtr<SWidget> FocusedWidget =
		SlateApp.GetUserFocusedWidget(0);
	// SlateApp.GetKeyboardFocusedWidget();
	if (!FocusedWidget)
		return;

	// Logger.Print(FocusedWidget->GetWidgetClass().GetWidgetType().ToString());
	const FName FocusedWidgetType =
		FocusedWidget->GetWidgetClass().GetWidgetType();

	if (FocusedWidgetType.IsEqual(ButtonType))
	{
		TSharedPtr<SButton> FocusedWidgetAsButton =
			StaticCastSharedPtr<SButton>(FocusedWidget);
		FocusedWidgetAsButton->SimulateClick();
		return;
	}
	// Will fetch and assign the item to Focused Widget (or not if not list view)
	FUMSlateHelpers::GetSelectedTreeViewItemAsWidget(SlateApp, FocusedWidget,
		TOptional<TSharedPtr<SListView<TSharedPtr<ISceneOutlinerTreeItem>>>>());

	FUMInputHelpers::SimulateClickOnWidget(SlateApp, FocusedWidget.ToSharedRef(),
		EKeys::LeftMouseButton, true /* Double-Click */);
}

bool FUMInputHelpers::GetStrDigitFromKey(const FKey& InKey, FString& OutStr,
	int32 MinClamp, int32 MaxClamp)
{
	static const TMap<FKey, FString> KeyToStringDigits = {
		{ EKeys::Zero, TEXT("0") },
		{ EKeys::One, TEXT("1") },
		{ EKeys::Two, TEXT("2") },
		{ EKeys::Three, TEXT("3") },
		{ EKeys::Four, TEXT("4") },
		{ EKeys::Five, TEXT("5") },
		{ EKeys::Six, TEXT("6") },
		{ EKeys::Seven, TEXT("7") },
		{ EKeys::Eight, TEXT("8") },
		{ EKeys::Nine, TEXT("9") },
	};

	const FString* FoundStr = KeyToStringDigits.Find(InKey);
	if (!FoundStr)
	{
		return false;
	}

	// Convert found string to number for clamping
	int32 NumValue = FCString::Atoi(**FoundStr);

	// Apply clamping if specified
	if (MinClamp > 0 || MaxClamp > 0)
	{
		// If only min is specified, use the found number as max
		const int32 EffectiveMax = (MaxClamp > 0) ? MaxClamp : NumValue;
		// If only max is specified, use 0 as min
		const int32 EffectiveMin = (MinClamp > 0) ? MinClamp : 0;

		NumValue = FMath::Clamp(NumValue, EffectiveMin, EffectiveMax);
	}

	// Convert back to string
	OutStr = FString::FromInt(NumValue);
	return true;
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
