#include "CQTest.h"
#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestExecute.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestUtilities.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace
{
	bool CompileSummaryHasErrorContaining(
		const FAngelscriptCompileTraceSummary& Summary,
		const FString& ExpectedFragment)
	{
		return Summary.Diagnostics.ContainsByPredicate([&ExpectedFragment](const FAngelscriptCompileTraceDiagnosticSummary& Diagnostic)
		{
			return Diagnostic.bIsError && Diagnostic.Message.Contains(ExpectedFragment);
		});
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptTArrayBindingsTest,
	"Angelscript.TestModule.Bindings.Container.TArray",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

	TEST_METHOD(TArrayContractSmoke)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule ModuleScope(*TestRunner, Engine, TEXT("ASTArray_ContractSmoke"), ASTEST_AS(R"AS(
			int VerifyTArrayContractSmoke()
			{
				TArray<int> Values;
				Values.Add(1);
				Values.Add(2);
				Values.Insert(9, 1);
				if (Values.Num() != 3 || Values[1] != 9)
				{
					return 0;
				}

				int Sum = 0;
				foreach (int Value : Values)
				{
					Sum += Value;
				}

				Values.RemoveSingle(9);
				return Sum == 12 && Values.Num() == 2 && Values.Contains(2) ? 1 : 0;
			}
			)AS"));
		if (!ModuleScope.IsValid()) return;

		ASSERT_THAT(IsTrue(ExpectGlobalInt(
			*TestRunner,
			Engine,
			ModuleScope.GetModule(),
			TEXT("int VerifyTArrayContractSmoke()"),
			TEXT("TArray construction, indexing, iteration, and mutation bindings should dispatch"),
			1)));
	}

	TEST_METHOD(TArrayNestedContainerRejection)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString ScriptSource = ASTEST_AS(R"AS(
			int NestedArrayBoundary()
			{
				TArray<TArray<int>> Nested;
				return Nested.Num();
			}
			)AS");

		FAngelscriptCompileTraceSummary Summary;
		const bool bCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::SoftReloadOnly,
			TEXT("ASTArray_NestedContainerRejection"),
			TEXT("ASTArray_NestedContainerRejection.as"),
			ScriptSource,
			false,
			Summary,
			true);

		TArray<FString> ExpectedDiagnostics;
		ExpectedDiagnostics.Add(TEXT("Containers cannot be nested"));

		ASSERT_THAT(IsFalse(bCompiled, TEXT("Nested TArray<TArray<int>> should remain unsupported")));
		ASSERT_THAT(AreEqual(ECompileResult::Error, Summary.CompileResult, TEXT("Nested TArray rejection should be a compile error")));
		ASSERT_THAT(IsTrue(CompileSummaryHasErrorContaining(Summary, ExpectedDiagnostics[0]),
			TEXT("Nested TArray rejection should report the nested container diagnostic")));
	}
};

#endif
