#include "AngelscriptBinds.h"

#include "Bind_FCpuProfilerTraceScoped_Functions.h"

namespace
{
	void BindFCpuProfilerTraceScoped(FAngelscriptBinds& Binds)
	{
		auto FCpuProfilerTraceScoped_ = Binds.ExistingClassForTarget("FCpuProfilerTraceScoped");
		FCpuProfilerTraceScoped_.Constructor(
			"void f(const FName& EventID)",
			&FAngelscriptFCpuProfilerTraceScopedBinds::Construct,
			"FCpuProfilerTraceScoped",
			true)
			.NoDiscard();
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_TraceCPUProfilerEventScoped(
	TEXT("FCpuProfilerTraceScoped"),
	EAngelscriptBindPhase::ManualBindings,
	&BindFCpuProfilerTraceScoped);
