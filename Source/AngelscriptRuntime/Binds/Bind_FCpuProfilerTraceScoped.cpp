#include "CoreMinimal.h"

struct FCpuProfilerTraceScoped;

struct FAngelscriptFCpuProfilerTraceScopedBinds
{
	static void Construct(FCpuProfilerTraceScoped* Address, const FName& EventId);
};

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
	EAngelscriptBindPhase::ExplicitBindings,
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

#include "FCpuProfilerTraceScoped.h"

void FAngelscriptFCpuProfilerTraceScopedBinds::Construct(FCpuProfilerTraceScoped* Address, const FName& EventId)
{
	new (Address) FCpuProfilerTraceScoped(EventId);
}
