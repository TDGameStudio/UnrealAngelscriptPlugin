#include "Bind_UActorComponent.h"

#include "AngelscriptBinds.h"
#include "AngelscriptEngine.h"
#include "AngelscriptSettings.h"
#include "AngelscriptType.h"
#include "Bind_AActor.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "UObject/UObjectIterator.h"

/**
 * Actor component binding and reflected component-access surface.
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                                                  | Purpose / parameter notes                                                                                            |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void UActorComponent.MarkRenderStateDirty();                                                                                 | Marks the component's render state for recreation on the render thread.                                              |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool UActorComponent.HasBegunPlay() const;                                                                                   | Reports whether BeginPlay has run for this component.                                                                |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | AActor UActorComponent.GetOwner() const;                                                                                     | Returns the actor that owns this component, or null.                                                                 |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void UActorComponent.Activate(bool bReset = false);                                                                          | Activates the component when allowed by its activation state.                                                        |
 * |                                                                                                                              | @param bReset Forces activation even when the component is already active.                                           |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void UActorComponent.Deactivate();                                                                                           | Deactivates the component.                                                                                           |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void UActorComponent.DestroyComponent(bool bPromoteChildren = false);                                                        | Unregisters and destroys this component.                                                                             |
 * |                                                                                                                              | @param bPromoteChildren Reattaches scene-component children to this component's parent when true.                    |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool UActorComponent.ComponentHasTag(FName Tag) const;                                                                       | Reports whether ComponentTags contains Tag.                                                                          |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void UActorComponent.SetbTickInEditor(bool Value);                                                                           | Enables or disables component ticking while editing.                                                                 |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void UActorComponent.SetbIsEditorOnly(bool Value);                                                                           | Marks whether the component is excluded from non-editor builds and cooked data.                                      |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | EComponentCreationMethod UActorComponent.GetComponentCreationMethod() const;                                                 | Returns how the component was created, such as native, instance, or construction script.                             |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void UActorComponent.SetIsVisualizationComponent(bool Value);                                                                | Marks whether this component exists only for editor visualization.                                                   |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool UActorComponent.IsVisualizationComponent() const;                                                                       | Reports whether the component is editor visualization-only.                                                          |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | UActorComponent FComponentReference.GetComponent(AActor OwningActor) const;                                                  | Resolves this component reference against OwningActor.                                                               |
 * |                                                                                                                              | @param OwningActor Actor used for relative component lookup.                                                         |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void __Actor_GetComponentByClass(                                                                                            | Compiler helper that resolves one component and writes its specialized output.                                       |
 * |     const AActor Actor,                                                                                                      | @param OutComponent Receives the matching component or null.                                                         |
 * |     const TSubclassOf<UObject>& Class,                                                                                       | @param WithName Optional component-name filter; NAME_None accepts any name.                                          |
 * |     ?& OutComponent,                                                                                                         |                                                                                                                      |
 * |     const FName& WithName);                                                                                                  |                                                                                                                      |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void __Actor_GetOrCreateComponentByClass(                                                                                    | Compiler helper that resolves or creates a component and writes its specialized output.                              |
 * |     AActor Actor,                                                                                                            | @param OutComponent Receives the existing or newly created component.                                                |
 * |     const TSubclassOf<UObject>& Class,                                                                                       | @param WithName Optional component-name filter or creation name.                                                     |
 * |     ?& OutComponent,                                                                                                         |                                                                                                                      |
 * |     const FName& WithName);                                                                                                  |                                                                                                                      |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void __Actor_GetAllComponentsByClass(const AActor Actor, const TSubclassOf<UObject>& Class, ?& OutComponents);               | Compiler helper that writes every matching component to a specialized output array.                                  |
 * |                                                                                                                              | @param OutComponents Receives the matching component array.                                                          |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void __Actor_CreateComponentByClass(                                                                                         | Compiler helper that creates a component and writes its specialized output.                                          |
 * |     AActor Actor,                                                                                                            | @param OutComponent Receives the newly created component.                                                            |
 * |     const TSubclassOf<UObject>& Class,                                                                                       | @param WithName Optional name for the new component.                                                                 |
 * |     ?& OutComponent,                                                                                                         |                                                                                                                      |
 * |     const FName& WithName);                                                                                                  |                                                                                                                      |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | UActorComponent AActor.CreateComponent(                                                                                      | Creates and returns a component whose script result specializes to ComponentClass.                                   |
 * |     const TSubclassOf<UActorComponent>& ComponentClass,                                                                      | @param WithName Optional name for the new component.                                                                 |
 * |     const FName& WithName = NAME_None);                                                                                      |                                                                                                                      |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | UActorComponent AActor.GetComponent(const TSubclassOf<UActorComponent>& ComponentClass, const FName& WithName = NAME_None);  | Returns the first matching component, with its script result specialized to ComponentClass.                          |
 * |                                                                                                                              | @param WithName Optional component-name filter.                                                                      |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | UActorComponent AActor.GetOrCreateComponent(                                                                                 | Returns a matching component or creates one, specializing the script result type.                                    |
 * |     const TSubclassOf<UActorComponent>& ComponentClass,                                                                      | @param WithName Optional component-name filter or creation name.                                                     |
 * |     const FName& WithName = NAME_None);                                                                                      |                                                                                                                      |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void AActor.GetAllComponents(UClass ComponentClass, TArray<UActorComponent>& OutComponents);                                 | Collects all components compatible with ComponentClass.                                                              |
 * |                                                                                                                              | @param OutComponents Receives all matching components.                                                               |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | <ComponentType> Value(AActor InActor, FName Name = NAME_None);                                                               | Runtime pattern: optional raw factory for each reflected component class.                                            |
 * |                                                                                                                              | Enabled only when raw actor/component constructors are allowed by configuration.                                     |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | <ComponentType> <ComponentType>::Get(const AActor Actor, const FName& WithName = NAME_None);                                 | Runtime pattern: returns a reflected component subtype from Actor.                                                   |
 * |                                                                                                                              | @param WithName Optional component-name filter.                                                                      |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | <ComponentType> <ComponentType>::GetOrCreate(AActor Actor, const FName& WithName = NAME_None);                               | Runtime pattern: returns or creates each spawnable reflected component subtype.                                      |
 * |                                                                                                                              | Classes marked NotAngelscriptSpawnable do not receive this declaration.                                              |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | <ComponentType> <ComponentType>::Create(AActor Actor, const FName& WithName = NAME_None);                                    | Runtime pattern: creates each spawnable reflected component subtype.                                                 |
 * |                                                                                                                              | Classes marked NotAngelscriptSpawnable do not receive this declaration.                                              |
 * +------------------------------------------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 */

namespace
{
	const FName NAME_NotAngelscriptSpawnable(TEXT("NotAngelscriptSpawnable"));


}

AS_FORCE_LINK const FAngelscriptBind Bind_UActorComponent(
	TEXT("UActorComponent.Manual"),
	EAngelscriptBindPhase::ManualBindings,
	[](FAngelscriptBinds& Binds)
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
	});

AS_FORCE_LINK const FAngelscriptBind Bind_Components(
	TEXT("UActorComponent.PostReflection"),
	EAngelscriptBindPhase::PostReflectionBindings,
	[](FAngelscriptBinds& Binds)
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
	});
