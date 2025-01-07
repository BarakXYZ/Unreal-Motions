#pragma once

#include "CoreMinimal.h"
#include "Framework/Application/SlateApplication.h"
#include "UMFocusManager.h"

class FUMEditorNavigation
{
public:
	FUMEditorNavigation();
	~FUMEditorNavigation();
	static const TSharedPtr<FUMEditorNavigation> Get();

	static void NavigatePanelTabs(
		FSlateApplication& SlateApp, const FKeyEvent& InKey);

	static TSharedPtr<FUMEditorNavigation> EditorNavigation;

	TSharedPtr<FUMFocusManager> FocusManager;
};
