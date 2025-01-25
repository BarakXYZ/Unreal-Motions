#pragma once
#include "Math/MathFwd.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SCanvas.h"
#include "SUMHintMarker.h"

class SUMHintOverlay : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SUMHintOverlay) {}
	SLATE_END_ARGS()

	void Construct(
		const FArguments& InArgs,
		// TArray<TSharedRef<SUMHintMarker>> InHintMarkers)
		TArray<TSharedRef<SUMHintMarker>>&& InHintMarkers) // Accept move
	{
		SetVisibility(EVisibility::HitTestInvisible);

		ChildSlot
			[SAssignNew(CanvasPanel, SCanvas)];

		for (const TSharedRef<SUMHintMarker>& HM : InHintMarkers)
		{
			CanvasPanel->AddSlot()
				.Size(FVector2D(22.0, 18.0))
				.Position(HM->LocalPositionInWindow)
					[HM];
		}

		HintMarkers = MoveTemp(InHintMarkers);
	}

	const TArray<TSharedRef<SUMHintMarker>> GetHintMarkers() { return HintMarkers; }

private:
	TSharedPtr<SCanvas>				  CanvasPanel;
	TArray<TSharedRef<SUMHintMarker>> HintMarkers;
};
