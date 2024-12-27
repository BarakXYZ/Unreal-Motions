#include "UnrealMotions.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "SUMStatusBarWidget.h"
#include "ToolMenus.h"
#include "LevelEditor.h"

#include "UMHelpers.h"
#include "UMNavigationConfig.h"
#include "UMCustomStatusBar.h"
#include "UMBufferVisualizer.h"
#include "Widgets/Input/SButton.h"
#include "StatusBarSubsystem.h"
#include "Interfaces/IMainFrameModule.h"

#include "Subsystems/AssetEditorSubsystem.h"
#include "Toolkits/IToolkitHost.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "UMStatusBarManager.h"

// DEFINE_LOG_CATEGORY_STATIC(LogUnrealMotionsModule, NoLogging, All); // Prod
DEFINE_LOG_CATEGORY_STATIC(LogUnrealMotionsModule, Log, All); // Dev

#define LOCTEXT_NAMESPACE "FUnrealMotionsModule"

FUMOnUserMovedToNewWindow FUnrealMotionsModule::OnUserMovedToNewWindow;
FUMOnUserMovedToNewTab	  FUnrealMotionsModule::OnUserMovedToNewTab;

void FUnrealMotionsModule::StartupModule()
{
	UE_LOG(LogUnrealMotionsModule, Display, TEXT("Unreal Motions: Startup."));
	FUMHelpers::SetPluginConfigFile();

	FUMStatusBarManager::Initialize(); // check if we can crash the editor without the plugin at all (with the anim blueprint thingy)

	TSharedRef<FUMNavigationConfig> UMNavConfig = MakeShareable(new FUMNavigationConfig());
	FSlateApplication::Get().SetNavigationConfig(UMNavConfig);

	FCoreDelegates::OnPostEngineInit.AddRaw(
		this, &FUnrealMotionsModule::BindPostEngineInitDelegates);

	// Register a function to be called when menu system is initialized
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(
		this, &FUnrealMotionsModule::RegisterMenuExtensions));

	FLevelEditorModule& LevelEditorModule =
		FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	const TSharedPtr<FExtender> MenuExtender = MakeShareable(new FExtender());

	MenuExtender->AddMenuBarExtension(
		"Help",
		EExtensionHook::After,
		nullptr,
		FMenuBarExtensionDelegate::CreateRaw(
			// FToolBarExtensionDelegate::CreateRaw(
			this, &FUnrealMotionsModule::AddMenu));

	LevelEditorModule.GetMenuExtensibilityManager()->AddExtender(MenuExtender);
}
void FUnrealMotionsModule::AddMenu(FMenuBarBuilder& MenuBarBuilder)
{
	MenuBarBuilder.AddPullDownMenu(
		FText::FromString("Unreal Motions"),
		FText::FromString("Unreal Motions Tools <3"),
		FNewMenuDelegate::CreateRaw(this, &FUnrealMotionsModule::FillMenu));
}

void FUnrealMotionsModule::AddMenu(FToolBarBuilder& ToolBarBuilder)
{
	ToolBarBuilder.AddWidget(
		SNew(SButton)
			.Text(FText::FromString("Unreal Motions"))
			.ToolTipText(FText::FromString("Unreal Motions Tools <3")),
		NAME_None,
		true,
		HAlign_Fill,
		FNewMenuDelegate::CreateRaw(this, &FUnrealMotionsModule::FillMenu));
}

void FUnrealMotionsModule::ShutdownModule()
{
	UE_LOG(LogUnrealMotionsModule, Display, TEXT("Unreal Motions: Shutdown."));

	// Unregister the startup function
	UToolMenus::UnRegisterStartupCallback(this);

	// Unregister all our menu extensions
	UToolMenus::UnregisterOwner(this);
}

void FUnrealMotionsModule::RegisterMenuExtensions()
{
	// Not the exact and all names
	// TArray<FName> ModuleNames;
	// FModuleManager::Get().FindModules(TEXT("*Editor*"), ModuleNames);

	// for (const FName& ModuleName : ModuleNames)
	// {
	// 	UE_LOG(LogTemp, Warning, TEXT("Editor App: %s"), *ModuleName.ToString());
	// }

	// const TArray<TWeakPtr<FTabSpawnerEntry>> Spawners = TabManager->CollectSpawners();
	// for (const auto& Spawner : Spawners)
	// {
	// 	UE_LOG(LogTemp, Warning, TEXT("Editor Tab: %s"), *Spawner.Pin()->GetDisplayName().ToString());
	// }

	// Use the current object as the owner of the menus
	// This allows us to remove all our custom menus when the
	// module is unloaded (see ShutdownModule below)
	FToolMenuOwnerScoped OwnerScoped(this);

	UToolMenus::Get();
	// 1#
	// Extend the "File" section of the main toolbar
	UToolMenu* ToolMenuToExtend = UToolMenus::Get()->ExtendMenu(
		// UToolMenu* ToolMenuToExtend = UToolMenus::Get()->RegisterMenu(
		"LevelEditor.StatusBar.ToolBar");
	// "AssetEditorToolbar.CommonActions");
	// "StatusBar");
	// "StatusBar.ToolBar");
	// "StatusBar.ToolBar.SourceControl");
	// "StatusBar.ToolBar.UnrealMotions");

	// FName DebugMenuName = "LevelEditor.StatusBar.ToolBar";
	// FName DebugMenuName = "LevelEditor";
	// if (!UToolMenus::Get()->IsMenuRegistered(DebugMenuName))
	// {
	// 	UE_LOG(LogUnrealMotionsModule, Error, TEXT("%s menu is not registered"), *DebugMenuName.ToString());
	// 	return;
	// }
	// UE_LOG(LogUnrealMotionsModule, Warning, TEXT("%s menu is registered <3"), *DebugMenuName.ToString());

	FToolMenuSection& MySection = ToolMenuToExtend->FindOrAddSection("MyCustomSection");

	// Create a widget or button
	FToolMenuEntry Entry = FToolMenuEntry::InitWidget(
		"MyCustomStatusBarButton",
		SNew(SButton)
			.Text(FText::FromString("My Global Button")),
		FText::GetEmpty(),
		true // bNoIndent
	);

	MySection.AddEntry(Entry);

	UToolMenus::Get()->RefreshAllWidgets();

	// FToolMenuSection& ToolbarSection1 =
	// 	ToolMenuToExtend->AddSection(
	// 		"UnrealMotions",
	// 		FText::GetEmpty());
	// // FText::GetEmpty(),
	// // FToolMenuInsert("SourceControl", EToolMenuInsertType::Before));

	// TSharedRef<SUMStatusBarWidget> StatWidget2 =
	// 	SNew(SUMStatusBarWidget)
	// 		.OnClicked(FOnClicked::CreateLambda([]() {
	// 			// Handle click event
	// 			return FReply::Handled();
	// 		}));

	// ToolbarSection1.AddEntry(
	// 	FToolMenuEntry::InitWidget(
	// 		"UMWidget",
	// 		StatWidget2,
	// 		FText::GetEmpty()));
}

void FUnrealMotionsModule::BindPostEngineInitDelegates()
{
	// Register our custom InputPreProcessor to intercept user input.
	FSlateApplication& App = FSlateApplication::Get();
	RegisterInputPreProcessor(App);
}

void FUnrealMotionsModule::RegisterInputPreProcessor(FSlateApplication& App)
{
	InputProcessor = MakeShared<FUMInputPreProcessor>();
	App.RegisterInputPreProcessor(InputProcessor);
}

FUMOnUserMovedToNewWindow& FUnrealMotionsModule::GetOnUserMovedToNewWindow()
{
	return FUnrealMotionsModule::OnUserMovedToNewWindow;
}

FUMOnUserMovedToNewTab& FUnrealMotionsModule::GetOnUserMovedToNewTab()
{
	return FUnrealMotionsModule::OnUserMovedToNewTab;
}

void FUnrealMotionsModule::FillMenu(FMenuBuilder& MenuBuilder)
{
}

void FUnrealMotionsModule::RegisterMenus()
{
	// This function is called once ToolMenus is ready.
	FUMCustomStatusBar::RegisterMenus();
}

#undef LOCTEXT_NAMESPACE
IMPLEMENT_MODULE(FUnrealMotionsModule, UnrealMotions)
