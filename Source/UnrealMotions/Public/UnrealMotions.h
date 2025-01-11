#pragma once

#include "Modules/ModuleManager.h"

#include "UMInputPreProcessor.h"
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
	TSharedPtr<FUMInputPreProcessor> InputProcessor = nullptr;
	TSharedPtr<SUMBufferVisualizer>	 BVis;
	TSharedPtr<SUMStatusBarWidget>	 StatWidget;
	static FUMLogger				 Logger;
};
