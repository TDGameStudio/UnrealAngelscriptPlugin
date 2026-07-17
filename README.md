# Unreal Angelscript Plugin

UnrealAngelscriptPlugin is a source Unreal Engine plugin that integrates AngelScript as a first-class scripting option for Unreal Engine projects.

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
bCompileAngelscriptModuleLocalBindings=false
```

`bCompileAngelscriptModuleLocalBindings` is disabled by default and requires a source-built Unreal Engine. It controls both UHT module-local binding shard generation and the Runtime modular-feature bridge. The Editor rejects enabling it for installed or unknown engine distributions; direct ini edits are rejected by UBT and UHT as well. After changing compile options, rebuild the editor target so `AngelscriptTest.Build.cs` can update `WITH_ANGELSCRIPT_UNITTESTS` and `AngelscriptRuntime.Build.cs` can update `WITH_ANGELSCRIPT_MODULE_LOCAL_BINDINGS`. In the original host project, tests are run through `Tools/RunTests.ps1`; standalone consumers can run Unreal Automation tests by prefix once the plugin is installed in a host project and rebuilt with test registration enabled.

## History

This repository starts from a snapshot import. Earlier development history and planning context remain in `TDGameStudio/AngelscriptProject`.

## License

See `LICENSE.md`.
