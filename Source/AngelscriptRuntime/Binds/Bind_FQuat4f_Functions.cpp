#include "Bind_FQuat4f_Functions.h"

void FAngelscriptFQuat4fBinds::ConstructDefault(FQuat4f* Address)
{
	new (Address) FQuat4f(0.f, 0.f, 0.f, 1.f);
}

void FAngelscriptFQuat4fBinds::ConstructCopy(FQuat4f* Address, const FQuat4f& Quat)
{
	new (Address) FQuat4f(Quat);
}

void FAngelscriptFQuat4fBinds::ConstructComponents(
	FQuat4f* Address,
	const float X,
	const float Y,
	const float Z,
	const float W)
{
	new (Address) FQuat4f(X, Y, Z, W);
}

void FAngelscriptFQuat4fBinds::ConstructFromRotator3f(FQuat4f* Address, const FRotator3f& Rotator)
{
	new (Address) FQuat4f(Rotator);
}

void FAngelscriptFQuat4fBinds::ConstructAxisAngle(
	FQuat4f* Address,
	const FVector3f Axis,
	const float AngleRadians)
{
	new (Address) FQuat4f(Axis, AngleRadians);
}

void FAngelscriptFQuat4fBinds::ConstructFromQuat(FQuat4f* Address, const FQuat& Quat)
{
	new (Address) FQuat4f(Quat);
}

void FAngelscriptFQuat4fBinds::AppendToString(void* Address, FString& OutString)
{
	OutString += static_cast<FQuat4f*>(Address)->ToString();
}
