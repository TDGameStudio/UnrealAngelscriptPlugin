#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

class AActor;

struct FAngelscriptUActorComponentBinds
{
	static void MarkRenderStateDirty(UActorComponent* Component);
	static AActor* GetOwner(UActorComponent* Component);
	static void Activate(UActorComponent* Component, bool bReset);
	static void Deactivate(UActorComponent* Component);
	static void DestroyComponent(UActorComponent* Component, bool bPromoteChildren);
	static bool ComponentHasTag(const UActorComponent* Component, FName Tag);
	static void SetTickInEditor(UActorComponent* Component, bool bTickInEditor);
	static void SetIsEditorOnly(UActorComponent* Component, bool bEditorOnly);
	static EComponentCreationMethod GetComponentCreationMethod(const UActorComponent* Component);
	static void SetIsVisualizationComponent(UActorComponent* Component, bool bVisualization);
	static bool IsVisualizationComponent(UActorComponent* Component);
};
