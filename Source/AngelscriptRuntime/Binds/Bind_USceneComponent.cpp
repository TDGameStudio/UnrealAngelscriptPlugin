#include "Bind_USceneComponent.h"

#include "AngelscriptBinds.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "UObject/UObjectGlobals.h"

/**
 * Scene-component manual binding surface.
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                | Purpose / parameter notes                                                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | int32 USceneComponent.GetNumChildrenComponents() const;                                    | Returns the number of direct child scene components.                                                                 |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | USceneComponent USceneComponent.GetChildComponentByClass(                                  | Returns the first matching child, with the result typed to ComponentClass.                                           |
 * |     TSubclassOf<USceneComponent> ComponentClass);                                          | @param ComponentClass Required scene-component subclass.                                                             |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void USceneComponent.GetChildrenComponentsByClass(                                         | Collects matching child components into the typed output array.                                                      |
 * |     UClass ComponentClass, bool bIncludeAllDescendants, ?& OutChildren);                   | @param bIncludeAllDescendants Includes recursive descendants when true.                                              |
 * |                                                                                            | @param OutChildren Receives an array whose element type matches ComponentClass.                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FTransform USceneComponent.GetComponentTransform() const;                                  | Returns the component world transform.                                                                               |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void USceneComponent.SetRelativeLocation(FVector NewLocation);                             | Sets location relative to the attachment parent, in Unreal units.                                                    |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void USceneComponent.SetComponentVelocity(const FVector& Velocity);                        | Sets world-space component velocity in Unreal units per second.                                                      |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FVector USceneComponent.GetComponentVelocity() const;                                      | Returns world-space component velocity in Unreal units per second.                                                   |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void USphereComponent.SetSphereRadius(                                                     | Sets the unscaled sphere radius in Unreal units.                                                                     |
 * |     float32 InSphereRadius, bool bUpdateOverlaps = true);                                  | @param bUpdateOverlaps Recomputes overlap state immediately when true.                                               |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | struct FScopedMovementUpdate;                                                              | Declares an RAII movement-update scope.                                                                              |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FScopedMovementUpdate Scope(USceneComponent Component);                                    | Defers scoped movement updates until Scope leaves its script lifetime.                                               |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 */

AS_FORCE_LINK const FAngelscriptBind Bind_USceneComponent(
	TEXT("USceneComponent.Functions"),
	EAngelscriptBindPhase::ExplicitBindings,
	[](FAngelscriptBinds& Binds)
	{
		auto USceneComponent_ = Binds.ExistingClassForTarget("USceneComponent");
		USceneComponent_.Method("int32 GetNumChildrenComponents() const", METHOD_TRIVIAL(USceneComponent, GetNumChildrenComponents));
		USceneComponent_.Method(
			"USceneComponent GetChildComponentByClass(TSubclassOf<USceneComponent> ComponentClass)",
			&FAngelscriptUSceneComponentBinds::GetChildComponentByClass)
			.DeterminesOutputType(0);
		USceneComponent_.Method(
			"void GetChildrenComponentsByClass(UClass ComponentClass, bool bIncludeAllDescendants, ?& OutChildren)",
			&FAngelscriptUSceneComponentBinds::GetChildrenComponentsByClass);
		USceneComponent_.Method(
			"FTransform GetComponentTransform() const",
			&FAngelscriptUSceneComponentBinds::GetComponentTransform);
		USceneComponent_.Method(
			"void SetRelativeLocation(FVector NewLocation)",
			&FAngelscriptUSceneComponentBinds::SetRelativeLocation);
		USceneComponent_.Method(
			"void SetComponentVelocity(const FVector& Velocity)",
			&FAngelscriptUSceneComponentBinds::SetComponentVelocity);
		USceneComponent_.Method("FVector GetComponentVelocity() const", METHOD_TRIVIAL(USceneComponent, GetComponentVelocity));

		auto USphereComponent_ = Binds.ExistingClassForTarget("USphereComponent");
		USphereComponent_.Method(
			"void SetSphereRadius(float32 InSphereRadius, bool bUpdateOverlaps = true)",
			METHOD_TRIVIAL(USphereComponent, SetSphereRadius));

		auto FScopedMovementUpdate_ = Binds.ValueClassForTarget<FAngelscriptUSceneComponentBinds::FScriptScopedMovementUpdate>(
			"FScopedMovementUpdate",
			FBindFlags());
		FScopedMovementUpdate_.Constructor(
			"void f(USceneComponent Component)",
			&FAngelscriptUSceneComponentBinds::ConstructScopedMovementUpdate)
			.NoDiscard();
		FScopedMovementUpdate_.Destructor(
			"void f()",
			&FAngelscriptUSceneComponentBinds::DestructScopedMovementUpdate);

	#if WITH_EDITOR
		// The Blueprint version is deprecated; suppress its reflective duplicate so
		// the hand-written script surface remains the only registration.
		UFunction* Function = FindObject<UFunction>(
			nullptr,
			TEXT("/Script/Engine.SceneComponent:GetSocketQuaternion"));
		if (Function != nullptr)
		{
			Function->SetMetaData(TEXT("NotInAngelscript"), TEXT("true"));
		}
	#endif
	});
