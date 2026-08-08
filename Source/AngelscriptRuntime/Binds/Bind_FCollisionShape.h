#pragma once

#include "CoreMinimal.h"
#include "CollisionShape.h"
#include "Helper_CppType.h"

struct FCollisionShapeType : TAngelscriptCppType<FCollisionShape>
{
	FString GetAngelscriptTypeName() const override;
	bool GetCppForm(const FAngelscriptTypeUsage& Usage, FCppForm& OutCppForm) const override;
};

struct FAngelscriptFCollisionShapeBinds
{
	static void ConstructDefault(FCollisionShape* Address);
	static void SetBox(FCollisionShape* Shape, const FVector& HalfExtent);
};
