#pragma once

#include "CoreMinimal.h"
#include "CollisionShape.h"

struct FAngelscriptFCollisionShapeBinds
{
	static void ConstructDefault(FCollisionShape* Address);
	static void SetBox(FCollisionShape* Shape, const FVector& HalfExtent);
};
