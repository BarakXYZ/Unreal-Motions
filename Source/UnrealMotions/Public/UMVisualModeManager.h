#pragma once
#include "Framework/Application/SlateApplication.h"

class FUMVisualModeManager
{
public:
	FUMVisualModeManager();
	~FUMVisualModeManager();

	bool IsVisualTextSelected(FSlateApplication& SlateApp);
};
