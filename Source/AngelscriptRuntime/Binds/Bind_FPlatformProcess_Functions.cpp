#include "Bind_FPlatformProcess_Functions.h"

#include "HAL/PlatformProcess.h"

FString FAngelscriptFPlatformProcessBinds::UserDir()
{
	return FString(FPlatformProcess::UserDir());
}

FString FAngelscriptFPlatformProcessBinds::UserSettingsDir()
{
	return FString(FPlatformProcess::UserSettingsDir());
}

FString FAngelscriptFPlatformProcessBinds::UserTempDir()
{
	return FString(FPlatformProcess::UserTempDir());
}

FString FAngelscriptFPlatformProcessBinds::ApplicationSettingsDir()
{
	return FString(FPlatformProcess::ApplicationSettingsDir());
}

FString FAngelscriptFPlatformProcessBinds::ExecutablePath()
{
	return FString(FPlatformProcess::ExecutablePath());
}

FString FAngelscriptFPlatformProcessBinds::ExecutableName()
{
	return FString(FPlatformProcess::ExecutableName());
}

FString FAngelscriptFPlatformProcessBinds::CurrentWorkingDirectory()
{
	return FPlatformProcess::GetCurrentWorkingDirectory();
}

void FAngelscriptFPlatformProcessBinds::LaunchUrl(const FString& Url, const FString& Params)
{
	FPlatformProcess::LaunchURL(*Url, *Params, nullptr);
}

void FAngelscriptFPlatformProcessBinds::LaunchUrlWithError(const FString& Url, const FString& Params, FString& OutError)
{
	FPlatformProcess::LaunchURL(*Url, *Params, &OutError);
}

bool FAngelscriptFPlatformProcessBinds::CanLaunchUrl(const FString& Url)
{
	return FPlatformProcess::CanLaunchURL(*Url);
}

FString FAngelscriptFPlatformProcessBinds::ComputerName()
{
	return FString(FPlatformProcess::ComputerName());
}

FString FAngelscriptFPlatformProcessBinds::UserName()
{
	return FString(FPlatformProcess::UserName());
}

FString FAngelscriptFPlatformProcessBinds::GameBundleId()
{
	return FPlatformProcess::GetGameBundleId();
}
