#include "AngelscriptTestAdapter.h"

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptNativeTestSupport;

TEST_CLASS_WITH_FLAGS(FAngelscriptSDKDefaultTraitTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	inline static FNativeSdkEngineFixture EngineFixture;

	BEFORE_ALL()
	{
		EngineFixture.Create(*TestRunner);
	}

	AFTER_ALL()
	{
		EngineFixture.Destroy();
	}

	BEFORE_EACH()
	{
		EngineFixture.ResetMessages();
	}

	TEST_METHOD(DefaultTraitModifiers)
	{
		asIScriptEngine* ScriptEngine = EngineFixture.Get();
		if (!TestRunner->TestNotNull(TEXT("SDK default-trait modifier test should create a standalone engine"), ScriptEngine))
		{
			return;
		}

		FScopedNativeModule Module(*TestRunner, EngineFixture, "SDKDefaultTraitModifiers", R"(
int DefaultsOnlyValue() defaults
{
	return 7;
}

int UnsafeConstructionValue() unsafe_during_construction
{
	return 5;
}

int Entry()
{
	return 1;
}
)");
		if (!Module.IsValid())
		{
			return;
		}
	}
};

#endif
