// Copyright 2024 BarakXYZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ErgonomicsBPFuncLib.generated.h"

/**
 *
 */
UCLASS()
class ERGONOMICSXYZ_API UErgonomicsBPFuncLib : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable)
	static void DebugCommands();
};
