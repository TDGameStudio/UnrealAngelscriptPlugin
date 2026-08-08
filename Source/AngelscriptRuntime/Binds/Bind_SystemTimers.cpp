#include "AngelscriptBinds.h"

#include "Bind_SystemTimers_Functions.h"

namespace
{
	void BindSystemTimers(FAngelscriptBinds& Binds)
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
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_SystemTimers(
	TEXT("SystemTimers"),
	EAngelscriptBindPhase::ManualBindings,
	&BindSystemTimers);
