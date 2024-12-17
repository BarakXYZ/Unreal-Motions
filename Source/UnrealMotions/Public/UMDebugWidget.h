#include "CoreMinimal.h"					  // Core Unreal functionality
#include "Widgets/SCompoundWidget.h"		  // For SCompoundWidget
#include "Rendering/DrawElements.h"			  // For FSlateDrawElement
#include "Styling/CoreStyle.h"				  // For default brush styles
#include "Widgets/DeclarativeSyntaxSupport.h" // SLATE_BEGIN_ARGS, SLATE_ARGUMENT, etc.

class SUMDebugWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SUMDebugWidget) {}
	SLATE_ARGUMENT(TWeakPtr<SWidget>, TargetWidget)			  // Target widget for debug visualization
	SLATE_ARGUMENT(TOptional<FPaintGeometry>, CustomGeometry) // Custom geometry for debug
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		TargetWidget = InArgs._TargetWidget.Pin();
		CustomGeometry = InArgs._CustomGeometry; // Store custom geometry if provided
	}

	virtual int32 OnPaint(
		const FPaintArgs&		 Args,
		const FGeometry&		 AllottedGeometry,
		const FSlateRect&		 MyClippingRect,
		FSlateWindowElementList& OutDrawElements,
		int32					 LayerId,
		const FWidgetStyle&		 InWidgetStyle,
		bool					 bParentEnabled) const override
	{
		// Use custom geometry if provided
		const FPaintGeometry TargetPaintGeometry = CustomGeometry.IsSet()
			? CustomGeometry.GetValue()
			: TargetWidget.IsValid() ? TargetWidget.Pin()->GetCachedGeometry().ToPaintGeometry()
									 : AllottedGeometry.ToPaintGeometry();

		// Draw a border or debug box
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId,
			TargetPaintGeometry,
			FCoreStyle::Get().GetBrush(TEXT("Debug.Border")),
			ESlateDrawEffect::None,
			FLinearColor::White // Use Red for the border color
		);

		return LayerId + 1;
	}

private:
	TWeakPtr<SWidget>		  TargetWidget; // Widget to debug
	TOptional<FPaintGeometry> CustomGeometry;
};
