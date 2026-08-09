#include "AngelscriptEngine.h"
#include "AngelscriptType.h"
#include "AngelscriptBinds.h"

/**
 * Generic platform process control.
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                              | Purpose / parameter notes                                                                                          |
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 * | void FGenericPlatformMisc::RequestExit(bool Force);                                      | Requests process shutdown; Force selects immediate rather than graceful exit.                                      |
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 */

AS_FORCE_LINK const FAngelscriptBind Bind_FGenericPlatformMisc(
	TEXT("FGenericPlatformMisc"),
	EAngelscriptBindPhase::ManualBindings,
	[](FAngelscriptBinds& Binds)
	{
		FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "FGenericPlatformMisc");
		Binds.BindGlobalFunctionForTarget("void RequestExit(bool Force)", &FGenericPlatformMisc::RequestExit)
			.NativeFunction("FGenericPlatformMisc::RequestExit", true);
	});
