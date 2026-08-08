#include "Bind_FMath.h"

#include "AngelscriptEngine.h"
#include "Kismet/KismetMathLibrary.h"

float FAngelscriptFMathBinds::FloatSpringInterp(
	float Current,
	float Target,
	FFloatSpringState& SpringState,
	float Stiffness,
	float CriticalDampingFactor,
	float DeltaTime,
	float Mass,
	float TargetVelocityAmount)
{
	return UKismetMathLibrary::FloatSpringInterp(
		Current,
		Target,
		SpringState,
		Stiffness,
		CriticalDampingFactor,
		DeltaTime,
		Mass,
		TargetVelocityAmount);
}

FVector FAngelscriptFMathBinds::VectorSpringInterp(
	FVector Current,
	FVector Target,
	FVectorSpringState& SpringState,
	float Stiffness,
	float CriticalDampingFactor,
	float DeltaTime,
	float Mass,
	float TargetVelocityAmount)
{
	return UKismetMathLibrary::VectorSpringInterp(
		Current,
		Target,
		SpringState,
		Stiffness,
		CriticalDampingFactor,
		DeltaTime,
		Mass,
		TargetVelocityAmount);
}

FQuat FAngelscriptFMathBinds::QuaternionSpringInterp(
	FQuat Current,
	FQuat Target,
	FQuaternionSpringState& SpringState,
	float Stiffness,
	float CriticalDampingFactor,
	float DeltaTime,
	float Mass,
	float TargetVelocityAmount)
{
	return UKismetMathLibrary::QuaternionSpringInterp(
		Current,
		Target,
		SpringState,
		Stiffness,
		CriticalDampingFactor,
		DeltaTime,
		Mass,
		TargetVelocityAmount);
}

bool FAngelscriptFMathBinds::IsPointInBox(const FVector& Point, const FVector& BoxOrigin, const FVector& BoxExtent)
{
	return UKismetMathLibrary::IsPointInBox(Point, BoxOrigin, BoxExtent);
}

bool FAngelscriptFMathBinds::IsPointInBoxWithTransform(
	const FVector& Point,
	const FTransform& BoxWorldTransform,
	const FVector& BoxExtent)
{
	return UKismetMathLibrary::IsPointInBoxWithTransform(Point, BoxWorldTransform, BoxExtent);
}

void FAngelscriptFMathBinds::FindNearestPointsOnLineSegments(
	FVector Segment1Start,
	FVector Segment1End,
	FVector Segment2Start,
	FVector Segment2End,
	FVector& Segment1Point,
	FVector& Segment2Point)
{
	UKismetMathLibrary::FindNearestPointsOnLineSegments(
		Segment1Start,
		Segment1End,
		Segment2Start,
		Segment2End,
		Segment1Point,
		Segment2Point);
}

int32 FAngelscriptFMathBinds::IntegerDivisionTruncInt32(int32 Value, int32 DivideBy)
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
}

int64 FAngelscriptFMathBinds::IntegerDivisionTruncInt64(int64 Value, int64 DivideBy)
{
	if (DivideBy == 0)
	{
		FAngelscriptEngine::Throw("Division by zero");
		return 0;
	}
	else if (Value == (int64(1) << 63))
	{
		FAngelscriptEngine::Throw("Overflow in integer division");
		return 0;
	}

	return Value / DivideBy;
}

uint32 FAngelscriptFMathBinds::IntegerDivisionTruncUInt32(uint32 Value, uint32 DivideBy)
{
	if (DivideBy == 0)
	{
		FAngelscriptEngine::Throw("Division by zero");
		return 0;
	}

	return Value / DivideBy;
}

uint64 FAngelscriptFMathBinds::IntegerDivisionTruncUInt64(uint64 Value, uint64 DivideBy)
{
	if (DivideBy == 0)
	{
		FAngelscriptEngine::Throw("Division by zero");
		return 0;
	}

	return Value / DivideBy;
}
