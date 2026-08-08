#include "AngelscriptBinds.h"

#include "Engine/OverlapResult.h"

#include "Bind_FOverlapResult_Functions.h"

namespace
{
	void BindFOverlapResult(FAngelscriptBinds& Binds)
	{
		auto FOverlapResult_ = Binds.ExistingClassForTarget("FOverlapResult");
		FOverlapResult_.Property("int ItemIndex", &FOverlapResult::ItemIndex);
		FOverlapResult_.Method("void SetComponent(UPrimitiveComponent InComp)", &FAngelscriptFOverlapResultBinds::SetComponent);
		FOverlapResult_.Method("UPrimitiveComponent GetComponent() const", &FAngelscriptFOverlapResultBinds::GetComponent);
		FOverlapResult_.Method("void SetActor(AActor InActor)", &FAngelscriptFOverlapResultBinds::SetActor);
		FOverlapResult_.Method("AActor GetActor() const", &FAngelscriptFOverlapResultBinds::GetActor);
		FOverlapResult_.Method("bool GetbBlockingHit() const", &FAngelscriptFOverlapResultBinds::GetBlockingHit);
		FOverlapResult_.Method("void SetBlockingHit(bool bIsBlocking)", &FAngelscriptFOverlapResultBinds::SetBlockingHit);
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_FOverlapResult(
	TEXT("FOverlapResult"),
	EAngelscriptBindPhase::ManualBindings,
	&BindFOverlapResult);
