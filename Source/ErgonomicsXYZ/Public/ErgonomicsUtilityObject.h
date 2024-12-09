// Copyright 2024 BarakXYZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorUtilityObject.h"
#include "ErgonomicsUtilityObject.generated.h"

/**
 *
 */

UENUM(BlueprintType)
enum class ETargetSection : uint8
{
	TabNavigation UMETA(DisplayName = "Tab Navigation"),
	Bookmarks	  UMETA(DisplayName = "Bookmarks")
};

UCLASS()
class ERGONOMICSXYZ_API UErgonomicsUtilityObject : public UEditorUtilityObject
{
	GENERATED_BODY()

public:
	void ConstructChords(
		bool bCtrl = true, bool bAlt = false,
		bool bShift = true, bool bCmd = false);

	UFUNCTION(BlueprintCallable, Category = "ErgonomicsXYZ | Utilities")
	void ClearHotkeys(ETargetSection TargetSection = ETargetSection::Bookmarks,
		bool						 bClearSecondary = false);

	UFUNCTION(BlueprintCallable, Category = "ErgonomicsXYZ | Utilities")
	void SetHotkeysToNums(
		ETargetSection TargetSection = ETargetSection::TabNavigation,
		bool		   bClearConflictingKeys = true,
		bool bCtrl = true, bool bAlt = false,
		bool bShift = true, bool bCmd = false);

	UFUNCTION(BlueprintCallable, Category = "ErgonomicsXYZ | Utilities")
	void DebugBookmarks();

	void ConstructTargetParams(ETargetSection TargetSection = ETargetSection::TabNavigation);

public:
	TArray<FInputChord> Chords;
	FName				ContextName;
	FString				TargetPrefix;
};
