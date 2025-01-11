#include "UMInputHelpers.h"
#include "Types/SlateEnums.h"
#include "UMSlateHelpers.h"
#include "Widgets/Input/SButton.h"

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
