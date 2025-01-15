#pragma once

#include "Modules/ModuleManager.h"

#include "SUMStatusBarWidget.h"
#include "UMLogger.h"

class FUnrealMotionsModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	void BindPostEngineInitDelegates();

private:
	TSharedPtr<SUMBufferVisualizer> BVis;
	TSharedPtr<SUMStatusBarWidget>	StatWidget;
	FUMLogger						Logger;
};
