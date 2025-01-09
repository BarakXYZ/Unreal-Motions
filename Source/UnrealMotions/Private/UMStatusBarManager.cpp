#include "UMStatusBarManager.h"
#include "Editor.h"
#include "Interfaces/IMainFrameModule.h"
#include "LevelEditor.h"
#include "SUMStatusBarWidget.h"
#include "StatusBarSubsystem.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "UMLogger.h"

// DEFINE_LOG_CATEGORY_STATIC(LogUMStatusBarManager, NoLogging, All); // Prod
DEFINE_LOG_CATEGORY_STATIC(LogUMStatusBarManager, Log, All); // Dev

#define LOCTEXT_NAMESPACE "FUnrealMotionsModule"

TSharedPtr<FUMStatusBarManager>
	FUMStatusBarManager::StatusBarManager = nullptr;

FUMStatusBarManager::FUMStatusBarManager()
{
	// AddDrawerToLevelEditor();

	// Register a function to be called when menu system is initialized
	UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateLambda([this]() {
			RegisterToolbarExtension("LevelEditor.StatusBar");
		}));

	// Use the current object as the owner of the menus
	// This allows us to remove all our custom menus when the
	// module is unloaded (see ShutdownModule below)
	FToolMenuOwnerScoped OwnerScoped(this);
}

FUMStatusBarManager::~FUMStatusBarManager()
{
	if (FUMStatusBarManager::StatusBarManager.IsValid())
	{
		FUMStatusBarManager::StatusBarManager.Reset();
	}

	// Unregister the startup function
	UToolMenus::UnRegisterStartupCallback(this);

	// Unregister all our menu extensions
	UToolMenus::UnregisterOwner(this);
}

void FUMStatusBarManager::Initialize()
{
	if (!StatusBarManager)
	{
		StatusBarManager = MakeShared<FUMStatusBarManager>();

		// Now it's safe to bind with AsShared()
		FCoreDelegates::OnPostEngineInit.AddSP(
			StatusBarManager.ToSharedRef(),
			&FUMStatusBarManager::BindPostEngineInitDelegates);
	}
}

TSharedPtr<FUMStatusBarManager> FUMStatusBarManager::Get()
{
	// Always ensure initialization first
	Initialize();
	return StatusBarManager;
}

void FUMStatusBarManager::BindPostEngineInitDelegates()
{
	if (UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
	{
		AssetEditorSubsystem->OnAssetEditorOpened().AddSP(
			AsShared(), &FUMStatusBarManager::OnAssetEditorOpened);
	}
}
FString FUMStatusBarManager::CleanupStatusBarName(FString TargetName)
{
	// Find the last occurrence of '_'
	int32 UnderscoreIndex;
	if (TargetName.FindLastChar('_', UnderscoreIndex))
	{
		// Check if the underscore is at the end or followed by digits only
		FString TrailingPart = TargetName.Mid(UnderscoreIndex + 1);
		if (TrailingPart.IsEmpty() || TrailingPart.IsNumeric())
		{
			return TargetName.Left(UnderscoreIndex); // Remove "_<digits>" or "_"
		}
	}

	// Return the original string if no cleaning is needed
	return TargetName;
}

void FUMStatusBarManager::OnAssetEditorOpened(UObject* OpenedAsset)
{
	if (UAssetEditorSubsystem* AssetEditorSubsystem =
			GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
	{
		IAssetEditorInstance* EditorInstance =
			AssetEditorSubsystem->FindEditorForAsset(
				OpenedAsset, /*bFocusIfOpen*/ false);
		if (EditorInstance)
		{
			// Cast to FAssetEditorToolkit to get the toolkit host
			if (FAssetEditorToolkit* AssetEditorToolkit =
					static_cast<FAssetEditorToolkit*>(EditorInstance))
			{
				TSharedRef<IToolkitHost> ToolkitHost =
					AssetEditorToolkit->GetToolkitHost();
				const FName StatusBarName = ToolkitHost->GetStatusBarName();
				FUMLogger::NotifySuccess(FText::FromName(StatusBarName));

				RegisterToolbarExtension(StatusBarName.ToString());
				// RegisterDrawer(StatusBarName);
			}
		}
	}
}

void FUMStatusBarManager::RegisterToolbarExtension(
	const FString& SerializedStatusBarName)
{
	// LookupAvailableModules();  // Debug

	UToolMenus* ToolMenus = UToolMenus::Get();
	// ToolMenus->IsMenuRegistered(MenuNameToExtend);
	const FName UMSectionName = "UnrealMotions";
	const FName StatusToolBarToExtend =
		*(CleanupStatusBarName(SerializedStatusBarName) + ".ToolBar");

	// We wanna find the menu, then look if our custom section exists
	UToolMenu* ToolMenu = ToolMenus->FindMenu(StatusToolBarToExtend);
	if (!ToolMenu) // Check if menu exists and valid.
	{
		FUMLogger::NotifySuccess(
			FText::FromString(FString::Printf(
				TEXT("Menu: %s was not found."),
				*StatusToolBarToExtend.ToString())),
			VisualLog);
		return;
	}
	FToolMenuSection* Section = ToolMenu->FindSection(UMSectionName);
	if (Section) // Our section was already added, we can safely skip.
	{
		const FString Log = FString::Printf(
			TEXT("Menu & Section found, skipping... Menu: %s Section: %s"),
			*StatusToolBarToExtend.ToString(), *UMSectionName.ToString());
		FUMLogger::NotifySuccess(FText::FromString(Log), VisualLog);
		return;
	}
	FUMLogger::NotifySuccess(
		FText::FromString("Section was not found, registering <3"), VisualLog);

	FToolMenuSection& UMSection =
		ToolMenu->AddSection(
			"UnrealMotions",
			LOCTEXT("UnrealMotions", "UnrealMotions"),
			FToolMenuInsert("SourceControl", EToolMenuInsertType::Before));

	TSharedRef<SUMStatusBarWidget> UMStatusBarWidget =
		SNew(SUMStatusBarWidget)
			.OnClicked(FOnClicked::CreateLambda([]() {
				// Handle click event
				return FReply::Handled();
			}));

	UMSection.AddEntry(
		FToolMenuEntry::InitWidget(
			"UMWidget",
			UMStatusBarWidget,
			FText::GetEmpty()));

	ToolMenus->RefreshAllWidgets();
}

void FUMStatusBarManager::AddDrawerToLevelEditor()
{
	IMainFrameModule& MainFrameModule =
		FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
	if (MainFrameModule.IsWindowInitialized())
		RegisterDrawer("LevelEditor.StatusBar");
	else
		MainFrameModule.OnMainFrameCreationFinished()
			.AddLambda([this](TSharedPtr<SWindow> Window, bool Cond) {
				RegisterDrawer("LevelEditor.StatusBar");
			});
}

FWidgetDrawerConfig FUMStatusBarManager::CreateGlobalDrawerConfig()
{
	FName				DrawerId = TEXT("MySimpleDrawer");
	FWidgetDrawerConfig DrawerConfig(DrawerId);

	DrawerConfig.GetDrawerContentDelegate.BindLambda([]() {
		return SNew(STextBlock).Text(LOCTEXT("MyDrawer_Text", "Hello from my global drawer!"));
	});

	DrawerConfig.ButtonText = LOCTEXT("MyDrawer_ButtonText", "My Drawer");
	DrawerConfig.ToolTipText = LOCTEXT("MyDrawer_ToolTip", "Open the My Drawer!");
	DrawerConfig.Icon = FAppStyle::Get().GetBrush("Icons.Help");

	return DrawerConfig;
}

void FUMStatusBarManager::RegisterDrawer(const FName& StatusBarName)
{
	// (Optional) If you want to define logic for OnDrawerOpened, OnDrawerDismissed, etc.:
	// MyDrawerConfig.OnDrawerOpenedDelegate.BindLambda([](FName StatusBarWithDrawerName)
	// {
	//     // Called when the user opens this drawer
	// });
	// MyDrawerConfig.OnDrawerDismissedDelegate.BindLambda([](const TSharedPtr<SWidget>& NewlyFocusedWidget)
	// {
	//     // Called when the drawer is closed
	// });

	// 4. Get the status bar subsystem and register the new drawer.
	if (UStatusBarSubsystem* StatusBarSubsystem = GEditor->GetEditorSubsystem<UStatusBarSubsystem>())
	{
		StatusBarSubsystem->RegisterDrawer(
			StatusBarName, // e.g. LevelEditor.StatusBar
			CreateGlobalDrawerConfig());
	}
}

void FUMStatusBarManager::LookupAvailableModules()
{
	// Names found won't match status bar formatted names :/
	TArray<FName> ModuleNames;
	FModuleManager::Get().FindModules(TEXT("*Editor*"), ModuleNames);

	for (const FName& ModuleName : ModuleNames)
	{
		UE_LOG(LogUMStatusBarManager, Warning,
			TEXT("Editor App: %s"), *ModuleName.ToString());
	}
}

// ~ MENU BAR EXTENSION SNIPPET ~ //
// NOTE: Not currently used, but keeping it for future reference.
void FUMStatusBarManager::AddMenuBarExtension()
{
	FLevelEditorModule& LevelEditorModule =
		FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	const TSharedPtr<FExtender> MenuExtender = MakeShareable(new FExtender());

	MenuExtender->AddMenuBarExtension(
		"Help",
		EExtensionHook::After,
		nullptr,
		FMenuBarExtensionDelegate::CreateRaw(
			// FToolBarExtensionDelegate::CreateRaw(
			this, &FUMStatusBarManager::AddMenuBar));

	LevelEditorModule.GetMenuExtensibilityManager()->AddExtender(MenuExtender);
}

void FUMStatusBarManager::AddMenuBar(FMenuBarBuilder& MenuBarBuilder)
{
	MenuBarBuilder.AddPullDownMenu(
		FText::FromString("Unreal Motions"),
		FText::FromString("ToolTip <3"),
		FNewMenuDelegate::CreateRaw(this, &FUMStatusBarManager::FillMenuBar));
}

void FUMStatusBarManager::FillMenuBar(FMenuBuilder& MenuBuilder)
{
}
// ~ MENU BAR EXTENSION SNIPPET ~ //

#undef LOCTEXT_NAMESPACE
