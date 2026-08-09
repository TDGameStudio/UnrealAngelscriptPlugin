#include "Bind_FHitResult.h"

#include "AngelscriptBinds.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"

/**
 * FHitResult construction, fields, and actor/component access.
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                          | Purpose / parameter notes                                                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FHitResult Hit(AActor InActor,                                                                       | Constructs a hit from resolved contact data.                                                                     |
 * |     UPrimitiveComponent InComponent,                                                                 | @param InActor Hit actor.                                                                                        |
 * |     const FVector& HitLoc,                                                                           | @param InComponent Hit primitive component.                                                                      |
 * |     const FVector& HitNorm);                                                                         | @param HitLoc World-space impact location.                                                                       |
 * |                                                                                                      | @param HitNorm World-space impact normal.                                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FHitResult Hit(const FVector& TraceStart, const FVector& TraceEnd);                                  | Constructs an empty trace hit for a segment.                                                                     |
 * |                                                                                                      | @param TraceStart World-space trace origin.                                                                      |
 * |                                                                                                      | @param TraceEnd World-space trace endpoint.                                                                      |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | int Hit.FaceIndex;                                                                                   | Exposes the triangle face index.                                                                                 |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | uint8 Hit.ElementIndex;                                                                              | Exposes the hit element index.                                                                                   |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | int Hit.Item;                                                                                        | Exposes the hit item identifier.                                                                                 |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | int Hit.MyItem;                                                                                      | Exposes the source item identifier.                                                                              |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | float32 Hit.PenetrationDepth;                                                                        | Exposes initial-overlap penetration depth.                                                                       |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | float32 Hit.Distance;                                                                                | Exposes trace distance to impact.                                                                                |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | float32 Hit.Time;                                                                                    | Exposes normalized trace time.                                                                                   |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FVector Hit.TraceStart;                                                                              | Exposes the trace start.                                                                                         |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FVector Hit.TraceEnd;                                                                                | Exposes the trace end.                                                                                           |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FVector Hit.ImpactNormal;                                                                            | Exposes the impact normal.                                                                                       |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FVector Hit.ImpactPoint;                                                                             | Exposes the impact point.                                                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FVector Hit.Location;                                                                                | Exposes the resolved hit location.                                                                               |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FVector Hit.Normal;                                                                                  | Exposes the swept-shape normal.                                                                                  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FName Hit.BoneName;                                                                                  | Exposes the target bone name.                                                                                    |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FName Hit.MyBoneName;                                                                                | Exposes the source bone name.                                                                                    |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | void Hit.SetComponent(UPrimitiveComponent InComponent);                                              | Sets the weak component reference.                                                                               |
 * |                                                                                                      | @param InComponent Primitive component associated with the hit.                                                  |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | UPrimitiveComponent Hit.GetComponent() const;                                                        | Returns the hit component.                                                                                       |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | void Hit.SetActor(AActor InActor);                                                                   | Sets the weak actor reference.                                                                                   |
 * |                                                                                                      | @param InActor Actor associated with the hit.                                                                    |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | AActor Hit.GetActor() const;                                                                         | Returns the hit actor.                                                                                           |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | void Hit.Reset();                                                                                    | Resets the hit result.                                                                                           |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool Hit.GetbBlockingHit() const;                                                                    | Returns the blocking-hit flag.                                                                                   |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | void Hit.SetBlockingHit(bool bIsBlocking);                                                           | Sets the blocking-hit flag.                                                                                      |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | void Hit.SetbBlockingHit(bool bIsBlocking);                                                          | Alias that sets the blocking-hit flag.                                                                           |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool Hit.GetbStartPenetrating() const;                                                               | Returns the initial-penetration flag.                                                                            |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | void Hit.SetbStartPenetrating(bool bStartPenetrating);                                               | Sets the initial-penetration flag.                                                                               |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 */

AS_FORCE_LINK const FAngelscriptBind Bind_FHitResult(
	TEXT("FHitResult.Functions"),
	EAngelscriptBindPhase::ExplicitBindings,
	[](FAngelscriptBinds& Binds)
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
	});
