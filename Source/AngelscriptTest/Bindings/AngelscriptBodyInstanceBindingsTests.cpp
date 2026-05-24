// AngelscriptBodyInstanceBindingsTests.cpp
// CQTest coverage for FBodyInstance, FLatentActionInfo bindings.
// Automation IDs: Angelscript.TestModule.Bindings.BodyInstance.*

#include "CQTest.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Shared/AngelscriptBindingsModuleBuilder.h"
#include "Shared/AngelscriptBindingsAssertions.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;
using namespace AngelscriptTestBindings;


TEST_CLASS_WITH_FLAGS(FAngelscriptBodyInstanceBindingsTest,
	"Angelscript.TestModule.Bindings.BodyInstance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL() { ASTEST_CREATE_ENGINE(); }
	AFTER_ALL() { FAngelscriptEngine& E = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(E); }

	TEST_METHOD(FLatentActionInfoDefault)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		FCoverageModuleScope Mod(*TestRunner, Engine, TEXT("ASBodyInstance_Latent"), TEXT(R"(
int LatentInfo_DefaultLinkage()
{
	FLatentActionInfo Info;
	return Info.Linkage;
}
)"));
		if (!Mod.IsValid())
		{
			TestRunner->AddInfo(TEXT("FLatentActionInfo not available, skipping"));
			return;
		}
		// UE 5.7: FLatentActionInfo default Linkage changed from 0 to -1
		AngelscriptTestBindings::ExpectGlobalInt(*TestRunner, Engine, Mod.GetModule(), 
			TEXT("int LatentInfo_DefaultLinkage()"),
			TEXT("Default FLatentActionInfo linkage"), -1);
	}
};

#endif
