#pragma once

#include "AngelscriptNativeTestSupport.h"

#include "Misc/AutomationTest.h"

#include <type_traits>

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
	struct FSdkFunctionInvoker
	{
		FSdkFunctionInvoker(
			FAutomationTestBase& InTest,
			asIScriptEngine* InScriptEngine,
			asIScriptModule* InModule,
			const char* InDeclaration)
			: Test(InTest)
			, ScriptEngine(InScriptEngine)
			, Function(InModule != nullptr ? AngelscriptNativeTestSupport::GetNativeFunctionByDecl(InModule, InDeclaration) : nullptr)
		{
			if (ScriptEngine == nullptr)
			{
				Test.AddError(TEXT("SDK raw invoker requires a valid script engine"));
				return;
			}

			if (Function == nullptr)
			{
				const FString Declaration = UTF8_TO_TCHAR(InDeclaration != nullptr ? InDeclaration : "");
				const FString AvailableFunctions = AngelscriptNativeTestSupport::CollectFunctionDeclarations(InModule);
				Test.AddError(FString::Printf(
					TEXT("SDK raw invoker could not resolve '%s'; module exposes {%s}"),
					*Declaration,
					*AvailableFunctions));
				return;
			}

			Context = ScriptEngine->CreateContext();
			if (!Test.TestNotNull(TEXT("SDK raw invoker should create an execution context"), Context))
			{
				return;
			}

			const int PrepareResult = Context->Prepare(Function);
			if (!Test.TestEqual(
					*FString::Printf(TEXT("SDK raw invoker should Prepare '%s'"), UTF8_TO_TCHAR(Function->GetDeclaration())),
					PrepareResult,
					static_cast<int32>(asSUCCESS)))
			{
				Context->Release();
				Context = nullptr;
				return;
			}

			bValid = true;
		}

		~FSdkFunctionInvoker()
		{
			if (Context != nullptr)
			{
				Context->Release();
				Context = nullptr;
			}
		}

		FSdkFunctionInvoker(const FSdkFunctionInvoker&) = delete;
		FSdkFunctionInvoker& operator=(const FSdkFunctionInvoker&) = delete;

		bool IsValid() const
		{
			return bValid;
		}

		FSdkFunctionInvoker& AddArg(bool Value) { return SetArg([&] { return Context->SetArgByte(NextArgIndex, Value ? 1 : 0); }); }
		FSdkFunctionInvoker& AddArg(uint8 Value) { return SetArg([&] { return Context->SetArgByte(NextArgIndex, Value); }); }
		FSdkFunctionInvoker& AddArg(int8 Value) { return SetArg([&] { return Context->SetArgByte(NextArgIndex, static_cast<uint8>(Value)); }); }
		FSdkFunctionInvoker& AddArg(uint16 Value) { return SetArg([&] { return Context->SetArgWord(NextArgIndex, Value); }); }
		FSdkFunctionInvoker& AddArg(int16 Value) { return SetArg([&] { return Context->SetArgWord(NextArgIndex, static_cast<uint16>(Value)); }); }
		FSdkFunctionInvoker& AddArg(uint32 Value) { return SetArg([&] { return Context->SetArgDWord(NextArgIndex, Value); }); }
		FSdkFunctionInvoker& AddArg(int32 Value) { return SetArg([&] { return Context->SetArgDWord(NextArgIndex, static_cast<uint32>(Value)); }); }
		FSdkFunctionInvoker& AddArg(uint64 Value) { return SetArg([&] { return Context->SetArgQWord(NextArgIndex, Value); }); }
		FSdkFunctionInvoker& AddArg(int64 Value) { return SetArg([&] { return Context->SetArgQWord(NextArgIndex, static_cast<uint64>(Value)); }); }
		FSdkFunctionInvoker& AddArg(float Value) { return SetArg([&] { return Context->SetArgFloat(NextArgIndex, Value); }); }
		FSdkFunctionInvoker& AddArg(double Value) { return SetArg([&] { return Context->SetArgDouble(NextArgIndex, Value); }); }
		FSdkFunctionInvoker& AddArgAddress(void* Ptr) { return SetArg([&] { return Context->SetArgAddress(NextArgIndex, Ptr); }); }
		FSdkFunctionInvoker& AddArgObject(void* Obj) { return SetArg([&] { return Context->SetArgObject(NextArgIndex, Obj); }); }

		template <typename T>
		FSdkFunctionInvoker& AddArgRef(T& InOutRef)
		{
			return AddArgAddress(const_cast<std::remove_const_t<T>*>(&InOutRef));
		}

		template <typename T>
		FSdkFunctionInvoker& AddArgStruct(T& Value)
		{
			return AddArgObject(static_cast<void*>(&Value));
		}

		bool Call()
		{
			if (!bValid)
			{
				return false;
			}

			if (!Test.TestEqual(
					*FString::Printf(TEXT("SDK raw invoker should receive the declared number of arguments for '%s'"),
						UTF8_TO_TCHAR(Function->GetDeclaration())),
					NextArgIndex,
					static_cast<asUINT>(Function->GetParamCount())))
			{
				return false;
			}

			const int ExecuteResult = Context->Execute();
			if (ExecuteResult != asEXECUTION_FINISHED)
			{
				const char* ExceptionText = Context->GetExceptionString();
				Test.AddError(FString::Printf(
					TEXT("SDK raw invoker failed to execute '%s' (code %d%s%s)"),
					UTF8_TO_TCHAR(Function->GetDeclaration()),
					ExecuteResult,
					ExceptionText != nullptr ? TEXT(": ") : TEXT(""),
					ExceptionText != nullptr ? UTF8_TO_TCHAR(ExceptionText) : TEXT("")));
				return false;
			}

			bHasRun = true;
			return true;
		}

		template <typename ReturnType>
		ReturnType CallAndReturn(const ReturnType& Fallback = ReturnType{})
		{
			if (!Call())
			{
				return Fallback;
			}

			return ReadReturn<ReturnType>(Fallback);
		}

	private:
		template <typename SetArgFn>
		FSdkFunctionInvoker& SetArg(SetArgFn&& Fn)
		{
			if (!bValid)
			{
				return *this;
			}

			if (NextArgIndex >= Function->GetParamCount())
			{
				Test.AddError(FString::Printf(
					TEXT("SDK raw invoker '%s' has %u parameters; AddArg cursor out of range at %u"),
					UTF8_TO_TCHAR(Function->GetDeclaration()),
					static_cast<uint32>(Function->GetParamCount()),
					NextArgIndex));
				bValid = false;
				return *this;
			}

			const int Code = Fn();
			if (Code != asSUCCESS)
			{
				Test.AddError(FString::Printf(
					TEXT("SDK raw invoker '%s' SetArg index %u failed with code %d"),
					UTF8_TO_TCHAR(Function->GetDeclaration()),
					NextArgIndex,
					Code));
				bValid = false;
				return *this;
			}

			++NextArgIndex;
			return *this;
		}

		template <typename R>
		R ReadReturn(const R& Fallback)
		{
			if constexpr (std::is_same_v<R, bool>)
			{
				return Context->GetReturnByte() != 0;
			}
			else if constexpr (std::is_same_v<R, uint8> || std::is_same_v<R, int8>)
			{
				return static_cast<R>(Context->GetReturnByte());
			}
			else if constexpr (std::is_same_v<R, uint16> || std::is_same_v<R, int16>)
			{
				return static_cast<R>(Context->GetReturnWord());
			}
			else if constexpr (std::is_same_v<R, uint32> || std::is_same_v<R, int32>)
			{
				return static_cast<R>(Context->GetReturnDWord());
			}
			else if constexpr (std::is_same_v<R, uint64> || std::is_same_v<R, int64>)
			{
				return static_cast<R>(Context->GetReturnQWord());
			}
			else if constexpr (std::is_same_v<R, float>)
			{
				return ScriptEngine->GetEngineProperty(asEP_FLOAT_IS_FLOAT64) != 0
					? static_cast<float>(Context->GetReturnDouble())
					: Context->GetReturnFloat();
			}
			else if constexpr (std::is_same_v<R, double>)
			{
				return Context->GetReturnDouble();
			}
			else if constexpr (std::is_pointer_v<R>)
			{
				return static_cast<R>(Context->GetReturnObject());
			}
			else
			{
				static_assert(sizeof(R) == 0, "Unsupported return type for FSdkFunctionInvoker::CallAndReturn");
				return Fallback;
			}
		}

	private:
		FAutomationTestBase& Test;
		asIScriptEngine* ScriptEngine = nullptr;
		asIScriptFunction* Function = nullptr;
		asIScriptContext* Context = nullptr;
		asUINT NextArgIndex = 0;
		bool bValid = false;
		bool bHasRun = false;
	};

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
