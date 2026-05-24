#pragma once

// ============================================================================
// AngelscriptTestExecute
// ============================================================================
//
// Themed sub-header split out of `AngelscriptTestUtilities.h` (Phase 1 of
// OpenSpec change `refactor-as-test-shared-layout-and-naming`).
//
// Responsibility:
//   - Hosts the **execution-side** helpers that drive an already-compiled
//     `asIScriptFunction` through an `asIScriptContext` and surface the
//     result to a `FAutomationTestBase`.
//   - Three legacy entry points are kept here verbatim during Phase 1:
//       * ExecuteIntFunction
//       * ExecuteIntFunctionExpectingScriptException
//       * ExecuteInt64Function
//
// Phase 2/3 (see OpenSpec tasks 2.x / 3.x) will:
//   - Fold `AngelscriptGlobalFunctionInvoker.h` (FASGlobalFunctionInvoker)
//     and `AngelscriptBindingsAssertions.h` (ExpectGlobal*) into this header.
//   - Introduce `FAngelscriptTestExecutor` + the `Execute*` naming family
//     (`ExecuteAndExpect*`, `ExecuteAndExpectNear*`, `ExecuteBatchAndExpect*`,
//     `ExecuteAndValidate<T>`, `CompileAndExpectFailure`), with the legacy
//     names kept as permanent inline aliases.
//
// Original location: AngelscriptTestUtilities.h lines 873-1007.
// ============================================================================

#include "AngelscriptEngine.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Containers/StringConv.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_context.h"
#include "EndAngelscriptHeaders.h"

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
