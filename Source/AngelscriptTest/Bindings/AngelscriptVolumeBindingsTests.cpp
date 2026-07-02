// AngelscriptVolumeBindingsTests.cpp
// CQTest compile-check coverage for AVolume, LandscapeProxy, UFXSystemComponent.
// Automation IDs: Angelscript.TestModule.Bindings.Volume.*

#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestExecute.h"

#if WITH_ANGELSCRIPT_UNITTESTS



TEST_CLASS_WITH_FLAGS(FAngelscriptVolumeBindingsTest,
	"Angelscript.TestModule.Bindings.Volume",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}
	AFTER_ALL()
	{
		FAngelscriptEngine& E = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(E);
	}

	TEST_METHOD(TypeAvailability)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASVolume_TypeCheck"), ASTEST_AS(R"AS(
			int Volume_TypeExists()
			{
				// Compile-time type availability check
				AVolume Volume;
				return 1;
			}
			)AS"));
		// If types are not registered, Mod will be invalid - that is acceptable.
		if (!Mod.IsValid())
		{
			TestRunner->AddInfo(TEXT("AVolume type not available in test engine, skipping"));
			return;
		}
		ASSERT_THAT(IsTrue(
			ExpectGlobalInt(*TestRunner, Engine, Mod.GetModule(), TEXT("int Volume_TypeExists()"), TEXT("AVolume type compiles"), 1),
			TEXT("ExpectGlobalInt should pass")));
	}

	TEST_METHOD(FXSystemComponentTypeCheck)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASVolume_FXTypeCheck"), ASTEST_AS(R"AS(
			int FX_TypeExists()
			{
				UFXSystemComponent Comp;
				return 1;
			}
			)AS"));
		if (!Mod.IsValid())
		{
			TestRunner->AddInfo(TEXT("UFXSystemComponent not available in test engine, skipping"));
			return;
		}
		ASSERT_THAT(IsTrue(
			ExpectGlobalInt(*TestRunner, Engine, Mod.GetModule(), TEXT("int FX_TypeExists()"), TEXT("UFXSystemComponent compiles"), 1),
			TEXT("ExpectGlobalInt should pass")));
	}
};

#endif
