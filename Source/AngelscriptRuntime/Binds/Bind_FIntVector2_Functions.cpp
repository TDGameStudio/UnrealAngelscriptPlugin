#include "Bind_FIntVector2.h"

void FAngelscriptFIntVector2Binds::ConstructXY(FIntVector2* Address, const int32 X, const int32 Y)
{
	new (Address) FIntVector2(X, Y);
}

void FAngelscriptFIntVector2Binds::ConstructZero(FIntVector2* Address)
{
	new (Address) FIntVector2(0);
}

void FAngelscriptFIntVector2Binds::ConstructScalar(FIntVector2* Address, const int32 Scalar)
{
	new (Address) FIntVector2(Scalar);
}

void FAngelscriptFIntVector2Binds::ConstructCopy(FIntVector2* Address, const FIntVector2& Other)
{
	new (Address) FIntVector2(Other);
}

void FAngelscriptFIntVector2Binds::AppendToString(void* Ptr, FString& Str)
{
	const FIntVector2* Vector = static_cast<FIntVector2*>(Ptr);
	Str += FString::Printf(TEXT("X=%s Y=%s"), *LexToString(Vector->X), *LexToString(Vector->Y));
}
