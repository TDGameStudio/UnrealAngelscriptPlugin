#include "Bind_FQuat.h"

void FAngelscriptFQuatBinds::ConstructDefault(FQuat* Address)
{
	new (Address) FQuat(0.f, 0.f, 0.f, 1.f);
}

void FAngelscriptFQuatBinds::ConstructCopy(FQuat* Address, const FQuat& Quat)
{
	new (Address) FQuat(Quat);
}

void FAngelscriptFQuatBinds::ConstructComponents(
	FQuat* Address,
	const double X,
	const double Y,
	const double Z,
	const double W)
{
	new (Address) FQuat(X, Y, Z, W);
}

void FAngelscriptFQuatBinds::ConstructFromRotator(FQuat* Address, const FRotator& Rotator)
{
	new (Address) FQuat(Rotator);
}

void FAngelscriptFQuatBinds::ConstructAxisAngle(
	FQuat* Address,
	const FVector Axis,
	const double AngleRadians)
{
	new (Address) FQuat(Axis, AngleRadians);
}

void FAngelscriptFQuatBinds::ConstructFromQuat4f(FQuat* Address, const FQuat4f& Quat)
{
	new (Address) FQuat(Quat);
}

void FAngelscriptFQuatBinds::AppendToString(void* Address, FString& OutString)
{
	OutString += static_cast<FQuat*>(Address)->ToString();
}
