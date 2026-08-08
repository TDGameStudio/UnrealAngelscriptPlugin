#include "Bind_FBoxSphereBounds3f.h"

void FAngelscriptFBoxSphereBounds3fBinds::ConstructDefault(FBoxSphereBounds3f* Address)
{
	new (Address) FBoxSphereBounds3f(ForceInit);
}

void FAngelscriptFBoxSphereBounds3fBinds::ConstructOriginExtentRadius(
	FBoxSphereBounds3f* Address,
	const FVector3f& Origin,
	const FVector3f& BoxExtent,
	const float SphereRadius)
{
	new (Address) FBoxSphereBounds3f(Origin, BoxExtent, SphereRadius);
}

void FAngelscriptFBoxSphereBounds3fBinds::ConstructFromBounds(FBoxSphereBounds3f* Address, const FBoxSphereBounds& Bounds)
{
	new (Address) FBoxSphereBounds3f(Bounds);
}

void FAngelscriptFBoxSphereBounds3fBinds::ConstructBoxSphere(FBoxSphereBounds3f* Address, const FBox3f& Box, const FSphere3f& Sphere)
{
	new (Address) FBoxSphereBounds3f(Box, Sphere);
}

void FAngelscriptFBoxSphereBounds3fBinds::ConstructFromBox(FBoxSphereBounds3f* Address, const FBox3f& Box)
{
	new (Address) FBoxSphereBounds3f(Box);
}

void FAngelscriptFBoxSphereBounds3fBinds::ConstructFromSphere(FBoxSphereBounds3f* Address, const FSphere3f& Sphere)
{
	new (Address) FBoxSphereBounds3f(Sphere);
}

void FAngelscriptFBoxSphereBounds3fBinds::ConstructFromPoints(FBoxSphereBounds3f* Address, TArray<FVector3f>& Points)
{
	new (Address) FBoxSphereBounds3f(Points.GetData(), Points.Num());
}

void FAngelscriptFBoxSphereBounds3fBinds::AppendToString(void* Ptr, FString& Str)
{
	Str += static_cast<FBoxSphereBounds3f*>(Ptr)->ToString();
}
