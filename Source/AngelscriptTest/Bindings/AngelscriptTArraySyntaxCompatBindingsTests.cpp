// ============================================================================
// AngelscriptTArraySyntaxCompatBindingsTests.cpp
//
// T[] syntax compatibility contract smoke. Broad TArray semantics live in
// Coverage (`03-containers`).
// ============================================================================

#include "CQTest.h"
#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestExecute.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptTArraySyntaxCompatBindingsTest,
	"Angelscript.TestModule.Bindings.Container.TArraySyntaxCompat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static FAngelscriptCompileTraceSummary CompileScriptExpectingFailure(
		FAngelscriptEngine& Engine,
		FName ModuleName,
		const FString& ScriptSource,
		bool& bOutCompiled)
	{
		FAngelscriptCompileTraceSummary Summary;
		bOutCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::SoftReloadOnly,
			ModuleName,
			FString::Printf(TEXT("%s.as"), *ModuleName.ToString()),
			ScriptSource,
			false,
			Summary,
			true);
		return Summary;
	}

public:
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

	TEST_METHOD(TArraySyntaxContractSmoke)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule ModuleScope(*TestRunner, Engine, TEXT("ASTArraySyntaxCompat_ContractSmoke"), ASTEST_AS(R"AS(
			int VerifyTArraySyntaxContractSmoke()
			{
				int[] Values;
				Values.Add(1);
				Values.Add(2);
				Values.Insert(9, 1);

				int Sum = 0;
				foreach (int Value : Values)
				{
					Sum += Value;
				}

				int[] Copy = Values;
				Copy.RemoveSingle(9);
				return Values.Num() == 3
					&& Values[1] == 9
					&& Sum == 12
					&& Copy.Num() == 2
					&& Copy.Contains(2) ? 1 : 0;
			}
			)AS"));
		if (!ModuleScope.IsValid()) return;

		ASSERT_THAT(IsTrue(ExpectGlobalInt(
			*TestRunner,
			Engine,
			ModuleScope.GetModule(),
			TEXT("int VerifyTArraySyntaxContractSmoke()"),
			TEXT("int[] syntax should route to TArray construction, mutation, foreach, copy, and Contains bindings"),
			1)));
	}

	TEST_METHOD(TArraySyntaxNestedContainerRejection)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString ScriptSource = ASTEST_AS(R"AS(
			int NestedArrayBoundary()
			{
				int[][] Nested;
				return Nested.Num();
			}
			)AS");

		bool bCompiled = false;
		const FAngelscriptCompileTraceSummary Summary = CompileScriptExpectingFailure(
			Engine,
			TEXT("ASTArraySyntaxCompat_NestedContainerRejection"),
			ScriptSource,
			bCompiled);

		ASSERT_THAT(IsFalse(bCompiled, TEXT("int[][] should remain unsupported")));
		ASSERT_THAT(AreEqual(ECompileResult::Error, Summary.CompileResult, TEXT("int[][] rejection should be a compile error")));
	}

	TEST_METHOD(ObjectArraySyntaxBoundary)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString ScriptSource = ASTEST_AS(R"AS(
			int ObjectArrayBoundary()
			{
				UObject[] Objects;
				return Objects.Num();
			}
			)AS");

		bool bCompiled = false;
		const FAngelscriptCompileTraceSummary Summary = CompileScriptExpectingFailure(
			Engine,
			TEXT("ASTArraySyntaxCompat_ObjectArrayBoundary"),
			ScriptSource,
			bCompiled);

		ASSERT_THAT(IsFalse(bCompiled, TEXT("UObject[] shorthand should remain an explicit unsupported boundary")));
		ASSERT_THAT(AreEqual(ECompileResult::Error, Summary.CompileResult, TEXT("UObject[] boundary should be a compile error")));
		ASSERT_THAT(IsTrue(Summary.Diagnostics.Num() > 0, TEXT("UObject[] boundary should emit at least one diagnostic")));
	}
};

#endif
