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

		ASSERT_THAT(IsFalse(
			bEmptyCompiled,
			TEXT("Empty source should fail instead of compiling successfully")));
		ASSERT_THAT(IsTrue(
			EmptySummary.bUsedPreprocessor,
			TEXT("Empty source failure should still report that the preprocessor-enabled pipeline ran")));
		ASSERT_THAT(AreEqual(
			ECompileResult::Error,
			EmptySummary.CompileResult,
			TEXT("Empty source failure should surface a compile error result")));
		ASSERT_THAT(AreEqual(
			0,
			EmptySummary.CompiledModuleCount,
			TEXT("Empty source failure should not leave any compiled modules behind")));
		ASSERT_THAT(IsTrue(
			CompilerPipelineFailureTest::HasErrorDiagnostic(EmptySummary.Diagnostics),
			TEXT("Empty source failure should capture at least one error diagnostic")));

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

		ASSERT_THAT(IsTrue(
			bRecoveryCompiled,
			TEXT("A valid script compiled after the empty-source failure should succeed on the same engine")));
		ASSERT_THAT(AreEqual(
			ECompileResult::FullyHandled,
			RecoverySummary.CompileResult,
			TEXT("The recovery compile should report a fully handled result")));
		ASSERT_THAT(AreEqual(
			1,
			RecoverySummary.CompiledModuleCount,
			TEXT("The recovery compile should produce exactly one compiled module")));
		ASSERT_THAT(AreEqual(
			0,
			RecoverySummary.Diagnostics.Num(),
			TEXT("The recovery compile should not inherit stale diagnostics from the failed compile")));
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
		ASSERT_THAT(IsTrue(
			bExecuted,
			TEXT("The recovery compile should still execute the minimal Entry function")));
		if (bExecuted)
		{
			ASSERT_THAT(AreEqual(
				42,
				EntryResult,
				TEXT("The recovery compile should return the expected runtime value")));
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
		if (!this->Assert.IsTrue(bInitialCompiled, TEXT("Syntax-error recovery test should compile the initial annotated module")))
		{
			return;
		}

		UClass* InitialClass = FindGeneratedClass(&Engine, CompilerPipelineFailureTest::SyntaxFailureClassName);
		if (!this->Assert.IsNotNull(InitialClass, TEXT("Syntax-error recovery test should materialize the initial generated class")))
		{
			return;
		}

		UFunction* InitialFunction = FindGeneratedFunction(InitialClass, CompilerPipelineFailureTest::SyntaxFailureFunctionName);
		if (!this->Assert.IsNotNull(InitialFunction, TEXT("Syntax-error recovery test should find the initial generated function")))
		{
			return;
		}

		UObject* InitialObject = NewObject<UObject>(GetTransientPackage(), InitialClass);
		if (!this->Assert.IsNotNull(InitialObject, TEXT("Syntax-error recovery test should instantiate the initial generated class")))
		{
			return;
		}

		int32 InitialResult = 0;
		const bool bInitialExecuted = ExecuteGeneratedIntEventOnGameThread(&Engine, InitialObject, InitialFunction, InitialResult);
		ASSERT_THAT(IsTrue(
			bInitialExecuted,
			TEXT("Syntax-error recovery test should execute the initial generated function")));
		if (bInitialExecuted)
		{
			ASSERT_THAT(AreEqual(
				7,
				InitialResult,
				TEXT("Syntax-error recovery test should observe the initial runtime result before failure")));
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
		ASSERT_THAT(IsFalse(
			bBrokenCompiled,
			TEXT("Syntax-error recovery test should fail the broken recompile")));
		ASSERT_THAT(IsTrue(
			BrokenSummary.bUsedPreprocessor,
			TEXT("Syntax-error recovery test should report that the broken recompile used the preprocessor-enabled pipeline")));
		ASSERT_THAT(AreEqual(
			ECompileResult::Error,
			BrokenSummary.CompileResult,
			TEXT("Syntax-error recovery test should surface a compile error result for the broken recompile")));

		const FAngelscriptCompileTraceDiagnosticSummary* BrokenDiagnostic =
			CompilerPipelineFailureTest::FindFirstErrorDiagnostic(BrokenSummary.Diagnostics);
		const bool bHasBrokenDiagnostic = this->Assert.IsNotNull(
			BrokenDiagnostic,
			TEXT("Syntax-error recovery test should capture an error diagnostic for the broken recompile"));
		if (bHasBrokenDiagnostic)
		{
			ASSERT_THAT(AreEqual(
				CompilerPipelineFailureTest::SyntaxFailureLine,
				BrokenDiagnostic->Row,
				TEXT("Syntax-error recovery test should point the diagnostic at the missing semicolon line")));
			ASSERT_THAT(IsTrue(
				!BrokenDiagnostic->Message.IsEmpty(),
				TEXT("Syntax-error recovery test should emit a non-empty diagnostic message for the broken recompile")));
			ASSERT_THAT(IsTrue(
				BrokenDiagnostic->Message.Contains(CompilerPipelineFailureTest::SyntaxDiagnosticFragment)
					|| BrokenDiagnostic->Message.Contains(CompilerPipelineFailureTest::SyntaxDiagnosticFallbackFragment),
				TEXT("Syntax-error recovery test should keep a syntax-oriented diagnostic message")));
		}

		UClass* ClassAfterFailure = FindGeneratedClass(&Engine, CompilerPipelineFailureTest::SyntaxFailureClassName);
		ASSERT_THAT(IsTrue(
			ClassAfterFailure == InitialClass,
			TEXT("Syntax-error recovery test should keep the previously generated class active after the broken recompile")));

		UFunction* FunctionAfterFailure = FindGeneratedFunction(ClassAfterFailure, CompilerPipelineFailureTest::SyntaxFailureFunctionName);
		ASSERT_THAT(IsTrue(
			FunctionAfterFailure == InitialFunction,
			TEXT("Syntax-error recovery test should keep the previously generated function active after the broken recompile")));

		if (ClassAfterFailure != nullptr && FunctionAfterFailure != nullptr)
		{
			UObject* ObjectAfterFailure = NewObject<UObject>(GetTransientPackage(), ClassAfterFailure);
			int32 ResultAfterFailure = 0;
			const bool bExecutedAfterFailure = ObjectAfterFailure != nullptr
				&& ExecuteGeneratedIntEventOnGameThread(&Engine, ObjectAfterFailure, FunctionAfterFailure, ResultAfterFailure);
			ASSERT_THAT(IsTrue(
				bExecutedAfterFailure,
				TEXT("Syntax-error recovery test should still execute the last good generated function after the broken recompile")));
			if (bExecutedAfterFailure)
			{
				ASSERT_THAT(AreEqual(
					7,
					ResultAfterFailure,
					TEXT("Syntax-error recovery test should keep the last good runtime result active after the broken recompile")));
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
		ASSERT_THAT(IsTrue(
			bFixedCompiled,
			TEXT("Syntax-error recovery test should successfully compile the fixed script after the broken recompile")));
		ASSERT_THAT(IsTrue(
			CompilerPipelineFailureTest::IsHandledCompileResult(FixedSummary.CompileResult),
			TEXT("Syntax-error recovery test should report a handled compile result for the fixed recompile")));
		ASSERT_THAT(AreEqual(
			1,
			FixedSummary.CompiledModuleCount,
			TEXT("Syntax-error recovery test should produce exactly one compiled module for the fixed recompile")));
		ASSERT_THAT(AreEqual(
			0,
			FixedSummary.Diagnostics.Num(),
			TEXT("Syntax-error recovery test should clear broken diagnostics once the script is fixed")));
		if (!bFixedCompiled)
		{
			return;
		}

		UClass* FixedClass = FindGeneratedClass(&Engine, CompilerPipelineFailureTest::SyntaxFailureClassName);
		if (!this->Assert.IsNotNull(FixedClass, TEXT("Syntax-error recovery test should keep a generated class available after the fixed recompile")))
		{
			return;
		}

		UFunction* FixedFunction = FindGeneratedFunction(FixedClass, CompilerPipelineFailureTest::SyntaxFailureFunctionName);
		if (!this->Assert.IsNotNull(FixedFunction, TEXT("Syntax-error recovery test should expose the fixed generated function")))
		{
			return;
		}

		UObject* FixedObject = NewObject<UObject>(GetTransientPackage(), FixedClass);
		if (!this->Assert.IsNotNull(FixedObject, TEXT("Syntax-error recovery test should instantiate the fixed generated class")))
		{
			return;
		}

		int32 FixedResult = 0;
		const bool bFixedExecuted = ExecuteGeneratedIntEventOnGameThread(&Engine, FixedObject, FixedFunction, FixedResult);
		ASSERT_THAT(IsTrue(
			bFixedExecuted,
			TEXT("Syntax-error recovery test should execute the fixed generated function after the broken recompile")));
		if (bFixedExecuted)
		{
			ASSERT_THAT(AreEqual(
				9,
				FixedResult,
				TEXT("Syntax-error recovery test should observe the updated runtime result after the fixed recompile")));
		}

		}
	}
};

#endif
