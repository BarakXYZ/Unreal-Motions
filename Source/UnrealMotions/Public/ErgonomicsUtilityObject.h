#pragma once

#include "CoreMinimal.h"
#include "EditorUtilityObject.h"
#include "Data/UMHotkeySectionInfo.h"
#include "ErgonomicsUtilityObject.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnUtilityObjectAction);

/**
 *
 */
UCLASS()
class UNREALMOTIONS_API UErgonomicsUtilityObject : public UEditorUtilityObject
{
	GENERATED_BODY()

public:
	void ConstructChords(
		bool bCtrl = true, bool bAlt = false,
		bool bShift = true, bool bCmd = false);

	UFUNCTION(BlueprintCallable, Category = "UnrealMotions | Utilities")
	void ClearHotkeys(EUMHotkeySection Section, bool bClearSecondary = false);

	UFUNCTION(BlueprintCallable, Category = "UnrealMotions | Utilities")
	void SetHotkeysToNums(
		EUMHotkeySection Section = EUMHotkeySection::MajorTabNavigation,
		bool			 bClearConflictingKeys = true,
		bool bCtrl = true, bool bAlt = false,
		bool bShift = true, bool bCmd = false);

	UFUNCTION(BlueprintCallable, Category = "UnrealMotions | Utilities")
	void DebugBookmarks();

	UFUNCTION(BlueprintCallable, Category = "UnrealMotions | Utilities")
	void GetHotkeyMappingState(
		EUMHotkeySection Section, TArray<FString>& OutMappingState);

public:
	TArray<FInputChord> Chords;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FUMHotkeySectionInfo UMHotkeySectionInfo;
	UPROPERTY(BlueprintAssignable)
	FOnUtilityObjectAction OnUtilityObjectAction;
};
