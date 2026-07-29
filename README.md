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
