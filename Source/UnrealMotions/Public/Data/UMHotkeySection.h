#pragma once

#include "CoreMinimal.h"
#include "UMHotkeySection.generated.h"

UENUM(BlueprintType)
enum class EUMHotkeySection : uint8
{
	TabNavigation		 UMETA(DisplayName = "Tab Navigation"),
	ViewportBookmarks	 UMETA(DisplayName = "Viewport Bookmarks"),
	GraphEditorBookmarks UMETA(DisplayName = "Graph Editor Bookmarks")
};
