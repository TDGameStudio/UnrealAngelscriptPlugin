#include "Bind_UActorComponent_Functions.h"

#include "GameFramework/Actor.h"

void FAngelscriptUActorComponentBinds::MarkRenderStateDirty(UActorComponent* Component)
{
	Component->MarkRenderStateDirty();
}

AActor* FAngelscriptUActorComponentBinds::GetOwner(UActorComponent* Component)
{
	return Component->GetOwner();
}

void FAngelscriptUActorComponentBinds::Activate(UActorComponent* Component, bool bReset)
{
	Component->Activate(bReset);
}

void FAngelscriptUActorComponentBinds::Deactivate(UActorComponent* Component)
{
	Component->Deactivate();
}

void FAngelscriptUActorComponentBinds::DestroyComponent(UActorComponent* Component, bool bPromoteChildren)
{
	Component->DestroyComponent(bPromoteChildren);
}

bool FAngelscriptUActorComponentBinds::ComponentHasTag(const UActorComponent* Component, FName Tag)
{
	if (Tag.IsNone() || Tag == FName(TEXT("None")))
		return false;
	return Component->ComponentHasTag(Tag);
}

void FAngelscriptUActorComponentBinds::SetTickInEditor(UActorComponent* Component, bool bTickInEditor)
{
	Component->bTickInEditor = bTickInEditor;
}

void FAngelscriptUActorComponentBinds::SetIsEditorOnly(UActorComponent* Component, bool bEditorOnly)
{
	Component->bIsEditorOnly = bEditorOnly;
}

EComponentCreationMethod FAngelscriptUActorComponentBinds::GetComponentCreationMethod(
	const UActorComponent* Component)
{
	return Component->CreationMethod;
}

void FAngelscriptUActorComponentBinds::SetIsVisualizationComponent(
	UActorComponent* Component,
	bool bVisualization)
{
#if WITH_EDITOR
	Component->SetIsVisualizationComponent(bVisualization);
#endif
}

bool FAngelscriptUActorComponentBinds::IsVisualizationComponent(UActorComponent* Component)
{
#if WITH_EDITOR
	return Component->IsVisualizationComponent();
#else
	return false;
#endif
}
