#include "UMFocusVisualizer.h"

#include "ILevelEditor.h"
#include "Layout/PaintGeometry.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "GraphEditor.h"
#include "UMLogger.h"
#include "Framework/Application/SlateApplication.h"
#include "Layout/WidgetPath.h"
#include "Misc/Optional.h"
#include "Styling/CoreStyle.h"
#include "Rendering/DrawElements.h"
#include "Widgets/SWindow.h"
#include "Framework/Docking/TabManager.h"

#include "UMTabsNavigationManager.h"

TSharedPtr<FUMFocusVisualizer> FUMFocusVisualizer::FocusVisualizer =
	MakeShared<FUMFocusVisualizer>();

FUMFocusVisualizer::FUMFocusVisualizer()
{
	// FCoreDelegates::OnPostEngineInit.AddLambda([this]() {
	// 	StartDebugOnFocusChanged();
	// });
	StartDebugOnFocusChanged();
	FUMTabsNavigationManager::Get().OnNewMajorTabChanged.AddLambda(
		[this](TWeakPtr<SDockTab> MajorTab, TWeakPtr<SDockTab> MinorTab) {
			// DrawDebugOutlineOnWidget(MinorTab.Pin()->GetContent());
			// DrawDebugOutlineOnWidget(MajorTab.Pin()->GetContent());
			// DrawDebugOutlineOnWidget(
			// 	MinorTab.Pin()->GetParentWidget().ToSharedRef());
			DrawDebugOutlineOnWidget(
				MinorTab.Pin()->GetContent());
		});
}

FUMFocusVisualizer::~FUMFocusVisualizer()
{
	if (LastActiveBorder.IsValid())
		LastActiveBorder.Reset();
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication& App = FSlateApplication::Get();
		App.OnFocusChanging().RemoveAll(this);
	}

	// Optionally remove all overlays from their windows
	for (auto& Pair : OverlaysByWindow)
	{
		if (Pair.Key.IsValid() && Pair.Value.IsValid())
		{
			Pair.Key.Pin()->RemoveOverlaySlot(Pair.Value.ToSharedRef());
		}
	}
	OverlaysByWindow.Empty();
}

const TSharedPtr<FUMFocusVisualizer> FUMFocusVisualizer::Get()
{
	return FocusVisualizer;
}

void FUMFocusVisualizer::GetLastActiveEditor()
{
	UAssetEditorSubsystem* AssetEditorSubsystem =
		GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();

	const TArray<UObject*> EditedAssets{ AssetEditorSubsystem->GetAllEditedAssets() };
	IAssetEditorInstance*  ActiveEditorInstance{ nullptr };
	double				   ActivationTime{ 0 };

	// Seems to not be correct when some assets are in a separated window!
	// But we can still match and fetch the currently edited asset by comparing
	// against the major tab label or something
	for (UObject* EditedAsset : EditedAssets)
	{
		IAssetEditorInstance* EditorInstance =
			AssetEditorSubsystem->FindEditorForAsset(EditedAsset, false);

		double CurrEditorActTime = EditorInstance->GetLastActivationTime();
		if (CurrEditorActTime > ActivationTime)
		{
			ActiveEditorInstance = EditorInstance;
			ActivationTime = CurrEditorActTime;
		}
		// FString OutStr = EditorInstance->GetEditorName().ToString() + ": " + FString::SanitizeFloat(EditorInstance->GetLastActivationTime());
		// FUMLogger::NotifySuccess(FText::FromString(OutStr));
	}
	if (!ActiveEditorInstance)
		return;

	// FString OutStr = ActiveEditorInstance->GetEditorName().ToString()
	// 	+ ": " + ActiveEditorInstance->GetEditingAssetTypeName().ToString();
	// FUMLogger::NotifySuccess(FText::FromString(OutStr));

	FAssetEditorToolkit* AssetEditorToolkit =
		static_cast<FAssetEditorToolkit*>(ActiveEditorInstance);
	if (!AssetEditorToolkit)
		return;

	FUMLogger::NotifySuccess(
		// FText::FromName(AssetEditorToolkit->GetToolkitName()));
		AssetEditorToolkit->GetToolkitName());
	AssetEditorToolkit->GetInlineContent(); // ???

	AssetEditorToolkit->GetObjectsCurrentlyBeingEdited();

	TSharedPtr<IToolkitHost> ToolkitHost = AssetEditorToolkit->GetToolkitHost();
	ToolkitHost->GetActiveViewportSize();
	UTypedElementCommonActions* Actions = ToolkitHost->GetCommonActions();

	// TSharedPtr<FTabManager> TabManager =
	// 	AssetEditorToolkit->GetTabManager(); // Not helpful
}

void FUMFocusVisualizer::StartDebugOnFocusChanged()
{
	FSlateApplication& App = FSlateApplication::Get();
	App.OnFocusChanging().AddRaw(
		this, &FUMFocusVisualizer::DebugOnFocusChanged);
}

void FUMFocusVisualizer::DebugOnFocusChanged(
	const FFocusEvent&		   FocusEvent,
	const FWeakWidgetPath&	   WeakWidgetPath,
	const TSharedPtr<SWidget>& OldWidget,
	const FWidgetPath&		   WidgetPath,
	const TSharedPtr<SWidget>& NewWidget)
{
	return; // Skip all
	// GetLastActiveEditor();

	// if (!OldWidget.IsValid() || !NewWidget.IsValid())
	// 	return;

	// FString OldWidgetStr = "Old Widget: " + OldWidget->GetTypeAsString();
	// FString NewWidgetStr = "New Widget: " + NewWidget->GetTypeAsString();

	// If you only want to debug certain transitions, you can check that here.
	// E.g. skipping invalid widget transitions, or ignoring certain types:
	if (!NewWidget.IsValid())
		return;

	// We potentially want to have a few different overlay layers of debugging
	// on top of each window:
	// 1. For the entire Window
	// 2. For the currently active Major Tab (that resides inside the Window)
	// 3. For the currently active Minor Tab (that resides inside the Major Tab)
	// 4. For the currently focused Widget (that resided inside the Minor Tab)

	DrawDebugOutlineOnWidget(NewWidget.ToSharedRef());
}

void FUMFocusVisualizer::DrawDebugOutlineOnWidget(
	const TSharedRef<SWidget> InWidget)
{
	// Find the window in which this newly focused widget resides
	// TSharedPtr<SWindow> FoundWindow =
	// 	FSlateApplication::Get().FindWidgetWindow(InWidget);
	TSharedPtr<SWindow> FoundWindow =
		FSlateApplication::Get().GetActiveTopLevelRegularWindow();
	if (!FoundWindow.IsValid())
		return; // Possibly a pop-up or something with no real window

	// FUMLogger::NotifySuccess(
	// 	FText::FromString("Draw Debug Outline On Widget"));

	// Ensure that window has an overlay
	FocusVisualizer->CreateOverlayIfNeeded(FoundWindow);

	// Now highlight the new widget in that window
	FocusVisualizer->UpdateOverlayInWindow(FoundWindow, InWidget);
	return;
}

void FUMFocusVisualizer::OnWindowCreated(const TSharedRef<SWindow>& InNewWindow)
{
	// The moment we learn about a brand new window, we can create an overlay right away
	CreateOverlayIfNeeded(InNewWindow);
}

void FUMFocusVisualizer::OnWindowClosed(const TSharedRef<SWindow>& InWindow)
{
	// Remove from our map and remove the overlay from the window
	TSharedPtr<SUMFocusDebugOverlay> OverlayPtr = OverlaysByWindow.FindRef(InWindow);
	if (OverlayPtr.IsValid())
	{
		InWindow->RemoveOverlaySlot(OverlayPtr.ToSharedRef());
	}
	OverlaysByWindow.Remove(InWindow);
}

void FUMFocusVisualizer::CreateOverlayIfNeeded(const TSharedPtr<SWindow>& InWindow)
{
	if (!InWindow.IsValid())
	{
		return;
	}

	// Already have an overlay for this window?
	if (OverlaysByWindow.Contains(InWindow))
	{
		// FUMLogger::NotifySuccess(FText::FromString("Window already exists: " + InWindow->GetTitle().ToString()));
		return;
	}

	// FUMLogger::NotifySuccess(FText::FromString("New Window Found: " + InWindow->GetTitle().ToString()));

	// Create a new overlay
	TSharedPtr<SUMFocusDebugOverlay> NewOverlay =
		SNew(SUMFocusDebugOverlay)
			.InitialVisibility(EVisibility::HitTestInvisible);

	// Add to the window's overlay with a large Z-order so it’s above typical overlays
	InWindow->AddOverlaySlot(999)
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
			[NewOverlay.ToSharedRef()];

	// Remember it in our map
	OverlaysByWindow.Add(InWindow, NewOverlay);
}

void FUMFocusVisualizer::UpdateOverlayInWindow(
	const TSharedPtr<SWindow>& InWindow,
	const TSharedPtr<SWidget>& InWidget)
{
	if (!InWindow.IsValid() || !InWidget.IsValid())
		return;

	TSharedPtr<SUMFocusDebugOverlay> OverlayPtr =
		OverlaysByWindow.FindRef(InWindow);
	if (!OverlayPtr.IsValid())
		return; // Shouldn’t happen if CreateOverlayIfNeeded was called

	// FUMLogger::NotifySuccess(FText::FromString("Update Overlay In Window"));

	// Now compute geometry. We want geometry relative to the window’s top-left.
	const FGeometry WidgetGeometry = InWidget->GetCachedGeometry();

	// The window’s absolute position in screen space
	const FVector2D WindowScreenPos = InWindow->GetPositionInScreen();

	FPaintGeometry WindowSpaceGeometry = WidgetGeometry.ToPaintGeometry();
	WindowSpaceGeometry.AppendTransform(
		TransformCast<FSlateLayoutTransform>(Inverse(WindowScreenPos)));

	// Update the overlay
	OverlayPtr->SetTargetGeometry(WindowSpaceGeometry);

	if (ActiveDebugOverlayWidget.IsValid())
		ActiveDebugOverlayWidget->ToggleOutlineActiveColor(false); // Deactivate
	ActiveDebugOverlayWidget = OverlayPtr;						   // Track
	ActiveDebugOverlayWidget->ToggleOutlineActiveColor(true);	   // Activate
}

// OLD
void FUMFocusVisualizer::EnsureDebugOverlayExists()
{
	if (DebugOverlayWidget.IsValid())
	{
		// Already created the debug overlay
		return;
	}

	// TSharedPtr<SWindow> ActiveWindow =
	// 	FGlobalTabmanager::Get()->GetRootWindow();
	// Attempt to get the top-level window

	TArray<TSharedRef<SWindow>> VisWins;
	FSlateApplication::Get().GetAllVisibleWindowsOrdered(VisWins);
	TSharedPtr<SWindow> ActiveWindow =
		// FSlateApplication::Get().GetActiveTopLevelRegularWindow();
		// FSlateApplication::Get().GetActiveTopLevelWindow();
		VisWins.Last();
	if (!ActiveWindow.IsValid())
	{
		return;
	}

	// Create the debug overlay widget. It starts with no target geometry.
	DebugOverlayWidget = SNew(SUMFocusDebugOverlay)
							 .Visibility(EVisibility::HitTestInvisible); // 2) Don't consume hits

	// 1) Large z-order & 3) fill the entire window
	ActiveWindow->AddOverlaySlot(/*ZOrder=*/9999)
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
			[DebugOverlayWidget.ToSharedRef()];

	DebugOverlayWindow = ActiveWindow; // store TWeakPtr
}

void FUMFocusVisualizer::UpdateDebugOverlay(const TSharedPtr<SWidget>& InWidget)
{
	if (!InWidget.IsValid() || !DebugOverlayWidget.IsValid())
	{
		return;
	}

	// We assume we’re drawing in the same window as DebugOverlayWidget.
	TSharedPtr<SWindow> WindowPin = DebugOverlayWindow.Pin();
	if (!WindowPin.IsValid())
	{
		return;
	}

	// Convert the widget’s absolute (desktop) geometry into the window’s space
	const FGeometry WidgetGeometry = InWidget->GetCachedGeometry();
	// The “window space” transform is the window’s position in screen space
	const FVector2D WindowScreenPos = WindowPin->GetPositionInScreen();

	// Build a FPaintGeometry that is local to the window’s coordinate space
	// i.e. removing the window’s own screen offset from the widget’s absolute pos.
	FPaintGeometry WindowSpaceGeometry = WidgetGeometry.ToPaintGeometry();
	WindowSpaceGeometry.AppendTransform(
		TransformCast<FSlateLayoutTransform>(
			Inverse(WindowScreenPos)));

	// Now simply tell our debug widget to highlight that geometry
	DebugOverlayWidget->SetTargetGeometry(WindowSpaceGeometry);
}

void FUMFocusVisualizer::DrawWidgetDebugOutline(const TSharedPtr<SWidget>& InWidget)
{
	if (!InWidget.IsValid())
	{
		return;
	}

	FSlateApplication& SlateApp = FSlateApplication::Get();
	if (!SlateApp.IsInitialized())
	{
		return;
	}

	TSharedPtr<SWindow> ActiveWindow = SlateApp.GetActiveTopLevelRegularWindow();
	if (!ActiveWindow.IsValid()
		|| !ActiveWindow->IsAccessible()
		|| ActiveWindow->GetTitle().IsEmpty())
	{
		return;
	}

	// Recalculate geometry for the widget
	FPaintGeometry WindowSpaceGeometry;
	GetWidgetWindowSpaceGeometry(InWidget, ActiveWindow, WindowSpaceGeometry);

	// Remove the previous overlay if it exists
	if (DebugWidgetInstance.IsValid())
	{
		if (DebugWidgetInstance.IsValid())
		{
			if (!ActiveWindow->RemoveOverlaySlot(
					DebugWidgetInstance.ToSharedRef()))
				return;
			else
				DebugWidgetInstance.Reset();
		}
	}
	DebugWidgetInstance = SNew(SUMDebugWidget)
							  .TargetWidget(InWidget.ToWeakPtr())
							  .CustomGeometry(WindowSpaceGeometry)
							  .Visibility(EVisibility::HitTestInvisible);

	ActiveWindow->AddOverlaySlot()
		[DebugWidgetInstance.ToSharedRef()];
}

// ** Made from a few snippets taken from Widget Reflector.
// It looks like without doing that, we will have some offset between the actual widget
// and our outline debugger widget.
// Here's what they have to say:
// The FGeometry we get is from a WidgetPath, so it's rooted in desktop space.
// We need to APPEND a transform to the Geometry to essentially undo this root transform
// and get us back into Window Space.
// This is nonstandard so we have to go through some hoops and a specially exposed method
// in FPaintGeometry to allow appending layout transforms.
// */
void FUMFocusVisualizer::GetWidgetWindowSpaceGeometry(const TSharedPtr<SWidget>& InWidget, const TSharedPtr<SWindow>& WidgetWindow, FPaintGeometry& WindowSpaceGeometry)
{
	if (!InWidget.IsValid())
		return;

	FGeometry WidgetGeometry = InWidget->GetCachedGeometry();
	WindowSpaceGeometry = WidgetGeometry.ToPaintGeometry();
	WindowSpaceGeometry.AppendTransform(
		TransformCast<FSlateLayoutTransform>(
			Inverse(WidgetWindow->GetPositionInScreen())));
	FUMLogger::NotifySuccess(FText::FromString(WindowSpaceGeometry.GetLocalSize().ToString()));
}
