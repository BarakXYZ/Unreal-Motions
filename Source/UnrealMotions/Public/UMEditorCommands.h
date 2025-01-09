#pragma once

#include "CoreMinimal.h"
#include "Framework/Application/SlateApplication.h"
#include "UMLogger.h"

class FUMEditorCommands
{
public:
	FUMEditorCommands();
	~FUMEditorCommands();
	const TSharedPtr<FUMEditorCommands> Get();

	static void ClearAllDebugMessages();

	static void ToggleAllowNotifications();

	static TSharedPtr<FUMEditorCommands> EditorCommands;

	FUMLogger Logger;
};
