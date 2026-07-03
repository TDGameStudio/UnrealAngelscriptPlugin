// ============================================================================
// AngelscriptMapBindingsTests.cpp
//
// TMap binding contract smoke. Broad TMap semantics live in Coverage
// (`03-containers`).
// ============================================================================

#include "CQTest.h"
#include "AngelscriptTestExecute.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptMapBindingsTest,
	"Angelscript.TestModule.Bindings.Container.Map",
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

	TEST_METHOD(TMapContractSmoke)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule ModuleScope(*TestRunner, Engine, TEXT("ASMap_ContractSmoke"), ASTEST_AS(R"AS(
			int VerifyTMapContractSmoke()
			{
				TMap<FName, int> Values;
				Values.Add(FName("Alpha"), 4);
				Values.Add(FName("Alpha"), 7);

				int Existing = 0;
				if (!Values.Find(FName("Alpha"), Existing) || Existing != 7)
				{
					return 0;
				}

				int MissingSentinel = 99;
				if (Values.Find(FName("Missing"), MissingSentinel) || MissingSentinel != 99)
				{
					return 0;
				}

				int& Gamma = Values.FindOrAdd(FName("Gamma"));
				Gamma = 33;
				int& Delta = Values.FindOrAdd(FName("Delta"), 11);
				Delta += 1;

				int GammaValue = 0;
				int DeltaValue = 0;
				return Values.Num() == 3
					&& Values.Find(FName("Gamma"), GammaValue)
					&& Values.Find(FName("Delta"), DeltaValue)
					&& GammaValue == 33
					&& DeltaValue == 12 ? 1 : 0;
			}
			)AS"));
		if (!ModuleScope.IsValid()) return;

		ASSERT_THAT(IsTrue(ExpectGlobalInt(
			*TestRunner,
			Engine,
			ModuleScope.GetModule(),
			TEXT("int VerifyTMapContractSmoke()"),
			TEXT("TMap Add/Find/FindOrAdd/ref-return bindings should dispatch"),
			1)));
	}
};

#endif
