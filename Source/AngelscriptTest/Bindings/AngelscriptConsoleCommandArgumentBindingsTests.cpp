#include "AngelscriptConsoleBindingsSections.h"

#include "CQTest.h"
#include "Shared/AngelscriptTestEngineHelper.h"
#include "Shared/AngelscriptTestMacros.h"

#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;
using namespace AngelscriptTestBindings;

TEST_CLASS_WITH_FLAGS(FAngelscriptConsoleCommandArgumentBindingsTest,
	"Angelscript.TestModule.Bindings.ConsoleCommandArgumentMarshalling",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	TEST_METHOD(EmptyArgsMarker)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		RunConsoleCommandArgumentEmptySection(*TestRunner, Engine);
	}

	TEST_METHOD(ContentAndOrder)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		RunConsoleCommandArgumentContentSection(*TestRunner, Engine);
	}
};

#endif
