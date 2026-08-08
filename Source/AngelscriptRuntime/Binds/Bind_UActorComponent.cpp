#include "AngelscriptBinds.h"
#include "AngelscriptEngine.h"
#include "AngelscriptSettings.h"
#include "AngelscriptType.h"
#include "Bind_AActor_Functions.h"
#include "Bind_UActorComponent_Functions.h"

#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "UObject/UObjectIterator.h"

namespace
{
	const FName NAME_NotAngelscriptSpawnable(TEXT("NotAngelscriptSpawnable"));

	void BindUActorComponent(FAngelscriptBinds& Binds)
	{
		auto ComponentType = Binds.ExistingClassForTarget("UActorComponent");
		ComponentType.Method(
			"void MarkRenderStateDirty()",
			&FAngelscriptUActorComponentBinds::MarkRenderStateDirty);
		ComponentType.Method("bool HasBegunPlay() const", METHOD_TRIVIAL(UActorComponent, HasBegunPlay));
		ComponentType.Method("AActor GetOwner() const", &FAngelscriptUActorComponentBinds::GetOwner);
		ComponentType.Method("void Activate(bool bReset = false)", &FAngelscriptUActorComponentBinds::Activate);
		ComponentType.Method("void Deactivate()", &FAngelscriptUActorComponentBinds::Deactivate);
		ComponentType.Method(
			"void DestroyComponent(bool bPromoteChildren = false)",
			&FAngelscriptUActorComponentBinds::DestroyComponent);
		ComponentType.Method("bool ComponentHasTag(FName Tag) const", &FAngelscriptUActorComponentBinds::ComponentHasTag);
		ComponentType.Method("void SetbTickInEditor(bool Value)", &FAngelscriptUActorComponentBinds::SetTickInEditor);
		ComponentType.Method("void SetbIsEditorOnly(bool Value)", &FAngelscriptUActorComponentBinds::SetIsEditorOnly);
		ComponentType.Method(
			"EComponentCreationMethod GetComponentCreationMethod() const",
			&FAngelscriptUActorComponentBinds::GetComponentCreationMethod);
		ComponentType.Method(
			"void SetIsVisualizationComponent(bool Value)",
			&FAngelscriptUActorComponentBinds::SetIsVisualizationComponent);
		ComponentType.Method(
			"bool IsVisualizationComponent() const",
			&FAngelscriptUActorComponentBinds::IsVisualizationComponent);

		auto ComponentReferenceType = Binds.ExistingClassForTarget("FComponentReference");
		ComponentReferenceType.Method(
			"UActorComponent GetComponent(AActor OwningActor) const",
			&FComponentReference::GetComponent);

		Binds.BindGlobalFunctionForTarget(
			"void __Actor_GetComponentByClass(const AActor Actor, const TSubclassOf<UObject>& Class, ?& OutComponent, const FName& WithName)",
			FUNC_TRIVIAL(FAngelscriptActorBinds::GetComponentGeneric));
		Binds.BindGlobalFunctionForTarget(
			"void __Actor_GetOrCreateComponentByClass(AActor Actor, const TSubclassOf<UObject>& Class, ?& OutComponent, const FName& WithName)",
			FUNC_TRIVIAL(FAngelscriptActorBinds::GetOrCreateComponentGeneric));
		Binds.BindGlobalFunctionForTarget(
			"void __Actor_GetAllComponentsByClass(const AActor Actor, const TSubclassOf<UObject>& Class, ?& OutComponents)",
			FUNC_TRIVIAL(FAngelscriptActorBinds::GetAllComponentsGeneric));
		Binds.BindGlobalFunctionForTarget(
			"void __Actor_CreateComponentByClass(AActor Actor, const TSubclassOf<UObject>& Class, ?& OutComponent, const FName& WithName)",
			FUNC_TRIVIAL(FAngelscriptActorBinds::CreateComponentGeneric));

		auto ActorType = Binds.ExistingClassForTarget("AActor");
		ActorType.Method(
			"UActorComponent CreateComponent(const TSubclassOf<UActorComponent>& ComponentClass, const FName& WithName = NAME_None)",
			FUNC(FAngelscriptActorBinds::CreateComponent))
			.DeterminesOutputType(0);
		ActorType.Method(
			"UActorComponent GetComponent(const TSubclassOf<UActorComponent>& ComponentClass, const FName& WithName = NAME_None)",
			FUNC(FAngelscriptActorBinds::GetComponent))
			.DeterminesOutputType(0);
		ActorType.Method(
			"UActorComponent GetOrCreateComponent(const TSubclassOf<UActorComponent>& ComponentClass, const FName& WithName = NAME_None)",
			FUNC(FAngelscriptActorBinds::GetOrCreateComponent))
			.DeterminesOutputType(0);
		ActorType.Method(
			"void GetAllComponents(UClass ComponentClass, TArray<UActorComponent>& OutComponents)",
			FUNC(FAngelscriptActorBinds::GetAllComponents));
	}

	void BindComponentPostReflectionAccessors(FAngelscriptBinds& Binds)
	{
		for (UClass* Class : TObjectRange<UClass>())
		{
			if (!Class->IsChildOf(UActorComponent::StaticClass()))
				continue;

			const TSharedPtr<FAngelscriptType> Type = FAngelscriptType::GetByClass(
				Binds.GetTargetTypeDatabase(),
				Class);
			if (!Type.IsValid())
				continue;

			const FString ClassName = Type->GetAngelscriptTypeName();
			auto ComponentType = Binds.ExistingClassForTarget(ClassName);
			if (Binds.GetTargetEngine().ConfigSettings->bAllowRawConstructorsForComponentsAndActors)
			{
				const FString Declaration = FString::Printf(
					TEXT("%s f(AActor InActor, FName Name = NAME_None)"),
					*ClassName);
				ComponentType.Factory(
					Declaration,
					&FAngelscriptActorBinds::CreateComponentFromMeta,
					Class)
					.PassScriptFunctionAsFirstParam();
			}

			FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), ClassName);
			const FString GetDeclaration = FString::Printf(
				TEXT("%s Get(const AActor Actor, const FName& WithName = NAME_None)"),
				*ClassName);
			Binds.BindGlobalFunctionForTarget(
				GetDeclaration,
				FUNC(FAngelscriptActorBinds::GetComponentFromMeta),
				Class)
				.PassScriptFunctionAsFirstParam();

			bool bSpawnable = true;
#if WITH_EDITOR
			if (Class->HasMetaData(NAME_NotAngelscriptSpawnable))
				bSpawnable = false;
#endif
			if (bSpawnable)
			{
				const FString GetOrCreateDeclaration = FString::Printf(
					TEXT("%s GetOrCreate(AActor Actor, const FName& WithName = NAME_None)"),
					*ClassName);
				Binds.BindGlobalFunctionForTarget(
					GetOrCreateDeclaration,
					FUNC(FAngelscriptActorBinds::GetOrCreateComponentFromMeta),
					Class)
					.PassScriptFunctionAsFirstParam();

				const FString CreateDeclaration = FString::Printf(
					TEXT("%s Create(AActor Actor, const FName& WithName = NAME_None)"),
					*ClassName);
				Binds.BindGlobalFunctionForTarget(
					CreateDeclaration,
					FUNC(FAngelscriptActorBinds::CreateComponentFromMeta),
					Class)
					.PassScriptFunctionAsFirstParam();
			}
		}
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_UActorComponent(
	TEXT("UActorComponent.Manual"),
	EAngelscriptBindPhase::ManualBindings,
	&BindUActorComponent);

AS_FORCE_LINK const FAngelscriptBind Bind_Components(
	TEXT("UActorComponent.PostReflection"),
	EAngelscriptBindPhase::PostReflectionBindings,
	&BindComponentPostReflectionAccessors);
