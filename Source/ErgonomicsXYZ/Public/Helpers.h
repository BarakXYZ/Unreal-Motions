// Copyright 2024 BarakXYZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"

class FHelpers
{
public:
	/** Displays a success notification with optional hyperlink */
	static void NotifySuccess(
		const FText&   NotificationText = FText::GetEmpty(),
		const FString& HyperlinkURL = FString(),
		const FText&   HyperlinkText = FText::GetEmpty(),
		float		   Lifetime = 3.0f);
};
