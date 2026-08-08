#include "Bind_FSphere3f_Functions.h"

void FAngelscriptFSphere3fBinds::ConstructDefault(FSphere3f* Address)
{
	new (Address) FSphere3f(ForceInit);
}

void FAngelscriptFSphere3fBinds::ConstructCenterRadius(FSphere3f* Address, const FVector3f Center, const float Radius)
{
	new (Address) FSphere3f(Center, Radius);
}

void FAngelscriptFSphere3fBinds::ConstructCopy(FSphere3f* Address, const FSphere3f& Sphere)
{
	new (Address) FSphere3f(Sphere);
}

void FAngelscriptFSphere3fBinds::ConstructFromSphere(FSphere3f* Address, const FSphere& Sphere)
{
	new (Address) FSphere3f(Sphere);
}

void FAngelscriptFSphere3fBinds::ConstructFromPoints(FSphere3f* Address, TArray<FVector3f>& Points)
{
	new (Address) FSphere3f(Points.GetData(), Points.Num());
}
