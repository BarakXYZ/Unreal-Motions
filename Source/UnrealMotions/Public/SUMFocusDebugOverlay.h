#pragma once

#include "CoreMinimal.h"
#include "Layout/Visibility.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "VimInputProcessor.h"

DECLARE_MULTICAST_DELEGATE_OneParam(
	FUMOnOutlineOverlayVisibilityChanged, bool /* bIsVisible */)

	/**
	 * A per-window debug overlay that can draw an outline over any widget(s)
	 * in that window. SetVisibility(EVisibility::HitTestInvisible) so it
	 * never intercepts clicks.
	 */
	class SUMFocusDebugOverlay : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SUMFocusDebugOverlay) {}
	/** Optionally pass a default geometry */
	SLATE_ARGUMENT(TOptional<FPaintGeometry>, InitialGeometry)
	/** Optionally control default visibility */
	SLATE_ARGUMENT(EVisibility, InitialVisibility)
	SLATE_END_ARGS()

	/** Construct the overlay. */
	void Construct(const FArguments& InArgs);

	/**
	 * Sets (or updates) the geometry you want to highlight.
	 * @param InGemoetry - The new geometry to visualize
	 */
	void SetTargetGeometry(const FPaintGeometry& InGeometry);

	/** Clears the geometry so we draw nothing. */
	void ClearTargetGeometry();

	void ToggleOutlineActiveColor(bool bIsActive);

	static void ToggleVisibility(bool bIsVisible);

	void HandleOnVisibilityChanged(bool bIsVisible);

	void HandleOnVimModeChanged(EVimMode NewVimMode);

	//~ Begin SWidget interface
	virtual int32 OnPaint(
		const FPaintArgs&		 Args,
		const FGeometry&		 AllottedGeometry,
		const FSlateRect&		 MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32					 LayerId,
		const FWidgetStyle&		 InWidgetStyle,
		bool					 bParentEnabled) const override;
	//~ End SWidget interface

private:
	/** The geometry we want to highlight in this window’s space. */
	TOptional<FPaintGeometry> TargetGeometry;
	FLinearColor			  OutlineColor = FLinearColor(1.0f, 0.0f, 1.0f, 1.0f);
	EVisibility				  Visibility;

public:
	static FUMOnOutlineOverlayVisibilityChanged OnOutlineOverlayVisibilityChanged;
};
