// Single top-level LLM tag for all Angelscript memory.
// Only placed at allocator gateways (SDKAlloc, BindScriptTypes, ClassGenerator, CompileModules).
// All AS-internal allocations flow through SDKAlloc/asAllocMem, so one tag at
// the gateway covers the entire SDK footprint.

#pragma once

#include "HAL/LowLevelMemTracker.h"

LLM_DECLARE_TAG_API(Angelscript, ANGELSCRIPTRUNTIME_API);
