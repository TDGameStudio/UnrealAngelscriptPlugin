#pragma once

// Legacy forward header. The original ~378-line content (namespace
// `AngelscriptTestBindings` — `ExpectGlobalInt` / `ExpectGlobalIntAtLeast` /
// `ExpectGlobalBool` / `ExpectGlobalDouble` / `ExpectGlobalInts` /
// `ExpectGlobalReturnBool` / `ExpectGlobalReturnFloat` /
// `ExpectGlobalReturnCustom<T>` / `ExpectBindingCompileFailure` /
// `ExecuteFunctionExpectingScriptException` / `AngelscriptTestTraceCase`) was merged
// into `AngelscriptTestExecute.h` during Phase 2 task 2.4 of OpenSpec change
// `refactor-as-test-shared-layout-and-naming`. The `WITH_DEV_AUTOMATION_TESTS`
// guard travels with the relocated content. This stub is retained indefinitely
// so existing call sites that already `#include` it keep compiling; new code
// should `#include "AngelscriptTestExecute.h"` directly.
#include "AngelscriptTestExecute.h"
