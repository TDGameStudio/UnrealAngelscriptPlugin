// ============================================================================
// AngelscriptMathOrientationBindingsTests.cpp
//
// Math orientation binding coverage — CQTest refactor.
// Automation ID:
//   Angelscript.TestModule.Bindings.Math.Orientation.FAngelscriptMathOrientationBindingsTest.*
//
// Sections:
//   FactoriesAndTransformMutators — FRotator factories, FQuat factories,
//     FTransform Blend/BlendWith/SetRotation parity
//
// CQTest adaptation notes:
//   One legacy automation test merged into one TEST_CLASS.
//   Struct returns use FAngelscriptTestExecutor::ExecuteAndExtractStruct with
//   tolerance checks from Bindings/AngelscriptMathBindingsTestCompare.h.
// ============================================================================

#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestExecute.h"
#include "Bindings/AngelscriptMathBindingsTestCompare.h"

#include "Math/Quat.h"
#include "Math/RotationMatrix.h"

#if WITH_ANGELSCRIPT_UNITTESTS



TEST_CLASS_WITH_FLAGS(FAngelscriptMathOrientationBindingsTest, "Angelscript.TestModule.Bindings.Math.Orientation",
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

	TEST_METHOD(FactoriesAndTransformMutators)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASMath_Orientation_FactoriesAndMutators"), ASTEST_AS(R"AS(
			FRotator GetAxesRotator()
			{
				return FRotator::MakeFromAxes(FVector(1.0f, 0.0f, 0.0f), FVector(0.0f, 1.0f, 0.0f), FVector(0.0f, 0.0f, 1.0f));
			}

			FVector GetAxesForward()
			{
				return GetAxesRotator().GetForwardVector();
			}

			FVector GetAxesForwardMember()
			{
				const FRotator Rotator = GetAxesRotator();
				return Rotator.GetForwardVector();
			}

			FVector GetAxesRight()
			{
				return GetAxesRotator().GetRightVector();
			}

			FVector GetAxesRightMember()
			{
				const FRotator Rotator = GetAxesRotator();
				return Rotator.GetRightVector();
			}

			FVector GetAxesUp()
			{
				return GetAxesRotator().GetUpVector();
			}

			FVector GetAxesUpMember()
			{
				const FRotator Rotator = GetAxesRotator();
				return Rotator.GetUpVector();
			}

			FRotator GetComposedRotator()
			{
				const FRotator A = FRotator(0.0f, 90.0f, 0.0f);
				const FRotator B = FRotator(45.0f, 0.0f, 0.0f);
				return A.Compose(B);
			}

			double GetRotatorAngularDistance()
			{
				const FRotator A = FRotator(0.0f, 0.0f, 0.0f);
				const FRotator B = FRotator(0.0f, 90.0f, 0.0f);
				return A.AngularDistance(B);
			}

			FQuat GetQuatFromX()
			{
				return FQuat::MakeFromX(FVector(1.0f, 1.0f, 0.0f));
			}

			FQuat GetQuatFromY()
			{
				return FQuat::MakeFromY(FVector(-1.0f, 1.0f, 0.0f));
			}

			FQuat GetQuatFromZ()
			{
				return FQuat::MakeFromZ(FVector(0.0f, 0.0f, 1.0f));
			}

			FQuat GetQuatFromXY()
			{
				return FQuat::MakeFromXY(FVector(1.0f, 1.0f, 0.0f), FVector(-1.0f, 1.0f, 0.0f));
			}

			FQuat GetQuatFromXZ()
			{
				return FQuat::MakeFromXZ(FVector(1.0f, 1.0f, 0.0f), FVector(0.0f, 0.0f, 1.0f));
			}

			FQuat GetQuatFromYX()
			{
				return FQuat::MakeFromYX(FVector(-1.0f, 1.0f, 0.0f), FVector(1.0f, 1.0f, 0.0f));
			}

			FQuat GetQuatFromYZ()
			{
				return FQuat::MakeFromYZ(FVector(-1.0f, 1.0f, 0.0f), FVector(0.0f, 0.0f, 1.0f));
			}

			FQuat GetQuatFromZX()
			{
				return FQuat::MakeFromZX(FVector(0.0f, 0.0f, 1.0f), FVector(1.0f, 1.0f, 0.0f));
			}

			FQuat GetQuatFromZY()
			{
				return FQuat::MakeFromZY(FVector(0.0f, 0.0f, 1.0f), FVector(-1.0f, 1.0f, 0.0f));
			}

			FTransform GetBlendTransform()
			{
				FTransform Result;
				const FTransform A = FTransform(FRotator(10.0f, 20.0f, 30.0f), FVector(100.0f, -50.0f, 25.0f), FVector(1.25f, 0.75f, 2.0f));
				const FTransform B = FTransform(FRotator(-20.0f, 70.0f, 10.0f), FVector(-40.0f, 80.0f, 5.0f), FVector(0.5f, 1.5f, 1.0f));
				Result.Blend(A, B, 0.25f);
				return Result;
			}

			FTransform GetBlendWithTransform()
			{
				FTransform Result = FTransform(FRotator(10.0f, 20.0f, 30.0f), FVector(100.0f, -50.0f, 25.0f), FVector(1.25f, 0.75f, 2.0f));
				const FTransform Other = FTransform(FRotator(-20.0f, 70.0f, 10.0f), FVector(-40.0f, 80.0f, 5.0f), FVector(0.5f, 1.5f, 1.0f));
				Result.BlendWith(Other, 0.5f);
				return Result;
			}

			FTransform GetSetRotationTransform()
			{
				FTransform Result = FTransform(FRotator(10.0f, 20.0f, 30.0f), FVector(100.0f, -50.0f, 25.0f), FVector(1.25f, 0.75f, 2.0f));
				Result.SetRotation(FRotator(-30.0f, 15.0f, 45.0f));
				return Result;
			}
			)AS"));
		if (!Mod.IsValid()) return;
		auto& Module = Mod.GetModule();

		const FVector CanonicalForward(1.0f, 0.0f, 0.0f);
		const FVector CanonicalRight(0.0f, 1.0f, 0.0f);
		const FVector CanonicalUp(0.0f, 0.0f, 1.0f);
		const FRotator ComposeA(0.0f, 90.0f, 0.0f);
		const FRotator ComposeB(45.0f, 0.0f, 0.0f);
		const FVector FactoryX(1.0f, 1.0f, 0.0f);
		const FVector FactoryY(-1.0f, 1.0f, 0.0f);
		const FVector FactoryZ(0.0f, 0.0f, 1.0f);
		const FTransform TransformA(FRotator(10.0f, 20.0f, 30.0f), FVector(100.0f, -50.0f, 25.0f), FVector(1.25f, 0.75f, 2.0f));
		const FTransform TransformB(FRotator(-20.0f, 70.0f, 10.0f), FVector(-40.0f, 80.0f, 5.0f), FVector(0.5f, 1.5f, 1.0f));
		const FRotator ReplacementRotation(-30.0f, 15.0f, 45.0f);

		FRotator ScriptAxesRotator;
		FVector ScriptAxesForward;
		FVector ScriptAxesForwardMember;
		FVector ScriptAxesRight;
		FVector ScriptAxesRightMember;
		FVector ScriptAxesUp;
		FVector ScriptAxesUpMember;
		FRotator ScriptComposedRotator;
		double ScriptRotatorAngularDistance = 0.0;
		FQuat ScriptQuatFromX;
		FQuat ScriptQuatFromY;
		FQuat ScriptQuatFromZ;
		FQuat ScriptQuatFromXY;
		FQuat ScriptQuatFromXZ;
		FQuat ScriptQuatFromYX;
		FQuat ScriptQuatFromYZ;
		FQuat ScriptQuatFromZX;
		FQuat ScriptQuatFromZY;
		FTransform ScriptBlendTransform;
		FTransform ScriptBlendWithTransform;
		FTransform ScriptSetRotationTransform;

		const auto ExecuteStructGlobal = [&](const TCHAR* FunctionDecl, auto& OutValue)
		{
			FAngelscriptTestExecutor Executor(*TestRunner, Engine, Module, FunctionDecl);
			return Executor.ExecuteAndExtractStruct(OutValue);
		};
		const auto ExecuteScalarGlobal = [&](const TCHAR* FunctionDecl, auto& OutValue)
		{
			FAngelscriptTestExecutor Executor(*TestRunner, Engine, Module, FunctionDecl);
			OutValue = Executor.ExecuteAndGet<std::remove_reference_t<decltype(OutValue)>>();
			return Executor.HasRun();
		};

		const bool bExecutedAll =
			ExecuteStructGlobal(TEXT("FRotator GetAxesRotator()"), ScriptAxesRotator) &&
			ExecuteStructGlobal(TEXT("FVector GetAxesForward()"), ScriptAxesForward) &&
			ExecuteStructGlobal(TEXT("FVector GetAxesForwardMember()"), ScriptAxesForwardMember) &&
			ExecuteStructGlobal(TEXT("FVector GetAxesRight()"), ScriptAxesRight) &&
			ExecuteStructGlobal(TEXT("FVector GetAxesRightMember()"), ScriptAxesRightMember) &&
			ExecuteStructGlobal(TEXT("FVector GetAxesUp()"), ScriptAxesUp) &&
			ExecuteStructGlobal(TEXT("FVector GetAxesUpMember()"), ScriptAxesUpMember) &&
			ExecuteStructGlobal(TEXT("FRotator GetComposedRotator()"), ScriptComposedRotator) &&
			ExecuteScalarGlobal(TEXT("double GetRotatorAngularDistance()"), ScriptRotatorAngularDistance) &&
			ExecuteStructGlobal(TEXT("FQuat GetQuatFromX()"), ScriptQuatFromX) &&
			ExecuteStructGlobal(TEXT("FQuat GetQuatFromY()"), ScriptQuatFromY) &&
			ExecuteStructGlobal(TEXT("FQuat GetQuatFromZ()"), ScriptQuatFromZ) &&
			ExecuteStructGlobal(TEXT("FQuat GetQuatFromXY()"), ScriptQuatFromXY) &&
			ExecuteStructGlobal(TEXT("FQuat GetQuatFromXZ()"), ScriptQuatFromXZ) &&
			ExecuteStructGlobal(TEXT("FQuat GetQuatFromYX()"), ScriptQuatFromYX) &&
			ExecuteStructGlobal(TEXT("FQuat GetQuatFromYZ()"), ScriptQuatFromYZ) &&
			ExecuteStructGlobal(TEXT("FQuat GetQuatFromZX()"), ScriptQuatFromZX) &&
			ExecuteStructGlobal(TEXT("FQuat GetQuatFromZY()"), ScriptQuatFromZY) &&
			ExecuteStructGlobal(TEXT("FTransform GetBlendTransform()"), ScriptBlendTransform) &&
			ExecuteStructGlobal(TEXT("FTransform GetBlendWithTransform()"), ScriptBlendWithTransform) &&
			ExecuteStructGlobal(TEXT("FTransform GetSetRotationTransform()"), ScriptSetRotationTransform);
		if (!bExecutedAll)
		{
			return;
		}

		const FRotator ExpectedAxesRotator = FMatrix(CanonicalForward.GetSafeNormal(), CanonicalRight.GetSafeNormal(), CanonicalUp.GetSafeNormal(), FVector::ZeroVector).Rotator();
		const FRotator ExpectedComposedRotator = FRotator(FQuat(ComposeB) * FQuat(ComposeA));
		const double ExpectedRotatorAngularDistance = FMath::RadiansToDegrees(FQuat(FRotator::ZeroRotator).AngularDistance(FQuat(FRotator(0.0f, 90.0f, 0.0f))));
		const FQuat ExpectedQuatFromX = FRotationMatrix::MakeFromX(FactoryX).ToQuat();
		const FQuat ExpectedQuatFromY = FRotationMatrix::MakeFromY(FactoryY).ToQuat();
		const FQuat ExpectedQuatFromZ = FRotationMatrix::MakeFromZ(FactoryZ).ToQuat();
		const FQuat ExpectedQuatFromXY = FRotationMatrix::MakeFromXY(FactoryX, FactoryY).ToQuat();
		const FQuat ExpectedQuatFromXZ = FRotationMatrix::MakeFromXZ(FactoryX, FactoryZ).ToQuat();
		const FQuat ExpectedQuatFromYX = FRotationMatrix::MakeFromYX(FactoryY, FactoryX).ToQuat();
		const FQuat ExpectedQuatFromYZ = FRotationMatrix::MakeFromYZ(FactoryY, FactoryZ).ToQuat();
		const FQuat ExpectedQuatFromZX = FRotationMatrix::MakeFromZX(FactoryZ, FactoryX).ToQuat();
		const FQuat ExpectedQuatFromZY = FRotationMatrix::MakeFromZY(FactoryZ, FactoryY).ToQuat();

		FTransform ExpectedBlendTransform;
		ExpectedBlendTransform.Blend(TransformA, TransformB, 0.25f);
		FTransform ExpectedBlendWithTransform = TransformA;
		ExpectedBlendWithTransform.BlendWith(TransformB, 0.5f);
		FTransform ExpectedSetRotationTransform = TransformA;
		ExpectedSetRotationTransform.SetRotation(ReplacementRotation.Quaternion());

		ASSERT_THAT(IsTrue(
			VerifyMathBindingsRotator(
			*TestRunner,
			TEXT("FRotator::MakeFromAxes should build the same orientation as the native matrix conversion"),
			ScriptAxesRotator,
			ExpectedAxesRotator)));
		ASSERT_THAT(IsTrue(
			VerifyMathBindingsVector(
			*TestRunner,
			TEXT("FRotator.GetForwardVector should recover the canonical forward axis from MakeFromAxes"),
			ScriptAxesForward,
			CanonicalForward)));
		ASSERT_THAT(IsTrue(
			VerifyMathBindingsVector(
			*TestRunner,
			TEXT("FRotator.GetForwardVector should recover the canonical forward axis from MakeFromAxes"),
			ScriptAxesForwardMember,
			CanonicalForward)));
		ASSERT_THAT(IsTrue(
			VerifyMathBindingsVector(
			*TestRunner,
			TEXT("FRotator.GetRightVector should recover the canonical right axis from MakeFromAxes"),
			ScriptAxesRight,
			CanonicalRight)));
		ASSERT_THAT(IsTrue(
			VerifyMathBindingsVector(
			*TestRunner,
			TEXT("FRotator.GetRightVector should recover the canonical right axis from MakeFromAxes"),
			ScriptAxesRightMember,
			CanonicalRight)));
		ASSERT_THAT(IsTrue(
			VerifyMathBindingsVector(
				*TestRunner,
				TEXT("FRotator.GetUpVector should recover the canonical up axis from MakeFromAxes"),
				ScriptAxesUp,
				CanonicalUp)));
		ASSERT_THAT(IsTrue(
			VerifyMathBindingsVector(
				*TestRunner,
				TEXT("FRotator.GetUpVector should recover the canonical up axis from MakeFromAxes"),
				ScriptAxesUpMember,
				CanonicalUp)));
		ASSERT_THAT(IsTrue(
			VerifyMathBindingsRotator(
				*TestRunner,
				TEXT("FRotator.Compose should preserve the native B * A multiplication order"),
				ScriptComposedRotator,
				ExpectedComposedRotator)));
		ASSERT_THAT(IsTrue(
			FMath::Abs(ScriptRotatorAngularDistance - ExpectedRotatorAngularDistance) <= 0.05,
			TEXT("FRotator.AngularDistance should expose orientation distance as an instance method")));
		ASSERT_THAT(IsTrue(
			VerifyMathBindingsQuat(
				*TestRunner,
				TEXT("FQuat::MakeFromX should match the native rotation matrix factory"),
				ScriptQuatFromX,
				ExpectedQuatFromX)));
		ASSERT_THAT(IsTrue(
			VerifyMathBindingsQuat(
				*TestRunner,
				TEXT("FQuat::MakeFromY should match the native rotation matrix factory"),
				ScriptQuatFromY,
				ExpectedQuatFromY)));
		ASSERT_THAT(IsTrue(
			VerifyMathBindingsQuat(
				*TestRunner,
				TEXT("FQuat::MakeFromZ should match the native rotation matrix factory"),
				ScriptQuatFromZ,
				ExpectedQuatFromZ)));
		ASSERT_THAT(IsTrue(
			VerifyMathBindingsQuat(
				*TestRunner,
				TEXT("FQuat::MakeFromXY should match the native rotation matrix factory"),
				ScriptQuatFromXY,
				ExpectedQuatFromXY)));
		ASSERT_THAT(IsTrue(
			VerifyMathBindingsQuat(
				*TestRunner,
				TEXT("FQuat::MakeFromXZ should match the native rotation matrix factory"),
				ScriptQuatFromXZ,
				ExpectedQuatFromXZ)));
		ASSERT_THAT(IsTrue(
			VerifyMathBindingsQuat(
			*TestRunner,
			TEXT("FQuat::MakeFromYX should match the native rotation matrix factory"),
			ScriptQuatFromYX,
			ExpectedQuatFromYX)));
		ASSERT_THAT(IsTrue(
			VerifyMathBindingsQuat(
			*TestRunner,
			TEXT("FQuat::MakeFromYZ should match the native rotation matrix factory"),
			ScriptQuatFromYZ,
			ExpectedQuatFromYZ)));
		ASSERT_THAT(IsTrue(
			VerifyMathBindingsQuat(
			*TestRunner,
			TEXT("FQuat::MakeFromZX should match the native rotation matrix factory"),
			ScriptQuatFromZX,
			ExpectedQuatFromZX)));
		ASSERT_THAT(IsTrue(
			VerifyMathBindingsQuat(
			*TestRunner,
			TEXT("FQuat::MakeFromZY should match the native rotation matrix factory"),
			ScriptQuatFromZY,
			ExpectedQuatFromZY)));
		ASSERT_THAT(IsTrue(
			VerifyMathBindingsTransform(
			*TestRunner,
			TEXT("FTransform::Blend should match native transform blending"),
			ScriptBlendTransform,
			ExpectedBlendTransform)));
		ASSERT_THAT(IsTrue(
			VerifyMathBindingsTransform(
			*TestRunner,
			TEXT("FTransform::BlendWith should match native in-place blending"),
			ScriptBlendWithTransform,
			ExpectedBlendWithTransform)));
		ASSERT_THAT(IsTrue(
			VerifyMathBindingsTransform(
			*TestRunner,
			TEXT("FTransform::SetRotation should update only the rotation component"),
			ScriptSetRotationTransform,
			ExpectedSetRotationTransform)));
		ASSERT_THAT(IsTrue(
			VerifyMathBindingsVector(
			*TestRunner,
			TEXT("FTransform::SetRotation should preserve the original translation"),
			ScriptSetRotationTransform.GetLocation(),
			TransformA.GetLocation())));
		ASSERT_THAT(IsTrue(
			VerifyMathBindingsVector(
			*TestRunner,
			TEXT("FTransform::SetRotation should preserve the original scale"),
			ScriptSetRotationTransform.GetScale3D(),
			TransformA.GetScale3D())));
	}

	TEST_METHOD(StaticDeltaAndRelativeHelpers)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASMath_Orientation_StaticDelta"), ASTEST_AS(R"AS(
			int QuatDeltaRoundTrips()
			{
				const FQuat Origin = FQuat(FRotator(0.0f, 15.0f, 0.0f));
				const FQuat Target = FQuat(FRotator(10.0f, 75.0f, 5.0f));
				const FQuat Delta = FQuat::GetDelta(Origin, Target);
				return FQuat::ApplyDelta(Origin, Delta).Equals(Target, 0.001f) ? 1 : 0;
			}

			int QuatRelativeRoundTrips()
			{
				const FQuat Parent = FQuat(FRotator(0.0f, 45.0f, 0.0f));
				const FQuat Child = FQuat(FRotator(20.0f, 90.0f, 0.0f));
				const FQuat Relative = FQuat::GetRelative(Parent, Child);
				return FQuat::ApplyRelative(Parent, Relative).Equals(Child, 0.001f) ? 1 : 0;
			}

			int QuatAngularVelocityRoundTrips()
			{
				const FVector AngularVelocity = FVector(0.0f, 0.0f, 2.0f);
				const FQuat Delta = FQuat::MakeDeltaRotationFromAngularVelocity(AngularVelocity, 0.5f);
				const FVector RoundTrip = FQuat::MakeAngularVelocityFromDeltaRotation(Delta, 0.5f);
				return RoundTrip.Equals(AngularVelocity, 0.001f) ? 1 : 0;
			}

			int RotatorDeltaRoundTrips()
			{
				const FRotator Origin = FRotator(0.0f, 15.0f, 0.0f);
				const FRotator Target = FRotator(10.0f, 75.0f, 5.0f);
				const FRotator Delta = FRotator::GetDelta(Origin, Target);
				return FRotator::ApplyDelta(Origin, Delta).Equals(Target, 0.05f) ? 1 : 0;
			}

			int RotatorRelativeRoundTrips()
			{
				const FRotator Parent = FRotator(0.0f, 45.0f, 0.0f);
				const FRotator Child = FRotator(20.0f, 90.0f, 0.0f);
				const FRotator Relative = FRotator::GetRelative(Parent, Child);
				return FRotator::ApplyRelative(Parent, Relative).Equals(Child, 0.05f) ? 1 : 0;
			}

			int TransformDeltaRoundTrips()
			{
				const FTransform Origin = FTransform(FRotator(0.0f, 15.0f, 0.0f), FVector(10.0f, 0.0f, 0.0f), FVector::OneVector);
				const FTransform Target = FTransform(FRotator(10.0f, 75.0f, 5.0f), FVector(25.0f, -5.0f, 2.0f), FVector::OneVector);
				const FTransform Delta = FTransform::GetDelta(Origin, Target);
				return FTransform::ApplyDelta(Origin, Delta).Equals(Target, 0.01f) ? 1 : 0;
			}

			int TransformRelativeRoundTrips()
			{
				const FTransform Parent = FTransform(FRotator(0.0f, 45.0f, 0.0f), FVector(100.0f, 0.0f, 0.0f), FVector::OneVector);
				const FTransform Child = FTransform(FRotator(20.0f, 90.0f, 0.0f), FVector(150.0f, 40.0f, 10.0f), FVector(1.0f, 2.0f, 1.0f));
				const FTransform Relative = FTransform::GetRelative(Parent, Child);
				return FTransform::ApplyRelative(Parent, Relative).Equals(Child, 0.01f) ? 1 : 0;
			}
			)AS"));
		if (!Mod.IsValid()) return;
		asIScriptModule& Module = Mod.GetModule();

		ASSERT_THAT(IsTrue(
			ExpectGlobalInt(*TestRunner, Engine, Module,  TEXT("int QuatDeltaRoundTrips()"), TEXT("FQuat::GetDelta/ApplyDelta should round-trip target rotations"), 1),
			TEXT("ExpectGlobalInt should pass")));
		ASSERT_THAT(IsTrue(
			ExpectGlobalInt(*TestRunner, Engine, Module,  TEXT("int QuatRelativeRoundTrips()"), TEXT("FQuat::GetRelative/ApplyRelative should round-trip child rotations"), 1),
			TEXT("ExpectGlobalInt should pass")));
		ASSERT_THAT(IsTrue(
			ExpectGlobalInt(*TestRunner, Engine, Module,  TEXT("int QuatAngularVelocityRoundTrips()"), TEXT("FQuat angular velocity helpers should round-trip axis speed"), 1),
			TEXT("ExpectGlobalInt should pass")));
		ASSERT_THAT(IsTrue(
			ExpectGlobalInt(*TestRunner, Engine, Module,  TEXT("int RotatorDeltaRoundTrips()"), TEXT("FRotator::GetDelta/ApplyDelta should round-trip target rotations"), 1),
			TEXT("ExpectGlobalInt should pass")));
		ASSERT_THAT(IsTrue(
			ExpectGlobalInt(*TestRunner, Engine, Module,  TEXT("int RotatorRelativeRoundTrips()"), TEXT("FRotator::GetRelative/ApplyRelative should round-trip child rotations"), 1),
			TEXT("ExpectGlobalInt should pass")));
		ASSERT_THAT(IsTrue(
			ExpectGlobalInt(*TestRunner, Engine, Module,  TEXT("int TransformDeltaRoundTrips()"), TEXT("FTransform::GetDelta/ApplyDelta should round-trip target transforms"), 1),
			TEXT("ExpectGlobalInt should pass")));
		ASSERT_THAT(IsTrue(
			ExpectGlobalInt(*TestRunner, Engine, Module,  TEXT("int TransformRelativeRoundTrips()"), TEXT("FTransform::GetRelative/ApplyRelative should round-trip child transforms"), 1),
			TEXT("ExpectGlobalInt should pass")));
	}
};

#endif
