#pragma once

#include "Framework/Application/SlateApplication.h"
#include "UMLogger.h"
#include "SUMEditableText.h"

class FUMVisualModeManager
{
public:
	FUMVisualModeManager();
	~FUMVisualModeManager();

	bool IsVisualTextSelected(FSlateApplication& SlateApp);

	FUMLogger						   Logger;
	static FEditableTextStyle		   TextStyle;
	static const FSlateBrush*		   CaretBrush;
	static FSlateBrush				   TransparentBrush;
	static TSharedPtr<SUMEditableText> WrappedEditableText;
	static TSharedPtr<SEditableText>   MyEditableText;
	// FSlateBrush* CaretBrushPtr;
	// FSlateBrush	 CaretBrush;
	// FSlateBrush	 CustomCaretBrush;
	//
	static TSharedPtr<FUMVisualModeManager> VisualModeManager;
};
