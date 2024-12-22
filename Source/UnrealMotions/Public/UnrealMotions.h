#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWindow.h"
#include "Widgets/Docking/SDockTab.h"
#include "Modules/ModuleManager.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FUMOnUserMovedToNewWindow, TWeakPtr<SWindow>);
DECLARE_MULTICAST_DELEGATE_OneParam(FUMOnUserMovedToNewTab, TWeakPtr<SDockTab>);

class FUnrealMotionsModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static FUMOnUserMovedToNewWindow& GetOnUserMovedToNewWindow();
	static FUMOnUserMovedToNewTab&	  GetOnUserMovedToNewTab();

private:
	static FUMOnUserMovedToNewWindow OnUserMovedToNewWindow;
	static FUMOnUserMovedToNewTab	 OnUserMovedToNewTab;
};
