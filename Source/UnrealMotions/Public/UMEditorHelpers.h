#pragma once

#include "BlueprintEditor.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UMLogger.h"

class FUMEditorHelpers
{
public:
	static IAssetEditorInstance* GetLastActiveEditor();
	static UObject*				 GetLastActiveAsset();

	static TArray<FAssetData> GetAllBlueprintAssets();

	static FBlueprintEditor* GetActiveBlueprintEditor();
};
