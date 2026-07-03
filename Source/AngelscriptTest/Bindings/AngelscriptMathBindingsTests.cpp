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
};

#endif
