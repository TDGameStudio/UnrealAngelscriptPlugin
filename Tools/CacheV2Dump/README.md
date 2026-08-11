# AngelScript Cache V2 Dump

`cache_v2_dump.py` is a read-only, dependency-free Python inspector for the
frozen Cache V2 `UEASCV2C`, `UEASCV2M`, and `UEASCV2P` formats. It never starts
Unreal and never writes to the inspected cache root.

Its intended use is cache testing, debugging, verification, and human
observation. It does not generate, repair, activate, or recover Runtime cache
state.

The standard-library BLAKE3 implementation is always available. If the optional
`blake3` Python package is already installed, the tool uses it as an accelerator;
the on-disk format and output do not depend on that package.

```powershell
python Plugins/Angelscript/Tools/CacheV2Dump/cache_v2_dump.py <cache-root>
python Plugins/Angelscript/Tools/CacheV2Dump/cache_v2_dump.py <cache-root> --json
python Plugins/Angelscript/Tools/CacheV2Dump/cache_v2_dump.py <cache-root> `
    --generation Current --module Gameplay --record-kind FunctionBody
python Plugins/Angelscript/Tools/CacheV2Dump/cache_v2_dump.py <cache-root> `
    --json --session-report Saved/Diagnostics/cache-v2-session.json
python Plugins/Angelscript/Tools/CacheV2Dump/cache_v2_dump.py <cache-root> `
    --json --diff Previous Current
python Plugins/Angelscript/Tools/CacheV2Dump/cache_v2_dump.py <cache-root> `
    --json --explain <function-stable-key>
```

Inputs may be a cache root, one namespace root, one `.asmanifest`, one
`.aspack`, or one named pointer. Filters are repeatable. Module filters accept
an exact canonical module name or a stable ModuleKey prefix; stable-key filters
accept a hexadecimal prefix.

The generic decoder validates BLAKE3 identities, pointer checksums, Manifest to
Pack locations, Pack indexes, None/Zlib payload integrity, and stable RecordIds.
For payload schema v1 it fully decodes ModuleInterface declarations, parameters,
imports and dependencies, plus TypeSchema inheritance/layout inputs, properties,
methods, VFT slots, behavior slots, exact Unreal/original/script names for
reflection members, and dependencies. It also
decodes FunctionBody dependencies and ModuleSnapshot links. VM execution,
initializer, and debug payloads remain opaque and are displayed only by codec,
size, and BLAKE3 metadata.

`--session-report` accepts the schema-versioned JSON emitted by the Runtime C++
diagnostic API, Blueprint `Get AngelScript Cache Status JSON`, or
`as.Cache.Status`. Correlation is read-only and uses compatibility, context,
profile, source snapshot, and full stable module keys. Diagnostic schema 3 also
checks every reported ModuleSnapshot, TypeSchema and FunctionBody RecordId, then
joins the live Engine function-route snapshot to persisted FunctionBody records
by full FunctionKey. ExecutionHash, DebugHash, Profile and ModuleKey differences
are printed as individual `session=... store=...` mismatches, including whether
the live route selected VM or Native execution. Diagnostic schema 4 additionally
validates and displays the candidate GenerationId plus candidate-module,
restored-function, compiled-miss, not-cacheable and rejected-corrupt counts for
one production hybrid compiler-reuse transaction. It also reports whether that
candidate Generation is present in the inspected Store. It never uses
process-local pointers or numeric FunctionIds. Schemas 1, 2 and 3 remain accepted for their
narrower coordinate set so older saved diagnostic reports remain inspectable.

`--diff LEFT RIGHT` selects two persisted generations by full/prefix ID or
`Current`/`Previous`/`PendingColdStart` pointer name. Records are joined by
record kind plus their stable module/type/function owner, so a body edit is one
`FunctionBody:changed` entry rather than unrelated physical remove/add events.
Content-identical records remain `unchanged` even when Pack layout differs.
Unknown future payload schemas are not guessed into a semantic owner and fall
back to exact immutable RecordId identity. Existing module, record-kind, and
stable-key filters constrain the diff without changing the inspected files.

`--explain STABLE_KEY` reads already-decoded persisted semantic dependency edges
and emits a deterministic, RecordId-bounded tree. Resolved script module/type/
function targets link to their stable semantic owner; cycles are marked and stop
recursion. Targets not represented by the common decoder (for example detailed
global/property/environment authorities) remain explicitly `unresolved` rather
than being guessed or reconstructed from source.

This is a physical-integrity and diagnostic decoder, not a replacement for the
Runtime's complete semantic and graph validation. An unsupported record payload
schema is labelled `decoder_scope: unavailable`; it is never presented as a
successfully decoded semantic record. Malformed or trailing data inside a
supported schema is instead reported as a structured error with a nonzero
process exit code. The Runtime remains authoritative for derived-hash, current
environment ABI, ownership, relocation and whole-generation graph validation.

Run the standalone tests through the repository wrapper:

```powershell
Tools\RunCacheV2DumpTests.ps1
```
