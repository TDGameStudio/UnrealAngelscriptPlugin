#include "Bind_USceneComponent_Functions.h"

#include "AngelscriptEngine.h"
#include "AngelscriptType.h"
#include "Components/SceneComponent.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_objecttype.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

USceneComponent* FAngelscriptUSceneComponentBinds::GetChildComponentByClass(
	USceneComponent* ParentComponent,
	const TSubclassOf<USceneComponent> ComponentClass)
{
	if (ComponentClass == nullptr)
	{
		return nullptr;
	}

	const int32 ChildCount = ParentComponent->GetNumChildrenComponents();
	for (int32 Index = 0; Index < ChildCount; ++Index)
	{
		USceneComponent* Child = ParentComponent->GetChildComponent(Index);
		if (Child != nullptr && Child->IsA(ComponentClass))
		{
			return Child;
		}
	}

	return nullptr;
}

void FAngelscriptUSceneComponentBinds::GetChildrenComponentsByClass(
	USceneComponent* ParentComponent,
	UClass* ComponentClass,
	const bool bIncludeAllDescendants,
	TArray<USceneComponent*>& OutComponents,
	const int TypeId)
{
	FAngelscriptEngine& Manager = FAngelscriptEngine::Get();
	asCTypeInfo* ScriptType = static_cast<asCTypeInfo*>(Manager.Engine->GetTypeInfoById(TypeId));
	if (ScriptType == nullptr || (ScriptType->flags & asOBJ_VALUE) == 0)
	{
		FAngelscriptEngine::Throw("GetChildrenComponentsByClass must take a TArray of scene components as its out argument.");
		return;
	}

	asCObjectType* ObjectType = static_cast<asCObjectType*>(ScriptType);
	if (ObjectType->templateBaseType != FAngelscriptType::GetArrayTemplateTypeInfo())
	{
		FAngelscriptEngine::Throw("GetChildrenComponentsByClass must take a TArray of scene components as its out argument.");
		return;
	}

	asCTypeInfo* SubTypeInfo = ObjectType->templateSubTypes[0].GetTypeInfo();
	if (SubTypeInfo == nullptr
		|| (SubTypeInfo->GetFlags() & asOBJ_REF) == 0
		|| SubTypeInfo->plainUserData == 0)
	{
		FAngelscriptEngine::Throw("GetChildrenComponentsByClass must take a TArray of scene components as its out argument.");
		return;
	}

	UClass* SubClass = reinterpret_cast<UClass*>(SubTypeInfo->plainUserData);
	if (!SubClass->IsChildOf<USceneComponent>())
	{
		FAngelscriptEngine::Throw("GetChildrenComponentsByClass must take a TArray of scene components as its out argument.");
		return;
	}

	if (ParentComponent == nullptr)
	{
		FAngelscriptEngine::Throw("Scene component was null.");
		return;
	}

	if (ComponentClass == nullptr)
	{
		FAngelscriptEngine::Throw("Component class was null.");
		return;
	}

	if (!ComponentClass->IsChildOf(SubClass))
	{
		FAngelscriptEngine::Throw("Class specified to GetChildrenComponentsByClass is not a child of array element class.");
		return;
	}

	TArray<USceneComponent*> Children;
	ParentComponent->GetChildrenComponents(bIncludeAllDescendants, Children);
	for (USceneComponent* Component : Children)
	{
		if (Component != nullptr && Component->IsA(ComponentClass))
		{
			OutComponents.Add(Component);
		}
	}
}

FTransform FAngelscriptUSceneComponentBinds::GetComponentTransform(USceneComponent* Component)
{
	if (Component == nullptr)
	{
		return FTransform::Identity;
	}

	if (!Component->IsRegistered())
	{
		const FTransform RelativeTransform = Component->GetRelativeTransform();
		if (USceneComponent* AttachParent = Component->GetAttachParent())
		{
			return RelativeTransform * AttachParent->GetComponentTransform();
		}
		return RelativeTransform;
	}

	return Component->GetComponentTransform();
}

void FAngelscriptUSceneComponentBinds::SetRelativeLocation(
	USceneComponent* Component,
	const FVector NewLocation)
{
	Component->SetRelativeLocation(NewLocation);
	Component->UpdateComponentToWorld();
}

void FAngelscriptUSceneComponentBinds::SetComponentVelocity(
	USceneComponent* Component,
	const FVector& Velocity)
{
	Component->ComponentVelocity = Velocity;
}

void FAngelscriptUSceneComponentBinds::ConstructScopedMovementUpdate(
	FScriptScopedMovementUpdate* Address,
	USceneComponent* Component)
{
	new (Address) FScriptScopedMovementUpdate(Component);
}

void FAngelscriptUSceneComponentBinds::DestructScopedMovementUpdate(FScriptScopedMovementUpdate& Scope)
{
	Scope.~FScriptScopedMovementUpdate();
}
