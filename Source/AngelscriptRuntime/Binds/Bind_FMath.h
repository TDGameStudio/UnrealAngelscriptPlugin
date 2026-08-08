#pragma once

#include "CoreMinimal.h"

struct FFloatSpringState;
struct FQuaternionSpringState;
struct FVectorSpringState;

struct FAngelscriptFMathBinds
{
	static float FloatSpringInterp(
		float Current,
		float Target,
		FFloatSpringState& SpringState,
		float Stiffness,
		float CriticalDampingFactor,
		float DeltaTime,
		float Mass,
		float TargetVelocityAmount);
	static FVector VectorSpringInterp(
		FVector Current,
		FVector Target,
		FVectorSpringState& SpringState,
		float Stiffness,
		float CriticalDampingFactor,
		float DeltaTime,
		float Mass,
		float TargetVelocityAmount);
	static FQuat QuaternionSpringInterp(
		FQuat Current,
		FQuat Target,
		FQuaternionSpringState& SpringState,
		float Stiffness,
		float CriticalDampingFactor,
		float DeltaTime,
		float Mass,
		float TargetVelocityAmount);

	template<typename TValue, typename TAlpha>
	static TValue EaseIn(const TValue& A, const TValue& B, TAlpha Alpha, TAlpha Exp)
	{
		return FMath::InterpEaseIn<TValue>(A, B, Alpha, Exp);
	}

	template<typename TValue, typename TAlpha>
	static TValue EaseOut(const TValue& A, const TValue& B, TAlpha Alpha, TAlpha Exp)
	{
		return FMath::InterpEaseOut<TValue>(A, B, Alpha, Exp);
	}

	template<typename TValue, typename TAlpha>
	static TValue EaseInOut(const TValue& A, const TValue& B, TAlpha Alpha, TAlpha Exp)
	{
		return FMath::InterpEaseInOut<TValue>(A, B, Alpha, Exp);
	}

	template<typename TValue, typename TAlpha>
	static TValue SinusoidalIn(const TValue& A, const TValue& B, TAlpha Alpha)
	{
		return FMath::InterpSinIn<TValue>(A, B, Alpha);
	}

	template<typename TValue, typename TAlpha>
	static TValue SinusoidalOut(const TValue& A, const TValue& B, TAlpha Alpha)
	{
		return FMath::InterpSinOut<TValue>(A, B, Alpha);
	}

	template<typename TValue, typename TAlpha>
	static TValue SinusoidalInOut(const TValue& A, const TValue& B, TAlpha Alpha)
	{
		return FMath::InterpSinInOut<TValue>(A, B, Alpha);
	}

	template<typename TValue, typename TAlpha>
	static TValue ExpoIn(const TValue& A, const TValue& B, TAlpha Alpha)
	{
		return FMath::InterpExpoIn<TValue>(A, B, Alpha);
	}

	template<typename TValue, typename TAlpha>
	static TValue ExpoOut(const TValue& A, const TValue& B, TAlpha Alpha)
	{
		return FMath::InterpExpoOut<TValue>(A, B, Alpha);
	}

	template<typename TValue, typename TAlpha>
	static TValue ExpoInOut(const TValue& A, const TValue& B, TAlpha Alpha)
	{
		return FMath::InterpExpoInOut<TValue>(A, B, Alpha);
	}

	template<typename TValue, typename TAlpha>
	static TValue CircularIn(const TValue& A, const TValue& B, TAlpha Alpha)
	{
		return FMath::InterpCircularIn<TValue>(A, B, Alpha);
	}

	template<typename TValue, typename TAlpha>
	static TValue CircularOut(const TValue& A, const TValue& B, TAlpha Alpha)
	{
		return FMath::InterpCircularOut<TValue>(A, B, Alpha);
	}

	template<typename TValue, typename TAlpha>
	static TValue CircularInOut(const TValue& A, const TValue& B, TAlpha Alpha)
	{
		return FMath::InterpCircularInOut<TValue>(A, B, Alpha);
	}

	static bool IsPointInBox(const FVector& Point, const FVector& BoxOrigin, const FVector& BoxExtent);
	static bool IsPointInBoxWithTransform(const FVector& Point, const FTransform& BoxWorldTransform, const FVector& BoxExtent);
	static void FindNearestPointsOnLineSegments(
		FVector Segment1Start,
		FVector Segment1End,
		FVector Segment2Start,
		FVector Segment2End,
		FVector& Segment1Point,
		FVector& Segment2Point);
	static int32 IntegerDivisionTruncInt32(int32 Value, int32 DivideBy);
	static int64 IntegerDivisionTruncInt64(int64 Value, int64 DivideBy);
	static uint32 IntegerDivisionTruncUInt32(uint32 Value, uint32 DivideBy);
	static uint64 IntegerDivisionTruncUInt64(uint64 Value, uint64 DivideBy);
};
