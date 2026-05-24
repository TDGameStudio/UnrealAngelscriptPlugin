#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestLegacyHelpers.h"

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


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptAutoInferenceByOverloadTest,
	"Angelscript.TestModule.Functional.Types.Auto.InferenceByOverload",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptAutoInferenceByOverloadTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Angelscript_AngelscriptAutoTypeTests_Private;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	asIScriptEngine* ScriptEngine = Engine.GetScriptEngine();
	if (!TestNotNull(TEXT("Types.Auto.InferenceByOverload should expose a script engine"), ScriptEngine))
	{
		return false;
	}

	const bool bFloatUsesFloat64 = ScriptEngine->GetEngineProperty(asEP_FLOAT_IS_FLOAT64) != 0;
	const FString Script = BuildAutoInferenceByOverloadScript(bFloatUsesFloat64);

	int32 Result = 0;
	ASTEST_COMPILE_RUN_INT(
		Engine,
		"ASTypeAutoInferenceByOverload",
		Script,
		TEXT("int Run()"),
		Result);

	TestEqual(
		TEXT("Auto inference should pick int, float-or-double, and bool overloads according to the inferred type"),
		Result,
		123);

	}
	return true;
}

#endif
