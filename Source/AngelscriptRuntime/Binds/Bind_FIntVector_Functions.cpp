#include "Bind_FIntVector.h"

void FAngelscriptFIntVectorBinds::ConstructXYZ(FIntVector* Address, const int32 X, const int32 Y, const int32 Z)
{
	new (Address) FIntVector(X, Y, Z);
}

void FAngelscriptFIntVectorBinds::ConstructZero(FIntVector* Address)
{
	new (Address) FIntVector(0);
}

void FAngelscriptFIntVectorBinds::ConstructScalar(FIntVector* Address, const int32 Scalar)
{
	new (Address) FIntVector(Scalar);
}

void FAngelscriptFIntVectorBinds::ConstructCopy(FIntVector* Address, const FIntVector& Other)
{
	new (Address) FIntVector(Other);
}

FIntVector FAngelscriptFIntVectorBinds::Negate(const FIntVector* Vector)
{
	return FIntVector(-Vector->X, -Vector->Y, -Vector->Z);
}

void FAngelscriptFIntVectorBinds::AppendToString(void* Ptr, FString& Str)
{
	Str += static_cast<FIntVector*>(Ptr)->ToString();
}
