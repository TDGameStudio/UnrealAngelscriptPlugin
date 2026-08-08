#include "Bind_FRotator3f.h"

namespace
{
	FRotator3f FixupRotation(FRotator3f Original)
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

void FAngelscriptFRotator3fBinds::ConstructComponents(
	FRotator3f* Address,
	const float Pitch,
	const float Yaw,
	const float Roll)
{
	new (Address) FRotator3f(Pitch, Yaw, Roll);
}

void FAngelscriptFRotator3fBinds::ConstructDefault(FRotator3f* Address)
{
	new (Address) FRotator3f(0.f);
}

void FAngelscriptFRotator3fBinds::ConstructScalar(FRotator3f* Address, const float Value)
{
	new (Address) FRotator3f(Value);
}

void FAngelscriptFRotator3fBinds::ConstructCopy(FRotator3f* Address, const FRotator3f& Other)
{
	new (Address) FRotator3f(Other);
}

void FAngelscriptFRotator3fBinds::ConstructFromQuat4f(FRotator3f* Address, const FQuat4f& Quat)
{
	new (Address) FRotator3f(Quat);
}

void FAngelscriptFRotator3fBinds::ConstructFromRotator(FRotator3f* Address, const FRotator& Rotator)
{
	new (Address) FRotator3f(Rotator);
}

FString FAngelscriptFRotator3fBinds::ToColorString(const FRotator3f& Rotator)
{
	const FRotator3f FixedRotation = FixupRotation(Rotator);
	const FString PitchString = FString::Printf(TEXT("<Green>P=%3.3f </>"), FixedRotation.Pitch);
	const FString YawString = FString::Printf(TEXT("<Blue>Y=%3.3f </>"), FixedRotation.Yaw);
	const FString RollString = FString::Printf(TEXT("<Red>R=%3.3f </>"), FixedRotation.Roll);
	return YawString + PitchString + RollString;
}

void FAngelscriptFRotator3fBinds::AppendToString(void* Address, FString& OutString)
{
	OutString += static_cast<FRotator3f*>(Address)->ToString();
}
