#pragma once

#include "CoreMinimal.h"
#include "HotkeyInfo.generated.h"

USTRUCT(BlueprintType)
struct FHotkeyInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Command Context")
	FName ContextName;

	UPROPERTY(BlueprintReadWrite, Category = "Command Context")
	FString CommandPrefix;

	FHotkeyInfo()
		: ContextName("")
		, CommandPrefix("")
	{
	}

	FHotkeyInfo(const FName& InContextName, const FString& InCommandPrefix)
		: ContextName(InContextName)
		, CommandPrefix(InCommandPrefix)
	{
	}
};
