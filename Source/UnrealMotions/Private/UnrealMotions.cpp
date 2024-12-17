#include "UnrealMotions.h"
#include "UMHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogUnrealMotionsModule, NoLogging, All); // Prod
// DEFINE_LOG_CATEGORY_STATIC(LogUnrealMotionsModule, Log, All); // Dev

void FUnrealMotionsModule::StartupModule()
{
	UE_LOG(LogUnrealMotionsModule, Display, TEXT("Unreal Motions: Startup."));
	// GraphNavigationManager = MakeUnique<FUMGraphNavigationManager>();
	TabNavigationManager = MakeUnique<FUMTabNavigationManager>();
	FUMHelpers::SetPluginConfigFile();
}

void FUnrealMotionsModule::ShutdownModule()
{
	UE_LOG(LogUnrealMotionsModule, Display, TEXT("Unreal Motions: Shutdown."));
	// GraphNavigationManager.Reset();
	// TabNavigationManager.Reset();
}

IMPLEMENT_MODULE(FUnrealMotionsModule, UnrealMotions)
