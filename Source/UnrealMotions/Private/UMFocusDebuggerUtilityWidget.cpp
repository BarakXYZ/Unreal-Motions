#include "UMFocusDebuggerUtilityWidget.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "UMSlateHelpers.h"
#include "Widgets/Docking/SDockTab.h"

void UUMFocusDebuggerUtilityWidget::DebugFocus(TMap<FString, FString>& OutMap)
{
	static const FString DefaultName = "NotFound / Invalid";
	FSlateApplication&	 SlateApp = FSlateApplication::Get();

	if (const TSharedPtr<SWindow> ActiveWindow =
			SlateApp.GetActiveTopLevelRegularWindow())
		OutMap.Add("ActiveWindow", ActiveWindow->GetTitle().ToString());
	else
		OutMap.Add("ActiveWindow", DefaultName);

	TSharedPtr<SDockTab> FrontmostMajorTab;
	if (FUMSlateHelpers::GetFrontmostForegroundedMajorTab(FrontmostMajorTab))
		OutMap.Add("ActiveMajorTab", FrontmostMajorTab->GetTabLabel().ToString());
	else
		OutMap.Add("ActiveMajorTab", DefaultName);

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
		OutMap.Add("ActiveMinorTab", DefaultName);
		OutMap.Add("ActiveMinorTabMajorTabParent", DefaultName);
	}

	if (const auto ActiveWidget = SlateApp.GetUserFocusedWidget(0))
		OutMap.Add("ActiveWidget", ActiveWidget->GetTypeAsString());
	else
		OutMap.Add("ActiveWidget", DefaultName);
}
