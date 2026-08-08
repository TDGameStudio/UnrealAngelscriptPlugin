#include "Bind_FLatentActionInfo.h"

#include "AngelscriptBinds.h"

#include "Engine/LatentActionManager.h"

/**
 * FLatentActionInfo construction and fields.
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                          | Purpose / parameter notes                                                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FLatentActionInfo Info(int32 InLinkage,                                                              | Constructs latent callback routing information.                                                                  |
 * |     int32 InUUID,                                                                                    | @param InLinkage Continuation link selected when the latent action completes.                                    |
 * |     const FName InFunctionName,                                                                      | @param InUUID Identifier used to find or replace the latent action.                                              |
 * |     UObject InCallbackTarget);                                                                       | @param InFunctionName Callback function invoked on completion.                                                   |
 * |                                                                                                      | @param InCallbackTarget Object that receives the callback.                                                       |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | int32 Info.Linkage;                                                                                  | Exposes the latent continuation linkage.                                                                         |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | int32 Info.UUID;                                                                                     | Exposes the latent action identifier.                                                                            |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FName Info.ExecutionFunction;                                                                        | Exposes the callback function name.                                                                              |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | UObject unresolved_object Info.CallbackTarget;                                                       | Exposes the unresolved callback target.                                                                          |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 */

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
