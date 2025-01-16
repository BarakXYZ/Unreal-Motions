#include "UMConfig.h"

DEFINE_LOG_CATEGORY_STATIC(LogUMConfig, Log, All); // Dev

FUMConfig::FUMConfig()
{
	Logger = FUMLogger(&LogUMConfig);

	FString UMPluginConfigDir =
		FPaths::ProjectPluginsDir() / TEXT("UnrealMotions") / TEXT("Config");
	FString UMConfigFilePath =
		UMPluginConfigDir / TEXT("DefaultUnrealMotions.ini");

	// Use platform-specific path separators
	FPaths::MakeStandardFilename(UMConfigFilePath);

	ConfigFile.Read(UMConfigFilePath);
	if (ConfigFile.IsEmpty())
		Logger.Print("Unreal Motions Config file is Empty!", ELogVerbosity::Error);
}

FUMConfig::~FUMConfig()
{
}

TSharedRef<FUMConfig> FUMConfig::Get()
{
	const static TSharedRef<FUMConfig> UMConfig = MakeShared<FUMConfig>();
	return UMConfig;
}

bool FUMConfig::IsValid()
{
	return !ConfigFile.IsEmpty();
}

bool FUMConfig::IsVimEnabled()
{
	const TCHAR* KeyName = TEXT("bStartVim");
	bool		 bIsEnabled;

	ConfigFile.GetBool(VimSection, KeyName, bIsEnabled);

	return bIsEnabled;

	// TODO: Remove to a more general place? Debug
	// ConfigFile.GetBool(*DebugSection, TEXT("bVisualLog"), bVisualLog);
}

bool FUMConfig::IsTabNavigatorEnabled()
{
	const TCHAR* KeyName = TEXT("bIsTabNavigatorEnabled");
	bool		 bIsEnabled;

	ConfigFile.GetBool(TabSection, KeyName, bIsEnabled);

	return bIsEnabled;
}
