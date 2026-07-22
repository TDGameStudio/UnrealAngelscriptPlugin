#include "Support/AngelscriptNativeExecutionTestSupport.h"

// Raw SDK default-trait coverage.

#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS


TEST_CLASS_WITH_FLAGS(FDefaultTraitTests,
	"Angelscript.TestModule.AngelScriptSDK.Compiler",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	inline static AngelscriptNativeTestSupport::FNativeTestEngine Engine;

	BEFORE_ALL()
	{
		Engine.Create(*TestRunner);
	}

	AFTER_ALL()
	{
		Engine.Destroy();
	}

	BEFORE_EACH()
	{
		Engine.ResetMessages();
	}

	TEST_METHOD(DefaultTraitModifiers)
	{
		asIScriptEngine* ScriptEngine = Engine.Get();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("SDK default-trait modifier test should create a standalone engine")));

		AngelscriptNativeTestSupport::FScopedNativeModule Module(*TestRunner, Engine, "SDKDefaultTraitModifiers", R"(
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
