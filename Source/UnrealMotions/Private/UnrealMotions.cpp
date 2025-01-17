#include "UnrealMotions.h"

// DEFINE_LOG_CATEGORY_STATIC(LogUnrealMotionsModule, NoLogging, All); // Prod
DEFINE_LOG_CATEGORY_STATIC(LogUnrealMotionsModule, Log, All); // Dev

#define LOCTEXT_NAMESPACE "FUnrealMotionsModule"

void FUnrealMotionsModule::StartupModule()
{
	Logger = FUMLogger(&LogUnrealMotionsModule);
	Logger.Print("Unreal Motions: Startup.");
}

void FUnrealMotionsModule::ShutdownModule()
{
	Logger.Print("Unreal Motions: Shutdown.");
}

#undef LOCTEXT_NAMESPACE
IMPLEMENT_MODULE(FUnrealMotionsModule, UnrealMotions)
