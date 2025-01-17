#pragma once

#include "Framework/Commands/InputBindingManager.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "WidgetDrawerConfig.h"
#include "UMLogger.h"
#include "EditorSubsystem.h"
#include "VimStatusBarEditorSubsystem.generated.h"

/**
 *
 */
UCLASS()
class UNREALMOTIONS_API UVimStatusBarEditorSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * Depending on if Vim is enabled in the config, will control if the
	 * subsystem should be created.
	 */
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	/**
	 * @brief Initializes the Vim editor subsystem with configuration settings.
	 *
	 * @details Performs the following setup operations:
	 * 1. Reads configuration file for startup preferences
	 * 2. Initializes Vim mode based on config
	 * 3. Sets up weak pointers and input processor
	 * 4. Binds command handlers
	 *
	 * @param Collection The subsystem collection being initialized
	 */
	virtual void Initialize(FSubsystemCollectionBase& Collction) override;

	virtual void Deinitialize() override;

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
	TOptional<FWidgetDrawerConfig> GlobalDrawerConfig;
	FUMLogger					   Logger;
};
