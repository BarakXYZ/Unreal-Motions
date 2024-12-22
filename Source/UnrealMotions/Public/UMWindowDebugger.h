// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EditorUtilityWidget.h"
#include "Data/UMWindowInfo.h"
#include "Framework/Application/SlateApplication.h"
#include "UMWindowDebugger.generated.h"

/**
 *
 */

UENUM(BlueprintType)
enum class EUMActiveWindowQueryType : uint8
{
	ActiveTopLevel		  UMETA(DisplayName = "Active Top Level Window"),
	ActiveTopLevelRegular UMETA(DisplayName = "Active Top Level Regular Window"),
	ActiveModal			  UMETA(DisplayName = "Active Modal Window"),
	VisibleMenu			  UMETA(DisplayName = "Visible Menu Window")
};

UENUM(BlueprintType)
enum class EUMWindowQueryType : uint8
{
	AllVisibleOrdered	UMETA(DisplayName = "All Visible Windows Ordered"),
	TopLevel			UMETA(DisplayName = "Top Level Windows"),
	InteractiveTopLevel UMETA(DisplayName = "Interactive Top Level Windows")
};

USTRUCT(BlueprintType)
struct FUMWindowDebugMap
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TMap<FString, FString> PropertyMap;
};

UCLASS(BlueprintType)
class UNREALMOTIONS_API UUMWindowDebugger : public UEditorUtilityWidget
{
	GENERATED_BODY()

	UFUNCTION(BlueprintCallable, Category = "Unreal Motions: Windows Debugger")
	void DebugAllWindows(
		TArray<FUMWindowDebugMap>& OutDebugInfo,
		EUMWindowQueryType		   WinQueryType);

	UFUNCTION(BlueprintCallable, Category = "Unreal Motions: Windows Debugger")
	void DebugActiveWindow(
		TMap<FString, FString>& OutDebugInfo, EUMActiveWindowQueryType WinQueryType);

	void GetWindowActivationPolicyString(EWindowActivationPolicy Policy, FString& OutString);

	void GetFocusCauseString(EFocusCause FocusCause, FString& OutString);

	void GetWindowModeString(EWindowMode::Type WindowMode, FString& OutString);

	UFUNCTION(BlueprintCallable, Category = "Unreal Motions: Windows Debugger")
	void DebugTrackedWindows(TArray<FString>& WindowsNames);
};
