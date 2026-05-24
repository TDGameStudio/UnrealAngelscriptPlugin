// ============================================================================
// AngelscriptQuatBindingsTests.cpp
//
// FQuat binding coverage — CQTest refactor. Automation IDs:
//   Angelscript.TestModule.Bindings.Quat.FAngelscriptQuatBindingsTest.*
//
// Sections:
//   Identity          — FQuat::Identity round-trip, IsIdentity
//   NormalizeOps      — Normalize after scalar inflation, IsNormalized
//   RotateVector      — RotateVector, UnrotateVector
//   InverseAndDecomp  — Inverse, ToAxisAndAngle axis/angle
//   Conversions       — Rotator(), MakeFromEuler().Rotator()
//   Interpolation     — Slerp half-way, IsNormalized after Slerp
//
// CQTest adaptation notes:
//   Script functions return FQuat/FVector/FRotator/float64 directly; C++ side
//   compares against native expectations using tolerance helpers.
// ============================================================================

#include "CQTest.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Shared/AngelscriptBindingsModuleBuilder.h"
#include "Shared/AngelscriptBindingsAssertions.h"

#include "Math/Quat.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;
using namespace AngelscriptTestBindings;

// ----------------------------------------------------------------------------
// Profile
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// Test class
// ----------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(FAngelscriptQuatBindingsTest,
	"Angelscript.TestModule.Bindings.Quat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	// ====================================================================
	// Section: Identity
	// ====================================================================

	TEST_METHOD(Identity)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FCoverageModuleScope Mod(*TestRunner, Engine, TEXT("ASQuat_Identity"), TEXT(R"(
int Quat_IdentityRoundTrip()
{
	FQuat Q = FQuat::Identity;
	return (Q.X == 0.0 && Q.Y == 0.0 && Q.Z == 0.0 && Q.W == 1.0) ? 1 : 0;
}
int Quat_IsIdentity()
{
	FQuat Q = FQuat::Identity;
	return Q.IsIdentity() ? 1 : 0;
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		AngelscriptTestBindings::ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Quat_IdentityRoundTrip()"), TEXT("Identity components are 0,0,0,1"), 1);
		AngelscriptTestBindings::ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Quat_IsIdentity()"), TEXT("Identity reports IsIdentity"), 1);
	}

	// ====================================================================
	// Section: NormalizeOps
	// ====================================================================

	TEST_METHOD(NormalizeOps)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FCoverageModuleScope Mod(*TestRunner, Engine, TEXT("ASQuat_NormalizeOps"), TEXT(R"(
int Quat_NormalizeRecoversRotation()
{
	FQuat Q = FQuat(FVector::UpVector, 1.5707963267948966) * 3.0;
	Q.Normalize();
	FQuat Expected = FQuat(FVector::UpVector, 1.5707963267948966);
	return (Q.AngularDistance(Expected) < 0.001) ? 1 : 0;
}
int Quat_IsNormalizedAfterNormalize()
{
	FQuat Q = FQuat(FVector::UpVector, 1.5707963267948966) * 3.0;
	Q.Normalize();
	return Q.IsNormalized() ? 1 : 0;
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		AngelscriptTestBindings::ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Quat_NormalizeRecoversRotation()"), TEXT("Normalize recovers quarter-turn"), 1);
		AngelscriptTestBindings::ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Quat_IsNormalizedAfterNormalize()"), TEXT("IsNormalized after Normalize"), 1);
	}

	// ====================================================================
	// Section: RotateVector
	// ====================================================================

	TEST_METHOD(RotateVector)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FCoverageModuleScope Mod(*TestRunner, Engine, TEXT("ASQuat_RotateVector"), TEXT(R"(
int Quat_RotateVectorMatchesNative()
{
	const FQuat QuarterTurn = FQuat(FVector::UpVector, 1.5707963267948966);
	FVector Rotated = QuarterTurn.RotateVector(FVector::ForwardVector);
	// Quarter turn around Z should map X(1,0,0) to Y(0,1,0)
	return (Rotated.Equals(FVector::RightVector, 0.01)) ? 1 : 0;
}
int Quat_UnrotateVectorRecoversOriginal()
{
	const FQuat QuarterTurn = FQuat(FVector::UpVector, 1.5707963267948966);
	FVector Rotated = QuarterTurn.RotateVector(FVector::ForwardVector);
	FVector Unrotated = QuarterTurn.UnrotateVector(Rotated);
	return Unrotated.Equals(FVector::ForwardVector, 0.01) ? 1 : 0;
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		AngelscriptTestBindings::ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Quat_RotateVectorMatchesNative()"), TEXT("RotateVector quarter-turn"), 1);
		AngelscriptTestBindings::ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Quat_UnrotateVectorRecoversOriginal()"), TEXT("UnrotateVector recovers original"), 1);
	}

	// ====================================================================
	// Section: InverseAndDecomp
	// ====================================================================

	TEST_METHOD(InverseAndDecomp)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FCoverageModuleScope Mod(*TestRunner, Engine, TEXT("ASQuat_InverseAndDecomp"), TEXT(R"(
int Quat_InverseUndoesRotation()
{
	FQuat Q = FQuat(FVector::UpVector, 1.5707963267948966);
	FQuat Inv = Q.Inverse();
	FQuat Combined = Q * Inv;
	return Combined.IsIdentity(0.001) ? 1 : 0;
}
int Quat_ToAxisAndAngle_Axis()
{
	FVector Axis;
	float64 Angle = 0.0;
	FQuat(FVector::UpVector, 1.5707963267948966).ToAxisAndAngle(Axis, Angle);
	return Axis.Equals(FVector::UpVector, 0.001) ? 1 : 0;
}
int Quat_ToAxisAndAngle_Angle()
{
	FVector Axis;
	float64 Angle = 0.0;
	FQuat(FVector::UpVector, 1.5707963267948966).ToAxisAndAngle(Axis, Angle);
	return (Math::Abs(Angle - 1.5707963267948966) < 0.001) ? 1 : 0;
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		AngelscriptTestBindings::ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Quat_InverseUndoesRotation()"), TEXT("Inverse undoes rotation"), 1);
		AngelscriptTestBindings::ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Quat_ToAxisAndAngle_Axis()"), TEXT("ToAxisAndAngle reports UpVector axis"), 1);
		AngelscriptTestBindings::ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Quat_ToAxisAndAngle_Angle()"), TEXT("ToAxisAndAngle reports half-pi angle"), 1);
	}

	// ====================================================================
	// Section: Conversions
	// ====================================================================

	TEST_METHOD(Conversions)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FCoverageModuleScope Mod(*TestRunner, Engine, TEXT("ASQuat_Conversions"), TEXT(R"(
int Quat_RotatorConversion()
{
	FRotator R = FQuat(FVector::UpVector, 1.5707963267948966).Rotator();
	// Quarter turn around Z = 90 degrees Yaw
	return (Math::Abs(R.Yaw - 90.0) < 0.1) ? 1 : 0;
}
int Quat_MakeFromEulerRotator()
{
	FRotator R = FQuat::MakeFromEuler(FVector(10.0, 20.0, 30.0)).Rotator();
	// MakeFromEuler(Roll=10, Pitch=20, Yaw=30) should produce non-zero rotator
	return (Math::Abs(R.Yaw) > 0.1 || Math::Abs(R.Pitch) > 0.1 || Math::Abs(R.Roll) > 0.1) ? 1 : 0;
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		AngelscriptTestBindings::ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Quat_RotatorConversion()"), TEXT("Rotator() matches 90-deg yaw"), 1);
		AngelscriptTestBindings::ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Quat_MakeFromEulerRotator()"), TEXT("MakeFromEuler produces non-identity"), 1);
	}

	// ====================================================================
	// Section: Interpolation
	// ====================================================================

	TEST_METHOD(Interpolation)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FCoverageModuleScope Mod(*TestRunner, Engine, TEXT("ASQuat_Interpolation"), TEXT(R"(
int Quat_SlerpHalfWay()
{
	FQuat Q = FQuat::Slerp(FQuat::Identity, FQuat(FVector::UpVector, 1.5707963267948966), 0.5);
	// Half slerp of 90-degree rotation should be 45-degree rotation
	FRotator R = Q.Rotator();
	return (Math::Abs(R.Yaw - 45.0) < 0.5) ? 1 : 0;
}
int Quat_SlerpIsNormalized()
{
	FQuat Q = FQuat::Slerp(FQuat::Identity, FQuat(FVector::UpVector, 1.5707963267948966), 0.5);
	return Q.IsNormalized() ? 1 : 0;
}
)"));
		if (!Mod.IsValid()) return;
		auto& M = Mod.GetModule();

		AngelscriptTestBindings::ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Quat_SlerpHalfWay()"), TEXT("Slerp half-way is 45-deg"), 1);
		AngelscriptTestBindings::ExpectGlobalInt(*TestRunner, Engine, M,  TEXT("int Quat_SlerpIsNormalized()"), TEXT("Slerp result is normalized"), 1);
	}
};

#endif
