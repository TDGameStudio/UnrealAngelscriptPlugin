#include "Shared/AngelscriptTestUtilities.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Shared/AngelscriptTestLegacyHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptTypeConversionNegativeTruncateTowardZeroTest,
	"Angelscript.TestModule.Functional.Types.Conversion.NegativeTruncateTowardZero",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptTypeConversionNegativeTruncateTowardZeroTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	asIScriptEngine* ScriptEngine = Engine.GetScriptEngine();
	if (!TestNotNull(TEXT("Types.Conversion.NegativeTruncateTowardZero should expose a script engine"), ScriptEngine))
	{
		return false;
	}

	const bool bFloatUsesFloat64 = ScriptEngine->GetEngineProperty(asEP_FLOAT_IS_FLOAT64) != 0;
	const FString Script = bFloatUsesFloat64
		? TEXT("int Run() { double Negative = -3.7; double Positive = 3.7; return int(Negative) * 10 + int(Positive); }")
		: TEXT("int Run() { float Negative = -3.7f; float Positive = 3.7f; return int(Negative) * 10 + int(Positive); }");

	int32 Result = 0;
	ASTEST_COMPILE_RUN_INT(
		Engine,
		"ASTypeConversionNegativeTruncateTowardZero",
		Script,
		TEXT("int Run()"),
		Result);

	TestEqual(
		TEXT("Explicit numeric conversion should truncate both negative and positive floating-point values toward zero"),
		Result,
		-27);

	}
	return true;
}

#endif
