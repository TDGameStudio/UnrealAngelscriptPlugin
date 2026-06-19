#include "CQTest.h"
#include "AngelscriptBindingsAssertions.h"
#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"

#if WITH_DEV_AUTOMATION_TESTS


TEST_CLASS_WITH_FLAGS(
	FAngelscriptTypeConversionTests,
	"Angelscript.TestModule.Functional.Types.Conversion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	TEST_METHOD(NegativeTruncateTowardZero)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		asIScriptEngine* ScriptEngine = Engine.GetScriptEngine();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Types.Conversion.NegativeTruncateTowardZero should expose a script engine")));

		const bool bFloatUsesFloat64 = ScriptEngine->GetEngineProperty(asEP_FLOAT_IS_FLOAT64) != 0;
		const FString Script = bFloatUsesFloat64
			? TEXT("int Run() { double Negative = -3.7; double Positive = 3.7; return int(Negative) * 10 + int(Positive); }")
			: TEXT("int Run() { float Negative = -3.7f; float Positive = 3.7f; return int(Negative) * 10 + int(Positive); }");

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASTypeConversionNegativeTruncateTowardZero"), Script);
		ASSERT_THAT(IsTrue(Module.IsValid()));

		ExpectGlobalInt(
			*TestRunner,
			Engine,
			Module.GetModule(),
			TEXT("int Run()"),
			TEXT("Explicit numeric conversion should truncate both negative and positive floating-point values toward zero"),
			-27);
	}
};

#endif
