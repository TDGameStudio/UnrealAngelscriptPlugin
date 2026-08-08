#pragma once

#include "CoreMinimal.h"

struct FGeometry;

struct FAngelscriptFGeometryBinds
{
	static FVector2D GetLocalSize(FGeometry* Geometry);
	static FVector2D GetAbsoluteSize(FGeometry* Geometry);
	static FVector2D AbsoluteToLocal(FGeometry* Geometry, const FVector2D& Position);
	static FVector2D LocalToAbsolute(FGeometry* Geometry, const FVector2D& Position);
	static FGeometry MakeChild(FGeometry* Geometry, const FVector2D& Position, const FVector2D& Size);
};
