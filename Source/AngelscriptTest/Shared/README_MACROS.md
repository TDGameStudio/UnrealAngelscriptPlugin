# Angelscript Test Macros

Chinese: `README_MACROS_ZH.md`

## Current Macros (defined in `AngelscriptTestMacros.h`)

| Macro | Purpose |
|-------|---------|
| `ASTEST_CREATE_ENGINE()` | Shared engine with reset (use in BEFORE_ALL) |
| `ASTEST_GET_ENGINE()` | Shared engine without reset (use in TEST_METHOD) |
| `ASTEST_CREATE_ENGINE_FULL()` | Fresh isolated full engine |
| `ASTEST_CREATE_ENGINE_NATIVE()` | Raw asIScriptEngine* from SDK |
| `ASTEST_RESET_ENGINE(Engine)` | Reset shared engine (use in AFTER_ALL) |
| `ASTEST_AS(SourceLiteral)` | Normalize indented inline AngelScript raw string literals to `FString` |
| `ASTEST_AS_ANSI(SourceLiteral)` | Normalize indented inline AngelScript raw string literals to UTF-8 `std::string` |

## Legacy Macros (defined in `AngelscriptTestLegacyHelpers.h`)

These are deprecated and only used by ~11 old `IMPLEMENT_SIMPLE_AUTOMATION_TEST` files:

- `ASTEST_COMPILE_RUN_INT` / `ASTEST_COMPILE_RUN_INT64` / `ASTEST_BUILD_MODULE`

They use `return false` internally and are incompatible with CQTest. Do not use in new tests.

## See Also

- Full guide: `TESTING_GUIDE.md`
- CQTest template: `Template/Template_CQTest.cpp`
- PIE template: `Template/Template_PIE.cpp`
- Multiplayer PIE template: `Template/Template_MultiplayerPIE.cpp`
