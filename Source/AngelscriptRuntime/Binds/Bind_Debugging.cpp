#include "Bind_Debugging.h"

#include "AngelscriptBinds.h"
#include "Bind_Debugging_Functions.h"

#include "GameDelegates.h"

namespace
{
	void BindDebugging(FAngelscriptBinds& Binds)
	{
#if WITH_EDITOR
		FGameDelegates::Get().GetEndPlayMapDelegate().AddStatic(
			&FAngelscriptDebuggingBinds::HandleEndPlayMap);
#endif

		Binds.BindGlobalFunctionForTarget("void DebugBreak()", &FAngelscriptDebuggingBinds::DebugBreak);
		Binds.BindGlobalFunctionForTarget(
			"bool ensure(bool Condition)",
			&FAngelscriptDebuggingBinds::Ensure)
			.CompileOutAsEnsure();
		Binds.BindGlobalFunctionForTarget(
			"bool ensure(bool Condition, const FString& Message)",
			&FAngelscriptDebuggingBinds::EnsureWithMessage)
			.CompileOutAsEnsure();
		Binds.BindGlobalFunctionForTarget(
			"bool ensureAlways(bool Condition)",
			&FAngelscriptDebuggingBinds::EnsureAlways)
			.CompileOutAsEnsure();
		Binds.BindGlobalFunctionForTarget(
			"bool ensureAlways(bool Condition, const FString& Message)",
			&FAngelscriptDebuggingBinds::EnsureAlwaysWithMessage)
			.CompileOutAsEnsure();
		Binds.BindGlobalFunctionForTarget(
			"void check(bool Condition)",
			&FAngelscriptDebuggingBinds::Check)
			.CompileOutAsCheck();
		Binds.BindGlobalFunctionForTarget(
			"void check(bool Condition, const FString& Message)",
			&FAngelscriptDebuggingBinds::CheckWithMessage)
			.CompileOutAsCheck();
		Binds.BindGlobalFunctionForTarget("void throw(const FString& Message)", &FAngelscriptDebuggingBinds::Throw);
		Binds.BindGlobalFunctionForTarget(
			"TArray<FString> GetAngelscriptCallstack()",
			&FAngelscriptDebuggingBinds::GetCallstack);
		Binds.BindGlobalFunctionForTarget(
			"FString FormatAngelscriptCallstack()",
			&FAngelscriptDebuggingBinds::FormatCallstack);
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_Debugging(
	TEXT("Debugging.Manual"),
	EAngelscriptBindPhase::ManualBindings,
	&BindDebugging);
