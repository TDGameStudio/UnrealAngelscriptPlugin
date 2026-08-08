#include "AngelscriptBinds.h"

#include "CoreGlobals.h"
#include "UObject/Class.h"

namespace
{
	void BindCoreGlobals(FAngelscriptBinds& Binds)
	{
		Binds.BindGlobalFunctionForTarget("bool IsRunningCommandlet() no_discard", &IsRunningCommandlet)
			.NativeFunction("IsRunningCommandlet", true);
		Binds.BindGlobalFunctionForTarget("bool IsRunningCookCommandlet() no_discard", &IsRunningCookCommandlet)
			.NativeFunction("IsRunningCookCommandlet", true);
		Binds.BindGlobalFunctionForTarget("bool IsRunningDLCCookCommandlet() no_discard", &IsRunningDLCCookCommandlet)
			.NativeFunction("IsRunningDLCCookCommandlet", true);
		Binds.BindGlobalFunctionForTarget("UClass GetRunningCommandletClass() no_discard", &GetRunningCommandletClass)
			.NativeFunction("GetRunningCommandletClass", true);
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_CoreGlobals(
	TEXT("CoreGlobals"),
	EAngelscriptBindPhase::ManualBindings,
	&BindCoreGlobals);
