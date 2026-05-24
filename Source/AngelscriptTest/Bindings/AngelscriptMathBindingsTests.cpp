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
//   Two IMPLEMENT_SIMPLE_AUTOMATION_TEST merged into one TEST_CLASS.
//   The custom helper namespace (ExecuteValueFunction, VerifyRotator, etc.) is
//   retained as-is because these tests return struct types (FRotator, FVector,
//   FTransform) via GetAddressOfReturnValue.
// ============================================================================

#include "CQTest.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Shared/AngelscriptTestUtilities.h"
#include "Shared/AngelscriptTestEngineHelper.h"
#include "Shared/AngelscriptBindingsCoverage.h"
#include "Shared/AngelscriptBindingsModuleBuilder.h"
#include "Shared/AngelscriptBindingsAssertions.h"

#include "Math/Quat.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;
using namespace AngelscriptTestBindings;
using namespace AngelscriptReflectiveAccess;

// ----------------------------------------------------------------------------
// Profile
// ----------------------------------------------------------------------------

static const FBindingsCoverageProfile GMathBindingsProfile{
	TEXT("Math"),                // Theme
	TEXT(""),                    // Variant
	TEXT("ASMathBindings"),      // ModulePrefix
	TEXT("Math"),                // CasePrefix
	TEXT("MathBindings"),        // LogCategory
};

// ----------------------------------------------------------------------------
// Helpers (retained from original)
// ----------------------------------------------------------------------------

namespace AngelscriptTest_Bindings_AngelscriptMathBindingsTests_Private
{
	bool ReadReturnValue(FAutomationTestBase&, asIScriptContext& Context, float& OutValue) { OutValue = Context.GetReturnFloat(); return true; }
	bool ReadReturnValue(FAutomationTestBase&, asIScriptContext& Context, double& OutValue) { OutValue = Context.GetReturnDouble(); return true; }

	template <typename TValue>
	bool ReadReturnValue(FAutomationTestBase& Test, asIScriptContext& Context, TValue& OutValue)
	{
		void* ReturnValueAddress = Context.GetAddressOfReturnValue();
		return Test.TestNotNull(TEXT("Math binding test should expose the return value storage"), ReturnValueAddress) && (OutValue = *static_cast<TValue*>(ReturnValueAddress), true);
	}

	template <typename TValue>
	bool ExecuteValueFunction(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		asIScriptFunction& Function,
		TValue& OutValue)
	{
		asIScriptContext* Context = Engine.CreateContext();
		if (!Test.TestNotNull(TEXT("Math binding test should create an execution context"), Context))
		{
			return false;
		}

		const int PrepareResult = Context->Prepare(&Function);
		if (!Test.TestEqual(TEXT("Math binding test should prepare the script function"), PrepareResult, asSUCCESS))
		{
			Context->Release();
			return false;
		}

		const int ExecuteResult = Context->Execute();
		if (!Test.TestEqual(TEXT("Math binding test should execute the script function"), ExecuteResult, asEXECUTION_FINISHED))
		{
			if (ExecuteResult == asEXECUTION_EXCEPTION && Context->GetExceptionString() != nullptr)
			{
				Test.AddError(FString::Printf(
					TEXT("Math binding test saw a script exception: %s"),
					UTF8_TO_TCHAR(Context->GetExceptionString())));
			}
			Context->Release();
			return false;
		}

		const bool bReadReturnValue = ReadReturnValue(Test, *Context, OutValue);
		Context->Release();
		return bReadReturnValue;
	}

	bool RotatorMatches(const FRotator& Actual, const FRotator& Expected, double ToleranceDegrees = 0.05)
	{
		FQuat ActualQuat(Actual);
		FQuat ExpectedQuat(Expected);
		ActualQuat.Normalize();
		ExpectedQuat.Normalize();
		return FMath::RadiansToDegrees(ActualQuat.AngularDistance(ExpectedQuat)) <= ToleranceDegrees;
	}

	bool VerifyRotator(
		FAutomationTestBase& Test,
		const TCHAR* What,
		const FRotator& Actual,
		const FRotator& Expected,
		double ToleranceDegrees = 0.05)
	{
		return Test.TestTrue(
			What,
			RotatorMatches(Actual, Expected, ToleranceDegrees));
	}

	bool VerifyVector(
		FAutomationTestBase& Test,
		const TCHAR* What,
		const FVector& Actual,
		const FVector& Expected,
		double Tolerance = KINDA_SMALL_NUMBER)
	{
		const bool bMatches = Actual.Equals(Expected, Tolerance);
		if (!bMatches)
		{
			Test.AddInfo(FString::Printf(TEXT("%s actual=%s expected=%s"), What, *Actual.ToCompactString(), *Expected.ToCompactString()));
		}
		return Test.TestTrue(What, bMatches);
	}

	bool VerifyVector3f(
		FAutomationTestBase& Test,
		const TCHAR* What,
		const FVector3f& Actual,
		const FVector3f& Expected,
		float Tolerance = KINDA_SMALL_NUMBER)
	{
		const bool bMatches = Actual.Equals(Expected, Tolerance);
		if (!bMatches)
		{
			Test.AddInfo(FString::Printf(TEXT("%s actual=%s expected=%s"), What, *Actual.ToString(), *Expected.ToString()));
		}
		return Test.TestTrue(What, bMatches);
	}

	template <typename TValue>
	bool VerifyNumeric(
		FAutomationTestBase& Test,
		const TCHAR* What,
		TValue Actual,
		TValue Expected,
		double Tolerance)
	{
		return Test.TestTrue(What, FMath::Abs(Actual - Expected) <= static_cast<TValue>(Tolerance));
	}

	bool VerifyTransform(
		FAutomationTestBase& Test,
		const TCHAR* What,
		const FTransform& Actual,
		const FTransform& Expected,
		double Tolerance = 0.01)
	{
		const bool bRotationMatches = RotatorMatches(Actual.Rotator(), Expected.Rotator(), Tolerance);
		const bool bTranslationMatches = Actual.GetLocation().Equals(Expected.GetLocation(), Tolerance);
		const bool bScaleMatches = Actual.GetScale3D().Equals(Expected.GetScale3D(), Tolerance);
		return Test.TestTrue(What, bRotationMatches && bTranslationMatches && bScaleMatches);
	}

	FRotator MakeShortestPathLerpReference(const FRotator& A, const FRotator& B, double Alpha)
	{
		FQuat Result = FQuat::Slerp(FQuat(A), FQuat(B), Alpha);
		Result.Normalize();
		return Result.Rotator();
	}

	FRotator MakeShortestPathInterpReference(const FRotator& Current, const FRotator& Target, float DeltaTime, float InterpSpeed)
	{
		FQuat Result = FMath::QInterpTo(FQuat(Current), FQuat(Target), DeltaTime, InterpSpeed);
		Result.Normalize();
		return Result.Rotator();
	}

	FRotator MakeShortestPathConstantInterpReference(const FRotator& Current, const FRotator& Target, float DeltaTime, float InterpSpeedDegrees)
	{
		FQuat Result = FMath::QInterpConstantTo(
			FQuat(Current),
			FQuat(Target),
			DeltaTime,
			FMath::DegreesToRadians(InterpSpeedDegrees));
		Result.Normalize();
		return Result.Rotator();
	}

	FTransform MakeTransformInterpReference(const FTransform& Current, const FTransform& Target, float DeltaTime, float InterpSpeed)
	{
		if (InterpSpeed <= 0.f)
		{
			return Target;
		}

		const float Alpha = FMath::Clamp(DeltaTime * InterpSpeed, 0.f, 1.f);

		FTransform Result;
		FTransform NormalizedCurrent = Current;
		FTransform NormalizedTarget = Target;
		NormalizedCurrent.NormalizeRotation();
		NormalizedTarget.NormalizeRotation();
		Result.Blend(NormalizedCurrent, NormalizedTarget, Alpha);
		return Result;
	}
}


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
		using namespace AngelscriptTest_Bindings_AngelscriptMathBindingsTests_Private;
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FCoverageModuleScope Mod(*TestRunner, Engine, GMathBindingsProfile, TEXT("ShortestPath"), TEXT(R"(
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

		asIScriptFunction* ShortestLerpFunction = GetFunctionByDecl(*TestRunner, Module, TEXT("FRotator GetShortestLerp()"));
		asIScriptFunction* ShortestInterpFunction = GetFunctionByDecl(*TestRunner, Module, TEXT("FRotator GetShortestInterp()"));
		asIScriptFunction* ShortestInterpConstantFunction = GetFunctionByDecl(*TestRunner, Module, TEXT("FRotator GetShortestInterpConstant()"));
		asIScriptFunction* ZeroSpeedTransformFunction = GetFunctionByDecl(*TestRunner, Module, TEXT("FTransform GetZeroSpeedTransform()"));
		asIScriptFunction* PositiveSpeedTransformFunction = GetFunctionByDecl(*TestRunner, Module, TEXT("FTransform GetPositiveSpeedTransform()"));
		asIScriptFunction* TransformedRotationFunction = GetFunctionByDecl(*TestRunner, Module, TEXT("FRotator GetTransformedRotation()"));
		asIScriptFunction* RoundTripRotationFunction = GetFunctionByDecl(*TestRunner, Module, TEXT("FRotator GetRoundTripRotation()"));
		asIScriptFunction* MoveSmallStepFunction = GetFunctionByDecl(*TestRunner, Module, TEXT("FVector GetMoveSmallStep()"));
		asIScriptFunction* MoveLargeStepFunction = GetFunctionByDecl(*TestRunner, Module, TEXT("FVector GetMoveLargeStep()"));
		if (ShortestLerpFunction == nullptr
			|| ShortestInterpFunction == nullptr
			|| ShortestInterpConstantFunction == nullptr
			|| ZeroSpeedTransformFunction == nullptr
			|| PositiveSpeedTransformFunction == nullptr
			|| TransformedRotationFunction == nullptr
			|| RoundTripRotationFunction == nullptr
			|| MoveSmallStepFunction == nullptr
			|| MoveLargeStepFunction == nullptr)
		{
			return;
		}

		const bool bExecutedAll =
			ExecuteValueFunction(*TestRunner, Engine, *ShortestLerpFunction, ScriptShortestLerp) &&
			ExecuteValueFunction(*TestRunner, Engine, *ShortestInterpFunction, ScriptShortestInterp) &&
			ExecuteValueFunction(*TestRunner, Engine, *ShortestInterpConstantFunction, ScriptShortestInterpConstant) &&
			ExecuteValueFunction(*TestRunner, Engine, *ZeroSpeedTransformFunction, ScriptZeroSpeedTransform) &&
			ExecuteValueFunction(*TestRunner, Engine, *PositiveSpeedTransformFunction, ScriptPositiveSpeedTransform) &&
			ExecuteValueFunction(*TestRunner, Engine, *TransformedRotationFunction, ScriptTransformedRotation) &&
			ExecuteValueFunction(*TestRunner, Engine, *RoundTripRotationFunction, ScriptRoundTripRotation) &&
			ExecuteValueFunction(*TestRunner, Engine, *MoveSmallStepFunction, ScriptMoveSmallStep) &&
			ExecuteValueFunction(*TestRunner, Engine, *MoveLargeStepFunction, ScriptMoveLargeStep);
		if (!bExecutedAll)
		{
			return;
		}

		const FRotator ExpectedShortestLerp = MakeShortestPathLerpReference(A, B, 0.5);
		const FRotator ExpectedShortestInterp = MakeShortestPathInterpReference(A, B, 0.5f, 4.0f);
		const FRotator ExpectedShortestInterpConstant = MakeShortestPathConstantInterpReference(A, B, 0.5f, 90.0f);
		const FTransform ExpectedZeroSpeedTransform = MakeTransformInterpReference(CurrentTransform, TargetTransform, 0.25f, 0.0f);
		const FTransform ExpectedPositiveSpeedTransform = MakeTransformInterpReference(CurrentTransform, TargetTransform, 0.25f, 2.0f);
		const FRotator ExpectedTransformedRotation = CurrentTransform.TransformRotation(LocalRotation.Quaternion()).Rotator();
		const FRotator ExpectedRoundTripRotation = CurrentTransform.InverseTransformRotation(ExpectedTransformedRotation.Quaternion()).Rotator();
		const FVector ExpectedMoveSmallStep = FMath::VInterpConstantTo(Start, Target, 3.0, 1.0f);
		const FVector ExpectedMoveLargeStep = FMath::VInterpConstantTo(Start, Target, 20.0, 1.0f);

		TestRunner->TestTrue(
			TEXT("Math::LerpShortestPath should stay near the 180-degree seam instead of crossing back toward zero"),
			FMath::Abs(FMath::FindDeltaAngleDegrees(ScriptShortestLerp.Yaw, 0.0f)) > 90.0f);
		VerifyRotator(*TestRunner, TEXT("Math::LerpShortestPath should match native quaternion slerp"), ScriptShortestLerp, ExpectedShortestLerp);
		VerifyRotator(*TestRunner, TEXT("Math::RInterpShortestPathTo should match native quaternion interp"), ScriptShortestInterp, ExpectedShortestInterp);
		VerifyRotator(*TestRunner, TEXT("Math::RInterpConstantShortestPathTo should match native constant-speed quaternion interp"), ScriptShortestInterpConstant, ExpectedShortestInterpConstant);
		VerifyTransform(*TestRunner, TEXT("Math::TInterpTo should return the target transform when InterpSpeed is zero"), ScriptZeroSpeedTransform, ExpectedZeroSpeedTransform);
		VerifyTransform(*TestRunner, TEXT("Math::TInterpTo should match native blend semantics for positive InterpSpeed"), ScriptPositiveSpeedTransform, ExpectedPositiveSpeedTransform);
		VerifyRotator(*TestRunner, TEXT("FTransform.TransformRotation should match native quaternion-based rotation transform"), ScriptTransformedRotation, ExpectedTransformedRotation);
		VerifyRotator(*TestRunner, TEXT("FTransform.InverseTransformRotation should round-trip the transformed rotator"), ScriptRoundTripRotation, ExpectedRoundTripRotation);
		VerifyRotator(*TestRunner, TEXT("FTransform rotation round-trip should recover the original local rotator"), ScriptRoundTripRotation, LocalRotation);
		VerifyVector(*TestRunner, TEXT("MoveTowards should advance by the fixed step distance when the target is farther away"), ScriptMoveSmallStep, ExpectedMoveSmallStep);
		VerifyVector(*TestRunner, TEXT("MoveTowards should clamp to the target when the requested step overshoots"), ScriptMoveLargeStep, ExpectedMoveLargeStep);
	}

	// ====================================================================
	// Section: PlanarProjectionAndColorFormatting
	// ====================================================================

	TEST_METHOD(PlanarProjectionAndColorFormatting)
	{
		using namespace AngelscriptTest_Bindings_AngelscriptMathBindingsTests_Private;
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FCoverageModuleScope Mod(*TestRunner, Engine, GMathBindingsProfile, TEXT("PlanarProjection"), TEXT(R"AS(
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

		const bool bExecutedAll =
			ExecuteValueFunction(*TestRunner, Engine, *GetFunctionByDecl(*TestRunner, Module, TEXT("double GetVectorSize2D()")), ScriptVectorSize2D) &&
			ExecuteValueFunction(*TestRunner, Engine, *GetFunctionByDecl(*TestRunner, Module, TEXT("double GetVectorSizeSquared2D()")), ScriptVectorSizeSquared2D) &&
			ExecuteValueFunction(*TestRunner, Engine, *GetFunctionByDecl(*TestRunner, Module, TEXT("FVector GetVectorProjected()")), ScriptVectorProjected) &&
			ExecuteValueFunction(*TestRunner, Engine, *GetFunctionByDecl(*TestRunner, Module, TEXT("double GetVectorDist2D()")), ScriptVectorDist2D) &&
			ExecuteValueFunction(*TestRunner, Engine, *GetFunctionByDecl(*TestRunner, Module, TEXT("double GetVectorDistSquared2D()")), ScriptVectorDistSquared2D) &&
			ExecuteValueFunction(*TestRunner, Engine, *GetFunctionByDecl(*TestRunner, Module, TEXT("FString GetVectorColorString()")), ScriptVectorColorString) &&
			ExecuteValueFunction(*TestRunner, Engine, *GetFunctionByDecl(*TestRunner, Module, TEXT("float32 GetVector3fSize2D()")), ScriptVector3fSize2D) &&
			ExecuteValueFunction(*TestRunner, Engine, *GetFunctionByDecl(*TestRunner, Module, TEXT("float32 GetVector3fSizeSquared2D()")), ScriptVector3fSizeSquared2D) &&
			ExecuteValueFunction(*TestRunner, Engine, *GetFunctionByDecl(*TestRunner, Module, TEXT("FVector3f GetVector3fProjected()")), ScriptVector3fProjected) &&
			ExecuteValueFunction(*TestRunner, Engine, *GetFunctionByDecl(*TestRunner, Module, TEXT("float32 GetVector3fDist2D()")), ScriptVector3fDist2D) &&
			ExecuteValueFunction(*TestRunner, Engine, *GetFunctionByDecl(*TestRunner, Module, TEXT("float32 GetVector3fDistSquared2D()")), ScriptVector3fDistSquared2D) &&
			ExecuteValueFunction(*TestRunner, Engine, *GetFunctionByDecl(*TestRunner, Module, TEXT("FString GetVector3fColorString()")), ScriptVector3fColorString);
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

		VerifyNumeric(*TestRunner, TEXT("FVector Size2D should match native planar length"), ScriptVectorSize2D, ExpectedVectorSize2D, KINDA_SMALL_NUMBER);
		VerifyNumeric(*TestRunner, TEXT("FVector SizeSquared2D should match native planar squared length"), ScriptVectorSizeSquared2D, ExpectedVectorSizeSquared2D, KINDA_SMALL_NUMBER);
		VerifyVector(*TestRunner, TEXT("FVector PointPlaneProject should match native projection"), ScriptVectorProjected, ExpectedVectorProjected);
		VerifyNumeric(*TestRunner, TEXT("FVector Dist2D should match native planar distance"), ScriptVectorDist2D, ExpectedVectorDist2D, KINDA_SMALL_NUMBER);
		VerifyNumeric(*TestRunner, TEXT("FVector DistSquared2D should match native planar squared distance"), ScriptVectorDistSquared2D, ExpectedVectorDistSquared2D, KINDA_SMALL_NUMBER);
		TestRunner->TestEqual(TEXT("FVector ToColorString should preserve the exact formatted debug string"), ScriptVectorColorString, ExpectedVectorColorString);
		VerifyNumeric(*TestRunner, TEXT("FVector3f Size2D should match native planar length"), ScriptVector3fSize2D, ExpectedVector3fSize2D, KINDA_SMALL_NUMBER);
		VerifyNumeric(*TestRunner, TEXT("FVector3f SizeSquared2D should match native planar squared length"), ScriptVector3fSizeSquared2D, ExpectedVector3fSizeSquared2D, KINDA_SMALL_NUMBER);
		VerifyVector3f(*TestRunner, TEXT("FVector3f PointPlaneProject should match native projection"), ScriptVector3fProjected, ExpectedVector3fProjected);
		VerifyNumeric(*TestRunner, TEXT("FVector3f Dist2D should match native planar distance"), ScriptVector3fDist2D, ExpectedVector3fDist2D, KINDA_SMALL_NUMBER);
		VerifyNumeric(*TestRunner, TEXT("FVector3f DistSquared2D should match native planar squared distance"), ScriptVector3fDistSquared2D, ExpectedVector3fDistSquared2D, KINDA_SMALL_NUMBER);
		TestRunner->TestEqual(TEXT("FVector3f ToColorString should preserve the exact formatted debug string"), ScriptVector3fColorString, ExpectedVector3fColorString);
	}
};

#endif
