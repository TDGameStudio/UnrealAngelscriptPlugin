#include "CoreMinimal.h"

struct FAngelscriptFPlatformApplicationMiscBinds
{
	static void ClipboardCopy(const FString& String);
};

#include "AngelscriptBinds.h"

#include "HAL/PlatformApplicationMisc.h"

/**
 * FPlatformApplicationMisc namespace binding surface.
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                | Purpose / parameter notes                                                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FPlatformApplicationMisc::ClipboardCopy(const FString& Str);                          | Copies Str to the platform clipboard.                                                                                |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void FPlatformApplicationMisc::ClipboardPaste(FString& Dest);                              | Reads platform clipboard text.                                                                                       |
 * |                                                                                            | @param Dest Receives the clipboard contents.                                                                         |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 */

AS_FORCE_LINK const FAngelscriptBind Bind_FPlatformApplicationMisc(
	TEXT("FPlatformApplicationMisc"),
	EAngelscriptBindPhase::ExplicitBindings,
	[](FAngelscriptBinds& Binds)
	{
		FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "FPlatformApplicationMisc");
		Binds.BindGlobalFunctionForTarget("void ClipboardCopy(const FString& Str)", &FAngelscriptFPlatformApplicationMiscBinds::ClipboardCopy);
		Binds.BindGlobalFunctionForTarget("void ClipboardPaste(FString&\tDest)", &FPlatformApplicationMisc::ClipboardPaste)
			.NativeFunction("FPlatformApplicationMisc::ClipboardPaste", true);
	});

#include "HAL/PlatformApplicationMisc.h"

void FAngelscriptFPlatformApplicationMiscBinds::ClipboardCopy(const FString& String)
{
	FPlatformApplicationMisc::ClipboardCopy(*String);
}
