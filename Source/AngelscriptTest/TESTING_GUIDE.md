# Angelscript Test Guide

This guide is the local quick reference for writing or refactoring C++
automation tests under `Plugins/Angelscript/Source/AngelscriptTest/`.
`Documents/UnitTest/UnitTest.md` is the newest policy source for unit-test
style. Broader test layers, runners, and inline AngelScript formatting are
covered by:

- `Documents/Guides/Test.md`
- `Documents/Guides/TestConventions.md`
- `Documents/Rules/ASInlineFormattingRule.md`
- `Plugins/Angelscript/Source/AngelscriptTest/Template/`

## Macro Quick Reference

All macros are defined in `Shared/AngelscriptTestMacros.h`:

| Macro | Returns | Usage |
|---|---|---|
| `ASTEST_CREATE_ENGINE()` | `FAngelscriptEngine&` | Shared engine, reset to clean state. Use once in `BEFORE_ALL()`. |
| `ASTEST_GET_ENGINE()` | `FAngelscriptEngine&` | Existing shared engine, no reset. Use in `TEST_METHOD()`. |
| `ASTEST_CREATE_ENGINE_FULL()` | `FAngelscriptEngine&` | Fresh isolated full engine. Use for hot reload, bind environment, or GC tests that require isolation. |
| `ASTEST_CREATE_ENGINE_NATIVE()` | `asIScriptEngine*` | Raw AngelScript SDK engine. Use for SDK API tests. |
| `ASTEST_RESET_ENGINE(Engine)` | void | Reset shared engine. Use in `AFTER_ALL()`. |

## CQTest Structure Rules

New tests should use CQTest (`TEST_CLASS_WITH_FLAGS`) unless an existing
neighbor has a clear reason to stay on `IMPLEMENT_SIMPLE_AUTOMATION_TEST`.

Keep the test implementation inside the owning test class:

- Let each `TEST_METHOD` show the test flow directly.
- Put constants, narrow helpers, and observation structs used by only one test
  class under that class's `private:` section.
- Do not create an anonymous namespace just for one CQTest class.
- Do not move the main flow into a file-level static function and leave the
  `TEST_METHOD` as a one-line forwarder.
- Do not wrap CQTest assertions with file-level aliases such as
  `#define TestTrue(...)` or `#define TestEqual(...)`.

Recommended shape:

```cpp
#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestExecute.h"

TEST_CLASS_WITH_FLAGS(FExampleHotReloadTest,
	"Angelscript.TestModule.HotReload.Example",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FReloadObservation
	{
		int32 PostReloadCount = 0;
	};

public:
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

	TEST_METHOD(BroadcastsPostReload)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FReloadObservation Observation;
		// Compile, reload, assert.
	}
};
```

Avoid:

```cpp
namespace ExampleTest_Private
{
	static bool RunBroadcastsPostReload(FAutomationTestBase& Test);
}

TEST_METHOD(BroadcastsPostReload)
{
	ASSERT_THAT(IsTrue(ExampleTest_Private::RunBroadcastsPostReload(*TestRunner)));
}
```

### Engine Lifecycle

Each CQTest class should create the shared test engine in `BEFORE_ALL()` and
reset it in `AFTER_ALL()`. A `TEST_METHOD` should only acquire the existing
engine:

```cpp
BEFORE_ALL()
{
	ASTEST_CREATE_ENGINE();
}

AFTER_ALL()
{
	FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
	ASTEST_RESET_ENGINE(Engine);
}

TEST_METHOD(MyCase)
{
	FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
	FAngelscriptEngineScope Scope(Engine);
}
```

Rules:

- Do not call `ASTEST_CREATE_ENGINE()` in every `TEST_METHOD`.
- Do not call `ASTEST_RESET_ENGINE(Engine)` from each method's `ON_SCOPE_EXIT`.
- Each `TEST_METHOD` must clean up the AS modules, delegate handles, and
  transient objects it creates.
- If a test genuinely needs an isolated full engine, use
  `ASTEST_CREATE_ENGINE_FULL()` and explicitly drain modules created during the
  method.
- If the class contains `private:` helpers, restore `public:` before
  `BEFORE_ALL()`, `AFTER_ALL()`, and `TEST_METHOD` entries so CQTest can
  register them.

### Full Engine Pattern

Use this for complete isolation, especially hot reload, bind environment, or GC
cases:

```cpp
TEST_METHOD(IsolatedTest)
{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
	FAngelscriptEngineScope Scope(Engine);
	ON_SCOPE_EXIT
	{
		for (const auto& Module : Engine.GetActiveModules())
		{
			Engine.DiscardModule(*Module->ModuleName);
		}
	};

	// Test code.
}
```

### Native SDK Pattern

Use this only for direct AngelScript SDK API tests. Do not introduce
`FAngelscriptEngine` in `AngelScriptSDK/` native tests.

Native SDK sources live under behavior-owned `AngelScriptSDK/Engine`, `Frontend`,
`Compiler`, `Runtime`, `Module`, `TypeSystem`, `Language`, `Embedding`, and
`Conformance` directories. Include `Support/AngelscriptNativeCoreTestSupport.h`
for raw engine/module support and `Support/AngelscriptNativeExecutionTestSupport.h`
only when invoking a compiled function. Resolve functions by their full canonical
declaration; do not fall back to a name-only lookup. The native core suite excludes
all `sdk/add_on` registrations.

Current fork behavior is a valid regression contract. In particular, tests must
assert actual float64 ABI slots and explicit rejection diagnostics rather than
accepting alternate upstream outcomes. Expressible selected 2.38 script semantics
are compiled CQTest classes with `EAutomationTestFlags::Disabled` and the
`#as-v238-backport` tag; do not hide them behind a preprocessor false branch.

```cpp
TEST_METHOD(SDKTest)
{
	asIScriptEngine* NativeEngine = ASTEST_CREATE_ENGINE_NATIVE();
	ASSERT_THAT(IsNotNull(NativeEngine, TEXT("Native AngelScript engine should be created")));
	ON_SCOPE_EXIT { NativeEngine->ShutDownAndRelease(); };

	asCScriptEngine* BareEngine = reinterpret_cast<asCScriptEngine*>(NativeEngine);
	// Raw SDK test code.
}
```

Decision tree:

```text
Testing hot reload / bind environment / GC?
  -> ASTEST_CREATE_ENGINE_FULL()

Testing raw AngelScript SDK APIs?
  -> ASTEST_CREATE_ENGINE_NATIVE()

Everything else, including bindings, syntax, compiler, and functional tests?
  -> ASTEST_CREATE_ENGINE() in BEFORE_ALL() + ASTEST_GET_ENGINE() in TEST_METHOD()
```

## Bindings Test Organization

Bindings/CQTest files with several independent coverage surfaces should split
them into scenario-focused `TEST_METHOD`s under one test class instead of
packing every section into one `Compat` or `OptionalCompat` method.

Good split dimensions:

- baseline or compatibility behavior
- type matrix
- API entry-point coverage
- null, boundary, and exception paths
- return-type or log diagnostic paths

Rules:

- `TEST_METHOD` names must describe the scenario.
- Create `FScopedAngelscriptModule` inside the corresponding `TEST_METHOD`; the
  module name should match the scenario.
- Consume return values from `ExpectGlobalInts`, `Execute...`, or similar
  helpers with `ASSERT_THAT(IsTrue(...))` or an equivalent assertion.
- File-level native bind registration objects such as
  `AS_FORCE_LINK const FAngelscriptBinds::FBind ...` are allowed because they
  must register during AS bind initialization. The test flow, fixtures, and
  assertions still belong inside `TEST_CLASS_WITH_FLAGS`.

## Inline AngelScript Fixture Rules

Inline AngelScript source in C++ tests must use `ASTEST_AS(R"AS(... )AS")`.
Use `ASTEST_AS_ANSI(...)` for ASSDK/raw SDK paths that require `const char*` or
`std::string`.

Do not pass visually indented raw strings directly to compile helpers:

```cpp
const FString Source = TEXT(R"AS(
	UCLASS()
	class AMyActor : AActor
	{
	}
	)AS");
```

Use:

```cpp
const FString Source = ASTEST_AS(R"AS(
	UCLASS()
	class AMyActor : AActor
	{
	}
	)AS");
```

### Keep Fixtures Near Their Tests

Test-only AS source should usually be a local variable inside the `TEST_METHOD`
that uses it, not a file-level `GetXxxScriptV1()` / `GetXxxScriptV2()` helper.
This keeps fixture, compile/reload steps, and assertions in reading order.

Exceptions are allowed when:

- several `TEST_METHOD`s share a large, stable fixture;
- parameterized fixture generation is clearer than repeated source; or
- the helper name expresses a test-domain concept, not just a versioned source
  getter.

When a test has several AS source snippets, name variables by version or
scenario, for example:

- `ReloadV1Source`
- `ReloadV2Source`
- `DelegateSignatureV1Source`
- `DelegateSignatureV2Source`
- `TypeReloadV1Source`
- `TypeReloadV2Source`

Avoid `Script1`, `Script2`, `Text`, and generic `Source`. `ScriptSource` is
only appropriate when the method has one obvious script fixture.

Inline AS formatting must stay readable:

- AS content and the closing delimiter must not start at column 0.
- `UCLASS()`, `USTRUCT()`, and `delegate` code follows the C++ embedding indent.
- `{` uses Allman style on its own line.
- Separate multiple `UCLASS`, `USTRUCT`, and function declarations with a blank
  line.
- Keep a blank line after a `UPROPERTY()` declaration when another member
  follows.

## Hot Reload Test Rules

Hot reload tests should cover externally observable reload behavior, not just
"compiles successfully". Assert at least one of:

- reload delegate broadcast;
- old and new class, struct, delegate, or enum visibility and identity;
- generated class, struct, enum, or delegate lookup after reload;
- Blueprint child, instance, CDO, or property retargeting to the correct new
  type;
- property, function, or delegate signature retarget after reload.

Each `TEST_METHOD` must manage the modules and delegate handles it creates:

```cpp
ON_SCOPE_EXIT
{
	Engine.GetOnDelegateReload().Remove(DelegateReloadHandle);
	Engine.DiscardModule(*ModuleName.ToString());
};
```

Register cleanup before success-path work that can early-return.

### Delegate Hot Reload

In AngelScript, a `delegate` declaration itself is not a `UPROPERTY`. The class
member that uses the delegate type may be marked as `UPROPERTY`:

```angelscript
delegate void FHotReloadSignal(int Value);

UCLASS()
class UHotReloadDelegateSignatureCarrier : UObject
{
	UPROPERTY()
	FHotReloadSignal Signal;
}
```

This member produces an `FDelegateProperty`; its `SignatureFunction` points at
the corresponding `UDelegateFunction`. Delegate signature hot reload tests can
therefore validate two layers:

- `GetOnDelegateReload()` broadcasts old and new `UDelegateFunction` values.
- `FDelegateProperty::SignatureFunction` retargets to the new signature after
  reload.

If the test only targets delegate reload broadcast, a `UPROPERTY` member is not
required. If it targets property retargeting, keep the member.

Runtime delegate hot reload coverage should include the paths relevant to the
test goal:

- `UPROPERTY` delegate member: create an AS parent class, create a transient
  Blueprint child, instantiate it, then verify `FDelegateProperty::SignatureFunction`,
  property flags, and instance behavior after reload.
- Before/after runtime behavior: execute V1 behavior, reload, execute V2/V3
  behavior, and assert return values or reflected state changes.
- Blueprint runtime: use `Template_BlueprintWorldTick.cpp` /
  `FAngelscriptTestWorld` to run a Blueprint child through `BeginPlay` / `Tick`,
  reload after the actor is running, then continue ticking or invoke functions.
- Global function runtime: use `Template_GlobalFunctions.cpp` /
  `FAngelscriptTestExecutor::ExecuteAndGet` to cover delegates created and bound
  from AS global functions before and after reload.
- Property flag changes: use full reload for specifier changes such as
  `NotEditable`, `EditAnywhere`, or `BlueprintReadWrite`, then assert actual
  `CPF_Edit` / `CPF_BlueprintVisible` flags.
- Parameter or signature changes: use full reload, assert the new
  `UDelegateFunction` parameters exist, and execute the new signature path.

Current AS property defaults include `BlueprintReadWrite`. `UPROPERTY(NotEditable)`
only disables the `CPF_Edit` editing flag; it does not disable
`CPF_BlueprintVisible`. Tests that expect Blueprint invisibility must use the
appropriate Blueprint specifier or change the default configuration explicitly.

Prefer separating reload semantics by version:

- V1: baseline, create objects, Blueprints, or runtime instances.
- V2: body-only or delegate-handler body change with `ECompileType::SoftReloadOnly`;
  verify live actors continue with new behavior.
- V3: `UPROPERTY` flag or reflected-surface change with `ECompileType::FullReload`;
  verify Blueprint recompilation and new instance creation.
- V4: delegate signature or parameter-shape change with `ECompileType::FullReload`;
  verify signature retargeting and new-signature runtime behavior.

Blueprint child hot reload tests should include an ordinary
`UBlueprintGeneratedClass`, not only `UASClass`. A previous crash path occurred
when hot reload walked a Blueprint generated class and treated it as `UASClass`.
The regression shape is:

- create an AS class;
- create a transient Blueprint child;
- change the AS class reflected surface, such as adding `UPROPERTY(EditAnywhere)`;
- execute reload;
- assert no crash and that the Blueprint generated class remains a child of the
  AS class.

## AS USTRUCT Argument Regressions

AS-defined `USTRUCT` values used as delegate or `UFUNCTION` parameters need real
execution tests, not only compile or metadata checks. At least one focused test
must cover the argument-passing path:

- create an AS `USTRUCT`;
- define a delegate or `UFUNCTION` that takes the struct;
- bind a real receiver;
- execute the delegate or invoke the function reflectively;
- assert that struct field values arrive and produce the expected result.

This path goes through `FScriptCall::PushArgument`, the UE event argument buffer,
`FFrame::StepCompiledInRef`, `FUStructType::SetArgument`,
`UScriptStruct::InitializeStruct`, and the `UASStruct` CppStructOps fake-vtable
callbacks. Compile-only tests do not cover struct lifetime when Unreal owns the
destination memory.

The OpenSpec change `fix-script-struct-delegate-argument-crash` records a crash
root cause:

- AS by-value struct parameters reflect as `const FStructName&`.
- Delegate execution enters `FUStructType::SetArgument`, which calls
  `UScriptStruct::InitializeStruct` for the event argument buffer.
- UE 5.8 fake-vtable callbacks use signatures such as `Construct(void*)`,
  `Destruct(void*)`, and `Copy(void*, const void*, int32)`; they do not pass
  `ICppStructOps*` as the first argument.
- Old `FASStructOps` callbacks declared the first argument as `FASStructOps*`,
  so Unreal's destination address was misread as an ops pointer and crashed.
- The fix stores `ScriptType` / `CppStructOps` in an AS struct value header and
  injects the ops context through `UASStruct::InitializeStruct` for first-time
  construction.

For this class of issue, write a focused runtime regression first, then let
larger hot reload tests reuse the same argument shape. The focused case proves
runtime lifetime; the hot reload case proves type migration after reload.

Cleanup rules:

- remove registered delegate handles;
- discard compiled modules;
- use clear module names for multiple modules, or drain modules in full-engine
  tests;
- register cleanup before any operation that can early-return.

## CQTest Assertions and Helper Boundaries

New or refactored CQTest methods should prefer matcher assertions in the main
flow:

- `ASSERT_THAT(AreEqual(Expected, Actual, TEXT("...")))`
- `ASSERT_THAT(AreNotEqual(Expected, Actual, TEXT("...")))`
- `ASSERT_THAT(IsTrue(Value, TEXT("...")))`
- `ASSERT_THAT(IsFalse(Value, TEXT("...")))`
- `ASSERT_THAT(IsNotNull(Value, TEXT("...")))`
- `ASSERT_THAT(IsNull(Value, TEXT("...")))`

Avoid using these in new CQTest main flows:

- `TestRunner->TestEqual`
- `TestRunner->TestTrue`
- `TestRunner->TestFalse`
- `TestRunner->TestNotNull`
- `TestRunner->TestNull`
- `TestRunner->TestNotEqual`

If a helper must return `bool` so the caller can use
`ASSERT_THAT(IsTrue(...))`, it may create a local `FNoDiscardAsserter`:

```cpp
static bool ExpectNotNull(FAutomationTestBase& Test, UObject* Value, const TCHAR* Message)
{
	FNoDiscardAsserter LocalAssert(Test);
	return LocalAssert.IsNotNull(Value, Message);
}
```

Use this only to remove repeated noise. Do not hide the main test intent.

`TestRunner` is a static pointer. Pass `*TestRunner` to helpers that require
`FAutomationTestBase&`:

```cpp
CompileScriptModule(*TestRunner, Engine, ModuleName, Filename, Source);
```

Do not pass:

```cpp
CompileScriptModule(TestRunner, Engine, ModuleName, Filename, Source);
```

Helpers are appropriate for:

- pure lookup or conversion logic;
- observation structs, such as reload delegate counters;
- repeated cleanup;
- small class-private functions used only by one test class.

Avoid helpers that:

- move the whole `TEST_METHOD` flow out of the class;
- move V1/V2 AS fixtures into `Get...ScriptV1()` getters;
- combine compile, reload, and assert so the test method becomes a one-liner.

Readers should be able to see fixture setup, V1 compile, observation setup, V2
reload, and assertions directly in the `TEST_METHOD`.

## Where to Put Test Helpers

| Condition | Location | Example |
|---|---|---|
| Used by two or more theme directories | `Shared/*.h`; include as `AngelscriptTestExecute.h` etc. | `BuildModule`, `FAngelscriptTestExecutor`, `ExecuteAndExpectInt` |
| Used by two or more files under `Bindings/` only | `Bindings/Angelscript*TestHelpers.h` | `Bindings/AngelscriptWorldCollisionBindingsTestHelpers.h` |
| Single CQTest `.cpp` only | The owning `TEST_CLASS_WITH_FLAGS` under `private:` | Nested fixtures, observation structs, local constants |
| Large bindings file split by section | `Bindings/*Sections.h` plus main `.cpp` | Console bindings cluster |

`AngelscriptTest.Build.cs` adds `Shared/` to `PrivateIncludePaths`, so new tests
include `#include "AngelscriptTestExecute.h"` rather than
`#include "Shared/..."`. Headers under `Bindings/` still use the `Bindings/`
include prefix.

## Naming Conventions

| Category | Pattern | Example |
|---|---|---|
| File name | `Angelscript[Theme]Tests.cpp` | `AngelscriptControlFlowTests.cpp` |
| Test path | `Angelscript.TestModule.[Theme].[Feature]` | `Angelscript.TestModule.Syntax.CQTest` |
| Module prefix | `AS[Theme][Feature]` | `ASControlFlowForLoop` |

## Infrastructure Files

| File | Purpose |
|---|---|
| `AngelscriptTestMacros.h` | Engine macros and inline AS source wrappers for new tests |
| `AngelscriptTestLegacyHelpers.h` | Legacy `COMPILE_RUN` / `BUILD_MODULE` macros; deprecated for new tests |
| `AngelscriptTestUtilities.h` | Engine creation/destruction utilities |
| `AngelscriptTestEnginePool.h` | Module-clean engine pool and `FScopedModuleCleanEngine` |
| `AngelscriptTestEngineHelper.h` | Compile/reload helper functions |
| `AngelscriptTestExecute.h` | Canonical `FAngelscriptTestExecutor`, `ExecuteAndExpect*`, `ExpectGlobalInt`, and `FASGlobalFunctionInvoker` alias |
| `AngelscriptTestModuleScope.h` | `FScopedAngelscriptModule` with explicit module name and source |
| `AngelscriptBindingsAssertions.h` | Forward shim to `AngelscriptTestExecute.h` |
| `AngelscriptBindingsModuleBuilder.h` | Forward shim to `AngelscriptTestModuleScope.h` |
| `AngelscriptGlobalFunctionInvoker.h` | Forward shim to `AngelscriptTestExecute.h` |
| `Bindings/Angelscript*TestHelpers.h` | Bindings-only shared helpers |
| `Shared/AngelscriptReflectiveAccess.h` | Property/function reflective access helpers |

## Templates

`Template/` files are teaching fixtures and starting points, not the final
destination for feature-specific tests.

| Template | Purpose |
|---|---|
| `Template_CQTest.cpp` | CQTest compile/run, assertions, struct return, argument passing, negative path, early return |
| `Template_GlobalFunctions.cpp` | C++ to AS global function invocation through `FASGlobalFunctionInvoker` |
| `Template_ReflectionAccess.cpp` | UPROPERTY path read/write and UFUNCTION invoke across a broad type matrix |
| `Template_WorldTick.cpp` | World tick, actor tick, and component tick driving paths |
| `Template_GameLifetime.cpp` | Full actor lifecycle: construction, `BeginPlay`, tick, `EndPlay`, `Destroyed` |
| `Template_Blueprint.cpp` | Transient Blueprint child with an AS parent class |
| `Template_BlueprintWorldTick.cpp` | Blueprint actor child driven through `FAngelscriptTestWorld` callbacks |
| `Template_PIE.cpp` | Editor PIE teaching template with transient map, AS GameMode, AS-parent Level Blueprint, and explicit EndPIE cleanup |
| `Template_MultiplayerPIE.cpp` | Multiplayer PIE template covering 2/3/4 player listen-server sessions, client worlds, AS GameMode, AS-parent Level Blueprint, NetDriver, and cleanup |

## Verification Rules

After changing CQTest or hot reload tests, run the narrowest Automation prefix
first:

```powershell
Tools\RunTests.ps1 -TestPrefix "Angelscript.TestModule.HotReload.ReloadDelegates" -Label hotreload-reload-delegates -TimeoutMs 600000
Tools\RunTests.ps1 -TestPrefix "Angelscript.TestModule.HotReload.Delegates" -Label hotreload-delegates -TimeoutMs 600000
```

If the change affects compile structure, includes, unity build behavior, or
module dependencies, also run:

```powershell
Tools\RunBuild.ps1 -ExtraArgs -NoHotReloadFromIDE -TimeoutMs 1800000
```

Record pass/fail numbers from the run. Do not replace actual verification with
"should pass".
