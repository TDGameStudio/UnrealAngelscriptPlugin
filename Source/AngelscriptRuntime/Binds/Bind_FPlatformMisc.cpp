#include "Bind_FPlatformMisc.h"

#include "AngelscriptBinds.h"

/**
 * FPlatformMisc namespace binding surface.
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                | Purpose / parameter notes                                                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FPlatformMisc::RequestExit(bool Force);                                               | Requests process exit.                                                                                               |
 * |                                                                                            | @param Force Forces immediate shutdown rather than a graceful exit request when true.                                |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FPlatformMisc::RequestExit(bool Force, const FString& CallSite);                      | Requests process exit and records the supplied call-site description.                                                |
 * |                                                                                            | @param Force Forces immediate shutdown rather than a graceful exit request when true.                                |
 * |                                                                                            | @param CallSite Diagnostic description of the caller requesting exit.                                                |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FString FPlatformMisc::GetEnvironmentVariable(const FString& VariableName);                | Returns the named process environment variable, or an empty string when it is unset.                                 |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 */

AS_FORCE_LINK const FAngelscriptBind Bind_FPlatformMisc(
	TEXT("FPlatformMisc"),
	EAngelscriptBindPhase::ManualBindings,
	[](FAngelscriptBinds& Binds)
	{
		FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "FPlatformMisc");
		Binds.BindGlobalFunctionForTarget("void RequestExit(bool Force)", &FAngelscriptFPlatformMiscBinds::RequestExit);
		Binds.BindGlobalFunctionForTarget(
			"void RequestExit(bool Force, const FString& CallSite)",
			&FAngelscriptFPlatformMiscBinds::RequestExitWithCallSite);
		Binds.BindGlobalFunctionForTarget(
			"FString GetEnvironmentVariable(const FString& VariableName) no_discard",
			&FAngelscriptFPlatformMiscBinds::GetEnvironmentVariable);
	});
