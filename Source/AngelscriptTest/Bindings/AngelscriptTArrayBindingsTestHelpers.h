// Bindings-local helpers shared by TArray and TArray syntax-compat binding tests.

#pragma once

#include "CoreMinimal.h"
#include "AngelscriptEngine.h"
#include "Containers/ScriptArray.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestExecute.h"
#include "Templates/Function.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_context.h"
#include "source/as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

#if WITH_DEV_AUTOMATION_TESTS

struct FArraySyntaxCoverageProfile
{
	const TCHAR* CasePrefix = nullptr;
	const TCHAR* ModulePrefix = nullptr;
	const TCHAR* TraceFunctionDecl = TEXT("void TraceTArrayCase(const FString&in)");
	bool bBracketArraySyntax = false;
};

inline FArraySyntaxCoverageProfile TArrayBindingsCoverageProfile()
{
	return FArraySyntaxCoverageProfile{
		TEXT("TArray"),
		TEXT("ASTArray"),
		TEXT("void TraceTArrayCase(const FString&in)"),
		false};
}

inline FArraySyntaxCoverageProfile TArraySyntaxCompatCoverageProfile()
{
	return FArraySyntaxCoverageProfile{
		TEXT("TArraySyntax"),
		TEXT("ASTArraySyntaxCompat"),
		TEXT("void TraceSyntaxCase(const FString&in)"),
		true};
}

struct FTArrayExpectedGlobalInt
{
	const TCHAR* FunctionDecl = nullptr;
	const TCHAR* ContextLabel = nullptr;
	int32 ExpectedValue = 0;
};

struct FTArrayExpectedGlobalIntAtLeast
{
	const TCHAR* FunctionDecl = nullptr;
	const TCHAR* ContextLabel = nullptr;
	int32 MinimumValue = 0;
};

inline FString TArrayBindingsMakeModuleName(const FArraySyntaxCoverageProfile& Profile, const TCHAR* SectionName)
{
	return FString::Printf(TEXT("%s%s"), Profile.ModulePrefix, SectionName);
}

inline FString TArrayBindingsMakeArrayFunctionDecl(
	const FArraySyntaxCoverageProfile& Profile,
	const TCHAR* ElementType,
	const TCHAR* FunctionName)
{
	if (Profile.bBracketArraySyntax)
	{
		return FString::Printf(TEXT("%s[] %s()"), ElementType, FunctionName);
	}
	return FString::Printf(TEXT("TArray<%s> %s()"), ElementType, FunctionName);
}

inline asIScriptModule* TArrayBindingsBuildCoverageModule(
	FAutomationTestBase& Test,
	FAngelscriptEngine& Engine,
	const FArraySyntaxCoverageProfile& Profile,
	const TCHAR* SectionName,
	const FString& Source)
{
	const FString ModuleName = TArrayBindingsMakeModuleName(Profile, SectionName);
	FTCHARToUTF8 ModuleNameUtf8(*ModuleName);
	return BuildModule(Test, Engine, ModuleNameUtf8.Get(), Source);
}

inline bool TArrayBindingsTraceCase(
	FAutomationTestBase& Test,
	FAngelscriptEngine& Engine,
	asIScriptModule& Module,
	const FArraySyntaxCoverageProfile& Profile,
	const FString& ContextLabel)
{
	FString CaseName(ContextLabel);
	FAngelscriptTestExecutor Invoker(Test, Engine, Module, Profile.TraceFunctionDecl);
	Invoker.AddArgRef(CaseName);
	return Invoker.Execute();
}

inline bool ExpectTArrayBindingsGlobalInt(
	FAutomationTestBase& Test,
	FAngelscriptEngine& Engine,
	asIScriptModule& Module,
	const FArraySyntaxCoverageProfile& Profile,
	const TCHAR* FunctionDecl,
	const TCHAR* ContextLabel,
	int32 ExpectedValue)
{
	const FString ContextLabelString(ContextLabel);
	const bool bTracePassed = TArrayBindingsTraceCase(Test, Engine, Module, Profile, ContextLabelString);
	FAngelscriptTestExecutor Invoker(Test, Engine, Module, FunctionDecl);
	const int32 ActualValue = Invoker.ExecuteAndGet<int32>(INDEX_NONE);
	Test.AddInfo(FString::Printf(TEXT("%s returned %d"), *ContextLabelString, ActualValue));
	return bTracePassed && Test.TestEqual(
		*FString::Printf(TEXT("%s should return the expected script-visible value"), *ContextLabelString),
		ActualValue,
		ExpectedValue);
}

inline bool ExpectTArrayBindingsGlobalIntAtLeast(
	FAutomationTestBase& Test,
	FAngelscriptEngine& Engine,
	asIScriptModule& Module,
	const FArraySyntaxCoverageProfile& Profile,
	const TCHAR* FunctionDecl,
	const TCHAR* ContextLabel,
	int32 MinimumValue)
{
	const FString ContextLabelString(ContextLabel);
	const bool bTracePassed = TArrayBindingsTraceCase(Test, Engine, Module, Profile, ContextLabelString);
	FAngelscriptTestExecutor Invoker(Test, Engine, Module, FunctionDecl);
	const int32 ActualValue = Invoker.ExecuteAndGet<int32>(INDEX_NONE);
	Test.AddInfo(FString::Printf(TEXT("%s returned %d"), *ContextLabelString, ActualValue));
	return bTracePassed && Test.TestTrue(
		*FString::Printf(TEXT("%s should be at least %d"), *ContextLabelString, MinimumValue),
		ActualValue >= MinimumValue);
}

inline bool ExpectTArrayBindingsGlobalInts(
	FAutomationTestBase& Test,
	FAngelscriptEngine& Engine,
	asIScriptModule& Module,
	const FArraySyntaxCoverageProfile& Profile,
	const TArray<FTArrayExpectedGlobalInt>& Cases)
{
	bool bPassed = true;
	for (const FTArrayExpectedGlobalInt& TestCase : Cases)
	{
		bPassed &= ExpectTArrayBindingsGlobalInt(
			Test,
			Engine,
			Module,
			Profile,
			TestCase.FunctionDecl,
			TestCase.ContextLabel,
			TestCase.ExpectedValue);
	}
	return bPassed;
}

inline bool ExpectTArrayBindingsGlobalIntsAtLeast(
	FAutomationTestBase& Test,
	FAngelscriptEngine& Engine,
	asIScriptModule& Module,
	const FArraySyntaxCoverageProfile& Profile,
	const TArray<FTArrayExpectedGlobalIntAtLeast>& Cases)
{
	bool bPassed = true;
	for (const FTArrayExpectedGlobalIntAtLeast& TestCase : Cases)
	{
		bPassed &= ExpectTArrayBindingsGlobalIntAtLeast(
			Test,
			Engine,
			Module,
			Profile,
			TestCase.FunctionDecl,
			TestCase.ContextLabel,
			TestCase.MinimumValue);
	}
	return bPassed;
}

inline bool TArrayBindingsExecuteFunctionExpectingScriptException(
	FAutomationTestBase& Test,
	FAngelscriptEngine& Engine,
	asIScriptModule& Module,
	const FArraySyntaxCoverageProfile& Profile,
	const FString& FunctionDecl,
	const FString& ExpectedExceptionText,
	const FString& ContextLabel)
{
	const FString ContextLabelString(ContextLabel);
	if (!TArrayBindingsTraceCase(Test, Engine, Module, Profile, ContextLabelString))
	{
		return false;
	}

	asIScriptFunction* Function = ResolveFunctionByDecl(Test, Module, FunctionDecl);
	if (Function == nullptr)
	{
		return false;
	}

	FAngelscriptEngineScope EngineScope(Engine);
	asIScriptContext* ScriptContext = Engine.CreateContext();
	if (!Test.TestNotNull(*FString::Printf(TEXT("%s should create an execution context"), *ContextLabelString), ScriptContext))
	{
		return false;
	}

	ON_SCOPE_EXIT
	{
		ScriptContext->Release();
	};

	const int PrepareResult = ScriptContext->Prepare(Function);
	const int ExecuteResult = PrepareResult == asSUCCESS ? ScriptContext->Execute() : PrepareResult;
	const FString ExceptionString = UTF8_TO_TCHAR(
		ScriptContext->GetExceptionString() != nullptr ? ScriptContext->GetExceptionString() : "");
	const int32 ExceptionLine = ScriptContext->GetExceptionLineNumber();

	const bool bPrepared = Test.TestEqual(
		*FString::Printf(TEXT("%s should prepare successfully before the runtime error path"), *ContextLabelString),
		PrepareResult,
		static_cast<int32>(asSUCCESS));
	const bool bThrew = Test.TestEqual(
		*FString::Printf(TEXT("%s should raise a script execution exception"), *ContextLabelString),
		ExecuteResult,
		static_cast<int32>(asEXECUTION_EXCEPTION));
	const bool bHasMessage = Test.TestFalse(
		*FString::Printf(TEXT("%s should provide a non-empty exception string"), *ContextLabelString),
		ExceptionString.IsEmpty());
	const bool bHasExpectedMessage = Test.TestTrue(
		*FString::Printf(TEXT("%s should report the expected exception text"), *ContextLabelString),
		ExceptionString.Contains(ExpectedExceptionText));
	const bool bHasLine = Test.TestTrue(
		*FString::Printf(TEXT("%s should report a positive exception line"), *ContextLabelString),
		ExceptionLine > 0);
	Test.AddInfo(FString::Printf(
		TEXT("%s raised script exception at line %d: %s"),
		*ContextLabelString,
		ExceptionLine,
		*ExceptionString));

	return bPrepared && bThrew && bHasMessage && bHasExpectedMessage && bHasLine;
}

inline bool TArrayBindingsExecuteFunctionReturningScriptArray(
	FAutomationTestBase& Test,
	FAngelscriptEngine& Engine,
	asIScriptModule& Module,
	const FArraySyntaxCoverageProfile& Profile,
	const FString& FunctionDecl,
	const FString& ContextLabel,
	TFunctionRef<bool(const FScriptArray&)> ValidateReturnedArray)
{
	const FString ContextLabelString(ContextLabel);
	if (!TArrayBindingsTraceCase(Test, Engine, Module, Profile, ContextLabelString))
	{
		return false;
	}

	asIScriptFunction* Function = ResolveFunctionByDecl(Test, Module, FunctionDecl);
	if (Function == nullptr)
	{
		return false;
	}

	FAngelscriptEngineScope EngineScope(Engine);
	asIScriptContext* ScriptContext = Engine.CreateContext();
	if (!Test.TestNotNull(*FString::Printf(TEXT("%s should create an execution context"), *ContextLabelString), ScriptContext))
	{
		return false;
	}

	ON_SCOPE_EXIT
	{
		ScriptContext->Release();
	};

	const int PrepareResult = ScriptContext->Prepare(Function);
	const int ExecuteResult = PrepareResult == asSUCCESS ? ScriptContext->Execute() : PrepareResult;
	const bool bPrepared = Test.TestEqual(
		*FString::Printf(TEXT("%s should prepare successfully"), *ContextLabelString),
		PrepareResult,
		static_cast<int32>(asSUCCESS));
	const bool bExecuted = Test.TestEqual(
		*FString::Printf(TEXT("%s should execute successfully"), *ContextLabelString),
		ExecuteResult,
		static_cast<int32>(asEXECUTION_FINISHED));
	if (!bPrepared || !bExecuted)
	{
		if (ScriptContext->GetExceptionString() != nullptr)
		{
			Test.AddError(FString::Printf(
				TEXT("%s failed while returning array: %s"),
				*ContextLabelString,
				UTF8_TO_TCHAR(ScriptContext->GetExceptionString())));
		}
		return false;
	}

	const FScriptArray* ReturnedArray = static_cast<const FScriptArray*>(ScriptContext->GetReturnObject());
	const bool bHasArray = Test.TestNotNull(
		*FString::Printf(TEXT("%s should expose a returned FScriptArray object"), *ContextLabelString),
		ReturnedArray);
	if (!bHasArray)
	{
		return false;
	}

	Test.AddInfo(FString::Printf(TEXT("%s returned array with Num=%d"), *ContextLabelString, ReturnedArray->Num()));
	return ValidateReturnedArray(*ReturnedArray);
}

inline bool TArrayBindingsCompileSummaryContainsDiagnosticMessage(
	const FAngelscriptCompileTraceSummary& Summary,
	const FString& ExpectedMessage)
{
	for (const FAngelscriptCompileTraceDiagnosticSummary& Diagnostic : Summary.Diagnostics)
	{
		if (Diagnostic.Message.Contains(ExpectedMessage))
		{
			return true;
		}
	}
	return false;
}

inline void TArrayBindingsReportCompileSummaryDiagnostics(
	FAutomationTestBase& Test,
	const TCHAR* ContextLabel,
	const FAngelscriptCompileTraceSummary& Summary)
{
	Test.AddInfo(FString::Printf(
		TEXT("%s compile result=%d diagnostics=%d"),
		ContextLabel,
		static_cast<int32>(Summary.CompileResult),
		Summary.Diagnostics.Num()));

	for (const FAngelscriptCompileTraceDiagnosticSummary& Diagnostic : Summary.Diagnostics)
	{
		Test.AddInfo(FString::Printf(
			TEXT("%s diagnostic %s:%d:%d %s"),
			ContextLabel,
			*Diagnostic.Section,
			Diagnostic.Row,
			Diagnostic.Column,
			*Diagnostic.Message));
	}
}

inline bool TArraySyntaxCompatExpectCompileFailure(
	FAutomationTestBase& Test,
	FAngelscriptEngine& Engine,
	const FArraySyntaxCoverageProfile& Profile,
	const TCHAR* SectionName,
	const FString& Source,
	const TCHAR* ContextLabel)
{
	const FString ModuleName = TArrayBindingsMakeModuleName(Profile, SectionName);
	const FString SourceFilename = FString::Printf(TEXT("%s.as"), *ModuleName);

	FAngelscriptCompileTraceSummary Summary;
	const bool bCompiled = CompileModuleWithSummary(
		&Engine,
		ECompileType::SoftReloadOnly,
		FName(*ModuleName),
		SourceFilename,
		Source,
		false,
		Summary,
		true);

	TArrayBindingsReportCompileSummaryDiagnostics(Test, ContextLabel, Summary);

	bool bPassed = true;
	bPassed &= Test.TestFalse(
		*FString::Printf(TEXT("%s should fail compilation"), ContextLabel),
		bCompiled);
	bPassed &= Test.TestEqual(
		*FString::Printf(TEXT("%s should be reported as a compile error"), ContextLabel),
		Summary.CompileResult,
		ECompileResult::Error);
	return bPassed;
}

#endif // WITH_DEV_AUTOMATION_TESTS
