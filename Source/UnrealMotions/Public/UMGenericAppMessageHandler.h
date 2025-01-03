#pragma once

#include "GenericPlatform/GenericApplicationMessageHandler.h"

/**
 * A "chaining" message handler that forwards all events to the native handler
 * except the character input in OnKeyChar that we will intercept on Vim mode.
 */
class FUMGenericAppMessageHandler : public FGenericApplicationMessageHandler
{
public:
	/**
	 * Wrap the original (default) handler so we can selectively
	 * intercept OnKeyChar calls.
	 * This class should be constructed using the original platform
	 * GenericApplicationMessageHandler (see assignment in VimSubsystem init)
	 */
	explicit FUMGenericAppMessageHandler(TSharedPtr<FGenericApplicationMessageHandler> InOriginalHandler)
		: OriginalHandler(InOriginalHandler)
	{
	}

	virtual ~FUMGenericAppMessageHandler() override
	{
		OriginalHandler.Reset();
	}

	/** That will be our way to block and unblock the char input in runtime */
	void ToggleBlockAllCharInput(bool bBlockAllCharInput)
	{
		bBlockAllChars = bBlockAllCharInput;
	}

	bool bBlockAllChars = false;

	//-----------------------------------------------------------------
	// The ONLY method we actually change is OnKeyChar. Everything else
	// just calls through to OriginalHandler.
	//-----------------------------------------------------------------

	virtual bool OnKeyChar(const TCHAR Character, const bool IsRepeat) override
	{
		if (bBlockAllChars || Character == TEXT('\x1B'))
		{
			// Returning true => "handled," so it never reaches
			// Slate’s text widgets (or any editable text widgets, etc.)
			return true;
		}

		// Otherwise, forward the event to the original native handler.
		return OriginalHandler->OnKeyChar(Character, IsRepeat);
	}

	//-----------------------------------------------------------------
	// Everything below here is just forwarded 1:1:
	//-----------------------------------------------------------------

	virtual bool ShouldProcessUserInputMessages(const TSharedPtr<FGenericWindow>& PlatformWindow) const override
	{
		return OriginalHandler->ShouldProcessUserInputMessages(PlatformWindow);
	}

	virtual bool OnKeyDown(const int32 KeyCode, const uint32 CharacterCode, const bool IsRepeat) override
	{
		return OriginalHandler->OnKeyDown(KeyCode, CharacterCode, IsRepeat);
	}

	virtual bool OnKeyUp(const int32 KeyCode, const uint32 CharacterCode, const bool IsRepeat) override
	{
		return OriginalHandler->OnKeyUp(KeyCode, CharacterCode, IsRepeat);
	}

	virtual void OnInputLanguageChanged() override
	{
		OriginalHandler->OnInputLanguageChanged();
	}

	virtual bool OnMouseDown(const TSharedPtr<FGenericWindow>& Window, const EMouseButtons::Type Button) override
	{
		return OriginalHandler->OnMouseDown(Window, Button);
	}

	virtual bool OnMouseDown(const TSharedPtr<FGenericWindow>& Window, const EMouseButtons::Type Button, const FVector2D CursorPos) override
	{
		return OriginalHandler->OnMouseDown(Window, Button, CursorPos);
	}

	virtual bool OnMouseUp(const EMouseButtons::Type Button) override
	{
		return OriginalHandler->OnMouseUp(Button);
	}

	virtual bool OnMouseUp(const EMouseButtons::Type Button, const FVector2D CursorPos) override
	{
		return OriginalHandler->OnMouseUp(Button, CursorPos);
	}

	virtual bool OnMouseDoubleClick(const TSharedPtr<FGenericWindow>& Window, const EMouseButtons::Type Button) override
	{
		return OriginalHandler->OnMouseDoubleClick(Window, Button);
	}

	virtual bool OnMouseDoubleClick(const TSharedPtr<FGenericWindow>& Window, const EMouseButtons::Type Button, const FVector2D CursorPos) override
	{
		return OriginalHandler->OnMouseDoubleClick(Window, Button, CursorPos);
	}

	virtual bool OnMouseWheel(const float Delta) override
	{
		return OriginalHandler->OnMouseWheel(Delta);
	}

	virtual bool OnMouseWheel(const float Delta, const FVector2D CursorPos) override
	{
		return OriginalHandler->OnMouseWheel(Delta, CursorPos);
	}

	virtual bool OnMouseMove() override
	{
		return OriginalHandler->OnMouseMove();
	}

	virtual bool OnRawMouseMove(const int32 X, const int32 Y) override
	{
		return OriginalHandler->OnRawMouseMove(X, Y);
	}

	virtual bool OnCursorSet() override
	{
		return OriginalHandler->OnCursorSet();
	}

	virtual bool ShouldUsePlatformUserId() const override
	{
		return OriginalHandler->ShouldUsePlatformUserId();
	}

	virtual bool OnControllerAnalog(FGamepadKeyNames::Type KeyName, FPlatformUserId PlatformUserId,
		FInputDeviceId InputDeviceId, float AnalogValue) override
	{
		return OriginalHandler->OnControllerAnalog(KeyName, PlatformUserId, InputDeviceId, AnalogValue);
	}

	virtual bool OnControllerButtonPressed(FGamepadKeyNames::Type KeyName, FPlatformUserId PlatformUserId,
		FInputDeviceId InputDeviceId, bool IsRepeat) override
	{
		return OriginalHandler->OnControllerButtonPressed(KeyName, PlatformUserId, InputDeviceId, IsRepeat);
	}

	virtual bool OnControllerButtonReleased(FGamepadKeyNames::Type KeyName, FPlatformUserId PlatformUserId,
		FInputDeviceId InputDeviceId, bool IsRepeat) override
	{
		return OriginalHandler->OnControllerButtonReleased(KeyName, PlatformUserId, InputDeviceId, IsRepeat);
	}

	virtual void OnBeginGesture() override
	{
		OriginalHandler->OnBeginGesture();
	}

	virtual bool OnTouchGesture(EGestureEvent GestureType, const FVector2D& Delta, float WheelDelta,
		bool bIsDirectionInvertedFromDevice) override
	{
		return OriginalHandler->OnTouchGesture(GestureType, Delta, WheelDelta, bIsDirectionInvertedFromDevice);
	}

	virtual void OnEndGesture() override
	{
		OriginalHandler->OnEndGesture();
	}

	virtual bool OnTouchStarted(const TSharedPtr<FGenericWindow>& Window, const FVector2D& Location,
		float Force, int32 TouchIndex, FPlatformUserId PlatformUserId, FInputDeviceId DeviceId) override
	{
		return OriginalHandler->OnTouchStarted(Window, Location, Force, TouchIndex, PlatformUserId, DeviceId);
	}

	virtual bool OnTouchMoved(const FVector2D& Location, float Force, int32 TouchIndex,
		FPlatformUserId PlatformUserId, FInputDeviceId DeviceID) override
	{
		return OriginalHandler->OnTouchMoved(Location, Force, TouchIndex, PlatformUserId, DeviceID);
	}

	virtual bool OnTouchEnded(const FVector2D& Location, int32 TouchIndex,
		FPlatformUserId PlatformUserId, FInputDeviceId DeviceID) override
	{
		return OriginalHandler->OnTouchEnded(Location, TouchIndex, PlatformUserId, DeviceID);
	}

	virtual bool OnTouchForceChanged(const FVector2D& Location, float Force, int32 TouchIndex,
		FPlatformUserId PlatformUserId, FInputDeviceId DeviceID) override
	{
		return OriginalHandler->OnTouchForceChanged(Location, Force, TouchIndex, PlatformUserId, DeviceID);
	}

	virtual bool OnTouchFirstMove(const FVector2D& Location, float Force, int32 TouchIndex,
		FPlatformUserId PlatformUserId, FInputDeviceId DeviceID) override
	{
		return OriginalHandler->OnTouchFirstMove(Location, Force, TouchIndex, PlatformUserId, DeviceID);
	}

	virtual bool OnMotionDetected(const FVector& Tilt, const FVector& RotationRate, const FVector& Gravity,
		const FVector& Acceleration, FPlatformUserId PlatformUserId, FInputDeviceId InputDeviceId) override
	{
		return OriginalHandler->OnMotionDetected(Tilt, RotationRate, Gravity, Acceleration, PlatformUserId, InputDeviceId);
	}

	virtual bool OnSizeChanged(const TSharedRef<FGenericWindow>& Window, const int32 Width,
		const int32 Height, bool bWasMinimized) override
	{
		return OriginalHandler->OnSizeChanged(Window, Width, Height, bWasMinimized);
	}

	virtual void OnOSPaint(const TSharedRef<FGenericWindow>& Window) override
	{
		OriginalHandler->OnOSPaint(Window);
	}

	virtual FWindowSizeLimits GetSizeLimitsForWindow(const TSharedRef<FGenericWindow>& Window) const override
	{
		return OriginalHandler->GetSizeLimitsForWindow(Window);
	}

	virtual void OnResizingWindow(const TSharedRef<FGenericWindow>& Window) override
	{
		OriginalHandler->OnResizingWindow(Window);
	}

	virtual bool BeginReshapingWindow(const TSharedRef<FGenericWindow>& Window) override
	{
		return OriginalHandler->BeginReshapingWindow(Window);
	}

	virtual void FinishedReshapingWindow(const TSharedRef<FGenericWindow>& Window) override
	{
		OriginalHandler->FinishedReshapingWindow(Window);
	}

	virtual void HandleDPIScaleChanged(const TSharedRef<FGenericWindow>& Window) override
	{
		OriginalHandler->HandleDPIScaleChanged(Window);
	}

	virtual void SignalSystemDPIChanged(const TSharedRef<FGenericWindow>& Window) override
	{
		OriginalHandler->SignalSystemDPIChanged(Window);
	}

	virtual void OnMovedWindow(const TSharedRef<FGenericWindow>& Window, const int32 X, const int32 Y) override
	{
		OriginalHandler->OnMovedWindow(Window, X, Y);
	}

	virtual bool OnWindowActivationChanged(const TSharedRef<FGenericWindow>& Window, const EWindowActivation ActivationType) override
	{
		return OriginalHandler->OnWindowActivationChanged(Window, ActivationType);
	}

	virtual bool OnApplicationActivationChanged(const bool IsActive) override
	{
		return OriginalHandler->OnApplicationActivationChanged(IsActive);
	}

	virtual bool OnConvertibleLaptopModeChanged() override
	{
		return OriginalHandler->OnConvertibleLaptopModeChanged();
	}

	virtual EWindowZone::Type GetWindowZoneForPoint(const TSharedRef<FGenericWindow>& Window, const int32 X, const int32 Y) override
	{
		return OriginalHandler->GetWindowZoneForPoint(Window, X, Y);
	}

	virtual void OnWindowClose(const TSharedRef<FGenericWindow>& Window) override
	{
		OriginalHandler->OnWindowClose(Window);
	}

	virtual EDropEffect::Type OnDragEnterText(const TSharedRef<FGenericWindow>& Window, const FString& Text) override
	{
		return OriginalHandler->OnDragEnterText(Window, Text);
	}

	virtual EDropEffect::Type OnDragEnterFiles(const TSharedRef<FGenericWindow>& Window, const TArray<FString>& Files) override
	{
		return OriginalHandler->OnDragEnterFiles(Window, Files);
	}

	virtual EDropEffect::Type OnDragEnterExternal(const TSharedRef<FGenericWindow>& Window, const FString& Text, const TArray<FString>& Files) override
	{
		return OriginalHandler->OnDragEnterExternal(Window, Text, Files);
	}

	virtual EDropEffect::Type OnDragOver(const TSharedPtr<FGenericWindow>& Window) override
	{
		return OriginalHandler->OnDragOver(Window);
	}

	virtual void OnDragLeave(const TSharedPtr<FGenericWindow>& Window) override
	{
		OriginalHandler->OnDragLeave(Window);
	}

	virtual EDropEffect::Type OnDragDrop(const TSharedPtr<FGenericWindow>& Window) override
	{
		return OriginalHandler->OnDragDrop(Window);
	}

	virtual bool OnWindowAction(const TSharedRef<FGenericWindow>& Window, const EWindowAction::Type InActionType) override
	{
		return OriginalHandler->OnWindowAction(Window, InActionType);
	}

	virtual void SetCursorPos(const FVector2D& MouseCoordinate) override
	{
		OriginalHandler->SetCursorPos(MouseCoordinate);
	}

	// **********************************************************************
	// Deprecated methods that forward (UE5.1+).
	// NOTE: Not sure if I should still pass them. They do seem
	// to throw warnings on compile, so I've commented them for now.

	// virtual bool OnControllerAnalog(FGamepadKeyNames::Type KeyName, int32 ControllerId, float AnalogValue) override
	// {
	// 	return OriginalHandler->OnControllerAnalog(KeyName, ControllerId, AnalogValue);
	// }
	// virtual bool OnControllerButtonPressed(FGamepadKeyNames::Type KeyName, int32 ControllerId, bool IsRepeat) override
	// {
	// 	return OriginalHandler->OnControllerButtonPressed(KeyName, ControllerId, IsRepeat);
	// }
	// virtual bool OnControllerButtonReleased(FGamepadKeyNames::Type KeyName, int32 ControllerId, bool IsRepeat) override
	// {
	// 	return OriginalHandler->OnControllerButtonReleased(KeyName, ControllerId, IsRepeat);
	// }
	// **********************************************************************

	virtual bool OnTouchStarted(const TSharedPtr<FGenericWindow>& Window, const FVector2D& Location, float Force,
		int32 TouchIndex, int32 ControllerId) override
	{
		return OriginalHandler->OnTouchStarted(Window, Location, Force, TouchIndex, ControllerId);
	}
	virtual bool OnTouchMoved(const FVector2D& Location, float Force, int32 TouchIndex, int32 ControllerId) override
	{
		return OriginalHandler->OnTouchMoved(Location, Force, TouchIndex, ControllerId);
	}
	virtual bool OnTouchEnded(const FVector2D& Location, int32 TouchIndex, int32 ControllerId) override
	{
		return OriginalHandler->OnTouchEnded(Location, TouchIndex, ControllerId);
	}
	virtual bool OnTouchForceChanged(const FVector2D& Location, float Force, int32 TouchIndex, int32 ControllerId) override
	{
		return OriginalHandler->OnTouchForceChanged(Location, Force, TouchIndex, ControllerId);
	}
	virtual bool OnTouchFirstMove(const FVector2D& Location, float Force, int32 TouchIndex, int32 ControllerId) override
	{
		return OriginalHandler->OnTouchFirstMove(Location, Force, TouchIndex, ControllerId);
	}
	virtual bool OnMotionDetected(const FVector& Tilt, const FVector& RotationRate, const FVector& Gravity,
		const FVector& Acceleration, int32 ControllerId) override
	{
		return OriginalHandler->OnMotionDetected(Tilt, RotationRate, Gravity, Acceleration, ControllerId);
	}

private:
	/** The original message handler we forward to. */
	TSharedPtr<FGenericApplicationMessageHandler> OriginalHandler;
};
