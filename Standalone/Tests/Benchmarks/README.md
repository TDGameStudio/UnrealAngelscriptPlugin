# Standalone benchmark contract

`AngelscriptStandalone.Benchmarks` collects eleven samples for cold engine
create/shutdown, native compile, native compile-run, bundle load/index, UE
core analysis, template analysis, and resource analysis. It writes
`benchmark-results.csv` and
`benchmark-results.json` beneath the ignored CMake build directory.

The comparison rule is intentionally narrow:

- environment identity, compiler/configuration, profile hashes, corpus and
  bundle identity must match;
- comparable medians fail when the current value is more than 20% above the
  accepted baseline;
- mismatched environments are reported as `incomparable`, never pass/fail;
- elapsed times and peak memory are evidence only and never enter compile or
  artifact identity.

Raw machine results are not committed as portable thresholds. A release
verification record captures the command, commit, environment identity,
sample count, medians, peak memory, modules and corpus revision.
