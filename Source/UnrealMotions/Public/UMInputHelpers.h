#pragma once

#include "CoreMinimal.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWidget.h"
#include "InputCoreTypes.h"

class FUMInputHelpers
{
public:
	FToggleVisibilityUMInputHelpers();
	~FUMInputHelpers();

	static const TSharedPtr<FUMInputHelpers> Get();

	static void SimulateClickOnWidget(
		FSlateApplication& SlateApp, const TSharedRef<SWidget> Widget,
		const FKey& EffectingButton, bool bIsDoubleClick = false);

	static bool AreMouseButtonsPressed(const TSet<FKey>& InButtons);

	static const TSharedPtr<FUMInputHelpers> InputHelpers;
};
