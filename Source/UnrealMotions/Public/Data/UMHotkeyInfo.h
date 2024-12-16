#pragma once

#include "CoreMinimal.h"
#include "UMHotkeyInfo.generated.h"

USTRUCT(BlueprintType)
struct FUMHotkeyInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Command Context")
	FName ContextName;

	UPROPERTY(BlueprintReadWrite, Category = "Command Context")
	FString CommandPrefix;

	FUMHotkeyInfo()
		: ContextName("")
		, CommandPrefix("")
	{
	}

	FUMHotkeyInfo(const FName& InContextName, const FString& InCommandPrefix)
		: ContextName(InContextName)
		, CommandPrefix(InCommandPrefix)
	{
	}
};
