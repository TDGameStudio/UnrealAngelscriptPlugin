#include "Bind_FVector4f_Functions.h"

void FAngelscriptFVector4fBinds::Construct(FVector4f* Address, const float X, const float Y, const float Z, const float W)
{
	new (Address) FVector4f(X, Y, Z, W);
}

void FAngelscriptFVector4fBinds::ConstructZero(FVector4f* Address)
{
	new (Address) FVector4f(0.f, 0.f, 0.f, 0.f);
}

void FAngelscriptFVector4fBinds::ConstructCopy(FVector4f* Address, const FVector4f& Other)
{
	new (Address) FVector4f(Other);
}

void FAngelscriptFVector4fBinds::ConstructFromVector3f(FVector4f* Address, const FVector3f InVector, const float InW)
{
	new (Address) FVector4f(InVector, InW);
}

void FAngelscriptFVector4fBinds::ConstructFromVector4(FVector4f* Address, const FVector4& Other)
{
	new (Address) FVector4f(Other);
}

void FAngelscriptFVector4fBinds::AppendToString(void* Ptr, FString& Str)
{
	Str += static_cast<FVector4f*>(Ptr)->ToString();
}
