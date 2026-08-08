#include "Bind_SystemTimers.h"

#include "AngelscriptEngine.h"
#include "Kismet/KismetSystemLibrary.h"

FTimerHandle FAngelscriptSystemTimersBinds::SetTimer(
	const UObject* Object,
	const FName& FunctionName,
	float Time,
	bool bLooping)
{
	return UKismetSystemLibrary::K2_SetTimer(
		const_cast<UObject*>(Object),
		FunctionName.ToString(),
		Time,
		bLooping,
		false,
		0.f,
		0.f);
}

bool FAngelscriptSystemTimersBinds::IsTimerPaused(FTimerHandle Handle)
{
	return UKismetSystemLibrary::K2_IsTimerPausedHandle(FAngelscriptEngine::TryGetCurrentWorldContextObject(), Handle);
}

void FAngelscriptSystemTimersBinds::PauseTimer(FTimerHandle Handle)
{
	UKismetSystemLibrary::K2_PauseTimerHandle(FAngelscriptEngine::TryGetCurrentWorldContextObject(), Handle);
}

void FAngelscriptSystemTimersBinds::UnpauseTimer(FTimerHandle Handle)
{
	UKismetSystemLibrary::K2_UnPauseTimerHandle(FAngelscriptEngine::TryGetCurrentWorldContextObject(), Handle);
}

void FAngelscriptSystemTimersBinds::ClearAndInvalidateTimer(FTimerHandle& Handle)
{
	UKismetSystemLibrary::K2_ClearAndInvalidateTimerHandle(FAngelscriptEngine::TryGetCurrentWorldContextObject(), Handle);
}
