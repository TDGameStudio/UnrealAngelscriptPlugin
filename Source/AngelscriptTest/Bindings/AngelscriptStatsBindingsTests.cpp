#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestExecute.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptStatsBindingsTests,
	"Angelscript.TestModule.Bindings.Stats",
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

	TEST_METHOD(StatAndScopeConstructorsRemainScriptVisible)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASStats_Constructors"), ASTEST_AS(R"AS(
			int ExerciseStatsBindings()
			{
				FStatID Stat(n"DirectBindStats");
				FScopeCycleCounter Counter(Stat);
				return 1;
			}
			)AS"));

		ASSERT_THAT(IsTrue(Module.IsValid(), TEXT("Stats value types and constructors should compile from script")));
		if (!Module.IsValid())
		{
			return;
		}
		ASSERT_THAT(IsTrue(
			ExpectGlobalInt(
				*TestRunner,
				Engine,
				Module.GetModule(),
				TEXT("int ExerciseStatsBindings()"),
				TEXT("Stats constructors and destructors should execute successfully"),
				1),
			TEXT("Stats scoped values should execute and tear down successfully")));
	}
};

#endif
