#include "CQTest.h"
#include "AngelscriptBindingsAssertions.h"
#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"

#if WITH_DEV_AUTOMATION_TESTS


namespace AngelscriptTest_Angelscript_AngelscriptAutoTypeTests_Private
{
	FString BuildAutoInferenceByOverloadScript(const bool bFloatUsesFloat64)
	{
		return bFloatUsesFloat64
			? TEXT(R"AS(
int Pick(int Value) { return 1; }
int Pick(double Value) { return 2; }
int Pick(bool Value) { return 3; }

int Run()
{
	auto I = 42;
	auto F = 3.5;
	auto B = true;
	return Pick(I) * 100 + Pick(F) * 10 + Pick(B);
}
)AS")
			: TEXT(R"AS(
int Pick(int Value) { return 1; }
int Pick(float Value) { return 2; }
int Pick(bool Value) { return 3; }

int Run()
{
	auto I = 42;
	auto F = 3.5f;
	auto B = true;
	return Pick(I) * 100 + Pick(F) * 10 + Pick(B);
}
)AS");
	}
}


TEST_CLASS_WITH_FLAGS(
	FAngelscriptAutoTypeTests,
	"Angelscript.TestModule.Functional.Types.Auto",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	TEST_METHOD(InferenceByOverload)
	{
		using namespace AngelscriptTest_Angelscript_AngelscriptAutoTypeTests_Private;
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptEngine* ScriptEngine = Engine.GetScriptEngine();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Types.Auto.InferenceByOverload should expose a script engine")));

		const bool bFloatUsesFloat64 = ScriptEngine->GetEngineProperty(asEP_FLOAT_IS_FLOAT64) != 0;
		const FString Script = BuildAutoInferenceByOverloadScript(bFloatUsesFloat64);

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASTypeAutoInferenceByOverload"), Script);
		ASSERT_THAT(IsTrue(Module.IsValid()));

		ExpectGlobalInt(
			*TestRunner,
			Engine,
			Module.GetModule(),
			TEXT("int Run()"),
			TEXT("Auto inference should pick int, float-or-double, and bool overloads according to the inferred type"),
			123);
	}
};

#endif
