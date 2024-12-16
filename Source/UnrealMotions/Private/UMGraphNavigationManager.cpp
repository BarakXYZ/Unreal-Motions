#include "GraphEditor.h"
#include "BlueprintEditor.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UMGraphNavigationManager.h"
#include "UMHelpers.h"

FUMGraphNavigationManager::FUMGraphNavigationManager()
{
}

FUMGraphNavigationManager::~FUMGraphNavigationManager()
{
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
