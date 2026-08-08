#include "Bind_FIntPoint_Functions.h"

void FAngelscriptFIntPointBinds::ConstructXY(FIntPoint* Address, int32 X, int32 Y)
{
	new (Address) FIntPoint(X, Y);
}

void FAngelscriptFIntPointBinds::ConstructZero(FIntPoint* Address)
{
	new (Address) FIntPoint(0);
}

void FAngelscriptFIntPointBinds::ConstructScalar(FIntPoint* Address, int32 Scalar)
{
	new (Address) FIntPoint(Scalar);
}

void FAngelscriptFIntPointBinds::ConstructCopy(FIntPoint* Address, const FIntPoint& Other)
{
	new (Address) FIntPoint(Other);
}

FIntPoint FAngelscriptFIntPointBinds::Negate(FIntPoint* Point)
{
	return FIntPoint(-Point->X, -Point->Y);
}

void FAngelscriptFIntPointBinds::AppendToString(void* Address, FString& String)
{
	String += static_cast<FIntPoint*>(Address)->ToString();
}
