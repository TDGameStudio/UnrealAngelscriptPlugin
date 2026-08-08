#include "Bind_FPlatformApplicationMisc.h"

#include "HAL/PlatformApplicationMisc.h"

void FAngelscriptFPlatformApplicationMiscBinds::ClipboardCopy(const FString& String)
{
	FPlatformApplicationMisc::ClipboardCopy(*String);
}
