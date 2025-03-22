#pragma once

#include "CoreMinimal.h"
#include "UMLogger.h"
#include "Widgets/SCompoundWidget.h"

class SWindow;
class SMultiLineEditableTextBox;

/**
 * SVimConsole
 *
 * A Slate widget that wraps Unreal's console input box inside a pop-up window.
 * You can call SVimConsole::Open() to display it.
 */
class SVimConsole : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SVimConsole) {}
	SLATE_END_ARGS()

	/** Constructs the widget. */
	void Construct(const FArguments& InArgs);

	/**
	 * Opens the console pop-up in a floating window.
	 * Call this from anywhere in your Editor module code.
	 */
	static void Open();

private:
	/**
	 * Holds the text box that the OutputLog module gives us (isn't too useful
	 * because it is a private class, but we can still traverse and get stuff
	 * from it)
	 */
	TSharedPtr<SMultiLineEditableTextBox> ConsoleTextBox;

	/** Called internally when the console is closed. */
	void HandleConsoleClosed();

	/** Called when a console command is executed. */
	void HandleConsoleCommandExecuted();

	void RegisterLastFocusedWidget();
	void TryFocusLastFocusedWidget();

	static TWeakPtr<SWidget> PreConsoleFocusedWidget;
	static FVector2D		 LastWidgetAbsCoords;
	static FUMLogger		 Logger;
};
