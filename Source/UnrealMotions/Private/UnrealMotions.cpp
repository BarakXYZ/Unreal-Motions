#include "UnrealMotions.h"
#include "Framework/Application/SlateApplication.h"

#include "UMHelpers.h"
#include "UMNavigationConfig.h"

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

	// TODO: Check in the config if the user wants this feature.
	FUMStatusBarManager::Initialize();

	FCoreDelegates::OnPostEngineInit.AddRaw(
		this, &FUnrealMotionsModule::BindPostEngineInitDelegates);

	// NOTE: This will only change the in-game navigation config, so not really
	// helpful.
	// FCoreDelegates::OnPostEngineInit.AddLambda(
	// 	[]() {
	// 		TSharedRef<FUMNavigationConfig> UMNavConfig =
	// 			MakeShareable(new FUMNavigationConfig());
	// 		FSlateApplication::Get().SetNavigationConfig(UMNavConfig);
	// 		FSlateApplication::Get().TryDumpNavigationConfig(UMNavConfig);
	// 	});
}

void FUnrealMotionsModule::ShutdownModule()
{
	UE_LOG(LogUnrealMotionsModule, Display, TEXT("Unreal Motions: Shutdown."));
}

void FUnrealMotionsModule::BindPostEngineInitDelegates()
{
	// Register our custom InputPreProcessor to intercept user input.
	FSlateApplication& App = FSlateApplication::Get();
	App.RegisterInputPreProcessor(FUMInputPreProcessor::Get());
}

FUMOnUserMovedToNewWindow& FUnrealMotionsModule::GetOnUserMovedToNewWindow()
{
	return FUnrealMotionsModule::OnUserMovedToNewWindow;
}

FUMOnUserMovedToNewTab& FUnrealMotionsModule::GetOnUserMovedToNewTab()
{
	return FUnrealMotionsModule::OnUserMovedToNewTab;
}

#undef LOCTEXT_NAMESPACE
IMPLEMENT_MODULE(FUnrealMotionsModule, UnrealMotions)
