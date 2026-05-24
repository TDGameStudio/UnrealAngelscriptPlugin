# Angelscript 测试指南

## 宏速查

所有宏定义在 `Shared/AngelscriptTestMacros.h`：

| 宏 | 返回值 | 用途 |
|---|---|---|
| `ASTEST_CREATE_ENGINE()` | `FAngelscriptEngine&` | 共享引擎，自动 reset。用于 `BEFORE_ALL()` |
| `ASTEST_GET_ENGINE()` | `FAngelscriptEngine&` | 共享引擎，不 reset。用于 `TEST_METHOD()` |
| `ASTEST_CREATE_ENGINE_FULL()` | `FAngelscriptEngine&` | 独立完整引擎。用于热重载、绑定环境测试 |
| `ASTEST_CREATE_ENGINE_NATIVE()` | `asIScriptEngine*` | 原生 SDK 引擎。用于 SDK API 测试 |
| `ASTEST_RESET_ENGINE(Engine)` | void | 重置共享引擎。用于 `AFTER_ALL()` |

## CQTest 标准写法（推荐）

```cpp
#include "CQTest.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Shared/AngelscriptTestModuleScope.h"
#include "Shared/AngelscriptTestExecute.h"

TEST_CLASS_WITH_FLAGS(FMyTest,
    "Angelscript.TestModule.Category.Feature",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
    BEFORE_ALL()  { ASTEST_CREATE_ENGINE(); }

    AFTER_ALL()
    {
        FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
        ASTEST_RESET_ENGINE(Engine);
    }

    TEST_METHOD(BasicCase)
    {
        FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
        FAngelscriptEngineScope Scope(Engine);
        FScopedAngelscriptModule Mod(*TestRunner, Engine, TEXT("ASCategoryFeature_Basic"), TEXT(R"(
int GetValue() { return 42; }
)"));
        if (!Mod.IsValid()) return;
        auto& M = Mod.GetModule();
        ExpectGlobalInt(*TestRunner, Engine, M,
            TEXT("int GetValue()"), TEXT("Returns 42"), 42);
    }
};
```

要点：
- `BEFORE_ALL` 用 `CREATE`（带 reset），`TEST_METHOD` 用 `GET`（不 reset）
- `FScopedAngelscriptModule` 负责每个测试方法的模块隔离
- 断言函数传 `*TestRunner`（不是 `*this`）
- 新代码直接 include `AngelscriptTestModuleScope.h` / `AngelscriptTestExecute.h`，勿依赖 `AngelscriptBindingsAssertions.h` 等转发头

## 测试 helper 放哪里

| 条件 | 位置 | 示例 |
|------|------|------|
| ≥2 个主题目录共用 | `Shared/*.h` | `FAngelscriptTestExecutor`、`BuildModule` |
| 仅 `Bindings/` 内 ≥2 个 `.cpp` | `Bindings/Angelscript*TestHelpers.h` | `AngelscriptTArrayBindingsTestHelpers.h` |
| 仅单个 `.cpp` | 保留 `AngelscriptTest_<File>_Private` | ReflectiveFallback 缓存探测 |
| 大文件按 Section 拆 | `Bindings/*Sections.h` | Console 簇 |

参考形态：`Bindings/AngelscriptQuatBindingsTests.cpp`。Bindings 模块头清单见 `Shared/README.md` §「Bindings Execute migration」。

## 选择哪种引擎

```
热重载 / 绑定环境 / GC 测试？  --> ASTEST_CREATE_ENGINE_FULL()
AngelScript SDK API 测试？      --> ASTEST_CREATE_ENGINE_NATIVE()
其他所有（绑定、语法、编译器）？ --> CREATE + GET 标准模式
```

## 详细说明和模板

- 完整英文指南：`TESTING_GUIDE.md`
- CQTest 教学模板：`Template/Template_CQTest.cpp`
- Shared 布局与 Bindings 迁移记录：`Shared/README.md`
- 宏定义：`Shared/AngelscriptTestMacros.h`
