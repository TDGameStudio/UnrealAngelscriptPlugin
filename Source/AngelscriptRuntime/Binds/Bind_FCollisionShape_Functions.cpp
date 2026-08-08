#include "Bind_FCollisionShape.h"

void FAngelscriptFCollisionShapeBinds::ConstructDefault(FCollisionShape* Address)
{
	new (Address) FCollisionShape();
}

void FAngelscriptFCollisionShapeBinds::SetBox(FCollisionShape* Shape, const FVector& HalfExtent)
{
	Shape->SetBox(FVector3f(HalfExtent));
}
