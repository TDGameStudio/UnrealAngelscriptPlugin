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

## History

This repository starts from a snapshot import. Earlier development history and planning context remain in `TDGameStudio/AngelscriptProject`.

## License

See `LICENSE.md`.
