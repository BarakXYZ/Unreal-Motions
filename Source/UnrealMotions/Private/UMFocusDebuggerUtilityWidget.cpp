#include "UMFocusDebuggerUtilityWidget.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "UMSlateHelpers.h"
#include "Widgets/Docking/SDockTab.h"

void UUMFocusDebuggerUtilityWidget::DebugFocus(TMap<FString, FString>& OutMap)
{
	static const FString DefaultInvalidStr = "NotFound / Invalid";
	FSlateApplication&	 SlateApp = FSlateApplication::Get();

	if (const TSharedPtr<SWindow> ActiveWindow =
			SlateApp.GetActiveTopLevelRegularWindow())
		OutMap.Add("ActiveWindow", ActiveWindow->GetTitle().ToString());
	else
		OutMap.Add("ActiveWindow", DefaultInvalidStr);

	TSharedPtr<SDockTab> FrontmostMajorTab;
	if (FUMSlateHelpers::GetFrontmostForegroundedMajorTab(FrontmostMajorTab))
		OutMap.Add("ActiveMajorTab", FrontmostMajorTab->GetTabLabel().ToString());
	else
		OutMap.Add("ActiveMajorTab", DefaultInvalidStr);

	TSharedRef<FGlobalTabmanager>
		GTM = FGlobalTabmanager::Get();

	if (const TSharedPtr<SDockTab> ActiveMinorTab =
			FUMSlateHelpers::GetActiveMinorTab())
	{
		OutMap.Add("ActiveMinorTab", ActiveMinorTab->GetTabLabel().ToString());

		if (const auto ParentMajorTab = FUMSlateHelpers::GetActiveMajorTab())
		{
			OutMap.Add("ActiveMinorTabMajorTabParent",
				ActiveMinorTab->GetTabLabel().ToString());
		}
	}
	else
	{
		OutMap.Add("ActiveMinorTab", DefaultInvalidStr);
		OutMap.Add("ActiveMinorTabMajorTabParent", DefaultInvalidStr);
	}

	if (const auto ActiveWidget = SlateApp.GetUserFocusedWidget(0))
	{
		OutMap.Add("ActiveWidgetSpecificType", ActiveWidget->GetTypeAsString());
		OutMap.Add("ActiveWidgetClassType", ActiveWidget->GetWidgetClass().GetWidgetType().ToString());
	}
	else
	{
		OutMap.Add("ActiveWidgetSpecificType", DefaultInvalidStr);
		OutMap.Add("ActiveWidgetClassType", DefaultInvalidStr);
	}
}
