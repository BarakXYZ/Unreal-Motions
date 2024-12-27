#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/InputBindingManager.h"
#include "WidgetDrawerConfig.h"

class FUMStatusBarManager : public TSharedFromThis<FUMStatusBarManager>
{
public:
	FUMStatusBarManager();
	~FUMStatusBarManager();
	static TSharedPtr<FUMStatusBarManager> Get();
	static void							   Initialize();
	FWidgetDrawerConfig					   CreateGlobalDrawerConfig();
	void								   RegisterDrawer(const FName& StatusBarName);
	void								   BindPostEngineInitDelegates();
	void								   OnAssetEditorOpened(UObject* OpenedAsset);
	void								   AddStatusBarToLevelEditor();

private:
	static TSharedPtr<FUMStatusBarManager> StatusBarManager;
	TOptional<FWidgetDrawerConfig>		   GlobalDrawerConfig;
};
