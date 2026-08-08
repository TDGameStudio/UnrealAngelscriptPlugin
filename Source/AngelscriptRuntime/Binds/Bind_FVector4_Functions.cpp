#include "Bind_FVector4.h"

void FAngelscriptFVector4Binds::Construct(FVector4* Address, const double X, const double Y, const double Z, const double W)
{
	new (Address) FVector4(X, Y, Z, W);
}

void FAngelscriptFVector4Binds::ConstructZero(FVector4* Address)
{
	new (Address) FVector4(0.f, 0.f, 0.f, 0.f);
}

void FAngelscriptFVector4Binds::ConstructCopy(FVector4* Address, const FVector4& Other)
{
	new (Address) FVector4(Other);
}

void FAngelscriptFVector4Binds::ConstructFromVector(FVector4* Address, const FVector InVector, const double InW)
{
	new (Address) FVector4(InVector, InW);
}

void FAngelscriptFVector4Binds::ConstructFromVector4f(FVector4* Address, const FVector4f& Other)
{
	new (Address) FVector4(Other);
}

void FAngelscriptFVector4Binds::AppendToString(void* Ptr, FString& Str)
{
	Str += static_cast<FVector4*>(Ptr)->ToString();
}
