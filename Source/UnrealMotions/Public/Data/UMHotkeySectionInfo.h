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
		Map.Add(EUMHotkeySection::MajorTabNavigation,
			FUMHotkeyInfo(TEXT("MainFrame"), TEXT("MoveToMajorTab")));

		Map.Add(EUMHotkeySection::MinorTabNavigation,
			FUMHotkeyInfo(TEXT("MainFrame"), TEXT("MoveToMinorTab")));

		Map.Add(EUMHotkeySection::ViewportBookmarks,
			FUMHotkeyInfo(TEXT("LevelViewport"), TEXT("SetBookmark")));

		Map.Add(EUMHotkeySection::GraphEditorBookmarks,
			FUMHotkeyInfo(TEXT("GraphEditor"), TEXT("SetQuickJump")));
	}
};
