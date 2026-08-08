// ============================================================================
// AngelscriptMathBindingsTests.cpp
//
// Math namespace / struct binding contract smoke. Numeric semantics live in
// Coverage (`02-math-structs`); this file only proves representative bindings
// resolve and dispatch from AS.
// ============================================================================

#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestExecute.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptMathBindingsTest,
	"Angelscript.TestModule.Bindings.Math",
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

	TEST_METHOD(MathNamespaceBindSmoke)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule ModuleScope(*TestRunner, Engine, TEXT("ASMath_BindContractSmoke"), ASTEST_AS(R"AS(
			int VerifyMathNamespaceBindSmoke()
			{
				const FRotator StartRotation = FRotator(0.0f, 170.0f, 0.0f);
				const FRotator TargetRotation = FRotator(0.0f, -170.0f, 0.0f);
				FRotator Shortest = Math::LerpShortestPath(StartRotation, TargetRotation, 0.5);

				const FTransform CurrentTransform = FTransform(
					FRotator(0.0f, 90.0f, 0.0f),
					FVector(10.0f, 0.0f, 0.0f),
					FVector::OneVector);
				const FTransform TargetTransform = FTransform(
					FRotator(0.0f, 180.0f, 0.0f),
					FVector(20.0f, 0.0f, 0.0f),
					FVector::OneVector);
				FTransform Interped = Math::TInterpTo(CurrentTransform, TargetTransform, 0.25f, 0.0f);

				const FVector Vector = FVector(3.0f, 4.0f, 12.0f);
				FVector Projected = Vector.PointPlaneProject(
					FVector(0.0f, 0.0f, 2.0f),
					FVector(0.0f, 0.0f, 1.0f));
				FString ColorString = FVector(1.0f, 0.5f, 0.25f).ToColorString();

				return Shortest.Yaw != 0.0f
					&& Interped.GetLocation().X != 0.0f
					&& Projected.Z == 2.0f
					&& ColorString.Contains("Red") ? 1 : 0;
			}
			)AS"));
		if (!ModuleScope.IsValid()) return;

		ASSERT_THAT(IsTrue(ExpectGlobalInt(
			*TestRunner,
			Engine,
			ModuleScope.GetModule(),
			TEXT("int VerifyMathNamespaceBindSmoke()"),
			TEXT("Math namespace and representative FVector/FTransform bindings should compile and dispatch"),
			1)));
	}

	TEST_METHOD(ManualCallableOwnerSurfaceDispatches)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString ScriptSource = ASTEST_AS(R"AS(
			int VerifyManualCallableOwnerSurface()
			{
				const float64 Eased = Math::EaseIn(0.0, 10.0, 0.5, 2.0);
				if (!Math::IsNearlyEqual(Eased, 2.5, 0.0001))
				{
					return 0;
				}

				if (!Math::IsPointInBox(FVector(1.0f, 2.0f, 3.0f), FVector::ZeroVector, FVector(5.0f, 5.0f, 5.0f)))
				{
					return 0;
				}

				FVector Segment1Point;
				FVector Segment2Point;
				Math::FindNearestPointsOnLineSegments(
					FVector(0.0f, 0.0f, 0.0f),
					FVector(10.0f, 0.0f, 0.0f),
					FVector(5.0f, 5.0f, 0.0f),
					FVector(5.0f, 10.0f, 0.0f),
					Segment1Point,
					Segment2Point);

				return Segment1Point.Equals(FVector(5.0f, 0.0f, 0.0f), 0.001f)
					&& Segment2Point.Equals(FVector(5.0f, 5.0f, 0.0f), 0.001f)
					&& Math::IntegerDivisionTrunc(7, 3) == 2 ? 1 : 0;
			}
			)AS");

		FScopedAngelscriptModule ModuleScope(
			*TestRunner,
			Engine,
			TEXT("ASMath_ManualCallableOwnerSurface"),
			ScriptSource);
		ASSERT_THAT(IsTrue(ModuleScope.IsValid(), TEXT("Manual FMath callable owner surface should compile")));
		ASSERT_THAT(IsTrue(ExpectGlobalInt(
			*TestRunner,
			Engine,
			ModuleScope.GetModule(),
			TEXT("int VerifyManualCallableOwnerSurface()"),
			TEXT("Manual FMath callable owner functions should dispatch through their bindings"),
			1)));
	}
};

#endif
