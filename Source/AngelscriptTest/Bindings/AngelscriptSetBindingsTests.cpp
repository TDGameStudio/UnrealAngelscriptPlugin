// ============================================================================
// AngelscriptSetBindingsTests.cpp
//
// TSet binding contract smoke. Broad TSet semantics live in Coverage
// (`03-containers`).
// ============================================================================

#include "CQTest.h"
#include "AngelscriptTestExecute.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptSetBindingsTest,
	"Angelscript.TestModule.Bindings.Container.Set",
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

	TEST_METHOD(TSetContractSmoke)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule ModuleScope(*TestRunner, Engine, TEXT("ASSet_ContractSmoke"), ASTEST_AS(R"AS(
			int VerifyTSetContractSmoke()
			{
				TSet<int> Values;
				Values.Add(4);
				Values.Add(4);
				Values.Add(7);
				if (Values.Num() != 2 || !Values.Contains(4) || !Values.Contains(7))
				{
					return 0;
				}

				TArray<int> MoreValues;
				MoreValues.Add(1);
				MoreValues.Add(7);
				Values.Append(MoreValues);
				if (Values.Num() != 3 || !Values.Contains(1))
				{
					return 0;
				}

				TSet<int> Copy = Values;
				Copy.Remove(4);
				return Values.Contains(4) && !Copy.Contains(4) && !(Values == Copy) ? 1 : 0;
			}
			)AS"));
		if (!ModuleScope.IsValid()) return;

		ASSERT_THAT(IsTrue(ExpectGlobalInt(
			*TestRunner,
			Engine,
			ModuleScope.GetModule(),
			TEXT("int VerifyTSetContractSmoke()"),
			TEXT("TSet Add/Contains/Append/copy/compare bindings should dispatch"),
			1)));
	}
};

#endif
