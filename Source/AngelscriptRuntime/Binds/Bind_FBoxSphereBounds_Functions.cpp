#include "Bind_FBoxSphereBounds.h"

void FAngelscriptFBoxSphereBoundsBinds::ConstructDefault(FBoxSphereBounds* Address)
{
	new (Address) FBoxSphereBounds(ForceInit);
}

void FAngelscriptFBoxSphereBoundsBinds::ConstructOriginExtentRadius(
	FBoxSphereBounds* Address,
	const FVector& Origin,
	const FVector& BoxExtent,
	const double SphereRadius)
{
	new (Address) FBoxSphereBounds(Origin, BoxExtent, SphereRadius);
}

void FAngelscriptFBoxSphereBoundsBinds::ConstructBoxSphere(FBoxSphereBounds* Address, const FBox& Box, const FSphere& Sphere)
{
	new (Address) FBoxSphereBounds(Box, Sphere);
}

void FAngelscriptFBoxSphereBoundsBinds::ConstructFromBounds3f(FBoxSphereBounds* Address, const FBoxSphereBounds3f& Bounds)
{
	new (Address) FBoxSphereBounds(Bounds);
}

void FAngelscriptFBoxSphereBoundsBinds::ConstructFromBox(FBoxSphereBounds* Address, const FBox& Box)
{
	new (Address) FBoxSphereBounds(Box);
}

void FAngelscriptFBoxSphereBoundsBinds::ConstructFromSphere(FBoxSphereBounds* Address, const FSphere& Sphere)
{
	new (Address) FBoxSphereBounds(Sphere);
}

void FAngelscriptFBoxSphereBoundsBinds::ConstructFromPoints(FBoxSphereBounds* Address, TArray<FVector>& Points)
{
	new (Address) FBoxSphereBounds(Points.GetData(), Points.Num());
}

void FAngelscriptFBoxSphereBoundsBinds::AppendToString(void* Ptr, FString& Str)
{
	Str += static_cast<FBoxSphereBounds*>(Ptr)->ToString();
}
