#include "AngelscriptBinds.h"

#include "HAL/PlatformApplicationMisc.h"

#include "Bind_FPlatformApplicationMisc_Functions.h"

namespace
{
	void BindFPlatformApplicationMisc(FAngelscriptBinds& Binds)
	{
		FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "FPlatformApplicationMisc");
		Binds.BindGlobalFunctionForTarget("void ClipboardCopy(const FString& Str)", &FAngelscriptFPlatformApplicationMiscBinds::ClipboardCopy);
		Binds.BindGlobalFunctionForTarget("void ClipboardPaste(FString&\tDest)", &FPlatformApplicationMisc::ClipboardPaste)
			.NativeFunction("FPlatformApplicationMisc::ClipboardPaste", true);
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_FPlatformApplicationMisc(
	TEXT("FPlatformApplicationMisc"),
	EAngelscriptBindPhase::ManualBindings,
	&BindFPlatformApplicationMisc);
