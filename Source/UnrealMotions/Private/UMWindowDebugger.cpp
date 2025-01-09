// Fill out your copyright notice in the Description page of Project Settings.

#include "UMWindowDebugger.h"
#include "UMWindowsNavigationManager.h"
#include "Framework/Docking/TabManager.h"

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
		case EUMWindowQueryType::AllChildWindows:
			TSharedPtr<SWindow> RootWin = FGlobalTabmanager::Get()->GetRootWindow();
			if (!RootWin.IsValid())
				return;
			TopLevelWins = RootWin->GetChildWindows();
			break;
	}

	for (const auto& Win : TopLevelWins)
	{
		FUMWindowDebugMap WinInfo;

		WinInfo.PropertyMap.Add(TEXT("Title"),
			Win->GetTitle().ToString());

		WinInfo.PropertyMap.Add(TEXT("ID"),
			FString::FromInt(Win->GetId()));

		WinInfo.PropertyMap.Add(TEXT("IsActive"),
			Win->IsActive() ? TEXT("True") : TEXT("False"));

		WinInfo.PropertyMap.Add(TEXT("IsVisible"),
			Win->IsVisible() ? TEXT("True") : TEXT("False"));

		WinInfo.PropertyMap.Add(TEXT("IsRegularWindow"),
			Win->IsRegularWindow() ? TEXT("True") : TEXT("False"));

		WinInfo.PropertyMap.Add(TEXT("IsModal"),
			Win->IsModalWindow() ? TEXT("True") : TEXT("False"));

		WinInfo.PropertyMap.Add(TEXT("IsTopmost"),
			Win->IsTopmostWindow() ? TEXT("True") : TEXT("False"));

		WinInfo.PropertyMap.Add(TEXT("IsWindowMinimized"),
			Win->IsWindowMinimized() ? TEXT("True") : TEXT("False"));

		if (auto UserFocus = Win->HasUserFocus(0))
		{
			FString FocusStr;
			GetFocusCauseString(UserFocus.GetValue(), FocusStr);
			WinInfo.PropertyMap.Add(TEXT("UserFocusType"), FocusStr);
		}
		WinInfo.PropertyMap.Add(TEXT("HasAnyUserFocus"),
			Win->HasAnyUserFocus() ? TEXT("True") : TEXT("False"));

		WinInfo.PropertyMap.Add(TEXT("HasAnyUserFocusOrFocusedDescendants"),
			Win->HasAnyUserFocusOrFocusedDescendants() ? TEXT("True") : TEXT("False"));

		WinInfo.PropertyMap.Add(TEXT("HasActiveChildren"),
			Win->HasActiveChildren() ? TEXT("True") : TEXT("False"));

		WinInfo.PropertyMap.Add(TEXT("IsEnabled"),
			Win->IsEnabled() ? TEXT("True") : TEXT("False"));

		WinInfo.PropertyMap.Add(TEXT("IsMirror"),
			Win->IsMirrorWindow() ? TEXT("True") : TEXT("False"));

		WinInfo.PropertyMap.Add(TEXT("IsAccessible"),
			Win->IsAccessible() ? TEXT("True") : TEXT("False"));

		WinInfo.PropertyMap.Add(TEXT("IsMorphing"),
			Win->IsMorphing() ? TEXT("True") : TEXT("False"));

		WinInfo.PropertyMap.Add(TEXT("IsFocusedInitially"),
			Win->IsFocusedInitially() ? TEXT("True") : TEXT("False"));

		WinInfo.PropertyMap.Add(TEXT("ActivationPolicy"),
			[&]() { FString PolicyStr; GetWindowActivationPolicyString(Win->ActivationPolicy(), PolicyStr); return PolicyStr; }());

		WinInfo.PropertyMap.Add(TEXT("Tag"),
			Win->GetTag().ToString());

		WinInfo.PropertyMap.Add(TEXT("ToolTip"),
			Win->GetToolTip().IsValid() ? Win->GetToolTip()->GetContentWidget()->ToString() : TEXT("None"));

		WinInfo.PropertyMap.Add(TEXT("IsHDR"),
			Win->GetIsHDR() ? TEXT("True") : TEXT("False"));

		WinInfo.PropertyMap.Add(TEXT("Opacity"),
			FString::SanitizeFloat(Win->GetOpacity()));

		FString WindowModeStr;
		GetWindowModeString(Win->GetWindowMode(), WindowModeStr);
		WinInfo.PropertyMap.Add(TEXT("WindowMode"), WindowModeStr);

		OutDebugInfo.Add(WinInfo);
	}
}

void UUMWindowDebugger::DebugActiveWindow(
	TMap<FString, FString>& OutDebugInfo, EUMActiveWindowQueryType WinQueryType)
{
	FSlateApplication&	App = FSlateApplication::Get();
	TSharedPtr<SWindow> Win;

	switch (WinQueryType)
	{
		case EUMActiveWindowQueryType::ActiveTopLevel:
			Win = App.GetActiveTopLevelWindow();
			break;
		case EUMActiveWindowQueryType::ActiveTopLevelRegular:
			Win = App.GetActiveTopLevelRegularWindow();
			break;
		case EUMActiveWindowQueryType::ActiveModal:
			Win = App.GetActiveModalWindow();
			break;
		case EUMActiveWindowQueryType::VisibleMenu:
			Win = App.GetVisibleMenuWindow();
			break;
		case EUMActiveWindowQueryType::RootWindow:
			Win = FGlobalTabmanager::Get()->GetRootWindow();
			break;

		default:
			Win = App.GetActiveTopLevelWindow();
			break;
	}

	if (Win.IsValid())
	{
		OutDebugInfo.Add(TEXT("Title"), Win->GetTitle().ToString());

		OutDebugInfo.Add(TEXT("ID"), FString::FromInt(Win->GetId()));

		OutDebugInfo.Add(TEXT("IsActive"),
			Win->IsActive() ? TEXT("True") : TEXT("False"));

		OutDebugInfo.Add(TEXT("IsVisible"),
			Win->IsVisible() ? TEXT("True") : TEXT("False"));

		OutDebugInfo.Add(TEXT("IsRegularWindow"),
			Win->IsRegularWindow() ? TEXT("True") : TEXT("False"));

		OutDebugInfo.Add(TEXT("IsModal"),
			Win->IsModalWindow() ? TEXT("True") : TEXT("False"));

		OutDebugInfo.Add(TEXT("IsTopmost"),
			Win->IsTopmostWindow() ? TEXT("True") : TEXT("False"));

		OutDebugInfo.Add(TEXT("IsWindowMinimized"),
			Win->IsWindowMinimized() ? TEXT("True") : TEXT("False"));

		if (auto UserFocus = Win->HasUserFocus(0))
		{
			FString FocusStr;
			GetFocusCauseString(UserFocus.GetValue(), FocusStr);
			OutDebugInfo.Add(TEXT("UserFocusType"), FocusStr);
		}

		OutDebugInfo.Add(TEXT("HasAnyUserFocus"),
			Win->HasAnyUserFocus() ? TEXT("True") : TEXT("False"));

		OutDebugInfo.Add(TEXT("HasAnyUserFocusOrFocusedDescendants"),
			Win->HasAnyUserFocusOrFocusedDescendants() ? TEXT("True") : TEXT("False"));

		OutDebugInfo.Add(TEXT("HasActiveChildren"),
			Win->HasActiveChildren() ? TEXT("True") : TEXT("False"));

		OutDebugInfo.Add(TEXT("IsEnabled"),
			Win->IsEnabled() ? TEXT("True") : TEXT("False"));

		OutDebugInfo.Add(TEXT("IsMirror"),
			Win->IsMirrorWindow() ? TEXT("True") : TEXT("False"));

		OutDebugInfo.Add(TEXT("IsAccessible"),
			Win->IsAccessible() ? TEXT("True") : TEXT("False"));

		OutDebugInfo.Add(TEXT("IsMorphing"),
			Win->IsMorphing() ? TEXT("True") : TEXT("False"));

		OutDebugInfo.Add(TEXT("IsFocusedInitially"),
			Win->IsFocusedInitially() ? TEXT("True") : TEXT("False"));

		OutDebugInfo.Add(TEXT("ActivationPolicy"),
			[&]() { FString PolicyStr; GetWindowActivationPolicyString(Win->ActivationPolicy(), PolicyStr); return PolicyStr; }());

		OutDebugInfo.Add(TEXT("Tag"), Win->GetTag().ToString());

		OutDebugInfo.Add(TEXT("ToolTip"),
			Win->GetToolTip().IsValid() ? Win->GetToolTip()->GetContentWidget()->ToString() : TEXT("None"));

		OutDebugInfo.Add(TEXT("IsHDR"),
			Win->GetIsHDR() ? TEXT("True") : TEXT("False"));

		OutDebugInfo.Add(TEXT("Opacity"),
			FString::SanitizeFloat(Win->GetOpacity()));

		FString WindowModeStr;
		GetWindowModeString(Win->GetWindowMode(), WindowModeStr);
		OutDebugInfo.Add(TEXT("WindowMode"),
			WindowModeStr);

		const TArray<TSharedRef<SWindow>> ChildWins = Win->GetChildWindows();
		if (ChildWins.Num() > 0)
		{
			FString ChildWindowNames;
			for (int32 i = 0; i < ChildWins.Num(); i++)
			{
				const auto& Child = ChildWins[i];
				ChildWindowNames.Append(FString::Printf(TEXT("[%d] %s, "), i, *Child->GetTitle().ToString()));
			}
			// Remove trailing comma and space
			if (!ChildWindowNames.IsEmpty())
			{
				ChildWindowNames.RemoveAt(ChildWindowNames.Len() - 2, 2);
			}
			OutDebugInfo.Add(TEXT("ChildWindows"), ChildWindowNames);
		}
		else
		{
			OutDebugInfo.Add(TEXT("ChildWindows"), TEXT("None"));
		}

		Win->GetWindowVisibility();
		Win->GetVisibility();
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
	// if (FUMTabNavigationManager::IsInitialized())
	// {
	const TMap<uint64, TWeakPtr<SWindow>>& Wins = FUMWindowsNavigationManager::Get().GetTrackedWindows();

	for (const auto& Win : Wins)
	{
		if (Win.Value.IsValid())
		{
			const TSharedPtr<SWindow>& EdWin = Win.Value.Pin();
			FString					   LogVal = EdWin->IsActive()
								   ? "{ Currently Active } "
								   : "{ Currently Inactive } ";
			WindowsNames.Add(LogVal + EdWin->GetTitle().ToString());
		}
	}
	// }
	// else
	// {
	// 	WindowsNames.Add("Tab Navigation Manager it NOT initialized!");
	// }
}
