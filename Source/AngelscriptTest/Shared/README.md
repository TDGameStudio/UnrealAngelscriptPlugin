# AngelscriptTest/Shared/ Layout Guide

This directory holds the test-side facade and supporting helpers used by the entire `AngelscriptTest` module. It was reorganised by OpenSpec change `refactor-as-test-shared-layout-and-naming` (Phase 1, May 2026) — most notably the 1093-line `AngelscriptTestUtilities.h` "god header" was split into six themed sub-headers behind a thin umbrella.

## Recommended entry points

| Goal | Include |
|---|---|
| New code that drives an `asIScriptFunction` from C++ | `AngelscriptTestExecute.h` (canonical) |
| Build an in-memory `.as` module + look up a function | `AngelscriptTestModuleBuilder.h` |
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
| `AngelscriptTestExecute.h` | ~175 | Currently: `ExecuteIntFunction[ExpectingScriptException]`, `ExecuteInt64Function`. Phase 2 will fold `AngelscriptGlobalFunctionInvoker.h` + `AngelscriptBindingsAssertions.h` into here; Phase 3 will introduce `FAngelscriptTestExecutor` + the `Execute*` naming family (`ExecuteAndExpect*`, `ExecuteAndExpectNear*`, `ExecuteBatchAndExpect*`, `ExecuteAndValidate<T>`, `CompileAndExpectFailure`) with all legacy names kept as permanent inline aliases. |
| `AngelscriptTestFixture.h` | ~120 | `ETestEngineMode` + `FAngelscriptTestFixture`. The **only** Shared/* header that depends on the other five themed sub-headers. |

### Bindings cluster (sibling headers, not part of the Phase 1 split)

These predate the split and remain unchanged. Phase 2/3 will fold the first two into `AngelscriptTestExecute.h` and reduce them to ~3-line forward shims (kept indefinitely for source-level compatibility).

| Header | Lines | Role | Phase 2/3 plan |
|---|---|---|---|
| `AngelscriptGlobalFunctionInvoker.h` | ~440 | `AngelscriptReflectiveAccess::ResolveFunctionByDecl/Name`, `FASGlobalFunctionInvoker`. | Body folded into `AngelscriptTestExecute.h`; this file becomes a forward shim. `FASGlobalFunctionInvoker` becomes a `using`-alias of the new `FAngelscriptTestExecutor`. |
| `AngelscriptBindingsAssertions.h` | ~410 | `AngelscriptTestBindings::ExpectGlobal*` family + `ExpectBindingCompileFailure` + `Detail::TraceCase`. | Body folded into `AngelscriptTestExecute.h`; this file becomes a forward shim. All public function names retained as inline forwards. |
| `AngelscriptBindingsCoverage.h` | ~115 | `FBindingsCoverageProfile` + `FCoverageModuleScope`. | Phase 5 adds full-word field aliases (`BodyInstance` for `BodyInst`, etc.) alongside the existing abbreviated names. |
| `AngelscriptBindingsModuleBuilder.h` | ~100 | Coverage-specific module compile + discard helper. | Unchanged. |
| `AngelscriptBindingsExampleSection.h` | ~90 | Canonical 80-line example using the bindings stack. | Phase 4 rewrites the example using the new `Execute*` API surface. |

### Other Shared/* headers (no change in this OpenSpec change)

`AngelscriptCollisionTestHelpers.h`, `AngelscriptConstructionContextProbe.h`, `AngelscriptDebuggerScriptFixture.h`, `AngelscriptDebuggerTestClient.h`, `AngelscriptDebuggerTestContext.h`, `AngelscriptDebuggerTestHelpers.h`, `AngelscriptDebuggerTestMonitor.h`, `AngelscriptDebuggerTestSession.h`, `AngelscriptFunctionalTestUtils.h`, `AngelscriptLearningTrace.h`, `AngelscriptMockDebugServer.h`, `AngelscriptNativeInterfaceTestHelpers.h`, `AngelscriptNativeInterfaceTestTypes.h`, `AngelscriptNativeScriptTestObject.h`, `AngelscriptPerformanceTestUtils.h`, `AngelscriptReflectiveAccess.h`, `AngelscriptTestEngine.h`, `AngelscriptTestEngineHelper.h`, `AngelscriptTestEnginePool.h`, `AngelscriptTestLegacyHelpers.h`, `AngelscriptTestMacros.h`, `AngelscriptTestWorld.h`.

## Legacy alias retirement (Phase 1 task 1.8)

The following four pure-forward wrappers were removed from `AngelscriptTestSupport::`. All 19 prior call sites inside `AngelscriptTest/` were migrated to the canonical entry points:

| Removed alias | Canonical replacement |
|---|---|
| `GetSharedTestEngine` | `GetOrCreateSharedCloneEngine` |
| `GetResetSharedTestEngine` | `AcquireCleanSharedCloneEngine` |
| `ResetSharedInitializedTestEngine` | `ResetSharedCloneEngine` |
| `AcquireFreshSharedCloneEngine` | `DestroySharedAndStrayGlobalTestEngine()` + `AcquireCleanSharedCloneEngine()` (explicit pair) |

`AngelscriptTestEngineHelperGetSharedTestEngineAliasesSharedCloneTest` (a pure alias-existence regression) was deleted because `SharedEngineNeverAttachesToProduction` already covers the same idempotency invariant. `GetResetSharedTestEngineResetsSharedState` was renamed to `AcquireCleanSharedCloneEngineResetsModules` and its `AngelscriptTestSupport::GetResetSharedTestEngine()` callsite swapped for `AcquireCleanSharedCloneEngine()`; the reset-then-discard-modules invariant remains under test.

## Scattered private `Execute*Function*` helpers (deferred to follow-ups)

Phase 1 left the following 14 private helpers in 11 `Bindings/*.cpp` files in place, each carrying a `// TODO(refactor-as-test-shared-layout-and-naming)` marker at the definition. They will be migrated to `AngelscriptTestExecute.h` as part of a dedicated follow-up change once the Phase 3 `Execute*` naming family lands.

| File | Helper(s) |
|---|---|
| `AngelscriptAssetRegistryBindingsTests.cpp` | `ExecuteFunctionExpectingException` |
| `AngelscriptCollisionParamsBindingsTests.cpp` | `ExecuteIntFunction` |
| `AngelscriptCurveFunctionLibraryTests.cpp` | `ExecuteIntFunctionWithAddressArg` |
| `AngelscriptJsonBindingsTests.cpp` | `ExecuteFunctionExpectingException` |
| `AngelscriptMathBindingsTests.cpp` | `ExecuteValueFunction<T>` |
| `AngelscriptMathOrientationBindingsTests.cpp` | `ExecuteValueFunction<T>` |
| `AngelscriptScriptFunctionLibraryTests.cpp` | `ExecuteValueFunction<T>` |
| `AngelscriptWorldFunctionLibraryTests.cpp` | `ExecuteIntFunction`, `ExecuteBoolFunction`, `ExecuteFunctionExpectingException` |
| `AngelscriptWorldCollisionFunctionLibraryTraceTests.cpp` | `ExecuteBoolFunction`, `ExecuteAddressBoolFunction<T>` |
| `AngelscriptWorldCollisionFunctionLibraryComponentTests.cpp` | `ExecuteBoolFunction` |
| `AngelscriptWorldCollisionBindingsTests.cpp` | `ExecuteBoolFunction` |

## Scope guards

* **Only `AngelscriptTestEngineCleanup.h`** may include `BlueprintActionDatabase.h`, `K2Node_GetSubsystem.h`, or `Subsystems/Subsystem.h`. Other Shared/* headers must stay editor-include-free; consumers that need cleanup from a runtime context branch on `WITH_EDITOR` themselves.
* `AngelscriptTestFixture.h` is the only Shared/* header allowed to depend on its sibling themed sub-headers; the other five sub-headers are linearly layered (Acquisition / Cleanup → MemoryProbe; Acquisition / Builder / Execute consumed by Fixture).
* The umbrella `AngelscriptTestUtilities.h` MUST NOT regrow inline implementations — it is exclusively a re-export shim.

## See also

* OpenSpec change: `openspec/changes/refactor-as-test-shared-layout-and-naming/`
* Test conventions: `Documents/Guides/TestConventions.md`
* Test catalogue: `Documents/Guides/TestCatalog.md`
