# Angelscript 测试宏说明

## 当前宏（定义在 `AngelscriptTestMacros.h`）

| 宏 | 用途 |
|---|---|
| `ASTEST_CREATE_ENGINE()` | 共享引擎 + reset（BEFORE_ALL 用） |
| `ASTEST_GET_ENGINE()` | 共享引擎，不 reset（TEST_METHOD 用） |
| `ASTEST_CREATE_ENGINE_FULL()` | 独立完整引擎 |
| `ASTEST_CREATE_ENGINE_NATIVE()` | 原生 SDK 引擎 |
| `ASTEST_RESET_ENGINE(Engine)` | 重置共享引擎（AFTER_ALL 用） |
| `ASTEST_AS(SourceLiteral)` | 将缩进后的内联 AngelScript raw string 规范化为 `FString` |
| `ASTEST_AS_ANSI(SourceLiteral)` | 将缩进后的内联 AngelScript raw string 规范化为 UTF-8 `std::string` |

## 废弃宏（定义在 `AngelscriptTestLegacyHelpers.h`）

仅供约 11 个旧式 `IMPLEMENT_SIMPLE_AUTOMATION_TEST` 文件使用，新测试不要用。

## 参考

- 完整指南：`TESTING_GUIDE.md` / `TESTING_GUIDE_ZH.md`
- CQTest 模板：`Template/Template_CQTest.cpp`
- PIE 模板：`Template/Template_PIE.cpp`
- 多人 PIE 模板：`Template/Template_MultiplayerPIE.cpp`
