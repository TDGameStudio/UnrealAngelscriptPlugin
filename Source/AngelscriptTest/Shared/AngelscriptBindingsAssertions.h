#pragma once

#include "CoreMinimal.h"
#include "AngelscriptEngine.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

#include "AngelscriptBindingsCoverage.h"
#include "AngelscriptGlobalFunctionInvoker.h"
#include "AngelscriptTestEngineHelper.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_context.h"
#include "source/as_scriptengine.h"
#include "source/as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

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
