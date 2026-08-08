#include "Bind_AActor.h"

#include "AngelscriptEngine.h"
#include "AngelscriptType.h"

#include "Components/InputComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/Class.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_objecttype.h"
#include "source/as_scriptengine.h"
#include "source/as_scriptfunction.h"
#include "EndAngelscriptHeaders.h"

namespace
{
	UClass* ResolveWildcardArrayElementClass(int TypeId, UClass* RequiredBaseClass, const ANSICHAR* ErrorMessage)
	{
		auto& Manager = FAngelscriptEngine::Get();
		asCTypeInfo* ScriptType = (asCTypeInfo*)Manager.Engine->GetTypeInfoById(TypeId);
		if (ScriptType == nullptr || (ScriptType->flags & asOBJ_VALUE) == 0)
		{
			FAngelscriptEngine::Throw(ErrorMessage);
			return nullptr;
		}

		asCObjectType* ObjectType = (asCObjectType*)ScriptType;
		const FAngelscriptTypeDatabase* TypeDatabase = Manager.GetTypeDatabase();
		if (TypeDatabase == nullptr || ObjectType->templateBaseType != TypeDatabase->ArrayTemplateTypeInfo)
		{
			FAngelscriptEngine::Throw(ErrorMessage);
			return nullptr;
		}

		auto* ElementTypeInfo = ObjectType->templateSubTypes[0].GetTypeInfo();
		if (ElementTypeInfo == nullptr
			|| (ElementTypeInfo->GetFlags() & asOBJ_REF) == 0
			|| ElementTypeInfo->plainUserData == 0)
		{
			FAngelscriptEngine::Throw(ErrorMessage);
			return nullptr;
		}

		UClass* ElementClass = (UClass*)ElementTypeInfo->plainUserData;
		if (!ElementClass->IsChildOf(RequiredBaseClass))
		{
			FAngelscriptEngine::Throw(ErrorMessage);
			return nullptr;
		}

		return ElementClass;
	}

	ULevel* ResolveSpawnLevel(UObject* WorldContext, UWorld* World, ULevel* ExplicitLevel)
	{
		if (ExplicitLevel != nullptr)
			return ExplicitLevel;
		if (World->IsGameWorld() && FAngelscriptEngine::Get().GetDynamicSpawnLevel().IsBound())
			return FAngelscriptEngine::Get().GetDynamicSpawnLevel().Execute();
		if (auto* Component = Cast<UActorComponent>(WorldContext))
			return Component->GetOwner() ? Component->GetOwner()->GetLevel() : nullptr;
		if (auto* Actor = Cast<AActor>(WorldContext))
			return Actor->GetLevel();
		return nullptr;
	}
}

void FAngelscriptActorBinds::GetComponentsByClass(
	const AActor* Actor,
	TArray<UActorComponent*>& OutComponents,
	int TypeId)
{
	UClass* ComponentClass = ResolveWildcardArrayElementClass(
		TypeId,
		UActorComponent::StaticClass(),
		"GetComponentsByClass must take a TArray of components as its out argument.");
	if (ComponentClass == nullptr)
		return;

	if (Actor == nullptr)
	{
		FAngelscriptEngine::Throw("Actor was null.");
		return;
	}

	for (UActorComponent* Component : Actor->GetComponents())
	{
		if (Component != nullptr && Component->IsA(ComponentClass))
			OutComponents.Add(Component);
	}
}

void FAngelscriptActorBinds::GetComponentsByClassWithExplicitClass(
	const AActor* Actor,
	UClass* ComponentClass,
	TArray<UActorComponent*>& OutComponents,
	int TypeId)
{
	UClass* ArrayElementClass = ResolveWildcardArrayElementClass(
		TypeId,
		UActorComponent::StaticClass(),
		"GetComponentsByClass must take a TArray of components as its out argument.");
	if (ArrayElementClass == nullptr)
		return;

	if (Actor == nullptr)
	{
		FAngelscriptEngine::Throw("Actor was null.");
		return;
	}
	if (ComponentClass == nullptr)
	{
		FAngelscriptEngine::Throw("Component class was null.");
		return;
	}
	if (!ComponentClass->IsChildOf(ArrayElementClass))
	{
		FAngelscriptEngine::Throw("Class specified to GetComponentsByClass is not a child of array element class.");
		return;
	}

	for (UActorComponent* Component : Actor->GetComponents())
	{
		if (Component != nullptr && Component->IsA(ComponentClass))
			OutComponents.Add(Component);
	}
}

APawn* FAngelscriptActorBinds::GetInstigator(const AActor* Actor)
{
	return Actor->GetInstigator();
}

AController* FAngelscriptActorBinds::GetInstigatorController(const AActor* Actor)
{
	return Actor->GetInstigatorController();
}

UInputComponent* FAngelscriptActorBinds::GetInputComponent(const AActor* Actor)
{
	return Actor != nullptr ? Actor->InputComponent : nullptr;
}

void FAngelscriptActorBinds::EnableInput(AActor* Actor, APlayerController* PlayerController)
{
	if (Actor != nullptr)
		Actor->EnableInput(PlayerController);
}

void FAngelscriptActorBinds::DisableInput(AActor* Actor, APlayerController* PlayerController)
{
	if (Actor != nullptr)
		Actor->DisableInput(PlayerController);
}

UActorComponent* FAngelscriptActorBinds::CreateComponent(
	AActor* InActor,
	const TSubclassOf<UActorComponent>& ComponentClass,
	const FName& WithName)
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
	if (WithName != NAME_None && FindObjectFast<UObject>(InActor, WithName) != nullptr)
	{
		FAngelscriptEngine::Throw("Cannot create component: object with this name already exists.");
		return nullptr;
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

	bool bAddAsInstanceComponent = false;
#if WITH_EDITOR
	if (UWorld* World = InActor->GetWorld(); World != nullptr && !World->IsGameWorld())
	{
		if (!InActor->IsRunningUserConstructionScript())
			bAddAsInstanceComponent = true;
	}
#endif

	if (bAddAsInstanceComponent)
	{
		InActor->AddInstanceComponent(Component);
	}
	else
	{
		struct FPostCreateBlueprintComponentHelper : public AActor
		{
			void CallPostCreateBlueprintComponent(UActorComponent* InComponent)
			{
				PostCreateBlueprintComponent(InComponent);
			}
		};
		((FPostCreateBlueprintComponentHelper*)InActor)->CallPostCreateBlueprintComponent(Component);
	}

	FAngelscriptEngine::Get().GetComponentCreated().ExecuteIfBound(Component);
	Component->OnComponentCreated();

	if (USceneComponent* SceneComponent = Cast<USceneComponent>(Component))
	{
		if (InActor->GetRootComponent() == nullptr)
		{
			InActor->SetRootComponent(SceneComponent);
		}
		else
		{
			const FAttachmentTransformRules AttachmentRules(EAttachmentRule::KeepRelative, false);
			if (SceneComponent->Mobility == EComponentMobility::Static
				&& InActor->GetRootComponent()->Mobility != EComponentMobility::Static)
			{
				SceneComponent->Mobility = InActor->GetRootComponent()->Mobility;
			}
			SceneComponent->AttachToComponent(InActor->GetRootComponent(), AttachmentRules);
		}
	}

	Component->RegisterComponent();
	return Component;
}

UActorComponent* FAngelscriptActorBinds::GetComponent(
	AActor* OnActor,
	const TSubclassOf<UActorComponent>& ComponentClass,
	const FName& WithName)
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
	if (WithName == NAME_None)
		return OnActor->FindComponentByClass(ComponentClass.Get());

	for (UActorComponent* Component : OnActor->GetComponents())
	{
		if (Component != nullptr && Component->GetFName() == WithName && Component->IsA(ComponentClass.Get()))
			return Component;
	}
	return nullptr;
}

UActorComponent* FAngelscriptActorBinds::GetOrCreateComponent(
	AActor* OnActor,
	const TSubclassOf<UActorComponent>& ComponentClass,
	const FName& WithName)
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
	if (UActorComponent* ExistingComponent = GetComponent(OnActor, ComponentClass, WithName))
		return ExistingComponent;
	return CreateComponent(OnActor, ComponentClass, WithName);
}

void FAngelscriptActorBinds::GetAllComponents(
	AActor* OnActor,
	UClass* ComponentClass,
	TArray<UActorComponent*>& OutComponents)
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
	for (UActorComponent* Component : OnActor->GetComponents())
	{
		if (Component != nullptr && Component->IsA(ComponentClass))
			OutComponents.Add(Component);
	}
}

UActorComponent* FAngelscriptActorBinds::GetComponentFromMeta(
	asCScriptFunction* Meta,
	AActor* OnActor,
	const FName& WithName)
{
	if (OnActor == nullptr)
	{
		FAngelscriptEngine::Throw("Actor was null.");
		return nullptr;
	}
	UClass* ComponentClass = (UClass*)Meta->userData;
	if (WithName == NAME_None)
		return OnActor->FindComponentByClass(ComponentClass);
	for (UActorComponent* Component : OnActor->GetComponents())
	{
		if (Component != nullptr && Component->GetFName() == WithName && Component->IsA(ComponentClass))
			return Component;
	}
	return nullptr;
}

UActorComponent* FAngelscriptActorBinds::GetOrCreateComponentFromMeta(
	asCScriptFunction* Meta,
	AActor* OnActor,
	const FName& WithName)
{
	if (OnActor == nullptr)
	{
		FAngelscriptEngine::Throw("Actor was null.");
		return nullptr;
	}
	UClass* ComponentClass = (UClass*)Meta->userData;
	if (WithName == NAME_None)
	{
		if (UActorComponent* Component = OnActor->FindComponentByClass(ComponentClass))
			return Component;
	}
	else
	{
		for (UActorComponent* Component : OnActor->GetComponents())
		{
			if (Component != nullptr && Component->GetFName() == WithName && Component->IsA(ComponentClass))
				return Component;
		}
	}
	return CreateComponent(OnActor, ComponentClass, WithName);
}

UActorComponent* FAngelscriptActorBinds::CreateComponentFromMeta(
	asCScriptFunction* Meta,
	AActor* Actor,
	const FName& WithName)
{
	return CreateComponent(Actor, (UClass*)Meta->userData, WithName);
}

AActor* FAngelscriptActorBinds::SpawnActorFromMeta(
	asCScriptFunction* Meta,
	const FVector& Location,
	const FRotator& Rotation,
	const FName& Name,
	ULevel* Level)
{
	UObject* WorldContext = FAngelscriptEngine::TryGetCurrentWorldContextObject();
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull);
	if (World == nullptr)
	{
		FAngelscriptEngine::Throw("Invalid World Context");
		return nullptr;
	}
	UClass* ActorClass = (UClass*)Meta->userData;
	if (ActorClass == nullptr)
	{
		FAngelscriptEngine::Throw("Class was nullptr.");
		return nullptr;
	}

	FActorSpawnParameters Parameters;
	Parameters.Name = Name;
	Parameters.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
	Parameters.OverrideLevel = ResolveSpawnLevel(WorldContext, World, Level);
	return World->SpawnActor(ActorClass, &Location, &Rotation, Parameters);
}

AActor* FAngelscriptActorBinds::SpawnActor(
	const TSubclassOf<AActor>& Class,
	const FVector& Location,
	const FRotator& Rotation,
	const FName& Name,
	bool bDeferredSpawn,
	ULevel* Level)
{
	UObject* WorldContext = FAngelscriptEngine::TryGetCurrentWorldContextObject();
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull);
	if (World == nullptr)
	{
		FAngelscriptEngine::Throw("Invalid World Context");
		return nullptr;
	}
	if (Class == nullptr)
	{
		FAngelscriptEngine::Throw("Class was nullptr.");
		return nullptr;
	}

	FActorSpawnParameters Parameters;
	Parameters.Name = Name;
	Parameters.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
	Parameters.bDeferConstruction = bDeferredSpawn;
	Parameters.OverrideLevel = ResolveSpawnLevel(WorldContext, World, Level);
	return World->SpawnActor(Class, &Location, &Rotation, Parameters);
}

AActor* FAngelscriptActorBinds::SpawnPersistentActor(
	const TSubclassOf<AActor>& Class,
	const FVector& Location,
	const FRotator& Rotation,
	const FName& Name,
	bool bDeferredSpawn)
{
	UObject* WorldContext = FAngelscriptEngine::TryGetCurrentWorldContextObject();
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull);
	if (World == nullptr)
	{
		FAngelscriptEngine::Throw("Invalid World Context");
		return nullptr;
	}
	if (Class == nullptr)
	{
		FAngelscriptEngine::Throw("Class was nullptr.");
		return nullptr;
	}

	FActorSpawnParameters Parameters;
	Parameters.Name = Name;
	Parameters.NameMode = FActorSpawnParameters::ESpawnActorNameMode::Requested;
	Parameters.bDeferConstruction = bDeferredSpawn;
	return World->SpawnActor(Class, &Location, &Rotation, Parameters);
}

void FAngelscriptActorBinds::FinishSpawningActor(AActor* Actor)
{
	if (Actor == nullptr)
		return;
	if (Actor->HasActorBegunPlay())
	{
		FAngelscriptEngine::Throw("Actor has already finished spawning. Did you pass bDeferredSpawn=true to the spawn call?");
		return;
	}
	Actor->FinishSpawning(Actor->GetActorTransform());
}

void FAngelscriptActorBinds::FinishSpawningActor_Transform(AActor* Actor, const FTransform& SpawnTransform)
{
	if (Actor == nullptr)
		return;
	if (Actor->HasActorBegunPlay())
	{
		FAngelscriptEngine::Throw("Actor has already finished spawning. Did you pass bDeferredSpawn=true to the spawn call?");
		return;
	}
	Actor->FinishSpawning(SpawnTransform);
}

void FAngelscriptActorBinds::GetAllActorsOfClass(TArray<AActor*>& OutActors, int TypeId)
{
	UClass* ActorClass = ResolveWildcardArrayElementClass(
		TypeId,
		AActor::StaticClass(),
		"GetAllActors must take a TArray of actors as its out argument.");
	if (ActorClass != nullptr)
	{
		UGameplayStatics::GetAllActorsOfClass(
			FAngelscriptEngine::TryGetCurrentWorldContextObject(),
			ActorClass,
			OutActors);
	}
}

void FAngelscriptActorBinds::GetAllActorsOfClassWithExplicitClass(
	UClass* ActorClass,
	TArray<AActor*>& OutActors,
	int TypeId)
{
	UClass* ArrayElementClass = ResolveWildcardArrayElementClass(
		TypeId,
		AActor::StaticClass(),
		"GetAllActors must take a TArray of actors as its out argument.");
	if (ArrayElementClass == nullptr)
		return;
	if (ActorClass == nullptr)
	{
		FAngelscriptEngine::Throw("Actor class was null.");
		return;
	}
	if (!ActorClass->IsChildOf(ArrayElementClass))
	{
		FAngelscriptEngine::Throw("Class specified to GetAllActorsOfClass is not a child of array element class.");
		return;
	}
	UGameplayStatics::GetAllActorsOfClass(
		FAngelscriptEngine::TryGetCurrentWorldContextObject(),
		ActorClass,
		OutActors);
}

void FAngelscriptActorBinds::GetAllActorsOfClassWithTag(
	FName Tag,
	TArray<AActor*>& OutActors,
	int TypeId)
{
	UClass* ActorClass = ResolveWildcardArrayElementClass(
		TypeId,
		AActor::StaticClass(),
		"GetAllActors must take a TArray of actors as its out argument.");
	if (ActorClass != nullptr)
	{
		UGameplayStatics::GetAllActorsOfClassWithTag(
			FAngelscriptEngine::TryGetCurrentWorldContextObject(),
			ActorClass,
			Tag,
			OutActors);
	}
}
