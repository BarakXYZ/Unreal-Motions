#pragma once

#include "CoreMinimal.h"
#include "Framework/Application/SlateApplication.h"
#include "Templates/SharedPointer.h"

class FUMEditorNavigationHelpers
{
public:
	FUMEditorNavigationHelpers();
	~FUMEditorNavigationHelpers();
	static const TSharedPtr<FUMEditorNavigationHelpers> Get();

	static bool GetWidgetInDirection(const TSharedRef<SWidget> InBaseWidget,
		EKeys Direction, TSharedPtr<SWidget>& OutWidget);

	static TSharedPtr<FUMEditorNavigationHelpers> EditorNavigationHelpers;
};
