#pragma once

#include "UMLogger.h"

class FUMConfig
{
public:
	FUMConfig();
	~FUMConfig();

	static TSharedRef<FUMConfig> Get();
	bool						 IsValid();
	bool						 IsVimEnabled();
	bool						 IsTabNavigatorEnabled();
	bool						 IsWindowNavigatorEnabled();
	bool						 IsFocuserEnabled();

	FConfigFile ConfigFile;

private:
	FUMLogger Logger;

	const TCHAR* VimSection = TEXT("/Script/Vim");
	const TCHAR* TabSection = TEXT("/Script/TabNavigator");
	const TCHAR* WindowSection = TEXT("/Script/WindowNavigator");
	const TCHAR* FocuserSection = TEXT("/Script/Focuser");
	const TCHAR* DebugSection = TEXT("/Script/Debug");
};
