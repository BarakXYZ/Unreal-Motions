// Fill out your copyright notice in the Description page of Project Settings.

#include "UMWindowDebugger.h"
#include "UMTabNavigationManager.h"

// In the cpp file:
void UUMWindowDebugger::DebugAllWindows(
	TArray<FUMWindowDebugMap>& OutDebugInfo, EUMWindowQueryType WinQueryType)
{
	TArray<TSharedRef<SWindow>> TopLevelWins;

	switch (WinQueryType)
	{
		case EUMWindowQueryType::AllVisibleOrdered:
			FSlateApplication::Get().GetAllVisibleWindowsOrdered(TopLevelWins);
			break;
		case EUMWindowQueryType::TopLevel:
			TopLevelWins = FSlateApplication::Get().GetTopLevelWindows();
			break;
		case EUMWindowQueryType::InteractiveTopLevel:
			TopLevelWins = FSlateApplication::Get().GetInteractiveTopLevelWindows();
			break;
		default:
			FSlateApplication::Get().GetAllVisibleWindowsOrdered(TopLevelWins);
			break;
	}

	for (const auto& Win : TopLevelWins)
	{
		FUMWindowDebugMap WindowInfo;

		WindowInfo.PropertyMap.Add(TEXT("Title"), Win->GetTitle().ToString());
		WindowInfo.PropertyMap.Add(TEXT("ID"), FString::FromInt(Win->GetId()));
		WindowInfo.PropertyMap.Add(TEXT("bIsActive"), Win->IsActive() ? TEXT("True") : TEXT("False"));
		WindowInfo.PropertyMap.Add(TEXT("bIsVisible"), Win->IsVisible() ? TEXT("True") : TEXT("False"));
		WindowInfo.PropertyMap.Add(TEXT("bIsModal"), Win->IsModalWindow() ? TEXT("True") : TEXT("False"));
		WindowInfo.PropertyMap.Add(TEXT("bIsTopmost"), Win->IsTopmostWindow() ? TEXT("True") : TEXT("False"));
		if (auto UserFocus = Win->HasUserFocus(0))
		{
			FString FocusStr;
			GetFocusCauseString(UserFocus.GetValue(), FocusStr);
			WindowInfo.PropertyMap.Add(TEXT("UserFocusType"), FocusStr);
		}
		WindowInfo.PropertyMap.Add(TEXT("bHasActiveChildren"), Win->HasActiveChildren() ? TEXT("True") : TEXT("False"));
		WindowInfo.PropertyMap.Add(TEXT("bIsEnabled"), Win->IsEnabled() ? TEXT("True") : TEXT("False"));
		WindowInfo.PropertyMap.Add(TEXT("bIsMirror"), Win->IsMirrorWindow() ? TEXT("True") : TEXT("False"));
		WindowInfo.PropertyMap.Add(TEXT("bIsAccessible"), Win->IsAccessible() ? TEXT("True") : TEXT("False"));
		WindowInfo.PropertyMap.Add(TEXT("bIsMorphing"), Win->IsMorphing() ? TEXT("True") : TEXT("False"));
		WindowInfo.PropertyMap.Add(TEXT("bIsFocusedInitially"), Win->IsFocusedInitially() ? TEXT("True") : TEXT("False"));
		WindowInfo.PropertyMap.Add(TEXT("ActivationPolicy"), [&]() { FString PolicyStr; GetWindowActivationPolicyString(Win->ActivationPolicy(), PolicyStr); return PolicyStr; }());
		WindowInfo.PropertyMap.Add(TEXT("Tag"), Win->GetTag().ToString());
		WindowInfo.PropertyMap.Add(TEXT("ToolTip"),
			Win->GetToolTip().IsValid() ? Win->GetToolTip()->GetContentWidget()->ToString() : TEXT("None"));
		WindowInfo.PropertyMap.Add(TEXT("IsHDR"), Win->GetIsHDR() ? TEXT("True") : TEXT("False"));
		WindowInfo.PropertyMap.Add(TEXT("Opacity"), FString::SanitizeFloat(Win->GetOpacity()));
		FString WindowModeStr;
		GetWindowModeString(Win->GetWindowMode(), WindowModeStr);
		WindowInfo.PropertyMap.Add(TEXT("WindowMode"), WindowModeStr);

		OutDebugInfo.Add(WindowInfo);
	}
}

void UUMWindowDebugger::DebugActiveWindow(
	TMap<FString, FString>& OutDebugInfo, EUMActiveWindowQueryType WinQueryType)
{
	FSlateApplication&	App = FSlateApplication::Get();
	TSharedPtr<SWindow> ActiveWindow;

	switch (WinQueryType)
	{
		case EUMActiveWindowQueryType::ActiveTopLevel:
			ActiveWindow = App.GetActiveTopLevelWindow();
			break;
		case EUMActiveWindowQueryType::ActiveTopLevelRegular:
			ActiveWindow = App.GetActiveTopLevelRegularWindow();
			break;
		case EUMActiveWindowQueryType::ActiveModal:
			ActiveWindow = App.GetActiveModalWindow();
			break;
		case EUMActiveWindowQueryType::VisibleMenu:
			ActiveWindow = App.GetVisibleMenuWindow();
			break;
		default:
			ActiveWindow = App.GetActiveTopLevelWindow();
			break;
	}

	if (ActiveWindow.IsValid())
	{
		OutDebugInfo.Add(TEXT("Title"), ActiveWindow->GetTitle().ToString());
		OutDebugInfo.Add(TEXT("ID"), FString::FromInt(ActiveWindow->GetId()));
		OutDebugInfo.Add(TEXT("bIsActive"), ActiveWindow->IsActive() ? TEXT("True") : TEXT("False"));
		OutDebugInfo.Add(TEXT("bIsVisible"), ActiveWindow->IsVisible() ? TEXT("True") : TEXT("False"));
		OutDebugInfo.Add(TEXT("bIsModal"), ActiveWindow->IsModalWindow() ? TEXT("True") : TEXT("False"));
		OutDebugInfo.Add(TEXT("bIsTopmost"), ActiveWindow->IsTopmostWindow() ? TEXT("True") : TEXT("False"));
		if (auto UserFocus = ActiveWindow->HasUserFocus(0))
		{
			FString FocusStr;
			GetFocusCauseString(UserFocus.GetValue(), FocusStr);
			OutDebugInfo.Add(TEXT("UserFocusType"), FocusStr);
		}
		OutDebugInfo.Add(TEXT("bHasActiveChildren"), ActiveWindow->HasActiveChildren() ? TEXT("True") : TEXT("False"));
		OutDebugInfo.Add(TEXT("bIsEnabled"), ActiveWindow->IsEnabled() ? TEXT("True") : TEXT("False"));
		OutDebugInfo.Add(TEXT("bIsMirror"), ActiveWindow->IsMirrorWindow() ? TEXT("True") : TEXT("False"));
		OutDebugInfo.Add(TEXT("bIsAccessible"), ActiveWindow->IsAccessible() ? TEXT("True") : TEXT("False"));
		OutDebugInfo.Add(TEXT("bIsMorphing"), ActiveWindow->IsMorphing() ? TEXT("True") : TEXT("False"));
		OutDebugInfo.Add(TEXT("bIsFocusedInitially"), ActiveWindow->IsFocusedInitially() ? TEXT("True") : TEXT("False"));
		OutDebugInfo.Add(TEXT("ActivationPolicy"), [&]() { FString PolicyStr; GetWindowActivationPolicyString(ActiveWindow->ActivationPolicy(), PolicyStr); return PolicyStr; }());
		OutDebugInfo.Add(TEXT("Tag"), ActiveWindow->GetTag().ToString());
		OutDebugInfo.Add(TEXT("ToolTip"),
			ActiveWindow->GetToolTip().IsValid() ? ActiveWindow->GetToolTip()->GetContentWidget()->ToString() : TEXT("None"));
		OutDebugInfo.Add(TEXT("IsHDR"), ActiveWindow->GetIsHDR() ? TEXT("True") : TEXT("False"));
		OutDebugInfo.Add(TEXT("Opacity"), FString::SanitizeFloat(ActiveWindow->GetOpacity()));
		FString WindowModeStr;
		GetWindowModeString(ActiveWindow->GetWindowMode(), WindowModeStr);
		OutDebugInfo.Add(TEXT("WindowMode"), WindowModeStr);
	}
}

void UUMWindowDebugger::GetWindowActivationPolicyString(EWindowActivationPolicy Policy, FString& OutString)
{
	switch (Policy)
	{
		case EWindowActivationPolicy::Never:
			OutString = TEXT("Never");
			break;
		case EWindowActivationPolicy::Always:
			OutString = TEXT("Always");
			break;
		case EWindowActivationPolicy::FirstShown:
			OutString = TEXT("FirstShown");
			break;
		default:
			OutString = TEXT("Unknown");
			break;
	}
}

void UUMWindowDebugger::GetFocusCauseString(
	EFocusCause FocusCause, FString& OutString)
{
	switch (FocusCause)
	{
		case EFocusCause::Mouse:
			OutString = TEXT("Mouse");
			break;
		case EFocusCause::Navigation:
			OutString = TEXT("Navigation");
			break;
		case EFocusCause::SetDirectly:
			OutString = TEXT("SetDirectly");
			break;
		case EFocusCause::Cleared:
			OutString = TEXT("Cleared");
			break;
		case EFocusCause::OtherWidgetLostFocus:
			OutString = TEXT("OtherWidgetLostFocus");
			break;
		case EFocusCause::WindowActivate:
			OutString = TEXT("WindowActivate");
			break;
		default:
			OutString = TEXT("Unknown");
			break;
	}
}

void UUMWindowDebugger::GetWindowModeString(
	EWindowMode::Type WindowMode, FString& OutString)
{
	switch (WindowMode)
	{
		case EWindowMode::Fullscreen:
			OutString = TEXT("Fullscreen");
			break;
		case EWindowMode::WindowedFullscreen:
			OutString = TEXT("WindowedFullscreen");
			break;
		case EWindowMode::Windowed:
			OutString = TEXT("Windowed");
			break;
		default:
			OutString = TEXT("Unknown");
			break;
	}
}

void UUMWindowDebugger::DebugTrackedWindows(TArray<FString>& WindowsNames)
{
	FUMTabNavigationManager::Get()->TrackedWindows;
}
