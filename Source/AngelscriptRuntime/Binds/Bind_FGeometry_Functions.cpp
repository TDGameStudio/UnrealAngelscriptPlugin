#include "Bind_FGeometry_Functions.h"

#include "Layout/Geometry.h"

FVector2D FAngelscriptFGeometryBinds::GetLocalSize(FGeometry* Geometry)
{
	return FVector2D(Geometry->GetLocalSize());
}

FVector2D FAngelscriptFGeometryBinds::GetAbsoluteSize(FGeometry* Geometry)
{
	return FVector2D(Geometry->GetAbsoluteSize());
}

FVector2D FAngelscriptFGeometryBinds::AbsoluteToLocal(FGeometry* Geometry, const FVector2D& Position)
{
	return FVector2D(Geometry->AbsoluteToLocal(FVector2f(Position)));
}

FVector2D FAngelscriptFGeometryBinds::LocalToAbsolute(FGeometry* Geometry, const FVector2D& Position)
{
	return FVector2D(Geometry->LocalToAbsolute(FVector2f(Position)));
}

FGeometry FAngelscriptFGeometryBinds::MakeChild(FGeometry* Geometry, const FVector2D& Position, const FVector2D& Size)
{
	return Geometry->MakeChild(FVector2f(Size), FSlateLayoutTransform(FVector2f(Position)));
}
