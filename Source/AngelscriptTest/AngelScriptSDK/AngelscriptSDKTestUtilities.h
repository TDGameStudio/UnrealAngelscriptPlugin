#pragma once

// Forward to the new unified execution helpers
#include "AngelscriptSDKTestExecutionHelpers.h"

// =============================================================================
// AngelscriptSDKTestUtilities
// =============================================================================
// Compatibility layer for tests that were refactored to use the shared
// execution utilities. This file now forwards to AngelscriptSDKTestExecutionHelpers.h
// which provides the template-based ExecuteScriptFunction<T>() interface.
//
// Legacy function names are preserved for backward compatibility:
//   - ExecuteScriptBoolFunction   -> ExecuteScriptFunction<bool>
//   - ExecuteScriptIntFunction    -> ExecuteScriptFunction<int32>
//   - ExecuteScriptDoubleFunction -> ExecuteScriptFunction<double>
//   - ExecuteScriptVoidFunction   -> ExecuteScriptVoidFunction (unchanged)
//
// New tests should use ExecuteScriptFunction<T>() directly from
// AngelscriptSDKTestExecutionHelpers.h for better type safety.
// =============================================================================

namespace AngelscriptSDKTestUtilities
{
	using AngelscriptSDKTestSupport::ExecuteScriptFunction;
	using AngelscriptSDKTestSupport::ExecuteScriptVoidFunction;

	// Legacy compatibility aliases
	inline bool ExecuteScriptBoolFunction(
		FAutomationTestBase& Test,
		asIScriptEngine* ScriptEngine,
		asIScriptModule* Module,
		const char* Declaration,
		bool& OutValue)
	{
		return ExecuteScriptFunction(Test, ScriptEngine, Module, Declaration, OutValue);
	}

	inline bool ExecuteScriptIntFunction(
		FAutomationTestBase& Test,
		asIScriptEngine* ScriptEngine,
		asIScriptModule* Module,
		const char* Declaration,
		int32& OutValue)
	{
		return ExecuteScriptFunction(Test, ScriptEngine, Module, Declaration, OutValue);
	}

	inline bool ExecuteScriptDoubleFunction(
		FAutomationTestBase& Test,
		asIScriptEngine* ScriptEngine,
		asIScriptModule* Module,
		const char* Declaration,
		double& OutValue)
	{
		return ExecuteScriptFunction(Test, ScriptEngine, Module, Declaration, OutValue);
	}
}
