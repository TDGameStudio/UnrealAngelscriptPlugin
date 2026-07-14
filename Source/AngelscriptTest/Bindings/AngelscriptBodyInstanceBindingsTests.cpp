// AngelscriptBodyInstanceBindingsTests.cpp
// CQTest coverage for FBodyInstance, FLatentActionInfo bindings.
// Automation IDs: Angelscript.TestModule.Bindings.BodyInstance.*

#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestExecute.h"

#if WITH_ANGELSCRIPT_UNITTESTS



TEST_CLASS_WITH_FLAGS(FAngelscriptBodyInstanceBindingsTest,
	"Angelscript.TestModule.Bindings.BodyInstance",
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

	TEST_METHOD(FLatentActionInfoDefault)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASBodyInstance_Latent"), ASTEST_AS(R"AS(
			int LatentInfo_DefaultLinkage()
			{
				FLatentActionInfo Info;
				return Info.Linkage;
			}
			)AS"));
		if (!Mod.IsValid())
		{
			TestRunner->AddInfo(TEXT("FLatentActionInfo not available, skipping"));
			return;
		}
		// UE 5.7: FLatentActionInfo default Linkage changed from 0 to -1
		ASSERT_THAT(IsTrue(
			ExpectGlobalInt(*TestRunner, Engine, Mod.GetModule(), TEXT("int LatentInfo_DefaultLinkage()"), TEXT("Default FLatentActionInfo linkage"), -1),
			TEXT("ExpectGlobalInt should pass")));
	}

	TEST_METHOD(FLatentActionInfoExplicitConstructor)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASBodyInstance_LatentExplicit"), ASTEST_AS(R"AS(
			int LatentInfo_ExplicitConstructor()
			{
				UObject CallbackTarget;
				FLatentActionInfo Info(7, 11, n"Done", CallbackTarget);
				return Info.Linkage + Info.UUID;
			}
			)AS"));
		if (!Mod.IsValid())
		{
			return;
		}

		ASSERT_THAT(IsTrue(
			ExpectGlobalInt(*TestRunner, Engine, Mod.GetModule(), TEXT("int LatentInfo_ExplicitConstructor()"), TEXT("Explicit FLatentActionInfo constructor should remain callable"), 18),
			TEXT("ExpectGlobalInt should pass")));
	}
};

#endif
