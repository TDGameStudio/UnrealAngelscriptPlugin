#include "AngelscriptBinds.h"

#include "Engine/LatentActionManager.h"

#include "Bind_FLatentActionInfo_Functions.h"

namespace
{
	void BindFLatentActionInfo(FAngelscriptBinds& Binds)
	{
		auto FLatentActionInfo_ = Binds.ExistingClassForTarget("FLatentActionInfo");
		FLatentActionInfo_.Constructor(
			"void f(int32 InLinkage, int32 InUUID, const FName InFunctionName, UObject InCallbackTarget)",
			&FAngelscriptFLatentActionInfoBinds::Construct)
			.NoDiscard();

		FLatentActionInfo_.Property("int32 Linkage", &FLatentActionInfo::Linkage);
		FLatentActionInfo_.Property("int32 UUID", &FLatentActionInfo::UUID);
		FLatentActionInfo_.Property("FName ExecutionFunction", &FLatentActionInfo::ExecutionFunction);
		FLatentActionInfo_.Property("UObject unresolved_object CallbackTarget", &FLatentActionInfo::CallbackTarget);
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_FLatentActionInfo(
	TEXT("FLatentActionInfo"),
	EAngelscriptBindPhase::ManualBindings,
	&BindFLatentActionInfo);
