#include "UMGraphNavigationManager.h"

#include "Layout/PaintGeometry.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "GraphEditor.h"
#include "UMHelpers.h"
#include "Framework/Application/SlateApplication.h"

FUMGraphNavigationManager::FUMGraphNavigationManager()
{
	FCoreDelegates::OnPostEngineInit.AddLambda([this]() {
		StartDebugOnFocusChanged();
	});
}

FUMGraphNavigationManager::~FUMGraphNavigationManager()
{
	if (LastActiveBorder.IsValid())
		LastActiveBorder.Reset();
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication& App = FSlateApplication::Get();
		App.OnFocusChanging().RemoveAll(this);
	}
}

void FUMGraphNavigationManager::GetLastActiveEditor()
{
	UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	TArray<UObject*>	   EditedAssets = AssetEditorSubsystem->GetAllEditedAssets();
	IAssetEditorInstance*  ActiveEditorInstance = nullptr;

	double ActivationTime{ 0 };
	for (UObject* EditedAsset : EditedAssets)
	{
		IAssetEditorInstance* EditorInstance = AssetEditorSubsystem->FindEditorForAsset(EditedAsset, false);
		double				  CurrEditorActTime = EditorInstance->GetLastActivationTime();
		if (CurrEditorActTime > ActivationTime)
		{
			ActiveEditorInstance = EditorInstance;
			ActivationTime = CurrEditorActTime;
		}
		// FString OutStr = EditorInstance->GetEditorName().ToString() + ": " + FString::SanitizeFloat(EditorInstance->GetLastActivationTime());
		// FUMHelpers::NotifySuccess(FText::FromString(OutStr));
	}
	FString OutStr = ActiveEditorInstance->GetEditorName().ToString() + ": " + ActiveEditorInstance->GetEditingAssetTypeName().ToString();
	FUMHelpers::NotifySuccess(FText::FromString(OutStr));

	FAssetEditorToolkit* AssetEditorToolkit = static_cast<FAssetEditorToolkit*>(ActiveEditorInstance);
	if (AssetEditorToolkit)
	{
		FUMHelpers::NotifySuccess(FText::FromName(AssetEditorToolkit->GetToolMenuName()));
		AssetEditorToolkit->GetInlineContent();
	}
}

void FUMGraphNavigationManager::StartDebugOnFocusChanged()
{
	FSlateApplication& App = FSlateApplication::Get();
	App.OnFocusChanging().AddRaw(this, &FUMGraphNavigationManager::DebugOnFocusChanged);
}
void FUMGraphNavigationManager::DebugOnFocusChanged(
	const FFocusEvent&		   FocusEvent,
	const FWeakWidgetPath&	   WeakWidgetPath,
	const TSharedPtr<SWidget>& OldWidget,
	const FWidgetPath&		   WidgetPath,
	const TSharedPtr<SWidget>& NewWidget)
{
	if (OldWidget && NewWidget)
	{
		FString OldWidgetStr = "Old Widget: " + OldWidget->GetTypeAsString();
		FString NewWidgetStr = "New Widget: " + NewWidget->GetTypeAsString();

		if (NewWidget->GetTypeAsString() == "SDockingTabStack")
		{
			if (FChildren* Children = NewWidget->GetChildren())
			{
				TWeakPtr<SWidget> Child = Children->GetChildAt(0);
				if (Child.IsValid())
				{
					if (FChildren* Children2 = Child.Pin()->GetChildren())
					{
						Child = Children2->GetChildAt(0);
						if (Child.Pin()->GetTypeAsString() == "SBorder")
						{
							FUMHelpers::NotifySuccess(FText::FromString("SBorder!"));
							TWeakPtr<SBorder> TempBorder = nullptr;
							if (LastActiveBorder.IsValid())
								TempBorder = LastActiveBorder;
							LastActiveBorder = StaticCastWeakPtr<SBorder>(Child);
							if (LastActiveBorder.IsValid())
							{
								if (LastActiveBorder != TempBorder && TempBorder.IsValid())
									TempBorder.Pin()->SetBorderBackgroundColor(FLinearColor());
								FUMHelpers::NotifySuccess(FText::FromString(LastActiveBorder.Pin()->GetBorderBackgroundColor().GetSpecifiedColor().ToString()));
								LastActiveBorder.Pin()->SetBorderBackgroundColor(FocusedBorderColor);
							}
							DrawWidgetDebugOutline(LastActiveBorder.Pin());
						}
					}
				}
			}
		}
		else
		{
			if (LastActiveBorder.IsValid())
				LastActiveBorder.Pin()->SetBorderBackgroundColor(FLinearColor());
		}

		FUMHelpers::NotifySuccess(FText::FromString(OldWidgetStr));

		if (NewWidget->GetTypeAsString() == "SGraphPanel")
		{
			FUMHelpers::NotifySuccess(FText::FromString(NewWidgetStr));
		}
		// FUMHelpers::NotifySuccess(FText::FromString(NewWidgetStr));  // Debug all
	}
}

void FUMGraphNavigationManager::DrawWidgetDebugOutline(const TSharedPtr<SWidget>& InWidget)
{
	FSlateApplication&	SlateApp = FSlateApplication::Get();
	TSharedPtr<SWindow> ActiveWindow = SlateApp.GetActiveTopLevelWindow();

	if (ActiveWindow.IsValid())
	{
		FPaintGeometry WindowSpaceGeometry;
		GetWidgetWindowSpaceGeometry(InWidget, ActiveWindow, WindowSpaceGeometry);

		// Remove previous overlay if it exists
		if (DebugWidgetInstance.IsValid())
		{
			ActiveWindow->RemoveOverlaySlot(DebugWidgetInstance.ToSharedRef());
		}
		DebugWidgetInstance = SNew(SUMDebugWidget)
								  .TargetWidget(InWidget.ToWeakPtr())
								  .CustomGeometry(WindowSpaceGeometry)
								  .Visibility(EVisibility::HitTestInvisible);
		ActiveWindow->AddOverlaySlot()
			[DebugWidgetInstance.ToSharedRef()];
	}
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
void FUMGraphNavigationManager::GetWidgetWindowSpaceGeometry(const TSharedPtr<SWidget>& InWidget, const TSharedPtr<SWindow>& WidgetWindow, FPaintGeometry& WindowSpaceGeometry)
{
	FGeometry WidgetGeometry = InWidget->GetCachedGeometry();
	WindowSpaceGeometry = WidgetGeometry.ToPaintGeometry();
	WindowSpaceGeometry.AppendTransform(TransformCast<FSlateLayoutTransform>(Inverse(WidgetWindow->GetPositionInScreen())));
	FUMHelpers::NotifySuccess(FText::FromString(WindowSpaceGeometry.GetLocalSize().ToString()));
}
