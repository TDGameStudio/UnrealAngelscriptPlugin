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

#include "AngelscriptBinds.h"

#include "Engine/OverlapResult.h"

/**
 * FOverlapResult manual binding surface.
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                | Purpose / parameter notes                                                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | int FOverlapResult.ItemIndex;                                                              | Exposes the overlapped item or body index supplied by the collision query.                                           |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FOverlapResult.SetComponent(UPrimitiveComponent InComp);                              | Stores the overlapped primitive component as a weak object reference.                                                |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | UPrimitiveComponent FOverlapResult.GetComponent() const;                                   | Returns the overlapped primitive component while it remains valid.                                                   |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FOverlapResult.SetActor(AActor InActor);                                              | Stores the overlapped actor through the actor-instance handle.                                                       |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | AActor FOverlapResult.GetActor() const;                                                    | Returns the overlapped actor while it remains resolvable.                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool FOverlapResult.GetbBlockingHit() const;                                               | Returns whether this overlap is classified as blocking.                                                              |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FOverlapResult.SetBlockingHit(bool bIsBlocking);                                      | Sets whether this overlap is classified as blocking.                                                                 |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 */

AS_FORCE_LINK const FAngelscriptBind Bind_FOverlapResult(
	TEXT("FOverlapResult"),
	EAngelscriptBindPhase::ExplicitBindings,
	[](FAngelscriptBinds& Binds)
	{
		auto FOverlapResult_ = Binds.ExistingClassForTarget("FOverlapResult");
		FOverlapResult_.Property("int ItemIndex", &FOverlapResult::ItemIndex);
		FOverlapResult_.Method("void SetComponent(UPrimitiveComponent InComp)", &FAngelscriptFOverlapResultBinds::SetComponent);
		FOverlapResult_.Method("UPrimitiveComponent GetComponent() const", &FAngelscriptFOverlapResultBinds::GetComponent);
		FOverlapResult_.Method("void SetActor(AActor InActor)", &FAngelscriptFOverlapResultBinds::SetActor);
		FOverlapResult_.Method("AActor GetActor() const", &FAngelscriptFOverlapResultBinds::GetActor);
		FOverlapResult_.Method("bool GetbBlockingHit() const", &FAngelscriptFOverlapResultBinds::GetBlockingHit);
		FOverlapResult_.Method("void SetBlockingHit(bool bIsBlocking)", &FAngelscriptFOverlapResultBinds::SetBlockingHit);
	});

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
