#include "Engine/LatentActionManager.h"

#include "AngelscriptBinds.h"

AS_FORCE_LINK const FAngelscriptBinds::FBind Bind_FLatentActionInfo((int32)FAngelscriptBinds::EOrder::Late + 1, []
{
	auto FLatentActionInfo_ = FAngelscriptBinds::ExistingClass("FLatentActionInfo");

	FLatentActionInfo_.Constructor("void f(int32 InLinkage, int32 InUUID, const FName InFunctionName, UObject InCallbackTarget)",
	[](FLatentActionInfo* Address, int32 InLinkage, int32 InUUID, const FName InFunctionName, UObject* InCallbackTarget)
	{
		new(Address) FLatentActionInfo();
		Address->Linkage = InLinkage;
		Address->UUID = InUUID;
		Address->ExecutionFunction = InFunctionName;
		Address->CallbackTarget = InCallbackTarget;
	});
	FAngelscriptBinds::SetPreviousBindNoDiscard(true);

	FLatentActionInfo_.Property("int32 Linkage", &FLatentActionInfo::Linkage);
	FLatentActionInfo_.Property("int32 UUID", &FLatentActionInfo::UUID);
	FLatentActionInfo_.Property("FName ExecutionFunction", &FLatentActionInfo::ExecutionFunction);
	FLatentActionInfo_.Property("UObject unresolved_object CallbackTarget", &FLatentActionInfo::CallbackTarget);
});
