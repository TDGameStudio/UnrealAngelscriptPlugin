#include "Bind_FCpuProfilerTraceScoped.h"

#include "FCpuProfilerTraceScoped.h"

void FAngelscriptFCpuProfilerTraceScopedBinds::Construct(FCpuProfilerTraceScoped* Address, const FName& EventId)
{
	new (Address) FCpuProfilerTraceScoped(EventId);
}
