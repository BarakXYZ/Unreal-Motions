#pragma once

#include "CoreMinimal.h"
#include "BlueprintEditor.h"
#include "Layout/PaintGeometry.h"
#include "UMDebugWidget.h"

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
	void GetWidgetWindowSpaceGeometry(const TSharedPtr<SWidget>& InWidget, const TSharedPtr<SWindow>& WidgetWindow, FPaintGeometry& WindowSpaceGeometry);
	void DrawWidgetDebugOutline(const TSharedPtr<SWidget>& InWidget);

public:
	FSlateColor				   FocusedBorderColor = FLinearColor(10.0, 10.0, 0.0f);
	TWeakPtr<SBorder>		   LastActiveBorder = nullptr;
	TSharedPtr<SUMDebugWidget> DebugWidgetInstance = nullptr;
	TSharedPtr<SOverlay>	   DebugOverlay = nullptr;
};
