#include "Bind_FApp.h"

#include "AngelscriptBinds.h"

#include "Misc/App.h"

/**
 * Application state helpers.
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                              | Purpose / parameter notes                                                                                          |
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 * | bool FApp::CanEverRender();                                                              | Reports whether this process can initialize or use rendering.                                                      |
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 * | FString FApp::GetProjectName();                                                          | Returns the current Unreal project name.                                                                           |
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 */

AS_FORCE_LINK const FAngelscriptBind Bind_FApp(
	TEXT("FApp"),
	EAngelscriptBindPhase::ExplicitBindings,
	[](FAngelscriptBinds& Binds)
	{
		FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "FApp");
		Binds.BindGlobalFunctionForTarget("bool CanEverRender()", &FApp::CanEverRender)
			.NativeFunction("FApp::CanEverRender", true);
		Binds.BindGlobalFunctionForTarget("FString GetProjectName()", &FAngelscriptFAppBinds::GetProjectName);
	});
