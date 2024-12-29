#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "Framework/Commands/InputBindingManager.h"
#include "UMHelpers.h"
#include "UMInputPreProcessor.h"
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

	void BindCommands();

	UFUNCTION(BlueprintCallable, Category = "Vim Editor Subsystem")
	void ToggleVim(bool bEnabled);

	bool MapVimToArrowNavigation(
		const FKeyEvent& InKeyEvent, FKeyEvent& OutKeyEvent);

	void ProcessVimNavigationInput(
		FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	void DeleteItem(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	void OnResetSequence();
	void OnCountPrefix(FString AddedCount);

	TWeakObjectPtr<UVimEditorSubsystem> VimSubWeak{ nullptr };
	TWeakPtr<FUMInputPreProcessor>		InputPP;
	EUMHelpersLogMethod					UMHelpersLogMethod = EUMHelpersLogMethod::PrintToScreen;
	static TMap<FKey, FKey>				VimToArrowKeys;
	FDelegateHandle						PreInputKeyDownDelegateHandle;
	bool								bVisualLog{ true };
	bool								bConsoleLog{ false };
	FString								CountBuffer;
};
