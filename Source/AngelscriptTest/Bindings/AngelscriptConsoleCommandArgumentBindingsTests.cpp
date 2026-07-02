#include "Bindings/AngelscriptConsoleBindingsSections.h"

#include "CQTest.h"
#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"

#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS


TEST_CLASS_WITH_FLAGS(FAngelscriptConsoleCommandArgumentBindingsTest,
	"Angelscript.TestModule.Bindings.ConsoleCommandArgumentMarshalling",
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

	TEST_METHOD(EmptyArgsMarker)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		ASSERT_THAT(IsTrue(
			VerifyConsoleCommandArgumentEmpty(*TestRunner, Engine),
			TEXT("VerifyConsoleCommandArgumentEmpty should pass")));
	}

	TEST_METHOD(ContentAndOrder)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		ASSERT_THAT(IsTrue(
			VerifyConsoleCommandArgumentContent(*TestRunner, Engine),
			TEXT("VerifyConsoleCommandArgumentContent should pass")));
	}
};

#endif
