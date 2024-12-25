#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/NavigationConfig.h"

#include "UnrealMotions.h"
#include "UMHelpers.h"
#include "UMNavigationConfig.h"

DEFINE_LOG_CATEGORY_STATIC(LogUnrealMotionsModule, NoLogging, All); // Prod
// DEFINE_LOG_CATEGORY_STATIC(LogUnrealMotionsModule, Log, All); // Dev

// #define LOCTEXT_NAMESPACE "FUnrealMotionsModule"

FUMOnUserMovedToNewWindow FUnrealMotionsModule::OnUserMovedToNewWindow;
FUMOnUserMovedToNewTab	  FUnrealMotionsModule::OnUserMovedToNewTab;

void FUnrealMotionsModule::StartupModule()
{
	UE_LOG(LogUnrealMotionsModule, Display, TEXT("Unreal Motions: Startup."));
	FUMHelpers::SetPluginConfigFile();

	TSharedRef<FUMNavigationConfig> UMNavConfig = MakeShareable(new FUMNavigationConfig());
	FSlateApplication::Get().SetNavigationConfig(UMNavConfig);

	// FCoreDelegates::OnPostEngineInit.AddRaw(
	// 	this, &FUnrealMotionsModule::AddConfigNavigationHJKL);
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

void FUnrealMotionsModule::AddConfigNavigationHJKL()
{
	// FSlateApplication&			  App = FSlateApplication::Get();
	// TSharedRef<FNavigationConfig> NavConfig = App.GetNavigationConfig();

	// Check and add the keys one by one
	// if (!NavConfig->KeyEventRules.Contains(EKeys::H))
	// {
	// 	NavConfig->KeyEventRules.Add(EKeys::H, EUINavigation::Up);
	// }

	// if (!NavConfig->KeyEventRules.Contains(EKeys::J))
	// {
	// 	NavConfig->KeyEventRules.Add(EKeys::J, EUINavigation::Down);
	// }

	// if (!NavConfig->KeyEventRules.Contains(EKeys::K))
	// {
	// 	NavConfig->KeyEventRules.Add(EKeys::K, EUINavigation::Left);
	// }

	// if (!NavConfig->KeyEventRules.Contains(EKeys::L))
	// {
	// 	NavConfig->KeyEventRules.Add(EKeys::L, EUINavigation::Right);
	// }
	// NavConfig->KeyEventRules.Add(EKeys::H, EUINavigation::Up);
	// NavConfig->KeyEventRules.Add(EKeys::J, EUINavigation::Down);
	// NavConfig->KeyEventRules.Add(EKeys::K, EUINavigation::Left);
	// NavConfig->KeyEventRules.Add(EKeys::L, EUINavigation::Right);

	TSharedRef<FUMNavigationConfig> UMNavConfig = MakeShareable(new FUMNavigationConfig());
	FSlateApplication::Get().SetNavigationConfig(UMNavConfig);
}

IMPLEMENT_MODULE(FUnrealMotionsModule, UnrealMotions)
