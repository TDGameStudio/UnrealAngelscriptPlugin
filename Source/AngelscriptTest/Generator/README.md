# Generator Tests

`Generator/` tests are grouped by the generated AngelScript surface or generator capability they exercise. Automation prefixes use `Angelscript.TestModule.Generator.*` so runner filters describe the broader generator coverage instead of only class generation.

## Test Families

| Family | Purpose | Typical assertion style |
| --- | --- | --- |
| Generated behavior | Compile AngelScript and verify the reflected/runtime type that gets generated. | Compile script fixtures, inspect `UASClass` / `UASStruct` / `UFunction` / properties, execute representative paths. |
| Generator capability | Exercise `FAngelscriptClassGenerator` or a focused seam around it. | Build generator inputs or pure helper graphs, assert reload classification, propagation, name conflict handling, or validation output. |

Choose the family first when adding a test. If the failure can be proven without compiling AngelScript, prefer a generator-capability test. If the behavior only matters once script syntax produces reflected types, use a generated-behavior test.

## Subdirectories

| Directory | Responsibility |
| --- | --- |
| `ASClass/` | Generated `UASClass` behavior: object/actor/component construction, metadata, reference schema, replication, ticking, interface dispatch. |
| `ASFunction/` | Generated `UASFunction` behavior: metadata, arguments, dispatch, optimized calls, `ProcessEvent`, world context. |
| `ASStruct/` | Generated `UASStruct` identity, discard behavior, and struct hot reload. |
| `ScriptClass/` | Script-facing class declaration shape and creation behavior before it becomes a concrete AS class surface test. |
| `ReloadPlanning/` | Reload classification and dependency propagation, including direct tests for `FAngelscriptClassReloadPlanner`. |
| `ComponentValidation/` | Component-specific metadata validation diagnostics. |
| `Core/` | General generator scaffolding, compile checks, name conflicts, compose-onto-class, default statement safety, literal asset post-init. |
