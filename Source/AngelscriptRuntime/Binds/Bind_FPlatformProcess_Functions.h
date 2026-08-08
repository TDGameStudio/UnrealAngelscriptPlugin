#pragma once

#include "CoreMinimal.h"

struct FAngelscriptFPlatformProcessBinds
{
	static FString UserDir();
	static FString UserSettingsDir();
	static FString UserTempDir();
	static FString ApplicationSettingsDir();
	static FString ExecutablePath();
	static FString ExecutableName();
	static FString CurrentWorkingDirectory();
	static void LaunchUrl(const FString& Url, const FString& Params);
	static void LaunchUrlWithError(const FString& Url, const FString& Params, FString& OutError);
	static bool CanLaunchUrl(const FString& Url);
	static FString ComputerName();
	static FString UserName();
	static FString GameBundleId();
};
