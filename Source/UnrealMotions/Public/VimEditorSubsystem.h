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

	void ActivateNewInvokedTab(FSlateApplication& SlateApp, const TSharedPtr<SDockTab> NewTab);

	/**
	 * @brief Opens the Widget Reflector debugging tool and auto focus the debug
	 * button for convenience.
	 *
	 * @details This function performs the following operations:
	 * 1. Opens/activates the Widget Reflector tab
	 * 2. Traverses widget hierarchy to locate specific UI elements
	 * 3. Simulates user interactions to auto-focus the reflector
	 *
	 * @note The timer delay (150ms) may need adjustment on different platforms(?)
	 */
	void OpenWidgetReflector(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);
	void OpenOutputLog();
	void OpenContentBrowser(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent);

	void RemoveActiveMajorTab();

	void OnResetSequence();
	void OnCountPrefix(FString AddedCount);

	FReply HandleDummyKeyChar(
		const FGeometry& MyGeometry, const FCharacterEvent& InCharacterEvent)
	{
		return FReply::Handled();
	}

	FReply HandleDummyKeyDown(
		const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
	{
		return FReply::Handled();
	}

	DECLARE_DELEGATE_RetVal_TwoParams(FReply, FOnKeyDown, const FGeometry&, const FKeyEvent&);

	TWeakObjectPtr<UVimEditorSubsystem> VimSubWeak{ nullptr };
	TWeakPtr<FUMInputPreProcessor>		InputPP;
	EUMHelpersLogMethod					UMHelpersLogMethod = EUMHelpersLogMethod::PrintToScreen;
	static TMap<FKey, FKey>				VimToArrowKeys;
	FDelegateHandle						PreInputKeyDownDelegateHandle;
	bool								bVisualLog{ true };
	bool								bConsoleLog{ false };
	FString								CountBuffer;
};
