#include "AngelscriptBinds.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"

#include "Bind_FHitResult_Functions.h"

namespace
{
	void BindFHitResultFunctions(FAngelscriptBinds& Binds)
	{
		auto FHitResult_ = Binds.ExistingClassForTarget("FHitResult");

		FHitResult_.Constructor(
			"void f(AActor InActor, UPrimitiveComponent InComponent, const FVector& HitLoc, const FVector& HitNorm)",
			&FAngelscriptFHitResultBinds::ConstructActorComponent,
			"FHitResult",
			true)
			.NoDiscard();
		FHitResult_.Constructor(
			"void f(const FVector& TraceStart, const FVector& TraceEnd)",
			&FAngelscriptFHitResultBinds::ConstructTrace,
			"FHitResult",
			true)
			.NoDiscard();

		FHitResult_.Property("int FaceIndex", &FHitResult::FaceIndex);
		FHitResult_.Property("uint8 ElementIndex", &FHitResult::ElementIndex);
		FHitResult_.Property("int Item", &FHitResult::Item);
		FHitResult_.Property("int MyItem", &FHitResult::MyItem);
		FHitResult_.Property("float32 PenetrationDepth", &FHitResult::PenetrationDepth);
		FHitResult_.Property("float32 Distance", &FHitResult::Distance);
		FHitResult_.Property("float32 Time", &FHitResult::Time);
		FHitResult_.Property("FVector TraceStart", &FHitResult::TraceStart);
		FHitResult_.Property("FVector TraceEnd", &FHitResult::TraceEnd);
		FHitResult_.Property("FVector ImpactNormal", &FHitResult::ImpactNormal);
		FHitResult_.Property("FVector ImpactPoint", &FHitResult::ImpactPoint);
		FHitResult_.Property("FVector Location", &FHitResult::Location);
		FHitResult_.Property("FVector Normal", &FHitResult::Normal);
		FHitResult_.Property("FName BoneName", &FHitResult::BoneName);
		FHitResult_.Property("FName MyBoneName", &FHitResult::MyBoneName);

		FHitResult_.Method("void SetComponent(UPrimitiveComponent InComponent)", &FAngelscriptFHitResultBinds::SetComponent);
		FHitResult_.Method("UPrimitiveComponent GetComponent() const", &FAngelscriptFHitResultBinds::GetComponent);
		FHitResult_.Method("void SetActor(AActor InActor)", &FAngelscriptFHitResultBinds::SetActor);
		FHitResult_.Method("AActor GetActor() const", &FAngelscriptFHitResultBinds::GetActor);
		FHitResult_.Method("void Reset()", &FAngelscriptFHitResultBinds::Reset);
		FHitResult_.Method("bool GetbBlockingHit() const", &FAngelscriptFHitResultBinds::GetBlockingHit);
		FHitResult_.Method("void SetBlockingHit(bool bIsBlocking)", &FAngelscriptFHitResultBinds::SetBlockingHit);
		FHitResult_.Method("void SetbBlockingHit(bool bIsBlocking)", &FAngelscriptFHitResultBinds::SetBlockingHit);
		FHitResult_.Method("bool GetbStartPenetrating() const", &FAngelscriptFHitResultBinds::GetStartPenetrating);
		FHitResult_.Method("void SetbStartPenetrating(bool bStartPenetrating)", &FAngelscriptFHitResultBinds::SetStartPenetrating);
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_FHitResult(
	TEXT("FHitResult.Functions"),
	EAngelscriptBindPhase::ManualBindings,
	&BindFHitResultFunctions);
