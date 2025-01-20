#pragma once

#include "Subsystems/AssetEditorSubsystem.h"
#include "UMLogger.h"

class FUMEditorHelpers
{
public:
	static IAssetEditorInstance* GetLastActiveEditor();
};
