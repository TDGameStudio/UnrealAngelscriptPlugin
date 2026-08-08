#pragma once

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"

class UObject;

struct FAngelscriptSystemTimersBinds
{
	static FTimerHandle SetTimer(const UObject* Object, const FName& FunctionName, float Time, bool bLooping);
	static bool IsTimerPaused(FTimerHandle Handle);
	static void PauseTimer(FTimerHandle Handle);
	static void UnpauseTimer(FTimerHandle Handle);
	static void ClearAndInvalidateTimer(FTimerHandle& Handle);
};
