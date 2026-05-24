// =============================================================================
// AngelscriptSphere3fBindingsTests.cpp
//
// CQTest coverage for FSphere, FSphere3f, FPlane4f bindings.
// Automation IDs: Angelscript.TestModule.Bindings.Sphere3f.*
// =============================================================================

#include "CQTest.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Shared/AngelscriptTestModuleScope.h"
#include "Shared/AngelscriptTestExecute.h"

#if WITH_DEV_AUTOMATION_TESTS



TEST_CLASS_WITH_FLAGS(FAngelscriptSphere3fBindingsTest,
	"Angelscript.TestModule.Bindings.Sphere3f",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	TEST_METHOD(FSphereBasics)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASSphere3f_Sphere"), TEXT(R"(
int Sphere_CenterPreserved()
{
	FSphere S = FSphere(FVector(1,2,3), 5.0);
	return (S.Center.X == 1.0 && S.Center.Y == 2.0 && S.Center.Z == 3.0) ? 1 : 0;
}
int Sphere_RadiusPreserved()
{
	FSphere S = FSphere(FVector(0,0,0), 12.5);
	return (S.W == 12.5) ? 1 : 0;
}
int Sphere_IsInsideTrue()
{
	FSphere S = FSphere(FVector(0,0,0), 10.0);
	return S.IsInside(FVector(1,1,1)) ? 1 : 0;
}
int Sphere_IsInsideFalse()
{
	FSphere S = FSphere(FVector(0,0,0), 1.0);
	return S.IsInside(FVector(10,10,10)) ? 0 : 1;
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		const FExpectedGlobalInt Cases[] = {
			{ TEXT("int Sphere_CenterPreserved()"), TEXT("Sphere center preserved"), 1 },
			{ TEXT("int Sphere_RadiusPreserved()"), TEXT("Sphere radius preserved"), 1 },
			{ TEXT("int Sphere_IsInsideTrue()"),    TEXT("Point inside sphere"), 1 },
			{ TEXT("int Sphere_IsInsideFalse()"),   TEXT("Point outside sphere"), 1 },
		};
		ExpectGlobalInts(*TestRunner, Engine, M,  Cases);
	}

	TEST_METHOD(FPlaneBasics)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASSphere3f_Plane"), TEXT(R"(
int Plane_NormalPreserved()
{
	FPlane P = FPlane(FVector(0,0,5), FVector(0,0,1));
	return (P.X == 0.0 && P.Y == 0.0 && P.Z == 1.0) ? 1 : 0;
}
int Plane_WPreserved()
{
	FPlane P = FPlane(FVector(0,0,5), FVector(0,0,1));
	return (P.W == 5.0) ? 1 : 0;
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		const FExpectedGlobalInt Cases[] = {
			{ TEXT("int Plane_NormalPreserved()"), TEXT("Plane point+normal constructor preserves normal"), 1 },
			{ TEXT("int Plane_WPreserved()"),      TEXT("Plane point+normal constructor computes W"), 1 },
		};
		ExpectGlobalInts(*TestRunner, Engine, M,  Cases);
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
