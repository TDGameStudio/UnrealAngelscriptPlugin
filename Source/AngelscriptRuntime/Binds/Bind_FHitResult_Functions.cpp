#include "Bind_FHitResult.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"

void FAngelscriptFHitResultBinds::ConstructActorComponent(
	FHitResult* Address,
	AActor* InActor,
	UPrimitiveComponent* InComponent,
	const FVector& HitLoc,
	const FVector& HitNorm)
{
	new (Address) FHitResult(InActor, InComponent, HitLoc, HitNorm);
}

void FAngelscriptFHitResultBinds::ConstructTrace(FHitResult* Address, const FVector& TraceStart, const FVector& TraceEnd)
{
	new (Address) FHitResult(TraceStart, TraceEnd);
}

void FAngelscriptFHitResultBinds::SetComponent(FHitResult* HitResult, UPrimitiveComponent* Component)
{
	HitResult->Component = Component;
}

UPrimitiveComponent* FAngelscriptFHitResultBinds::GetComponent(FHitResult* HitResult)
{
	return HitResult->GetComponent();
}

void FAngelscriptFHitResultBinds::SetActor(FHitResult* HitResult, AActor* Actor)
{
	HitResult->HitObjectHandle = FActorInstanceHandle(Actor);
}

AActor* FAngelscriptFHitResultBinds::GetActor(FHitResult* HitResult)
{
	return HitResult->GetActor();
}

void FAngelscriptFHitResultBinds::Reset(FHitResult* HitResult)
{
	HitResult->Reset();
}

bool FAngelscriptFHitResultBinds::GetBlockingHit(FHitResult* HitResult)
{
	return HitResult->bBlockingHit;
}

void FAngelscriptFHitResultBinds::SetBlockingHit(FHitResult* HitResult, const bool bIsBlocking)
{
	HitResult->bBlockingHit = bIsBlocking;
}

bool FAngelscriptFHitResultBinds::GetStartPenetrating(FHitResult* HitResult)
{
	return HitResult->bStartPenetrating;
}

void FAngelscriptFHitResultBinds::SetStartPenetrating(FHitResult* HitResult, const bool bStartPenetrating)
{
	HitResult->bStartPenetrating = bStartPenetrating;
}
