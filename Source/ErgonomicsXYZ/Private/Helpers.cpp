// Copyright 2024 BarakXYZ. All Rights Reserved.

#include "Helpers.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

void FHelpers::NotifySuccess(
	const FText&   NotificationText,
	const FString& HyperlinkURL,
	const FText&   HyperlinkText,
	float		   Lifetime)
{
#if WITH_EDITOR
	FNotificationInfo Info(NotificationText);

	Info.ExpireDuration = Lifetime;
	Info.Image = FCoreStyle::Get().GetBrush("Icons.SuccessWithColor");

	if (!HyperlinkURL.IsEmpty())
	{
		Info.HyperlinkText = HyperlinkText;
		Info.Hyperlink = FSimpleDelegate::CreateLambda([HyperlinkURL]() {
			FPlatformProcess::LaunchURL(*HyperlinkURL, nullptr, nullptr);
		});
	}

	FSlateNotificationManager::Get().AddNotification(Info);
#endif
}
