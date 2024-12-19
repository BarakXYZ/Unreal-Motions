#pragma once

#include "CoreMinimal.h"
#include "UMHotkeySection.generated.h"

UENUM(BlueprintType)
enum class EUMHotkeySection : uint8
{
	MajorTabNavigation	 UMETA(DisplayName = "Major Tab Navigation"),
	MinorTabNavigation	 UMETA(DisplayName = "Minor Tab Navigation"),
	ViewportBookmarks	 UMETA(DisplayName = "Viewport Bookmarks"),
	GraphEditorBookmarks UMETA(DisplayName = "Graph Editor Bookmarks")
};
