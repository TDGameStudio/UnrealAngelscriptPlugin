#include "Bind_FBox3f.h"

void FAngelscriptFBox3fBinds::ConstructDefault(FBox3f* Address)
{
	new (Address) FBox3f(ForceInit);
}

void FAngelscriptFBox3fBinds::ConstructMinMax(FBox3f* Address, const FVector3f& Min, const FVector3f& Max)
{
	new (Address) FBox3f(Min, Max);
}

void FAngelscriptFBox3fBinds::ConstructFromBox(FBox3f* Address, const FBox& Box)
{
	new (Address) FBox3f(Box);
}

void FAngelscriptFBox3fBinds::AppendToString(void* Ptr, FString& Str)
{
	Str += static_cast<FBox3f*>(Ptr)->ToString();
}
