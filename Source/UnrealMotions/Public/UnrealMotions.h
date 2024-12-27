#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWindow.h"
#include "Widgets/Docking/SDockTab.h"
#include "Modules/ModuleManager.h"
#include "ToolMenus.h"
#include "LevelEditor.h"

#include "UMInputPreProcessor.h"
#include "SUMStatusBarWidget.h"

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

	void BindPostEngineInitDelegates();
	void RegisterInputPreProcessor(FSlateApplication& App);

	/** Called after ToolMenus become available, to register our custom status bar. */
	void RegisterMenus();
	void RegisterMenuExtensions();

	void				AddMenu(FMenuBarBuilder& MenuBarBuilder);
	void				AddMenu(FToolBarBuilder& ToolBarBuilder);
	void				FillMenu(FMenuBuilder& MenuBuilder);
	void				OnButtonClicked();
	FWidgetDrawerConfig CreateGlobalDrawerConfig();

private:
	static FUMOnUserMovedToNewWindow OnUserMovedToNewWindow;
	static FUMOnUserMovedToNewTab	 OnUserMovedToNewTab;
	TSharedPtr<FUMInputPreProcessor> InputProcessor = nullptr;
	TSharedPtr<SUMBufferVisualizer>	 BVis;
	TSharedPtr<SUMStatusBarWidget>	 StatWidget;
};
