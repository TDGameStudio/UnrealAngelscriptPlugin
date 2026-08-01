# Frozen producer fixtures

These `default-engine` and `project` bundles are emitted by the Unreal
`FAngelscriptOfflineBundleWriter` through
`ProjectAndDefaultBundlesPublishAtomically` on 2026-07-31. They are committed
as the producer/consumer compatibility boundary for offline contract schema
`1.0`; standalone tests must consume the bytes as-is rather than reconstructing
the manifest independently. The fixture producer uses the supported
`2.33+selective-2.38` fork and `ue-as-standalone-v1` compiler-contract values.

The fixtures intentionally contain one declaration-only manual type and no
assets. The bundle kinds and bundle identities differ, while the canonical
symbol file is identical. Runtime engine/project exports are covered
separately and are not committed because they contain the complete loaded
symbol surface.
