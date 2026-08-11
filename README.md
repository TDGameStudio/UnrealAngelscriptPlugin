# Unreal AngelScript 1.0.0

Unreal AngelScript is a source Unreal Engine plugin that integrates AngelScript as a first-class scripting option for Unreal Engine projects.

## Version and source lineage

- Product version: `Unreal AngelScript 1.0.0` (`10000` in the public encoded version contract).
- Upstream source lineage: `AngelScript 2.33.0 WIP lineage + selective 2.38 backports`.
- Compatibility: an older 1.x header may use a newer compatible 1.x runtime; breaking public C API or ABI changes require a new major version.

The upstream lineage is provenance, not the plugin version. Vanilla AngelScript 2.33 headers pass `23300` to `asCreateScriptEngine()` and are intentionally rejected. Consumers must compile against the `angelscript.h` shipped with this plugin.

Before packaging or publishing a release, validate the public header and plugin descriptor:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File Tools\ValidateVersion.ps1
```

This repository is imported from `TDGameStudio/AngelscriptProject` as a clean plugin snapshot. The plugin directory name remains `Angelscript`, so install it under:

```text
<ProjectRoot>/Plugins/Angelscript/
```

## Contents

- `Angelscript.uplugin` - Unreal plugin descriptor.
- `Source/AngelscriptRuntime/` - runtime integration, bindings, type system, script compilation, debugging, code coverage, static JIT, and dump support.
- `Source/AngelscriptEditor/` - editor integration, hot reload, content browser, source navigation, and tooling.
- `Source/AngelscriptTest/` - automation tests for the plugin.
- `Source/AngelscriptUHTTool/` - UHT integration toolchain.
- `Standalone/` - the no-Unreal CMake build, native runner, and offline compiler/analysis delivery surface.

## Requirements

- Unreal Engine 5.7 project source build or compatible local engine setup.
- A host Unreal project with this repository checked out at `Plugins/Angelscript`.

## Basic Usage

1. Clone this repository into your project plugin directory as `Angelscript`.
2. Regenerate project files if needed.
3. Build your editor target.
4. Enable or keep the `Angelscript` plugin enabled in the project.

Example layout:

```text
MyProject/
├── MyProject.uproject
└── Plugins/
    └── Angelscript/
        ├── Angelscript.uplugin
        └── Source/
```

## Incremental script cache

Cache V2 is enabled by default for Editor and packaged Runtime. The first
launch compiles authoritative loose `.as` source normally and publishes a
content-addressed generation under `Saved/Angelscript/CacheV2`; later launches
restore an exact matching generation or reuse only still-valid function, type,
and module-state records. Packages do not require a pre-generated script cache.

Cache V2 never persists process-local FunctionIds. Logical functions use stable
BLAKE3-256 keys, and each Engine rebuilds the stable-key-to-current-FunctionId
route. StaticJIT providers consume the same stable identity plus content,
profile, and ABI coordinates; a missing or stale Native entry falls back to VM
without changing cache validity.

Project Settings exposes **AngelScript Incremental Cache**, including the
shutdown flush timeout, Pack target, bounded immutable-preparation workers,
packaged reload policy, and opt-in decision trace. Production defaults are a
64 MiB Pack target and at most four preparation workers. Engine mutation,
declaration/type materialization, ClassGenerator, module swap, and route
publication remain serialized per Engine.

Runtime diagnostics include `as.Cache.Status`, `as.Cache.Flush`,
`as.Cache.Verify`, `as.Cache.Compact`, `as.Cache.ForceClean`, `as.Cache.Trace`,
and `as.Cache.Explain`. Add `-as-cache-report=<absolute-json-path>` to emit a
pointer-free process report. The read-only offline inspector is documented at
`Tools/CacheV2Dump/README.md`; it validates and dumps pointer, Manifest, Pack,
record, generation, and stable-route data without starting Unreal or mutating
the Store.

## Class Rename Redirects

AngelScript-generated classes are regular Unreal classes in the `Angelscript` script package. When hot reload can see an unambiguous one-class rename, the plugin writes an official Unreal `[CoreRedirects]` class redirect to project config and registers it for the current editor session:

```ini
[CoreRedirects]
+ClassRedirects=(OldName="/Script/Angelscript.AOldScriptClass",NewName="/Script/Angelscript.ANewScriptClass")
```

For ambiguous edits, offline asset migration, or hand-authored redirects, use the same format in project or plugin config. Use the actual generated class names, including the `A` / `U` prefix used by the AS class. Unlike many native C++ examples, do not strip the prefix for AS classes. The plugin consumes UE's loaded CoreRedirects during hot reload so existing Blueprint children can be reparented from the removed AS class to the renamed AS class.

## Standalone compiler

`Standalone/` builds the maintained fork and shared language-core sources
without Unreal Engine. Its native profile compiles and executes a bounded
portable standard library; its UE profile performs compile-only validation
against exactly one complete offline bundle. UE declarations are installed as
non-executable traps, and UE-validation artifacts cannot be run or loaded as
UE bytecode.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File Tools\RunTestSuite.ps1 -Suite Standalone -LabelPrefix standalone -TimeoutMs 600000
powershell.exe -NoProfile -ExecutionPolicy Bypass -File Tools\RunTestSuite.ps1 -Suite StandaloneRelease -TimeoutMs 1200000
```

See `Standalone/README.md` for build, CLI, package, security, adapter, resource,
and support-tier contracts.

## Standalone offline contract export

The editor commandlet `AngelscriptOfflineExport` captures the complete,
final AngelScript declaration surface after manual bindings, generated
bindings, reflective fallback, optional plugins, project registrations, and
the last successful script compilation. Existing `Bind_*.cpp` providers do
not contain standalone branches or exporter hooks.

Example project export through the repository runner, including an external
consumer `.uproject`:

```powershell
$bundleArgs = @("-BundleKind=Project", "-Output=D:\Exports\MyProjectAS", "-AssetRoots=/Game")
& Tools\RunCommandlet.ps1 -Commandlet AngelscriptOfflineExport -ProjectFile D:\Projects\MyGame\MyGame.uproject -Label offline-bundle-external -TimeoutMs 600000 -ExtraArgs $bundleArgs
```

Without `-Output`, the commandlet writes to the ignored
`Saved/AngelscriptStandalone/project/` or
`Saved/AngelscriptStandalone/default-engine/` directory. It always requires
a complete final symbol scope. An independently incomplete asset scope may
only be published with `-AllowIncompleteAssets`; its scope remains explicitly
incomplete so offline resource queries cannot report authoritative absence.
There are no module or plugin symbol filters.

`DefaultEngine` is a packaged-selection role, not an engine-only symbol
filter. The Standalone distribution's default is generated from the
checked-in UE 5.8 `AngelscriptProject` host with its normally enabled plugins
and explicit `/Game` scope. External projects run the same plugin-owned
commandlet against their own `.uproject`, export `BundleKind=Project`, and
pass that directory explicitly to `as-standalone --bundle`; it replaces the
packaged default without merge or fallback.

For UE validation, each `--script-root` is treated as a project `Script/`
root. A relative source such as `Foo/Bar.as` uses
`/Angelscript/Game/Foo/Bar.as` as its offline virtual-source identity, so it
exactly replaces the matching exported script baseline. V1 does not infer
plugin or memory mounts from arbitrary filesystem roots.

After building `StandaloneRelease`, validate the real delivery boundary with:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File Tools\RunStandaloneExternalSmoke.ps1 -TimeoutMs 1200000
```

This creates a transient content-only project with no C++ host module,
compares default-path and explicit-path Project exports byte for byte, then
extracts and runs the CLI from the final Release ZIP. Its machine-readable
summary is written under `Saved/StandaloneExternalSmoke/<RunId>/Summary.json`.

Each atomically published bundle consists of `manifest.json`,
`symbols.jsonl`, and `assets.jsonl` in canonical UTF-8 JSON/JSONL form with
record counts and SHA-256 hashes. It contains declarations, normalized asset
paths, provenance, and compatibility metadata only—never native/object
addresses, source text, function bodies, bytecode, executable code, or asset
payloads. The no-UE standalone process reads one complete bundle and never
loads Unreal Engine or project/plugin binaries.

## Tests

The `AngelscriptTest` module is included with the plugin because this is a source plugin and the tests are part of its validation surface. Consumer builds default to not registering the Angelscript C++ automation tests.

Enable test registration in the host project before running Angelscript C++ automation tests:

```ini
; Config/DefaultAngelscriptCompileOptions.ini
[/Script/AngelscriptRuntime.AngelscriptCompileOptions]
bCompileAngelscriptUnitTests=true
FunctionBindingMethod=NativeRuntimeLinked
+NativeRuntimeLinkedModules=Engine
+NativeRuntimeLinkedModules=UMG
```

`FunctionBindingMethod` selects one global automatic binding backend: `None`, `NativeRuntimeLinked`, or `NativeModuleFunctionAddress`. Runtime-linked modules are listed with `+NativeRuntimeLinkedModules=ModuleName`; target-module address generation is listed with `+NativeModuleFunctionAddressModules=ModuleName`. `NativeModuleFunctionAddress` requires a source-built Unreal Engine and is rejected by the Editor, UBT, and UHT for installed or unknown engine distributions. After changing compile options, rebuild the editor target so `AngelscriptTest.Build.cs` and `AngelscriptRuntime.Build.cs` consume the new method and module arrays. In the original host project, tests are run through `Tools/RunTests.ps1`; standalone consumers can run Unreal Automation tests by prefix once the plugin is installed in a host project and rebuilt with test registration enabled.

The module arrays use UE config operations consistently: an unprefixed assignment replaces the array, `+` appends, `-` removes, and `!Array=ClearArray` clears it. The Runtime-linked build emits one stable per-module aggregator that conditionally includes only the shards produced by the current UHT pass; it no longer creates a fixed set of wrapper translation units. Target-module descriptors are owned by the registering modular feature at runtime, remain pending while their `UClass` is unavailable, and are removed when that feature unregisters.

### Reflected AngelScript test suites

Script tests are ordinary reflected classes derived from the Runtime-owned
`UAngelscriptTestSuite`. A method becomes an independent UE Automation leaf
only when it is a non-static `void()` method marked with
`meta=(AngelscriptTest)`; neither a `TEST_` name nor `UFUNCTION(Test)` is used.

```angelscript
UCLASS(meta=(AngelscriptTestFlags="EditorContext;EngineFilter"))
class UInventoryScriptTests : UAngelscriptTestSuite
{
	UFUNCTION(BlueprintOverride)
	void BeforeEach()
	{
		// A fresh suite instance is created for each leaf.
	}

	UFUNCTION(meta=(AngelscriptTest))
	void AddingAnItemUpdatesTheCount()
	{
		AssertEquals(2, 1 + 1);
	}

	void UnmarkedHelper()
	{
	}
}
```

`AngelscriptTestFlags` is a semicolon-separated **exact**
`EAutomationTestFlags` mask. It must contain at least one application context
and exactly one filter; empty, duplicate, and unknown tokens are rejected
instead of receiving defaults. `#if EDITOR` still controls compilation of
editor-only APIs, while `EditorContext` independently controls Automation
execution eligibility.

Each leaf gets a fresh transient fixture instance. `BeforeEach`, the marked
method, ordinary command callbacks, and `AfterEach` share that instance.
`BeforeAll` and `AfterAll` run synchronously on a separate suite-scope
instance for each worker and registry generation. Lifecycle exceptions retain
their script source and line. A failed `BeforeAll` skips the generation's
leaves but still attempts `AfterAll`; an `AfterAll` failure is reported as a
suite/session diagnostic.

The suite instance owns fixture state and supplies fail-fast assertions and
expected-log matching. Explicit environment tools live on the fieldless
`FAngelscriptTest` USTRUCT, exposed to script as same-name namespace global
functions. A pure test creates no World. Tests that need one call
`FAngelscriptTest::CreateTestWorld`, then use
`FAngelscriptTest::SpawnObject`, `SpawnActor`, `SpawnComponent`, `BeginPlay`,
`TickWorld`, `TickActor`, `TickComponent`, `AdvanceTime`, and explicit or
automatic cleanup. These tools are no longer methods on
`UAngelscriptTestSuite`. No public test World, Map, or Network UObject type is
required.

`SpawnObject` itself does not require a World: by default the new object is
owned by the current leaf fixture and is released during terminal cleanup.
Use `FAngelscriptTest::CreateTestWorld(true)` only when the test needs a
GameInstance and initialized subsystem context;
`FAngelscriptTest::CreateTestWorld(false)` is the lighter Actor or Component
test mode. Script-defined `StaticClass()` calls remain
editor-only, so examples that pass a script class to `SpawnObject` or
`SpawnActor` must still be compiled inside `#if EDITOR`.

Expected-log matching is finalized against the leaf's own result in UE
Automation, commandlet, and automatic hot-reload execution. It therefore does
not rely on an enclosing Automation test and cannot consume a fail-fast
assertion diagnostic.

```angelscript
ExpectError("intentional warning", 1);                // contains
ExpectErrorRegex("item-[0-9]+ unavailable", 2);      // regex + count
Error("prefix intentional warning suffix");
Error("item-12 unavailable");
Error("item-34 unavailable");
```

For common latent flows, start with `FAngelscriptTest::Commands()` and
construct a fluent queue with `Do`/`Then`,
`StartWhen`/`Until`, `WaitDelay`, and LIFO `OnTearDown`/`OnCleanup`.
Queue construction is synchronous—it is not a resumable script `await`—so
work that depends on a wait belongs in a later `Then` callback. Existing
advanced `ULatentAutomationCommand` implementations remain supported through
`FAngelscriptTest::Commands().AddLatentCommand(...)` and can access their active fixture with
`GetCurrentSuite()`. Ordinary script exceptions from an advanced command's
`Before`, `Update`, or `After` callback fail the owning leaf and stop its
remaining main-command sequence; the hot-reload callback guard does not hide
those errors. An ordinary exception from `AfterEach` or a registered cleanup
callback remains a separate source-located error even after an earlier leaf
failure; only the framework's controlled assertion exception is deduplicated.

For client-enabled advanced commands, the configured timeout is an overall
deadline through client finalization. Reaching it always runs server `After`
at most once, destroys the executor, clears the suite association, and
completes the leaf. `bAllowTimeout` suppresses the timeout error only; it
cannot leave a command waiting forever in `AfterOnClient`. Losing the weak
client executor also fails and finalizes the leaf with a phase diagnostic.

Leaves are exposed as:

```text
Angelscript.ScriptTests.<Module>.<Suite>.<Method>
```

For ordinary files below a `Script/` root, `<Module>` is the relative path
with `.as` removed and path separators replaced by dots. For example,
`Script/Tests/Test_ReflectedScriptSuites.as` exposes this concrete leaf:

```text
Angelscript.ScriptTests.Tests.Test_ReflectedScriptSuites.UReflectedFixtureScriptTests.FirstLeafGetsFreshFixtureState
```

The same prefix can select the root, one module, one suite, or one method.
For custom source providers, use the Editor Automation list or the exported
report's `fullTestPath` as the authoritative name. The repository's runnable
example file currently covers twelve leaves: assertions, fixture isolation,
expected errors, World-free UObject creation, GameInstance/World tools,
fluent and advanced latent work, exact runtime flags, and editor-only
compilation.

The registry publishes only after successful class generation. Hot reload
cancels affected latent work at a callback boundary, runs old-generation
teardown and World cleanup, and then exposes the newest names, markers,
bodies, and exact flag buckets. Failed compilation retains the last-good
registry. An open Automation suite section closes its old-generation All-hook
session before compile and lazily opens the new one for the next leaf.
Completed automatic-run failures are reported once rather than on every idle
tick. Engine shutdown likewise cancels only leaves owned by that
`FAngelscriptEngine` and closes its All-hook session before script functions
are released.

The `AngelscriptTest` commandlet selects non-disabled leaves whose exact mask
contains `CommandletContext`. It reports `selected`, `executed`, `passed`, and
`failed` counts and fails when no eligible leaf runs, selected work is not
fully executed, a leaf fails, or suite lifecycle teardown fails. Diagnostics
retain the script file, line, and original error text; the summary always
keeps `passed + failed == executed`.

The retired global `Test_*(FUnitTest&)` and
`IntegrationTest_*(FIntegrationTest&)` protocols, their implicit Map/PIE
startup, and their Automation roots are no longer registered. Parameterized
providers, automatic Map/PIE/network sessions, lambda/function-handle
callbacks, and resumable VM `await` are intentionally outside this framework.

## History

This repository starts from a snapshot import. Earlier development history and planning context remain in `TDGameStudio/AngelscriptProject`.

## License

See `LICENSE.md`.
