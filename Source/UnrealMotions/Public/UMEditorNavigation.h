#pragma once

#include "CoreMinimal.h"
#include "Framework/Application/SlateApplication.h"
#include "UMLogger.h"

DECLARE_LOG_CATEGORY_EXTERN(LogUMEditorNavigation, Log, All);

class FUMEditorNavigation
{
public:
	FUMEditorNavigation();
	~FUMEditorNavigation();
	static const TSharedPtr<FUMEditorNavigation> Get();

	static void NavigatePanelTabs(
		FSlateApplication& SlateApp, const FKeyEvent& InKey);

	static TSharedPtr<FUMEditorNavigation> EditorNavigation;
	static FUMLogger					   Logger;
};
