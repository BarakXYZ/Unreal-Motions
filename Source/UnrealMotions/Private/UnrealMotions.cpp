#include "UnrealMotions.h"
#include "Framework/Application/SlateApplication.h"
#include "UMStatusBarManager.h"

// DEFINE_LOG_CATEGORY_STATIC(LogUnrealMotionsModule, NoLogging, All); // Prod
DEFINE_LOG_CATEGORY_STATIC(LogUnrealMotionsModule, Log, All); // Dev
FUMLogger FUnrealMotionsModule::Logger(&LogUnrealMotionsModule);

#define LOCTEXT_NAMESPACE "FUnrealMotionsModule"

void FUnrealMotionsModule::StartupModule()
{
	Logger.Print("Unreal Motions: Startup.");

	// TODO: Refactor
	FUMLogger::SetPluginConfigFile();

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
	// 	});
}

void FUnrealMotionsModule::ShutdownModule()
{
	Logger.Print("Unreal Motions: Shutdown.");
}

void FUnrealMotionsModule::BindPostEngineInitDelegates()
{
	// Register our custom InputPreProcessor to intercept user input.
	FSlateApplication& App = FSlateApplication::Get();
	App.RegisterInputPreProcessor(FUMInputPreProcessor::Get());
}

#undef LOCTEXT_NAMESPACE
IMPLEMENT_MODULE(FUnrealMotionsModule, UnrealMotions)
