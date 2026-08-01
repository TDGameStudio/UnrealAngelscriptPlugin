# Standalone third-party sources

This directory contains reviewed, pinned AngelScript add-on sources used by the
standalone compiler. It deliberately does **not** contain another copy of the
AngelScript engine. The engine is always built from the maintained fork under
`Source/AngelscriptRuntime/ThirdParty/angelscript`.

## Provenance

- Upstream: `git@github.com:anjo76/angelscript.git`
- Release: AngelScript `v2.38.0`
- Revision: `0601da029d846a658bf23f2888e953a45a94450a`
- Local audit source: `Reference/angelscript-v2.38.0`
- Imported source root: `sdk/add_on`

`Reference/angelscript-v2.38.0` is an offline development reference only. It is
not a build, runtime, packaging, or release dependency.

The following files were imported:

| Imported file | Upstream SHA-256 |
| --- | --- |
| `AngelScriptAddons/scriptstdstring/scriptstdstring.cpp` | `83397E22AF59DD2BCE2854BEB0825F56DE9FF60658BDD3A2AC55784DE7B2A42E` |
| `AngelScriptAddons/scriptstdstring/scriptstdstring.h` | `56D67F8D84477841EDF63D1F2E65BABC0457905474574596FA071A5AA9FF75B0` |
| `AngelScriptAddons/scriptstdstring/scriptstdstring_utils.cpp` | `8E48DB8C5F9E17DA8DF1AA52C4FE1E5AA668FCD5B23FC7BA88A1962EA069660E` |
| `AngelScriptAddons/scriptarray/scriptarray.cpp` | `953C6C389CC0DDC150A730538A5980F3BCDB9366F9D4AF98AF93CBEEF9EDF917` |
| `AngelScriptAddons/scriptarray/scriptarray.h` | `EB83C6117702F54E65156536CDE0EC8AF2DB836415BEFF485A2DE9FF2592018B` |
| `AngelScriptAddons/scriptdictionary/scriptdictionary.cpp` | `07E242CB4F08B9BB1F1949A39B618AA461CC2AD8B0DF86ECBB4713A3C8F7D760` |
| `AngelScriptAddons/scriptdictionary/scriptdictionary.h` | `E0662AE1BBF5315F5FF06ABAAF60A31E172A5F37E3B65243D3757A6CF3978CBC` |
| `AngelScriptAddons/scriptmath/scriptmath.cpp` | `BEBCCAC9E179C3BB72B9A64EB384F22C9E1B4A13B42D99C1AFA97AF48119CB0F` |
| `AngelScriptAddons/scriptmath/scriptmath.h` | `26850EEC4C7A319D551D0356BA2D2D197BE0C16D1A336B590570F67BDB851CE9` |

The hashes describe the pristine upstream inputs, not the maintained files
after the adaptations below. `scriptmathcomplex` is intentionally excluded
because it has no generic registration path under `AS_MAX_PORTABILITY`.

## Adaptation ledger

These are altered sources and must not be represented as the original
AngelScript distribution.

| File | Maintained delta |
| --- | --- |
| `scriptstdstring/scriptstdstring.h` | Marked as an altered maintained source. Adds the add-on-local `TScriptStdAllocator`, counted `scriptstring_t`, and stable string hasher; these types route script-owned character and container storage through `asAllocMem`/`asFreeMem` without changing the maintained fork API. |
| `scriptstdstring/scriptstdstring.cpp` | Marked as an altered maintained source. Replaced both `GetStringFactory()` calls with the maintained 2.33-compatible `GetStringFactoryReturnTypeId()` API. Normalized public registration declarations from ambiguous `float`/`double` to the fork's explicit `float32`/`float64` names. The two upstream variadic `scan`/`format` registrations are not exposed because this fork has no variadic declaration token; fixed-signature format/parse helpers remain available. String characters, formatting streams, cache nodes, and the engine-owned string factory use the counted allocation boundary. `regexFind` is intentionally not registered because `std::regex` has no complete transitive allocator control. |
| `scriptarray/scriptarray.cpp` | Marked as an altered maintained source. Adapted its allocator calls to the fork's alignment-aware `asALLOCFUNC_t` contract while keeping the public `asAllocMem` default. Adapted the generic template callback declaration, including required parameter names, to this fork's `int&out ErrorMessage` slot; the add-on retains its own validation diagnostics and does not write the fork-internal string slot. The generic profile uses the fork's implicit-handle syntax because explicit `@` tokens are intentionally unavailable, removes the unsupported upstream `explicit` factory suffix, and makes the array-to-array insert parameter explicitly `&in`. Upstream generic list construction is deliberately not registered because its two-parameter ABI is incompatible with the fork's one-buffer list-factory ABI; default construction and mutation remain available. |
| `scriptstdstring/scriptstdstring_utils.cpp` | Marked as an altered maintained source. Adapted the generic `split` return declaration to the fork's implicit-handle array syntax and uses the counted script string type. |
| `scriptdictionary/scriptdictionary.h` / `.cpp` | Marked as altered maintained sources. Adapted generic dictionary/iterator reference types, factories, and methods to the fork's implicit-handle syntax. Upstream generic dictionary list construction is deliberately not registered for the same list-factory ABI reason as array. Normalized registration declarations to `float32`/`float64`. Dictionary keys, map nodes, cache, objects, and iterators use the counted AngelScript allocation boundary. |
| `scriptmath/scriptmath.cpp` | Marked as an altered maintained source. Normalized registration declarations to the fork's unambiguous `float32`/`float64` names; C++ implementation types remain `float`/`double`. |

Later allocator, UTF-8, cancellation, and exposed-surface changes must be added
to this table as they land.

## RapidJSON

- Upstream: `https://github.com/Tencent/rapidjson`
- Pinned release: `v1.1.0`
- Local source input: Unreal Engine 5.8
  `Engine/Source/ThirdParty/RapidJSON/1.1.0/rapidjson`
- Imported destination: `RapidJSON/include/rapidjson`
- License: MIT; retained verbatim as `RapidJSON/LICENSE.txt`

RapidJSON is header-only and is exposed only through the standalone CMake
host target. Unreal Build Tool modules do not include or link this vendored
copy. It parses the small manifest as a DOM and one JSONL record at a time;
the bundle loader never builds a DOM for an entire symbol or asset file.

## AngelScript license notice

The plugin's own source is licensed under `Plugins/Angelscript/LICENSE.md`.
The copied add-on sources retain AngelScript's zlib-style license:

```text
AngelCode Scripting Library
Copyright (c) 2003-2025 Andreas Jonsson

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any
damages arising from the use of this software.

Permission is granted to anyone to use this software for any
purpose, including commercial applications, and to alter it and
redistribute it freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you
   must not claim that you wrote the original software. If you use
   this software in a product, an acknowledgment in the product
   documentation would be appreciated but is not required.

2. Altered source versions must be plainly marked as such, and
   must not be misrepresented as being the original software.

3. This notice may not be removed or altered from any source
   distribution.

The original version of this library can be located at:
http://www.angelcode.com/angelscript/

Andreas Jonsson
andreas@angelcode.com
```
