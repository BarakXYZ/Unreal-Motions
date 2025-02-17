#include "UMEditorHelpers.h"
#include "BlueprintEditor.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "AssetRegistry/AssetRegistryModule.h"

IAssetEditorInstance* FUMEditorHelpers::GetLastActiveEditor()
{
	UAssetEditorSubsystem* AssetEditorSubsystem =
		GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();

	const TArray<UObject*> EditedAssets{ AssetEditorSubsystem->GetAllEditedAssets() };
	IAssetEditorInstance*  LastActiveEditorInstance{ nullptr };
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
			LastActiveEditorInstance = EditorInstance;
			ActivationTime = CurrEditorActTime;
		}
	}
	return LastActiveEditorInstance;

	// if (!LastActiveEditorInstance)
	// 	return nullptr;

	// FAssetEditorToolkit* AssetEditorToolkit =
	// 	static_cast<FAssetEditorToolkit*>(LastActiveEditorInstance);
	// if (!AssetEditorToolkit)
	// 	return nullptr;

	// AssetEditorToolkit->GetObjectsCurrentlyBeingEdited();
	// AssetEditorToolkit->GetEditorName();

	// TSharedPtr<IToolkitHost> ToolkitHost = AssetEditorToolkit->GetToolkitHost();
}

UObject* FUMEditorHelpers::GetLastActiveAsset()
{
	UAssetEditorSubsystem* AssetEditorSubsystem =
		GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();

	const TArray<UObject*> EditedAssets{ AssetEditorSubsystem->GetAllEditedAssets() };
	UObject*			   LastActiveObject{ nullptr };
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
			LastActiveObject = EditedAsset;
			ActivationTime = CurrEditorActTime;
		}
	}
	return LastActiveObject;
}

TArray<FAssetData> FUMEditorHelpers::GetAllBlueprintAssets()
{
	FAssetRegistryModule& AssetRegistryModule =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

	// New way: Use FTopLevelAssetPath for the Blueprint class
	FTopLevelAssetPath BlueprintClassPath = UBlueprint::StaticClass()->GetClassPathName();

	TArray<FAssetData> BlueprintAssets;
	AssetRegistryModule.Get().GetAssetsByClass(BlueprintClassPath, BlueprintAssets, /*bSearchSubClasses=*/false);
	return BlueprintAssets;
}

FBlueprintEditor* FUMEditorHelpers::GetActiveBlueprintEditor()
{
	if (IAssetEditorInstance* LastActiveEditor = GetLastActiveEditor())
	{

		// 5) Check if this editor is specifically an FBlueprintEditor
		// if (FBlueprintEditor* BlueprintEditor =
		// 		Cast<FBlueprintEditor>(LastActiveEditor))
		return nullptr;
	}
	// If we reach here, either no Blueprint editors are open or none is currently active
	return nullptr;
}
