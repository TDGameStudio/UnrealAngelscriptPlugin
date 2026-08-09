#include "AngelscriptBinds.h"

#include "CoreGlobals.h"
#include "UObject/Class.h"

/**
 * Unreal process-mode globals.
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                              | Purpose / parameter notes                                                                                          |
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 * | bool IsRunningCommandlet();                                                              | Reports whether the process is executing any commandlet.                                                           |
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 * | bool IsRunningCookCommandlet();                                                          | Reports whether the active commandlet is cooking content.                                                          |
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 * | bool IsRunningDLCCookCommandlet();                                                       | Reports whether the active cook is producing DLC content.                                                          |
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 * | UClass GetRunningCommandletClass();                                                      | Returns the running commandlet class, or null outside commandlet execution.                                        |
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 */

AS_FORCE_LINK const FAngelscriptBind Bind_CoreGlobals(
	TEXT("CoreGlobals"),
	EAngelscriptBindPhase::ManualBindings,
	[](FAngelscriptBinds& Binds)
	{
		Binds.BindGlobalFunctionForTarget("bool IsRunningCommandlet() no_discard", &IsRunningCommandlet)
			.NativeFunction("IsRunningCommandlet", true);
		Binds.BindGlobalFunctionForTarget("bool IsRunningCookCommandlet() no_discard", &IsRunningCookCommandlet)
			.NativeFunction("IsRunningCookCommandlet", true);
		Binds.BindGlobalFunctionForTarget("bool IsRunningDLCCookCommandlet() no_discard", &IsRunningDLCCookCommandlet)
			.NativeFunction("IsRunningDLCCookCommandlet", true);
		Binds.BindGlobalFunctionForTarget("UClass GetRunningCommandletClass() no_discard", &GetRunningCommandletClass)
			.NativeFunction("GetRunningCommandletClass", true);
	});
