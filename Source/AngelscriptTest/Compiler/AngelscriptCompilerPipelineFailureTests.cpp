#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/Class.h"
#include "UObject/UObjectGlobals.h"

#if WITH_DEV_AUTOMATION_TESTS


namespace CompilerPipelineFailureTest
{
	static const FName EmptyModuleName(TEXT("CompilerEmptySourceFailure"));
	static const FString EmptyScriptFilename(TEXT("CompilerEmptySourceFailure.as"));
	static const FName RecoveryModuleName(TEXT("CompilerEmptySourceRecovery"));
	static const FString RecoveryScriptFilename(TEXT("CompilerEmptySourceRecovery.as"));
	static const TCHAR* EmptySourceDiagnostic(TEXT("Script file contains no code to compile."));
	static const FName SyntaxFailureModuleName(TEXT("CompilerSyntaxErrorFailure"));
	static const FString SyntaxFailureScriptFilename(TEXT("CompilerSyntaxErrorFailure.as"));
	static const FName SyntaxFailureClassName(TEXT("UBrokenCarrier"));
	static const FName SyntaxFailureFunctionName(TEXT("GetValue"));
	static const int32 SyntaxFailureLine = 8;
	static const TCHAR* SyntaxDiagnosticFragment(TEXT("Expected ';'"));
	static const TCHAR* SyntaxDiagnosticFallbackFragment(TEXT("Instead found '}'"));

	static bool HasErrorDiagnostic(const TArray<FAngelscriptCompileTraceDiagnosticSummary>& Diagnostics)
	{
		for (const FAngelscriptCompileTraceDiagnosticSummary& Diagnostic : Diagnostics)
		{
			if (Diagnostic.bIsError && !Diagnostic.Message.IsEmpty())
			{
				return true;
			}
		}

		return false;
	}

	static const FAngelscriptCompileTraceDiagnosticSummary* FindFirstErrorDiagnostic(const TArray<FAngelscriptCompileTraceDiagnosticSummary>& Diagnostics)
	{
		for (const FAngelscriptCompileTraceDiagnosticSummary& Diagnostic : Diagnostics)
		{
			if (Diagnostic.bIsError)
			{
				return &Diagnostic;
			}
		}

		return nullptr;
	}

	static bool IsHandledCompileResult(const ECompileResult CompileResult)
	{
		return CompileResult == ECompileResult::FullyHandled
			|| CompileResult == ECompileResult::PartiallyHandled;
	}
}

TEST_CLASS_WITH_FLAGS(FCompilerEndToEndFailureTests,
	"Angelscript.TestModule.Compiler.EndToEnd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(EmptySourceFailsWithoutStateLeak)
	{
		using namespace CompilerPipelineFailureTest;

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*CompilerPipelineFailureTest::EmptyModuleName.ToString());
			Engine.DiscardModule(*CompilerPipelineFailureTest::RecoveryModuleName.ToString());
		};

		TestRunner->AddExpectedError(CompilerPipelineFailureTest::EmptySourceDiagnostic, EAutomationExpectedErrorFlags::Contains, 1);

		FAngelscriptCompileTraceSummary EmptySummary;
		const bool bEmptyCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::SoftReloadOnly,
			CompilerPipelineFailureTest::EmptyModuleName,
			CompilerPipelineFailureTest::EmptyScriptFilename,
			FString(),
			true,
			EmptySummary,
			true);

		TestRunner->TestFalse(
			TEXT("Empty source should fail instead of compiling successfully"),
			bEmptyCompiled);
		TestRunner->TestTrue(
			TEXT("Empty source failure should still report that the preprocessor-enabled pipeline ran"),
			EmptySummary.bUsedPreprocessor);
		TestRunner->TestEqual(
			TEXT("Empty source failure should surface a compile error result"),
			EmptySummary.CompileResult,
			ECompileResult::Error);
		TestRunner->TestEqual(
			TEXT("Empty source failure should not leave any compiled modules behind"),
			EmptySummary.CompiledModuleCount,
			0);
		TestRunner->TestTrue(
			TEXT("Empty source failure should capture at least one error diagnostic"),
			CompilerPipelineFailureTest::HasErrorDiagnostic(EmptySummary.Diagnostics));

		FAngelscriptCompileTraceSummary RecoverySummary;
		const FString RecoveryScript = TEXT(R"AS(
	int Entry()
	{
		return 42;
	}
	)AS");
		const bool bRecoveryCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::SoftReloadOnly,
			CompilerPipelineFailureTest::RecoveryModuleName,
			CompilerPipelineFailureTest::RecoveryScriptFilename,
			RecoveryScript,
			true,
			RecoverySummary);

		TestRunner->TestTrue(
			TEXT("A valid script compiled after the empty-source failure should succeed on the same engine"),
			bRecoveryCompiled);
		TestRunner->TestEqual(
			TEXT("The recovery compile should report a fully handled result"),
			RecoverySummary.CompileResult,
			ECompileResult::FullyHandled);
		TestRunner->TestEqual(
			TEXT("The recovery compile should produce exactly one compiled module"),
			RecoverySummary.CompiledModuleCount,
			1);
		TestRunner->TestEqual(
			TEXT("The recovery compile should not inherit stale diagnostics from the failed compile"),
			RecoverySummary.Diagnostics.Num(),
			0);
		if (!bRecoveryCompiled)
		{
			return;
		}

		int32 EntryResult = 0;
		const bool bExecuted = ExecuteIntFunction(
			&Engine,
			CompilerPipelineFailureTest::RecoveryScriptFilename,
			CompilerPipelineFailureTest::RecoveryModuleName,
			TEXT("int Entry()"),
			EntryResult);
		TestRunner->TestTrue(
			TEXT("The recovery compile should still execute the minimal Entry function"),
			bExecuted);
		if (bExecuted)
		{
			TestRunner->TestEqual(
				TEXT("The recovery compile should return the expected runtime value"),
				EntryResult,
				42);
		}

		}
	}

	TEST_METHOD(SyntaxErrorFailsWithoutResidualReflection)
	{
		using namespace CompilerPipelineFailureTest;

		const FString InitialScript = TEXT(R"AS(
	UCLASS()
	class UBrokenCarrier : UObject
	{
		UFUNCTION()
		int GetValue()
		{
			return 7;
		}
	}
	)AS");
		const FString BrokenScript = TEXT(R"AS(
	UCLASS()
	class UBrokenCarrier : UObject
	{
		UFUNCTION()
		int GetValue()
		{
			return 8
		}
	}
	)AS");
		const FString FixedScript = TEXT(R"AS(
	UCLASS()
	class UBrokenCarrier : UObject
	{
		UFUNCTION()
		int GetValue()
		{
			return 9;
		}
	}
	)AS");

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*CompilerPipelineFailureTest::SyntaxFailureModuleName.ToString());
		};

		const bool bInitialCompiled = CompileAnnotatedModuleFromMemory(
			&Engine,
			CompilerPipelineFailureTest::SyntaxFailureModuleName,
			CompilerPipelineFailureTest::SyntaxFailureScriptFilename,
			InitialScript);
		if (!TestRunner->TestTrue(TEXT("Syntax-error recovery test should compile the initial annotated module"), bInitialCompiled))
		{
			return;
		}

		UClass* InitialClass = FindGeneratedClass(&Engine, CompilerPipelineFailureTest::SyntaxFailureClassName);
		if (!TestRunner->TestNotNull(TEXT("Syntax-error recovery test should materialize the initial generated class"), InitialClass))
		{
			return;
		}

		UFunction* InitialFunction = FindGeneratedFunction(InitialClass, CompilerPipelineFailureTest::SyntaxFailureFunctionName);
		if (!TestRunner->TestNotNull(TEXT("Syntax-error recovery test should find the initial generated function"), InitialFunction))
		{
			return;
		}

		UObject* InitialObject = NewObject<UObject>(GetTransientPackage(), InitialClass);
		if (!TestRunner->TestNotNull(TEXT("Syntax-error recovery test should instantiate the initial generated class"), InitialObject))
		{
			return;
		}

		int32 InitialResult = 0;
		const bool bInitialExecuted = ExecuteGeneratedIntEventOnGameThread(&Engine, InitialObject, InitialFunction, InitialResult);
		TestRunner->TestTrue(
			TEXT("Syntax-error recovery test should execute the initial generated function"),
			bInitialExecuted);
		if (bInitialExecuted)
		{
			TestRunner->TestEqual(
				TEXT("Syntax-error recovery test should observe the initial runtime result before failure"),
				InitialResult,
				7);
		}

		FAngelscriptCompileTraceSummary BrokenSummary;
		const bool bBrokenCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::SoftReloadOnly,
			CompilerPipelineFailureTest::SyntaxFailureModuleName,
			CompilerPipelineFailureTest::SyntaxFailureScriptFilename,
			BrokenScript,
			true,
			BrokenSummary,
			true);
		TestRunner->TestFalse(
			TEXT("Syntax-error recovery test should fail the broken recompile"),
			bBrokenCompiled);
		TestRunner->TestTrue(
			TEXT("Syntax-error recovery test should report that the broken recompile used the preprocessor-enabled pipeline"),
			BrokenSummary.bUsedPreprocessor);
		TestRunner->TestEqual(
			TEXT("Syntax-error recovery test should surface a compile error result for the broken recompile"),
			BrokenSummary.CompileResult,
			ECompileResult::Error);

		const FAngelscriptCompileTraceDiagnosticSummary* BrokenDiagnostic =
			CompilerPipelineFailureTest::FindFirstErrorDiagnostic(BrokenSummary.Diagnostics);
		const bool bHasBrokenDiagnostic = TestRunner->TestNotNull(
			TEXT("Syntax-error recovery test should capture an error diagnostic for the broken recompile"),
			BrokenDiagnostic);
		if (bHasBrokenDiagnostic)
		{
			TestRunner->TestEqual(
				TEXT("Syntax-error recovery test should point the diagnostic at the missing semicolon line"),
				BrokenDiagnostic->Row,
				CompilerPipelineFailureTest::SyntaxFailureLine);
			TestRunner->TestTrue(
				TEXT("Syntax-error recovery test should emit a non-empty diagnostic message for the broken recompile"),
				!BrokenDiagnostic->Message.IsEmpty());
			TestRunner->TestTrue(
				TEXT("Syntax-error recovery test should keep a syntax-oriented diagnostic message"),
				BrokenDiagnostic->Message.Contains(CompilerPipelineFailureTest::SyntaxDiagnosticFragment)
					|| BrokenDiagnostic->Message.Contains(CompilerPipelineFailureTest::SyntaxDiagnosticFallbackFragment));
		}

		UClass* ClassAfterFailure = FindGeneratedClass(&Engine, CompilerPipelineFailureTest::SyntaxFailureClassName);
		TestRunner->TestTrue(
			TEXT("Syntax-error recovery test should keep the previously generated class active after the broken recompile"),
			ClassAfterFailure == InitialClass);

		UFunction* FunctionAfterFailure = FindGeneratedFunction(ClassAfterFailure, CompilerPipelineFailureTest::SyntaxFailureFunctionName);
		TestRunner->TestTrue(
			TEXT("Syntax-error recovery test should keep the previously generated function active after the broken recompile"),
			FunctionAfterFailure == InitialFunction);

		if (ClassAfterFailure != nullptr && FunctionAfterFailure != nullptr)
		{
			UObject* ObjectAfterFailure = NewObject<UObject>(GetTransientPackage(), ClassAfterFailure);
			int32 ResultAfterFailure = 0;
			const bool bExecutedAfterFailure = ObjectAfterFailure != nullptr
				&& ExecuteGeneratedIntEventOnGameThread(&Engine, ObjectAfterFailure, FunctionAfterFailure, ResultAfterFailure);
			TestRunner->TestTrue(
				TEXT("Syntax-error recovery test should still execute the last good generated function after the broken recompile"),
				bExecutedAfterFailure);
			if (bExecutedAfterFailure)
			{
				TestRunner->TestEqual(
					TEXT("Syntax-error recovery test should keep the last good runtime result active after the broken recompile"),
					ResultAfterFailure,
					7);
			}
		}

		FAngelscriptCompileTraceSummary FixedSummary;
		const bool bFixedCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::SoftReloadOnly,
			CompilerPipelineFailureTest::SyntaxFailureModuleName,
			CompilerPipelineFailureTest::SyntaxFailureScriptFilename,
			FixedScript,
			true,
			FixedSummary);
		TestRunner->TestTrue(
			TEXT("Syntax-error recovery test should successfully compile the fixed script after the broken recompile"),
			bFixedCompiled);
		TestRunner->TestTrue(
			TEXT("Syntax-error recovery test should report a handled compile result for the fixed recompile"),
			CompilerPipelineFailureTest::IsHandledCompileResult(FixedSummary.CompileResult));
		TestRunner->TestEqual(
			TEXT("Syntax-error recovery test should produce exactly one compiled module for the fixed recompile"),
			FixedSummary.CompiledModuleCount,
			1);
		TestRunner->TestEqual(
			TEXT("Syntax-error recovery test should clear broken diagnostics once the script is fixed"),
			FixedSummary.Diagnostics.Num(),
			0);
		if (!bFixedCompiled)
		{
			return;
		}

		UClass* FixedClass = FindGeneratedClass(&Engine, CompilerPipelineFailureTest::SyntaxFailureClassName);
		if (!TestRunner->TestNotNull(TEXT("Syntax-error recovery test should keep a generated class available after the fixed recompile"), FixedClass))
		{
			return;
		}

		UFunction* FixedFunction = FindGeneratedFunction(FixedClass, CompilerPipelineFailureTest::SyntaxFailureFunctionName);
		if (!TestRunner->TestNotNull(TEXT("Syntax-error recovery test should expose the fixed generated function"), FixedFunction))
		{
			return;
		}

		UObject* FixedObject = NewObject<UObject>(GetTransientPackage(), FixedClass);
		if (!TestRunner->TestNotNull(TEXT("Syntax-error recovery test should instantiate the fixed generated class"), FixedObject))
		{
			return;
		}

		int32 FixedResult = 0;
		const bool bFixedExecuted = ExecuteGeneratedIntEventOnGameThread(&Engine, FixedObject, FixedFunction, FixedResult);
		TestRunner->TestTrue(
			TEXT("Syntax-error recovery test should execute the fixed generated function after the broken recompile"),
			bFixedExecuted);
		if (bFixedExecuted)
		{
			TestRunner->TestEqual(
				TEXT("Syntax-error recovery test should observe the updated runtime result after the fixed recompile"),
				FixedResult,
				9);
		}

		}
	}
};

#endif
