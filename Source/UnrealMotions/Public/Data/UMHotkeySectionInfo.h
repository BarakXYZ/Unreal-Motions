#pragma once

#include "CoreMinimal.h"
#include "UMHotkeyInfo.h"
#include "UMHotkeySection.h"
#include "UMHotkeySectionInfo.generated.h"

USTRUCT(BlueprintType)
struct FUMHotkeySectionInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<EUMHotkeySection, FUMHotkeyInfo> Map;

	FUMHotkeySectionInfo()
	{
		Map.Add(EUMHotkeySection::TabNavigation,
			FUMHotkeyInfo(TEXT("MainFrame"), TEXT("MoveToTab")));

		Map.Add(EUMHotkeySection::ViewportBookmarks,
			FUMHotkeyInfo(TEXT("LevelViewport"), TEXT("SetBookmark")));

		Map.Add(EUMHotkeySection::GraphEditorBookmarks,
			FUMHotkeyInfo(TEXT("GraphEditor"), TEXT("SetQuickJump")));
	}
};
