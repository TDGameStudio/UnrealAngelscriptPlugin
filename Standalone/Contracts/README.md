# Packaged UE contracts

`UE5.8/default-engine.zip` contains the one complete offline contract shipped
with Standalone. It is generated from the checked-in `AngelscriptProject`
host under Unreal Engine 5.8, including the project's normally enabled
plugins and `/Game` asset scope.

The archive is source-storage only. CMake extracts and verifies its
`manifest.json`, `symbols.jsonl`, and `assets.jsonl`; build and install trees
always expose the normal directory contract at `contracts/default-engine/`.
Standalone never reads or merges the ZIP at runtime.

Regenerate the raw bundle twice with the plugin-owned
`AngelscriptOfflineExport` Commandlet, verify that all three output files are
byte-identical, then replace the ZIP. The expected per-file SHA-256 values in
`Standalone/CMakeLists.txt` must be updated in the same change.
