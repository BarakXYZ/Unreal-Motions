#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "Framework/Commands/InputBindingManager.h"
#include "UMHelpers.h"
#include "VimEditorSubsystem.generated.h"

/**
 *
 */
UCLASS()
class UNREALMOTIONS_API UVimEditorSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

	virtual void Initialize(FSubsystemCollectionBase& Collction) override;
	virtual void Deinitialize() override;

	void OnPreInputKeyDown(const FKeyEvent& KeyEvent);

	void DetectVimMode(const FKeyEvent& KeyEvent);

	UFUNCTION(BlueprintCallable, Category = "Vim Editor Subsystem")
	void ToggleVim(bool bEnabled);

	EUMHelpersLogMethod UMHelpersLogMethod = EUMHelpersLogMethod::PrintToScreen;
	FDelegateHandle		PreInputKeyDownDelegateHandle;
	bool				bVisualLog{ true };
	bool				bConsoleLog{ false };
};
