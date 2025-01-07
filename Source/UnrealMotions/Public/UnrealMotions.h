#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWindow.h"
#include "Widgets/Docking/SDockTab.h"
#include "Modules/ModuleManager.h"
#include "LevelEditor.h"

#include "UMInputPreProcessor.h"
#include "SUMStatusBarWidget.h"

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
};
