#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/InputBindingManager.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "WidgetDrawerConfig.h"

class FUMStatusBarManager : public TSharedFromThis<FUMStatusBarManager>
{
public:
	FUMStatusBarManager();
	~FUMStatusBarManager();
	static TSharedPtr<FUMStatusBarManager> Get();
	static void							   Initialize();

	void BindPostEngineInitDelegates();
	void OnAssetEditorOpened(UObject* OpenedAsset);

	FWidgetDrawerConfig CreateGlobalDrawerConfig();
	void				AddDrawerToLevelEditor();
	void				RegisterDrawer(const FName& StatusBarName);

	void	RegisterToolbarExtension(const FString& SerializedStatusBarName);
	FString CleanupStatusBarName(FString TargetName);

	void LookupAvailableModules();

	void AddMenuBarExtension();
	void AddMenuBar(FMenuBarBuilder& MenuBarBuilder);
	void FillMenuBar(FMenuBuilder& MenuBuilder);

private:
	static TSharedPtr<FUMStatusBarManager> StatusBarManager;
	TOptional<FWidgetDrawerConfig>		   GlobalDrawerConfig;
	bool								   VisualLog{ true };
};
