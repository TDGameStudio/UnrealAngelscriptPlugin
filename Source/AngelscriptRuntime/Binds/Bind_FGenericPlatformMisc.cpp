#include "AngelscriptEngine.h"
#include "AngelscriptType.h"
#include "AngelscriptBinds.h"

namespace
{
	void BindFGenericPlatformMisc(FAngelscriptBinds& Binds)
	{
		FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "FGenericPlatformMisc");
		Binds.BindGlobalFunctionForTarget("void RequestExit(bool Force)", &FGenericPlatformMisc::RequestExit)
			.NativeFunction("FGenericPlatformMisc::RequestExit", true);
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_FGenericPlatformMisc(
	TEXT("FGenericPlatformMisc"),
	EAngelscriptBindPhase::ManualBindings,
	&BindFGenericPlatformMisc);
