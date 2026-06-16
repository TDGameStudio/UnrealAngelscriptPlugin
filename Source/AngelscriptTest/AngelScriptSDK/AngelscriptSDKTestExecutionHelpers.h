#pragma once

#include "AngelscriptNativeTestSupport.h"

#include "Misc/AutomationTest.h"

// =============================================================================
// AngelscriptSDKTestExecutionHelpers
// =============================================================================
// Unified execution helper utilities for AngelScript SDK tests.
// Eliminates the duplicate Execute*Entry functions found across 30+ test files.
//
// Instead of each file defining its own:
//   - ExecuteIntEntry
//   - ExecuteBoolEntry
//   - ExecuteDoubleEntry
//   - ExecuteVoidEntry
//
// All tests can now use the standardized ExecuteScriptFunction<T>() template.
//
// Usage:
//   int32 result = 0;
//   if (!ExecuteScriptFunction(*TestRunner, ScriptEngine, Module,
//       "int MyFunction()", result))
//   {
//       return;
//   }
// =============================================================================

namespace AngelscriptSDKTestSupport
{
	/**
	 * Execute a script function and capture its return value (template specializations below).
	 *
	 * @param Test - The test runner instance for reporting failures
	 * @param ScriptEngine - The AngelScript engine
	 * @param Module - The compiled script module
	 * @param Declaration - Function declaration (e.g., "int Entry()")
	 * @param OutValue - Output parameter to receive the return value
	 * @return true if execution succeeded, false otherwise
	 */
	template<typename TReturnType>
	inline bool ExecuteScriptFunction(
		FAutomationTestBase& Test,
		asIScriptEngine* ScriptEngine,
		asIScriptModule* Module,
		const char* Declaration,
		TReturnType& OutValue);

	// -------------------------------------------------------------------------
	// Template specializations for common return types
	// -------------------------------------------------------------------------

	/**
	 * Specialization for bool return type.
	 */
	template<>
	inline bool ExecuteScriptFunction<bool>(
		FAutomationTestBase& Test,
		asIScriptEngine* ScriptEngine,
		asIScriptModule* Module,
		const char* Declaration,
		bool& OutValue)
	{
		using namespace AngelscriptNativeTestSupport;

		asIScriptFunction* Function = GetNativeFunctionByDecl(Module, Declaration);
		if (!Test.TestNotNull(TEXT("Should resolve the bool-returning script function"), Function))
		{
			return false;
		}

		asIScriptContext* Context = ScriptEngine->CreateContext();
		if (!Test.TestNotNull(TEXT("Should create a script execution context"), Context))
		{
			return false;
		}

		const int ExecuteResult = PrepareAndExecute(Context, Function);
		OutValue = Context->GetReturnByte() != 0;
		Context->Release();

		return Test.TestEqual(
			TEXT("Script execution should finish successfully"),
			ExecuteResult,
			static_cast<int32>(asEXECUTION_FINISHED));
	}

	/**
	 * Specialization for int32 return type.
	 */
	template<>
	inline bool ExecuteScriptFunction<int32>(
		FAutomationTestBase& Test,
		asIScriptEngine* ScriptEngine,
		asIScriptModule* Module,
		const char* Declaration,
		int32& OutValue)
	{
		using namespace AngelscriptNativeTestSupport;

		asIScriptFunction* Function = GetNativeFunctionByDecl(Module, Declaration);
		if (!Test.TestNotNull(TEXT("Should resolve the int-returning script function"), Function))
		{
			return false;
		}

		asIScriptContext* Context = ScriptEngine->CreateContext();
		if (!Test.TestNotNull(TEXT("Should create a script execution context"), Context))
		{
			return false;
		}

		const int ExecuteResult = PrepareAndExecute(Context, Function);
		OutValue = static_cast<int32>(Context->GetReturnDWord());
		Context->Release();

		return Test.TestEqual(
			TEXT("Script execution should finish successfully"),
			ExecuteResult,
			static_cast<int32>(asEXECUTION_FINISHED));
	}

	/**
	 * Specialization for double return type.
	 */
	template<>
	inline bool ExecuteScriptFunction<double>(
		FAutomationTestBase& Test,
		asIScriptEngine* ScriptEngine,
		asIScriptModule* Module,
		const char* Declaration,
		double& OutValue)
	{
		using namespace AngelscriptNativeTestSupport;

		asIScriptFunction* Function = GetNativeFunctionByDecl(Module, Declaration);
		if (!Test.TestNotNull(TEXT("Should resolve the double-returning script function"), Function))
		{
			return false;
		}

		asIScriptContext* Context = ScriptEngine->CreateContext();
		if (!Test.TestNotNull(TEXT("Should create a script execution context"), Context))
		{
			return false;
		}

		const int ExecuteResult = PrepareAndExecute(Context, Function);
		OutValue = Context->GetReturnDouble();
		Context->Release();

		return Test.TestEqual(
			TEXT("Script execution should finish successfully"),
			ExecuteResult,
			static_cast<int32>(asEXECUTION_FINISHED));
	}

	/**
	 * Specialization for float return type.
	 */
	template<>
	inline bool ExecuteScriptFunction<float>(
		FAutomationTestBase& Test,
		asIScriptEngine* ScriptEngine,
		asIScriptModule* Module,
		const char* Declaration,
		float& OutValue)
	{
		using namespace AngelscriptNativeTestSupport;

		asIScriptFunction* Function = GetNativeFunctionByDecl(Module, Declaration);
		if (!Test.TestNotNull(TEXT("Should resolve the float-returning script function"), Function))
		{
			return false;
		}

		asIScriptContext* Context = ScriptEngine->CreateContext();
		if (!Test.TestNotNull(TEXT("Should create a script execution context"), Context))
		{
			return false;
		}

		const int ExecuteResult = PrepareAndExecute(Context, Function);
		OutValue = Context->GetReturnFloat();
		Context->Release();

		return Test.TestEqual(
			TEXT("Script execution should finish successfully"),
			ExecuteResult,
			static_cast<int32>(asEXECUTION_FINISHED));
	}

	/**
	 * Execute a script function with no return value (void).
	 *
	 * @param Test - The test runner instance for reporting failures
	 * @param ScriptEngine - The AngelScript engine
	 * @param Module - The compiled script module
	 * @param Declaration - Function declaration (e.g., "void Entry()")
	 * @return true if execution succeeded, false otherwise
	 */
	inline bool ExecuteScriptVoidFunction(
		FAutomationTestBase& Test,
		asIScriptEngine* ScriptEngine,
		asIScriptModule* Module,
		const char* Declaration)
	{
		using namespace AngelscriptNativeTestSupport;

		asIScriptFunction* Function = GetNativeFunctionByDecl(Module, Declaration);
		if (!Test.TestNotNull(TEXT("Should resolve the void script function"), Function))
		{
			return false;
		}

		asIScriptContext* Context = ScriptEngine->CreateContext();
		if (!Test.TestNotNull(TEXT("Should create a script execution context"), Context))
		{
			return false;
		}

		const int ExecuteResult = PrepareAndExecute(Context, Function);
		Context->Release();

		return Test.TestEqual(
			TEXT("Script execution should finish successfully"),
			ExecuteResult,
			static_cast<int32>(asEXECUTION_FINISHED));
	}
}
