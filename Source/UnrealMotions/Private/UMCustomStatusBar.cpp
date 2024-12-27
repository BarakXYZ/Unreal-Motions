#include "UMCustomStatusBar.h"
#include "ToolMenus.h"
#include "ToolMenu.h"
#include "ToolMenuSection.h"
#include "ToolMenuEntry.h"
#include "ToolMenuContext.h"

#include "Styling/AppStyle.h"
#include "Framework/MultiBox/MultiBoxDefs.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SSeparator.h"

#define LOCTEXT_NAMESPACE "FUMCustomStatusBar"

void FUMCustomStatusBar::RegisterMenus()
{
	// 1) Register a new parent tool menu for the status bar
	//    By convention, we name it "StatusBar.ToolBar.XXX"
	//    The Editor automatically places such menus in the bottom bar.
	static const FName MenuName("StatusBar.ToolBar.UnrealMotions");
	UToolMenu*		   StatusBarMenu = UToolMenus::Get()->RegisterMenu(
		MenuName,
		NAME_None,
		EMultiBoxType::ToolBar,
		/*bWarnIfRegistered=*/false);

	// Clear any existing sections (in case we hot reload)
	// StatusBarMenu->ResetSections();

	// 2) Add a section and place our SWidget as an entry
	FToolMenuSection& Section = StatusBarMenu->AddSection("UnrealMotionsSection");

	Section.AddEntry(
		FToolMenuEntry::InitWidget(
			TEXT("UnrealMotionsButtonEntry"),	   // Entry name
			MakeMyCustomStatusWidget(),			   // The actual widget
			LOCTEXT("UM_ButtonLabel", "My Custom") // Some label (not always shown in toolbars)
			));
}

TSharedRef<SWidget> FUMCustomStatusBar::MakeMyCustomStatusWidget()
{
	// This creates the SComboButton that sits in the status bar.
	// When clicked, it invokes OnGetMenuContent -> GenerateMyCustomMenuContent
	return SNew(SComboButton)
		.ContentPadding(FMargin(6.0f, 0.0f))
		.ToolTipText(LOCTEXT("UM_StatusBarTooltip", "Click to show the UnrealMotions menu"))
		.ComboButtonStyle(&FAppStyle::Get().GetWidgetStyle<FComboButtonStyle>("SimpleComboButton"))
		.MenuPlacement(MenuPlacement_AboveAnchor)
		.ButtonContent()
			[
				// The button’s visible part (icon/text/whatever).
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
						[SNew(STextBlock)
								.Text(LOCTEXT("UM_StatusBarButtonText", "My Custom Button"))]]
		.OnGetMenuContent(FOnGetContent::CreateStatic(&FUMCustomStatusBar::GenerateMyCustomMenuContent));
}

TSharedRef<SWidget> FUMCustomStatusBar::GenerateMyCustomMenuContent()
{
	// 1) Register or retrieve a child tool menu for the pop-up content
	//    "StatusBar.ToolBar.UnrealMotions.Menu" is an arbitrary sub-menu name
	static const FName MenuName("StatusBar.ToolBar.UnrealMotions.Menu");
	UToolMenu*		   MyMenu = UToolMenus::Get()->RegisterMenu(
		MenuName,
		NAME_None,
		EMultiBoxType::Menu,
		/*bWarnIfRegistered=*/false);

	// Clear any existing sections
	// MyMenu->ResetSections();

	// 2) Add a section for the pop-up
	FToolMenuSection& Section = MyMenu->AddSection("UMMenuSection", LOCTEXT("UM_MenuHeading", "Unreal Motions"));

	// 3) Add entries (buttons, commands, etc.)
	Section.AddMenuEntry(
		NAME_None,
		LOCTEXT("UM_SayHello", "Say Hello"),
		LOCTEXT("UM_SayHelloTooltip", "Example menu entry"),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([]() {
			UE_LOG(LogTemp, Display, TEXT("Hello from My Custom Status Bar Button!"));
		})));

	// Add another for demonstration
	Section.AddMenuEntry(
		NAME_None,
		LOCTEXT("UM_DoSomething", "Do Something"),
		LOCTEXT("UM_DoSomethingTooltip", "Another demonstration button"),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([]() {
			UE_LOG(LogTemp, Display, TEXT("Did something else!"));
		})));

	// 4) Convert that menu into an actual SWidget
	return UToolMenus::Get()->GenerateWidget(MenuName, FToolMenuContext());
}

#undef LOCTEXT_NAMESPACE
