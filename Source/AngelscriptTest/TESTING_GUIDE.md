# Angelscript Test Guide

## Macro Quick Reference

All macros defined in `Shared/AngelscriptTestMacros.h`:

| Macro | Returns | Usage |
|-------|---------|-------|
| `ASTEST_CREATE_ENGINE()` | `FAngelscriptEngine&` | Shared engine, reset to clean state. Use in `BEFORE_ALL()`. |
| `ASTEST_GET_ENGINE()` | `FAngelscriptEngine&` | Shared engine, no reset. Use in `TEST_METHOD()`. |
| `ASTEST_CREATE_ENGINE_FULL()` | `FAngelscriptEngine&` | Fresh isolated engine. Use for hot-reload, bind environment tests. |
| `ASTEST_CREATE_ENGINE_NATIVE()` | `asIScriptEngine*` | Raw AngelScript SDK engine. Use for SDK API tests. |
| `ASTEST_RESET_ENGINE(Engine)` | void | Reset shared engine. Use in `AFTER_ALL()`. |

## CQTest Standard Pattern (Recommended)

All new tests should use CQTest (`TEST_CLASS_WITH_FLAGS`):

```cpp
#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestExecute.h"

TEST_CLASS_WITH_FLAGS(FMyTest,
    "Angelscript.TestModule.Category.Feature",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
    BEFORE_ALL()
    {
        ASTEST_CREATE_ENGINE();  // one-time clean acquisition
    }

    AFTER_ALL()
    {
        FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
        ASTEST_RESET_ENGINE(Engine);
    }

    TEST_METHOD(BasicCase)
    {
        FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
        FAngelscriptEngineScope Scope(Engine);

        FScopedAngelscriptModule Mod(*TestRunner, Engine,
            TEXT("ASCategoryFeature_Basic"), TEXT(R"(
int GetValue() { return 42; }
)"));
        if (!Mod.IsValid()) return;
        auto& M = Mod.GetModule();

        ExpectGlobalInt(*TestRunner, Engine, M,
            TEXT("int GetValue()"), TEXT("Returns 42"), 42);
    }
};
```

Key points:
- `BEFORE_ALL`: call `ASTEST_CREATE_ENGINE()` once (resets shared engine)
- `TEST_METHOD`: call `ASTEST_GET_ENGINE()` (no reset, fast)
- `AFTER_ALL`: call `ASTEST_RESET_ENGINE()` to leave clean state
- `FScopedAngelscriptModule`: RAII module isolation per test method
- Pass `*TestRunner` (not `*this`) to assertion helpers

## Full Engine Pattern

For tests needing complete isolation (hot-reload, bind environment, GC):

```cpp
TEST_METHOD(IsolatedTest)
{
    FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
    FAngelscriptEngineScope Scope(Engine);
    ON_SCOPE_EXIT
    {
        for (const auto& Module : Engine.GetActiveModules())
            Engine.DiscardModule(*Module->ModuleName);
    };

    // ... test code ...
}
```

## Native SDK Pattern

For testing AngelScript SDK APIs directly:

```cpp
TEST_METHOD(SDKTest)
{
    asIScriptEngine* NativeEngine = ASTEST_CREATE_ENGINE_NATIVE();
    if (NativeEngine == nullptr) { TestRunner->AddError(TEXT("Failed")); return; }
    ON_SCOPE_EXIT { NativeEngine->ShutDownAndRelease(); };

    // For internal SDK classes (asCBuilder, asCParser):
    asCScriptEngine* BareEngine = reinterpret_cast<asCScriptEngine*>(NativeEngine);
    // ... test code ...
}
```

## Decision Tree

```
Testing engine core / hot-reload / bind environment?
  --> ASTEST_CREATE_ENGINE_FULL()

Testing AngelScript SDK APIs?
  --> ASTEST_CREATE_ENGINE_NATIVE()

Everything else (bindings, syntax, compiler, functional):
  --> ASTEST_CREATE_ENGINE() + ASTEST_GET_ENGINE() pattern
```

## Where to Put Test Helpers

| Condition | Location | Example |
|-----------|----------|---------|
| Used by ≥2 theme directories (Bindings + Syntax + HotReload…) | `Shared/*.h` on disk; include as `AngelscriptTestExecute.h` etc. | `BuildModule`, `FAngelscriptTestExecutor`, `ExecuteAndExpectInt` |
| Used by ≥2 files under `Bindings/` only | `Bindings/Angelscript*TestHelpers.h` | `Bindings/AngelscriptTArrayBindingsTestHelpers.h` 等 |
| Single CQTest `.cpp` only | Put helpers inside the owning `TEST_CLASS_WITH_FLAGS` class under `private:` | Nested fixture structs, runner functions, local constants |
| Large file split by section | `Bindings/*Sections.h` + main `.cpp` | Console bindings cluster |

Single-file CQTest helpers should live inside the owning test class. Use `private:` for helper constants, nested structs, and runner functions, then restore `public:` before `BEFORE_*`, `AFTER_*`, and `TEST_METHOD` entries so CQTest registration stays visible. Keep file-level helper namespaces only for explicitly reviewed non-CQTest or multi-class cases where moving the helper would duplicate meaningful setup.

**Bindings reference:** `Bindings/AngelscriptQuatBindingsTests.cpp` (`FScopedAngelscriptModule` + `ExpectGlobalInt` / `ExecuteAndExpect*`).

## Naming Conventions

| Category | Pattern | Example |
|----------|---------|---------|
| File name | `Angelscript[Theme]Tests.cpp` | `AngelscriptControlFlowTests.cpp` |
| Test path | `Angelscript.TestModule.[Theme].[Feature]` | `Angelscript.TestModule.Syntax.CQTest` |
| Module prefix | `AS[Theme][Feature]` | `ASControlFlowForLoop` |

## Infrastructure Files

| File | Purpose |
|------|---------|
| `AngelscriptTestMacros.h` | 5 engine macros (the only macro file for new tests) |
| `AngelscriptTestLegacyHelpers.h` | Legacy COMPILE_RUN/BUILD_MODULE macros (deprecated) |
| `AngelscriptTestUtilities.h` | Engine creation/destruction utility functions (umbrella) |
| `AngelscriptTestEnginePool.h` | Module-clean engine pool and FScopedModuleCleanEngine |
| `AngelscriptTestEngineHelper.h` | Compile/execute helper functions |
| `AngelscriptTestExecute.h` | **Canonical** `FAngelscriptTestExecutor`, `ExecuteAndExpect*`, `ExpectGlobalInt` |
| `AngelscriptTestModuleScope.h` | `FScopedAngelscriptModule` (explicit module name + source) |
| `AngelscriptBindingsAssertions.h` | Forward shim → `AngelscriptTestExecute.h` |
| `AngelscriptBindingsModuleBuilder.h` | Forward shim → `AngelscriptTestModuleScope.h` |
| `AngelscriptGlobalFunctionInvoker.h` | Forward shim → `AngelscriptTestExecute.h` (`FASGlobalFunctionInvoker` alias) |
| `Bindings/Angelscript*TestHelpers.h` | Bindings-only shared helpers (TArray, World collision, math compare) |

`AngelscriptTest.Build.cs` adds `Shared/` to `PrivateIncludePaths`, so new tests use `#include "AngelscriptTestExecute.h"` rather than `#include "Shared/..."`. Headers under `Bindings/` still use the `Bindings/` include prefix.
| `Shared/AngelscriptReflectiveAccess.h` | Property/function reflective access helpers |
| `Template/Template_CQTest.cpp` | CQTest teaching template (6 examples) |
