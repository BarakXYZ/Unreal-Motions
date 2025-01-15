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

	FConfigFile ConfigFile;

private:
	FUMLogger	 Logger;
	const TCHAR* VimSection = TEXT("/Script/Vim");
	const TCHAR* DebugSection = TEXT("/Script/Debug");
};
