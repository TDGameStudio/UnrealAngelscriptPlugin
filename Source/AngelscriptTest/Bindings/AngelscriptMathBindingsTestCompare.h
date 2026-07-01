// Bindings-local compare/reference helpers for Math binding tests.
// Execute paths use Shared/AngelscriptTestExecute.h (FAngelscriptTestExecutor).

#pragma once

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Math/Quat.h"

#if WITH_ANGELSCRIPT_UNITTESTS

inline bool MathBindingsRotatorMatches(const FRotator& Actual, const FRotator& Expected, double ToleranceDegrees = 0.05)
{
	FQuat ActualQuat(Actual);
	FQuat ExpectedQuat(Expected);
	ActualQuat.Normalize();
	ExpectedQuat.Normalize();
	return FMath::RadiansToDegrees(ActualQuat.AngularDistance(ExpectedQuat)) <= ToleranceDegrees;
}

inline bool VerifyMathBindingsRotator(
	FAutomationTestBase& Test,
	const TCHAR* What,
	const FRotator& Actual,
	const FRotator& Expected,
	double ToleranceDegrees = 0.05)
{
	return Test.TestTrue(What, MathBindingsRotatorMatches(Actual, Expected, ToleranceDegrees));
}

inline bool MathBindingsQuatMatches(const FQuat& Actual, const FQuat& Expected, double ToleranceDegrees = 0.05)
{
	FQuat ActualQuat = Actual;
	FQuat ExpectedQuat = Expected;
	ActualQuat.Normalize();
	ExpectedQuat.Normalize();
	if ((ActualQuat | ExpectedQuat) < 0.0)
	{
		ExpectedQuat = FQuat(-ExpectedQuat.X, -ExpectedQuat.Y, -ExpectedQuat.Z, -ExpectedQuat.W);
	}
	return FMath::RadiansToDegrees(ActualQuat.AngularDistance(ExpectedQuat)) <= ToleranceDegrees;
}

inline bool VerifyMathBindingsQuat(
	FAutomationTestBase& Test,
	const TCHAR* What,
	const FQuat& Actual,
	const FQuat& Expected,
	double ToleranceDegrees = 0.05)
{
	return Test.TestTrue(What, MathBindingsQuatMatches(Actual, Expected, ToleranceDegrees));
}

inline bool VerifyMathBindingsVector(
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

inline bool VerifyMathBindingsVector3f(
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
inline bool VerifyMathBindingsNumeric(
	FAutomationTestBase& Test,
	const TCHAR* What,
	TValue Actual,
	TValue Expected,
	double Tolerance)
{
	return Test.TestTrue(What, FMath::Abs(Actual - Expected) <= static_cast<TValue>(Tolerance));
}

inline bool VerifyMathBindingsTransform(
	FAutomationTestBase& Test,
	const TCHAR* What,
	const FTransform& Actual,
	const FTransform& Expected,
	double Tolerance = 0.01)
{
	const bool bRotationMatches = MathBindingsRotatorMatches(Actual.Rotator(), Expected.Rotator(), Tolerance);
	const bool bTranslationMatches = Actual.GetLocation().Equals(Expected.GetLocation(), Tolerance);
	const bool bScaleMatches = Actual.GetScale3D().Equals(Expected.GetScale3D(), Tolerance);
	if (!(bRotationMatches && bTranslationMatches && bScaleMatches))
	{
		Test.AddInfo(FString::Printf(
			TEXT("%s actual rotation=%s expected rotation=%s actual translation=%s expected translation=%s actual scale=%s expected scale=%s"),
			What,
			*Actual.Rotator().ToCompactString(),
			*Expected.Rotator().ToCompactString(),
			*Actual.GetLocation().ToCompactString(),
			*Expected.GetLocation().ToCompactString(),
			*Actual.GetScale3D().ToCompactString(),
			*Expected.GetScale3D().ToCompactString()));
	}
	return Test.TestTrue(What, bRotationMatches && bTranslationMatches && bScaleMatches);
}

inline FRotator MakeMathBindingsShortestPathLerpReference(const FRotator& A, const FRotator& B, double Alpha)
{
	FQuat Result = FQuat::Slerp(FQuat(A), FQuat(B), Alpha);
	Result.Normalize();
	return Result.Rotator();
}

inline FRotator MakeMathBindingsShortestPathInterpReference(const FRotator& Current, const FRotator& Target, float DeltaTime, float InterpSpeed)
{
	FQuat Result = FMath::QInterpTo(FQuat(Current), FQuat(Target), DeltaTime, InterpSpeed);
	Result.Normalize();
	return Result.Rotator();
}

inline FRotator MakeMathBindingsShortestPathConstantInterpReference(
	const FRotator& Current,
	const FRotator& Target,
	float DeltaTime,
	float InterpSpeedDegrees)
{
	FQuat Result = FMath::QInterpConstantTo(
		FQuat(Current),
		FQuat(Target),
		DeltaTime,
		FMath::DegreesToRadians(InterpSpeedDegrees));
	Result.Normalize();
	return Result.Rotator();
}

inline FTransform MakeMathBindingsTransformInterpReference(
	const FTransform& Current,
	const FTransform& Target,
	float DeltaTime,
	float InterpSpeed)
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

#endif // WITH_ANGELSCRIPT_UNITTESTS
