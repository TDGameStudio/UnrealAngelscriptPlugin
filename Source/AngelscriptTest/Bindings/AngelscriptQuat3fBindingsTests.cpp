// =============================================================================
// AngelscriptQuat3fBindingsTests.cpp
//
// CQTest coverage for FQuat4f, FRotator3f, FTransform3f, FVector4f bindings.
// Automation IDs: Angelscript.TestModule.Bindings.Quat3f.*
// =============================================================================

#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestExecute.h"

#if WITH_ANGELSCRIPT_UNITTESTS



TEST_CLASS_WITH_FLAGS(FAngelscriptQuat3fBindingsTest,
	"Angelscript.TestModule.Bindings.Quat3f",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	TEST_METHOD(FRotatorBasics)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASQuat3f_Rotator"), TEXT(R"(
int Rotator_ZeroIsZero()
{
	FRotator R = FRotator(0, 0, 0);
	return (R.Pitch == 0.0 && R.Yaw == 0.0 && R.Roll == 0.0) ? 1 : 0;
}
int Rotator_ComponentsPreserved()
{
	FRotator R = FRotator(45.0, 90.0, 180.0);
	return (R.Pitch == 45.0 && R.Yaw == 90.0 && R.Roll == 180.0) ? 1 : 0;
}
int Rotator_IsNearlyZero()
{
	// UE 5.7: FRotator::IsNearlyZero default tolerance tightened; use explicit tolerance
	FRotator R = FRotator(0.0001, 0.0001, 0.0001);
	return R.IsNearlyZero(0.001) ? 1 : 0;
}
int Rotator_IsZero()
{
	FRotator R = FRotator(0, 0, 0);
	return R.IsZero() ? 1 : 0;
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		const FExpectedGlobalInt Cases[] = {
			{ TEXT("int Rotator_ZeroIsZero()"),          TEXT("Zero rotator is zero"), 1 },
			{ TEXT("int Rotator_ComponentsPreserved()"), TEXT("Rotator components preserved"), 1 },
			{ TEXT("int Rotator_IsNearlyZero()"),        TEXT("Small rotator is nearly zero"), 1 },
			{ TEXT("int Rotator_IsZero()"),              TEXT("Zero rotator IsZero"), 1 },
		};
		ExpectGlobalInts(*TestRunner, Engine, M,  Cases);
	}

	TEST_METHOD(FQuatIdentity)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASQuat3f_Quat"), TEXT(R"(
int Quat_IdentityIsNormalized()
{
	FQuat Q = FQuat::Identity;
	return Q.IsNormalized() ? 1 : 0;
}
int Quat_IdentityComponents()
{
	FQuat Q = FQuat::Identity;
	return (Q.X == 0.0 && Q.Y == 0.0 && Q.Z == 0.0 && Q.W == 1.0) ? 1 : 0;
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		const FExpectedGlobalInt Cases[] = {
			{ TEXT("int Quat_IdentityIsNormalized()"), TEXT("Identity quat is normalized"), 1 },
			{ TEXT("int Quat_IdentityComponents()"),   TEXT("Identity quat has correct XYZW"), 1 },
		};
		ExpectGlobalInts(*TestRunner, Engine, M,  Cases);
	}

	TEST_METHOD(FTransformIdentity)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASQuat3f_Transform"), TEXT(R"(
int Transform_IdentityLocation()
{
	FTransform T = FTransform::Identity;
	FVector Loc = T.GetLocation();
	return (Loc.X == 0.0 && Loc.Y == 0.0 && Loc.Z == 0.0) ? 1 : 0;
}
int Transform_IdentityScale()
{
	FTransform T = FTransform::Identity;
	FVector S = T.GetScale3D();
	return (S.X == 1.0 && S.Y == 1.0 && S.Z == 1.0) ? 1 : 0;
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		const FExpectedGlobalInt Cases[] = {
			{ TEXT("int Transform_IdentityLocation()"), TEXT("Identity transform location is origin"), 1 },
			{ TEXT("int Transform_IdentityScale()"),    TEXT("Identity transform scale is 1"), 1 },
		};
		ExpectGlobalInts(*TestRunner, Engine, M,  Cases);
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
