// ============================================================================
// AngelscriptSetBindingsAdvancedTests.cpp
//
// TSet advanced binding contract smoke. Broad TSet semantics live in Coverage
// (`03-containers`); this file only proves the advanced entrypoints are exposed.
// ============================================================================

#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestExecute.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptSetAdvancedBindingsTest,
	"Angelscript.TestModule.Bindings.SetAdvanced",
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

	TEST_METHOD(TSetAdvancedContractSmoke)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule ModuleScope(*TestRunner, Engine, TEXT("ASSetAdvanced_ContractSmoke"), ASTEST_AS(R"AS(
			int VerifyTSetAdvancedContractSmoke()
			{
				TArray<int> SourceArray;
				SourceArray.Add(4);
				SourceArray.Add(7);
				SourceArray.Add(7);

				TSet<int> Values;
				Values.Append(SourceArray);

				TSet<int> Extra;
				Extra.Add(1);
				Extra.Add(4);
				Values.Append(Extra);

				TSet<int> Copy = Values;
				Copy.Remove(4);

				TSet<int> Assigned;
				Assigned.Add(99);
				Assigned = Copy;
				Assigned.Empty(8);

				return Values.Num() == 3
					&& Values.Contains(1)
					&& Values.Contains(4)
					&& Values.Contains(7)
					&& Copy.Num() == 2
					&& !Copy.Contains(4)
					&& Assigned.IsEmpty() ? 1 : 0;
			}
			)AS"));
		if (!ModuleScope.IsValid()) return;

		ASSERT_THAT(IsTrue(ExpectGlobalInt(
			*TestRunner,
			Engine,
			ModuleScope.GetModule(),
			TEXT("int VerifyTSetAdvancedContractSmoke()"),
			TEXT("TSet Append, copy, assignment, Remove, and Empty bindings should dispatch"),
			1)));
	}
};

#endif
