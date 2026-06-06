# AngelscriptTest/Shared/ Layout Guide

This directory holds the test-side facade and supporting helpers used by the entire `AngelscriptTest` module. It was reorganised by OpenSpec change `refactor-as-test-shared-layout-and-naming` (Phases 1–5, May 2026) — most notably the 1093-line `AngelscriptTestUtilities.h` "god header" was split into six themed sub-headers behind a thin umbrella, AS execution was consolidated into `AngelscriptTestExecute.h`, and `FBindingsCoverageProfile` was removed (Phase 5).

## Recommended entry points

| Goal | Include |
|---|---|
| New code that drives an `asIScriptFunction` from C++ | `AngelscriptTestExecute.h` (canonical) |
| Build an in-memory `.as` module + look up a function | `AngelscriptTestModuleBuilder.h` |

`AngelscriptTest.Build.cs` adds `Shared/` to `PrivateIncludePaths`, so use `#include "AngelscriptTestExecute.h"` — not `#include "Shared/AngelscriptTestExecute.h"`. Bindings-local helpers remain `#include "Bindings/..."`.
| Acquire / reset / log / destroy the shared test engine | `AngelscriptTestEngineAcquisition.h` |
| Sweep detached `UASClass` / `UASStruct` before GC | `AngelscriptTestEngineCleanup.h` |
| `Memory.BindFreeEvidence`-style probes | `AngelscriptTestMemoryProbe.h` |
| RAII fixture wrapping engine + scope + builder + executor | `AngelscriptTestFixture.h` |
| Stay with the umbrella (no breakage) | `AngelscriptTestUtilities.h` |

## Header inventory

### Umbrella (post-split)

| Header | Lines | Role |
|---|---|---|
| `AngelscriptTestUtilities.h` | ~60 | Re-exports the six themed sub-headers + `AngelscriptTestEngine.h` + `Misc/AutomationTest.h`. Kept indefinitely for backward source-level compatibility — 305 consumer TUs (directly or via `AngelscriptTestMacros.h`) need no change. |

### Six themed sub-headers (new in Phase 1)

| Header | Lines | Carries |
|---|---|---|
| `AngelscriptTestEngineAcquisition.h` | ~490 | Engine factories, shared-engine singleton, reset / debug-log / destroy, production-like resolver, top-level `FAngelscriptTestEngineScopeAccess`. |
| `AngelscriptTestEngineCleanup.h` | ~195 | `FDetachedASTypeCleanupResult` + `CleanupDetachedASTypesForGarbageCollection`. **Sole** sub-header that includes editor-only Blueprint headers (`BlueprintActionDatabase.h`, `K2Node_GetSubsystem.h`, `Subsystems/Subsystem.h`). |
| `AngelscriptTestMemoryProbe.h` | ~110 | `SampleBindFreeMem`, `AcquireTransientFullTestEngine` (the real definition), `AcquireTransientFullTestEngineWithProbe`. Depends on Acquisition + Cleanup. |
| `AngelscriptTestModuleBuilder.h` | ~220 | `ReportCompileDiagnostics`, `FScopedAutomaticImportsOverride`, `BuildModule`, `GetFunctionByDecl`. |
| `AngelscriptTestExecute.h` | ~1280 | **Canonical AS execution entry** (global API, no namespace): `FAngelscriptTestExecutor`, `Execute*` / `Compile*` families, `ResolveFunction*`, legacy `ExpectGlobal*` / `FASGlobalFunctionInvoker` aliases, and `ExecuteIntFunction*` trio. Does **not** depend on Bindings profile types (Phase 5). |
| `AngelscriptTestFixture.h` | ~120 | `ETestEngineMode` + `FAngelscriptTestFixture`. The **only** Shared/* header that depends on the other five themed sub-headers. |

### Bindings cluster (sibling headers)

| Header | Lines | Role |
|---|---|---|
| `AngelscriptGlobalFunctionInvoker.h` | ~10 | Permanent forward shim → `AngelscriptTestExecute.h`. |
| `AngelscriptBindingsAssertions.h` | ~14 | Permanent forward shim → `AngelscriptTestExecute.h`. |
| `AngelscriptTestModuleScope.h` | ~100 | `FScopedAngelscriptModule` — AS module lifecycle RAII; takes explicit `ModuleName` + `Source`. |
| `AngelscriptBindingsModuleBuilder.h` | ~10 | Forward shim → `AngelscriptTestModuleScope.h`. |
| `AngelscriptBindingsExampleSection.h` | ~90 | Official example: global `Execute*` + explicit module names. |

**Deleted in Phase 5:** `AngelscriptBindingsCoverage.h` (`FBindingsCoverageProfile`, `FormatCaseLabel`, `MakeCoverageModuleName`). Bindings tests pass full-word module names directly to `FScopedAngelscriptModule` and plain case labels to `Execute*` / legacy `ExpectGlobal*`.

### Other Shared/* headers (no change in this OpenSpec change)

`AngelscriptCollisionTestHelpers.h`, `AngelscriptConstructionContextProbe.h`, `AngelscriptDebuggerScriptFixture.h`, `AngelscriptDebuggerTestClient.h`, `AngelscriptDebuggerTestContext.h`, `AngelscriptDebuggerTestHelpers.h`, `AngelscriptDebuggerTestMonitor.h`, `AngelscriptDebuggerTestSession.h`, `AngelscriptFunctionalTestUtils.h`, `AngelscriptMockDebugServer.h`, `AngelscriptNativeInterfaceTestHelpers.h`, `AngelscriptNativeInterfaceTestTypes.h`, `AngelscriptNativeScriptTestObject.h`, `AngelscriptPerformanceTestUtils.h`, `AngelscriptReflectiveAccess.h`, `AngelscriptTestEngine.h`, `AngelscriptTestEngineHelper.h`, `AngelscriptTestEnginePool.h`, `AngelscriptTestLegacyHelpers.h`, `AngelscriptTestMacros.h`, `AngelscriptTestWorld.h`.

## Legacy alias retirement (Phase 1 task 1.8)

The following four pure-forward wrappers were removed from `AngelscriptTestSupport::`. All 19 prior call sites inside `AngelscriptTest/` were migrated to the canonical entry points:

| Removed alias | Canonical replacement |
|---|---|
| `GetSharedTestEngine` | `GetOrCreateSharedCloneEngine` |
| `GetResetSharedTestEngine` | `AcquireCleanSharedCloneEngine` |
| `ResetSharedInitializedTestEngine` | `ResetSharedCloneEngine` |
| `AcquireFreshSharedCloneEngine` | `DestroySharedAndStrayGlobalTestEngine()` + `AcquireCleanSharedCloneEngine()` (explicit pair) |

`AngelscriptTestEngineHelperGetSharedTestEngineAliasesSharedCloneTest` (a pure alias-existence regression) was deleted because `SharedEngineNeverAttachesToProduction` already covers the same idempotency invariant. `GetResetSharedTestEngineResetsSharedState` was renamed to `AcquireCleanSharedCloneEngineResetsModules` and its `AngelscriptTestSupport::GetResetSharedTestEngine()` callsite swapped for `AcquireCleanSharedCloneEngine()`; the reset-then-discard-modules invariant remains under test.

## Bindings `Execute*` migration (completed 2026-05-25)

The 11 `Bindings/*.cpp` files that previously duplicated `Execute*` helpers in `AngelscriptTest_*_Private` now use global `FAngelscriptTestExecutor` / `ExecuteAndExpect*` from `AngelscriptTestExecute.h`. Bindings-local scaffolding lives in module headers (not Shared):

| Header | Role |
|---|---|
| `Bindings/AngelscriptMathBindingsTestCompare.h` | Math tolerance / `VerifyMathBindings*` / reference rotators |
| `Bindings/AngelscriptWorldCollisionBindingsTestHelpers.h` | `WorldCollisionExecute*` / `WorldCollisionSetArg*` |
| `Bindings/AngelscriptTArrayBindingsTestHelpers.h` | `TArrayBindings*` / `ExpectTArrayBindings*` (`TArrayBindings` prefix avoids Unity ODR clash with global `ExpectGlobalInt`) |

**Reference test shape:** `Bindings/AngelscriptQuatBindingsTests.cpp` (`FScopedAngelscriptModule` + global `ExpectGlobal*`).

**Optional follow-ups (not blocking):** P3-only Private in ReflectiveFallback / TextFormatting / MathAndPlatform; Syntax theme still on forward shims; full-module `AngelscriptTest_*_Private` convergence outside Bindings.

## Scope guards

* **Only `AngelscriptTestEngineCleanup.h`** may include `BlueprintActionDatabase.h`, `K2Node_GetSubsystem.h`, or `Subsystems/Subsystem.h`. Other Shared/* headers must stay editor-include-free; consumers that need cleanup from a runtime context branch on `WITH_EDITOR` themselves.
* `AngelscriptTestFixture.h` is the only Shared/* header allowed to depend on its sibling themed sub-headers; the other five sub-headers are linearly layered (Acquisition / Cleanup → MemoryProbe; Acquisition / Builder / Execute consumed by Fixture).
* The umbrella `AngelscriptTestUtilities.h` MUST NOT regrow inline implementations — it is exclusively a re-export shim.

## See also

* OpenSpec change: `openspec/changes/refactor-as-test-shared-layout-and-naming/`
* Test conventions: `Documents/Guides/TestConventions.md`
* Test catalogue: `Documents/Guides/TestCatalog.md`
