#pragma once

#include "CoreMinimal.h"
#include "HotkeySection.generated.h"

UENUM(BlueprintType)
enum class EHotkeySection : uint8
{
	TabNavigation		 UMETA(DisplayName = "Tab Navigation"),
	ViewportBookmarks	 UMETA(DisplayName = "Viewport Bookmarks"),
	GraphEditorBookmarks UMETA(DisplayName = "Graph Editor Bookmarks")
};
