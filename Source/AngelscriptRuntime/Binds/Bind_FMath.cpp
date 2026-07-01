#include "AngelscriptBinds.h"
#include "AngelscriptDocs.h"
#include "AngelscriptEngine.h"
#include "AngelscriptSettings.h"
#include "Kismet/KismetMathLibrary.h"

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_FMath([]
{
	const bool bDeprecateDouble = UAngelscriptSettings::Get().bDeprecateDoubleType;

	FAngelscriptBinds::FNamespace ns(
		(UAngelscriptSettings::Get().MathNamespace == EAngelscriptMathNamespace::Math)
		? "Math": "FMath"
	);

	FAngelscriptBinds::BindGlobalFunction("int32 RandHelper(int32 Max) no_discard", FUNCPR_TRIVIAL(int32, FMath::RandHelper, (int32)));
	FAngelscriptBinds::BindGlobalFunction("int32 RandRange(int32 Min, int32 Max) no_discard", FUNCPR_TRIVIAL(int32, FMath::RandRange, (int32, int32)));
	SCRIPT_BIND_DOCUMENTATION("Returns a random integer >= Min and <= Max");
	FAngelscriptBinds::BindGlobalFunction("float64 RandRange(float64 Min, float64 Max) no_discard", FUNCPR_TRIVIAL(double, FMath::RandRange, (double, double)));
	FAngelscriptBinds::BindGlobalFunction("float32 RandRange(float32 Min, float32 Max) no_discard", FUNCPR_TRIVIAL(float, FMath::RandRange, (float, float)));
	FAngelscriptBinds::BindGlobalFunction("bool RandBool() no_discard", FUNC_TRIVIAL(FMath::RandBool));
	FAngelscriptBinds::BindGlobalFunction("FVector VRand() no_discard", FUNC_TRIVIAL(FMath::VRand));
	SCRIPT_BIND_DOCUMENTATION("Returns a random vector with length of 1");


	FAngelscriptBinds::BindGlobalFunction("FVector VRandCone(const FVector& DDir, float32 HorizontalConeHalfAngleRad, float32 VerticalConeHalfAngleRad) no_discard",
		FUNCPR_TRIVIAL(FVector, FMath::VRandCone, (const FVector&, float, float)));
	SCRIPT_BIND_DOCUMENTATION(
    "Returns a random unit vector, uniformly distributed, within the specified cone\n"
    "ConeHalfAngleRad is the half-angle of cone, in radians.  Returns a normalized vector."
    );

	FAngelscriptBinds::BindGlobalFunction("FVector VRandCone(const FVector& DDir, float32 ConeHalfAngleRad) no_discard",
		FUNCPR_TRIVIAL(FVector, FMath::VRandCone, (const FVector&, float)));
	
	FAngelscriptBinds::BindGlobalFunction("FVector2D RandPointInCircle(float32 Radius) no_discard",
		FUNCPR_TRIVIAL(FVector2D, FMath::RandPointInCircle, (float)));
	SCRIPT_BIND_DOCUMENTATION("Get a random point on a unit circle, evenly spread across the circumference.");

	FAngelscriptBinds::BindGlobalFunction("FVector GetReflectionVector(const FVector& Direction, const FVector& SurfaceNormal) no_discard", FUNC_TRIVIAL(FMath::GetReflectionVector));
	SCRIPT_BIND_DOCUMENTATION(
	"Given a direction vector and a surface normal, returns the vector reflected across the surface normal.\n"
    "Produces a result like shining a laser at a mirror!\n"
    "@param Direction Direction vector the ray is coming from.\n"
    "@param SurfaceNormal A normal of the surface the ray should be reflected on.\n"
    "@returns Reflected vector."
    );
	
	FAngelscriptBinds::BindGlobalFunction("float32 MakePulsatingValue( const float64 InCurrentTime, const float32 InPulsesPerSecond, const float32 InPhase = 0.0f ) no_discard", FUNC_TRIVIAL(FMath::MakePulsatingValue));
	SCRIPT_BIND_DOCUMENTATION(
	"Simple function to create a pulsating scalar value\n"
    "@param  InCurrentTime  Current absolute time\n"
    "@param  InPulsesPerSecond  How many full pulses per second?\n"
    "@param  InPhase  Optional phase amount, between 0.0 and 1.0 (to synchronize pulses)\n"
    "@return  Pulsating value (0.0-1.0)"
    );

	FAngelscriptBinds::BindGlobalFunction("bool IsNearlyEqual(float64 A, float64 B, float64 ErrorTolerance = SMALL_NUMBER) no_discard",
		FUNCPR_TRIVIAL(bool, FMath::IsNearlyEqual, (double, double, double)));
	FAngelscriptBinds::BindGlobalFunction("bool IsNearlyEqual(float32 A, float32 B, float32 ErrorTolerance = SMALL_NUMBER) no_discard",
		FUNCPR_TRIVIAL(bool, FMath::IsNearlyEqual, (float, float, float)));

	FAngelscriptBinds::BindGlobalFunction("bool IsNearlyZero(float64 Value, float64 ErrorTolerance = SMALL_NUMBER) no_discard",
		FUNCPR_TRIVIAL(bool, FMath::IsNearlyZero, (double, double)));
	FAngelscriptBinds::BindGlobalFunction("bool IsNearlyZero(float32 Value, float32 ErrorTolerance = SMALL_NUMBER) no_discard",
		FUNCPR_TRIVIAL(bool, FMath::IsNearlyZero, (float, float)));

	FAngelscriptBinds::BindGlobalFunction("bool IsPowerOfTwo(int32 Value) no_discard", FUNC_TRIVIAL(FMath::IsPowerOfTwo<int32>));

#if ENGINE_MAJOR_VERSION >= 5
	FAngelscriptBinds::BindGlobalFunction("float64 SmoothStep(float64 A, float64 B, float64 X) no_discard", FUNCPR_TRIVIAL(double, FMath::SmoothStep, (double, double, double)));
	SCRIPT_BIND_DOCUMENTATION(
		"Returns a smooth Hermite interpolation between 0 and 1 for the value X (where X ranges between A and B)\n"
		"Clamped to 0 for X <= A and 1 for X >= B.\n"
		"@param A Minimum value of X\n"
		"@param B Maximum value of X\n"
		"@param X Parameter\n"
		"@return Smoothed value between 0 and 1\n"
	);
#endif

	FAngelscriptBinds::BindGlobalFunction("float32 SmoothStep(float32 A, float32 B, float32 X) no_discard", FUNCPR_TRIVIAL(float, FMath::SmoothStep, (float, float, float)));
	SCRIPT_BIND_DOCUMENTATION(
		"Returns a smooth Hermite interpolation between 0 and 1 for the value X (where X ranges between A and B)\n"
		"Clamped to 0 for X <= A and 1 for X >= B.\n"
		"@param A Minimum value of X\n"
		"@param B Maximum value of X\n"
		"@param X Parameter\n"
		"@return Smoothed value between 0 and 1\n"
	);

	FAngelscriptBinds::BindGlobalFunction("float64 Clamp(float64 X, float64 Min, float64 Max) no_discard", FUNC_TRIVIAL(FMath::Clamp<double>));
	SCRIPT_BIND_DOCUMENTATION("Clamps X to be between Min and Max, inclusive");
	FAngelscriptBinds::BindGlobalFunction("float32 Clamp(float32 X, float32 Min, float32 Max) no_discard", FUNC_TRIVIAL(FMath::Clamp<float>));
	SCRIPT_BIND_DOCUMENTATION("Clamps X to be between Min and Max, inclusive");
	FAngelscriptBinds::BindGlobalFunction("int32 Clamp(int32 X, int32 Min, int32 Max) no_discard", FUNC_TRIVIAL(FMath::Clamp<int32>));
	SCRIPT_BIND_DOCUMENTATION("Clamps X to be between Min and Max, inclusive");
	FAngelscriptBinds::BindGlobalFunction("uint32 Clamp(uint32 X, uint32 Min, uint32 Max) no_discard", FUNC_TRIVIAL(FMath::Clamp<uint32>));
	SCRIPT_BIND_DOCUMENTATION("Clamps X to be between Min and Max, inclusive");
	FAngelscriptBinds::BindGlobalFunction("int64 Clamp(int64 X, int64 Min, int64 Max) no_discard", FUNC_TRIVIAL(FMath::Clamp<int64>));
	SCRIPT_BIND_DOCUMENTATION("Clamps X to be between Min and Max, inclusive");
	FAngelscriptBinds::BindGlobalFunction("uint64 Clamp(uint64 X, uint64 Min, uint64 Max) no_discard", FUNC_TRIVIAL(FMath::Clamp<uint64>));
	SCRIPT_BIND_DOCUMENTATION("Clamps X to be between Min and Max, inclusive");

	FAngelscriptBinds::BindGlobalFunction("float64 FastAsin(float64 Value) no_discard", FUNCPR_TRIVIAL(double, FMath::FastAsin, (double)));
	FAngelscriptBinds::BindGlobalFunction("float32 FastAsin(float32 Value) no_discard", FUNCPR_TRIVIAL(float, FMath::FastAsin, (float)));

	FAngelscriptBinds::BindGlobalFunction("float64 RadiansToDegrees(const float64& RadVal) no_discard", FUNC_TRIVIAL(FMath::RadiansToDegrees<double>));
	FAngelscriptBinds::BindGlobalFunction("float64 DegreesToRadians(const float64& DegVal) no_discard", FUNC_TRIVIAL(FMath::DegreesToRadians<double>));

	FAngelscriptBinds::BindGlobalFunction("float32 RadiansToDegrees(const float32& RadVal) no_discard", FUNC_TRIVIAL(FMath::RadiansToDegrees<float>));
	FAngelscriptBinds::BindGlobalFunction("float32 DegreesToRadians(const float32& DegVal) no_discard", FUNC_TRIVIAL(FMath::DegreesToRadians<float>));

	FAngelscriptBinds::BindGlobalFunction("float32 ClampAngle(float32 AngleDegrees, float32 MinAngleDegrees, float32 MaxAngleDegrees) no_discard", FUNC_TRIVIAL(FMath::ClampAngle<float>));
	FAngelscriptBinds::BindGlobalFunction("float64 ClampAngle(float64 AngleDegrees, float64 MinAngleDegrees, float64 MaxAngleDegrees) no_discard", FUNC_TRIVIAL(FMath::ClampAngle<double>));

	FAngelscriptBinds::BindGlobalFunction("float64 FindDeltaAngleDegrees(float64 A1, float64 A2) no_discard", FUNCPR_TRIVIAL(double, FMath::FindDeltaAngleDegrees, (double, double)));
	FAngelscriptBinds::BindGlobalFunction("float64 FindDeltaAngleRadians(float64 A1, float64 A2) no_discard", FUNCPR_TRIVIAL(double, FMath::FindDeltaAngleRadians, (double, double)));

	FAngelscriptBinds::BindGlobalFunction("float32 FindDeltaAngleDegrees(float32 A1, float32 A2) no_discard", FUNCPR_TRIVIAL(float, FMath::FindDeltaAngleDegrees, (float, float)));
	FAngelscriptBinds::BindGlobalFunction("float32 FindDeltaAngleRadians(float32 A1, float32 A2) no_discard", FUNCPR_TRIVIAL(float, FMath::FindDeltaAngleRadians, (float, float)));

	FAngelscriptBinds::BindGlobalFunction("float64 UnwindDegrees(float64 A) no_discard", FUNCPR_TRIVIAL(double, FMath::UnwindDegrees, (double)));
	SCRIPT_BIND_DOCUMENTATION("Utility to ensure angle is between +/- 180 degrees by unwinding.")
	
	FAngelscriptBinds::BindGlobalFunction("float64 UnwindRadians(float64 A) no_discard", FUNCPR_TRIVIAL(double, FMath::UnwindRadians, (double)));
	SCRIPT_BIND_DOCUMENTATION("Utility to ensure angle is between +/- 180 degrees by unwinding.")

	FAngelscriptBinds::BindGlobalFunction("float32 UnwindDegrees(float32 A) no_discard", FUNCPR_TRIVIAL(float, FMath::UnwindDegrees, (float)));
	SCRIPT_BIND_DOCUMENTATION("Utility to ensure angle is between +/- 180 degrees by unwinding.")
	
	FAngelscriptBinds::BindGlobalFunction("float32 UnwindRadians(float32 A) no_discard", FUNCPR_TRIVIAL(float, FMath::UnwindRadians, (float)));
	SCRIPT_BIND_DOCUMENTATION("Utility to ensure angle is between +/- 180 degrees by unwinding.")
	
	FAngelscriptBinds::BindGlobalFunction(
		"float64 LerpStable(const float64& A, const float64& B, float64 Alpha) no_discard",
		FUNCPR_TRIVIAL(double, FMath::LerpStable<double>, (const double&, const double&, double))
	);

	FAngelscriptBinds::BindGlobalFunction(
		"float32 LerpStable(const float32& A, const float32& B, float32 Alpha) no_discard",
		FUNCPR_TRIVIAL(float, FMath::LerpStable<float>, (const float&, const float&, float))
	);

	// The helper macros break due to the , for stuff with multiple template arguments
#if AS_CAN_GENERATE_JIT
	FAngelscriptBinds::BindGlobalFunction("float64 Lerp(const float64& A, const float64& B, const float64& Alpha) no_discard", &FMath::Lerp<double, double>, "FMath::Lerp<double, double>", true);
	FAngelscriptBinds::BindGlobalFunction("float32 Lerp(const float32& A, const float32& B, const float32& Alpha) no_discard", &FMath::Lerp<float, float>, "FMath::Lerp<float, float>", true);

	FAngelscriptBinds::BindGlobalFunction("FVector Lerp(const FVector& A, const FVector& B, const float64& Alpha) no_discard", &FMath::Lerp<FVector, double>, "FMath::Lerp<FVector, double>", true);
	FAngelscriptBinds::BindGlobalFunction("FVector2D Lerp(const FVector2D& A, const FVector2D& B, const float64& Alpha) no_discard", &FMath::Lerp<FVector2D, double>, "FMath::Lerp<FVector2D, double>", true);

	FAngelscriptBinds::BindGlobalFunction("FVector3f Lerp(const FVector3f& A, const FVector3f& B, const float32& Alpha) no_discard", &FMath::Lerp<FVector3f, float>, "FMath::Lerp<FVector3f, float>", true);
	FAngelscriptBinds::BindGlobalFunction("FVector2f Lerp(const FVector2f& A, const FVector2f& B, const float32& Alpha) no_discard", &FMath::Lerp<FVector2f, float>, "FMath::Lerp<FVector2f, float>", true);

	FAngelscriptBinds::BindGlobalFunction("FVector VLerp(const FVector& A, const FVector& B, const FVector& Alpha) no_discard", &FMath::Lerp<FVector, FVector>, "FMath::Lerp<FVector, FVector>", true);
	FAngelscriptBinds::BindGlobalFunction("FLinearColor Lerp(const FLinearColor& A, const FLinearColor& B, const float32& Alpha) no_discard", &FMath::Lerp<FLinearColor, float>, "FMath::Lerp<FLinearColor, float>", true);

	FAngelscriptBinds::BindGlobalFunction("bool IsWithin(const float64& TestValue, const float64& MinValue, const float64& MaxValue) no_discard", &FMath::IsWithin<double, double>, "FMath::IsWithin<double, double>", true);
	FAngelscriptBinds::BindGlobalFunction("bool IsWithin(const float32& TestValue, const float32&  MinValue, const float32&  MaxValue) no_discard",& FMath::IsWithin<float, float>, "FMath::IsWithin<float, float>", true);
	FAngelscriptBinds::BindGlobalFunction("bool IsWithin(const int32& TestValue, const int32& MinValue, const int32& MaxValue) no_discard", &FMath::IsWithin<int32, int32>, "FMath::IsWithin<int32, int32>", true);

	FAngelscriptBinds::BindGlobalFunction("bool IsWithinInclusive(const float64& TestValue, const float64& MinValue, const float64& MaxValue) no_discard", &FMath::IsWithinInclusive<double, double>, "FMath::IsWithinInclusive<double, double>", true);
	FAngelscriptBinds::BindGlobalFunction("bool IsWithinInclusive(const float32& TestValue, const float32&  MinValue, const float32&  MaxValue) no_discard",& FMath::IsWithinInclusive<float, float>, "FMath::IsWithinInclusive<float, float>", true);
	FAngelscriptBinds::BindGlobalFunction("bool IsWithinInclusive(const int32& TestValue, const int32& MinValue, const int32& MaxValue) no_discard", &FMath::IsWithinInclusive<int32, int32>, "FMath::IsWithinInclusive<int32, int32>", true);
#else
	FAngelscriptBinds::BindGlobalFunction("float64 Lerp(const float64& A, const float64& B, const float64& Alpha) no_discard", &FMath::Lerp<double, double>);
	FAngelscriptBinds::BindGlobalFunction("float32 Lerp(const float32& A, const float32& B, const float32& Alpha) no_discard", &FMath::Lerp<float, float>);

	FAngelscriptBinds::BindGlobalFunction("FVector Lerp(const FVector& A, const FVector& B, const float64& Alpha) no_discard", &FMath::Lerp<FVector, double>);
	FAngelscriptBinds::BindGlobalFunction("FVector2D Lerp(const FVector2D& A, const FVector2D& B, const float64& Alpha) no_discard", &FMath::Lerp<FVector2D, double>);

	FAngelscriptBinds::BindGlobalFunction("FVector3f Lerp(const FVector3f& A, const FVector3f& B, const float32& Alpha) no_discard", &FMath::Lerp<FVector3f, float>);
	FAngelscriptBinds::BindGlobalFunction("FVector2f Lerp(const FVector2f& A, const FVector2f& B, const float32& Alpha) no_discard", &FMath::Lerp<FVector2f, float>);

	FAngelscriptBinds::BindGlobalFunction("FVector VLerp(const FVector& A, const FVector& B, const FVector& Alpha) no_discard", &FMath::Lerp<FVector, FVector>);
	FAngelscriptBinds::BindGlobalFunction("FLinearColor Lerp(const FLinearColor& A, const FLinearColor& B, const float32& Alpha) no_discard", &FMath::Lerp<FLinearColor, float>);

	FAngelscriptBinds::BindGlobalFunction("bool IsWithin(const float64& TestValue, const float64& MinValue, const float64& MaxValue) no_discard", &FMath::IsWithin<double, double>);
	FAngelscriptBinds::BindGlobalFunction("bool IsWithin(const float32& TestValue, const float32&  MinValue, const float32&  MaxValue) no_discard",& FMath::IsWithin<float, float>);
	FAngelscriptBinds::BindGlobalFunction("bool IsWithin(const int32& TestValue, const int32& MinValue, const int32& MaxValue) no_discard", &FMath::IsWithin<int32, int32>);

	FAngelscriptBinds::BindGlobalFunction("bool IsWithinInclusive(const float64& TestValue, const float64& MinValue, const float64& MaxValue) no_discard", &FMath::IsWithinInclusive<double, double>);
	FAngelscriptBinds::BindGlobalFunction("bool IsWithinInclusive(const float32& TestValue, const float32&  MinValue, const float32&  MaxValue) no_discard",& FMath::IsWithinInclusive<float, float>);
	FAngelscriptBinds::BindGlobalFunction("bool IsWithinInclusive(const int32& TestValue, const int32& MinValue, const int32& MaxValue) no_discard", &FMath::IsWithinInclusive<int32, int32>);
#endif

	FAngelscriptBinds::BindGlobalFunction("float64 CubicInterp(const float64& Point0, const float64& Tangent0, const float64& Point1, const float64& Tangent1, const float64& Alpha) no_discard", &FMath::CubicInterp<double, double>, "FMath::CubicInterp<double, double>", true);
	FAngelscriptBinds::BindGlobalFunction("FVector CubicInterp(const FVector& Point0, const FVector& Tangent0, const FVector& Point1, const FVector& Tangent1, const float64& Alpha) no_discard", &FMath::CubicInterp<FVector, double>, "FMath::CubicInterp<FVector, double>", true);
	FAngelscriptBinds::BindGlobalFunction("FQuat CubicInterp(const FQuat& Point0, const FQuat& Tangent0, const FQuat& Point1, const FQuat& Tangent1, const float64& Alpha) no_discard", &FMath::CubicInterp<FQuat,double>, "FMath::CubicInterp<FQuat,double>", true);

	FAngelscriptBinds::BindGlobalFunction("float32 CubicInterp(const float32& Point0, const float32& Tangent0, const float32& Point1, const float32& Tangent1, const float32& Alpha) no_discard", &FMath::CubicInterp<float, float>, "FMath::CubicInterp<float, float>", true);
	FAngelscriptBinds::BindGlobalFunction("FVector CubicInterp(const FVector& Point0, const FVector& Tangent0, const FVector& Point1, const FVector& Tangent1, const float32& Alpha) no_discard", &FMath::CubicInterp<FVector, float>, "FMath::CubicInterp<FVector, float>", true);
	FAngelscriptBinds::BindGlobalFunction("FQuat CubicInterp(const FQuat& Point0, const FQuat& Tangent0, const FQuat& Point1, const FQuat& Tangent1, const float32& Alpha) no_discard", &FMath::CubicInterp<FQuat,float>, "FMath::CubicInterp<FQuat,float>", true);

	FAngelscriptBinds::BindGlobalFunction("FVector3f CubicInterp(const FVector3f& Point0, const FVector3f& Tangent0, const FVector3f& Point1, const FVector3f& Tangent1, const float32& Alpha) no_discard", &FMath::CubicInterp<FVector3f, float>, "FMath::CubicInterp<FVector3f, float>", true);
	FAngelscriptBinds::BindGlobalFunction("FQuat4f CubicInterp(const FQuat4f& Point0, const FQuat4f& Tangent0, const FQuat4f& Point1, const FQuat4f& Tangent1, const float32& Alpha) no_discard", &FMath::CubicInterp<FQuat4f,float>, "FMath::CubicInterp<FQuat4f,float>", true);

	FAngelscriptBinds::BindGlobalFunction("float64 CubicInterpDerivative(const float64& Point0, const float64& Tangent0, const float64& Point1, const float64& Tangent1, const float64& Alpha) no_discard", &FMath::CubicInterpDerivative<double, double>, "FMath::CubicInterpDerivative<double, double>", true);
	FAngelscriptBinds::BindGlobalFunction("FVector CubicInterpDerivative(const FVector& Point0, const FVector& Tangent0, const FVector& Point1, const FVector& Tangent1, const float64& Alpha) no_discard", &FMath::CubicInterpDerivative<FVector, double>, "FMath::CubicInterpDerivative<FVector, double>", true);
	FAngelscriptBinds::BindGlobalFunction("FRotator CubicInterpDerivative(const FRotator& Point0, const FRotator& Tangent0, const FRotator& Point1, const FRotator& Tangent1, const float64& Alpha) no_discard", &FMath::CubicInterpDerivative<FRotator, double>, "FMath::CubicInterpDerivative<FRotator, double>", true);

	FAngelscriptBinds::BindGlobalFunction("float32 CubicInterpDerivative(const float32& Point0, const float32& Tangent0, const float32& Point1, const float32& Tangent1, const float32& Alpha) no_discard", &FMath::CubicInterpDerivative<float, float>, "FMath::CubicInterpDerivative<float, float>", true);
	FAngelscriptBinds::BindGlobalFunction("FVector CubicInterpDerivative(const FVector& Point0, const FVector& Tangent0, const FVector& Point1, const FVector& Tangent1, const float32& Alpha) no_discard", &FMath::CubicInterpDerivative<FVector, float>, "FMath::CubicInterpDerivative<FVector, float>", true);
	FAngelscriptBinds::BindGlobalFunction("FRotator CubicInterpDerivative(const FRotator& Point0, const FRotator& Tangent0, const FRotator& Point1, const FRotator& Tangent1, const float32& Alpha) no_discard", &FMath::CubicInterpDerivative<FRotator, float>, "FMath::CubicInterpDerivative<FRotator, float>", true);

	FAngelscriptBinds::BindGlobalFunction("FVector3f CubicInterpDerivative(const FVector3f& Point0, const FVector3f& Tangent0, const FVector3f& Point1, const FVector3f& Tangent1, const float32& Alpha) no_discard", &FMath::CubicInterpDerivative<FVector3f, float>, "FMath::CubicInterpDerivative<FVector3f, float>", true);
	FAngelscriptBinds::BindGlobalFunction("FRotator3f CubicInterpDerivative(const FRotator3f& Point0, const FRotator3f& Tangent0, const FRotator3f& Point1, const FRotator3f& Tangent1, const float32& Alpha) no_discard", &FMath::CubicInterpDerivative<FRotator3f, float>, "FMath::CubicInterpDerivative<FRotator3f, float>", true);

	FAngelscriptBinds::BindGlobalFunction("FVector VInterpNormalRotationTo(const FVector& Current, const FVector& Target, float32 DeltaTime, float32 RotationSpeedDegrees) no_discard", FUNC_TRIVIAL(FMath::VInterpNormalRotationTo));
	FAngelscriptBinds::BindGlobalFunction("FVector VInterpConstantTo(const FVector& Current, const FVector& Target, float32 DeltaTime, float32 InterpSpeed) no_discard", FUNC_TRIVIAL(FMath::VInterpConstantTo));
	FAngelscriptBinds::BindGlobalFunction("FVector VInterpTo(const FVector& Current, const FVector& Target, float32 DeltaTime, float32 InterpSpeed) no_discard", FUNC_TRIVIAL(FMath::VInterpTo));

	FAngelscriptBinds::BindGlobalFunction("FVector2D Vector2DInterpConstantTo(const FVector2D& Current, const FVector2D& Target, float32 DeltaTime, float32 InterpSpeed) no_discard", FUNC_TRIVIAL(FMath::Vector2DInterpConstantTo));
	FAngelscriptBinds::BindGlobalFunction("FVector2D Vector2DInterpTo(const FVector2D& Current, const FVector2D& Target, float32 DeltaTime, float32 InterpSpeed) no_discard", FUNCPR_TRIVIAL(FVector2D, FMath::Vector2DInterpTo, (const FVector2D&, const FVector2D&, float, float)));

	FAngelscriptBinds::BindGlobalFunction("FRotator RInterpConstantTo(const FRotator& Current, const FRotator& Target, float32 DeltaTime, float32 InterpSpeed) no_discard", FUNC_TRIVIAL(FMath::RInterpConstantTo));
	FAngelscriptBinds::BindGlobalFunction("FRotator RInterpTo(const FRotator& Current, const FRotator& Target, float32 DeltaTime, float32 InterpSpeed) no_discard", FUNC_TRIVIAL(FMath::RInterpTo));
	FAngelscriptBinds::BindGlobalFunction("FRotator RotatorFromAxisAndAngle(FVector Axis, float32 Angle) no_discard", FUNC_TRIVIAL(UKismetMathLibrary::RotatorFromAxisAndAngle));
	
	FAngelscriptBinds::BindGlobalFunction("FVector RandomPointInBoundingBox(const FVector& Center, const FVector& HalfSize) no_discard", FUNC_TRIVIAL(UKismetMathLibrary::RandomPointInBoundingBox));
	FAngelscriptBinds::BindGlobalFunction("FRotator RandomRotator(bool bRoll) no_discard", FUNC_TRIVIAL(UKismetMathLibrary::RandomRotator));

	FAngelscriptBinds::BindGlobalFunction("FQuat QInterpConstantTo(const FQuat& Current, const FQuat& Target, float32 DeltaTime, float32 InterpSpeed) no_discard", FUNC_TRIVIAL(FMath::QInterpConstantTo<double>));
	FAngelscriptBinds::BindGlobalFunction("FQuat QInterpTo(const FQuat& Current, const FQuat& Target, float32 DeltaTime, float32 InterpSpeed) no_discard", FUNC_TRIVIAL(FMath::QInterpTo<double>));

	FAngelscriptBinds::BindGlobalFunction("FQuat4f QInterpConstantTo(const FQuat4f& Current, const FQuat4f& Target, float32 DeltaTime, float32 InterpSpeed) no_discard", FUNC_TRIVIAL(FMath::QInterpConstantTo<float>));
	FAngelscriptBinds::BindGlobalFunction("FQuat4f QInterpTo(const FQuat4f& Current, const FQuat4f& Target, float32 DeltaTime, float32 InterpSpeed) no_discard", FUNC_TRIVIAL(FMath::QInterpTo<float>));

	FAngelscriptBinds::BindGlobalFunction("float32 FInterpConstantTo(float32 Current, float32 Target, float32 DeltaTime, float32 InterpSpeed) no_discard", FUNC_TRIVIAL(FMath::FInterpConstantTo<float>));
	FAngelscriptBinds::BindGlobalFunction("float32 FInterpTo(float32 Current, float32 Target, float32 DeltaTime, float32 InterpSpeed) no_discard", FUNC_TRIVIAL(FMath::FInterpTo<float>));

	FAngelscriptBinds::BindGlobalFunction("float64 FInterpConstantTo(float64 Current, float64 Target, float64 DeltaTime, float64 InterpSpeed) no_discard", FUNC_TRIVIAL(FMath::FInterpConstantTo<double>));
	FAngelscriptBinds::BindGlobalFunction("float64 FInterpTo(float64 Current, float64 Target, float64 DeltaTime, float64 InterpSpeed) no_discard", FUNC_TRIVIAL(FMath::FInterpTo<double>));

	FAngelscriptBinds::BindGlobalFunction("FLinearColor CInterpTo(FLinearColor Current, FLinearColor Target, float32 DeltaTime, float32 InterpSpeed) no_discard", FUNC_TRIVIAL(FMath::CInterpTo));


	FAngelscriptBinds::BindGlobalFunction("bool SphereAABBIntersection(const FVector& SphereCenter, const float64 RadiusSquared, const FBox& AABB) no_discard",
		FUNCPR_TRIVIAL(bool, FMath::SphereAABBIntersection, (const FVector&, const double, const FBox&)));
	FAngelscriptBinds::BindGlobalFunction("bool SphereAABBIntersection(const FSphere& Sphere, const FBox& AABB) no_discard",
		FUNCPR_TRIVIAL(bool, FMath::SphereAABBIntersection, (const FSphere&, const FBox&)));

	FAngelscriptBinds::BindGlobalFunction("bool SphereAABBIntersection(const FVector3f& SphereCenter, const float32 RadiusSquared, const FBox3f& AABB) no_discard",
		FUNCPR_TRIVIAL(bool, FMath::SphereAABBIntersection, (const FVector3f&, const float, const FBox3f&)));
	FAngelscriptBinds::BindGlobalFunction("bool SphereAABBIntersection(const FSphere3f& Sphere, const FBox3f& AABB) no_discard",
		FUNCPR_TRIVIAL(bool, FMath::SphereAABBIntersection, (const FSphere3f&, const FBox3f&)));

	FAngelscriptBinds::BindGlobalFunction("FVector RayPlaneIntersection(const FVector& RayOrigin, const FVector& RayDirection, const FPlane& Plane) no_discard",
		FUNCPR_TRIVIAL(FVector, FMath::RayPlaneIntersection, (const FVector&, const FVector&, const FPlane&)));

	FAngelscriptBinds::BindGlobalFunction("FVector LinePlaneIntersection(const FVector& Point1, const FVector& Point2, const FVector& PlaneOrigin, const FVector& PlaneNormal) no_discard",
		FUNCPR_TRIVIAL(FVector, FMath::LinePlaneIntersection, (const FVector&, const FVector&, const FVector&, const FVector&)));
	FAngelscriptBinds::BindGlobalFunction("FVector LinePlaneIntersection(const FVector& Point1, const FVector& Point2, const FPlane& Plane) no_discard",
		FUNCPR_TRIVIAL(FVector, FMath::LinePlaneIntersection, (const FVector&, const FVector&, const FPlane&)));

	FAngelscriptBinds::BindGlobalFunction("FVector3f LinePlaneIntersection(const FVector3f& Point1, const FVector3f& Point2, const FVector3f& PlaneOrigin, const FVector3f& PlaneNormal) no_discard",
		FUNCPR_TRIVIAL(FVector3f, FMath::LinePlaneIntersection, (const FVector3f&, const FVector3f&, const FVector3f&, const FVector3f&)));

	FAngelscriptBinds::BindGlobalFunction("bool LineSphereIntersection(const FVector3f& Start, const FVector3f& Dir, float32 Length, const FVector3f& Origin, float32 Radius)", FUNC_TRIVIAL(FMath::LineSphereIntersection<float>));
	FAngelscriptBinds::BindGlobalFunction("bool LineSphereIntersection(const FVector& Start, const FVector& Dir, float64 Length, const FVector& Origin, float64 Radius)", FUNC_TRIVIAL(FMath::LineSphereIntersection<double>));

	FAngelscriptBinds::BindGlobalFunction("bool LineBoxIntersection(const FBox& Box, const FVector& Start, const FVector& End, const FVector& StartToEnd)", FUNCPR_TRIVIAL(bool, FMath::LineBoxIntersection, (const FBox&, const FVector&, const FVector&, const FVector&)));

	FAngelscriptBinds::BindGlobalFunction("FVector ClosestPointOnLine(const FVector& LineStart, const FVector& LineEnd, const FVector& Point) no_discard", FUNCPR_TRIVIAL(FVector, FMath::ClosestPointOnLine, (const FVector&, const FVector&, const FVector&)));
	FAngelscriptBinds::BindGlobalFunction("FVector ClosestPointOnInfiniteLine(const FVector& LineStart, const FVector& LineEnd, const FVector& Point) no_discard", FUNC_TRIVIAL(FMath::ClosestPointOnInfiniteLine));

	FAngelscriptBinds::BindGlobalFunction("FSphere ComputeBoundingSphereForCone(const FVector& ConeOrigin, const FVector& ConeDirection, float64 ConeRadius, float64 CosConeAngle, float64 SinConeAngle) no_discard",
		FUNCPR_TRIVIAL(FSphere, FMath::ComputeBoundingSphereForCone, (const FVector&, const FVector&, double, double, double)));

	FAngelscriptBinds::BindGlobalFunction("FSphere3f ComputeBoundingSphereForCone(const FVector3f& ConeOrigin, const FVector3f& ConeDirection, float32 ConeRadius, float32 CosConeAngle, float32 SinConeAngle) no_discard",
		FUNCPR_TRIVIAL(FSphere3f, FMath::ComputeBoundingSphereForCone, (const FVector3f&, const FVector3f&, float, float, float)));

	FAngelscriptBinds::BindGlobalFunction("int32 TruncToInt(float64 F) no_discard", FUNCPR_TRIVIAL(int32, FMath::TruncToInt32, (double)));
	FAngelscriptBinds::BindGlobalFunction("int32 TruncToInt(float32 F) no_discard", FUNCPR_TRIVIAL(int32, FMath::TruncToInt, (float)));
	FAngelscriptBinds::BindGlobalFunction("float64 TruncToFloat(float64 F) no_discard", FUNCPR_TRIVIAL(double, FMath::TruncToFloat, (double)));
	FAngelscriptBinds::BindGlobalFunction("float32 TruncToFloat(float32 F) no_discard", FUNCPR_TRIVIAL(float, FMath::TruncToFloat, (float)));

	FAngelscriptBinds::BindGlobalFunction("float64 TruncToDouble(float64 F) no_discard", FUNCPR_TRIVIAL(double, FMath::TruncToDouble, (double)));
	if (bDeprecateDouble)
		FAngelscriptBinds::DeprecatePreviousBind("Double is deprecated, use float or float64.");

	FAngelscriptBinds::BindGlobalFunction("int32 RoundToInt(float64 F) no_discard", FUNCPR_TRIVIAL(int32, FMath::RoundToInt32, (double)));
	FAngelscriptBinds::BindGlobalFunction("int32 RoundToInt(float32 F) no_discard", FUNCPR_TRIVIAL(int32, FMath::RoundToInt, (float)));
	FAngelscriptBinds::BindGlobalFunction("float64 RoundToFloat(float64 F) no_discard", FUNCPR_TRIVIAL(double, FMath::RoundToFloat, (double)));
	FAngelscriptBinds::BindGlobalFunction("float32 RoundToFloat(float32 F) no_discard", FUNCPR_TRIVIAL(float, FMath::RoundToFloat, (float)));

	FAngelscriptBinds::BindGlobalFunction("float64 RoundToDouble(float64 F) no_discard", FUNCPR_TRIVIAL(double, FMath::RoundToDouble, (double)));
	if (bDeprecateDouble)
		FAngelscriptBinds::DeprecatePreviousBind("Double is deprecated, use float or float64.");

	FAngelscriptBinds::BindGlobalFunction("int32 FloorToInt(float64 F) no_discard", FUNCPR_TRIVIAL(int32, FMath::FloorToInt32, (double)));
	FAngelscriptBinds::BindGlobalFunction("int32 FloorToInt(float32 F) no_discard", FUNCPR_TRIVIAL(int32, FMath::FloorToInt, (float)));
	FAngelscriptBinds::BindGlobalFunction("float64 FloorToFloat(float64 F) no_discard", FUNCPR_TRIVIAL(double, FMath::FloorToFloat, (double)));
	FAngelscriptBinds::BindGlobalFunction("float32 FloorToFloat(float32 F) no_discard", FUNCPR_TRIVIAL(float, FMath::FloorToFloat, (float)));

	FAngelscriptBinds::BindGlobalFunction("float64 FloorToDouble(float64 F) no_discard", FUNCPR_TRIVIAL(double, FMath::FloorToDouble, (double)));
	if (bDeprecateDouble)
		FAngelscriptBinds::DeprecatePreviousBind("Double is deprecated, use float or float64.");

	FAngelscriptBinds::BindGlobalFunction("int32 CeilToInt(float64 F) no_discard", FUNCPR_TRIVIAL(int32, FMath::CeilToInt32, (double)));
	FAngelscriptBinds::BindGlobalFunction("int32 CeilToInt(float32 F) no_discard", FUNCPR_TRIVIAL(int32, FMath::CeilToInt, (float)));
	FAngelscriptBinds::BindGlobalFunction("float64 CeilToFloat(float64 F) no_discard", FUNCPR_TRIVIAL(double, FMath::CeilToFloat, (double)));
	FAngelscriptBinds::BindGlobalFunction("float32 CeilToFloat(float32 F) no_discard", FUNCPR_TRIVIAL(float, FMath::CeilToFloat, (float)));

	FAngelscriptBinds::BindGlobalFunction("float64 CeilToDouble(float64 F) no_discard", FUNCPR_TRIVIAL(double, FMath::CeilToDouble, (double)));
		FAngelscriptBinds::DeprecatePreviousBind("Double is deprecated, use float or float64.");

	FAngelscriptBinds::BindGlobalFunction("float64 RoundFromZero(float64 F) no_discard", FUNCPR_TRIVIAL(double, FMath::RoundFromZero, (double)));
	FAngelscriptBinds::BindGlobalFunction("float32 RoundFromZero(float32 F) no_discard", FUNCPR_TRIVIAL(float, FMath::RoundFromZero, (float)));

	FAngelscriptBinds::BindGlobalFunction("bool IsNaN(float64 F) no_discard", FUNCPR_TRIVIAL(bool, FMath::IsNaN, (double)));
	FAngelscriptBinds::BindGlobalFunction("bool IsFinite(float64 F) no_discard", FUNCPR_TRIVIAL(bool, FMath::IsFinite, (double)));

	FAngelscriptBinds::BindGlobalFunction("float64 InvSqrt(float64 F) no_discard", FUNCPR_TRIVIAL(double, FMath::InvSqrt, (double)));
	FAngelscriptBinds::BindGlobalFunction("float64 InvSqrtEst(float64 F) no_discard", FUNCPR_TRIVIAL(double, FMath::InvSqrtEst, (double)));

	FAngelscriptBinds::BindGlobalFunction("float64 Fractional(float64 Value) no_discard", FUNCPR_TRIVIAL(double, FMath::Fractional, (double)));
	FAngelscriptBinds::BindGlobalFunction("float64 Frac(float64 Value) no_discard", FUNCPR_TRIVIAL(double, FMath::Frac, (double)));

	FAngelscriptBinds::BindGlobalFunction("bool IsNaN(float32 F) no_discard", FUNCPR_TRIVIAL(bool, FMath::IsNaN, (float)));
	FAngelscriptBinds::BindGlobalFunction("bool IsFinite(float32 F) no_discard", FUNCPR_TRIVIAL(bool, FMath::IsFinite, (float)));

	FAngelscriptBinds::BindGlobalFunction("float32 InvSqrt(float32 F) no_discard", FUNCPR_TRIVIAL(float, FMath::InvSqrt, (float)));
	FAngelscriptBinds::BindGlobalFunction("float32 InvSqrtEst(float32 F) no_discard", FUNCPR_TRIVIAL(float, FMath::InvSqrtEst, (float)));

	FAngelscriptBinds::BindGlobalFunction("float32 Fractional(float32 Value) no_discard", FUNCPR_TRIVIAL(float, FMath::Fractional, (float)));
	FAngelscriptBinds::BindGlobalFunction("float32 Frac(float32 Value) no_discard", FUNCPR_TRIVIAL(float, FMath::Frac, (float)));

	FAngelscriptBinds::BindGlobalFunction("float64 Exp(float64 Value) no_discard", FUNCPR_TRIVIAL(double, FMath::Exp, (double)));
	FAngelscriptBinds::BindGlobalFunction("float64 Exp2(float64 Value) no_discard", FUNCPR_TRIVIAL(double, FMath::Exp2, (double)));
	FAngelscriptBinds::BindGlobalFunction("float64 Loge(float64 Value) no_discard", FUNCPR_TRIVIAL(double, FMath::Loge, (double)));
	FAngelscriptBinds::BindGlobalFunction("float64 Log2(float64 Value) no_discard", FUNCPR_TRIVIAL(double, FMath::Log2, (double)));
	FAngelscriptBinds::BindGlobalFunction("float64 LogX(float64 Base, float64 Value) no_discard", FUNCPR_TRIVIAL(double, FMath::LogX, (double, double)));

	FAngelscriptBinds::BindGlobalFunction("float64 Fmod(float64 X, float64 Y) no_discard", FUNCPR_TRIVIAL(double, FMath::Fmod, (double, double)));
	FAngelscriptBinds::BindGlobalFunction("float64 Sin(float64 Value) no_discard", FUNCPR_TRIVIAL(double, FMath::Sin, (double)));
	FAngelscriptBinds::BindGlobalFunction("float64 Asin(float64 Value) no_discard", FUNCPR_TRIVIAL(double, FMath::Asin, (double)));
	FAngelscriptBinds::BindGlobalFunction("float64 Sinh(float64 Value) no_discard", FUNCPR_TRIVIAL(double, FMath::Sinh, (double)));
	FAngelscriptBinds::BindGlobalFunction("float64 Cos(float64 Value) no_discard", FUNCPR_TRIVIAL(double, FMath::Cos, (double)));
	FAngelscriptBinds::BindGlobalFunction("float64 Acos(float64 Value) no_discard", FUNCPR_TRIVIAL(double, FMath::Acos, (double)));
	FAngelscriptBinds::BindGlobalFunction("float64 Tan(float64 Value) no_discard", FUNCPR_TRIVIAL(double, FMath::Tan, (double)));
	FAngelscriptBinds::BindGlobalFunction("float64 Atan(float64 Value) no_discard", FUNCPR_TRIVIAL(double, FMath::Atan, (double)));
	FAngelscriptBinds::BindGlobalFunction("float64 Atan2(float64 Y, float64 X) no_discard", FUNCPR_TRIVIAL(double, FMath::Atan2, (double, double)));
	FAngelscriptBinds::BindGlobalFunction("float64 Sqrt(float64 Value) no_discard", FUNCPR_TRIVIAL(double, FMath::Sqrt, (double)));
	FAngelscriptBinds::BindGlobalFunction("float64 Pow(float64 A, float64 B) no_discard", FUNCPR_TRIVIAL(double, FMath::Pow, (double, double)));

	FAngelscriptBinds::BindGlobalFunction("float32 Exp(float32 Value) no_discard", FUNCPR_TRIVIAL(float, FMath::Exp, (float)));
	FAngelscriptBinds::BindGlobalFunction("float32 Exp2(float32 Value) no_discard", FUNCPR_TRIVIAL(float, FMath::Exp2, (float)));
	FAngelscriptBinds::BindGlobalFunction("float32 Loge(float32 Value) no_discard", FUNCPR_TRIVIAL(float, FMath::Loge, (float)));
	FAngelscriptBinds::BindGlobalFunction("float32 Log2(float32 Value) no_discard", FUNCPR_TRIVIAL(float, FMath::Log2, (float)));
	FAngelscriptBinds::BindGlobalFunction("float32 LogX(float32 Base, float32 Value) no_discard", FUNCPR_TRIVIAL(float, FMath::LogX, (float, float)));

	FAngelscriptBinds::BindGlobalFunction("float32 Fmod(float32 X, float32 Y) no_discard", FUNCPR_TRIVIAL(float, FMath::Fmod, (float, float)));
	FAngelscriptBinds::BindGlobalFunction("float32 Sin(float32 Value) no_discard", FUNCPR_TRIVIAL(float, FMath::Sin, (float)));
	FAngelscriptBinds::BindGlobalFunction("float32 Asin(float32 Value) no_discard", FUNCPR_TRIVIAL(float, FMath::Asin, (float)));
	FAngelscriptBinds::BindGlobalFunction("float32 Sinh(float32 Value) no_discard", FUNCPR_TRIVIAL(float, FMath::Sinh, (float)));
	FAngelscriptBinds::BindGlobalFunction("float32 Cos(float32 Value) no_discard", FUNCPR_TRIVIAL(float, FMath::Cos, (float)));
	FAngelscriptBinds::BindGlobalFunction("float32 Acos(float32 Value) no_discard", FUNCPR_TRIVIAL(float, FMath::Acos, (float)));
	FAngelscriptBinds::BindGlobalFunction("float32 Tan(float32 Value) no_discard", FUNCPR_TRIVIAL(float, FMath::Tan, (float)));
	FAngelscriptBinds::BindGlobalFunction("float32 Atan(float32 Value) no_discard", FUNCPR_TRIVIAL(float, FMath::Atan, (float)));
	FAngelscriptBinds::BindGlobalFunction("float32 Atan2(float32 Y, float32 X) no_discard", FUNCPR_TRIVIAL(float, FMath::Atan2, (float, float)));
	FAngelscriptBinds::BindGlobalFunction("float32 Sqrt(float32 Value) no_discard", FUNCPR_TRIVIAL(float, FMath::Sqrt, (float)));
	FAngelscriptBinds::BindGlobalFunction("float32 Pow(float32 A, float32 B) no_discard", FUNCPR_TRIVIAL(float, FMath::Pow, (float, float)));

	FAngelscriptBinds::BindGlobalFunction("int32 Rand() no_discard", FUNC_TRIVIAL(FMath::Rand));
	FAngelscriptBinds::BindGlobalFunction("float32 FRand() no_discard", FUNC_TRIVIAL(FMath::FRand));

	FAngelscriptBinds::BindGlobalFunction("float64 Abs(float64 Value) no_discard", FUNC_TRIVIAL(FMath::Abs<double>));
	FAngelscriptBinds::BindGlobalFunction("float32 Abs(float32 Value) no_discard", FUNC_TRIVIAL(FMath::Abs<float>));
	FAngelscriptBinds::BindGlobalFunction("int32 Abs(int32 Value) no_discard", FUNC_TRIVIAL(FMath::Abs<int32>));

	FAngelscriptBinds::BindGlobalFunction("float64 Sign(float64 Value) no_discard", FUNC_TRIVIAL(FMath::Sign<double>));
	FAngelscriptBinds::BindGlobalFunction("float32 Sign(float32 Value) no_discard", FUNC_TRIVIAL(FMath::Sign<float>));
	FAngelscriptBinds::BindGlobalFunction("int32 Sign(int32 Value) no_discard", FUNC_TRIVIAL(FMath::Sign<int32>));

	FAngelscriptBinds::BindGlobalFunction("float64 Min(float64 A, float64 B) no_discard", FUNCPR_TRIVIAL(double, FMath::Min<double>, (const double, const double)));
	FAngelscriptBinds::BindGlobalFunction("float32 Min(float32 A, float32 B) no_discard", FUNCPR_TRIVIAL(float, FMath::Min<float>, (const float, const float)));
	FAngelscriptBinds::BindGlobalFunction("int32 Min(int32 A, int32 B) no_discard", FUNCPR_TRIVIAL(int32, FMath::Min<int32>, (const int32, const int32)));
	FAngelscriptBinds::BindGlobalFunction("uint32 Min(uint32 A, uint32 B) no_discard", FUNCPR_TRIVIAL(uint32, FMath::Min<uint32>, (const uint32, const uint32)));

	FAngelscriptBinds::BindGlobalFunction("float64 Max3(float64 A, float64 B, float64 C) no_discard", FUNCPR_TRIVIAL(double, FMath::Max3<double>, (const double, const double, const double)));
	FAngelscriptBinds::BindGlobalFunction("float32 Max3(float32 A, float32 B, float32 C) no_discard", FUNCPR_TRIVIAL(float, FMath::Max3<float>, (const float, const float, const float)));

	FAngelscriptBinds::BindGlobalFunction("float64 Max(float64 A, float64 B) no_discard", FUNCPR_TRIVIAL(double, FMath::Max<double>, (const double, const double)));
	FAngelscriptBinds::BindGlobalFunction("float32 Max(float32 A, float32 B) no_discard", FUNCPR_TRIVIAL(float, FMath::Max<float>, (const float, const float)));
	FAngelscriptBinds::BindGlobalFunction("int32 Max(int32 A, int32 B) no_discard", FUNCPR_TRIVIAL(int32, FMath::Max<int32>, (const int32, const int32)));
	FAngelscriptBinds::BindGlobalFunction("uint32 Max(uint32 A, uint32 B) no_discard", FUNCPR_TRIVIAL(uint32, FMath::Max<uint32>, (const uint32, const uint32)));

	FAngelscriptBinds::BindGlobalFunction("float64 Square(float64 Value) no_discard", FUNC_TRIVIAL(FMath::Square<double>));
	FAngelscriptBinds::BindGlobalFunction("float32 Square(float32 Value) no_discard", FUNC_TRIVIAL(FMath::Square<float>));
	FAngelscriptBinds::BindGlobalFunction("int32 Square(int32 Value) no_discard", FUNC_TRIVIAL(FMath::Square<int32>));
	FAngelscriptBinds::BindGlobalFunction("uint32 Square(uint32 Value) no_discard", FUNC_TRIVIAL(FMath::Square<uint32>));

	FAngelscriptBinds::BindGlobalFunction("float64 GetMappedRangeValueClamped(const FVector2D& InputRange, const FVector2D& OutputRange, const float64 Value) no_discard", FUNCPR_TRIVIAL(double, FMath::GetMappedRangeValueClamped, (const FVector2D&, const FVector2D&, const double)));
	FAngelscriptBinds::BindGlobalFunction("float64 GetMappedRangeValueUnclamped(const FVector2D& InputRange, const FVector2D& OutputRange, const float64 Value) no_discard", FUNCPR_TRIVIAL(double, FMath::GetMappedRangeValueUnclamped, (const FVector2D&, const FVector2D&, const double)));

	FAngelscriptBinds::BindGlobalFunction("float32 GetMappedRangeValueClamped(const FVector2f& InputRange, const FVector2f& OutputRange, const float32 Value) no_discard", FUNCPR_TRIVIAL(float, FMath::GetMappedRangeValueClamped, (const FVector2f&, const FVector2f&, const float)));
	FAngelscriptBinds::BindGlobalFunction("float32 GetMappedRangeValueUnclamped(const FVector2f& InputRange, const FVector2f& OutputRange, const float32 Value) no_discard", FUNCPR_TRIVIAL(float, FMath::GetMappedRangeValueUnclamped, (const FVector2f&, const FVector2f&, const float)));

	FAngelscriptBinds::BindGlobalFunction("float32 PerlinNoise1D(float32 X) no_discard", FUNC_TRIVIAL(FMath::PerlinNoise1D));
	SCRIPT_BIND_DOCUMENTATION(
	"Generates a 1D Perlin noise from the given value.  Returns a continuous random value between -1.0 and 1.0.\n"
    "@param	Value	The input value that Perlin noise will be generated from.  This is usually a steadily incrementing time value.\n"
    "@return	Perlin noise in the range of -1.0 to 1.0"
    )
	
	FAngelscriptBinds::BindGlobalFunction("float32 PerlinNoise2D(const FVector2D& Location) no_discard", FUNC_TRIVIAL(FMath::PerlinNoise2D));
	SCRIPT_BIND_DOCUMENTATION(
	"Generates a 1D Perlin noise sample at the given location.  Returns a continuous random value between -1.0 and 1.0.\n"
    "@param	Location	Where to sample\n"
    "@return	Perlin noise in the range of -1.0 to 1.0\n"
	)

	FAngelscriptBinds::BindGlobalFunction("float32 PerlinNoise3D(const FVector& Location) no_discard", FUNC_TRIVIAL(FMath::PerlinNoise3D));
	SCRIPT_BIND_DOCUMENTATION(
	"Generates a 3D Perlin noise sample at the given location.  Returns a continuous random value between -1.0 and 1.0.\n"
    "@param	Location	Where to sample\n"
    "@return	Perlin noise in the range of -1.0 to 1.0"
	)


	FAngelscriptBinds::BindGlobalFunction("float64 GridSnap(float64 Location, float64 Grid) no_discard", FUNCPR_TRIVIAL(double, FMath::GridSnap, (double, double)));
	FAngelscriptBinds::BindGlobalFunction("float32 GridSnap(float32 Location, float32 Grid) no_discard", FUNCPR_TRIVIAL(float, FMath::GridSnap, (float, float)));

	FAngelscriptBinds::BindGlobalFunction("bool SegmentIntersection2D(const FVector& SegmentStartA, const FVector& SegmentEndA, const FVector& SegmentStartB, const FVector& SegmentEndB, FVector& out_IntersectionPoint)", FUNC_TRIVIAL(FMath::SegmentIntersection2D));
	SCRIPT_BIND_DOCUMENTATION(
	"Returns true if there is an intersection between the segment specified by SegmentStartA and SegmentEndA, and\n"
    "the segment specified by SegmentStartB and SegmentEndB, in 2D space. If there is an intersection, the point is placed in out_IntersectionPoint\n"
    "@param SegmentStartA - start point of first segment\n"
    "@param SegmentEndA   - end point of first segment\n"
    "@param SegmentStartB - start point of second segment\n"
    "@param SegmentEndB   - end point of second segment\n"
    "@param out_IntersectionPoint - out var for the intersection point (if any)\n"
    "@return true if intersection occurred"
    )

	FAngelscriptBinds::BindGlobalFunction("float32 FloatSpringInterp(float32 Current, float32 Target, FFloatSpringState& SpringState, float32 Stiffness, float32 CriticalDampingFactor, float32 DeltaTime, float32 Mass = 1.f, float32 TargetVelocityAmount = 1.f)",
	[](float Current, float Target, FFloatSpringState& SpringState, float Stiffness, float CriticalDampingFactor, float DeltaTime, float Mass = 1.f, float TargetVelocityAmount = 1.f) -> float
	{
		return UKismetMathLibrary::FloatSpringInterp(Current, Target, SpringState, Stiffness, CriticalDampingFactor, DeltaTime, Mass, TargetVelocityAmount);
	});
	SCRIPT_BIND_DOCUMENTATION(
		"Uses a simple spring model to interpolate a float32 from Current to Target.\n"
		"@param Current				Current value\n"
		"@param Target				Target value\n"
		"@param SpringState			Data related to spring model (velocity, error, etc..) - Create a unique variable per spring\n"
		"@param Stiffness				How stiff the spring model is (more stiffness means more oscillation around the target value)\n"
		"@param CriticalDampingFactor	How much damping to apply to the spring (0 means no damping, 1 means critically damped which means no oscillation)\n"
		"@param Mass					Multiplier that acts like mass on a spring\n"
		"@param TargetVelocityAmount	If 1 then the target velocity will be calculated and used, which results following the target more closely/without lag. Values down to zero (recommended when using this to smooth data) will progressively disable this effect.")

	FAngelscriptBinds::BindGlobalFunction("FVector VectorSpringInterp(FVector Current, FVector Target, FVectorSpringState& SpringState, float32 Stiffness, float32 CriticalDampingFactor, float32 DeltaTime, float32 Mass = 1.f, float32 TargetVelocityAmount = 1.f)",
	[](FVector Current, FVector Target, FVectorSpringState& SpringState, float Stiffness, float CriticalDampingFactor, float DeltaTime, float Mass = 1.f, float TargetVelocityAmount = 1.f) -> FVector
	{
		return UKismetMathLibrary::VectorSpringInterp(Current, Target, SpringState, Stiffness, CriticalDampingFactor, DeltaTime, Mass, TargetVelocityAmount);
	});
	SCRIPT_BIND_DOCUMENTATION(
		"Uses a simple spring model to interpolate a vector from Current to Target.\n"
		"@param Current				Current value\n"
		"@param Target				Target value\n"
		"@param SpringState			Data related to spring model (velocity, error, etc..) - Create a unique variable per spring\n"
		"@param Stiffness				How stiff the spring model is (more stiffness means more oscillation around the target value)\n"
		"@param CriticalDampingFactor	How much damping to apply to the spring (0 means no damping, 1 means critically damped which means no oscillation)\n"
		"@param Mass					Multiplier that acts like mass on a spring\n"
		"@param TargetVelocityAmount	If 1 then the target velocity will be calculated and used, which results following the target more closely/without lag. Values down to zero (recommended when using this to smooth data) will progressively disable this effect.")

	FAngelscriptBinds::BindGlobalFunction("FQuat QuaternionSpringInterp(FQuat Current, FQuat Target, FQuaternionSpringState& SpringState, float32 Stiffness, float32 CriticalDampingFactor, float32 DeltaTime, float32 Mass = 1.f, float32 TargetVelocityAmount = 1.f)",
	[](FQuat Current, FQuat Target, FQuaternionSpringState& SpringState, float Stiffness, float CriticalDampingFactor, float DeltaTime, float Mass = 1.f, float TargetVelocityAmount = 1.f) -> FQuat
	{
		return UKismetMathLibrary::QuaternionSpringInterp(Current, Target, SpringState, Stiffness, CriticalDampingFactor, DeltaTime, Mass, TargetVelocityAmount);
	});
	SCRIPT_BIND_DOCUMENTATION(
		"Uses a simple spring model to interpolate a quaternion from Current to Target.\n"
		"@param Current				Current value\n"
		"@param Target				Target value\n"
		"@param SpringState			Data related to spring model (velocity, error, etc..) - Create a unique variable per spring\n"
		"@param Stiffness				How stiff the spring model is (more stiffness means more oscillation around the target value)\n"
		"@param CriticalDampingFactor	How much damping to apply to the spring (0 means no damping, 1 means critically damped which means no oscillation)\n"
		"@param Mass					Multiplier that acts like mass on a spring\n"
		"@param TargetVelocityAmount	If 1 then the target velocity will be calculated and used, which results following the target more closely/without lag. Values down to zero (recommended when using this to smooth data) will progressively disable this effect.")

	// Ease double
	FAngelscriptBinds::BindGlobalFunction("float64 EaseIn(const float64& A, const float64& B, float64 Alpha, float64 Exp) no_discard", [](const double& A, const double& B, double Alpha, double Exp)
	{
		return FMath::InterpEaseIn<double>(A, B, Alpha, Exp);
	});

	FAngelscriptBinds::BindGlobalFunction("float64 EaseOut(const float64& A, const float64& B, float64 Alpha, float64 Exp) no_discard", [](const double& A, const double& B, double Alpha, double Exp)
	{
		return FMath::InterpEaseOut<double>(A, B, Alpha, Exp);
	});
	
	FAngelscriptBinds::BindGlobalFunction("float64 EaseInOut(const float64& A, const float64& B, float64 Alpha, float64 Exp) no_discard", [](const double& A, const double& B, double Alpha, double Exp)
	{
		return FMath::InterpEaseInOut<double>(A, B, Alpha, Exp);
	});

	FAngelscriptBinds::BindGlobalFunction("float64 SinusoidalIn(const float64& A, const float64& B, float64 Alpha) no_discard", [](const double& A, const double& B, double Alpha)
	{
		return FMath::InterpSinIn<double>(A, B, Alpha);
	});

	FAngelscriptBinds::BindGlobalFunction("float64 SinusoidalOut(const float64& A, const float64& B, float64 Alpha) no_discard", [](const double& A, const double& B, double Alpha)
	{
		return FMath::InterpSinOut<double>(A, B, Alpha);
	});

	FAngelscriptBinds::BindGlobalFunction("float64 SinusoidalInOut(const float64& A, const float64& B, float64 Alpha) no_discard", [](const double& A, const double& B, double Alpha)
	{
		return FMath::InterpSinInOut<double>(A, B, Alpha);
	});

	FAngelscriptBinds::BindGlobalFunction("float64 ExpoIn(const float64& A, const float64& B, float64 Alpha) no_discard", [](const double& A, const double& B, double Alpha)
	{
		return FMath::InterpExpoIn<double>(A, B, Alpha);
	});

	FAngelscriptBinds::BindGlobalFunction("float64 ExpoOut(const float64& A, const float64& B, float64 Alpha) no_discard", [](const double& A, const double& B, double Alpha)
	{
		return FMath::InterpExpoOut<double>(A, B, Alpha);
	});

	FAngelscriptBinds::BindGlobalFunction("float64 ExpoInOut(const float64& A, const float64& B, float64 Alpha) no_discard", [](const double& A, const double& B, double Alpha)
	{
		return FMath::InterpExpoInOut<double>(A, B, Alpha);
	});

	FAngelscriptBinds::BindGlobalFunction("float64 CircularIn(const float64& A, const float64& B, float64 Alpha) no_discard", [](const double& A, const double& B, double Alpha)
	{
		return FMath::InterpCircularIn<double>(A, B, Alpha);
	});

	FAngelscriptBinds::BindGlobalFunction("float64 CircularOut(const float64& A, const float64& B, float64 Alpha) no_discard", [](const double& A, const double& B, double Alpha)
	{
		return FMath::InterpCircularOut<double>(A, B, Alpha);
	});

	FAngelscriptBinds::BindGlobalFunction("float64 CircularInOut(const float64& A, const float64& B, float64 Alpha) no_discard", [](const double& A, const double& B, double Alpha)
	{
		return FMath::InterpCircularInOut<double>(A, B, Alpha);
	});

	// Ease float
	FAngelscriptBinds::BindGlobalFunction("float32 EaseIn(const float32& A, const float32& B, float32 Alpha, float32 Exp) no_discard", [](const float& A, const float& B, float Alpha, float Exp)
	{
		return FMath::InterpEaseIn<float>(A, B, Alpha, Exp);
	});

	FAngelscriptBinds::BindGlobalFunction("float32 EaseOut(const float32& A, const float32& B, float32 Alpha, float32 Exp) no_discard", [](const float& A, const float& B, float Alpha, float Exp)
	{
		return FMath::InterpEaseOut<float>(A, B, Alpha, Exp);
	});
	
	FAngelscriptBinds::BindGlobalFunction("float32 EaseInOut(const float32& A, const float32& B, float32 Alpha, float32 Exp) no_discard", [](const float& A, const float& B, float Alpha, float Exp)
	{
		return FMath::InterpEaseInOut<float>(A, B, Alpha, Exp);
	});

	FAngelscriptBinds::BindGlobalFunction("float32 SinusoidalIn(const float32& A, const float32& B, float32 Alpha) no_discard", [](const float& A, const float& B, float Alpha)
	{
		return FMath::InterpSinIn<float>(A, B, Alpha);
	});

	FAngelscriptBinds::BindGlobalFunction("float32 SinusoidalOut(const float32& A, const float32& B, float32 Alpha) no_discard", [](const float& A, const float& B, float Alpha)
	{
		return FMath::InterpSinOut<float>(A, B, Alpha);
	});

	FAngelscriptBinds::BindGlobalFunction("float32 SinusoidalInOut(const float32& A, const float32& B, float32 Alpha) no_discard", [](const float& A, const float& B, float Alpha)
	{
		return FMath::InterpSinInOut<float>(A, B, Alpha);
	});

	FAngelscriptBinds::BindGlobalFunction("float32 ExpoIn(const float32& A, const float32& B, float32 Alpha) no_discard", [](const float& A, const float& B, float Alpha)
	{
		return FMath::InterpExpoIn<float>(A, B, Alpha);
	});

	FAngelscriptBinds::BindGlobalFunction("float32 ExpoOut(const float32& A, const float32& B, float32 Alpha) no_discard", [](const float& A, const float& B, float Alpha)
	{
		return FMath::InterpExpoOut<float>(A, B, Alpha);
	});

	FAngelscriptBinds::BindGlobalFunction("float32 ExpoInOut(const float32& A, const float32& B, float32 Alpha) no_discard", [](const float& A, const float& B, float Alpha)
	{
		return FMath::InterpExpoInOut<float>(A, B, Alpha);
	});

	FAngelscriptBinds::BindGlobalFunction("float32 CircularIn(const float32& A, const float32& B, float32 Alpha) no_discard", [](const float& A, const float& B, float Alpha)
	{
		return FMath::InterpCircularIn<float>(A, B, Alpha);
	});

	FAngelscriptBinds::BindGlobalFunction("float32 CircularOut(const float32& A, const float32& B, float32 Alpha) no_discard", [](const float& A, const float& B, float Alpha)
	{
		return FMath::InterpCircularOut<float>(A, B, Alpha);
	});

	FAngelscriptBinds::BindGlobalFunction("float32 CircularInOut(const float32& A, const float32& B, float32 Alpha) no_discard", [](const float& A, const float& B, float Alpha)
	{
		return FMath::InterpCircularInOut<float>(A, B, Alpha);
	});

	// Ease Vector
	FAngelscriptBinds::BindGlobalFunction("FVector EaseIn(const FVector& A, const FVector& B, float32 Alpha, float32 Exp) no_discard", [](const FVector& A, const FVector& B, float Alpha, float Exp)
	{
		return FMath::InterpEaseIn<FVector>(A, B, Alpha, Exp);
	});

	FAngelscriptBinds::BindGlobalFunction("FVector EaseOut(const FVector& A, const FVector& B, float32 Alpha, float32 Exp) no_discard", [](const FVector& A, const FVector& B, float Alpha, float Exp)
	{
		return FMath::InterpEaseOut<FVector>(A, B, Alpha, Exp);
	});

	FAngelscriptBinds::BindGlobalFunction("FVector EaseInOut(const FVector& A, const FVector& B, float32 Alpha, float32 Exp) no_discard", [](const FVector& A, const FVector& B, float Alpha, float Exp)
	{
		return FMath::InterpEaseInOut<FVector>(A, B, Alpha, Exp);
	});

	FAngelscriptBinds::BindGlobalFunction("FVector SinusoidalIn(const FVector& A, const FVector& B, float32 Alpha) no_discard", [](const FVector& A, const FVector& B, float Alpha)
	{
		return FMath::InterpSinIn<FVector>(A, B, Alpha);
	});

	FAngelscriptBinds::BindGlobalFunction("FVector SinusoidalOut(const FVector& A, const FVector& B, float32 Alpha) no_discard", [](const FVector& A, const FVector& B, float Alpha)
	{
		return FMath::InterpSinOut<FVector>(A, B, Alpha);
	});

	FAngelscriptBinds::BindGlobalFunction("FVector SinusoidalInOut(const FVector& A, const FVector& B, float32 Alpha) no_discard", [](const FVector& A, const FVector& B, float Alpha)
	{
		return FMath::InterpSinInOut<FVector>(A, B, Alpha);
	});

	FAngelscriptBinds::BindGlobalFunction("FVector ExpoIn(const FVector& A, const FVector& B, float32 Alpha) no_discard", [](const FVector& A, const FVector& B, float Alpha)
	{
		return FMath::InterpExpoIn<FVector>(A, B, Alpha);
	});

	FAngelscriptBinds::BindGlobalFunction("FVector ExpoOut(const FVector& A, const FVector& B, float32 Alpha) no_discard", [](const FVector& A, const FVector& B, float Alpha)
	{
		return FMath::InterpExpoOut<FVector>(A, B, Alpha);
	});

	FAngelscriptBinds::BindGlobalFunction("FVector ExpoInOut(const FVector& A, const FVector& B, float32 Alpha) no_discard", [](const FVector& A, const FVector& B, float Alpha)
	{
		return FMath::InterpExpoInOut<FVector>(A, B, Alpha);
	});

	FAngelscriptBinds::BindGlobalFunction("FVector CircularIn(const FVector& A, const FVector& B, float32 Alpha) no_discard", [](const FVector& A, const FVector& B, float Alpha)
	{
		return FMath::InterpCircularIn<FVector>(A, B, Alpha);
	});

	FAngelscriptBinds::BindGlobalFunction("FVector CircularOut(const FVector& A, const FVector& B, float32 Alpha) no_discard", [](const FVector& A, const FVector& B, float Alpha)
	{
		return FMath::InterpCircularOut<FVector>(A, B, Alpha);
	});

	FAngelscriptBinds::BindGlobalFunction("FVector CircularInOut(const FVector& A, const FVector& B, float32 Alpha) no_discard", [](const FVector& A, const FVector& B, float Alpha)
	{
		return FMath::InterpCircularInOut<FVector>(A, B, Alpha);
	});

	FAngelscriptBinds::BindGlobalFunction("bool IsPointInBox(const FVector& Point, const FVector& BoxOrigin, const FVector& BoxExtent) no_discard",
	[](const FVector& Point, const FVector& BoxOrigin, const FVector& BoxExtent) -> bool
	{
		return UKismetMathLibrary::IsPointInBox(Point, BoxOrigin, BoxExtent);
	});

	FAngelscriptBinds::BindGlobalFunction("bool IsPointInBoxWithTransform(const FVector& Point, const FTransform& BoxWorldTransform, const FVector& BoxExtent) no_discard",
	[](const FVector& Point, const FTransform& BoxWorldTransform, const FVector& BoxExtent) -> bool
	{
		return UKismetMathLibrary::IsPointInBoxWithTransform(Point, BoxWorldTransform, BoxExtent);
	});

	FAngelscriptBinds::BindGlobalFunction("void FindNearestPointsOnLineSegments(FVector Segment1Start, FVector Segment1End, FVector Segment2Start, FVector Segment2End, FVector& Segment1Point, FVector& Segment2Point)",
	[](FVector Segment1Start, FVector Segment1End, FVector Segment2Start, FVector Segment2End, FVector& Segment1Point, FVector& Segment2Point)
	{
		return UKismetMathLibrary::FindNearestPointsOnLineSegments(Segment1Start, Segment1End, Segment2Start, Segment2End, Segment1Point, Segment2Point);
	});

	FAngelscriptBinds::BindGlobalFunction("float64 NormalizeToRange(float64 Value, float64 RangeMin, float64 RangeMax) no_discard", FUNCPR_TRIVIAL(double, UKismetMathLibrary::NormalizeToRange, (double, double, double)));

	FAngelscriptBinds::BindGlobalFunction("int32 IntegerDivisionTrunc(int32 Value, int32 DivideBy) no_discard",
	[](int32 Value, int32 DivideBy) -> int32
	{
		if (DivideBy == 0)
		{
			FAngelscriptEngine::Throw("Division by zero");
			return 0;
		}
		else if (Value == int32(0x80000000))
		{
			FAngelscriptEngine::Throw("Overflow in integer division");
			return 0;
		}

		return Value / DivideBy;
	});
	SCRIPT_BIND_DOCUMENTATION("Divide integer Value by DivideBy, truncating any decimals from the result.");

	FAngelscriptBinds::BindGlobalFunction("int64 IntegerDivisionTrunc(int64 Value, int64 DivideBy) no_discard",
	[](int64 Value, int64 DivideBy) -> int64
	{
		if (DivideBy == 0)
		{
			FAngelscriptEngine::Throw("Division by zero");
			return 0;
		}
		else if (Value == (int64(1)<<63))
		{
			FAngelscriptEngine::Throw("Overflow in integer division");
			return 0;
		}

		return Value / DivideBy;
	});
	SCRIPT_BIND_DOCUMENTATION("Divide integer Value by DivideBy, truncating any decimals from the result.");

	FAngelscriptBinds::BindGlobalFunction("uint32 IntegerDivisionTrunc(uint32 Value, uint32 DivideBy) no_discard",
	[](uint32 Value, uint32 DivideBy) -> uint32
	{
		if (DivideBy == 0)
		{
			FAngelscriptEngine::Throw("Division by zero");
			return 0;
		}

		return Value / DivideBy;
	});
	SCRIPT_BIND_DOCUMENTATION("Divide integer Value by DivideBy, truncating any decimals from the result.");

	FAngelscriptBinds::BindGlobalFunction("uint64 IntegerDivisionTrunc(uint64 Value, uint64 DivideBy) no_discard",
	[](uint64 Value, uint64 DivideBy) -> uint64
	{
		if (DivideBy == 0)
		{
			FAngelscriptEngine::Throw("Division by zero");
			return 0;
		}

		return Value / DivideBy;
	});
	SCRIPT_BIND_DOCUMENTATION("Divide integer Value by DivideBy, truncating any decimals from the result.");

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
});
