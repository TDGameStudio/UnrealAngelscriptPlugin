#pragma once

class AActor;
class UPrimitiveComponent;
struct FOverlapResult;

struct FAngelscriptFOverlapResultBinds
{
	static void SetComponent(FOverlapResult* OverlapResult, UPrimitiveComponent* Component);
	static UPrimitiveComponent* GetComponent(FOverlapResult* OverlapResult);
	static void SetActor(FOverlapResult* OverlapResult, AActor* Actor);
	static AActor* GetActor(FOverlapResult* OverlapResult);
	static bool GetBlockingHit(FOverlapResult* OverlapResult);
	static void SetBlockingHit(FOverlapResult* OverlapResult, bool bIsBlocking);
};
