# Unreal AngelScript Standalone

`as-standalone` is the no-Unreal delivery surface of the maintained
`Plugins/Angelscript` fork. It builds the same maintained AngelScript compiler
sources with CMake and compiles its private standard-C++ frontend directly
into `AngelscriptStandaloneHost`; it does not include or link Unreal Engine.
The UE plugin keeps its original `FAngelscriptPreprocessor` and descriptor
graph. The two hosts share the maintained fork and the complete offline JSON
bundle, not an in-memory frontend or public Language layer.

It exposes two deliberately different profiles:

- `compile --dialect native` compiles portable AngelScript and may emit
  bytecode. `run` executes only this native profile in the restricted
  standalone standard library.
- `compile --dialect ue` performs compile-only analysis against one complete
  offline Unreal declaration bundle. Its bytecode and class records are
  validation artifacts only: they cannot be run and are not advertised as
  Unreal-loadable.

There is no UE execution command. The standalone process never loads Unreal,
project or plugin DLLs, UObjects, assets, native binding addresses, or
ClassGenerator.

## Build and test

From this directory with CMake 3.25+ and Visual Studio 2022:

```powershell
cmake --preset win64-msvc
cmake --build --preset win64-msvc-debug
ctest --preset win64-msvc-debug --output-on-failure
```

From the repository root, the supported runner is:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File Tools\RunTestSuite.ps1 -Suite Standalone -LabelPrefix standalone -TimeoutMs 600000
powershell.exe -NoProfile -ExecutionPolicy Bypass -File Tools\RunTestSuite.ps1 -Suite StandaloneRelease -TimeoutMs 1200000
```

The Debug and Release suites each run 19 CTests and have isolated report
directories. Their counts remain separate from the repository's Unreal
Automation counts. `StandaloneRelease` also builds the final package target
before CTest and intentionally remains outside the repository `All` suite.

Build and inspect the Win64 Release distribution:

```powershell
cmake --build --preset win64-msvc-release --target AngelscriptStandalonePackage
ctest --preset win64-msvc-release -R AngelscriptStandalone.Package --output-on-failure
```

The directory and zip are written below
`out/build/win64-msvc/package/Release/`. Package inspection runs the installed
executable, validates both examples and stable per-file hashes, requires the
generated `contracts/default-engine/`, and rejects project bundles or any
unlisted delivery surface.

From the repository root, validate the package against an external consuming
project after the Release suite succeeds:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File Tools\RunStandaloneExternalSmoke.ps1 -TimeoutMs 1200000
```

The smoke project has no C++ host module. It exports its complete Project
bundle through the plugin-owned commandlet twice, compares the three files
byte for byte, extracts the final ZIP, and runs that installed CLI against the
explicit bundle. Evidence is written below
`Saved/StandaloneExternalSmoke/<RunId>/`.

## Commands

Compile and run native AngelScript:

```powershell
as-standalone compile --dialect native --script-root Examples/Native --entry hello.as --output out/native --diagnostics text --emit-bytecode
as-standalone run --script-root Examples/Native --entry hello.as --output out/run --diagnostics text -- Codex
```

Analyze UE AngelScript with the packaged default contract:

```powershell
as-standalone compile --dialect ue --script-root Examples/UEValidation --entry basic.as --output out/ue --diagnostics text --emit-bytecode
```

Analyze against a complete project bundle exported from the matching Unreal
project:

```powershell
as-standalone compile --dialect ue --script-root Script --entry Examples/Core/Example_Array.as --bundle D:/Exports/MyProjectAS --output out/project --diagnostics json --emit-bytecode --allow-ue-required
```

An explicit `--bundle` replaces the packaged default in full. Bundles are
never merged, no cache or environment variable is searched, and an invalid
explicit bundle fails with infrastructure exit code `2` without falling back.

For UE validation, `--script-root` is the project `Script/` root. Relative
logical source paths map to UE virtual identities below `/Angelscript/Game/`.
For example, `Foo/Bar.as` uses `/Angelscript/Game/Foo/Bar.as`; the offline
module ID is SHA-256 over `module-id-v1`, the logical module name, and that
virtual identity. This is distinct from the private Standalone frontend's
internal source identity and allows exact replacement of the same module's
exported script baseline. That internal identity is not a UE/Standalone ABI.
V1 does not guess plugin or memory mounts from arbitrary filesystem roots.

## Native profile boundary

The native standard library contains UTF-8 `string`, `array`, `dictionary`,
math, `print`, and `assert`. Application calls use AngelScript's generic
calling convention. The profile does not register file, network, process,
dynamic-library, Unreal reflection, arbitrary native-address, or FFI APIs.

`run` defaults to a 5-second deadline and a 256 MiB tracked allocation limit;
values below 16 MiB are rejected before engine creation because they cannot
safely satisfy the measured engine-bootstrap floor. The hard limit counts the
engine, contexts, script objects, Compat container storage, arrays, dictionary
objects/keys/maps, and script string/stream/constant-cache storage. CLI/result
objects, diagnostics/JSON storage, the C/C++ runtime, and operating-system
allocations are outside this counter. The result records current/peak tracked
allocation, abort/timeout/exception state, script result and normalized
diagnostics. A counted allocation that would exceed the limit is not performed;
the standalone-only exception-enabled fork build converts bounded-library
failure to exit `4`, and all tracked allocations must return to zero after
shutdown. These are defense-in-depth process limits, not an operating-system
sandbox.

`AS_USE_EXCEPTIONS=1` is private to the standalone CMake maintained-fork
target. Unreal/UBT keeps the fork's historical `AS_NO_EXCEPTIONS` policy; this
is not a `.uplugin` setting, public plugin macro, or supported consumer option.

## UE-validation profile boundary

The packaged `contracts/default-engine/` bundle is the complete snapshot
exported from the checked-in Unreal Engine 5.8 `AngelscriptProject` host and
its normally enabled plugins. It contains that host's project, Blueprint,
optional-plugin, active script-baseline, and declared `/Game` asset scope.
`default-engine` describes its packaged selection role; it does not mean
engine-only or minimal-host. Use a matching explicit project bundle whenever
the target project's exact surface differs. Both bundle kinds are complete
independent snapshots, not deltas.

The source distribution stores this large contract compressed at
`Contracts/UE5.8/default-engine.zip`. CMake extracts and verifies fixed
per-file SHA-256 values before placing the normal three-file directory beside
the executable or in an installed package. The CLI never reads ZIP files at
runtime.

Imported callables and adapter behaviors are registered with a compile-only
generic trap. Static analysis may type-check them; execution is forbidden.
Artifact metadata marks the profile `ue-validation`, `ueValidationOnly: true`,
and template layouts `non-ue-abi`.

Resource analysis recognizes typed soft object/class paths, reviewed
load-like contexts, and parameter-level resource semantics exported in the
selected bundle. Parameter markers are consumed only after the compiler has
resolved the owning callable to its stable symbol ID and supplied the exact
argument span; a same-named function or unmarked string is ignored. It
normalizes paths and reports:

| State | Default meaning |
| --- | --- |
| `found` | Path and requested type are proven by the selected complete scope. |
| `redirected` | A recorded redirect reaches a compatible final asset. |
| `missing` | An authoritative included scope proves absence; soft contexts warn and hard contexts fail. |
| `incompatible` | The asset exists but is not assignable to the requested type. |
| `unknown` | The bundle scope or hierarchy cannot decide; never promoted to a fabricated missing result. |

`--strict-resources` promotes authoritative soft missing only. It does not turn
unknown scope into missing. Standalone never loads, resolves, cooks, or
executes an asset.

## Output and exit contract

Compile output is replaced as one artifact directory and contains
`result.json`, `diagnostics.jsonl`, and optional module `.asbc` files.
UE-validation also writes `.classes.jsonl`. Identity fields are canonical and
deterministic; elapsed time and process-local runtime IDs are not compile
identity.

- `0`: requested compile/run completed successfully
- `1`: source validation or compilation failed
- `2`: usage, bundle, I/O, schema, integrity, or other infrastructure failure
- `3`: native script exception/assertion
- `4`: native timeout, cancellation, or allocation-limit termination

Use `as-standalone --version` for product, upstream lineage and profile/schema
versions. See `Schemas/` for the shipped machine-readable contracts and
`SUPPORT_MATRIX.md` for the evidence-scoped support statement.

## Licenses and provenance

Package license files live under `LICENSES/`. The maintained engine sources
remain under the plugin runtime rather than being copied into this directory.
Pinned standalone add-on and RapidJSON provenance, source revisions, hashes
and maintained deltas are recorded in `ThirdParty/README.md`.
