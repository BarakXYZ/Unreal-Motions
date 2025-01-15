#include "UnrealMotions.h"
#include "Framework/Application/SlateApplication.h"
#include "UMStatusBarManager.h"
#include "UMConfig.h"
#include "UMInputPreProcessor.h"

// DEFINE_LOG_CATEGORY_STATIC(LogUnrealMotionsModule, NoLogging, All); // Prod
DEFINE_LOG_CATEGORY_STATIC(LogUnrealMotionsModule, Log, All); // Dev

#define LOCTEXT_NAMESPACE "FUnrealMotionsModule"

void FUnrealMotionsModule::StartupModule()
{
	Logger = FUMLogger(&LogUnrealMotionsModule);
	Logger.Print("Unreal Motions: Startup.");

	if (FUMConfig::Get()->IsVimEnabled())
	{
		FUMStatusBarManager::Initialize();

		FCoreDelegates::OnPostEngineInit.AddRaw(
			this, &FUnrealMotionsModule::BindPostEngineInitDelegates);
	}
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
