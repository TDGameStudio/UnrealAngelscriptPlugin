#pragma once

#include "CoreMinimal.h"
#include "Engine/ScopedMovementUpdate.h"

class UClass;
class USceneComponent;

struct FAngelscriptUSceneComponentBinds
{
	struct FScriptScopedMovementUpdate : FScopedMovementUpdate
	{
		void* operator new(std::size_t Count, void* Pointer)
		{
			return Pointer;
		}

		void operator delete(void*)
		{
		}

		explicit FScriptScopedMovementUpdate(USceneComponent* Component)
			: FScopedMovementUpdate(Component)
		{
		}

		~FScriptScopedMovementUpdate()
		{
		}
	};

	static USceneComponent* GetChildComponentByClass(
		USceneComponent* ParentComponent,
		TSubclassOf<USceneComponent> ComponentClass);
	static void GetChildrenComponentsByClass(
		USceneComponent* ParentComponent,
		UClass* ComponentClass,
		bool bIncludeAllDescendants,
		TArray<USceneComponent*>& OutComponents,
		int TypeId);
	static FTransform GetComponentTransform(USceneComponent* Component);
	static void SetRelativeLocation(USceneComponent* Component, FVector NewLocation);
	static void SetComponentVelocity(USceneComponent* Component, const FVector& Velocity);

	static void ConstructScopedMovementUpdate(
		FScriptScopedMovementUpdate* Address,
		USceneComponent* Component);
	static void DestructScopedMovementUpdate(FScriptScopedMovementUpdate& Scope);
};
