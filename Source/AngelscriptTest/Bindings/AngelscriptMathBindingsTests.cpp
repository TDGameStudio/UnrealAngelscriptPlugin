// ============================================================================
// AngelscriptMathBindingsTests.cpp
//
// Math binding coverage -- CQTest refactor. Automation IDs:
//   Angelscript.TestModule.Bindings.Math.FAngelscriptMathBindingsTest.*
//
// Sections:
//   ShortestPathAndTransformSemantics — quaternion lerp/interp, transform interp,
//                                       transform rotation, MoveTowards
//   PlanarProjectionAndColorFormatting — Size2D/Dist2D/PointPlaneProject/ToColorString
//                                        for FVector and FVector3f
//
// CQTest adaptation notes:
//   Two legacy automation tests merged into one TEST_CLASS.
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

#if WITH_DEV_AUTOMATION_TESTS


// ----------------------------------------------------------------------------
// Profile
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// Test class
// ----------------------------------------------------------------------------

TEST_CLASS_WITH_FLAGS(FAngelscriptMathBindingsTest,
	"Angelscript.TestModule.Bindings.Math",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	// ====================================================================
	// Section: ShortestPathAndTransformSemantics
	// ====================================================================

	TEST_METHOD(ShortestPathAndTransformSemantics)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASMath_ShortestPath"), TEXT(R"(
FRotator GetShortestLerp()
{
	const FRotator A = FRotator(0.0f, 170.0f, 0.0f);
	const FRotator B = FRotator(0.0f, -170.0f, 0.0f);
	return Math::LerpShortestPath(A, B, 0.5);
}

FRotator GetShortestInterp()
{
	const FRotator A = FRotator(0.0f, 170.0f, 0.0f);
	const FRotator B = FRotator(0.0f, -170.0f, 0.0f);
	return Math::RInterpShortestPathTo(A, B, 0.5f, 4.0f);
}

FRotator GetShortestInterpConstant()
{
	const FRotator A = FRotator(0.0f, 170.0f, 0.0f);
	const FRotator B = FRotator(0.0f, -170.0f, 0.0f);
	return Math::RInterpConstantShortestPathTo(A, B, 0.5f, 90.0f);
}

FTransform GetZeroSpeedTransform()
{
	const FTransform CurrentTransform = FTransform(FRotator(0.0f, 90.0f, 0.0f), FVector(10.0f, 0.0f, 0.0f), FVector::OneVector);
	const FTransform TargetTransform = FTransform(FRotator(0.0f, 180.0f, 0.0f), FVector(20.0f, 0.0f, 0.0f), FVector::OneVector);
	return Math::TInterpTo(CurrentTransform, TargetTransform, 0.25f, 0.0f);
}

FTransform GetPositiveSpeedTransform()
{
	const FTransform CurrentTransform = FTransform(FRotator(0.0f, 90.0f, 0.0f), FVector(10.0f, 0.0f, 0.0f), FVector::OneVector);
	const FTransform TargetTransform = FTransform(FRotator(0.0f, 180.0f, 0.0f), FVector(20.0f, 0.0f, 0.0f), FVector::OneVector);
	return Math::TInterpTo(CurrentTransform, TargetTransform, 0.25f, 2.0f);
}

FRotator GetTransformedRotation()
{
	const FTransform CurrentTransform = FTransform(FRotator(0.0f, 90.0f, 0.0f), FVector(10.0f, 0.0f, 0.0f), FVector::OneVector);
	const FRotator LocalRotation = FRotator(10.0f, 20.0f, 30.0f);
	return CurrentTransform.TransformRotation(LocalRotation);
}

FRotator GetRoundTripRotation()
{
	const FTransform CurrentTransform = FTransform(FRotator(0.0f, 90.0f, 0.0f), FVector(10.0f, 0.0f, 0.0f), FVector::OneVector);
	const FRotator LocalRotation = FRotator(10.0f, 20.0f, 30.0f);
	return CurrentTransform.InverseTransformRotation(CurrentTransform.TransformRotation(LocalRotation));
}

FVector GetMoveSmallStep()
{
	const FVector Start = FVector::ZeroVector;
	return Start.MoveTowards(FVector(10.0f, 0.0f, 0.0f), 3.0);
}

FVector GetMoveLargeStep()
{
	const FVector Start = FVector::ZeroVector;
	return Start.MoveTowards(FVector(10.0f, 0.0f, 0.0f), 20.0);
}
)"));
		if (!Mod.IsValid()) return;
		asIScriptModule& Module = Mod.GetModule();

		const FRotator A(0.0f, 170.0f, 0.0f);
		const FRotator B(0.0f, -170.0f, 0.0f);
		const FTransform CurrentTransform(FRotator(0.0f, 90.0f, 0.0f), FVector(10.0f, 0.0f, 0.0f), FVector::OneVector);
		const FTransform TargetTransform(FRotator(0.0f, 180.0f, 0.0f), FVector(20.0f, 0.0f, 0.0f), FVector::OneVector);
		const FRotator LocalRotation(10.0f, 20.0f, 30.0f);
		const FVector Start = FVector::ZeroVector;
		const FVector Target = FVector(10.0f, 0.0f, 0.0f);

		FRotator ScriptShortestLerp;
		FRotator ScriptShortestInterp;
		FRotator ScriptShortestInterpConstant;
		FTransform ScriptZeroSpeedTransform;
		FTransform ScriptPositiveSpeedTransform;
		FRotator ScriptTransformedRotation;
		FRotator ScriptRoundTripRotation;
		FVector ScriptMoveSmallStep;
		FVector ScriptMoveLargeStep;

		const auto ExecuteStructGlobal = [&](const TCHAR* FunctionDecl, auto& OutValue)
		{
			FAngelscriptTestExecutor Executor(*TestRunner, Engine, Module, FunctionDecl);
			return Executor.ExecuteAndExtractStruct(OutValue);
		};

		const bool bExecutedAll =
			ExecuteStructGlobal(TEXT("FRotator GetShortestLerp()"), ScriptShortestLerp) &&
			ExecuteStructGlobal(TEXT("FRotator GetShortestInterp()"), ScriptShortestInterp) &&
			ExecuteStructGlobal(TEXT("FRotator GetShortestInterpConstant()"), ScriptShortestInterpConstant) &&
			ExecuteStructGlobal(TEXT("FTransform GetZeroSpeedTransform()"), ScriptZeroSpeedTransform) &&
			ExecuteStructGlobal(TEXT("FTransform GetPositiveSpeedTransform()"), ScriptPositiveSpeedTransform) &&
			ExecuteStructGlobal(TEXT("FRotator GetTransformedRotation()"), ScriptTransformedRotation) &&
			ExecuteStructGlobal(TEXT("FRotator GetRoundTripRotation()"), ScriptRoundTripRotation) &&
			ExecuteStructGlobal(TEXT("FVector GetMoveSmallStep()"), ScriptMoveSmallStep) &&
			ExecuteStructGlobal(TEXT("FVector GetMoveLargeStep()"), ScriptMoveLargeStep);
		if (!bExecutedAll)
		{
			return;
		}

		const FRotator ExpectedShortestLerp = MakeMathBindingsShortestPathLerpReference(A, B, 0.5);
		const FRotator ExpectedShortestInterp = MakeMathBindingsShortestPathInterpReference(A, B, 0.5f, 4.0f);
		const FRotator ExpectedShortestInterpConstant = MakeMathBindingsShortestPathConstantInterpReference(A, B, 0.5f, 90.0f);
		const FTransform ExpectedZeroSpeedTransform = MakeMathBindingsTransformInterpReference(CurrentTransform, TargetTransform, 0.25f, 0.0f);
		const FTransform ExpectedPositiveSpeedTransform = MakeMathBindingsTransformInterpReference(CurrentTransform, TargetTransform, 0.25f, 2.0f);
		const FRotator ExpectedTransformedRotation = CurrentTransform.TransformRotation(LocalRotation.Quaternion()).Rotator();
		const FRotator ExpectedRoundTripRotation = CurrentTransform.InverseTransformRotation(ExpectedTransformedRotation.Quaternion()).Rotator();
		const FVector ExpectedMoveSmallStep = FMath::VInterpConstantTo(Start, Target, 3.0, 1.0f);
		const FVector ExpectedMoveLargeStep = FMath::VInterpConstantTo(Start, Target, 20.0, 1.0f);

		ASSERT_THAT(IsTrue(
			FMath::Abs(FMath::FindDeltaAngleDegrees(ScriptShortestLerp.Yaw, 0.0f)) > 90.0f,
			TEXT("Math::LerpShortestPath should stay near the 180-degree seam instead of crossing back toward zero")));
		VerifyMathBindingsRotator(*TestRunner, TEXT("Math::LerpShortestPath should match native quaternion slerp"), ScriptShortestLerp, ExpectedShortestLerp);
		VerifyMathBindingsRotator(*TestRunner, TEXT("Math::RInterpShortestPathTo should match native quaternion interp"), ScriptShortestInterp, ExpectedShortestInterp);
		VerifyMathBindingsRotator(*TestRunner, TEXT("Math::RInterpConstantShortestPathTo should match native constant-speed quaternion interp"), ScriptShortestInterpConstant, ExpectedShortestInterpConstant);
		VerifyMathBindingsTransform(*TestRunner, TEXT("Math::TInterpTo should return the target transform when InterpSpeed is zero"), ScriptZeroSpeedTransform, ExpectedZeroSpeedTransform);
		VerifyMathBindingsTransform(*TestRunner, TEXT("Math::TInterpTo should match native blend semantics for positive InterpSpeed"), ScriptPositiveSpeedTransform, ExpectedPositiveSpeedTransform);
		VerifyMathBindingsRotator(*TestRunner, TEXT("FTransform.TransformRotation should match native quaternion-based rotation transform"), ScriptTransformedRotation, ExpectedTransformedRotation);
		VerifyMathBindingsRotator(*TestRunner, TEXT("FTransform.InverseTransformRotation should round-trip the transformed rotator"), ScriptRoundTripRotation, ExpectedRoundTripRotation);
		VerifyMathBindingsRotator(*TestRunner, TEXT("FTransform rotation round-trip should recover the original local rotator"), ScriptRoundTripRotation, LocalRotation);
		VerifyMathBindingsVector(*TestRunner, TEXT("MoveTowards should advance by the fixed step distance when the target is farther away"), ScriptMoveSmallStep, ExpectedMoveSmallStep);
		VerifyMathBindingsVector(*TestRunner, TEXT("MoveTowards should clamp to the target when the requested step overshoots"), ScriptMoveLargeStep, ExpectedMoveLargeStep);
	}

	// ====================================================================
	// Section: PlanarProjectionAndColorFormatting
	// ====================================================================

	TEST_METHOD(PlanarProjectionAndColorFormatting)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASMath_PlanarProjection"), TEXT(R"AS(
double GetVectorSize2D() { const FVector V = FVector(3.0f, 4.0f, 12.0f); return V.Size2D(FVector(0.0f, 0.0f, 1.0f)); }
double GetVectorSizeSquared2D() { const FVector V = FVector(3.0f, 4.0f, 12.0f); return V.SizeSquared2D(FVector(0.0f, 0.0f, 1.0f)); }
FVector GetVectorProjected() { const FVector V = FVector(3.0f, 4.0f, 12.0f); return V.PointPlaneProject(FVector(0.0f, 0.0f, 2.0f), FVector(0.0f, 0.0f, 1.0f)); }
double GetVectorDist2D() { const FVector V = FVector(3.0f, 4.0f, 12.0f); return V.Dist2D(FVector(0.0f, 0.0f, 12.0f), FVector(0.0f, 0.0f, 1.0f)); }
double GetVectorDistSquared2D() { const FVector V = FVector(3.0f, 4.0f, 12.0f); return V.DistSquared2D(FVector(0.0f, 0.0f, 12.0f), FVector(0.0f, 0.0f, 1.0f)); }
FString GetVectorColorString() { const FVector V = FVector(1.0f, 0.5f, 0.25f); return V.ToColorString(); }

float32 GetVector3fSize2D() { const FVector3f V = FVector3f(3.0f, 4.0f, 12.0f); return V.Size2D(FVector3f(0.0f, 0.0f, 1.0f)); }
float32 GetVector3fSizeSquared2D() { const FVector3f V = FVector3f(3.0f, 4.0f, 12.0f); return V.SizeSquared2D(FVector3f(0.0f, 0.0f, 1.0f)); }
FVector3f GetVector3fProjected() { const FVector3f V = FVector3f(3.0f, 4.0f, 12.0f); return V.PointPlaneProject(FVector3f(0.0f, 0.0f, 2.0f), FVector3f(0.0f, 0.0f, 1.0f)); }
float32 GetVector3fDist2D() { const FVector3f V = FVector3f(3.0f, 4.0f, 12.0f); return V.Dist2D(FVector3f(0.0f, 0.0f, 12.0f), FVector3f(0.0f, 0.0f, 1.0f)); }
float32 GetVector3fDistSquared2D() { const FVector3f V = FVector3f(3.0f, 4.0f, 12.0f); return V.DistSquared2D(FVector3f(0.0f, 0.0f, 12.0f), FVector3f(0.0f, 0.0f, 1.0f)); }
FString GetVector3fColorString() { const FVector3f V = FVector3f(1.0f, 0.5f, 0.25f); return V.ToColorString(); }
)AS"));
		if (!Mod.IsValid()) return;
		asIScriptModule& Module = Mod.GetModule();

		const FVector Vector(3.0f, 4.0f, 12.0f);
		const FVector Other(0.0f, 0.0f, 12.0f);
		const FVector UpDirection(0.0f, 0.0f, 1.0f);
		const FVector PlaneBase(0.0f, 0.0f, 2.0f);
		const FVector PlaneNormal(0.0f, 0.0f, 1.0f);
		const FVector ColorVector(1.0f, 0.5f, 0.25f);

		const FVector3f Vector3f(3.0f, 4.0f, 12.0f), Other3f(0.0f, 0.0f, 12.0f), UpDirection3f(0.0f, 0.0f, 1.0f);
		const FVector3f PlaneBase3f(0.0f, 0.0f, 2.0f), PlaneNormal3f(0.0f, 0.0f, 1.0f), ColorVector3f(1.0f, 0.5f, 0.25f);

		double ScriptVectorSize2D = 0.0, ScriptVectorSizeSquared2D = 0.0, ScriptVectorDist2D = 0.0, ScriptVectorDistSquared2D = 0.0;
		float ScriptVector3fSize2D = 0.0f, ScriptVector3fSizeSquared2D = 0.0f, ScriptVector3fDist2D = 0.0f, ScriptVector3fDistSquared2D = 0.0f;
		FVector ScriptVectorProjected = FVector::ZeroVector;
		FVector3f ScriptVector3fProjected(ForceInitToZero);
		FString ScriptVectorColorString, ScriptVector3fColorString;

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
			ExecuteScalarGlobal(TEXT("double GetVectorSize2D()"), ScriptVectorSize2D) &&
			ExecuteScalarGlobal(TEXT("double GetVectorSizeSquared2D()"), ScriptVectorSizeSquared2D) &&
			ExecuteStructGlobal(TEXT("FVector GetVectorProjected()"), ScriptVectorProjected) &&
			ExecuteScalarGlobal(TEXT("double GetVectorDist2D()"), ScriptVectorDist2D) &&
			ExecuteScalarGlobal(TEXT("double GetVectorDistSquared2D()"), ScriptVectorDistSquared2D) &&
			ExecuteStructGlobal(TEXT("FString GetVectorColorString()"), ScriptVectorColorString) &&
			ExecuteScalarGlobal(TEXT("float32 GetVector3fSize2D()"), ScriptVector3fSize2D) &&
			ExecuteScalarGlobal(TEXT("float32 GetVector3fSizeSquared2D()"), ScriptVector3fSizeSquared2D) &&
			ExecuteStructGlobal(TEXT("FVector3f GetVector3fProjected()"), ScriptVector3fProjected) &&
			ExecuteScalarGlobal(TEXT("float32 GetVector3fDist2D()"), ScriptVector3fDist2D) &&
			ExecuteScalarGlobal(TEXT("float32 GetVector3fDistSquared2D()"), ScriptVector3fDistSquared2D) &&
			ExecuteStructGlobal(TEXT("FString GetVector3fColorString()"), ScriptVector3fColorString);
		if (!bExecutedAll)
		{
			return;
		}

		const FVector ExpectedVectorPlanar = FVector::VectorPlaneProject(Vector, UpDirection);
		const FVector ExpectedOtherPlanar = FVector::VectorPlaneProject(Other, UpDirection);
		const FVector ExpectedVectorProjected = FVector::PointPlaneProject(Vector, PlaneBase, PlaneNormal);
		const double ExpectedVectorSize2D = ExpectedVectorPlanar.Size();
		const double ExpectedVectorSizeSquared2D = ExpectedVectorPlanar.SizeSquared();
		const double ExpectedVectorDistSquared2D = FVector::DistSquared(ExpectedVectorPlanar, ExpectedOtherPlanar);
		const double ExpectedVectorDist2D = FMath::Sqrt(ExpectedVectorDistSquared2D);
		const FString ExpectedVectorColorString = FString::Printf(TEXT("<Red>X=%3.3f </><Green>Y=%3.3f </><Blue>Z=%3.3f </>"), ColorVector.X, ColorVector.Y, ColorVector.Z);

		const FVector3f ExpectedVector3fPlanar = FVector3f::VectorPlaneProject(Vector3f, UpDirection3f);
		const FVector3f ExpectedOther3fPlanar = FVector3f::VectorPlaneProject(Other3f, UpDirection3f);
		const FVector3f ExpectedVector3fProjected = FVector3f::PointPlaneProject(Vector3f, PlaneBase3f, PlaneNormal3f);
		const float ExpectedVector3fSize2D = ExpectedVector3fPlanar.Size();
		const float ExpectedVector3fSizeSquared2D = ExpectedVector3fPlanar.SizeSquared();
		const float ExpectedVector3fDistSquared2D = FVector3f::DistSquaredXY(ExpectedVector3fPlanar, ExpectedOther3fPlanar);
		const float ExpectedVector3fDist2D = FMath::Sqrt(ExpectedVector3fDistSquared2D);
		const FString ExpectedVector3fColorString = FString::Printf(TEXT("<Red>X=%3.3f </><Green>Y=%3.3f </><Blue>Z=%3.3f </>"), ColorVector3f.X, ColorVector3f.Y, ColorVector3f.Z);

		VerifyMathBindingsNumeric(*TestRunner, TEXT("FVector Size2D should match native planar length"), ScriptVectorSize2D, ExpectedVectorSize2D, KINDA_SMALL_NUMBER);
		VerifyMathBindingsNumeric(*TestRunner, TEXT("FVector SizeSquared2D should match native planar squared length"), ScriptVectorSizeSquared2D, ExpectedVectorSizeSquared2D, KINDA_SMALL_NUMBER);
		VerifyMathBindingsVector(*TestRunner, TEXT("FVector PointPlaneProject should match native projection"), ScriptVectorProjected, ExpectedVectorProjected);
		VerifyMathBindingsNumeric(*TestRunner, TEXT("FVector Dist2D should match native planar distance"), ScriptVectorDist2D, ExpectedVectorDist2D, KINDA_SMALL_NUMBER);
		VerifyMathBindingsNumeric(*TestRunner, TEXT("FVector DistSquared2D should match native planar squared distance"), ScriptVectorDistSquared2D, ExpectedVectorDistSquared2D, KINDA_SMALL_NUMBER);
		ASSERT_THAT(AreEqual(ExpectedVectorColorString, ScriptVectorColorString, TEXT("FVector ToColorString should preserve the exact formatted debug string")));
		VerifyMathBindingsNumeric(*TestRunner, TEXT("FVector3f Size2D should match native planar length"), ScriptVector3fSize2D, ExpectedVector3fSize2D, KINDA_SMALL_NUMBER);
		VerifyMathBindingsNumeric(*TestRunner, TEXT("FVector3f SizeSquared2D should match native planar squared length"), ScriptVector3fSizeSquared2D, ExpectedVector3fSizeSquared2D, KINDA_SMALL_NUMBER);
		VerifyMathBindingsVector3f(*TestRunner, TEXT("FVector3f PointPlaneProject should match native projection"), ScriptVector3fProjected, ExpectedVector3fProjected);
		VerifyMathBindingsNumeric(*TestRunner, TEXT("FVector3f Dist2D should match native planar distance"), ScriptVector3fDist2D, ExpectedVector3fDist2D, KINDA_SMALL_NUMBER);
		VerifyMathBindingsNumeric(*TestRunner, TEXT("FVector3f DistSquared2D should match native planar squared distance"), ScriptVector3fDistSquared2D, ExpectedVector3fDistSquared2D, KINDA_SMALL_NUMBER);
		ASSERT_THAT(AreEqual(ExpectedVector3fColorString, ScriptVector3fColorString, TEXT("FVector3f ToColorString should preserve the exact formatted debug string")));
	}
};

#endif
