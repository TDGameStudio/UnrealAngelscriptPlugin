#include "CQTest.h"

#include "AngelscriptTestExecute.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptScopeWorldContextBindingsTest,
	"Angelscript.TestModule.Bindings.ScopeWorldContext",
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

	TEST_METHOD(NullWorldContextScopeConstructsAndRestores)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		FScopedAngelscriptModule Module(
			*TestRunner,
			Engine,
			TEXT("ASScopeWorldContext"),
			ASTEST_AS(R"AS(
				int ConstructNullWorldContextScope()
				{
					FAngelscriptGameThreadScopeWorldContext Scope(nullptr);
					return 1;
				}
				)AS"));

		ASSERT_THAT(IsTrue(Module.IsValid(), TEXT("World-context scope module should compile")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(
			*TestRunner,
			Engine,
			Module.GetModule(),
			TEXT("int ConstructNullWorldContextScope()"),
			TEXT("Null world-context scope should construct and restore the ambient context"),
			1)));
	}
};

#endif
