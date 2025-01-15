#include "UMFocusDebuggerUtilityWidget.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "UMSlateHelpers.h"
#include "Widgets/Docking/SDockTab.h"

void UUMFocusDebuggerUtilityWidget::DebugFocus(
	FString& OutActiveWindowName,
	FString& OutActiveMajorTabName,
	FString& OutActiveMinorTabName,
	FString& OutActiveMinorTabMajorTabParentName,
	FString& OutActiveWidgetName)
{
	static const FString DefaultName = "NotFound / Invalid";
	FSlateApplication&	 SlateApp = FSlateApplication::Get();

	if (const TSharedPtr<SWindow> ActiveWindow =
			SlateApp.GetActiveTopLevelRegularWindow())
		OutActiveWindowName = ActiveWindow->GetTitle().ToString();
	else
		OutActiveWindowName = DefaultName;

	TSharedPtr<SDockTab> FrontmostMajorTab;
	if (FUMSlateHelpers::GetFrontmostForegroundedMajorTab(FrontmostMajorTab))
		OutActiveMajorTabName = FrontmostMajorTab->GetTabLabel().ToString();
	else
		OutActiveMajorTabName = DefaultName;

	TSharedRef<FGlobalTabmanager>
		GTM = FGlobalTabmanager::Get();

	if (const TSharedPtr<SDockTab> ActiveMinorTab =
			FUMSlateHelpers::GetActiveMinorTab())
	{
		OutActiveMinorTabName = ActiveMinorTab->GetTabLabel().ToString();

		if (const auto ParentMajorTab = FUMSlateHelpers::GetActiveMajorTab())
		{
			OutActiveMinorTabMajorTabParentName =
				ActiveMinorTab->GetTabLabel().ToString();
		}
	}
	else
	{
		OutActiveMinorTabName = DefaultName;
		OutActiveMinorTabMajorTabParentName = DefaultName;
	}

	if (const auto ActiveWidget = SlateApp.GetUserFocusedWidget(0))
		OutActiveWidgetName = ActiveWidget->GetTypeAsString();
	else
		OutActiveWidgetName = DefaultName;
}
