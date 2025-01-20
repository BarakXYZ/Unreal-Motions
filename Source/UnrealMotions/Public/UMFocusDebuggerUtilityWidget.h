#pragma once

#include "EditorUtilityWidget.h"
#include "UMFocusDebuggerUtilityWidget.generated.h"

/**
 *
 */

UCLASS(BlueprintType)
class UNREALMOTIONS_API UUMFocusDebuggerUtilityWidget : public UEditorUtilityWidget
{
	GENERATED_BODY()

	UFUNCTION(BlueprintCallable, Category = "Unreal Motions: Focus Debugger")
	void DebugFocus(TMap<FString, FString>& OutMap);
};
