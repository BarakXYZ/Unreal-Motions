#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "Framework/Commands/InputBindingManager.h"
#include "UMHelpers.h"
#include "VimEditorSubsystem.generated.h"

UENUM(BlueprintType)
enum class EVimMode : uint8
{
	Normal UMETA(DisplayName = "Normal"),
	Insert UMETA(DisplayName = "Insert"),
	Visual UMETA(DisplayName = "Visual"),
};

/**
 *
 */
UCLASS()
class UNREALMOTIONS_API UVimEditorSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	void OnPreInputKeyDown(const FKeyEvent& KeyEvent);
	void DetectVimMode(const FKeyEvent& KeyEvent);
	void SetMode(EVimMode NewMode);
	UFUNCTION(BlueprintCallable, Category = "Vim Editor Subsystem")
	void ToggleVim(bool bEnabled);

	EUMHelpersLogMethod UMHelpersLogMethod = EUMHelpersLogMethod::PrintToScreen;
	FDelegateHandle		PreInputKeyDownDelegateHandle;
	EVimMode			VimMode = EVimMode::Insert;
	bool				bVisualLog = false;
	bool				bConsoleLog = false;
};
