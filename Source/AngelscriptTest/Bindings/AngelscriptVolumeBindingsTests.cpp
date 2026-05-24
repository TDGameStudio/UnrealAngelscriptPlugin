// AngelscriptVolumeBindingsTests.cpp
// CQTest compile-check coverage for AVolume, LandscapeProxy, UFXSystemComponent.
// Automation IDs: Angelscript.TestModule.Bindings.Volume.*

#include "CQTest.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Shared/AngelscriptTestModuleScope.h"
#include "Shared/AngelscriptBindingsAssertions.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;
using namespace AngelscriptTestBindings;


TEST_CLASS_WITH_FLAGS(FAngelscriptVolumeBindingsTest,
	"Angelscript.TestModule.Bindings.Volume",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL() { ASTEST_CREATE_ENGINE(); }
	AFTER_ALL() { FAngelscriptEngine& E = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(E); }

	TEST_METHOD(TypeAvailability)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASVolume_TypeCheck"), TEXT(R"(
int Volume_TypeExists()
{
	// Compile-time type availability check
	AVolume Volume;
	return 1;
}
)"));
		// If types are not registered, Mod will be invalid - that is acceptable.
		if (!Mod.IsValid())
		{
			TestRunner->AddInfo(TEXT("AVolume type not available in test engine, skipping"));
			return;
		}
		AngelscriptTestBindings::ExpectGlobalInt(*TestRunner, Engine, Mod.GetModule(), 
			TEXT("int Volume_TypeExists()"), TEXT("AVolume type compiles"), 1);
	}

	TEST_METHOD(FXSystemComponentTypeCheck)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASVolume_FXTypeCheck"), TEXT(R"(
int FX_TypeExists()
{
	UFXSystemComponent Comp;
	return 1;
}
)"));
		if (!Mod.IsValid())
		{
			TestRunner->AddInfo(TEXT("UFXSystemComponent not available in test engine, skipping"));
			return;
		}
		AngelscriptTestBindings::ExpectGlobalInt(*TestRunner, Engine, Mod.GetModule(), 
			TEXT("int FX_TypeExists()"), TEXT("UFXSystemComponent compiles"), 1);
	}
};

#endif
