#include "AngelscriptBinds.h"

#include "Bind_FPlatformMisc_Functions.h"

namespace
{
	void BindFPlatformMisc(FAngelscriptBinds& Binds)
	{
		FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "FPlatformMisc");
		Binds.BindGlobalFunctionForTarget("void RequestExit(bool Force)", &FAngelscriptFPlatformMiscBinds::RequestExit);
		Binds.BindGlobalFunctionForTarget(
			"void RequestExit(bool Force, const FString& CallSite)",
			&FAngelscriptFPlatformMiscBinds::RequestExitWithCallSite);
		Binds.BindGlobalFunctionForTarget(
			"FString GetEnvironmentVariable(const FString& VariableName) no_discard",
			&FAngelscriptFPlatformMiscBinds::GetEnvironmentVariable);
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_FPlatformMisc(
	TEXT("FPlatformMisc"),
	EAngelscriptBindPhase::ManualBindings,
	&BindFPlatformMisc);
