#include "UnrealMotions.h"
#include "UMHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogUnrealMotionsModule, NoLogging, All); // Prod
// DEFINE_LOG_CATEGORY_STATIC(LogUnrealMotionsModule, Log, All); // Dev

// #define LOCTEXT_NAMESPACE "FUnrealMotionsModule"

FUMOnUserMovedToNewWindow FUnrealMotionsModule::OnUserMovedToNewWindow;
FUMOnUserMovedToNewTab	  FUnrealMotionsModule::OnUserMovedToNewTab;

void FUnrealMotionsModule::StartupModule()
{
	UE_LOG(LogUnrealMotionsModule, Display, TEXT("Unreal Motions: Startup."));
	FUMHelpers::SetPluginConfigFile();
}

void FUnrealMotionsModule::ShutdownModule()
{
	UE_LOG(LogUnrealMotionsModule, Display, TEXT("Unreal Motions: Shutdown."));
}

FUMOnUserMovedToNewWindow& FUnrealMotionsModule::GetOnUserMovedToNewWindow()
{
	return FUnrealMotionsModule::OnUserMovedToNewWindow;
}

FUMOnUserMovedToNewTab& FUnrealMotionsModule::GetOnUserMovedToNewTab()
{
	return FUnrealMotionsModule::OnUserMovedToNewTab;
}

IMPLEMENT_MODULE(FUnrealMotionsModule, UnrealMotions)
