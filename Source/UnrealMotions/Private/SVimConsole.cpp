#include "SVimConsole.h"

// Slate / Window
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"

// OutputLog
#include "Modules/ModuleManager.h"
#include "OutputLogModule.h"

// Helpers
#include "UMFocusHelpers.h"
#include "UMSlateHelpers.h"
#include "UMInputHelpers.h"

#include "Editor.h"

// Internal Classes
#include "VimInputProcessor.h"

// ---------------------------------------------------------
//  SVimConsole
// ---------------------------------------------------------
DEFINE_LOG_CATEGORY_STATIC(LogVimConsole, Log, All);
TWeakPtr<SWidget> SVimConsole::PreConsoleFocusedWidget = nullptr;
FVector2D		  SVimConsole::LastWidgetAbsCoords = FVector2D();
FUMLogger		  SVimConsole::Logger(&LogVimConsole);

void SVimConsole::Open()
{
	// Ensure the OutputLog module is loaded before we try to use it.
	if (!FModuleManager::Get().IsModuleLoaded("OutputLog"))
		FModuleManager::Get().LoadModule("OutputLog");

	// Create our new floating window
	const TSharedPtr<SWindow> Window =
		SNew(SWindow)
			.Title(FText::FromString(TEXT("Vim Console")))
			.AutoCenter(EAutoCenter::PreferredWorkArea)
			.ClientSize(FVector2D(420, 30))
			.SizingRule(ESizingRule::FixedSize)
			.IsPopupWindow(true)
			.FocusWhenFirstShown(true);

	// Create an instance of our console wrapper widget
	const TSharedRef<SVimConsole> VimConsolePopup = SNew(SVimConsole);

	// Set our widget as the window content
	Window->SetContent(VimConsolePopup);

	// Add the window to the Slate application so it becomes visible
	FSlateApplication::Get().AddWindow(Window.ToSharedRef());

	FVimInputProcessor::Get()->SetVimMode(FSlateApplication::Get(), EVimMode::Insert);

	// Simply trying to focus on the VimConsolePopup doesn't seem to be enough.
	// So have to go with the traverse and delay approach
	TSharedPtr<SWidget> FoundEditable;
	if (!FUMSlateHelpers::TraverseFindWidget(
			VimConsolePopup, FoundEditable, "SMultiLineEditableText"))
		return;

	FTimerHandle TimerHandle;
	FUMFocusHelpers::SetWidgetFocusWithDelay(FoundEditable.ToSharedRef(), TimerHandle, 0.025f, false);
}

void SVimConsole::Construct(const FArguments& InArgs)
{
	RegisterLastFocusedWidget();

	// Build delegates that handle console close & command execution
	FSimpleDelegate OnCloseConsoleDelegate = FSimpleDelegate::CreateSP(this, &SVimConsole::HandleConsoleClosed);
	FSimpleDelegate OnCommandExecutedDelegate = FSimpleDelegate::CreateSP(this, &SVimConsole::HandleConsoleCommandExecuted);

	// Use the OutputLog module to create the console input box (SConsoleInputBox behind the scenes).
	FOutputLogModule&	OutputLogModule = FOutputLogModule::Get();
	TSharedRef<SWidget> ConsoleInputWidget =
		OutputLogModule.MakeConsoleInputBox(
			ConsoleTextBox,			  // -> TSharedPtr<SMultiLineEditableTextBox>& OutExposedEditableTextBox
			OnCloseConsoleDelegate,	  // -> Called when the user attempts to close the console
			OnCommandExecutedDelegate // -> Called when a console command is actually executed
		);

	// Our SCompoundWidget is just the console widget
	ChildSlot
		[ConsoleInputWidget];
}

void SVimConsole::HandleConsoleClosed()
{
	TryFocusLastFocusedWidget();
	FVimInputProcessor::Get()->SetVimMode(FSlateApplication::Get(), EVimMode::Normal);
	TSharedPtr<SWindow> Window = FSlateApplication::Get().FindWidgetWindow(AsShared());
	if (Window.IsValid())
		Window->RequestDestroyWindow();
}

void SVimConsole::HandleConsoleCommandExecuted()
{
	HandleConsoleClosed();
}

void SVimConsole::RegisterLastFocusedWidget()
{
	FSlateApplication&	SlateApp = FSlateApplication::Get();
	TSharedPtr<SWidget> FocusedWidget = SlateApp.GetUserFocusedWidget(0);
	if (!FocusedWidget.IsValid())
	{
		LastWidgetAbsCoords = FVector2D(0, 0);
		PreConsoleFocusedWidget = nullptr;
		return;
	}

	PreConsoleFocusedWidget = FocusedWidget;
	LastWidgetAbsCoords = FVector2D(FUMSlateHelpers::GetWidgetCenterScreenSpacePosition(FocusedWidget.ToSharedRef()));

	// Logger.Print(FString::Printf(TEXT("Widget Registered: %s"),
	// 				 FocusedWidget.IsValid()
	// 					 ? *FocusedWidget->GetTypeAsString()
	// 					 : TEXT("NONE")),
	// 	ELogVerbosity::Log, true);
}

void SVimConsole::TryFocusLastFocusedWidget()
{
	FSlateApplication& SlateApp = FSlateApplication::Get();
	if (auto WidgetToFocusWidget = PreConsoleFocusedWidget.Pin())
	{
		// Logger.Print("Widget is Valid", true);
		FTimerHandle TimerHandle;
		FUMFocusHelpers::SetWidgetFocusWithDelay(
			WidgetToFocusWidget.ToSharedRef(), TimerHandle, 0.025f, false);
	}
	else
	{
		// Logger.Print("Widget is Invalid, trying to click...", true);
		if (!LastWidgetAbsCoords.IsZero())
		{
			FUMInputHelpers::SimulateClickAtPosition(SlateApp, LastWidgetAbsCoords, EKeys::LeftMouseButton, 0.025f);
		}
	}
}
