#include "Bind_FRotator_Functions.h"

namespace
{
	FRotator FixupRotation(FRotator Original)
	{
		if (FMath::Abs(Original.Pitch - SMALL_NUMBER) < KINDA_SMALL_NUMBER)
		{
			Original.Pitch = 0.f;
		}
		if (FMath::Abs(Original.Yaw - SMALL_NUMBER) < KINDA_SMALL_NUMBER)
		{
			Original.Yaw = 0.f;
		}
		if (FMath::Abs(Original.Roll - SMALL_NUMBER) < KINDA_SMALL_NUMBER)
		{
			Original.Roll = 0.f;
		}
		return Original;
	}
}

void FAngelscriptFRotatorBinds::ConstructComponents(
	FRotator* Address,
	const double Pitch,
	const double Yaw,
	const double Roll)
{
	new (Address) FRotator(Pitch, Yaw, Roll);
}

void FAngelscriptFRotatorBinds::ConstructDefault(FRotator* Address)
{
	new (Address) FRotator(0.f);
}

void FAngelscriptFRotatorBinds::ConstructScalar(FRotator* Address, const double Value)
{
	new (Address) FRotator(Value);
}

void FAngelscriptFRotatorBinds::ConstructCopy(FRotator* Address, const FRotator& Other)
{
	new (Address) FRotator(Other);
}

FVector FAngelscriptFRotatorBinds::GetRightVector(const FRotator& Rotator)
{
	return FRotationMatrix(Rotator).GetScaledAxis(EAxis::Y);
}

FVector FAngelscriptFRotatorBinds::GetUpVector(const FRotator& Rotator)
{
	return FRotationMatrix(Rotator).GetScaledAxis(EAxis::Z);
}

void FAngelscriptFRotatorBinds::ConstructFromQuat(FRotator* Address, const FQuat& Quat)
{
	new (Address) FRotator(Quat);
}

void FAngelscriptFRotatorBinds::ConstructFromRotator3f(FRotator* Address, const FRotator3f& Rotator)
{
	new (Address) FRotator(Rotator);
}

FString FAngelscriptFRotatorBinds::ToColorString(const FRotator& Rotator)
{
	const FRotator FixedRotation = FixupRotation(Rotator);
	const FString PitchString = FString::Printf(TEXT("<Green>P=%3.3f </>"), FixedRotation.Pitch);
	const FString YawString = FString::Printf(TEXT("<Blue>Y=%3.3f </>"), FixedRotation.Yaw);
	const FString RollString = FString::Printf(TEXT("<Red>R=%3.3f </>"), FixedRotation.Roll);
	return YawString + PitchString + RollString;
}

void FAngelscriptFRotatorBinds::AppendToString(void* Address, FString& OutString)
{
	OutString += static_cast<FRotator*>(Address)->ToString();
}
