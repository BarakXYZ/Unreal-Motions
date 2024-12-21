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

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FText Title;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 ID;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bIsActive;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bIsVisible;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bIsModal;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bIsTopmost;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EFocusCause UserFocusType;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bHasActiveChildren;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bIsEnabled;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bIsMirror;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bIsAccessible;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bIsMorphing;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bIsFocusedInitially;
};
