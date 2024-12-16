#pragma once

#include "CoreMinimal.h"
#include "BlueprintEditor.h"
#include "Framework/Application/SlateApplication.h"

class FUMGraphNavigationManager
{
public:
	FUMGraphNavigationManager();
	~FUMGraphNavigationManager();
	void GetLastActiveEditor();
	void DebugOnFocusChanged(
		const FFocusEvent&		   FocusEvent,
		const FWeakWidgetPath&	   WeakWidgetPath,
		const TSharedPtr<SWidget>& OldWidget,
		const FWidgetPath&		   WidgetPath,
		const TSharedPtr<SWidget>& NewWidget);
	void StartDebugOnFocusChanged();

public:
	// FSlateColor		  FocusedBorderColor = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("#854D0E")));
	// FSlateColor		  FocusedBorderColor = FLinearColor::FromSRGBColor(FColor(161, 98, 7, 255));
	FSlateColor		  FocusedBorderColor = FLinearColor(10.0, 10.0, 0.0f);
	TWeakPtr<SBorder> LastActiveBorder = nullptr;
};
