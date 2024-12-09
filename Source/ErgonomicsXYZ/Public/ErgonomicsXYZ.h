// Copyright 2024 BarakXYZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/InputBindingManager.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SWidget.h"

class FErgonomicsXYZModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

public: // Methods
	void InitTabNavInputChords(
		bool bCtrl = true, bool bAlt = false,
		bool bShift = true, bool bCmd = false);

	void RemoveDefaultCommand(FInputBindingManager& IBManager, FInputChord Cmd);
	void AddCommandsToList(TSharedPtr<FBindingContext> MainFrameContext);
	void MapTabCommands(TSharedRef<FUICommandList>& CommandList);

	void RemoveCommand();
	void OnMoveToTab(int32 TabIndex);
	bool TraverseWidgetTree(const TSharedPtr<SWidget>& TraverseWidget, TSharedPtr<SWidget>& DockingTabWell, int32 Depth = 0);
	void FocusTab(const TSharedPtr<SWidget>& DockingTabWell, int TabIndex);

public: // Members
	// FSlateApplication*			  SlateApp = nullptr;

	/*
	 * Mentioned in editor preferences as "System-wide" category
	 * Another option is MainFrame
	 * The general workflow to find the specific naming though is just
	 * to live grep and look for something similar.
	 * If not using the very specific names, the engine won't be able to open at all.
	 */
	const FName MainFrameContextName = TEXT("MainFrame");

	TSharedPtr<class FUICommandInfo>   CommandInfoTab1 = nullptr;
	TArray<TSharedPtr<FUICommandInfo>> CommandInfoTabs = {
		nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr
	};

	TSharedPtr<class FUICommandInfo>  _openLevelSequenceCommandInfo = nullptr;
	TMap<FName, TSharedRef<SDockTab>> OpenTabs;
	TArray<FInputChord>				  TabChords;
};
