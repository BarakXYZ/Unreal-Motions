#pragma once

#include "CoreMinimal.h"
#include "Components/SlateWrapperTypes.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"

class SUMBufferVisualizer : public SOverlay
{
public:
	SLATE_BEGIN_ARGS(SUMBufferVisualizer) {}
	SLATE_END_ARGS()

	// Construct the widget
	void Construct(const FArguments& InArgs);

	// Update the displayed buffer
	void UpdateBuffer(const FString& NewBuffer);

	void SetWidgetVisibility(EVisibility InVisibility = EVisibility::SelfHitTestInvisible);

private:
	// Text block to display the buffer
	TSharedPtr<STextBlock> BufferText;
};
