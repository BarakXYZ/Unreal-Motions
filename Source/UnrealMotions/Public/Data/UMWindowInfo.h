#pragma once

#include "CoreMinimal.h"
#include "Input/Events.h"
// #include "Misc/Optional.h"
#include "UMWindowInfo.generated.h"

USTRUCT(BlueprintType)
struct FUMWindowInfo
{
	GENERATED_BODY()

	// FUMWindowInfo();

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Unreal Motions|Windows")
	FText Title;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Unreal Motions|Windows")
	int32 ID;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Unreal Motions|Windows")
	bool bIsActive;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Unreal Motions|Windows")
	bool bIsVisible;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Unreal Motions|Windows")
	bool bIsModal;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Unreal Motions|Windows")
	bool bIsTopmost;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Unreal Motions|Windows")
	EFocusCause UserFocusType;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Unreal Motions|Windows")
	bool bHasActiveChildren;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Unreal Motions|Windows")
	bool bIsEnabled;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Unreal Motions|Windows")
	bool bIsMirror;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Unreal Motions|Windows")
	bool bIsAccessible;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Unreal Motions|Windows")
	bool bIsMorphing;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Unreal Motions|Windows")
	bool bIsFocusedInitially;
};
