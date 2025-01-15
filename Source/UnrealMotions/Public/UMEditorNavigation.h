#pragma once

#include "CoreMinimal.h"
#include "Framework/Application/SlateApplication.h"
#include "UMLogger.h"

DECLARE_LOG_CATEGORY_EXTERN(LogUMEditorNavigation, Log, All);

class FUMEditorNavigation
{
public:
	static void NavigatePanelTabs(
		FSlateApplication& SlateApp, const FKeyEvent& InKey);

	static FUMLogger Logger;
};
