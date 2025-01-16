#pragma once

#include "CoreMinimal.h"
#include "BlueprintEditor.h"
#include "Layout/PaintGeometry.h"
#include "UMDebugWidget.h"
#include "SUMFocusDebugOverlay.h"

class FUMFocusVisualizer
{
public:
	FUMFocusVisualizer();
	~FUMFocusVisualizer();

	static TSharedRef<FUMFocusVisualizer> Get();

	void GetLastActiveEditor();

	/**
	 * Ensures we have created/added the debug overlay widget
	 * to the current top-level window’s overlay, if not already.
	 */
	void EnsureDebugOverlayExists();

	/**
	 * Recompute the window-space geometry for InWidget
	 * and tell our debug overlay to highlight it.
	 */
	void UpdateDebugOverlay(const TSharedPtr<SWidget>& InWidget);

	void GetWidgetWindowSpaceGeometry(const TSharedPtr<SWidget>& InWidget, const TSharedPtr<SWindow>& WidgetWindow, FPaintGeometry& WindowSpaceGeometry);

	void DrawWidgetDebugOutline(const TSharedPtr<SWidget>& InWidget);

	/**
	 * Called externally (or from some global callback) when you detect a new window
	 * has opened. Then we can add an overlay to that new window.
	 */
	void OnWindowCreated(const TSharedRef<SWindow>& InNewWindow);

	/**
	 * Called externally (or from some global callback) when a window is about to be closed,
	 * so we can remove the overlay from our map. (Optional, but good housekeeping.)
	 */
	void OnWindowClosed(const TSharedRef<SWindow>& InWindow);

	void DrawDebugOutlineOnWidget(const TSharedRef<SWidget> InWidget);

private:
	/**
	 * Ensures there's a debug overlay in the given window. If none, create one
	 * and store it in OverlaysByWindow with a high Z-order for always-on-top (within that window).
	 */
	void CreateOverlayIfNeeded(const TSharedPtr<SWindow>& InWindow);

	/**
	 * Recompute the window-space geometry for the given widget in the given window,
	 * then update that window’s debug overlay.
	 */
	void UpdateOverlayInWindow(
		const TSharedPtr<SWindow>& InWindow,
		const TSharedPtr<SWidget>& InWidget);

public:
	FSlateColor				   FocusedBorderColor = FLinearColor(10.0, 10.0, 0.0f);
	TWeakPtr<SBorder>		   LastActiveBorder = nullptr;
	TSharedPtr<SUMDebugWidget> DebugWidgetInstance = nullptr;
	TSharedPtr<SOverlay>	   DebugOverlay = nullptr;

	// New
private:
	/** A pointer to the single debug overlay widget we keep around at all times. */
	TSharedPtr<SUMFocusDebugOverlay> DebugOverlayWidget;
	TSharedPtr<SUMFocusDebugOverlay> ActiveDebugOverlayWidget;

	/** A weak pointer to the window in which our overlay resides. */
	TWeakPtr<SWindow>										  DebugOverlayWindow;
	TMap<TWeakPtr<SWindow>, TSharedPtr<SUMFocusDebugOverlay>> OverlaysByWindow;
};
