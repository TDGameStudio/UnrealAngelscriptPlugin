#include "AngelscriptBinds.h"
#include "Bind_USceneComponent_Functions.h"

#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "UObject/UObjectGlobals.h"

namespace
{
	void BindSceneComponent(FAngelscriptBinds& Binds)
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
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_USceneComponent(
	TEXT("USceneComponent.Functions"),
	EAngelscriptBindPhase::ManualBindings,
	&BindSceneComponent);
