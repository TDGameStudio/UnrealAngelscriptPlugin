#include "Bind_FBox.h"

void FAngelscriptFBoxBinds::ConstructDefault(FBox* Address)
{
	new (Address) FBox(ForceInit);
}

void FAngelscriptFBoxBinds::ConstructMinMax(FBox* Address, const FVector& Min, const FVector& Max)
{
	new (Address) FBox(Min, Max);
}

void FAngelscriptFBoxBinds::ConstructFromBox3f(FBox* Address, const FBox3f& Box)
{
	new (Address) FBox(Box);
}

void FAngelscriptFBoxBinds::AppendToString(void* Ptr, FString& Str)
{
	Str += static_cast<FBox*>(Ptr)->ToString();
}
