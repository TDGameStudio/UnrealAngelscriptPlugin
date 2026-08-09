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

#include "AngelscriptBinds.h"

/**
 * System timer-handle binding surface.
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                | Purpose / parameter notes                                                                                            |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | FTimerHandle System::SetTimer(const UObject Object, const FName& FunctionName,             | Schedules a reflected object function on the object world timer manager.                                             |
 * |     float32 Time, bool bLooping = false);                                                  | @param FunctionName Parameterless reflected function invoked by the timer.                                           |
 * |                                                                                            | @param Time Interval in seconds. @param bLooping Repeats at Time intervals when true.                                |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | bool System::IsTimerPausedHandle(FTimerHandle Handle);                                     | Returns whether Handle identifies a paused timer in the current world.                                               |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void System::PauseTimerHandle(FTimerHandle Handle);                                        | Pauses the timer identified by Handle in the current world.                                                          |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void System::UnPauseTimerHandle(FTimerHandle Handle);                                      | Resumes the timer identified by Handle in the current world.                                                         |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 * | void System::ClearAndInvalidateTimerHandle(FTimerHandle& Handle);                          | Clears the current-world timer and invalidates the caller handle.                                                    |
 * |                                                                                            | @param Handle Timer handle mutated to the invalid state.                                                             |
 * +--------------------------------------------------------------------------------------------+----------------------------------------------------------------------------------------------------------------------+
 */

AS_FORCE_LINK const FAngelscriptBind Bind_SystemTimers(
	TEXT("SystemTimers"),
	EAngelscriptBindPhase::ExplicitBindings,
	[](FAngelscriptBinds& Binds)
	{
		FAngelscriptBinds::FNamespace Namespace(Binds.GetTargetEngine(), "System");
		Binds.BindGlobalFunctionForTarget(
			"FTimerHandle SetTimer(const UObject Object, const FName& FunctionName, float32 Time, bool bLooping = false)",
			&FAngelscriptSystemTimersBinds::SetTimer);
		Binds.BindGlobalFunctionForTarget("bool IsTimerPausedHandle(FTimerHandle Handle)", &FAngelscriptSystemTimersBinds::IsTimerPaused)
			.WorldContext();
		Binds.BindGlobalFunctionForTarget("void PauseTimerHandle(FTimerHandle Handle)", &FAngelscriptSystemTimersBinds::PauseTimer)
			.WorldContext();
		Binds.BindGlobalFunctionForTarget("void UnPauseTimerHandle(FTimerHandle Handle)", &FAngelscriptSystemTimersBinds::UnpauseTimer)
			.WorldContext();
		Binds.BindGlobalFunctionForTarget("void ClearAndInvalidateTimerHandle(FTimerHandle& Handle)", &FAngelscriptSystemTimersBinds::ClearAndInvalidateTimer)
			.WorldContext();
	});

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
