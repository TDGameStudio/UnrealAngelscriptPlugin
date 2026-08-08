#pragma once

#include "Math/MathFwd.h"

class AActor;
class UPrimitiveComponent;
struct FHitResult;

struct FAngelscriptFHitResultBinds
{
	static void ConstructActorComponent(
		FHitResult* Address,
		AActor* InActor,
		UPrimitiveComponent* InComponent,
		const FVector& HitLoc,
		const FVector& HitNorm);
	static void ConstructTrace(FHitResult* Address, const FVector& TraceStart, const FVector& TraceEnd);
	static void SetComponent(FHitResult* HitResult, UPrimitiveComponent* Component);
	static UPrimitiveComponent* GetComponent(FHitResult* HitResult);
	static void SetActor(FHitResult* HitResult, AActor* Actor);
	static AActor* GetActor(FHitResult* HitResult);
	static void Reset(FHitResult* HitResult);
	static bool GetBlockingHit(FHitResult* HitResult);
	static void SetBlockingHit(FHitResult* HitResult, bool bIsBlocking);
	static bool GetStartPenetrating(FHitResult* HitResult);
	static void SetStartPenetrating(FHitResult* HitResult, bool bStartPenetrating);
};
