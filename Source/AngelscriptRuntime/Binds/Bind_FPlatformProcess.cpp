#include "AngelscriptBinds.h"

#include "Bind_FPlatformProcess_Functions.h"

namespace
{
	void BindFPlatformProcess(FAngelscriptBinds& Binds)
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
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_FPlatformProcess(
	TEXT("FPlatformProcess"),
	EAngelscriptBindPhase::ManualBindings,
	&BindFPlatformProcess);
