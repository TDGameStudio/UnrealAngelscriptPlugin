#pragma once

// ============================================================================
// AngelscriptTestUtilities (umbrella header)
// ============================================================================
//
// Phase 1 of OpenSpec change `refactor-as-test-shared-layout-and-naming`
// reduced this file from a 1093-line "god header" to a ~40-line umbrella
// that simply re-exports the six themed sub-headers below.
//
// Existing consumers continue to `#include "AngelscriptTestUtilities.h"`
// (directly or transitively through `AngelscriptTestMacros.h`) without any
// callsite change — every public symbol is still in the
// `` namespace (or at top-level for
// `FAngelscriptTestEngineScopeAccess`).
//
// New code should prefer including the specific themed sub-header it needs,
// trimming compile-time fan-out and clarifying intent:
//
//   * AngelscriptTestEngineAcquisition.h
//       Engine factories, shared-engine singleton, reset/destroy/log,
//       production-like resolver, top-level `FAngelscriptTestEngineScopeAccess`.
//
//   * AngelscriptTestEngineCleanup.h
//       `CleanupDetachedASTypesForGarbageCollection` + Blueprint action
//       database invalidation (sole sub-header allowed to include
//       editor-only Blueprint headers).
//
//   * AngelscriptTestMemoryProbe.h
//       `SampleBindFreeMem` + `AcquireTransientFullTestEngine[WithProbe]`
//       for `Memory.BindFreeEvidence` diagnostics.
//
//   * AngelscriptTestModuleBuilder.h
//       `BuildModule`, `GetFunctionByDecl`, compile-diagnostic surface.
//
//   * AngelscriptTestExecute.h
//       `ExecuteIntFunction`, `ExecuteIntFunctionExpectingScriptException`,
//       `ExecuteInt64Function`. Phase 2 will fold the legacy
//       `AngelscriptGlobalFunctionInvoker.h` + `AngelscriptBindingsAssertions.h`
//       into this header and Phase 3 will introduce the new `Execute*`
//       naming family.
//
//   * AngelscriptTestFixture.h
//       `ETestEngineMode` + `FAngelscriptTestFixture` (depends on every
//       other themed sub-header — the only intra-Shared dependency edge).
//
// `Shared/AngelscriptTestEngine.h` (production engine wrapper) and
// `Misc/AutomationTest.h` (FAutomationTestBase) are re-exported here so
// the umbrella keeps full source-level compatibility with the pre-split
// header surface.
// ============================================================================

#include "AngelscriptTestEngineAcquisition.h"
#include "AngelscriptTestEngineCleanup.h"
#include "AngelscriptTestMemoryProbe.h"
#include "AngelscriptTestModuleBuilder.h"
#include "AngelscriptTestExecute.h"
#include "AngelscriptTestFixture.h"

#include "AngelscriptTestEngine.h"
#include "Misc/AutomationTest.h"
