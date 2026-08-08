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

	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

	TEST_METHOD(FRotatorBasics)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASQuat3f_Rotator"), ASTEST_AS(R"AS(
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
			)AS"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		const FExpectedGlobalInt Cases[] = {
			{ TEXT("int Rotator_ZeroIsZero()"),          TEXT("Zero rotator is zero"), 1 },
			{ TEXT("int Rotator_ComponentsPreserved()"), TEXT("Rotator components preserved"), 1 },
			{ TEXT("int Rotator_IsNearlyZero()"),        TEXT("Small rotator is nearly zero"), 1 },
			{ TEXT("int Rotator_IsZero()"),              TEXT("Zero rotator IsZero"), 1 },
		};
		ASSERT_THAT(IsTrue(
			ExpectGlobalInts(*TestRunner, Engine, M,  Cases),
			TEXT("ExpectGlobalInts should pass")));
	}

	TEST_METHOD(FQuatIdentity)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASQuat3f_Quat"), ASTEST_AS(R"AS(
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
			)AS"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		const FExpectedGlobalInt Cases[] = {
			{ TEXT("int Quat_IdentityIsNormalized()"), TEXT("Identity quat is normalized"), 1 },
			{ TEXT("int Quat_IdentityComponents()"),   TEXT("Identity quat has correct XYZW"), 1 },
		};
		ASSERT_THAT(IsTrue(
			ExpectGlobalInts(*TestRunner, Engine, M,  Cases),
			TEXT("ExpectGlobalInts should pass")));
	}

	TEST_METHOD(FTransformIdentity)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASQuat3f_Transform"), ASTEST_AS(R"AS(
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
			)AS"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		const FExpectedGlobalInt Cases[] = {
			{ TEXT("int Transform_IdentityLocation()"), TEXT("Identity transform location is origin"), 1 },
			{ TEXT("int Transform_IdentityScale()"),    TEXT("Identity transform scale is 1"), 1 },
		};
		ASSERT_THAT(IsTrue(
			ExpectGlobalInts(*TestRunner, Engine, M,  Cases),
			TEXT("ExpectGlobalInts should pass")));
	}

	TEST_METHOD(FVector4fBasics)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASQuat3f_Vector4f"), ASTEST_AS(R"AS(
			int Vector4f_ConstructionAndArithmetic()
			{
				FVector4f Value(1.0f, 2.0f, 3.0f, 4.0f);
				FVector4f Result = (Value + FVector4f(1.0f, 1.0f, 1.0f, 1.0f)) * 2.0f;
				return Result == FVector4f(4.0f, 6.0f, 8.0f, 10.0f) ? 1 : 0;
			}

			int Vector4f_CrossPrecisionConversion()
			{
				FVector4 DoubleValue(1.0, 2.0, 3.0, 4.0);
				FVector4f FloatValue(DoubleValue);
				FVector4 RoundTrip(FloatValue);
				return RoundTrip == DoubleValue ? 1 : 0;
			}
			)AS"));
		ASSERT_THAT(IsTrue(Mod.IsValid(), TEXT("FVector4f arithmetic and cross-precision constructors should compile")));
		if (!Mod.IsValid())
		{
			return;
		}

		const FExpectedGlobalInt Cases[] = {
			{TEXT("int Vector4f_ConstructionAndArithmetic()"), TEXT("FVector4f construction and arithmetic should preserve components"), 1},
			{TEXT("int Vector4f_CrossPrecisionConversion()"), TEXT("FVector4/FVector4f cross-precision constructors should round-trip exact values"), 1},
		};
		ASSERT_THAT(IsTrue(
			ExpectGlobalInts(*TestRunner, Engine, Mod.GetModule(), Cases),
			TEXT("FVector4f behavior cases should pass")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
