#include "Bind_FVector_Functions.h"

void FAngelscriptFVectorBinds::ConstructXYZ(FVector* Address, double X, double Y, double Z)
{
	new (Address) FVector(X, Y, Z);
}

void FAngelscriptFVectorBinds::ConstructZero(FVector* Address)
{
	new (Address) FVector(0.f);
}

void FAngelscriptFVectorBinds::ConstructScalar(FVector* Address, double Scalar)
{
	new (Address) FVector(Scalar);
}

void FAngelscriptFVectorBinds::ConstructCopy(FVector* Address, const FVector& Other)
{
	new (Address) FVector(Other);
}

void FAngelscriptFVectorBinds::ConstructFromVector3f(FVector* Address, const FVector3f& Other)
{
	new (Address) FVector(Other);
}

void FAngelscriptFVectorBinds::AppendToString(void* Address, FString& OutString)
{
	OutString += static_cast<FVector*>(Address)->ToString();
}
