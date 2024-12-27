#include "UMStatusBarManager.h"
#include "Editor.h"
#include "Interfaces/IMainFrameModule.h"
#include "StatusBarSubsystem.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Widgets/Text/STextBlock.h"
#include "UMHelpers.h"

// DEFINE_LOG_CATEGORY_STATIC(LogUMStatusBarManager, NoLogging, All); // Prod
DEFINE_LOG_CATEGORY_STATIC(LogUMStatusBarManager, Log, All); // Dev

#define LOCTEXT_NAMESPACE "FUnrealMotionsModule"

TSharedPtr<FUMStatusBarManager>
	FUMStatusBarManager::StatusBarManager = nullptr;

FUMStatusBarManager::FUMStatusBarManager()
{
	AddStatusBarToLevelEditor();
}

FUMStatusBarManager::~FUMStatusBarManager()
{
	if (FUMStatusBarManager::StatusBarManager.IsValid())
	{
		FUMStatusBarManager::StatusBarManager.Reset();
	}
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

void FUMStatusBarManager::AddStatusBarToLevelEditor()
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
				FUMHelpers::NotifySuccess(FText::FromName(StatusBarName));

				// ToolkitHost->OnActiveViewportChanged()
				// 	.AddLambda([](TSharedPtr<IAssetViewport> A, TSharedPtr<IAssetViewport> B) {
				// 		FUMHelpers::NotifySuccess();
				// 	});

				// if (!GlobalDrawerConfig.IsSet())
				// 	GlobalDrawerConfig = CreateGlobalDrawerConfig();

				if (UStatusBarSubsystem* StatusBarSubsystem =
						GEditor->GetEditorSubsystem<UStatusBarSubsystem>())
				{
					StatusBarSubsystem->RegisterDrawer(
						StatusBarName,
						CreateGlobalDrawerConfig());
					// MoveTemp(GlobalDrawerConfig.GetValue()));
				}
			}
		}
	}
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

#undef LOCTEXT_NAMESPACE
