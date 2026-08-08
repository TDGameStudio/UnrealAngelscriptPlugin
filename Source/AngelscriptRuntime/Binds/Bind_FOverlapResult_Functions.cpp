#include "Bind_FOverlapResult.h"

#include "Engine/OverlapResult.h"

void FAngelscriptFOverlapResultBinds::SetComponent(FOverlapResult* OverlapResult, UPrimitiveComponent* Component)
{
	OverlapResult->Component = Component;
}

UPrimitiveComponent* FAngelscriptFOverlapResultBinds::GetComponent(FOverlapResult* OverlapResult)
{
	return OverlapResult->GetComponent();
}

void FAngelscriptFOverlapResultBinds::SetActor(FOverlapResult* OverlapResult, AActor* Actor)
{
	OverlapResult->OverlapObjectHandle = FActorInstanceHandle(Actor);
}

AActor* FAngelscriptFOverlapResultBinds::GetActor(FOverlapResult* OverlapResult)
{
	return OverlapResult->GetActor();
}

bool FAngelscriptFOverlapResultBinds::GetBlockingHit(FOverlapResult* OverlapResult)
{
	return OverlapResult->bBlockingHit;
}

void FAngelscriptFOverlapResultBinds::SetBlockingHit(FOverlapResult* OverlapResult, bool bIsBlocking)
{
	OverlapResult->bBlockingHit = bIsBlocking;
}
