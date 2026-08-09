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

#include "AngelscriptBinds.h"

/**
 * Platform process directories, executable identity, URL launching, and host/user identity.
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                          | Purpose / parameter notes                                                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FString Path = FPlatformProcess::UserDir();                                                          | Returns the platform user directory.                                                                             |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FString Path = FPlatformProcess::UserSettingsDir();                                                  | Returns the platform user-settings directory.                                                                    |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FString Path = FPlatformProcess::UserTempDir();                                                      | Returns the platform user temporary directory.                                                                   |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FString Path = FPlatformProcess::ApplicationSettingsDir();                                           | Returns the application-settings directory.                                                                      |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FString Path = FPlatformProcess::ExecutablePath();                                                   | Returns the running executable's full path.                                                                      |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FString Name = FPlatformProcess::ExecutableName();                                                   | Returns the running executable's filename.                                                                       |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FString Path = FPlatformProcess::CurrentWorkingDirectory();                                          | Returns the process working directory.                                                                           |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FPlatformProcess::LaunchURL(const FString& URL, const FString& Params = FString());                  | Requests the operating system to open a URL.                                                                     |
 * |                                                                                                      | @param URL URL or registered protocol target.                                                                    |
 * |                                                                                                      | @param Params Optional platform-specific parameter string.                                                       |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FPlatformProcess::LaunchURL(const FString& URL, const FString& Params, FString& OutError);           | Requests the operating system to open a URL and returns diagnostic text.                                         |
 * |                                                                                                      | @param URL URL or registered protocol target.                                                                    |
 * |                                                                                                      | @param Params Platform-specific parameter string.                                                                |
 * |                                                                                                      | @param OutError Receives a platform error message when launch fails.                                             |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool bSupported = FPlatformProcess::CanLaunchURL(const FString& URL);                                | Reports whether the platform can launch the URL or protocol.                                                     |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FString Name = FPlatformProcess::ComputerName();                                                     | Returns the host computer name.                                                                                  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FString Name = FPlatformProcess::UserName();                                                         | Returns the current operating-system user name.                                                                  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FString Id = FPlatformProcess::GameBundleId();                                                       | Returns the platform bundle/application identifier.                                                              |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 */

AS_FORCE_LINK const FAngelscriptBind Bind_FPlatformProcess(
	TEXT("FPlatformProcess"),
	EAngelscriptBindPhase::ExplicitBindings,
	[](FAngelscriptBinds& Binds)
	{
		FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "FPlatformProcess");
		Binds.BindGlobalFunctionForTarget("FString UserDir()", &FAngelscriptFPlatformProcessBinds::UserDir);
		Binds.BindGlobalFunctionForTarget("FString UserSettingsDir()", &FAngelscriptFPlatformProcessBinds::UserSettingsDir);
		Binds.BindGlobalFunctionForTarget("FString UserTempDir()", &FAngelscriptFPlatformProcessBinds::UserTempDir);
		Binds.BindGlobalFunctionForTarget("FString ApplicationSettingsDir()", &FAngelscriptFPlatformProcessBinds::ApplicationSettingsDir);
		Binds.BindGlobalFunctionForTarget("FString ExecutablePath()", &FAngelscriptFPlatformProcessBinds::ExecutablePath);
		Binds.BindGlobalFunctionForTarget("FString ExecutableName()", &FAngelscriptFPlatformProcessBinds::ExecutableName);
		Binds.BindGlobalFunctionForTarget("FString CurrentWorkingDirectory()", &FAngelscriptFPlatformProcessBinds::CurrentWorkingDirectory);
		Binds.BindGlobalFunctionForTarget(
			"void LaunchURL(const FString& URL, const FString& Params = FString())",
			&FAngelscriptFPlatformProcessBinds::LaunchUrl);
		Binds.BindGlobalFunctionForTarget(
			"void LaunchURL(const FString& URL, const FString& Params, FString& OutError)",
			&FAngelscriptFPlatformProcessBinds::LaunchUrlWithError);
		Binds.BindGlobalFunctionForTarget("bool CanLaunchURL(const FString& URL)", &FAngelscriptFPlatformProcessBinds::CanLaunchUrl);
		Binds.BindGlobalFunctionForTarget("FString ComputerName()", &FAngelscriptFPlatformProcessBinds::ComputerName);
		Binds.BindGlobalFunctionForTarget("FString UserName()", &FAngelscriptFPlatformProcessBinds::UserName);
		Binds.BindGlobalFunctionForTarget("FString GameBundleId()", &FAngelscriptFPlatformProcessBinds::GameBundleId);
	});

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
