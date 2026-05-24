#pragma once

// Legacy forward header. The original ~408-line content (namespace
// `AngelscriptReflectiveAccess` — `ResolveFunctionByDecl` / `ResolveFunctionByName`
// / `FASGlobalFunctionInvoker`) was merged into `AngelscriptTestExecute.h`
// during Phase 2 task 2.3 of OpenSpec change
// `refactor-as-test-shared-layout-and-naming`. This stub is retained
// indefinitely so existing call sites that already `#include` it keep
// compiling; new code should `#include "AngelscriptTestExecute.h"` directly.
#include "AngelscriptTestExecute.h"
