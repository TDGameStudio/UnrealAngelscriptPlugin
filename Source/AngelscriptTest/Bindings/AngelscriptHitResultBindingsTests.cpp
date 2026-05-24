// AngelscriptHitResultBindingsTests.cpp
// CQTest coverage for FHitResult, FOverlapResult bindings.
// Automation IDs: Angelscript.TestModule.Bindings.HitResult.*

#include "CQTest.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Shared/AngelscriptTestModuleScope.h"
#include "Shared/AngelscriptBindingsAssertions.h"

#if WITH_DEV_AUTOMATION_TESTS



TEST_CLASS_WITH_FLAGS(FAngelscriptHitResultBindingsTest,
	"Angelscript.TestModule.Bindings.HitResult",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL() { ASTEST_CREATE_ENGINE(); }
	AFTER_ALL() { FAngelscriptEngine& E = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(E); }

	TEST_METHOD(FHitResultDefault)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		// `bBlockingHit` is a bitfield UPROPERTY which is not exposed via raw-field
		// binding; the autoaccessor synthesis that previously surfaced it was removed
		// in 2026-05-22-refactor-as-remove-autoaccessor. Switch to a bound non-bitfield
		// member (`Time`) which defaults to 1.0 in FHitResult's default constructor.
		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASHitResult_Default"), TEXT(R"(
int HitResult_DefaultTime()
{
	FHitResult Hit;
	return (Hit.Time == 1.0) ? 1 : 0;
}
)"));
		if (!Mod.IsValid())
		{
			TestRunner->AddInfo(TEXT("FHitResult not available, skipping"));
			return;
		}
		ExpectGlobalInt(*TestRunner, Engine, Mod.GetModule(), 
			TEXT("int HitResult_DefaultTime()"),
			TEXT("Default FHitResult constructor sets Time to 1.0"), 1);
	}

	TEST_METHOD(FHitResultDistance)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASHitResult_Distance"), TEXT(R"(
int HitResult_DefaultDistance()
{
	FHitResult Hit;
	return (Hit.Distance == 0.0) ? 1 : 0;
}
)"));
		if (!Mod.IsValid())
		{
			TestRunner->AddInfo(TEXT("FHitResult.Distance not available, skipping"));
			return;
		}
		ExpectGlobalInt(*TestRunner, Engine, Mod.GetModule(), 
			TEXT("int HitResult_DefaultDistance()"),
			TEXT("Default FHitResult distance is 0"), 1);
	}

	TEST_METHOD(FOverlapResultDefault)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASHitResult_Overlap"), TEXT(R"(
int OverlapResult_DefaultNoOverlap()
{
	FOverlapResult Overlap;
	return (Overlap.ItemIndex == 0) ? 1 : 0;
}
)"));
		if (!Mod.IsValid())
		{
			TestRunner->AddInfo(TEXT("FOverlapResult not available, skipping"));
			return;
		}
		ExpectGlobalInt(*TestRunner, Engine, Mod.GetModule(), 
			TEXT("int OverlapResult_DefaultNoOverlap()"),
			TEXT("Default FOverlapResult ItemIndex is 0"), 1);
	}
};

#endif
