#include "Bind_FCpuProfilerTraceScoped.h"

#include "AngelscriptBinds.h"

/**
 * Scoped CPU profiler event.
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                              | Purpose / parameter notes                                                                                          |
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 * | FCpuProfilerTraceScoped Event(const FName& EventId);                                     | Begins a named CPU trace event and ends it automatically when Event leaves scope.                                  |
 * +------------------------------------------------------------------------------------------+--------------------------------------------------------------------------------------------------------------------+
 */

AS_FORCE_LINK const FAngelscriptBind Bind_TraceCPUProfilerEventScoped(
	TEXT("FCpuProfilerTraceScoped"),
	EAngelscriptBindPhase::ManualBindings,
	[](FAngelscriptBinds& Binds)
	{
		auto FCpuProfilerTraceScoped_ = Binds.ExistingClassForTarget("FCpuProfilerTraceScoped");
		FCpuProfilerTraceScoped_.Constructor(
			"void f(const FName& EventID)",
			&FAngelscriptFCpuProfilerTraceScopedBinds::Construct,
			"FCpuProfilerTraceScoped",
			true)
			.NoDiscard();
	});
