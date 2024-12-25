#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "UMHelpers.h"

class FUMInputPreProcessor : public IInputProcessor
{
public:
	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override
	{
		// If you need a per-frame Tick, put it here
	}

	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override
	{
		// // 1) Check if you want to handle it
		// if (ShouldHandleThisKey(InKeyEvent))
		// {
		// 	// 2) Do your custom logic
		// 	SimulateArrowKey(SlateApp, InKeyEvent);

		// 	// 3) Return `true` to mark the event as handled
		// 	return true;
		// }

		// // Otherwise return `false` so it passes to other processors / editor
		// return false;
		FUMHelpers::NotifySuccess();
		return true;
	}

	// virtual bool HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override
	// {
	// 	// If needed
	// 	return false;
	// }

	// virtual bool HandleAnalogInputEvent(FSlateApplication& SlateApp, const FAnalogInputEvent& InAnalogInputEvent) override
	// {
	// 	return false;
	// }

	// etc...
};
