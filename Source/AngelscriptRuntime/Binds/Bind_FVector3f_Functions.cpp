#include "Bind_FVector3f.h"

void FAngelscriptFVector3fBinds::ConstructXYZ(FVector3f* Address, float X, float Y, float Z)
{
	new (Address) FVector3f(X, Y, Z);
}

void FAngelscriptFVector3fBinds::ConstructZero(FVector3f* Address)
{
	new (Address) FVector3f(0.f);
}

void FAngelscriptFVector3fBinds::ConstructScalar(FVector3f* Address, float Scalar)
{
	new (Address) FVector3f(Scalar);
}

void FAngelscriptFVector3fBinds::ConstructCopy(FVector3f* Address, const FVector3f& Other)
{
	new (Address) FVector3f(Other);
}

void FAngelscriptFVector3fBinds::ConstructFromVector(FVector3f* Address, const FVector& Other)
{
	new (Address) FVector3f(Other);
}

void FAngelscriptFVector3fBinds::AppendToString(void* Address, FString& OutString)
{
	OutString += static_cast<FVector3f*>(Address)->ToString();
}
