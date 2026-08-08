#include "Bind_FSphere_Functions.h"

void FAngelscriptFSphereBinds::ConstructDefault(FSphere* Address)
{
	new (Address) FSphere(ForceInit);
}

void FAngelscriptFSphereBinds::ConstructCenterRadius(FSphere* Address, const FVector Center, const float Radius)
{
	new (Address) FSphere(Center, Radius);
}

void FAngelscriptFSphereBinds::ConstructCopy(FSphere* Address, const FSphere& Sphere)
{
	new (Address) FSphere(Sphere);
}

void FAngelscriptFSphereBinds::ConstructFromSphere3f(FSphere* Address, const FSphere3f& Sphere)
{
	new (Address) FSphere(Sphere);
}

void FAngelscriptFSphereBinds::ConstructFromPoints(FSphere* Address, TArray<FVector>& Points)
{
	new (Address) FSphere(Points.GetData(), Points.Num());
}
