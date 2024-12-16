#pragma once

#include "CoreMinimal.h"
#include "HotkeyInfo.h"
#include "HotkeySection.h"
#include "HotkeySectionInfo.generated.h"

USTRUCT(BlueprintType)
struct FHotkeySectionInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<EHotkeySection, FHotkeyInfo> Map;

	FHotkeySectionInfo()
	{
		Map.Add(EHotkeySection::TabNavigation,
			FHotkeyInfo(TEXT("MainFrame"), TEXT("MoveToTab")));

		Map.Add(EHotkeySection::ViewportBookmarks,
			FHotkeyInfo(TEXT("LevelViewport"), TEXT("SetBookmark")));

		Map.Add(EHotkeySection::GraphEditorBookmarks,
			FHotkeyInfo(TEXT("GraphEditor"), TEXT("SetQuickJump")));
	}
};
