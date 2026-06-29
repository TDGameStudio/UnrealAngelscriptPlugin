#pragma once

// ============================================================================
// AngelscriptTestExecute
// ============================================================================
//
// Single themed entry point for driving AngelScript functions from C++ tests.
// Carved out of `AngelscriptTestUtilities.h` in Phase 1 of OpenSpec change
// `refactor-as-test-shared-layout-and-naming`, then consolidated in Phase 2
// (the old `AngelscriptGlobalFunctionInvoker.h` + `AngelscriptBindingsAssertions.h`
// were folded in verbatim and reduced to forward stubs), and finally
// rebalanced in Phase 3 to introduce the canonical `Execute*` naming family
// + `FAngelscriptTestExecutor` class as the **new mandatory entry point**
// for new code, with every legacy name kept as a permanent inline alias /
// using-declaration / forwarder so existing call sites continue to compile
// against the same fully-qualified symbol names.
//
// FOUR NAMESPACES coexist here:
//
//   namespace AngelscriptTestSupport                          [Phase 1 - Utilities-era]
//     Legacy entry points that take `asIScriptFunction&` directly (no
//     Profile / CaseLabel). Bodies are intentionally preserved verbatim
//     -- their callers depend on the specific error-string wording, and
//     `FAngelscriptTestExecutor` has a richer surface that does not map
//     1:1 (no Profile / no Case / no per-case AddInfo trace). Migration
//     of these three functions is deferred to a follow-up change; new
//     code should NOT call them.
//       * ExecuteIntFunction
//       * ExecuteIntFunctionExpectingScriptException
//       * ExecuteInt64Function
//
//   namespace AngelscriptTest                                 [Phase 3 - NEW primary]
//     Mandatory entry point for new tests. All `Execute*` and `Compile*`
//     helpers live here, alongside the renamed executor class.
//       * FAngelscriptTestExecutor (renamed from FASGlobalFunctionInvoker)
//           .Execute()                  (was .Call)
//           .ExecuteAndGet<R>()         (was .CallAndReturn<R>)
//           .ExecuteAndExtractStruct<T>() (was .ReadReturnStruct<T>)
//           .AddArg / .AddArgRef / .AddArgStruct  (unchanged)
//           legacy .Call / .CallAndReturn<R> / .ReadReturnStruct<T> kept
//             as inline forwarders to the new names.
//       * ResolveFunctionByDecl / ResolveFunctionByName (moved out of
//         AngelscriptReflectiveAccess; original namespace gets aliases).
//       * Execute* family — equality / tolerance / lower-bound / batch /
//         custom-validator / exception:
//           ExecuteAndExpectInt        ExecuteAndExpectNearFloat
//           ExecuteAndExpectBool       ExecuteAndExpectNearDouble
//           ExecuteAndExpectDouble     ExecuteAndExpectIntAtLeast
//           ExecuteBatchAndExpectInt   ExecuteAndValidate<T>
//           ExecuteAndExpectException
//       * Compile* family (compile-side, independent of Execute*):
//           CompileAndExpectFailure
//       * FExpectedInt — batch row struct for ExecuteBatchAndExpectInt.
//
//   namespace AngelscriptReflectiveAccess                     [Phase 2/3 back-compat]
//     Class + Resolve helpers used to live here. The class is now an
//     alias of `FAngelscriptTestExecutor`; the resolve
//     helpers are inline forwarders to the new home. Existing call sites
//     stay valid; new code should `*` directly.
//       * using FASGlobalFunctionInvoker = FAngelscriptTestExecutor
//       * inline ResolveFunctionByDecl / ResolveFunctionByName (forwarders)
//
//   namespace AngelscriptTestBindings                         [Phase 2/3 back-compat]
//     The 9 `ExpectGlobal*` helpers + `ExpectBindingCompileFailure` +
//     `ExecuteFunctionExpectingScriptException` are now thin inline
//     forwarders to the corresponding `*` entry. The
//     batch struct gets a `using` alias. The shape & semantics are
//     preserved one-to-one; the only observable change is that the
//     work physically happens behind a new function name.
//       * inline ExpectGlobalInt -> ExecuteAndExpectInt
//       * inline ExpectGlobalIntAtLeast -> ExecuteAndExpectIntAtLeast
//       * inline ExpectGlobalBool -> ExecuteAndExpectBool
//       * inline ExpectGlobalDouble -> ExecuteAndExpectNearDouble
//       * inline ExpectGlobalInts(batch) -> ExecuteBatchAndExpectInt
//       * inline ExpectGlobalReturnBool -> ExecuteAndExpectBool
//       * inline ExpectGlobalReturnFloat -> ExecuteAndExpectNearFloat
//       * inline ExpectGlobalReturnCustom<T> -> ExecuteAndValidate<T>
//       * inline ExpectBindingCompileFailure -> CompileAndExpectFailure
//       * inline ExecuteFunctionExpectingScriptException -> ExecuteAndExpectException
//       * using FExpectedGlobalInt = FExpectedInt
//       * namespace Detail { TraceCase } (kept for any external callers)
//     Guarded by `WITH_DEV_AUTOMATION_TESTS` to match the original layout.
//
// Naming family contract (Phase 3 / OpenSpec spec
// `as-bindings-test-execute-and-naming/spec.md`):
//
//   Execute[(empty)|AndGet|AndExpect|AndValidate|BatchAndExpect][Near|AtLeast|(empty)][<Type>|<T>]
//
//     Execute          — execute, no return.
//     AndGet           — execute, return raw value, no assertion.
//     AndExpect        — execute, return raw value, assert equality.
//     AndExpectNear    — execute, return raw value, assert within tolerance.
//     AndExpectAtLeast — execute, return raw value, assert >= minimum.
//     AndValidate      — execute, return raw value, caller-supplied validator.
//     BatchAndExpect   — execute N rows of (decl, label, expected) tuples.
//     ExecuteAndExpectException — execute, expect script exception, validate
//       message + line metadata.
//     CompileAndExpect* — compile-side family, independent of Execute (the
//       script never reaches asIScriptContext::Execute when it does not
//       compile in the first place).
//
// ============================================================================

#include "CoreMinimal.h"
#include "AngelscriptEngine.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Containers/StringConv.h"
#include "UObject/Object.h"

#include "AngelscriptTestEngineHelper.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_context.h"
#include "source/as_scriptengine.h"
#include "source/as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

// ============================================================================
// PART 1 — namespace AngelscriptTestSupport (Utilities-era legacy, Phase 1)
//
// These three free functions accept an already-resolved `asIScriptFunction&`
// rather than a (Module, Decl) pair. They predate the Bindings Coverage
// refactor and are NOT migrated to the new Execute* family in Phase 3: their
// callers depend on the exact error-string wording, and the new family takes
// `Profile` + `CaseLabel` which these signatures lack. Migration is tracked
// in `followups.md` of the parent OpenSpec change.
// ============================================================================
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


// ============================================================================
// PART 2 — namespace AngelscriptTest (NEW primary, Phase 3)
//
// New code MUST call helpers from this namespace. Old `AngelscriptTestBindings`
// helpers are inline forwarders to here; the layer is preserved indefinitely
// so 71+ Bindings/*.cpp call sites can be migrated incrementally.
// ============================================================================
/**
 * Resolve an asIScriptFunction by its AS declaration on the given module.
 *
 * Mirrors the logic of GetFunctionByDecl: try the
 * full declaration, then fall back to the bare name, then scan the module
 * by-index. We materialise a null-terminated FString before the UTF-8
 * conversion -- FStringView::GetData() is not guaranteed null-terminated.
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
 * Typed builder around `asIScriptContext` that matches the argument slots
 * of an AS global function. Each `AddArg` overload advances the cursor.
 * `Execute()` / `ExecuteAndGet<R>()` execute the context and tear it down.
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
 * Return mapping (`ExecuteAndGet<R>`):
 *   R = bool / integer / enum -> GetReturnByte/Word/DWord/QWord
 *   R = float / double        -> GetReturnFloat / GetReturnDouble
 *   R = T*                    -> GetReturnObject
 *
 * For AS `float` parameters, the AS runtime applies asEP_FLOAT_IS_FLOAT64=1
 * so the UFunction-side type is FDoubleProperty -- but at the raw AS
 * context level the parameter is still a `float`. So at this layer callers
 * should use AddArg(1.0f), NOT AddArg(1.0). (The UFUNCTION path is the
 * only place where you need `AddParam<double>`.)
 *
 * Legacy method names (`.Call`, `.CallAndReturn<R>`, `.ReadReturnStruct<T>`)
 * are kept as permanent inline forwarders so old call sites continue to
 * compile against the same surface.
 */
struct FAngelscriptTestExecutor
{
	FAngelscriptTestExecutor(
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
	FAngelscriptTestExecutor(
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

	~FAngelscriptTestExecutor()
	{
		if (Context != nullptr)
		{
			Context->Release();
			Context = nullptr;
		}
		EngineScope.Reset();
	}

	FAngelscriptTestExecutor(const FAngelscriptTestExecutor&) = delete;
	FAngelscriptTestExecutor& operator=(const FAngelscriptTestExecutor&) = delete;

	bool IsValid() const { return bValid; }
	asIScriptContext* GetContext() const { return Context; }

	// Typed argument setters -- each advances the cursor by one AS slot.
	FAngelscriptTestExecutor& AddArg(bool    Value)    { return SetArg([&]{ return Context->SetArgByte  (NextArgIndex, Value ? 1 : 0); }); }
	FAngelscriptTestExecutor& AddArg(uint8   Value)    { return SetArg([&]{ return Context->SetArgByte  (NextArgIndex, Value); }); }
	FAngelscriptTestExecutor& AddArg(int8    Value)    { return SetArg([&]{ return Context->SetArgByte  (NextArgIndex, static_cast<uint8>(Value)); }); }
	FAngelscriptTestExecutor& AddArg(uint16  Value)    { return SetArg([&]{ return Context->SetArgWord  (NextArgIndex, Value); }); }
	FAngelscriptTestExecutor& AddArg(int16   Value)    { return SetArg([&]{ return Context->SetArgWord  (NextArgIndex, static_cast<uint16>(Value)); }); }
	FAngelscriptTestExecutor& AddArg(uint32  Value)    { return SetArg([&]{ return Context->SetArgDWord (NextArgIndex, Value); }); }
	FAngelscriptTestExecutor& AddArg(int32   Value)    { return SetArg([&]{ return Context->SetArgDWord (NextArgIndex, static_cast<uint32>(Value)); }); }
	FAngelscriptTestExecutor& AddArg(uint64  Value)    { return SetArg([&]{ return Context->SetArgQWord (NextArgIndex, Value); }); }
	FAngelscriptTestExecutor& AddArg(int64   Value)    { return SetArg([&]{ return Context->SetArgQWord (NextArgIndex, static_cast<uint64>(Value)); }); }
	FAngelscriptTestExecutor& AddArg(float   Value)    { return SetArg([&]{ return Context->SetArgFloat (NextArgIndex, Value); }); }
	FAngelscriptTestExecutor& AddArg(double  Value)    { return SetArg([&]{ return Context->SetArgDouble(NextArgIndex, Value); }); }
	FAngelscriptTestExecutor& AddArgObject(void* Obj)  { return SetArg([&]{ return Context->SetArgObject(NextArgIndex, Obj); }); }
	FAngelscriptTestExecutor& AddArgAddress(void* Ptr) { return SetArg([&]{ return Context->SetArgAddress(NextArgIndex, Ptr); }); }

	/**
	 * Bind a reference-style argument (AS `&in` / `&out` / `&inout`) to the
	 * supplied live storage. The caller owns the lifetime and can read out
	 * any modifications after `Execute()` returns.
	 */
	template <typename T>
	FAngelscriptTestExecutor& AddArgRef(T& InOutRef)
	{
		return AddArgAddress(const_cast<std::remove_const_t<T>*>(&InOutRef));
	}

	/**
	 * Bind a value-style struct argument (AS USTRUCT passed by value) by
	 * copying through SetArgObject. The AS engine does NOT destroy the
	 * argument -- our live C++ temporary is torn down by the normal scope.
	 */
	template <typename T>
	FAngelscriptTestExecutor& AddArgStruct(T& LiveValue)
	{
		return AddArgObject(static_cast<void*>(&LiveValue));
	}

	/** Execute the function. Returns true if it ran to completion. */
	bool Execute()
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

	/** Execute the function and return the integer-family / pointer return value. */
	template <typename ReturnType>
	ReturnType ExecuteAndGet(const ReturnType& Fallback = ReturnType{})
	{
		if (!Execute())
		{
			return Fallback;
		}
		return ReadReturn<ReturnType>(Fallback);
	}

	/**
	 * Execute the function and read a USTRUCT return value out of the
	 * return register. Caller owns the copy. Returns true if the call ran
	 * and the address was readable.
	 */
	template <typename StructType>
	bool ExecuteAndExtractStruct(StructType& OutValue)
	{
		if (!Execute())
		{
			return false;
		}
		return ReadReturnStructInternal(OutValue);
	}

	/** Return whether `Execute()` has been invoked. */
	bool HasRun() const { return bHasRun; }

	// ---- Permanent inline aliases (legacy method names from Phase 2) ----
	// New code MUST use Execute / ExecuteAndGet / ExecuteAndExtractStruct.
	bool Call() { return Execute(); }

	template <typename ReturnType>
	ReturnType CallAndReturn(const ReturnType& Fallback = ReturnType{})
	{
		return ExecuteAndGet<ReturnType>(Fallback);
	}

	template <typename StructType>
	bool ReadReturnStruct(StructType& OutValue)
	{
		// Legacy semantics: caller had already invoked Call(), so we should
		// NOT execute again -- just read the existing return value off the
		// already-completed context. This preserves the original two-step
		// pattern (Call() + ReadReturnStruct(...)).
		if (!bHasRun)
		{
			Test.AddError(TEXT("ReadReturnStruct called before Call() completed"));
			return false;
		}
		return ReadReturnStructInternal(OutValue);
	}

private:
	template <typename SetArgFn>
	FAngelscriptTestExecutor& SetArg(SetArgFn&& Fn)
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

	// Return-value extractors. We specialise via if-constexpr so callers
	// just say `ExecuteAndGet<int32>()`.
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
			static_assert(sizeof(R) == 0, "Unsupported return type for FAngelscriptTestExecutor::ExecuteAndGet -- "
				"use ExecuteAndExtractStruct<T>() for USTRUCTs or call through a dedicated helper.");
			return Fallback;
		}
	}

	template <typename StructType>
	bool ReadReturnStructInternal(StructType& OutValue)
	{
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

/** Legacy alias for FAngelscriptTestExecutor. */
using FASGlobalFunctionInvoker = FAngelscriptTestExecutor;

#if WITH_DEV_AUTOMATION_TESTS

// ========================================================================
// Execute* family of free-function assertion helpers
//
// Module + decl + CaseLabel -- one-line per case. Every helper
// emits an Info trace line with the supplied case label so a passing run still
// leaves a readable per-case trail in the automation log. Failed helpers
// add a detailed AddError describing the actual value vs expected.
//
// Naming family (see top-of-file contract):
//   Execute[AndExpect|AndExpectNear|AndExpectAtLeast|AndValidate|BatchAndExpect][<Type>]
// ========================================================================

/** Invoke a no-arg `int F()` global; assert equality. */
inline bool ExecuteAndExpectInt(
	FAutomationTestBase& Test,
	FAngelscriptEngine& Engine,
	asIScriptModule& Module,
	const TCHAR* FunctionDecl,
	const TCHAR* CaseLabel,
	int32 Expected)
{
	Test.AddInfo(FString(CaseLabel));
	FAngelscriptTestExecutor Executor(Test, Engine, Module, FunctionDecl);
	if (!Executor.IsValid())
	{
		return false;
	}
	const int32 Actual = Executor.ExecuteAndGet<int32>(INDEX_NONE);
	return Test.TestEqual(
		*FString::Printf(TEXT("%s (decl=%s)"), *FString(CaseLabel), FunctionDecl),
		Actual,
		Expected);
}

/** Invoke a no-arg `int F()` global; assert `Actual >= Minimum`. */
inline bool ExecuteAndExpectIntAtLeast(
	FAutomationTestBase& Test,
	FAngelscriptEngine& Engine,
	asIScriptModule& Module,
	const TCHAR* FunctionDecl,
	const TCHAR* CaseLabel,
	int32 Minimum)
{
	Test.AddInfo(FString(CaseLabel));
	FAngelscriptTestExecutor Executor(Test, Engine, Module, FunctionDecl);
	if (!Executor.IsValid())
	{
		return false;
	}
	const int32 Actual = Executor.ExecuteAndGet<int32>(INDEX_NONE);
	return Test.TestTrue(
		*FString::Printf(TEXT("%s (decl=%s) returned %d, expected >= %d"),
			*FString(CaseLabel), FunctionDecl, Actual, Minimum),
		Actual >= Minimum);
}

/**
 * Invoke a no-arg `int F()` (or `bool F()` returning 0/1) and compare
 * boolean result. AS still returns the value via GetReturnDWord, so we
 * compare integer-int as 0/1 to preserve the historical contract.
 */
inline bool ExecuteAndExpectBool(
	FAutomationTestBase& Test,
	FAngelscriptEngine& Engine,
	asIScriptModule& Module,
	const TCHAR* FunctionDecl,
	const TCHAR* CaseLabel,
	bool Expected)
{
	Test.AddInfo(FString(CaseLabel));
	FAngelscriptTestExecutor Executor(Test, Engine, Module, FunctionDecl);
	if (!Executor.IsValid())
	{
		return false;
	}
	const bool Actual = Executor.ExecuteAndGet<bool>(false);
	return Test.TestEqual(
		*FString::Printf(TEXT("%s (decl=%s)"), *FString(CaseLabel), FunctionDecl),
		Actual,
		Expected);
}

/**
 * Invoke a no-arg `double F()` global; assert *strict equality* (no
 * tolerance). For float math comparisons use `ExecuteAndExpectNearDouble`.
 */
inline bool ExecuteAndExpectDouble(
	FAutomationTestBase& Test,
	FAngelscriptEngine& Engine,
	asIScriptModule& Module,
	const TCHAR* FunctionDecl,
	const TCHAR* CaseLabel,
	double Expected)
{
	Test.AddInfo(FString(CaseLabel));
	FAngelscriptTestExecutor Executor(Test, Engine, Module, FunctionDecl);
	if (!Executor.IsValid())
	{
		return false;
	}
	const double Actual = Executor.ExecuteAndGet<double>(0.0);
	return Test.TestEqual(
		*FString::Printf(TEXT("%s (decl=%s) returned %.9g, expected %.9g"),
			*FString(CaseLabel), FunctionDecl, Actual, Expected),
		Actual,
		Expected);
}

/** Invoke a no-arg `float F()` global; assert within `Tolerance`.
 *  Note: AS engine runs with asEP_FLOAT_IS_FLOAT64=1 so script `float`
 *  is actually stored as double in the return register. */
inline bool ExecuteAndExpectNearFloat(
	FAutomationTestBase& Test,
	FAngelscriptEngine& Engine,
	asIScriptModule& Module,
	const TCHAR* FunctionDecl,
	const TCHAR* CaseLabel,
	float Expected,
	float Tolerance = 0.01f)
{
	Test.AddInfo(FString(CaseLabel));
	FAngelscriptTestExecutor Executor(Test, Engine, Module, FunctionDecl);
	if (!Executor.IsValid())
	{
		return false;
	}
	const double Actual = Executor.ExecuteAndGet<double>(0.0);
	return Test.TestTrue(
		*FString::Printf(TEXT("%s (decl=%s) returned %.6g, expected %.6g (tol=%g)"),
			*FString(CaseLabel), FunctionDecl, Actual, (double)Expected, (double)Tolerance),
		FMath::IsNearlyEqual(Actual, (double)Expected, (double)Tolerance));
}

/** Invoke a no-arg `double F()` global; assert within `Tolerance`. */
inline bool ExecuteAndExpectNearDouble(
	FAutomationTestBase& Test,
	FAngelscriptEngine& Engine,
	asIScriptModule& Module,
	const TCHAR* FunctionDecl,
	const TCHAR* CaseLabel,
	double Expected,
	double Tolerance = 1e-6)
{
	Test.AddInfo(FString(CaseLabel));
	FAngelscriptTestExecutor Executor(Test, Engine, Module, FunctionDecl);
	if (!Executor.IsValid())
	{
		return false;
	}
	const double Actual = Executor.ExecuteAndGet<double>(0.0);
	return Test.TestTrue(
		*FString::Printf(TEXT("%s (decl=%s) returned %.9g, expected %.9g (tol=%g)"),
			*FString(CaseLabel), FunctionDecl, Actual, Expected, Tolerance),
		FMath::IsNearlyEqual(Actual, Expected, Tolerance));
}

/** One row in an `ExecuteBatchAndExpectInt` batch. */
struct FExpectedInt
{
	const TCHAR* FunctionDecl;
	const TCHAR* CaseLabel;
	int32 Expected;
};

/** Invoke many `int F()` globals and compare each. Aggregate pass/fail. */
inline bool ExecuteBatchAndExpectInt(
	FAutomationTestBase& Test,
	FAngelscriptEngine& Engine,
	asIScriptModule& Module,
	TArrayView<const FExpectedInt> Cases)
{
	bool bPassed = true;
	for (const FExpectedInt& Case : Cases)
	{
		bPassed &= ExecuteAndExpectInt(Test, Engine, Module,
			Case.FunctionDecl, Case.CaseLabel, Case.Expected);
	}
	return bPassed;
}

/**
 * Invoke a no-arg function returning a struct/container, read via
 * `ExecuteAndExtractStruct<T>`, and hand off to a caller-supplied
 * validator. Validator signature:
 *
 *     bool(FAutomationTestBase& Test, const T& Value)
 *
 * Validator returns true if all of its assertions passed.
 */
template <typename T, typename ValidatorFn>
inline bool ExecuteAndValidate(
	FAutomationTestBase& Test,
	FAngelscriptEngine& Engine,
	asIScriptModule& Module,
	const TCHAR* FunctionDecl,
	const TCHAR* CaseLabel,
	ValidatorFn&& Validator)
{
	Test.AddInfo(FString(CaseLabel));
	FAngelscriptTestExecutor Executor(Test, Engine, Module, FunctionDecl);
	if (!Executor.IsValid())
	{
		return false;
	}
	T Value{};
	if (!Executor.ExecuteAndExtractStruct<T>(Value))
	{
		Test.AddError(FString::Printf(TEXT("%s failed to read return struct"), *FString(CaseLabel)));
		return false;
	}
	return Validator(Test, Value);
}

/**
 * Negative path: invoke a function (typically `void F()` -- any return is
 * ignored) and assert that AS execution raises an exception whose message
 * *contains* `ExpectedExceptionContains`. Validates the full five-tuple:
 * Prepare success / Execute exception / non-empty message / message
 * substring / non-zero exception line.
 *
 * Caller is responsible for any necessary `AddExpectedError` registration
 * -- the exception will be logged by the AS log handler regardless.
 */
inline bool ExecuteAndExpectException(
	FAutomationTestBase& Test,
	FAngelscriptEngine& Engine,
	asIScriptModule& Module,
	const TCHAR* FunctionDecl,
	const TCHAR* CaseLabel,
	const FString& ExpectedExceptionContains)
{
	const FString Label(CaseLabel);
	Test.AddInfo(Label);

	asIScriptFunction* Function = ResolveFunctionByDecl(Test, Module, FunctionDecl);
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

// ========================================================================
// Compile* family — compile-side assertions, independent of Execute*.
//
// These never reach `asIScriptContext::Execute()` since the script does
// not compile in the first place. Tracked as a separate naming family so
// the failure mode (compile diagnostics) is obvious from the name.
// ========================================================================

/**
 * Compile-negative binding contract. Use this when a binding surface is
 * intentionally not available on the current branch: the test still runs,
 * proves the script fails to compile, and records the missing symbol or
 * unsupported API in the compile diagnostics.
 */
inline bool CompileAndExpectFailure(
	FAutomationTestBase& Test,
	FAngelscriptEngine& Engine,
	const TCHAR* ModuleName,
	const TCHAR* Source,
	const TCHAR* CaseLabel,
	TArrayView<const FString> ExpectedDiagnosticFragments)
{
	Test.AddInfo(FString(CaseLabel));

	FAngelscriptCompileTraceSummary Summary;
	const FString ModuleNameString(ModuleName);
	const FString Filename = FString::Printf(TEXT("%s.as"), *ModuleNameString);
	CompileModuleWithSummary(
		&Engine,
		ECompileType::FullReload,
		FName(*ModuleNameString),
		Filename,
		FString(Source),
		true,
		Summary,
		true);

	bool bPassed = true;
	bPassed &= Test.TestFalse(
		*FString::Printf(TEXT("%s should fail to compile as an explicit binding boundary"), *FString(CaseLabel)),
		Summary.bCompileSucceeded);
	bPassed &= Test.TestEqual(
		*FString::Printf(TEXT("%s compile result should be Error"), *FString(CaseLabel)),
		Summary.CompileResult,
		ECompileResult::Error);

	for (const FString& ExpectedFragment : ExpectedDiagnosticFragments)
	{
		bool bFoundFragment = false;
		for (const FAngelscriptCompileTraceDiagnosticSummary& Diagnostic : Summary.Diagnostics)
		{
			if (Diagnostic.bIsError && Diagnostic.Message.Contains(ExpectedFragment))
			{
				bFoundFragment = true;
				break;
			}
		}

		bPassed &= Test.TestTrue(
			*FString::Printf(TEXT("%s diagnostics should contain '%s'"),
				*FString(CaseLabel),
				*ExpectedFragment),
			bFoundFragment);
	}

	if (!bPassed || Summary.Diagnostics.Num() == 0)
	{
		Test.AddInfo(FString::Printf(TEXT("%s compile diagnostics: %d"),
			*FString(CaseLabel),
			Summary.Diagnostics.Num()));
		for (const FAngelscriptCompileTraceDiagnosticSummary& Diagnostic : Summary.Diagnostics)
		{
			Test.AddInfo(FString::Printf(
				TEXT("  %s Row%d:Col%d %s"),
				Diagnostic.bIsError ? TEXT("ERROR") : (Diagnostic.bIsInfo ? TEXT("INFO") : TEXT("WARN")),
				Diagnostic.Row,
				Diagnostic.Column,
				*Diagnostic.Message));
		}
	}

	Engine.DiscardModule(*ModuleNameString);
	return bPassed;
}

inline bool CompileAndExpectFailure(
	FAutomationTestBase& Test,
	FAngelscriptEngine& Engine,
	const TCHAR* ModuleName,
	const FString& Source,
	const TCHAR* CaseLabel,
	TArrayView<const FString> ExpectedDiagnosticFragments)
{
	return CompileAndExpectFailure(
		Test,
		Engine,
		ModuleName,
		*Source,
		CaseLabel,
		ExpectedDiagnosticFragments);
}

#endif // WITH_DEV_AUTOMATION_TESTS


// ============================================================================
// PART 4 — namespace AngelscriptTestBindings (Phase 2/3 back-compat forwarders)
//
// The 9 `ExpectGlobal*` helpers, `ExpectBindingCompileFailure`, and
// `ExecuteFunctionExpectingScriptException` are inline forwarders that
// delegate to the corresponding `*` entry. Signatures
// preserved verbatim so 260+ Bindings test call sites compile unchanged.
// New code MUST use the Execute*/Compile* family directly.
// ============================================================================
#if WITH_DEV_AUTOMATION_TESTS

/** Internal trace helper kept for any external callers of `AngelscriptTestTraceCase`. */
inline bool AngelscriptTestTraceCase(
	FAutomationTestBase& Test,
	const TCHAR* CaseLabel)
{
	Test.AddInfo(FString(CaseLabel));
	return true;
}


/** Legacy alias for the batch row struct -- now lives in . */
using FExpectedGlobalInt = FExpectedInt;

/** Legacy forwarder -- new code should call `CompileAndExpectFailure`. */
inline bool ExpectBindingCompileFailure(
	FAutomationTestBase& Test,
	FAngelscriptEngine& Engine,
	const TCHAR* ModuleName,
	const TCHAR* Source,
	const TCHAR* CaseLabel,
	TArrayView<const FString> ExpectedDiagnosticFragments)
{
	return CompileAndExpectFailure(
		Test, Engine, ModuleName, Source, CaseLabel, ExpectedDiagnosticFragments);
}

/** Legacy forwarder -- new code should call `ExecuteAndExpectInt`. */
inline bool ExpectGlobalInt(
	FAutomationTestBase& Test,
	FAngelscriptEngine& Engine,
	asIScriptModule& Module,
	const TCHAR* FunctionDecl,
	const TCHAR* CaseLabel,
	int32 Expected)
{
	return ExecuteAndExpectInt(
		Test, Engine, Module, FunctionDecl, CaseLabel, Expected);
}

/** Legacy forwarder -- new code should call `ExecuteAndExpectIntAtLeast`. */
inline bool ExpectGlobalIntAtLeast(
	FAutomationTestBase& Test,
	FAngelscriptEngine& Engine,
	asIScriptModule& Module,
	const TCHAR* FunctionDecl,
	const TCHAR* CaseLabel,
	int32 Minimum)
{
	return ExecuteAndExpectIntAtLeast(
		Test, Engine, Module, FunctionDecl, CaseLabel, Minimum);
}

/** Legacy forwarder -- new code should call `ExecuteAndExpectBool`. */
inline bool ExpectGlobalBool(
	FAutomationTestBase& Test,
	FAngelscriptEngine& Engine,
	asIScriptModule& Module,
	const TCHAR* FunctionDecl,
	const TCHAR* CaseLabel,
	bool Expected)
{
	return ExecuteAndExpectBool(
		Test, Engine, Module, FunctionDecl, CaseLabel, Expected);
}

/** Legacy forwarder -- new code should call `ExecuteAndExpectNearDouble`.
 *  Note: legacy `ExpectGlobalDouble` always used a tolerance (defaulting to 1e-6) and is
 *  therefore mapped to the *Near* variant of the new family. */
inline bool ExpectGlobalDouble(
	FAutomationTestBase& Test,
	FAngelscriptEngine& Engine,
	asIScriptModule& Module,
	const TCHAR* FunctionDecl,
	const TCHAR* CaseLabel,
	double Expected,
	double Tolerance = 1e-6)
{
	return ExecuteAndExpectNearDouble(
		Test, Engine, Module, FunctionDecl, CaseLabel, Expected, Tolerance);
}

/** Legacy forwarder -- new code should call `ExecuteBatchAndExpectInt`. */
inline bool ExpectGlobalInts(
	FAutomationTestBase& Test,
	FAngelscriptEngine& Engine,
	asIScriptModule& Module,
	TArrayView<const FExpectedGlobalInt> Cases)
{
	// FExpectedGlobalInt is a `using` alias of FExpectedInt,
	// so the array views are layout-compatible without any conversion.
	return ExecuteBatchAndExpectInt(
		Test, Engine, Module,
		TArrayView<const FExpectedInt>(Cases.GetData(), Cases.Num()));
}

/** Legacy forwarder -- new code should call `ExecuteAndExpectBool`.
 *  Functionally identical to `ExpectGlobalBool`; the original distinction was that
 *  this variant explicitly invokes a `bool F()` (vs. an `int F()` returning 0/1).
 *  AS still routes the value through the integer return register, so the new family
 *  collapses both into `ExecuteAndExpectBool`. */
inline bool ExpectGlobalReturnBool(
	FAutomationTestBase& Test,
	FAngelscriptEngine& Engine,
	asIScriptModule& Module,
	const TCHAR* FunctionDecl,
	const TCHAR* CaseLabel,
	bool Expected)
{
	return ExecuteAndExpectBool(
		Test, Engine, Module, FunctionDecl, CaseLabel, Expected);
}

/** Legacy forwarder -- new code should call `ExecuteAndExpectNearFloat`. */
inline bool ExpectGlobalReturnFloat(
	FAutomationTestBase& Test,
	FAngelscriptEngine& Engine,
	asIScriptModule& Module,
	const TCHAR* FunctionDecl,
	const TCHAR* CaseLabel,
	float Expected,
	float Tolerance = 0.01f)
{
	return ExecuteAndExpectNearFloat(
		Test, Engine, Module, FunctionDecl, CaseLabel, Expected, Tolerance);
}

/** Legacy forwarder -- new code should call `ExecuteAndValidate<T>`. */
template <typename T, typename ValidatorFn>
inline bool ExpectGlobalReturnCustom(
	FAutomationTestBase& Test,
	FAngelscriptEngine& Engine,
	asIScriptModule& Module,
	const TCHAR* FunctionDecl,
	const TCHAR* CaseLabel,
	ValidatorFn&& Validator)
{
	return ExecuteAndValidate<T>(
		Test, Engine, Module, FunctionDecl, CaseLabel,
		Forward<ValidatorFn>(Validator));
}

/** Legacy forwarder -- new code should call `ExecuteAndExpectException`. */
inline bool ExecuteFunctionExpectingScriptException(
	FAutomationTestBase& Test,
	FAngelscriptEngine& Engine,
	asIScriptModule& Module,
	const TCHAR* FunctionDecl,
	const TCHAR* CaseLabel,
	const FString& ExpectedExceptionContains)
{
	return ExecuteAndExpectException(
		Test, Engine, Module, FunctionDecl, CaseLabel, ExpectedExceptionContains);
}


#endif // WITH_DEV_AUTOMATION_TESTS
