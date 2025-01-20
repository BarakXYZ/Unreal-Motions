#include "UMEditorHelpers.h"
#include "Editor.h"

IAssetEditorInstance* FUMEditorHelpers::GetLastActiveEditor()
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
	}
	return ActiveEditorInstance;

	// if (!ActiveEditorInstance)
	// 	return;

	// FAssetEditorToolkit* AssetEditorToolkit =
	// 	static_cast<FAssetEditorToolkit*>(ActiveEditorInstance);
	// if (!AssetEditorToolkit)
	// 	return;

	// AssetEditorToolkit->GetObjectsCurrentlyBeingEdited();
	// AssetEditorToolkit->GetEditorName();

	// TSharedPtr<IToolkitHost> ToolkitHost = AssetEditorToolkit->GetToolkitHost();
}
