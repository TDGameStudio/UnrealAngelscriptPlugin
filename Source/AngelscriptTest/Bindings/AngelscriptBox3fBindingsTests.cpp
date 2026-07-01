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

#if WITH_ANGELSCRIPT_UNITTESTS



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

	TEST_METHOD(FBox3fConstruction)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASBox3f_FBox3fCtor"), TEXT(R"(
int FBox3f_DefaultIsValid()
{
	FBox3f B;
	return (B.Min.X == 0.0f && B.Min.Y == 0.0f && B.Min.Z == 0.0f && B.Max.X == 0.0f && B.Max.Y == 0.0f && B.Max.Z == 0.0f) ? 1 : 0;
}
int FBox3f_InitIsValid()
{
	FBox3f B = FBox3f(FVector3f(0.0f,0.0f,0.0f), FVector3f(1.0f,1.0f,1.0f));
	return (B.Min.X == 0.0f && B.Min.Y == 0.0f && B.Min.Z == 0.0f && B.Max.X == 1.0f && B.Max.Y == 1.0f && B.Max.Z == 1.0f) ? 1 : 0;
}
int FBox3f_GetCenter()
{
	FBox3f B = FBox3f(FVector3f(0.0f,0.0f,0.0f), FVector3f(10.0f,10.0f,10.0f));
	FVector3f C = B.GetCenter();
	return (C.X == 5.0f && C.Y == 5.0f && C.Z == 5.0f) ? 1 : 0;
}
int FBox3f_IsInside()
{
	FBox3f B = FBox3f(FVector3f(0.0f,0.0f,0.0f), FVector3f(4.0f,6.0f,8.0f));
	return B.IsInside(FVector3f(2.0f,3.0f,4.0f)) ? 1 : 0;
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		const FExpectedGlobalInt Cases[] = {
			{ TEXT("int FBox3f_DefaultIsValid()"), TEXT("Default FBox3f preserves ForceInit zero fields"), 1 },
			{ TEXT("int FBox3f_InitIsValid()"),    TEXT("Initialized FBox3f preserves Min and Max"), 1 },
			{ TEXT("int FBox3f_GetCenter()"),      TEXT("FBox3f GetCenter returns midpoint"), 1 },
			{ TEXT("int FBox3f_IsInside()"),       TEXT("FBox3f IsInside accepts an internal point"), 1 },
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

	TEST_METHOD(FBoxSphereBounds3fConstruction)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASBox3f_BSB3fCtor"), TEXT(R"(
int BSB3f_Origin()
{
	FBoxSphereBounds3f B = FBoxSphereBounds3f(FVector3f(1.0f,2.0f,3.0f), FVector3f(4.0f,5.0f,6.0f), 10.0f);
	return (B.Origin.X == 1.0f && B.Origin.Y == 2.0f && B.Origin.Z == 3.0f) ? 1 : 0;
}
int BSB3f_SphereRadius()
{
	FBoxSphereBounds3f B = FBoxSphereBounds3f(FVector3f(0.0f,0.0f,0.0f), FVector3f(1.0f,1.0f,1.0f), 7.5f);
	return (B.SphereRadius == 7.5f) ? 1 : 0;
}
int BSB3f_GetBox()
{
	FBoxSphereBounds3f B = FBoxSphereBounds3f(FVector3f(1.0f,2.0f,3.0f), FVector3f(4.0f,5.0f,6.0f), 10.0f);
	FBox3f Box = B.GetBox();
	return (Box.Min.X == -3.0f && Box.Min.Y == -3.0f && Box.Min.Z == -3.0f && Box.Max.X == 5.0f && Box.Max.Y == 7.0f && Box.Max.Z == 9.0f) ? 1 : 0;
}
int BSB3f_GetSphere()
{
	FBoxSphereBounds3f B = FBoxSphereBounds3f(FVector3f(0.0f,0.0f,0.0f), FVector3f(1.0f,1.0f,1.0f), 7.5f);
	FSphere3f S = B.GetSphere();
	return (S.Center.X == 0.0f && S.Center.Y == 0.0f && S.Center.Z == 0.0f && S.W == 7.5f) ? 1 : 0;
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		const FExpectedGlobalInt Cases[] = {
			{ TEXT("int BSB3f_Origin()"),       TEXT("BoxSphereBounds3f origin preserved"), 1 },
			{ TEXT("int BSB3f_SphereRadius()"), TEXT("BoxSphereBounds3f radius preserved"), 1 },
			{ TEXT("int BSB3f_GetBox()"),       TEXT("BoxSphereBounds3f GetBox returns expected FBox3f"), 1 },
			{ TEXT("int BSB3f_GetSphere()"),    TEXT("BoxSphereBounds3f GetSphere returns expected FSphere3f"), 1 },
		};
		ExpectGlobalInts(*TestRunner, Engine, M,  Cases);
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
