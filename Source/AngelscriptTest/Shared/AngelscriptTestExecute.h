#pragma once

// ============================================================================
// AngelscriptTestExecute
// ============================================================================
//
// Themed sub-header originally split out of `AngelscriptTestUtilities.h`
// (Phase 1 of OpenSpec change `refactor-as-test-shared-layout-and-naming`)
// and consolidated in Phase 2 to become the single entry point for driving
// AngelScript functions from C++ tests.
//
// Three legacy namespaces coexist here unchanged — every old call site keeps
// compiling against the same fully-qualified symbol names. The Phase-2 merge
// is purely a physical relocation; Phase 3 will add the new `Execute*` naming
// family and `FAngelscriptTestExecutor` class on top, keeping all of these as
// permanent inline aliases.
//
//   namespace AngelscriptTestSupport
//     - ExecuteIntFunction
//     - ExecuteIntFunctionExpectingScriptException
//     - ExecuteInt64Function
//       (Inlined from AngelscriptTestUtilities.h lines 873-1007 in Phase 1.)
//
//   namespace AngelscriptReflectiveAccess  [merged in Phase 2 task 2.1]
//     - ResolveFunctionByDecl / ResolveFunctionByName
//     - FASGlobalFunctionInvoker (fluent typed-arg builder around asIScriptContext)
//       (Was AngelscriptGlobalFunctionInvoker.h, now a 3-line forward header.)
//
//   namespace AngelscriptTestBindings  [merged in Phase 2 task 2.2]
//     - ExpectGlobalInt / ExpectGlobalIntAtLeast / ExpectGlobalBool / ExpectGlobalDouble
//     - ExpectGlobalInts (batched)
//     - ExpectGlobalReturnBool / ExpectGlobalReturnFloat / ExpectGlobalReturnCustom<T>
//     - ExpectBindingCompileFailure (compile-side assertion)
//     - ExecuteFunctionExpectingScriptException
//     - Detail::TraceCase
//       (Was AngelscriptBindingsAssertions.h, now a 3-line forward header.
//        Guarded by WITH_DEV_AUTOMATION_TESTS to match the original.)
//
// Phase 3 (see OpenSpec tasks 3.x) will introduce:
//   - FAngelscriptTestExecutor + `Execute*` naming family
//     (`ExecuteAndExpect*`, `ExecuteAndExpectNear*`, `ExecuteBatchAndExpect*`,
//     `ExecuteAndValidate<T>`, `CompileAndExpectFailure`)
//   - All legacy names kept as permanent inline aliases / using-declarations.
// ============================================================================

#include "CoreMinimal.h"
#include "AngelscriptEngine.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Containers/StringConv.h"
#include "UObject/Object.h"

#include "AngelscriptBindingsCoverage.h"
#include "AngelscriptTestEngineHelper.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_context.h"
#include "source/as_scriptengine.h"
#include "source/as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

// ----------------------------------------------------------------------------
// Original AngelscriptTestUtilities.h lines 873-1007 (Phase 1).
// ----------------------------------------------------------------------------
namespace AngelscriptTestSupport
{
	inline bool ExecuteIntFunction(FAutomationTestBase& Test, FAngelscriptEngine& Engine, asIScriptFunction& Function, int32& OutValue)
	{
		FAngelscriptEngineScope EngineScope(Engine);
		asIScriptContext* Context = Engine.CreateContext();
		if (Context == nullptr)
		{
			Test.AddError(TEXT("Failed to create Angelscript execution context"));
			return false;
		}

		const int PrepareResult = Context->Prepare(&Function);
		const int ExecuteResult = PrepareResult == asSUCCESS ? Context->Execute() : PrepareResult;
		if (PrepareResult != asSUCCESS)
		{
			Test.AddError(FString::Printf(TEXT("Failed to prepare function (code %d)"), PrepareResult));
		}
		if (ExecuteResult != asEXECUTION_FINISHED)
		{
			Test.AddError(FString::Printf(TEXT("Failed to execute function (code %d)"), ExecuteResult));
		}

		if (PrepareResult == asSUCCESS && ExecuteResult == asEXECUTION_FINISHED)
		{
			OutValue = static_cast<int32>(Context->GetReturnDWord());
		}

		Context->Release();
		return PrepareResult == asSUCCESS && ExecuteResult == asEXECUTION_FINISHED;
	}

	inline bool ExecuteIntFunctionExpectingScriptException(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		asIScriptFunction& Function,
		const TCHAR* ContextLabel,
		const TCHAR* ExpectedExceptionContains,
		const TCHAR* ExpectedStackFrameContains = nullptr,
		const TCHAR* ExpectedOuterStackFrameContains = nullptr)
	{
		if (const ANSICHAR* ModuleNameAnsi = Function.GetModuleName())
		{
			if (ModuleNameAnsi[0] != '\0')
			{
				Test.AddExpectedErrorPlain(UTF8_TO_TCHAR(ModuleNameAnsi), EAutomationExpectedErrorFlags::Contains, 0);
			}
		}

		if (ExpectedStackFrameContains != nullptr && ExpectedStackFrameContains[0] != TEXT('\0'))
		{
			Test.AddExpectedErrorPlain(ExpectedStackFrameContains, EAutomationExpectedErrorFlags::Contains, 0);
		}
		else if (const ANSICHAR* DeclarationAnsi = Function.GetDeclaration(true, false, false, true))
		{
			if (DeclarationAnsi[0] != '\0')
			{
				Test.AddExpectedErrorPlain(
					FString::Printf(TEXT("%s | Line"), UTF8_TO_TCHAR(DeclarationAnsi)),
					EAutomationExpectedErrorFlags::Contains,
					0);
			}
		}

		if (ExpectedOuterStackFrameContains != nullptr && ExpectedOuterStackFrameContains[0] != TEXT('\0'))
		{
			Test.AddExpectedErrorPlain(ExpectedOuterStackFrameContains, EAutomationExpectedErrorFlags::Contains, 0);
		}

		Test.AddExpectedError(ExpectedExceptionContains, EAutomationExpectedErrorFlags::Contains, 0);

		FAngelscriptEngineScope EngineScope(Engine);
		asIScriptContext* Context = Engine.CreateContext();
		if (!Test.TestNotNull(*FString::Printf(TEXT("%s should create execution context"), ContextLabel), Context))
		{
			return false;
		}
		ON_SCOPE_EXIT { Context->Release(); };

		const int PrepareResult = Context->Prepare(&Function);
		if (!Test.TestEqual(*FString::Printf(TEXT("%s should prepare successfully"), ContextLabel), PrepareResult, static_cast<int32>(asSUCCESS)))
		{
			return false;
		}

		const int ExecuteResult = Context->Execute();
		const char* ExceptionStringAnsi = Context->GetExceptionString();
		const FString ExceptionString = UTF8_TO_TCHAR(ExceptionStringAnsi != nullptr ? ExceptionStringAnsi : "");
		const int32 ExceptionLine = Context->GetExceptionLineNumber();

		bool bPassed = true;
		bPassed &= Test.TestEqual(
			*FString::Printf(TEXT("%s should raise asEXECUTION_EXCEPTION"), ContextLabel),
			ExecuteResult,
			static_cast<int32>(asEXECUTION_EXCEPTION));
		bPassed &= Test.TestFalse(
			*FString::Printf(TEXT("%s should expose a non-empty exception string"), ContextLabel),
			ExceptionString.IsEmpty());
		bPassed &= Test.TestTrue(
			*FString::Printf(TEXT("%s exception '%s' should contain '%s'"), ContextLabel, *ExceptionString, ExpectedExceptionContains),
			ExceptionString.Contains(ExpectedExceptionContains));
		bPassed &= Test.TestTrue(
			*FString::Printf(TEXT("%s should report a positive exception line (got=%d)"), ContextLabel, ExceptionLine),
			ExceptionLine > 0);

		return bPassed;
	}

	inline bool ExecuteInt64Function(FAutomationTestBase& Test, FAngelscriptEngine& Engine, asIScriptFunction& Function, int64& OutValue)
	{
		FAngelscriptEngineScope EngineScope(Engine);
		asIScriptContext* Context = Engine.CreateContext();
		if (Context == nullptr)
		{
			Test.AddError(TEXT("Failed to create Angelscript execution context"));
			return false;
		}

		const int PrepareResult = Context->Prepare(&Function);
		const int ExecuteResult = PrepareResult == asSUCCESS ? Context->Execute() : PrepareResult;
		if (PrepareResult != asSUCCESS)
		{
			Test.AddError(FString::Printf(TEXT("Failed to prepare int64 function (code %d)"), PrepareResult));
		}
		if (ExecuteResult != asEXECUTION_FINISHED)
		{
			Test.AddError(FString::Printf(TEXT("Failed to execute int64 function (code %d)"), ExecuteResult));
		}

		if (PrepareResult == asSUCCESS && ExecuteResult == asEXECUTION_FINISHED)
		{
			OutValue = static_cast<int64>(Context->GetReturnQWord());
		}

		Context->Release();
		return PrepareResult == asSUCCESS && ExecuteResult == asEXECUTION_FINISHED;
	}
}

// ----------------------------------------------------------------------------
// Verbatim relocation from AngelscriptGlobalFunctionInvoker.h (Phase 2 task 2.1).
// Original docblock preserved below. Old header is now a 3-line forward stub.
// ----------------------------------------------------------------------------
/**
 * AngelscriptGlobalFunctionInvoker — small, typed helper for calling AngelScript
 * *global* functions (script-module level, no enclosing UCLASS) from C++ tests.
 *
 * The existing FFunctionInvoker in AngelscriptReflectiveAccess.h targets
 * UFUNCTIONs on spawned UObjects — it goes through UObject::FindFunction and
 * UASFunction::RuntimeCallEvent. Global functions don't participate in UClass
 * reflection, so they must be called directly via asIScriptContext. This file
 * provides the same Get / Set / Call ergonomics, but the parameter bus is the
 * raw AngelScript argument register instead of a UFunction-laid-out packed
 * parameter buffer.
 *
 * Usage:
 *
 *     asIScriptModule* Module = AngelscriptTestSupport::BuildModule(...);
 *     FASGlobalFunctionInvoker Invoker(*this, Engine, *Module, TEXT("int Sum(int, int)"));
 *     Invoker.AddArg(static_cast<int32>(17));
 *     Invoker.AddArg(static_cast<int32>(25));
 *     const int32 Result = Invoker.CallAndReturn<int32>(INDEX_NONE);
 *
 * Convenience overloads ResolveFunctionByDecl / ResolveFunctionByName help
 * locate the target asIScriptFunction on the module.
 */
namespace AngelscriptReflectiveAccess
{
	/**
	 * Resolve an asIScriptFunction by its AS declaration on the given module.
	 *
	 * Mirrors the logic of AngelscriptTestSupport::GetFunctionByDecl: try the
	 * full declaration, then fall back to the bare name, then scan the module
	 * by-index. We materialise a null-terminated FString before the UTF-8
	 * conversion — FStringView::GetData() is not guaranteed null-terminated.
	 */
	inline asIScriptFunction* ResolveFunctionByDecl(
		FAutomationTestBase& Test,
		asIScriptModule& Module,
		FStringView Declaration)
	{
		const FString DeclarationStr(Declaration);
		const FTCHARToUTF8 DeclarationUtf8(*DeclarationStr);
		asIScriptFunction* Function = Module.GetFunctionByDecl(DeclarationUtf8.Get());

		FString FunctionName;
		if (Function == nullptr)
		{
			int32 OpenParenIndex = INDEX_NONE;
			if (DeclarationStr.FindChar(TEXT('('), OpenParenIndex))
			{
				const FString Prefix = DeclarationStr.Left(OpenParenIndex).TrimStartAndEnd();
				int32 NameSeparatorIndex = INDEX_NONE;
				if (Prefix.FindLastChar(TEXT(' '), NameSeparatorIndex))
				{
					FunctionName = Prefix.Mid(NameSeparatorIndex + 1).TrimStartAndEnd();
					if (!FunctionName.IsEmpty())
					{
						const FTCHARToUTF8 FunctionNameUtf8(*FunctionName);
						Function = Module.GetFunctionByName(FunctionNameUtf8.Get());
					}
				}
			}
		}

		if (Function == nullptr && !FunctionName.IsEmpty())
		{
			const asUINT FunctionCount = Module.GetFunctionCount();
			for (asUINT FunctionIndex = 0; FunctionIndex < FunctionCount; ++FunctionIndex)
			{
				asIScriptFunction* CandidateFunction = Module.GetFunctionByIndex(FunctionIndex);
				if (CandidateFunction != nullptr && FunctionName.Equals(UTF8_TO_TCHAR(CandidateFunction->GetName())))
				{
					Function = CandidateFunction;
					break;
				}
			}
		}

		if (Function == nullptr)
		{
			// Dump the module's actual function signatures so the test diagnostic
			// explains the mismatch rather than just "expected not-null".
			FString AvailableFunctions;
			const asUINT FunctionCount = Module.GetFunctionCount();
			for (asUINT FunctionIndex = 0; FunctionIndex < FunctionCount; ++FunctionIndex)
			{
				asIScriptFunction* Candidate = Module.GetFunctionByIndex(FunctionIndex);
				if (Candidate == nullptr)
				{
					continue;
				}
				if (!AvailableFunctions.IsEmpty())
				{
					AvailableFunctions += TEXT(", ");
				}
				AvailableFunctions += UTF8_TO_TCHAR(Candidate->GetDeclaration());
			}

			Test.AddError(FString::Printf(
				TEXT("AS global function '%s' did not resolve by declaration; module exposes {%s}"),
				*DeclarationStr, *AvailableFunctions));
		}

		return Function;
	}

	/** Resolve an asIScriptFunction by its unqualified AS name on the given module. */
	inline asIScriptFunction* ResolveFunctionByName(
		FAutomationTestBase& Test,
		asIScriptModule& Module,
		FStringView Name)
	{
		const FString NameStr(Name);
		const FTCHARToUTF8 NameUtf8(*NameStr);
		asIScriptFunction* Function = Module.GetFunctionByName(NameUtf8.Get());
		Test.TestNotNull(
			*FString::Printf(TEXT("AS global function '%s' should resolve by name"), *NameStr),
			Function);
		return Function;
	}

	/**
	 * Typed builder around asIScriptContext that matches the argument slots of
	 * an AS global function. Each AddArg overload advances the cursor. Call() /
	 * CallAndReturn<R>() execute the context and tear it down.
	 *
	 * Argument mapping:
	 *   AddArg(bool)         -> SetArgByte
	 *   AddArg(uint8)        -> SetArgByte
	 *   AddArg(int16/uint16) -> SetArgWord
	 *   AddArg(int32/uint32) -> SetArgDWord
	 *   AddArg(int64/uint64) -> SetArgQWord
	 *   AddArg(float)        -> SetArgFloat
	 *   AddArg(double)       -> SetArgDouble
	 *   AddArgObject(T*)     -> SetArgObject (for UObject / script class handles)
	 *   AddArgRef<T>(ref)    -> SetArgAddress (for &in / &out / &inout refs)
	 *   AddArgStruct<T>(val) -> SetArgObject on a live temp copy
	 *
	 * Return mapping (CallAndReturn<R>):
	 *   R = bool / integer / enum -> GetReturnByte/Word/DWord/QWord
	 *   R = float / double        -> GetReturnFloat / GetReturnDouble
	 *   R = T*                    -> GetReturnObject
	 *
	 * For AS `float` parameters, the AS runtime applies asEP_FLOAT_IS_FLOAT64=1
	 * so the UFunction-side type is FDoubleProperty — but at the raw AS context
	 * level the parameter is still a `float`. So at this layer callers should
	 * use AddArg(1.0f), NOT AddArg(1.0). (The UFUNCTION path is the
	 * only place where you need `AddParam<double>`.)
	 */
	struct FASGlobalFunctionInvoker
	{
		FASGlobalFunctionInvoker(
			FAutomationTestBase& InTest,
			FAngelscriptEngine& InEngine,
			asIScriptFunction& InFunction)
			: Test(InTest)
			, Engine(InEngine)
			, Function(&InFunction)
		{
			EngineScope = MakeUnique<FAngelscriptEngineScope>(Engine);
			Context = Engine.CreateContext();
			if (!Test.TestNotNull(TEXT("AS global invoker should create an execution context"), Context))
			{
				return;
			}

			const int PrepareResult = Context->Prepare(Function);
			if (!Test.TestEqual(
					*FString::Printf(TEXT("AS global invoker should Prepare '%s' (code %d)"),
						UTF8_TO_TCHAR(Function->GetDeclaration()), PrepareResult),
					PrepareResult,
					static_cast<int32>(asSUCCESS)))
			{
				Context->Release();
				Context = nullptr;
				return;
			}

			bValid = true;
		}

		/** Overload that takes a script module + AS declaration for the common case. */
		FASGlobalFunctionInvoker(
			FAutomationTestBase& InTest,
			FAngelscriptEngine& InEngine,
			asIScriptModule& Module,
			FStringView Declaration)
			: Test(InTest)
			, Engine(InEngine)
			, Function(ResolveFunctionByDecl(InTest, Module, Declaration))
		{
			if (Function == nullptr)
			{
				return;
			}

			EngineScope = MakeUnique<FAngelscriptEngineScope>(Engine);
			Context = Engine.CreateContext();
			if (!Test.TestNotNull(TEXT("AS global invoker should create an execution context"), Context))
			{
				return;
			}

			const int PrepareResult = Context->Prepare(Function);
			if (!Test.TestEqual(
					*FString::Printf(TEXT("AS global invoker should Prepare '%s' (code %d)"),
						*FString(Declaration), PrepareResult),
					PrepareResult,
					static_cast<int32>(asSUCCESS)))
			{
				Context->Release();
				Context = nullptr;
				return;
			}

			bValid = true;
		}

		~FASGlobalFunctionInvoker()
		{
			if (Context != nullptr)
			{
				Context->Release();
				Context = nullptr;
			}
			EngineScope.Reset();
		}

		FASGlobalFunctionInvoker(const FASGlobalFunctionInvoker&) = delete;
		FASGlobalFunctionInvoker& operator=(const FASGlobalFunctionInvoker&) = delete;

		bool IsValid() const { return bValid; }
		asIScriptContext* GetContext() const { return Context; }

		// Typed argument setters — each advances the cursor by one AS slot.
		FASGlobalFunctionInvoker& AddArg(bool    Value)    { return SetArg([&]{ return Context->SetArgByte  (NextArgIndex, Value ? 1 : 0); }); }
		FASGlobalFunctionInvoker& AddArg(uint8   Value)    { return SetArg([&]{ return Context->SetArgByte  (NextArgIndex, Value); }); }
		FASGlobalFunctionInvoker& AddArg(int8    Value)    { return SetArg([&]{ return Context->SetArgByte  (NextArgIndex, static_cast<uint8>(Value)); }); }
		FASGlobalFunctionInvoker& AddArg(uint16  Value)    { return SetArg([&]{ return Context->SetArgWord  (NextArgIndex, Value); }); }
		FASGlobalFunctionInvoker& AddArg(int16   Value)    { return SetArg([&]{ return Context->SetArgWord  (NextArgIndex, static_cast<uint16>(Value)); }); }
		FASGlobalFunctionInvoker& AddArg(uint32  Value)    { return SetArg([&]{ return Context->SetArgDWord (NextArgIndex, Value); }); }
		FASGlobalFunctionInvoker& AddArg(int32   Value)    { return SetArg([&]{ return Context->SetArgDWord (NextArgIndex, static_cast<uint32>(Value)); }); }
		FASGlobalFunctionInvoker& AddArg(uint64  Value)    { return SetArg([&]{ return Context->SetArgQWord (NextArgIndex, Value); }); }
		FASGlobalFunctionInvoker& AddArg(int64   Value)    { return SetArg([&]{ return Context->SetArgQWord (NextArgIndex, static_cast<uint64>(Value)); }); }
		FASGlobalFunctionInvoker& AddArg(float   Value)    { return SetArg([&]{ return Context->SetArgFloat (NextArgIndex, Value); }); }
		FASGlobalFunctionInvoker& AddArg(double  Value)    { return SetArg([&]{ return Context->SetArgDouble(NextArgIndex, Value); }); }
		FASGlobalFunctionInvoker& AddArgObject(void* Obj)  { return SetArg([&]{ return Context->SetArgObject(NextArgIndex, Obj); }); }
		FASGlobalFunctionInvoker& AddArgAddress(void* Ptr) { return SetArg([&]{ return Context->SetArgAddress(NextArgIndex, Ptr); }); }

		/**
		 * Bind a reference-style argument (AS `&in` / `&out` / `&inout`) to the
		 * supplied live storage. The caller owns the lifetime and can read out
		 * any modifications after Call() returns.
		 */
		template <typename T>
		FASGlobalFunctionInvoker& AddArgRef(T& InOutRef)
		{
			return AddArgAddress(const_cast<std::remove_const_t<T>*>(&InOutRef));
		}

		/**
		 * Bind a value-style struct argument (AS USTRUCT passed by value) by
		 * copying through SetArgObject. The AS engine does NOT destroy the
		 * argument — our live C++ temporary is torn down by the normal scope.
		 */
		template <typename T>
		FASGlobalFunctionInvoker& AddArgStruct(T& LiveValue)
		{
			return AddArgObject(static_cast<void*>(&LiveValue));
		}

		/** Execute the function. Returns true if it ran to completion. */
		bool Call()
		{
			if (!bValid)
			{
				return false;
			}

			if (!Test.TestEqual(
					*FString::Printf(TEXT("AS global '%s' should receive the declared number of arguments"),
						UTF8_TO_TCHAR(Function->GetDeclaration())),
					NextArgIndex,
					static_cast<uint32>(Function->GetParamCount())))
			{
				return false;
			}

			const int ExecuteResult = Context->Execute();
			if (ExecuteResult != asEXECUTION_FINISHED)
			{
				const char* ExceptionText = Context->GetExceptionString();
				Test.AddError(FString::Printf(
					TEXT("AS global '%s' failed to execute (code %d%s%s)"),
					UTF8_TO_TCHAR(Function->GetDeclaration()),
					ExecuteResult,
					ExceptionText != nullptr ? TEXT(": ") : TEXT(""),
					ExceptionText != nullptr ? UTF8_TO_TCHAR(ExceptionText) : TEXT("")));
				return false;
			}

			bHasRun = true;
			return true;
		}

		/** Call the function and return the integer-family / pointer return value. */
		template <typename ReturnType>
		ReturnType CallAndReturn(const ReturnType& Fallback = ReturnType{})
		{
			if (!Call())
			{
				return Fallback;
			}
			return ReadReturn<ReturnType>(Fallback);
		}

		/** Return whether Call() has been invoked. */
		bool HasRun() const { return bHasRun; }

	private:
		template <typename SetArgFn>
		FASGlobalFunctionInvoker& SetArg(SetArgFn&& Fn)
		{
			if (!bValid)
			{
				return *this;
			}

			if (NextArgIndex >= Function->GetParamCount())
			{
				Test.AddError(FString::Printf(
					TEXT("AS global '%s' has %u parameters; AddArg cursor out of range at %u"),
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
					TEXT("AS global '%s' SetArg index %u failed with code %d"),
					UTF8_TO_TCHAR(Function->GetDeclaration()), NextArgIndex, Code));
				bValid = false;
				return *this;
			}
			++NextArgIndex;
			return *this;
		}

		// Return-value extractors. We specialize via overloads on a dispatch tag
		// struct to keep the template surface simple for callers (`CallAndReturn<int32>()`).
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
				return Context->GetReturnFloat();
			}
			else if constexpr (std::is_same_v<R, double>)
			{
				return Context->GetReturnDouble();
			}
			else if constexpr (std::is_pointer_v<R>)
			{
				// Object-pointer return (e.g. UObject*, script-class handle).
				return static_cast<R>(Context->GetReturnObject());
			}
			else
			{
				static_assert(sizeof(R) == 0, "Unsupported return type for FASGlobalFunctionInvoker::CallAndReturn — "
					"use ReadReturnStruct<T>() for USTRUCTs or call through a dedicated helper.");
				return Fallback;
			}
		}

	public:
		/**
		 * Read a USTRUCT return value out of the return register. Only valid
		 * after Call() has run. The caller owns the copy.
		 */
		template <typename StructType>
		bool ReadReturnStruct(StructType& OutValue)
		{
			if (!bHasRun)
			{
				Test.AddError(TEXT("ReadReturnStruct called before Call() completed"));
				return false;
			}

			const void* Address = Context->GetAddressOfReturnValue();
			if (!Test.TestNotNull(TEXT("AS global return should provide a value address"), Address))
			{
				return false;
			}
			OutValue = *static_cast<const StructType*>(Address);
			return true;
		}

	private:
		FAutomationTestBase& Test;
		FAngelscriptEngine& Engine;
		asIScriptFunction* Function = nullptr;
		asIScriptContext* Context = nullptr;
		TUniquePtr<FAngelscriptEngineScope> EngineScope;
		asUINT NextArgIndex = 0;
		bool bValid = false;
		bool bHasRun = false;
	};
}

// ----------------------------------------------------------------------------
// Verbatim relocation from AngelscriptBindingsAssertions.h (Phase 2 task 2.2).
// Original docblock preserved below. Old header is now a 3-line forward stub.
// ----------------------------------------------------------------------------
/**
 * AngelscriptBindingsAssertions — one-line per-case assertion helpers that
 * wrap `FASGlobalFunctionInvoker` for the Bindings Coverage refactor.
 *
 * Each helper:
 *   1. Resolves the named global function on the supplied module.
 *   2. Invokes it (with no args by default — see batch overloads for arg
 *      lists; for parameterised cases drive `FASGlobalFunctionInvoker` directly).
 *   3. Compares the return value against `Expected` (or matches an exception
 *      pattern, for the negative-path helper).
 *   4. Pushes a friendly `Test.AddInfo` line so a passing run still leaves a
 *      readable per-case trail in the automation log.
 *
 * Usage spans every SubPlan; the canonical templates live in
 * `AngelscriptBindingsExampleSection.h`.
 *
 * Convention: all expectations take a `CaseLabel` that describes the
 * *behavior under test*, not the function name. The function declaration is
 * already echoed by `FASGlobalFunctionInvoker`'s own diagnostics on failure.
 */

#if WITH_DEV_AUTOMATION_TESTS

namespace AngelscriptTestBindings
{
	/** Common bookkeeping: trace + invoker preflight. Returns invoker validity. */
	namespace Detail
	{
		inline bool TraceCase(
			FAutomationTestBase& Test,
			const FBindingsCoverageProfile& Profile,
			const TCHAR* CaseLabel)
		{
			Test.AddInfo(FormatCaseLabel(Profile, CaseLabel));
			return true;
		}
	}

	/**
	 * Compile-negative binding contract. Use this when a binding surface is
	 * intentionally not available on the current branch: the test still runs,
	 * proves the script fails to compile, and records the missing symbol or
	 * unsupported API in the compile diagnostics.
	 */
	inline bool ExpectBindingCompileFailure(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		const FBindingsCoverageProfile& Profile,
		const TCHAR* SectionName,
		const TCHAR* Source,
		const TCHAR* CaseLabel,
		TArrayView<const FString> ExpectedDiagnosticFragments)
	{
		Detail::TraceCase(Test, Profile, CaseLabel);

		AngelscriptTestSupport::FAngelscriptCompileTraceSummary Summary;
		const FString ModuleName = MakeCoverageModuleName(Profile, SectionName);
		const FString Filename = FString::Printf(TEXT("%s.as"), *ModuleName);
		AngelscriptTestSupport::CompileModuleWithSummary(
			&Engine,
			ECompileType::FullReload,
			FName(*ModuleName),
			Filename,
			FString(Source),
			true,
			Summary,
			true);

		bool bPassed = true;
		bPassed &= Test.TestFalse(
			*FString::Printf(TEXT("%s should fail to compile as an explicit binding boundary"), *FormatCaseLabel(Profile, CaseLabel)),
			Summary.bCompileSucceeded);
		bPassed &= Test.TestEqual(
			*FString::Printf(TEXT("%s compile result should be Error"), *FormatCaseLabel(Profile, CaseLabel)),
			Summary.CompileResult,
			ECompileResult::Error);

		for (const FString& ExpectedFragment : ExpectedDiagnosticFragments)
		{
			bool bFoundFragment = false;
			for (const AngelscriptTestSupport::FAngelscriptCompileTraceDiagnosticSummary& Diagnostic : Summary.Diagnostics)
			{
				if (Diagnostic.bIsError && Diagnostic.Message.Contains(ExpectedFragment))
				{
					bFoundFragment = true;
					break;
				}
			}

			bPassed &= Test.TestTrue(
				*FString::Printf(TEXT("%s diagnostics should contain '%s'"),
					*FormatCaseLabel(Profile, CaseLabel),
					*ExpectedFragment),
				bFoundFragment);
		}

		if (!bPassed || Summary.Diagnostics.Num() == 0)
		{
			Test.AddInfo(FString::Printf(TEXT("%s compile diagnostics: %d"),
				*FormatCaseLabel(Profile, CaseLabel),
				Summary.Diagnostics.Num()));
			for (const AngelscriptTestSupport::FAngelscriptCompileTraceDiagnosticSummary& Diagnostic : Summary.Diagnostics)
			{
				Test.AddInfo(FString::Printf(
					TEXT("  %s Row%d:Col%d %s"),
					Diagnostic.bIsError ? TEXT("ERROR") : (Diagnostic.bIsInfo ? TEXT("INFO") : TEXT("WARN")),
					Diagnostic.Row,
					Diagnostic.Column,
					*Diagnostic.Message));
			}
		}

		Engine.DiscardModule(*ModuleName);
		return bPassed;
	}

	/**
	 * Invoke a no-arg `int F()` global, compare its return against `Expected`.
	 * Returns aggregate pass/fail.
	 */
	inline bool ExpectGlobalInt(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		asIScriptModule& Module,
		const FBindingsCoverageProfile& Profile,
		const TCHAR* FunctionDecl,
		const TCHAR* CaseLabel,
		int32 Expected)
	{
		Detail::TraceCase(Test, Profile, CaseLabel);
		AngelscriptReflectiveAccess::FASGlobalFunctionInvoker Invoker(Test, Engine, Module, FunctionDecl);
		if (!Invoker.IsValid())
		{
			return false;
		}
		const int32 Actual = Invoker.CallAndReturn<int32>(INDEX_NONE);
		return Test.TestEqual(
			*FString::Printf(TEXT("%s (decl=%s)"), *FormatCaseLabel(Profile, CaseLabel), FunctionDecl),
			Actual,
			Expected);
	}

	/** Same as `ExpectGlobalInt` but asserts `Actual >= Minimum`. */
	inline bool ExpectGlobalIntAtLeast(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		asIScriptModule& Module,
		const FBindingsCoverageProfile& Profile,
		const TCHAR* FunctionDecl,
		const TCHAR* CaseLabel,
		int32 Minimum)
	{
		Detail::TraceCase(Test, Profile, CaseLabel);
		AngelscriptReflectiveAccess::FASGlobalFunctionInvoker Invoker(Test, Engine, Module, FunctionDecl);
		if (!Invoker.IsValid())
		{
			return false;
		}
		const int32 Actual = Invoker.CallAndReturn<int32>(INDEX_NONE);
		return Test.TestTrue(
			*FString::Printf(TEXT("%s (decl=%s) returned %d, expected >= %d"),
				*FormatCaseLabel(Profile, CaseLabel), FunctionDecl, Actual, Minimum),
			Actual >= Minimum);
	}

	/**
	 * Invoke a no-arg `int F()` (or `bool F()` exposed as int) and assert the
	 * result matches `Expected`. The script is expected to return 0/1 to
	 * represent false/true.
	 */
	inline bool ExpectGlobalBool(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		asIScriptModule& Module,
		const FBindingsCoverageProfile& Profile,
		const TCHAR* FunctionDecl,
		const TCHAR* CaseLabel,
		bool Expected)
	{
		return ExpectGlobalInt(Test, Engine, Module, Profile, FunctionDecl, CaseLabel, Expected ? 1 : 0);
	}

	/** Invoke a no-arg `double F()` and compare with the supplied tolerance. */
	inline bool ExpectGlobalDouble(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		asIScriptModule& Module,
		const FBindingsCoverageProfile& Profile,
		const TCHAR* FunctionDecl,
		const TCHAR* CaseLabel,
		double Expected,
		double Tolerance = 1e-6)
	{
		Detail::TraceCase(Test, Profile, CaseLabel);
		AngelscriptReflectiveAccess::FASGlobalFunctionInvoker Invoker(Test, Engine, Module, FunctionDecl);
		if (!Invoker.IsValid())
		{
			return false;
		}
		const double Actual = Invoker.CallAndReturn<double>(0.0);
		return Test.TestTrue(
			*FString::Printf(TEXT("%s (decl=%s) returned %.9g, expected %.9g (tol=%g)"),
				*FormatCaseLabel(Profile, CaseLabel), FunctionDecl, Actual, Expected, Tolerance),
			FMath::IsNearlyEqual(Actual, Expected, Tolerance));
	}

	/** Batched variant — one entry per case. */
	struct FExpectedGlobalInt
	{
		const TCHAR* FunctionDecl;
		const TCHAR* CaseLabel;
		int32 Expected;
	};

	inline bool ExpectGlobalInts(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		asIScriptModule& Module,
		const FBindingsCoverageProfile& Profile,
		TArrayView<const FExpectedGlobalInt> Cases)
	{
		bool bPassed = true;
		for (const FExpectedGlobalInt& Case : Cases)
		{
			bPassed &= ExpectGlobalInt(Test, Engine, Module, Profile,
				Case.FunctionDecl, Case.CaseLabel, Case.Expected);
		}
		return bPassed;
	}

	/**
	 * Negative path: invoke a no-arg `void F()` (or any function whose return
	 * is irrelevant) and assert that AS execution raises an exception whose
	 * message *contains* `ExpectedExceptionContains`. Validates the full
	 * "Prepare success / Execute exception / non-empty message / message
	 * contains substring / non-zero line" five-tuple.
	 *
	 * Caller is responsible for any necessary `AddExpectedError` registration
	 * — the exception will be logged by the AS log handler regardless.
	 */
	inline bool ExecuteFunctionExpectingScriptException(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		asIScriptModule& Module,
		const FBindingsCoverageProfile& Profile,
		const TCHAR* FunctionDecl,
		const TCHAR* CaseLabel,
		const FString& ExpectedExceptionContains)
	{
		const FString Label = FormatCaseLabel(Profile, CaseLabel);
		Test.AddInfo(Label);

		asIScriptFunction* Function = AngelscriptReflectiveAccess::ResolveFunctionByDecl(Test, Module, FunctionDecl);
		if (Function == nullptr)
		{
			return false;
		}

		FAngelscriptEngineScope EngineScope(Engine);
		asIScriptContext* Context = Engine.CreateContext();
		if (!Test.TestNotNull(*FString::Printf(TEXT("%s should create execution context"), *Label), Context))
		{
			return false;
		}
		ON_SCOPE_EXIT { Context->Release(); };

		const int PrepareResult = Context->Prepare(Function);
		const int ExecuteResult = PrepareResult == asSUCCESS ? Context->Execute() : PrepareResult;
		const FString ExceptionString = UTF8_TO_TCHAR(
			Context->GetExceptionString() != nullptr ? Context->GetExceptionString() : "");
		const int32 ExceptionLine = Context->GetExceptionLineNumber();

		bool bPassed = true;
		bPassed &= Test.TestEqual(
			*FString::Printf(TEXT("%s should Prepare successfully (code=%d)"), *Label, PrepareResult),
			PrepareResult,
			static_cast<int32>(asSUCCESS));
		bPassed &= Test.TestEqual(
			*FString::Printf(TEXT("%s should raise asEXECUTION_EXCEPTION (got=%d)"), *Label, ExecuteResult),
			ExecuteResult,
			static_cast<int32>(asEXECUTION_EXCEPTION));
		bPassed &= Test.TestFalse(
			*FString::Printf(TEXT("%s should produce non-empty exception text"), *Label),
			ExceptionString.IsEmpty());
		bPassed &= Test.TestTrue(
			*FString::Printf(TEXT("%s exception '%s' should contain '%s'"),
				*Label, *ExceptionString, *ExpectedExceptionContains),
			ExceptionString.Contains(ExpectedExceptionContains));
		bPassed &= Test.TestTrue(
			*FString::Printf(TEXT("%s should report a positive exception line (got=%d)"), *Label, ExceptionLine),
			ExceptionLine > 0);

		Test.AddInfo(FString::Printf(TEXT("%s raised at line %d: %s"), *Label, ExceptionLine, *ExceptionString));
		return bPassed;
	}

	// ====================================================================
	// Return-type coverage helpers
	//
	// These invoke a no-arg global function whose *declared* return type is
	// the type under test (bool, float, FString, FVector, TArray, TSet,
	// TMap, ...).  The caller supplies a Validator lambda that receives the
	// raw return address and performs assertions.
	//
	// Usage:
	//   ExpectGlobalReturnBool(Test, Engine, Module, Profile,
	//       TEXT("bool F()"), TEXT("should return true"), true);
	//
	//   ExpectGlobalReturnFloat(Test, Engine, Module, Profile,
	//       TEXT("float F()"), TEXT("should be ~3.5"), 3.5f, 0.01f);
	//
	//   ExpectGlobalReturnCustom<FVector>(Test, Engine, Module, Profile,
	//       TEXT("FVector F()"), TEXT("X should be ~1"),
	//       [](auto& T, const FVector& V) { return T.TestTrue(..., V.X > 0.9f); });
	// ====================================================================

	/** Invoke a no-arg `bool F()` global, compare return. */
	inline bool ExpectGlobalReturnBool(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		asIScriptModule& Module,
		const FBindingsCoverageProfile& Profile,
		const TCHAR* FunctionDecl,
		const TCHAR* CaseLabel,
		bool Expected)
	{
		Detail::TraceCase(Test, Profile, CaseLabel);
		AngelscriptReflectiveAccess::FASGlobalFunctionInvoker Invoker(Test, Engine, Module, FunctionDecl);
		if (!Invoker.IsValid()) return false;
		const bool Actual = Invoker.CallAndReturn<bool>(false);
		return Test.TestEqual(
			*FString::Printf(TEXT("%s (decl=%s)"), *FormatCaseLabel(Profile, CaseLabel), FunctionDecl),
			Actual, Expected);
	}

	/** Invoke a no-arg `float F()` global, compare with tolerance.
	 *  Note: AS engine runs with asEP_FLOAT_IS_FLOAT64=1 so script `float`
	 *  is actually stored as double in the return register. */
	inline bool ExpectGlobalReturnFloat(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		asIScriptModule& Module,
		const FBindingsCoverageProfile& Profile,
		const TCHAR* FunctionDecl,
		const TCHAR* CaseLabel,
		float Expected,
		float Tolerance = 0.01f)
	{
		Detail::TraceCase(Test, Profile, CaseLabel);
		AngelscriptReflectiveAccess::FASGlobalFunctionInvoker Invoker(Test, Engine, Module, FunctionDecl);
		if (!Invoker.IsValid()) return false;
		// AS float is double under asEP_FLOAT_IS_FLOAT64; read as double.
		const double Actual = Invoker.CallAndReturn<double>(0.0);
		return Test.TestTrue(
			*FString::Printf(TEXT("%s (decl=%s) returned %.6g, expected %.6g (tol=%g)"),
				*FormatCaseLabel(Profile, CaseLabel), FunctionDecl, Actual, (double)Expected, (double)Tolerance),
			FMath::IsNearlyEqual(Actual, (double)Expected, (double)Tolerance));
	}

	/**
	 * Invoke a no-arg function returning a struct/container, read via
	 * GetAddressOfReturnValue, and hand off to a caller-supplied validator.
	 *
	 * Validator signature: bool(FAutomationTestBase& Test, const T& Value)
	 * Return true if all assertions passed.
	 */
	template <typename T, typename ValidatorFn>
	inline bool ExpectGlobalReturnCustom(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		asIScriptModule& Module,
		const FBindingsCoverageProfile& Profile,
		const TCHAR* FunctionDecl,
		const TCHAR* CaseLabel,
		ValidatorFn&& Validator)
	{
		Detail::TraceCase(Test, Profile, CaseLabel);
		AngelscriptReflectiveAccess::FASGlobalFunctionInvoker Invoker(Test, Engine, Module, FunctionDecl);
		if (!Invoker.IsValid()) return false;
		if (!Invoker.Call()) return false;

		T Value{};
		if (!Invoker.ReadReturnStruct(Value))
		{
			Test.AddError(FString::Printf(TEXT("%s failed to read return struct"), *FormatCaseLabel(Profile, CaseLabel)));
			return false;
		}
		return Validator(Test, Value);
	}
}

#endif // WITH_DEV_AUTOMATION_TESTS
