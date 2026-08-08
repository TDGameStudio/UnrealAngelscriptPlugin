#include "Bind_FIntVector4.h"

void FAngelscriptFIntVector4Binds::ConstructXYZW(FIntVector4* Address, const int32 X, const int32 Y, const int32 Z, const int32 W)
{
	new (Address) FIntVector4(X, Y, Z, W);
}

void FAngelscriptFIntVector4Binds::ConstructZero(FIntVector4* Address)
{
	new (Address) FIntVector4(0);
}

void FAngelscriptFIntVector4Binds::ConstructScalar(FIntVector4* Address, const int32 Scalar)
{
	new (Address) FIntVector4(Scalar);
}

void FAngelscriptFIntVector4Binds::ConstructCopy(FIntVector4* Address, const FIntVector4& Other)
{
	new (Address) FIntVector4(Other);
}

FIntVector4 FAngelscriptFIntVector4Binds::Negate(const FIntVector4* Vector)
{
	return FIntVector4(-Vector->X, -Vector->Y, -Vector->Z, -Vector->W);
}

void FAngelscriptFIntVector4Binds::AppendToString(void* Ptr, FString& Str)
{
	const FIntVector4* Vector = static_cast<FIntVector4*>(Ptr);
	Str += FString::Printf(
		TEXT("X=%s Y=%s Z=%s W=%s"),
		*LexToString(Vector->X),
		*LexToString(Vector->Y),
		*LexToString(Vector->Z),
		*LexToString(Vector->W));
}
