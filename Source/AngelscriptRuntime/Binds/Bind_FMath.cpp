#include "AngelscriptBinds.h"
#include "AngelscriptSettings.h"
#include "Bind_FMath_Functions.h"
#include "Kismet/KismetMathLibrary.h"

static void BindFMath(FAngelscriptBinds& Binds)
{
	const bool bDeprecateDouble = UAngelscriptSettings::Get().bDeprecateDoubleType;

	FAngelscriptBinds::FNamespace Namespace(
		Binds.GetTargetEngine(),
		(UAngelscriptSettings::Get().MathNamespace == EAngelscriptMathNamespace::Math)
		? "Math": "FMath"
	);

	Binds.BindGlobalFunctionForTarget("int32 RandHelper(int32 Max) no_discard", FUNCPR_TRIVIAL(int32, FMath::RandHelper, (int32)));
	Binds.BindGlobalFunctionForTarget("int32 RandRange(int32 Min, int32 Max) no_discard", FUNCPR_TRIVIAL(int32, FMath::RandRange, (int32, int32)))
		.Documentation(TEXT("Returns a random integer >= Min and <= Max"));
	Binds.BindGlobalFunctionForTarget("float64 RandRange(float64 Min, float64 Max) no_discard", FUNCPR_TRIVIAL(double, FMath::RandRange, (double, double)));
	Binds.BindGlobalFunctionForTarget("float32 RandRange(float32 Min, float32 Max) no_discard", FUNCPR_TRIVIAL(float, FMath::RandRange, (float, float)));
	Binds.BindGlobalFunctionForTarget("bool RandBool() no_discard", FUNC_TRIVIAL(FMath::RandBool));
	Binds.BindGlobalFunctionForTarget("FVector VRand() no_discard", FUNC_TRIVIAL(FMath::VRand))
		.Documentation(TEXT("Returns a random vector with length of 1"));


	Binds.BindGlobalFunctionForTarget("FVector VRandCone(const FVector& DDir, float32 HorizontalConeHalfAngleRad, float32 VerticalConeHalfAngleRad) no_discard",
		FUNCPR_TRIVIAL(FVector, FMath::VRandCone, (const FVector&, float, float)))
		.Documentation(TEXT(
			"Returns a random unit vector, uniformly distributed, within the specified cone\n"
			"ConeHalfAngleRad is the half-angle of cone, in radians.  Returns a normalized vector."));

	Binds.BindGlobalFunctionForTarget("FVector VRandCone(const FVector& DDir, float32 ConeHalfAngleRad) no_discard",
		FUNCPR_TRIVIAL(FVector, FMath::VRandCone, (const FVector&, float)));
	
	Binds.BindGlobalFunctionForTarget("FVector2D RandPointInCircle(float32 Radius) no_discard",
		FUNCPR_TRIVIAL(FVector2D, FMath::RandPointInCircle, (float)))
		.Documentation(TEXT("Get a random point on a unit circle, evenly spread across the circumference."));

	Binds.BindGlobalFunctionForTarget("FVector GetReflectionVector(const FVector& Direction, const FVector& SurfaceNormal) no_discard", FUNC_TRIVIAL(FMath::GetReflectionVector))
		.Documentation(TEXT(
			"Given a direction vector and a surface normal, returns the vector reflected across the surface normal.\n"
			"Produces a result like shining a laser at a mirror!\n"
			"@param Direction Direction vector the ray is coming from.\n"
			"@param SurfaceNormal A normal of the surface the ray should be reflected on.\n"
			"@returns Reflected vector."));

	Binds.BindGlobalFunctionForTarget("float32 MakePulsatingValue( const float64 InCurrentTime, const float32 InPulsesPerSecond, const float32 InPhase = 0.0f ) no_discard", FUNC_TRIVIAL(FMath::MakePulsatingValue))
		.Documentation(TEXT(
			"Simple function to create a pulsating scalar value\n"
			"@param  InCurrentTime  Current absolute time\n"
			"@param  InPulsesPerSecond  How many full pulses per second?\n"
			"@param  InPhase  Optional phase amount, between 0.0 and 1.0 (to synchronize pulses)\n"
			"@return  Pulsating value (0.0-1.0)"));

	Binds.BindGlobalFunctionForTarget("bool IsNearlyEqual(float64 A, float64 B, float64 ErrorTolerance = SMALL_NUMBER) no_discard",
		FUNCPR_TRIVIAL(bool, FMath::IsNearlyEqual, (double, double, double)));
	Binds.BindGlobalFunctionForTarget("bool IsNearlyEqual(float32 A, float32 B, float32 ErrorTolerance = SMALL_NUMBER) no_discard",
		FUNCPR_TRIVIAL(bool, FMath::IsNearlyEqual, (float, float, float)));

	Binds.BindGlobalFunctionForTarget("bool IsNearlyZero(float64 Value, float64 ErrorTolerance = SMALL_NUMBER) no_discard",
		FUNCPR_TRIVIAL(bool, FMath::IsNearlyZero, (double, double)));
	Binds.BindGlobalFunctionForTarget("bool IsNearlyZero(float32 Value, float32 ErrorTolerance = SMALL_NUMBER) no_discard",
		FUNCPR_TRIVIAL(bool, FMath::IsNearlyZero, (float, float)));

	Binds.BindGlobalFunctionForTarget("bool IsPowerOfTwo(int32 Value) no_discard", FUNC_TRIVIAL(FMath::IsPowerOfTwo<int32>));

#if ENGINE_MAJOR_VERSION >= 5
	Binds.BindGlobalFunctionForTarget("float64 SmoothStep(float64 A, float64 B, float64 X) no_discard", FUNCPR_TRIVIAL(double, FMath::SmoothStep, (double, double, double)))
		.Documentation(TEXT(
		"Returns a smooth Hermite interpolation between 0 and 1 for the value X (where X ranges between A and B)\n"
		"Clamped to 0 for X <= A and 1 for X >= B.\n"
		"@param A Minimum value of X\n"
		"@param B Maximum value of X\n"
		"@param X Parameter\n"
		"@return Smoothed value between 0 and 1\n"
		));
#endif

	Binds.BindGlobalFunctionForTarget("float32 SmoothStep(float32 A, float32 B, float32 X) no_discard", FUNCPR_TRIVIAL(float, FMath::SmoothStep, (float, float, float)))
		.Documentation(TEXT(
		"Returns a smooth Hermite interpolation between 0 and 1 for the value X (where X ranges between A and B)\n"
		"Clamped to 0 for X <= A and 1 for X >= B.\n"
		"@param A Minimum value of X\n"
		"@param B Maximum value of X\n"
		"@param X Parameter\n"
		"@return Smoothed value between 0 and 1\n"
		));

	Binds.BindGlobalFunctionForTarget("float64 Clamp(float64 X, float64 Min, float64 Max) no_discard", FUNC_TRIVIAL(FMath::Clamp<double>))
		.Documentation(TEXT("Clamps X to be between Min and Max, inclusive"));
	Binds.BindGlobalFunctionForTarget("float32 Clamp(float32 X, float32 Min, float32 Max) no_discard", FUNC_TRIVIAL(FMath::Clamp<float>))
		.Documentation(TEXT("Clamps X to be between Min and Max, inclusive"));
	Binds.BindGlobalFunctionForTarget("int32 Clamp(int32 X, int32 Min, int32 Max) no_discard", FUNC_TRIVIAL(FMath::Clamp<int32>))
		.Documentation(TEXT("Clamps X to be between Min and Max, inclusive"));
	Binds.BindGlobalFunctionForTarget("uint32 Clamp(uint32 X, uint32 Min, uint32 Max) no_discard", FUNC_TRIVIAL(FMath::Clamp<uint32>))
		.Documentation(TEXT("Clamps X to be between Min and Max, inclusive"));
	Binds.BindGlobalFunctionForTarget("int64 Clamp(int64 X, int64 Min, int64 Max) no_discard", FUNC_TRIVIAL(FMath::Clamp<int64>))
		.Documentation(TEXT("Clamps X to be between Min and Max, inclusive"));
	Binds.BindGlobalFunctionForTarget("uint64 Clamp(uint64 X, uint64 Min, uint64 Max) no_discard", FUNC_TRIVIAL(FMath::Clamp<uint64>))
		.Documentation(TEXT("Clamps X to be between Min and Max, inclusive"));

	Binds.BindGlobalFunctionForTarget("float64 FastAsin(float64 Value) no_discard", FUNCPR_TRIVIAL(double, FMath::FastAsin, (double)));
	Binds.BindGlobalFunctionForTarget("float32 FastAsin(float32 Value) no_discard", FUNCPR_TRIVIAL(float, FMath::FastAsin, (float)));

	Binds.BindGlobalFunctionForTarget("float64 RadiansToDegrees(const float64& RadVal) no_discard", FUNC_TRIVIAL(FMath::RadiansToDegrees<double>));
	Binds.BindGlobalFunctionForTarget("float64 DegreesToRadians(const float64& DegVal) no_discard", FUNC_TRIVIAL(FMath::DegreesToRadians<double>));

	Binds.BindGlobalFunctionForTarget("float32 RadiansToDegrees(const float32& RadVal) no_discard", FUNC_TRIVIAL(FMath::RadiansToDegrees<float>));
	Binds.BindGlobalFunctionForTarget("float32 DegreesToRadians(const float32& DegVal) no_discard", FUNC_TRIVIAL(FMath::DegreesToRadians<float>));

	Binds.BindGlobalFunctionForTarget("float32 ClampAngle(float32 AngleDegrees, float32 MinAngleDegrees, float32 MaxAngleDegrees) no_discard", FUNC_TRIVIAL(FMath::ClampAngle<float>));
	Binds.BindGlobalFunctionForTarget("float64 ClampAngle(float64 AngleDegrees, float64 MinAngleDegrees, float64 MaxAngleDegrees) no_discard", FUNC_TRIVIAL(FMath::ClampAngle<double>));

	Binds.BindGlobalFunctionForTarget("float64 FindDeltaAngleDegrees(float64 A1, float64 A2) no_discard", FUNCPR_TRIVIAL(double, FMath::FindDeltaAngleDegrees, (double, double)));
	Binds.BindGlobalFunctionForTarget("float64 FindDeltaAngleRadians(float64 A1, float64 A2) no_discard", FUNCPR_TRIVIAL(double, FMath::FindDeltaAngleRadians, (double, double)));

	Binds.BindGlobalFunctionForTarget("float32 FindDeltaAngleDegrees(float32 A1, float32 A2) no_discard", FUNCPR_TRIVIAL(float, FMath::FindDeltaAngleDegrees, (float, float)));
	Binds.BindGlobalFunctionForTarget("float32 FindDeltaAngleRadians(float32 A1, float32 A2) no_discard", FUNCPR_TRIVIAL(float, FMath::FindDeltaAngleRadians, (float, float)));

	Binds.BindGlobalFunctionForTarget("float64 UnwindDegrees(float64 A) no_discard", FUNCPR_TRIVIAL(double, FMath::UnwindDegrees, (double)))
		.Documentation(TEXT("Utility to ensure angle is between +/- 180 degrees by unwinding."));
	
	Binds.BindGlobalFunctionForTarget("float64 UnwindRadians(float64 A) no_discard", FUNCPR_TRIVIAL(double, FMath::UnwindRadians, (double)))
		.Documentation(TEXT("Utility to ensure angle is between +/- 180 degrees by unwinding."));

	Binds.BindGlobalFunctionForTarget("float32 UnwindDegrees(float32 A) no_discard", FUNCPR_TRIVIAL(float, FMath::UnwindDegrees, (float)))
		.Documentation(TEXT("Utility to ensure angle is between +/- 180 degrees by unwinding."));
	
	Binds.BindGlobalFunctionForTarget("float32 UnwindRadians(float32 A) no_discard", FUNCPR_TRIVIAL(float, FMath::UnwindRadians, (float)))
		.Documentation(TEXT("Utility to ensure angle is between +/- 180 degrees by unwinding."));
	
	Binds.BindGlobalFunctionForTarget(
		"float64 LerpStable(const float64& A, const float64& B, float64 Alpha) no_discard",
		FUNCPR_TRIVIAL(double, FMath::LerpStable<double>, (const double&, const double&, double))
	);

	Binds.BindGlobalFunctionForTarget(
		"float32 LerpStable(const float32& A, const float32& B, float32 Alpha) no_discard",
		FUNCPR_TRIVIAL(float, FMath::LerpStable<float>, (const float&, const float&, float))
	);

	// The helper macros break due to the , for stuff with multiple template arguments
#if AS_CAN_GENERATE_JIT
	Binds.BindGlobalFunctionForTarget("float64 Lerp(const float64& A, const float64& B, const float64& Alpha) no_discard", &FMath::Lerp<double, double>, "FMath::Lerp<double, double>", true);
	Binds.BindGlobalFunctionForTarget("float32 Lerp(const float32& A, const float32& B, const float32& Alpha) no_discard", &FMath::Lerp<float, float>, "FMath::Lerp<float, float>", true);

	Binds.BindGlobalFunctionForTarget("FVector Lerp(const FVector& A, const FVector& B, const float64& Alpha) no_discard", &FMath::Lerp<FVector, double>, "FMath::Lerp<FVector, double>", true);
	Binds.BindGlobalFunctionForTarget("FVector2D Lerp(const FVector2D& A, const FVector2D& B, const float64& Alpha) no_discard", &FMath::Lerp<FVector2D, double>, "FMath::Lerp<FVector2D, double>", true);

	Binds.BindGlobalFunctionForTarget("FVector3f Lerp(const FVector3f& A, const FVector3f& B, const float32& Alpha) no_discard", &FMath::Lerp<FVector3f, float>, "FMath::Lerp<FVector3f, float>", true);
	Binds.BindGlobalFunctionForTarget("FVector2f Lerp(const FVector2f& A, const FVector2f& B, const float32& Alpha) no_discard", &FMath::Lerp<FVector2f, float>, "FMath::Lerp<FVector2f, float>", true);

	Binds.BindGlobalFunctionForTarget("FVector VLerp(const FVector& A, const FVector& B, const FVector& Alpha) no_discard", &FMath::Lerp<FVector, FVector>, "FMath::Lerp<FVector, FVector>", true);
	Binds.BindGlobalFunctionForTarget("FLinearColor Lerp(const FLinearColor& A, const FLinearColor& B, const float32& Alpha) no_discard", &FMath::Lerp<FLinearColor, float>, "FMath::Lerp<FLinearColor, float>", true);

	Binds.BindGlobalFunctionForTarget("bool IsWithin(const float64& TestValue, const float64& MinValue, const float64& MaxValue) no_discard", &FMath::IsWithin<double, double>, "FMath::IsWithin<double, double>", true);
	Binds.BindGlobalFunctionForTarget("bool IsWithin(const float32& TestValue, const float32&  MinValue, const float32&  MaxValue) no_discard",& FMath::IsWithin<float, float>, "FMath::IsWithin<float, float>", true);
	Binds.BindGlobalFunctionForTarget("bool IsWithin(const int32& TestValue, const int32& MinValue, const int32& MaxValue) no_discard", &FMath::IsWithin<int32, int32>, "FMath::IsWithin<int32, int32>", true);

	Binds.BindGlobalFunctionForTarget("bool IsWithinInclusive(const float64& TestValue, const float64& MinValue, const float64& MaxValue) no_discard", &FMath::IsWithinInclusive<double, double>, "FMath::IsWithinInclusive<double, double>", true);
	Binds.BindGlobalFunctionForTarget("bool IsWithinInclusive(const float32& TestValue, const float32&  MinValue, const float32&  MaxValue) no_discard",& FMath::IsWithinInclusive<float, float>, "FMath::IsWithinInclusive<float, float>", true);
	Binds.BindGlobalFunctionForTarget("bool IsWithinInclusive(const int32& TestValue, const int32& MinValue, const int32& MaxValue) no_discard", &FMath::IsWithinInclusive<int32, int32>, "FMath::IsWithinInclusive<int32, int32>", true);
#else
	Binds.BindGlobalFunctionForTarget("float64 Lerp(const float64& A, const float64& B, const float64& Alpha) no_discard", &FMath::Lerp<double, double>);
	Binds.BindGlobalFunctionForTarget("float32 Lerp(const float32& A, const float32& B, const float32& Alpha) no_discard", &FMath::Lerp<float, float>);

	Binds.BindGlobalFunctionForTarget("FVector Lerp(const FVector& A, const FVector& B, const float64& Alpha) no_discard", &FMath::Lerp<FVector, double>);
	Binds.BindGlobalFunctionForTarget("FVector2D Lerp(const FVector2D& A, const FVector2D& B, const float64& Alpha) no_discard", &FMath::Lerp<FVector2D, double>);

	Binds.BindGlobalFunctionForTarget("FVector3f Lerp(const FVector3f& A, const FVector3f& B, const float32& Alpha) no_discard", &FMath::Lerp<FVector3f, float>);
	Binds.BindGlobalFunctionForTarget("FVector2f Lerp(const FVector2f& A, const FVector2f& B, const float32& Alpha) no_discard", &FMath::Lerp<FVector2f, float>);

	Binds.BindGlobalFunctionForTarget("FVector VLerp(const FVector& A, const FVector& B, const FVector& Alpha) no_discard", &FMath::Lerp<FVector, FVector>);
	Binds.BindGlobalFunctionForTarget("FLinearColor Lerp(const FLinearColor& A, const FLinearColor& B, const float32& Alpha) no_discard", &FMath::Lerp<FLinearColor, float>);

	Binds.BindGlobalFunctionForTarget("bool IsWithin(const float64& TestValue, const float64& MinValue, const float64& MaxValue) no_discard", &FMath::IsWithin<double, double>);
	Binds.BindGlobalFunctionForTarget("bool IsWithin(const float32& TestValue, const float32&  MinValue, const float32&  MaxValue) no_discard",& FMath::IsWithin<float, float>);
	Binds.BindGlobalFunctionForTarget("bool IsWithin(const int32& TestValue, const int32& MinValue, const int32& MaxValue) no_discard", &FMath::IsWithin<int32, int32>);

	Binds.BindGlobalFunctionForTarget("bool IsWithinInclusive(const float64& TestValue, const float64& MinValue, const float64& MaxValue) no_discard", &FMath::IsWithinInclusive<double, double>);
	Binds.BindGlobalFunctionForTarget("bool IsWithinInclusive(const float32& TestValue, const float32&  MinValue, const float32&  MaxValue) no_discard",& FMath::IsWithinInclusive<float, float>);
	Binds.BindGlobalFunctionForTarget("bool IsWithinInclusive(const int32& TestValue, const int32& MinValue, const int32& MaxValue) no_discard", &FMath::IsWithinInclusive<int32, int32>);
#endif

	Binds.BindGlobalFunctionForTarget("float64 CubicInterp(const float64& Point0, const float64& Tangent0, const float64& Point1, const float64& Tangent1, const float64& Alpha) no_discard", &FMath::CubicInterp<double, double>, "FMath::CubicInterp<double, double>", true);
	Binds.BindGlobalFunctionForTarget("FVector CubicInterp(const FVector& Point0, const FVector& Tangent0, const FVector& Point1, const FVector& Tangent1, const float64& Alpha) no_discard", &FMath::CubicInterp<FVector, double>, "FMath::CubicInterp<FVector, double>", true);
	Binds.BindGlobalFunctionForTarget("FQuat CubicInterp(const FQuat& Point0, const FQuat& Tangent0, const FQuat& Point1, const FQuat& Tangent1, const float64& Alpha) no_discard", &FMath::CubicInterp<FQuat,double>, "FMath::CubicInterp<FQuat,double>", true);

	Binds.BindGlobalFunctionForTarget("float32 CubicInterp(const float32& Point0, const float32& Tangent0, const float32& Point1, const float32& Tangent1, const float32& Alpha) no_discard", &FMath::CubicInterp<float, float>, "FMath::CubicInterp<float, float>", true);
	Binds.BindGlobalFunctionForTarget("FVector CubicInterp(const FVector& Point0, const FVector& Tangent0, const FVector& Point1, const FVector& Tangent1, const float32& Alpha) no_discard", &FMath::CubicInterp<FVector, float>, "FMath::CubicInterp<FVector, float>", true);
	Binds.BindGlobalFunctionForTarget("FQuat CubicInterp(const FQuat& Point0, const FQuat& Tangent0, const FQuat& Point1, const FQuat& Tangent1, const float32& Alpha) no_discard", &FMath::CubicInterp<FQuat,float>, "FMath::CubicInterp<FQuat,float>", true);

	Binds.BindGlobalFunctionForTarget("FVector3f CubicInterp(const FVector3f& Point0, const FVector3f& Tangent0, const FVector3f& Point1, const FVector3f& Tangent1, const float32& Alpha) no_discard", &FMath::CubicInterp<FVector3f, float>, "FMath::CubicInterp<FVector3f, float>", true);
	Binds.BindGlobalFunctionForTarget("FQuat4f CubicInterp(const FQuat4f& Point0, const FQuat4f& Tangent0, const FQuat4f& Point1, const FQuat4f& Tangent1, const float32& Alpha) no_discard", &FMath::CubicInterp<FQuat4f,float>, "FMath::CubicInterp<FQuat4f,float>", true);

	Binds.BindGlobalFunctionForTarget("float64 CubicInterpDerivative(const float64& Point0, const float64& Tangent0, const float64& Point1, const float64& Tangent1, const float64& Alpha) no_discard", &FMath::CubicInterpDerivative<double, double>, "FMath::CubicInterpDerivative<double, double>", true);
	Binds.BindGlobalFunctionForTarget("FVector CubicInterpDerivative(const FVector& Point0, const FVector& Tangent0, const FVector& Point1, const FVector& Tangent1, const float64& Alpha) no_discard", &FMath::CubicInterpDerivative<FVector, double>, "FMath::CubicInterpDerivative<FVector, double>", true);
	Binds.BindGlobalFunctionForTarget("FRotator CubicInterpDerivative(const FRotator& Point0, const FRotator& Tangent0, const FRotator& Point1, const FRotator& Tangent1, const float64& Alpha) no_discard", &FMath::CubicInterpDerivative<FRotator, double>, "FMath::CubicInterpDerivative<FRotator, double>", true);

	Binds.BindGlobalFunctionForTarget("float32 CubicInterpDerivative(const float32& Point0, const float32& Tangent0, const float32& Point1, const float32& Tangent1, const float32& Alpha) no_discard", &FMath::CubicInterpDerivative<float, float>, "FMath::CubicInterpDerivative<float, float>", true);
	Binds.BindGlobalFunctionForTarget("FVector CubicInterpDerivative(const FVector& Point0, const FVector& Tangent0, const FVector& Point1, const FVector& Tangent1, const float32& Alpha) no_discard", &FMath::CubicInterpDerivative<FVector, float>, "FMath::CubicInterpDerivative<FVector, float>", true);
	Binds.BindGlobalFunctionForTarget("FRotator CubicInterpDerivative(const FRotator& Point0, const FRotator& Tangent0, const FRotator& Point1, const FRotator& Tangent1, const float32& Alpha) no_discard", &FMath::CubicInterpDerivative<FRotator, float>, "FMath::CubicInterpDerivative<FRotator, float>", true);

	Binds.BindGlobalFunctionForTarget("FVector3f CubicInterpDerivative(const FVector3f& Point0, const FVector3f& Tangent0, const FVector3f& Point1, const FVector3f& Tangent1, const float32& Alpha) no_discard", &FMath::CubicInterpDerivative<FVector3f, float>, "FMath::CubicInterpDerivative<FVector3f, float>", true);
	Binds.BindGlobalFunctionForTarget("FRotator3f CubicInterpDerivative(const FRotator3f& Point0, const FRotator3f& Tangent0, const FRotator3f& Point1, const FRotator3f& Tangent1, const float32& Alpha) no_discard", &FMath::CubicInterpDerivative<FRotator3f, float>, "FMath::CubicInterpDerivative<FRotator3f, float>", true);

	Binds.BindGlobalFunctionForTarget("FVector VInterpNormalRotationTo(const FVector& Current, const FVector& Target, float32 DeltaTime, float32 RotationSpeedDegrees) no_discard", FUNC_TRIVIAL(FMath::VInterpNormalRotationTo));
	Binds.BindGlobalFunctionForTarget("FVector VInterpConstantTo(const FVector& Current, const FVector& Target, float32 DeltaTime, float32 InterpSpeed) no_discard", FUNC_TRIVIAL(FMath::VInterpConstantTo));
	Binds.BindGlobalFunctionForTarget("FVector VInterpTo(const FVector& Current, const FVector& Target, float32 DeltaTime, float32 InterpSpeed) no_discard", FUNC_TRIVIAL(FMath::VInterpTo));

	Binds.BindGlobalFunctionForTarget("FVector2D Vector2DInterpConstantTo(const FVector2D& Current, const FVector2D& Target, float32 DeltaTime, float32 InterpSpeed) no_discard", FUNC_TRIVIAL(FMath::Vector2DInterpConstantTo));
	Binds.BindGlobalFunctionForTarget("FVector2D Vector2DInterpTo(const FVector2D& Current, const FVector2D& Target, float32 DeltaTime, float32 InterpSpeed) no_discard", FUNCPR_TRIVIAL(FVector2D, FMath::Vector2DInterpTo, (const FVector2D&, const FVector2D&, float, float)));

	Binds.BindGlobalFunctionForTarget("FRotator RInterpConstantTo(const FRotator& Current, const FRotator& Target, float32 DeltaTime, float32 InterpSpeed) no_discard", FUNC_TRIVIAL(FMath::RInterpConstantTo));
	Binds.BindGlobalFunctionForTarget("FRotator RInterpTo(const FRotator& Current, const FRotator& Target, float32 DeltaTime, float32 InterpSpeed) no_discard", FUNC_TRIVIAL(FMath::RInterpTo));
	Binds.BindGlobalFunctionForTarget("FRotator RotatorFromAxisAndAngle(FVector Axis, float32 Angle) no_discard", FUNC_TRIVIAL(UKismetMathLibrary::RotatorFromAxisAndAngle));
	
	Binds.BindGlobalFunctionForTarget("FVector RandomPointInBoundingBox(const FVector& Center, const FVector& HalfSize) no_discard", FUNC_TRIVIAL(UKismetMathLibrary::RandomPointInBoundingBox));
	Binds.BindGlobalFunctionForTarget("FRotator RandomRotator(bool bRoll) no_discard", FUNC_TRIVIAL(UKismetMathLibrary::RandomRotator));

	Binds.BindGlobalFunctionForTarget("FQuat QInterpConstantTo(const FQuat& Current, const FQuat& Target, float32 DeltaTime, float32 InterpSpeed) no_discard", FUNC_TRIVIAL(FMath::QInterpConstantTo<double>));
	Binds.BindGlobalFunctionForTarget("FQuat QInterpTo(const FQuat& Current, const FQuat& Target, float32 DeltaTime, float32 InterpSpeed) no_discard", FUNC_TRIVIAL(FMath::QInterpTo<double>));

	Binds.BindGlobalFunctionForTarget("FQuat4f QInterpConstantTo(const FQuat4f& Current, const FQuat4f& Target, float32 DeltaTime, float32 InterpSpeed) no_discard", FUNC_TRIVIAL(FMath::QInterpConstantTo<float>));
	Binds.BindGlobalFunctionForTarget("FQuat4f QInterpTo(const FQuat4f& Current, const FQuat4f& Target, float32 DeltaTime, float32 InterpSpeed) no_discard", FUNC_TRIVIAL(FMath::QInterpTo<float>));

	Binds.BindGlobalFunctionForTarget("float32 FInterpConstantTo(float32 Current, float32 Target, float32 DeltaTime, float32 InterpSpeed) no_discard", FUNC_TRIVIAL(FMath::FInterpConstantTo<float>));
	Binds.BindGlobalFunctionForTarget("float32 FInterpTo(float32 Current, float32 Target, float32 DeltaTime, float32 InterpSpeed) no_discard", FUNC_TRIVIAL(FMath::FInterpTo<float>));

	Binds.BindGlobalFunctionForTarget("float64 FInterpConstantTo(float64 Current, float64 Target, float64 DeltaTime, float64 InterpSpeed) no_discard", FUNC_TRIVIAL(FMath::FInterpConstantTo<double>));
	Binds.BindGlobalFunctionForTarget("float64 FInterpTo(float64 Current, float64 Target, float64 DeltaTime, float64 InterpSpeed) no_discard", FUNC_TRIVIAL(FMath::FInterpTo<double>));

	Binds.BindGlobalFunctionForTarget("FLinearColor CInterpTo(FLinearColor Current, FLinearColor Target, float32 DeltaTime, float32 InterpSpeed) no_discard", FUNC_TRIVIAL(FMath::CInterpTo));


	Binds.BindGlobalFunctionForTarget("bool SphereAABBIntersection(const FVector& SphereCenter, const float64 RadiusSquared, const FBox& AABB) no_discard",
		FUNCPR_TRIVIAL(bool, FMath::SphereAABBIntersection, (const FVector&, const double, const FBox&)));
	Binds.BindGlobalFunctionForTarget("bool SphereAABBIntersection(const FSphere& Sphere, const FBox& AABB) no_discard",
		FUNCPR_TRIVIAL(bool, FMath::SphereAABBIntersection, (const FSphere&, const FBox&)));

	Binds.BindGlobalFunctionForTarget("bool SphereAABBIntersection(const FVector3f& SphereCenter, const float32 RadiusSquared, const FBox3f& AABB) no_discard",
		FUNCPR_TRIVIAL(bool, FMath::SphereAABBIntersection, (const FVector3f&, const float, const FBox3f&)));
	Binds.BindGlobalFunctionForTarget("bool SphereAABBIntersection(const FSphere3f& Sphere, const FBox3f& AABB) no_discard",
		FUNCPR_TRIVIAL(bool, FMath::SphereAABBIntersection, (const FSphere3f&, const FBox3f&)));

	Binds.BindGlobalFunctionForTarget("FVector RayPlaneIntersection(const FVector& RayOrigin, const FVector& RayDirection, const FPlane& Plane) no_discard",
		FUNCPR_TRIVIAL(FVector, FMath::RayPlaneIntersection, (const FVector&, const FVector&, const FPlane&)));

	Binds.BindGlobalFunctionForTarget("FVector LinePlaneIntersection(const FVector& Point1, const FVector& Point2, const FVector& PlaneOrigin, const FVector& PlaneNormal) no_discard",
		FUNCPR_TRIVIAL(FVector, FMath::LinePlaneIntersection, (const FVector&, const FVector&, const FVector&, const FVector&)));
	Binds.BindGlobalFunctionForTarget("FVector LinePlaneIntersection(const FVector& Point1, const FVector& Point2, const FPlane& Plane) no_discard",
		FUNCPR_TRIVIAL(FVector, FMath::LinePlaneIntersection, (const FVector&, const FVector&, const FPlane&)));

	Binds.BindGlobalFunctionForTarget("FVector3f LinePlaneIntersection(const FVector3f& Point1, const FVector3f& Point2, const FVector3f& PlaneOrigin, const FVector3f& PlaneNormal) no_discard",
		FUNCPR_TRIVIAL(FVector3f, FMath::LinePlaneIntersection, (const FVector3f&, const FVector3f&, const FVector3f&, const FVector3f&)));

	Binds.BindGlobalFunctionForTarget("bool LineSphereIntersection(const FVector3f& Start, const FVector3f& Dir, float32 Length, const FVector3f& Origin, float32 Radius)", FUNC_TRIVIAL(FMath::LineSphereIntersection<float>));
	Binds.BindGlobalFunctionForTarget("bool LineSphereIntersection(const FVector& Start, const FVector& Dir, float64 Length, const FVector& Origin, float64 Radius)", FUNC_TRIVIAL(FMath::LineSphereIntersection<double>));

	Binds.BindGlobalFunctionForTarget("bool LineBoxIntersection(const FBox& Box, const FVector& Start, const FVector& End, const FVector& StartToEnd)", FUNCPR_TRIVIAL(bool, FMath::LineBoxIntersection, (const FBox&, const FVector&, const FVector&, const FVector&)));

	Binds.BindGlobalFunctionForTarget("FVector ClosestPointOnLine(const FVector& LineStart, const FVector& LineEnd, const FVector& Point) no_discard", FUNCPR_TRIVIAL(FVector, FMath::ClosestPointOnLine, (const FVector&, const FVector&, const FVector&)));
	Binds.BindGlobalFunctionForTarget("FVector ClosestPointOnInfiniteLine(const FVector& LineStart, const FVector& LineEnd, const FVector& Point) no_discard", FUNC_TRIVIAL(FMath::ClosestPointOnInfiniteLine));

	Binds.BindGlobalFunctionForTarget("FSphere ComputeBoundingSphereForCone(const FVector& ConeOrigin, const FVector& ConeDirection, float64 ConeRadius, float64 CosConeAngle, float64 SinConeAngle) no_discard",
		FUNCPR_TRIVIAL(FSphere, FMath::ComputeBoundingSphereForCone, (const FVector&, const FVector&, double, double, double)));

	Binds.BindGlobalFunctionForTarget("FSphere3f ComputeBoundingSphereForCone(const FVector3f& ConeOrigin, const FVector3f& ConeDirection, float32 ConeRadius, float32 CosConeAngle, float32 SinConeAngle) no_discard",
		FUNCPR_TRIVIAL(FSphere3f, FMath::ComputeBoundingSphereForCone, (const FVector3f&, const FVector3f&, float, float, float)));

	Binds.BindGlobalFunctionForTarget("int32 TruncToInt(float64 F) no_discard", FUNCPR_TRIVIAL(int32, FMath::TruncToInt32, (double)));
	Binds.BindGlobalFunctionForTarget("int32 TruncToInt(float32 F) no_discard", FUNCPR_TRIVIAL(int32, FMath::TruncToInt, (float)));
	Binds.BindGlobalFunctionForTarget("float64 TruncToFloat(float64 F) no_discard", FUNCPR_TRIVIAL(double, FMath::TruncToFloat, (double)));
	Binds.BindGlobalFunctionForTarget("float32 TruncToFloat(float32 F) no_discard", FUNCPR_TRIVIAL(float, FMath::TruncToFloat, (float)));

	FAngelscriptBoundFunction TruncToDouble = Binds.BindGlobalFunctionForTarget(
		"float64 TruncToDouble(float64 F) no_discard",
		FUNCPR_TRIVIAL(double, FMath::TruncToDouble, (double)));
	if (bDeprecateDouble)
	{
		TruncToDouble.Deprecated("Double is deprecated, use float or float64.");
	}

	Binds.BindGlobalFunctionForTarget("int32 RoundToInt(float64 F) no_discard", FUNCPR_TRIVIAL(int32, FMath::RoundToInt32, (double)));
	Binds.BindGlobalFunctionForTarget("int32 RoundToInt(float32 F) no_discard", FUNCPR_TRIVIAL(int32, FMath::RoundToInt, (float)));
	Binds.BindGlobalFunctionForTarget("float64 RoundToFloat(float64 F) no_discard", FUNCPR_TRIVIAL(double, FMath::RoundToFloat, (double)));
	Binds.BindGlobalFunctionForTarget("float32 RoundToFloat(float32 F) no_discard", FUNCPR_TRIVIAL(float, FMath::RoundToFloat, (float)));

	FAngelscriptBoundFunction RoundToDouble = Binds.BindGlobalFunctionForTarget(
		"float64 RoundToDouble(float64 F) no_discard",
		FUNCPR_TRIVIAL(double, FMath::RoundToDouble, (double)));
	if (bDeprecateDouble)
	{
		RoundToDouble.Deprecated("Double is deprecated, use float or float64.");
	}

	Binds.BindGlobalFunctionForTarget("int32 FloorToInt(float64 F) no_discard", FUNCPR_TRIVIAL(int32, FMath::FloorToInt32, (double)));
	Binds.BindGlobalFunctionForTarget("int32 FloorToInt(float32 F) no_discard", FUNCPR_TRIVIAL(int32, FMath::FloorToInt, (float)));
	Binds.BindGlobalFunctionForTarget("float64 FloorToFloat(float64 F) no_discard", FUNCPR_TRIVIAL(double, FMath::FloorToFloat, (double)));
	Binds.BindGlobalFunctionForTarget("float32 FloorToFloat(float32 F) no_discard", FUNCPR_TRIVIAL(float, FMath::FloorToFloat, (float)));

	FAngelscriptBoundFunction FloorToDouble = Binds.BindGlobalFunctionForTarget(
		"float64 FloorToDouble(float64 F) no_discard",
		FUNCPR_TRIVIAL(double, FMath::FloorToDouble, (double)));
	if (bDeprecateDouble)
	{
		FloorToDouble.Deprecated("Double is deprecated, use float or float64.");
	}

	Binds.BindGlobalFunctionForTarget("int32 CeilToInt(float64 F) no_discard", FUNCPR_TRIVIAL(int32, FMath::CeilToInt32, (double)));
	Binds.BindGlobalFunctionForTarget("int32 CeilToInt(float32 F) no_discard", FUNCPR_TRIVIAL(int32, FMath::CeilToInt, (float)));
	Binds.BindGlobalFunctionForTarget("float64 CeilToFloat(float64 F) no_discard", FUNCPR_TRIVIAL(double, FMath::CeilToFloat, (double)));
	Binds.BindGlobalFunctionForTarget("float32 CeilToFloat(float32 F) no_discard", FUNCPR_TRIVIAL(float, FMath::CeilToFloat, (float)));

	Binds.BindGlobalFunctionForTarget("float64 CeilToDouble(float64 F) no_discard", FUNCPR_TRIVIAL(double, FMath::CeilToDouble, (double)))
		.Deprecated("Double is deprecated, use float or float64.");

	Binds.BindGlobalFunctionForTarget("float64 RoundFromZero(float64 F) no_discard", FUNCPR_TRIVIAL(double, FMath::RoundFromZero, (double)));
	Binds.BindGlobalFunctionForTarget("float32 RoundFromZero(float32 F) no_discard", FUNCPR_TRIVIAL(float, FMath::RoundFromZero, (float)));

	Binds.BindGlobalFunctionForTarget("bool IsNaN(float64 F) no_discard", FUNCPR_TRIVIAL(bool, FMath::IsNaN, (double)));
	Binds.BindGlobalFunctionForTarget("bool IsFinite(float64 F) no_discard", FUNCPR_TRIVIAL(bool, FMath::IsFinite, (double)));

	Binds.BindGlobalFunctionForTarget("float64 InvSqrt(float64 F) no_discard", FUNCPR_TRIVIAL(double, FMath::InvSqrt, (double)));
	Binds.BindGlobalFunctionForTarget("float64 InvSqrtEst(float64 F) no_discard", FUNCPR_TRIVIAL(double, FMath::InvSqrtEst, (double)));

	Binds.BindGlobalFunctionForTarget("float64 Fractional(float64 Value) no_discard", FUNCPR_TRIVIAL(double, FMath::Fractional, (double)));
	Binds.BindGlobalFunctionForTarget("float64 Frac(float64 Value) no_discard", FUNCPR_TRIVIAL(double, FMath::Frac, (double)));

	Binds.BindGlobalFunctionForTarget("bool IsNaN(float32 F) no_discard", FUNCPR_TRIVIAL(bool, FMath::IsNaN, (float)));
	Binds.BindGlobalFunctionForTarget("bool IsFinite(float32 F) no_discard", FUNCPR_TRIVIAL(bool, FMath::IsFinite, (float)));

	Binds.BindGlobalFunctionForTarget("float32 InvSqrt(float32 F) no_discard", FUNCPR_TRIVIAL(float, FMath::InvSqrt, (float)));
	Binds.BindGlobalFunctionForTarget("float32 InvSqrtEst(float32 F) no_discard", FUNCPR_TRIVIAL(float, FMath::InvSqrtEst, (float)));

	Binds.BindGlobalFunctionForTarget("float32 Fractional(float32 Value) no_discard", FUNCPR_TRIVIAL(float, FMath::Fractional, (float)));
	Binds.BindGlobalFunctionForTarget("float32 Frac(float32 Value) no_discard", FUNCPR_TRIVIAL(float, FMath::Frac, (float)));

	Binds.BindGlobalFunctionForTarget("float64 Exp(float64 Value) no_discard", FUNCPR_TRIVIAL(double, FMath::Exp, (double)));
	Binds.BindGlobalFunctionForTarget("float64 Exp2(float64 Value) no_discard", FUNCPR_TRIVIAL(double, FMath::Exp2, (double)));
	Binds.BindGlobalFunctionForTarget("float64 Loge(float64 Value) no_discard", FUNCPR_TRIVIAL(double, FMath::Loge, (double)));
	Binds.BindGlobalFunctionForTarget("float64 Log2(float64 Value) no_discard", FUNCPR_TRIVIAL(double, FMath::Log2, (double)));
	Binds.BindGlobalFunctionForTarget("float64 LogX(float64 Base, float64 Value) no_discard", FUNCPR_TRIVIAL(double, FMath::LogX, (double, double)));

	Binds.BindGlobalFunctionForTarget("float64 Fmod(float64 X, float64 Y) no_discard", FUNCPR_TRIVIAL(double, FMath::Fmod, (double, double)));
	Binds.BindGlobalFunctionForTarget("float64 Sin(float64 Value) no_discard", FUNCPR_TRIVIAL(double, FMath::Sin, (double)));
	Binds.BindGlobalFunctionForTarget("float64 Asin(float64 Value) no_discard", FUNCPR_TRIVIAL(double, FMath::Asin, (double)));
	Binds.BindGlobalFunctionForTarget("float64 Sinh(float64 Value) no_discard", FUNCPR_TRIVIAL(double, FMath::Sinh, (double)));
	Binds.BindGlobalFunctionForTarget("float64 Cos(float64 Value) no_discard", FUNCPR_TRIVIAL(double, FMath::Cos, (double)));
	Binds.BindGlobalFunctionForTarget("float64 Acos(float64 Value) no_discard", FUNCPR_TRIVIAL(double, FMath::Acos, (double)));
	Binds.BindGlobalFunctionForTarget("float64 Tan(float64 Value) no_discard", FUNCPR_TRIVIAL(double, FMath::Tan, (double)));
	Binds.BindGlobalFunctionForTarget("float64 Atan(float64 Value) no_discard", FUNCPR_TRIVIAL(double, FMath::Atan, (double)));
	Binds.BindGlobalFunctionForTarget("float64 Atan2(float64 Y, float64 X) no_discard", FUNCPR_TRIVIAL(double, FMath::Atan2, (double, double)));
	Binds.BindGlobalFunctionForTarget("float64 Sqrt(float64 Value) no_discard", FUNCPR_TRIVIAL(double, FMath::Sqrt, (double)));
	Binds.BindGlobalFunctionForTarget("float64 Pow(float64 A, float64 B) no_discard", FUNCPR_TRIVIAL(double, FMath::Pow, (double, double)));

	Binds.BindGlobalFunctionForTarget("float32 Exp(float32 Value) no_discard", FUNCPR_TRIVIAL(float, FMath::Exp, (float)));
	Binds.BindGlobalFunctionForTarget("float32 Exp2(float32 Value) no_discard", FUNCPR_TRIVIAL(float, FMath::Exp2, (float)));
	Binds.BindGlobalFunctionForTarget("float32 Loge(float32 Value) no_discard", FUNCPR_TRIVIAL(float, FMath::Loge, (float)));
	Binds.BindGlobalFunctionForTarget("float32 Log2(float32 Value) no_discard", FUNCPR_TRIVIAL(float, FMath::Log2, (float)));
	Binds.BindGlobalFunctionForTarget("float32 LogX(float32 Base, float32 Value) no_discard", FUNCPR_TRIVIAL(float, FMath::LogX, (float, float)));

	Binds.BindGlobalFunctionForTarget("float32 Fmod(float32 X, float32 Y) no_discard", FUNCPR_TRIVIAL(float, FMath::Fmod, (float, float)));
	Binds.BindGlobalFunctionForTarget("float32 Sin(float32 Value) no_discard", FUNCPR_TRIVIAL(float, FMath::Sin, (float)));
	Binds.BindGlobalFunctionForTarget("float32 Asin(float32 Value) no_discard", FUNCPR_TRIVIAL(float, FMath::Asin, (float)));
	Binds.BindGlobalFunctionForTarget("float32 Sinh(float32 Value) no_discard", FUNCPR_TRIVIAL(float, FMath::Sinh, (float)));
	Binds.BindGlobalFunctionForTarget("float32 Cos(float32 Value) no_discard", FUNCPR_TRIVIAL(float, FMath::Cos, (float)));
	Binds.BindGlobalFunctionForTarget("float32 Acos(float32 Value) no_discard", FUNCPR_TRIVIAL(float, FMath::Acos, (float)));
	Binds.BindGlobalFunctionForTarget("float32 Tan(float32 Value) no_discard", FUNCPR_TRIVIAL(float, FMath::Tan, (float)));
	Binds.BindGlobalFunctionForTarget("float32 Atan(float32 Value) no_discard", FUNCPR_TRIVIAL(float, FMath::Atan, (float)));
	Binds.BindGlobalFunctionForTarget("float32 Atan2(float32 Y, float32 X) no_discard", FUNCPR_TRIVIAL(float, FMath::Atan2, (float, float)));
	Binds.BindGlobalFunctionForTarget("float32 Sqrt(float32 Value) no_discard", FUNCPR_TRIVIAL(float, FMath::Sqrt, (float)));
	Binds.BindGlobalFunctionForTarget("float32 Pow(float32 A, float32 B) no_discard", FUNCPR_TRIVIAL(float, FMath::Pow, (float, float)));

	Binds.BindGlobalFunctionForTarget("int32 Rand() no_discard", FUNC_TRIVIAL(FMath::Rand));
	Binds.BindGlobalFunctionForTarget("float32 FRand() no_discard", FUNC_TRIVIAL(FMath::FRand));

	Binds.BindGlobalFunctionForTarget("float64 Abs(float64 Value) no_discard", FUNC_TRIVIAL(FMath::Abs<double>));
	Binds.BindGlobalFunctionForTarget("float32 Abs(float32 Value) no_discard", FUNC_TRIVIAL(FMath::Abs<float>));
	Binds.BindGlobalFunctionForTarget("int32 Abs(int32 Value) no_discard", FUNC_TRIVIAL(FMath::Abs<int32>));

	Binds.BindGlobalFunctionForTarget("float64 Sign(float64 Value) no_discard", FUNC_TRIVIAL(FMath::Sign<double>));
	Binds.BindGlobalFunctionForTarget("float32 Sign(float32 Value) no_discard", FUNC_TRIVIAL(FMath::Sign<float>));
	Binds.BindGlobalFunctionForTarget("int32 Sign(int32 Value) no_discard", FUNC_TRIVIAL(FMath::Sign<int32>));

	Binds.BindGlobalFunctionForTarget("float64 Min(float64 A, float64 B) no_discard", FUNCPR_TRIVIAL(double, FMath::Min<double>, (const double, const double)));
	Binds.BindGlobalFunctionForTarget("float32 Min(float32 A, float32 B) no_discard", FUNCPR_TRIVIAL(float, FMath::Min<float>, (const float, const float)));
	Binds.BindGlobalFunctionForTarget("int32 Min(int32 A, int32 B) no_discard", FUNCPR_TRIVIAL(int32, FMath::Min<int32>, (const int32, const int32)));
	Binds.BindGlobalFunctionForTarget("uint32 Min(uint32 A, uint32 B) no_discard", FUNCPR_TRIVIAL(uint32, FMath::Min<uint32>, (const uint32, const uint32)));

	Binds.BindGlobalFunctionForTarget("float64 Max3(float64 A, float64 B, float64 C) no_discard", FUNCPR_TRIVIAL(double, FMath::Max3<double>, (const double, const double, const double)));
	Binds.BindGlobalFunctionForTarget("float32 Max3(float32 A, float32 B, float32 C) no_discard", FUNCPR_TRIVIAL(float, FMath::Max3<float>, (const float, const float, const float)));

	Binds.BindGlobalFunctionForTarget("float64 Max(float64 A, float64 B) no_discard", FUNCPR_TRIVIAL(double, FMath::Max<double>, (const double, const double)));
	Binds.BindGlobalFunctionForTarget("float32 Max(float32 A, float32 B) no_discard", FUNCPR_TRIVIAL(float, FMath::Max<float>, (const float, const float)));
	Binds.BindGlobalFunctionForTarget("int32 Max(int32 A, int32 B) no_discard", FUNCPR_TRIVIAL(int32, FMath::Max<int32>, (const int32, const int32)));
	Binds.BindGlobalFunctionForTarget("uint32 Max(uint32 A, uint32 B) no_discard", FUNCPR_TRIVIAL(uint32, FMath::Max<uint32>, (const uint32, const uint32)));

	Binds.BindGlobalFunctionForTarget("float64 Square(float64 Value) no_discard", FUNC_TRIVIAL(FMath::Square<double>));
	Binds.BindGlobalFunctionForTarget("float32 Square(float32 Value) no_discard", FUNC_TRIVIAL(FMath::Square<float>));
	Binds.BindGlobalFunctionForTarget("int32 Square(int32 Value) no_discard", FUNC_TRIVIAL(FMath::Square<int32>));
	Binds.BindGlobalFunctionForTarget("uint32 Square(uint32 Value) no_discard", FUNC_TRIVIAL(FMath::Square<uint32>));

	Binds.BindGlobalFunctionForTarget("float64 GetMappedRangeValueClamped(const FVector2D& InputRange, const FVector2D& OutputRange, const float64 Value) no_discard", FUNCPR_TRIVIAL(double, FMath::GetMappedRangeValueClamped, (const FVector2D&, const FVector2D&, const double)));
	Binds.BindGlobalFunctionForTarget("float64 GetMappedRangeValueUnclamped(const FVector2D& InputRange, const FVector2D& OutputRange, const float64 Value) no_discard", FUNCPR_TRIVIAL(double, FMath::GetMappedRangeValueUnclamped, (const FVector2D&, const FVector2D&, const double)));

	Binds.BindGlobalFunctionForTarget("float32 GetMappedRangeValueClamped(const FVector2f& InputRange, const FVector2f& OutputRange, const float32 Value) no_discard", FUNCPR_TRIVIAL(float, FMath::GetMappedRangeValueClamped, (const FVector2f&, const FVector2f&, const float)));
	Binds.BindGlobalFunctionForTarget("float32 GetMappedRangeValueUnclamped(const FVector2f& InputRange, const FVector2f& OutputRange, const float32 Value) no_discard", FUNCPR_TRIVIAL(float, FMath::GetMappedRangeValueUnclamped, (const FVector2f&, const FVector2f&, const float)));

	Binds.BindGlobalFunctionForTarget("float32 PerlinNoise1D(float32 X) no_discard", FUNC_TRIVIAL(FMath::PerlinNoise1D))
		.Documentation(TEXT(
			"Generates a 1D Perlin noise from the given value.  Returns a continuous random value between -1.0 and 1.0.\n"
			"@param\tValue\tThe input value that Perlin noise will be generated from.  This is usually a steadily incrementing time value.\n"
			"@return\tPerlin noise in the range of -1.0 to 1.0"));

	Binds.BindGlobalFunctionForTarget("float32 PerlinNoise2D(const FVector2D& Location) no_discard", FUNC_TRIVIAL(FMath::PerlinNoise2D))
		.Documentation(TEXT(
			"Generates a 1D Perlin noise sample at the given location.  Returns a continuous random value between -1.0 and 1.0.\n"
			"@param\tLocation\tWhere to sample\n"
			"@return\tPerlin noise in the range of -1.0 to 1.0\n"));

	Binds.BindGlobalFunctionForTarget("float32 PerlinNoise3D(const FVector& Location) no_discard", FUNC_TRIVIAL(FMath::PerlinNoise3D))
		.Documentation(TEXT(
			"Generates a 3D Perlin noise sample at the given location.  Returns a continuous random value between -1.0 and 1.0.\n"
			"@param\tLocation\tWhere to sample\n"
			"@return\tPerlin noise in the range of -1.0 to 1.0"));


	Binds.BindGlobalFunctionForTarget("float64 GridSnap(float64 Location, float64 Grid) no_discard", FUNCPR_TRIVIAL(double, FMath::GridSnap, (double, double)));
	Binds.BindGlobalFunctionForTarget("float32 GridSnap(float32 Location, float32 Grid) no_discard", FUNCPR_TRIVIAL(float, FMath::GridSnap, (float, float)));

	Binds.BindGlobalFunctionForTarget("bool SegmentIntersection2D(const FVector& SegmentStartA, const FVector& SegmentEndA, const FVector& SegmentStartB, const FVector& SegmentEndB, FVector& out_IntersectionPoint)", FUNC_TRIVIAL(FMath::SegmentIntersection2D))
		.Documentation(TEXT(
			"Returns true if there is an intersection between the segment specified by SegmentStartA and SegmentEndA, and\n"
			"the segment specified by SegmentStartB and SegmentEndB, in 2D space. If there is an intersection, the point is placed in out_IntersectionPoint\n"
			"@param SegmentStartA - start point of first segment\n"
			"@param SegmentEndA   - end point of first segment\n"
			"@param SegmentStartB - start point of second segment\n"
			"@param SegmentEndB   - end point of second segment\n"
			"@param out_IntersectionPoint - out var for the intersection point (if any)\n"
			"@return true if intersection occurred"));

	Binds.BindGlobalFunctionForTarget("float32 FloatSpringInterp(float32 Current, float32 Target, FFloatSpringState& SpringState, float32 Stiffness, float32 CriticalDampingFactor, float32 DeltaTime, float32 Mass = 1.f, float32 TargetVelocityAmount = 1.f)",
		&FAngelscriptFMathBinds::FloatSpringInterp)
		.Documentation(TEXT(
			"Uses a simple spring model to interpolate a float32 from Current to Target.\n"
			"@param Current\t\t\t\tCurrent value\n"
			"@param Target\t\t\t\tTarget value\n"
			"@param SpringState\t\t\tData related to spring model (velocity, error, etc..) - Create a unique variable per spring\n"
			"@param Stiffness\t\t\t\tHow stiff the spring model is (more stiffness means more oscillation around the target value)\n"
			"@param CriticalDampingFactor\tHow much damping to apply to the spring (0 means no damping, 1 means critically damped which means no oscillation)\n"
			"@param Mass\t\t\t\t\tMultiplier that acts like mass on a spring\n"
			"@param TargetVelocityAmount\tIf 1 then the target velocity will be calculated and used, which results following the target more closely/without lag. Values down to zero (recommended when using this to smooth data) will progressively disable this effect."));

	Binds.BindGlobalFunctionForTarget("FVector VectorSpringInterp(FVector Current, FVector Target, FVectorSpringState& SpringState, float32 Stiffness, float32 CriticalDampingFactor, float32 DeltaTime, float32 Mass = 1.f, float32 TargetVelocityAmount = 1.f)",
		&FAngelscriptFMathBinds::VectorSpringInterp)
		.Documentation(TEXT(
			"Uses a simple spring model to interpolate a vector from Current to Target.\n"
			"@param Current\t\t\t\tCurrent value\n"
			"@param Target\t\t\t\tTarget value\n"
			"@param SpringState\t\t\tData related to spring model (velocity, error, etc..) - Create a unique variable per spring\n"
			"@param Stiffness\t\t\t\tHow stiff the spring model is (more stiffness means more oscillation around the target value)\n"
			"@param CriticalDampingFactor\tHow much damping to apply to the spring (0 means no damping, 1 means critically damped which means no oscillation)\n"
			"@param Mass\t\t\t\t\tMultiplier that acts like mass on a spring\n"
			"@param TargetVelocityAmount\tIf 1 then the target velocity will be calculated and used, which results following the target more closely/without lag. Values down to zero (recommended when using this to smooth data) will progressively disable this effect."));

	Binds.BindGlobalFunctionForTarget("FQuat QuaternionSpringInterp(FQuat Current, FQuat Target, FQuaternionSpringState& SpringState, float32 Stiffness, float32 CriticalDampingFactor, float32 DeltaTime, float32 Mass = 1.f, float32 TargetVelocityAmount = 1.f)",
		&FAngelscriptFMathBinds::QuaternionSpringInterp)
		.Documentation(TEXT(
			"Uses a simple spring model to interpolate a quaternion from Current to Target.\n"
			"@param Current\t\t\t\tCurrent value\n"
			"@param Target\t\t\t\tTarget value\n"
			"@param SpringState\t\t\tData related to spring model (velocity, error, etc..) - Create a unique variable per spring\n"
			"@param Stiffness\t\t\t\tHow stiff the spring model is (more stiffness means more oscillation around the target value)\n"
			"@param CriticalDampingFactor\tHow much damping to apply to the spring (0 means no damping, 1 means critically damped which means no oscillation)\n"
			"@param Mass\t\t\t\t\tMultiplier that acts like mass on a spring\n"
			"@param TargetVelocityAmount\tIf 1 then the target velocity will be calculated and used, which results following the target more closely/without lag. Values down to zero (recommended when using this to smooth data) will progressively disable this effect."));

	// Ease double
	Binds.BindGlobalFunctionForTarget("float64 EaseIn(const float64& A, const float64& B, float64 Alpha, float64 Exp) no_discard", &FAngelscriptFMathBinds::EaseIn<double, double>);

	Binds.BindGlobalFunctionForTarget("float64 EaseOut(const float64& A, const float64& B, float64 Alpha, float64 Exp) no_discard", &FAngelscriptFMathBinds::EaseOut<double, double>);
	
	Binds.BindGlobalFunctionForTarget("float64 EaseInOut(const float64& A, const float64& B, float64 Alpha, float64 Exp) no_discard", &FAngelscriptFMathBinds::EaseInOut<double, double>);

	Binds.BindGlobalFunctionForTarget("float64 SinusoidalIn(const float64& A, const float64& B, float64 Alpha) no_discard", &FAngelscriptFMathBinds::SinusoidalIn<double, double>);

	Binds.BindGlobalFunctionForTarget("float64 SinusoidalOut(const float64& A, const float64& B, float64 Alpha) no_discard", &FAngelscriptFMathBinds::SinusoidalOut<double, double>);

	Binds.BindGlobalFunctionForTarget("float64 SinusoidalInOut(const float64& A, const float64& B, float64 Alpha) no_discard", &FAngelscriptFMathBinds::SinusoidalInOut<double, double>);

	Binds.BindGlobalFunctionForTarget("float64 ExpoIn(const float64& A, const float64& B, float64 Alpha) no_discard", &FAngelscriptFMathBinds::ExpoIn<double, double>);

	Binds.BindGlobalFunctionForTarget("float64 ExpoOut(const float64& A, const float64& B, float64 Alpha) no_discard", &FAngelscriptFMathBinds::ExpoOut<double, double>);

	Binds.BindGlobalFunctionForTarget("float64 ExpoInOut(const float64& A, const float64& B, float64 Alpha) no_discard", &FAngelscriptFMathBinds::ExpoInOut<double, double>);

	Binds.BindGlobalFunctionForTarget("float64 CircularIn(const float64& A, const float64& B, float64 Alpha) no_discard", &FAngelscriptFMathBinds::CircularIn<double, double>);

	Binds.BindGlobalFunctionForTarget("float64 CircularOut(const float64& A, const float64& B, float64 Alpha) no_discard", &FAngelscriptFMathBinds::CircularOut<double, double>);

	Binds.BindGlobalFunctionForTarget("float64 CircularInOut(const float64& A, const float64& B, float64 Alpha) no_discard", &FAngelscriptFMathBinds::CircularInOut<double, double>);

	// Ease float
	Binds.BindGlobalFunctionForTarget("float32 EaseIn(const float32& A, const float32& B, float32 Alpha, float32 Exp) no_discard", &FAngelscriptFMathBinds::EaseIn<float, float>);

	Binds.BindGlobalFunctionForTarget("float32 EaseOut(const float32& A, const float32& B, float32 Alpha, float32 Exp) no_discard", &FAngelscriptFMathBinds::EaseOut<float, float>);
	
	Binds.BindGlobalFunctionForTarget("float32 EaseInOut(const float32& A, const float32& B, float32 Alpha, float32 Exp) no_discard", &FAngelscriptFMathBinds::EaseInOut<float, float>);

	Binds.BindGlobalFunctionForTarget("float32 SinusoidalIn(const float32& A, const float32& B, float32 Alpha) no_discard", &FAngelscriptFMathBinds::SinusoidalIn<float, float>);

	Binds.BindGlobalFunctionForTarget("float32 SinusoidalOut(const float32& A, const float32& B, float32 Alpha) no_discard", &FAngelscriptFMathBinds::SinusoidalOut<float, float>);

	Binds.BindGlobalFunctionForTarget("float32 SinusoidalInOut(const float32& A, const float32& B, float32 Alpha) no_discard", &FAngelscriptFMathBinds::SinusoidalInOut<float, float>);

	Binds.BindGlobalFunctionForTarget("float32 ExpoIn(const float32& A, const float32& B, float32 Alpha) no_discard", &FAngelscriptFMathBinds::ExpoIn<float, float>);

	Binds.BindGlobalFunctionForTarget("float32 ExpoOut(const float32& A, const float32& B, float32 Alpha) no_discard", &FAngelscriptFMathBinds::ExpoOut<float, float>);

	Binds.BindGlobalFunctionForTarget("float32 ExpoInOut(const float32& A, const float32& B, float32 Alpha) no_discard", &FAngelscriptFMathBinds::ExpoInOut<float, float>);

	Binds.BindGlobalFunctionForTarget("float32 CircularIn(const float32& A, const float32& B, float32 Alpha) no_discard", &FAngelscriptFMathBinds::CircularIn<float, float>);

	Binds.BindGlobalFunctionForTarget("float32 CircularOut(const float32& A, const float32& B, float32 Alpha) no_discard", &FAngelscriptFMathBinds::CircularOut<float, float>);

	Binds.BindGlobalFunctionForTarget("float32 CircularInOut(const float32& A, const float32& B, float32 Alpha) no_discard", &FAngelscriptFMathBinds::CircularInOut<float, float>);

	// Ease Vector
	Binds.BindGlobalFunctionForTarget("FVector EaseIn(const FVector& A, const FVector& B, float32 Alpha, float32 Exp) no_discard", &FAngelscriptFMathBinds::EaseIn<FVector, float>);

	Binds.BindGlobalFunctionForTarget("FVector EaseOut(const FVector& A, const FVector& B, float32 Alpha, float32 Exp) no_discard", &FAngelscriptFMathBinds::EaseOut<FVector, float>);

	Binds.BindGlobalFunctionForTarget("FVector EaseInOut(const FVector& A, const FVector& B, float32 Alpha, float32 Exp) no_discard", &FAngelscriptFMathBinds::EaseInOut<FVector, float>);

	Binds.BindGlobalFunctionForTarget("FVector SinusoidalIn(const FVector& A, const FVector& B, float32 Alpha) no_discard", &FAngelscriptFMathBinds::SinusoidalIn<FVector, float>);

	Binds.BindGlobalFunctionForTarget("FVector SinusoidalOut(const FVector& A, const FVector& B, float32 Alpha) no_discard", &FAngelscriptFMathBinds::SinusoidalOut<FVector, float>);

	Binds.BindGlobalFunctionForTarget("FVector SinusoidalInOut(const FVector& A, const FVector& B, float32 Alpha) no_discard", &FAngelscriptFMathBinds::SinusoidalInOut<FVector, float>);

	Binds.BindGlobalFunctionForTarget("FVector ExpoIn(const FVector& A, const FVector& B, float32 Alpha) no_discard", &FAngelscriptFMathBinds::ExpoIn<FVector, float>);

	Binds.BindGlobalFunctionForTarget("FVector ExpoOut(const FVector& A, const FVector& B, float32 Alpha) no_discard", &FAngelscriptFMathBinds::ExpoOut<FVector, float>);

	Binds.BindGlobalFunctionForTarget("FVector ExpoInOut(const FVector& A, const FVector& B, float32 Alpha) no_discard", &FAngelscriptFMathBinds::ExpoInOut<FVector, float>);

	Binds.BindGlobalFunctionForTarget("FVector CircularIn(const FVector& A, const FVector& B, float32 Alpha) no_discard", &FAngelscriptFMathBinds::CircularIn<FVector, float>);

	Binds.BindGlobalFunctionForTarget("FVector CircularOut(const FVector& A, const FVector& B, float32 Alpha) no_discard", &FAngelscriptFMathBinds::CircularOut<FVector, float>);

	Binds.BindGlobalFunctionForTarget("FVector CircularInOut(const FVector& A, const FVector& B, float32 Alpha) no_discard", &FAngelscriptFMathBinds::CircularInOut<FVector, float>);

	Binds.BindGlobalFunctionForTarget("bool IsPointInBox(const FVector& Point, const FVector& BoxOrigin, const FVector& BoxExtent) no_discard",
		&FAngelscriptFMathBinds::IsPointInBox);

	Binds.BindGlobalFunctionForTarget("bool IsPointInBoxWithTransform(const FVector& Point, const FTransform& BoxWorldTransform, const FVector& BoxExtent) no_discard",
		&FAngelscriptFMathBinds::IsPointInBoxWithTransform);

	Binds.BindGlobalFunctionForTarget("void FindNearestPointsOnLineSegments(FVector Segment1Start, FVector Segment1End, FVector Segment2Start, FVector Segment2End, FVector& Segment1Point, FVector& Segment2Point)",
		&FAngelscriptFMathBinds::FindNearestPointsOnLineSegments);

	Binds.BindGlobalFunctionForTarget("float64 NormalizeToRange(float64 Value, float64 RangeMin, float64 RangeMax) no_discard", FUNCPR_TRIVIAL(double, UKismetMathLibrary::NormalizeToRange, (double, double, double)));

	Binds.BindGlobalFunctionForTarget("int32 IntegerDivisionTrunc(int32 Value, int32 DivideBy) no_discard", &FAngelscriptFMathBinds::IntegerDivisionTruncInt32)
		.Documentation(TEXT("Divide integer Value by DivideBy, truncating any decimals from the result."));

	Binds.BindGlobalFunctionForTarget("int64 IntegerDivisionTrunc(int64 Value, int64 DivideBy) no_discard", &FAngelscriptFMathBinds::IntegerDivisionTruncInt64)
		.Documentation(TEXT("Divide integer Value by DivideBy, truncating any decimals from the result."));

	Binds.BindGlobalFunctionForTarget("uint32 IntegerDivisionTrunc(uint32 Value, uint32 DivideBy) no_discard", &FAngelscriptFMathBinds::IntegerDivisionTruncUInt32)
		.Documentation(TEXT("Divide integer Value by DivideBy, truncating any decimals from the result."));

	Binds.BindGlobalFunctionForTarget("uint64 IntegerDivisionTrunc(uint64 Value, uint64 DivideBy) no_discard", &FAngelscriptFMathBinds::IntegerDivisionTruncUInt64)
		.Documentation(TEXT("Divide integer Value by DivideBy, truncating any decimals from the result."));

#if WITH_EDITOR
	// If we've set an alternative math namespace, update the scriptname meta tag on the math library as well
	if (UAngelscriptSettings::Get().MathNamespace == EAngelscriptMathNamespace::FMath)
	{
		UClass* MathLib = FindObject<UClass>(nullptr, TEXT("/Script/AngelscriptRuntime.AngelscriptMathLibrary"));
		if (MathLib != nullptr)
		{
			MathLib->SetMetaData("ScriptName", TEXT("FMath"));
		}
	}
#endif
}

AS_FORCE_LINK const FAngelscriptBind Bind_FMath(
	TEXT("FMath.Manual"),
	EAngelscriptBindPhase::ManualBindings,
	&BindFMath);
