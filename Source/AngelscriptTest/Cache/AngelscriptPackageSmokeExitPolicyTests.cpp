#include "CoreMinimal.h"

#if WITH_ANGELSCRIPT_UNITTESTS

#include "Core/AngelscriptEngine.h"
#include "CQTest.h"

TEST_CLASS_WITH_FLAGS(
	FAngelscriptPackageSmokeExitPolicyTests,
	"Angelscript.TestModule.Cache.PackageSmokeExitPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ExplicitSmokeExitIsRequestedOnlyAfterSuccessfulStartup)
	{
		FAngelscriptEngineConfig Config;
		ASSERT_THAT(IsFalse(
			ShouldRequestAngelscriptCachePackageSmokeExit(Config, true)));

		Config.bExitAfterStartupForCacheSmoke = true;
		ASSERT_THAT(IsTrue(
			ShouldRequestAngelscriptCachePackageSmokeExit(Config, true)));
		ASSERT_THAT(IsFalse(
			ShouldRequestAngelscriptCachePackageSmokeExit(Config, false)));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
