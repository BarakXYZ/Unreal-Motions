#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "UMGraphNavigationManager.h"
#include "UMTabNavigationManager.h"

class FUnrealMotionsModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

public:
	TUniquePtr<FUMGraphNavigationManager> GraphNavigationManager = nullptr;
	TUniquePtr<FUMTabNavigationManager>	  TabNavigationManager = nullptr;
};
