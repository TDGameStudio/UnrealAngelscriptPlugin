#include "CQTest.h"
#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestExecute.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestUtilities.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptTArrayBindingsTest,
	"Angelscript.TestModule.Bindings.Container.TArray",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static bool CompileSummaryHasErrorContaining(
		const FAngelscriptCompileTraceSummary& Summary,
		const FString& ExpectedFragment)
	{
		return Summary.Diagnostics.ContainsByPredicate([&ExpectedFragment](const FAngelscriptCompileTraceDiagnosticSummary& Diagnostic)
		{
			return Diagnostic.bIsError && Diagnostic.Message.Contains(ExpectedFragment);
		});
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

	TEST_METHOD(TemplateNativeOperationsAndExplicitIterator)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString ScriptSource = ASTEST_AS(R"AS(
			int VerifyTemplateNativeOperationsAndExplicitIterator()
			{
				TArray<int> Values;
				Values.Add(10);
				Values.Add(20);
				Values.Add(30);

				const bool bAddedDuplicate = Values.AddUnique(20);
				const bool bAddedUnique = Values.AddUnique(40);
				Values.Swap(0, 2);

				const int LastValue = Values.Last();
				const int PreviousValue = Values.Last(1);
				const int FoundIndex = Values.FindIndex(20);

				int Sum = 0;
				{
					TArrayIterator<int> Iterator = Values.Iterator();
					while (Iterator.CanProceed)
					{
						Sum += Iterator.Proceed();
					}
				}

				TArray<int> MovedValues;
				MovedValues.MoveAssignFrom(Values);
				return !bAddedDuplicate
					&& bAddedUnique
					&& LastValue == 40
					&& PreviousValue == 10
					&& FoundIndex == 1
					&& Sum == 100
					&& Values.IsEmpty()
					&& MovedValues.Num() == 4 ? 1 : 0;
			}
			)AS");

		FScopedAngelscriptModule ModuleScope(
			*TestRunner,
			Engine,
			TEXT("ASTArray_TemplateNativeOperationsAndExplicitIterator"),
			ScriptSource);
		if (!ModuleScope.IsValid()) return;

		ASSERT_THAT(IsTrue(ExpectGlobalInt(
			*TestRunner,
			Engine,
			ModuleScope.GetModule(),
			TEXT("int VerifyTemplateNativeOperationsAndExplicitIterator()"),
			TEXT("TArray template-native operations and explicit iterator should preserve script-object injection"),
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
