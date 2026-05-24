// =============================================================================
// AngelscriptBox3fBindingsTests.cpp
//
// CQTest coverage for FBox, FBox3f, FBoxSphereBounds, FBoxSphereBounds3f bindings.
// Automation IDs: Angelscript.TestModule.Bindings.Box3f.*
// =============================================================================

#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestExecute.h"

#if WITH_DEV_AUTOMATION_TESTS



TEST_CLASS_WITH_FLAGS(FAngelscriptBox3fBindingsTest,
	"Angelscript.TestModule.Bindings.Box3f",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	TEST_METHOD(FBoxConstruction)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASBox3f_FBoxCtor"), TEXT(R"(
int FBox_DefaultIsValid()
{
	FBox B;
	return (B.Min.X == 0.0 && B.Min.Y == 0.0 && B.Min.Z == 0.0 && B.Max.X == 0.0 && B.Max.Y == 0.0 && B.Max.Z == 0.0) ? 1 : 0;
}
int FBox_InitIsValid()
{
	FBox B = FBox(FVector(0,0,0), FVector(1,1,1));
	return (B.Min.X == 0.0 && B.Min.Y == 0.0 && B.Min.Z == 0.0 && B.Max.X == 1.0 && B.Max.Y == 1.0 && B.Max.Z == 1.0) ? 1 : 0;
}
int FBox_GetCenter()
{
	FBox B = FBox(FVector(0,0,0), FVector(10,10,10));
	FVector C = B.GetCenter();
	return (C.X == 5.0 && C.Y == 5.0 && C.Z == 5.0) ? 1 : 0;
}
int FBox_GetExtent()
{
	FBox B = FBox(FVector(0,0,0), FVector(4,6,8));
	FVector E = B.GetExtent();
	return (E.X == 2.0 && E.Y == 3.0 && E.Z == 4.0) ? 1 : 0;
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		const FExpectedGlobalInt Cases[] = {
			{ TEXT("int FBox_DefaultIsValid()"), TEXT("Default FBox preserves ForceInit zero fields"), 1 },
			{ TEXT("int FBox_InitIsValid()"),    TEXT("Initialized FBox preserves Min and Max"), 1 },
			{ TEXT("int FBox_GetCenter()"),      TEXT("GetCenter returns midpoint"), 1 },
			{ TEXT("int FBox_GetExtent()"),      TEXT("GetExtent returns half-size"), 1 },
		};
		ExpectGlobalInts(*TestRunner, Engine, M,  Cases);
	}

	TEST_METHOD(FBoxSphereBoundsConstruction)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASBox3f_BSBCtor"), TEXT(R"(
int BSB_Origin()
{
	FBoxSphereBounds B = FBoxSphereBounds(FVector(1,2,3), FVector(4,5,6), 10.0);
	return (B.Origin.X == 1.0 && B.Origin.Y == 2.0 && B.Origin.Z == 3.0) ? 1 : 0;
}
int BSB_SphereRadius()
{
	FBoxSphereBounds B = FBoxSphereBounds(FVector(0,0,0), FVector(1,1,1), 7.5);
	return (B.SphereRadius == 7.5) ? 1 : 0;
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		const FExpectedGlobalInt Cases[] = {
			{ TEXT("int BSB_Origin()"),       TEXT("BoxSphereBounds origin preserved"), 1 },
			{ TEXT("int BSB_SphereRadius()"), TEXT("BoxSphereBounds radius preserved"), 1 },
		};
		ExpectGlobalInts(*TestRunner, Engine, M,  Cases);
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
