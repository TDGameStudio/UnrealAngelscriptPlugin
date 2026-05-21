#include "CoreMinimal.h"

#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "Binds/Bind_Actor.h"
#include "AngelscriptSettings.h"

#include "UObject/UObjectIterator.h"
#include "UObject/Class.h"

#include "AngelscriptEngine.h"
#include "AngelscriptType.h"
#include "AngelscriptBinds.h"

#include "StartAngelscriptHeaders.h"
//#include "as_context.h"
//#include "as_scriptfunction.h"
//#include "as_objecttype.h"

#include "source/as_context.h"
#include "source/as_scriptfunction.h"
#include "source/as_objecttype.h"
#include "EndAngelscriptHeaders.h"

UActorComponent* FAngelscriptActorBinds::CreateComponent(AActor* InActor, const TSubclassOf<UActorComponent>& ComponentClass, const FName& WithName)
{
	if (InActor == nullptr)
	{
		FAngelscriptEngine::Throw("Actor was null.");
		return nullptr;
	}

	if (ComponentClass.Get() == nullptr)
	{
		FAngelscriptEngine::Throw("Class was null.");
		return nullptr;
	}

	if (WithName != NAME_None)
	{
		// Throw an exception if we've already created a component with this name before, or things will go wrong
		auto* ExistingObject = FindObjectFast<UObject>(InActor, WithName);
		if (ExistingObject != nullptr)
		{
			FAngelscriptEngine::Throw("Cannot create component: object with this name already exists.");
			return nullptr;
		}
	}

	if (ComponentClass->HasAnyClassFlags(CLASS_Abstract))
	{
		FAngelscriptEngine::Throw("Cannot create component: component class is abstract.");
		return nullptr;
	}

	if (!ComponentClass->IsChildOf(UActorComponent::StaticClass()))
	{
		FAngelscriptEngine::Throw("Cannot create component: specified class is not a UActorComponent");
		return nullptr;
	}

	FAngelscriptExcludeScopeFromLoopTimeout TimeoutExclusion;
	UActorComponent* Component = NewObject<UActorComponent>(InActor, ComponentClass.Get(), WithName);

	if (!ensure(Component))
		return nullptr;

	// If this is the editor world and we aren't running a construction script,
	// we must be doing editor tooling, and we want to add it as an instance component instead
	bool bAddAsInstanceComponent = false;

#if WITH_EDITOR
	auto* World = InActor->GetWorld();
	if (World != nullptr && !World->IsGameWorld())
	{
		if (!InActor->IsRunningUserConstructionScript())
		{
			bAddAsInstanceComponent = true;
		}
	}
#endif

	if (bAddAsInstanceComponent)
	{
		InActor->AddInstanceComponent(Component);
	}
	else
	{
		struct FHelper_PostCreateBlueprintComponent : public AActor
		{
			void Helper_PostCreateBlueprintComponent(UActorComponent* Component)
			{
				// I regret nothing.
				PostCreateBlueprintComponent(Component);
			}
		};
		((FHelper_PostCreateBlueprintComponent*)InActor)->Helper_PostCreateBlueprintComponent(Component);
	}

	FAngelscriptEngine::Get().GetHooks().GetComponentCreated().ExecuteIfBound(Component);
	Component->OnComponentCreated();

	if (USceneComponent* SceneComponent = Cast<USceneComponent>(Component))
	{
		if (InActor->GetRootComponent() == nullptr)
		{
			// Set as root component if the actor doesn't have a root component
			InActor->SetRootComponent(SceneComponent);
		}
		else
		{
			// Attach to actor root component by default if the actor has a root component
			FAttachmentTransformRules AttachmentRules(EAttachmentRule::KeepRelative, false);
			if (SceneComponent->Mobility == EComponentMobility::Static && InActor->GetRootComponent()->Mobility != EComponentMobility::Static)
				SceneComponent->Mobility = InActor->GetRootComponent()->Mobility;
			SceneComponent->AttachToComponent(InActor->GetRootComponent(), AttachmentRules);
		}
	}

	Component->RegisterComponent();

	return Component;
}

UActorComponent* FAngelscriptActorBinds::GetComponent(AActor* OnActor, const TSubclassOf<UActorComponent>& ComponentClass, const FName& WithName)
{
	if (OnActor == nullptr)
	{
		FAngelscriptEngine::Throw("Actor was null.");
		return nullptr;
	}

	if (ComponentClass.Get() == nullptr)
	{
		FAngelscriptEngine::Throw("Class was null.");
		return nullptr;
	}

	// If a name is passed in, always find a component with that name only, or create it
	if (WithName == NAME_None)
	{
		return OnActor->FindComponentByClass(ComponentClass.Get());
	}
	else
	{
		for (UActorComponent* Comp : OnActor->GetComponents())
		{
			if (Comp == nullptr)
				continue;
			if (Comp->GetFName() != WithName)
				continue;
			if (Comp->IsA(ComponentClass.Get()))
			{
				return Comp;
			}
		}
	}

	return nullptr;
}

UActorComponent* FAngelscriptActorBinds::GetOrCreateComponent(AActor* OnActor, const TSubclassOf<UActorComponent>& ComponentClass, const FName& WithName)
{
	if (OnActor == nullptr)
	{
		FAngelscriptEngine::Throw("Actor was null.");
		return nullptr;
	}

	if (ComponentClass.Get() == nullptr)
	{
		FAngelscriptEngine::Throw("Class was null.");
		return nullptr;
	}

	if (auto* ExistingComponent = FAngelscriptActorBinds::GetComponent(OnActor, ComponentClass.Get(), WithName))
		return ExistingComponent;

	return FAngelscriptActorBinds::CreateComponent(OnActor, ComponentClass, WithName);
}

void FAngelscriptActorBinds::GetAllComponents(AActor* OnActor, UClass* ComponentClass, TArray<UActorComponent*>& OutComponents)
{
	if (OnActor == nullptr)
	{
		FAngelscriptEngine::Throw("Actor was null.");
		return;
	}

	if (ComponentClass == nullptr)
	{
		FAngelscriptEngine::Throw("Class was null.");
		return;
	}

	for (UActorComponent* Comp : OnActor->GetComponents())
	{
		if (Comp == nullptr)
			continue;
		if (Comp->IsA(ComponentClass))
			OutComponents.Add(Comp);
	}
}

UActorComponent* FAngelscriptActorBinds::GetComponentFromMeta(class asCScriptFunction* Meta, AActor* OnActor, const FName& WithName)
{
	if (OnActor == nullptr)
	{
		FAngelscriptEngine::Throw("Actor was null.");
		return nullptr;
	}

	// If a name is passed in, always find a component with that name only
	UClass* ComponentClass = (UClass*)Meta->userData;
	if (WithName == NAME_None)
	{
		return OnActor->FindComponentByClass(ComponentClass);
	}
	else
	{
		for (UActorComponent* Comp : OnActor->GetComponents())
		{
			if (Comp == nullptr)
				continue;
			if (Comp->GetFName() != WithName)
				continue;
			if (Comp->IsA(ComponentClass))
				return Comp;
		}
	}

	return nullptr;
}

UActorComponent* FAngelscriptActorBinds::GetOrCreateComponentFromMeta(class asCScriptFunction* Meta, AActor* OnActor, const FName& WithName)
{
	if (OnActor == nullptr)
	{
		FAngelscriptEngine::Throw("Actor was null.");
		return nullptr;
	}

	// If a name is passed in, always find a component with that name only, or create it
	UClass* ComponentClass = (UClass*)Meta->userData;
	if (WithName == NAME_None)
	{
		auto* Component = OnActor->FindComponentByClass(ComponentClass);
		if (Component != nullptr)
			return Component;
	}
	else
	{
		for (UActorComponent* Comp : OnActor->GetComponents())
		{
			if (Comp == nullptr)
				continue;
			if (Comp->GetFName() != WithName)
				continue;
			if (Comp->IsA(ComponentClass))
				return Comp;
		}
	}

	return FAngelscriptActorBinds::CreateComponent(OnActor, ComponentClass, WithName);
}

UActorComponent* FAngelscriptActorBinds::CreateComponentFromMeta(class asCScriptFunction* Meta, AActor* Actor, const FName& WithName)
{
	UClass* ComponentClass = (UClass*)Meta->userData;
	return FAngelscriptActorBinds::CreateComponent(Actor, ComponentClass, WithName);
}

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_UActorComponent([]
{
	FAngelscriptBinds Binds = FAngelscriptBinds::ExistingClass("UActorComponent");
	Binds.Method("void MarkRenderStateDirty()", [](UActorComponent* Component)
	{
		Component->MarkRenderStateDirty();
	});

	Binds.Method("bool HasBegunPlay() const", METHOD_TRIVIAL(UActorComponent, HasBegunPlay));
	Binds.Method("AActor GetOwner() const", [](UActorComponent* Component) -> AActor*
	{
		return Component->GetOwner();
	});
	Binds.Method("void Activate(bool bReset = false)", [](UActorComponent* Component, bool bReset)
	{
		Component->Activate(bReset);
	});
	Binds.Method("void Deactivate()", [](UActorComponent* Component)
	{
		Component->Deactivate();
	});
	Binds.Method("void DestroyComponent(bool bPromoteChildren = false)", [](UActorComponent* Component, bool bPromoteChildren)
	{
		Component->DestroyComponent(bPromoteChildren);
	});
	Binds.Method("bool ComponentHasTag(FName Tag) const", [](const UActorComponent* Component, FName Tag) -> bool
	{
		if (Tag.IsNone() || Tag == FName(TEXT("None")))
		{
			return false;
		}
		return Component->ComponentHasTag(Tag);
	});

	Binds.Method("void SetbTickInEditor(bool Value)", [](UActorComponent* Component, bool bTickInEditor)
	{
		Component->bTickInEditor = bTickInEditor;
	});

	Binds.Method("void SetbIsEditorOnly(bool Value)", [](UActorComponent* Component, bool bEditorOnly)
	{
		Component->bIsEditorOnly = bEditorOnly;
	});

	Binds.Method("EComponentCreationMethod GetComponentCreationMethod() const", [](const UActorComponent* Component) -> EComponentCreationMethod
	{
		return Component->CreationMethod;
	});

	Binds.Method("void SetIsVisualizationComponent(bool Value)", [](UActorComponent* Component, bool bVisualization)
	{
#if WITH_EDITOR
		Component->SetIsVisualizationComponent(bVisualization);
#endif
	});

	Binds.Method("bool IsVisualizationComponent() const", [](UActorComponent* Component) -> bool
	{
#if WITH_EDITOR
		return Component->IsVisualizationComponent();
#else
		return false;
#endif
	});

	FAngelscriptBinds FComponentReference_ = FAngelscriptBinds::ExistingClass("FComponentReference");
	FComponentReference_.Method("UActorComponent GetComponent(AActor OwningActor) const", &FComponentReference::GetComponent);
});

const static FName NAME_NotAngelscriptSpawnable("NotAngelscriptSpawnable");
AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_Components((int32)FAngelscriptBinds::EOrder::Late + 150, []
{
	FAngelscriptBinds AActor_ = FAngelscriptBinds::ExistingClass("AActor");
	AActor_.Method("UActorComponent CreateComponent(const TSubclassOf<UActorComponent>& ComponentClass, const FName& WithName = NAME_None)", FUNC(FAngelscriptActorBinds::CreateComponent));
	FAngelscriptBinds::SetPreviousBindArgumentDeterminesOutputType(0);

	AActor_.Method("UActorComponent GetComponent(const TSubclassOf<UActorComponent>& ComponentClass, const FName& WithName = NAME_None)", FUNC(FAngelscriptActorBinds::GetComponent));
	FAngelscriptBinds::SetPreviousBindArgumentDeterminesOutputType(0);

	AActor_.Method("UActorComponent GetOrCreateComponent(const TSubclassOf<UActorComponent>& ComponentClass, const FName& WithName = NAME_None)", FUNC(FAngelscriptActorBinds::GetOrCreateComponent));
	FAngelscriptBinds::SetPreviousBindArgumentDeterminesOutputType(0);

	AActor_.Method("void GetAllComponents(UClass ComponentClass, TArray<UActorComponent>& OutComponents)", FUNC(FAngelscriptActorBinds::GetAllComponents));

	for (UClass* Class : TObjectRange<UClass>())
	{
		if (!Class->IsChildOf(UActorComponent::StaticClass()))
			continue;

		auto Type = FAngelscriptType::GetByClass(Class);
		if (!Type.IsValid())
			continue;

		FString ClassName = Type->GetAngelscriptTypeName();
		FAngelscriptBinds Binds = FAngelscriptBinds::ExistingClass(ClassName);

		if (FAngelscriptEngine::Get().ConfigSettings->bAllowRawConstructorsForComponentsAndActors)
		{
			FString Decl = FString::Printf(TEXT("%s f(AActor InActor, FName Name = NAME_None)"), *ClassName);

			Binds.Factory(Decl, &FAngelscriptActorBinds::CreateComponentFromMeta, Class);
			FAngelscriptBinds::PreviousBindPassScriptFunctionAsFirstParam();
		}

		// Static accessors to get or create components
		{
			FAngelscriptBinds::FNamespace ns(ClassName);

			// UActorComponent::Get(Actor)
			{
				FString FuncDecl = FString::Printf(TEXT("%s Get(const AActor Actor, const FName& WithName = NAME_None)"), *ClassName);
				FAngelscriptBinds::BindGlobalFunction(FuncDecl, FUNC(FAngelscriptActorBinds::GetComponentFromMeta), Class);
				FAngelscriptBinds::PreviousBindPassScriptFunctionAsFirstParam();
			}

			bool bSpawnable = true;
#if WITH_EDITOR
			if (Class->HasMetaData(NAME_NotAngelscriptSpawnable))
				bSpawnable = false;
#endif

			// UActorComponent::GetOrCreate(Actor)
			if (bSpawnable)
			{
				FString FuncDecl = FString::Printf(TEXT("%s GetOrCreate(AActor Actor, const FName& WithName = NAME_None)"), *ClassName);
				FAngelscriptBinds::BindGlobalFunction(FuncDecl, FUNC(FAngelscriptActorBinds::GetOrCreateComponentFromMeta), Class);
				FAngelscriptBinds::PreviousBindPassScriptFunctionAsFirstParam();
			}

			// UActorComponent::Create(Actor)
			if (bSpawnable)
			{
				FString FuncDecl = FString::Printf(TEXT("%s Create(AActor Actor, const FName& WithName = NAME_None)"), *ClassName);
				FAngelscriptBinds::BindGlobalFunction(FuncDecl, FUNC(FAngelscriptActorBinds::CreateComponentFromMeta), Class);
				FAngelscriptBinds::PreviousBindPassScriptFunctionAsFirstParam();
			}
		}
	}

	FAngelscriptBinds::BindGlobalFunction("void __Actor_GetComponentByClass(const AActor Actor, const TSubclassOf<UObject>& Class, ?& OutComponent, const FName& WithName)", FUNC_TRIVIAL(FAngelscriptActorBinds::GetComponentGeneric));
	FAngelscriptBinds::BindGlobalFunction("void __Actor_GetOrCreateComponentByClass(AActor Actor, const TSubclassOf<UObject>& Class, ?& OutComponent, const FName& WithName)", FUNC_TRIVIAL(FAngelscriptActorBinds::GetOrCreateComponentGeneric));
	FAngelscriptBinds::BindGlobalFunction("void __Actor_GetAllComponentsByClass(const AActor Actor, const TSubclassOf<UObject>& Class, ?& OutComponents)", FUNC_TRIVIAL(FAngelscriptActorBinds::GetAllComponentsGeneric));
	FAngelscriptBinds::BindGlobalFunction("void __Actor_CreateComponentByClass(AActor Actor, const TSubclassOf<UObject>& Class, ?& OutComponent, const FName& WithName)", FUNC_TRIVIAL(FAngelscriptActorBinds::CreateComponentGeneric));
});
