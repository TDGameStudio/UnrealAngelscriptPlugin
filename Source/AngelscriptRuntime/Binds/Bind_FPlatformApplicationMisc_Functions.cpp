#include "Bind_FPlatformApplicationMisc_Functions.h"

#include "HAL/PlatformApplicationMisc.h"

void FAngelscriptFPlatformApplicationMiscBinds::ClipboardCopy(const FString& String)
{
	FPlatformApplicationMisc::ClipboardCopy(*String);
}
