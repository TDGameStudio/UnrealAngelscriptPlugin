#include "Bind_Debugging.h"

#include "AngelscriptBinds.h"

#include "GameDelegates.h"

/**
 * AngelScript debugging, assertion, and callstack globals.
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                          | Purpose / parameter notes                                                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | void DebugBreak();                                                                                   | Requests a debugger break when script debugging is enabled.                                                      |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool ensure(bool Condition);                                                                         | Reports a recoverable assertion failure and returns the condition.                                               |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool ensure(bool Condition, const FString& Message);                                                 | Reports a recoverable assertion failure with a message and returns the condition.                                |
 * |                                                                                                      | @param Condition Expression that must remain true.                                                               |
 * |                                                                                                      | @param Message Diagnostic text emitted on failure.                                                               |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool ensureAlways(bool Condition);                                                                   | Reports every recoverable assertion failure and returns the condition.                                           |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool ensureAlways(bool Condition, const FString& Message);                                           | Reports every recoverable assertion failure with a message and returns the condition.                            |
 * |                                                                                                      | @param Condition Expression that must remain true.                                                               |
 * |                                                                                                      | @param Message Diagnostic text emitted on every failure.                                                         |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | void check(bool Condition);                                                                          | Raises a fatal script assertion when the condition is false.                                                     |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | void check(bool Condition, const FString& Message);                                                  | Raises a fatal script assertion with a message when the condition is false.                                      |
 * |                                                                                                      | @param Condition Expression that must remain true.                                                               |
 * |                                                                                                      | @param Message Diagnostic text emitted before termination.                                                       |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | void throw(const FString& Message);                                                                  | Raises an AngelScript exception.                                                                                 |
 * |                                                                                                      | @param Message Exception text exposed to script error handling.                                                  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | TArray<FString> GetAngelscriptCallstack();                                                           | Returns the active AngelScript callstack as individual frames.                                                   |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FString FormatAngelscriptCallstack();                                                                | Returns the active AngelScript callstack as formatted text.                                                      |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 */

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
