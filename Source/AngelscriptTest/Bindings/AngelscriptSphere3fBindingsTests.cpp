// =============================================================================
// AngelscriptSphere3fBindingsTests.cpp
//
// CQTest coverage for FSphere, FSphere3f, FPlane4f bindings.
// Automation IDs: Angelscript.TestModule.Bindings.Sphere3f.*
// =============================================================================

#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestExecute.h"

#if WITH_ANGELSCRIPT_UNITTESTS



TEST_CLASS_WITH_FLAGS(FAngelscriptSphere3fBindingsTest,
	"Angelscript.TestModule.Bindings.Sphere3f",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

	TEST_METHOD(FSphereBasics)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASSphere3f_Sphere"), ASTEST_AS(R"AS(
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
			)AS"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		const FExpectedGlobalInt Cases[] = {
			{ TEXT("int Sphere_CenterPreserved()"), TEXT("Sphere center preserved"), 1 },
			{ TEXT("int Sphere_RadiusPreserved()"), TEXT("Sphere radius preserved"), 1 },
			{ TEXT("int Sphere_IsInsideTrue()"),    TEXT("Point inside sphere"), 1 },
			{ TEXT("int Sphere_IsInsideFalse()"),   TEXT("Point outside sphere"), 1 },
		};
		ASSERT_THAT(IsTrue(
			ExpectGlobalInts(*TestRunner, Engine, M,  Cases),
			TEXT("ExpectGlobalInts should pass")));
	}

	TEST_METHOD(FSphere3fBasics)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASSphere3f_Sphere3f"), ASTEST_AS(R"AS(
			int Sphere3f_CenterPreserved()
			{
				FSphere3f S = FSphere3f(FVector3f(1.0f,2.0f,3.0f), 5.0f);
				return (S.Center.X == 1.0f && S.Center.Y == 2.0f && S.Center.Z == 3.0f) ? 1 : 0;
			}

			int Sphere3f_RadiusPreserved()
			{
				FSphere3f S = FSphere3f(FVector3f(0.0f,0.0f,0.0f), 12.5f);
				return (S.W == 12.5f) ? 1 : 0;
			}

			int Sphere3f_EqualsCopy()
			{
				FSphere3f S = FSphere3f(FVector3f(1.0f,2.0f,3.0f), 5.0f);
				FSphere3f Copy = FSphere3f(S);
				return S.Equals(Copy) ? 1 : 0;
			}

			int Sphere3f_Intersects()
			{
				FSphere3f A = FSphere3f(FVector3f(0.0f,0.0f,0.0f), 10.0f);
				FSphere3f B = FSphere3f(FVector3f(1.0f,1.0f,1.0f), 1.0f);
				return A.Intersects(B) ? 1 : 0;
			}
			)AS"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		const FExpectedGlobalInt Cases[] = {
			{ TEXT("int Sphere3f_CenterPreserved()"), TEXT("Sphere3f center preserved"), 1 },
			{ TEXT("int Sphere3f_RadiusPreserved()"), TEXT("Sphere3f radius preserved"), 1 },
			{ TEXT("int Sphere3f_EqualsCopy()"),      TEXT("Sphere3f copy compares equal"), 1 },
			{ TEXT("int Sphere3f_Intersects()"),      TEXT("Sphere3f intersection succeeds"), 1 },
		};
		ASSERT_THAT(IsTrue(
			ExpectGlobalInts(*TestRunner, Engine, M,  Cases),
			TEXT("ExpectGlobalInts should pass")));
	}

	TEST_METHOD(FPlaneBasics)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASSphere3f_Plane"), ASTEST_AS(R"AS(
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
			)AS"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		const FExpectedGlobalInt Cases[] = {
			{ TEXT("int Plane_NormalPreserved()"), TEXT("Plane point+normal constructor preserves normal"), 1 },
			{ TEXT("int Plane_WPreserved()"),      TEXT("Plane point+normal constructor computes W"), 1 },
		};
		ASSERT_THAT(IsTrue(
			ExpectGlobalInts(*TestRunner, Engine, M,  Cases),
			TEXT("ExpectGlobalInts should pass")));
	}

	TEST_METHOD(FPlane4fBasics)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASSphere3f_Plane4f"), ASTEST_AS(R"AS(
			int Plane4f_NormalPreserved()
			{
				FPlane4f P = FPlane4f(FVector3f(0.0f,0.0f,5.0f), FVector3f(0.0f,0.0f,1.0f));
				FVector3f N = P.GetNormal();
				return (N.X == 0.0f && N.Y == 0.0f && N.Z == 1.0f) ? 1 : 0;
			}

			int Plane4f_OriginPreserved()
			{
				FPlane4f P = FPlane4f(FVector3f(0.0f,0.0f,5.0f), FVector3f(0.0f,0.0f,1.0f));
				FVector3f O = P.GetOrigin();
				return (O.X == 0.0f && O.Y == 0.0f && O.Z == 5.0f) ? 1 : 0;
			}

			int Plane4f_PlaneDotOrigin()
			{
				FPlane4f P = FPlane4f(FVector3f(0.0f,0.0f,5.0f), FVector3f(0.0f,0.0f,1.0f));
				return (P.PlaneDot(FVector3f(0.0f,0.0f,5.0f)) == 0.0f) ? 1 : 0;
			}
			)AS"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		const FExpectedGlobalInt Cases[] = {
			{ TEXT("int Plane4f_NormalPreserved()"), TEXT("Plane4f point+normal constructor preserves normal"), 1 },
			{ TEXT("int Plane4f_OriginPreserved()"), TEXT("Plane4f point+normal constructor preserves origin"), 1 },
			{ TEXT("int Plane4f_PlaneDotOrigin()"),  TEXT("Plane4f PlaneDot is zero at the origin point"), 1 },
		};
		ASSERT_THAT(IsTrue(
			ExpectGlobalInts(*TestRunner, Engine, M,  Cases),
			TEXT("ExpectGlobalInts should pass")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
