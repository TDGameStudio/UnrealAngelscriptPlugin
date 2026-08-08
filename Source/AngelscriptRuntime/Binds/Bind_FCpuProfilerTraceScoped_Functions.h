#pragma once

#include "CoreMinimal.h"

struct FCpuProfilerTraceScoped;

struct FAngelscriptFCpuProfilerTraceScopedBinds
{
	static void Construct(FCpuProfilerTraceScoped* Address, const FName& EventId);
};
