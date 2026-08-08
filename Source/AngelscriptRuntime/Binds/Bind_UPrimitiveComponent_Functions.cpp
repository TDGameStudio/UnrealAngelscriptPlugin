#include "Bind_UPrimitiveComponent.h"

FVector FAngelscriptUPrimitiveComponentBinds::GetBoundingBoxExtents(const UPrimitiveComponent* Component)
{
	return Component->GetCollisionShape().GetExtent();
}

FVector FAngelscriptUPrimitiveComponentBinds::GetBoundsOrigin(const UPrimitiveComponent* Component)
{
	return Component->Bounds.Origin;
}

FVector FAngelscriptUPrimitiveComponentBinds::GetBoundsExtent(const UPrimitiveComponent* Component)
{
	return Component->Bounds.BoxExtent;
}

double FAngelscriptUPrimitiveComponentBinds::GetBoundsRadius(const UPrimitiveComponent* Component)
{
	return Component->Bounds.SphereRadius;
}

bool FAngelscriptUPrimitiveComponentBinds::GetSelectable(const UPrimitiveComponent* Component)
{
	return Component->bSelectable;
}

void FAngelscriptUPrimitiveComponentBinds::SetSelectable(UPrimitiveComponent* Component, bool bSelectable)
{
	Component->bSelectable = bSelectable;
}

void FAngelscriptUPrimitiveComponentBinds::SetLightmapType(UPrimitiveComponent* Component, ELightmapType Type)
{
	Component->SetLightmapType(Type);
}
