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
#include "Shared/AngelscriptBindingsModuleBuilder.h"
#include "Shared/AngelscriptBindingsAssertions.h"

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
        FCoverageModuleScope Mod(*TestRunner, Engine, TEXT("ASCategoryFeature_Basic"), TEXT(R"(
int GetValue() { return 42; }
)"));
        if (!Mod.IsValid()) return;
        ExpectGlobalInt(*TestRunner, Engine, Mod.GetModule(), 
            TEXT("int GetValue()"), TEXT("Returns 42"), 42);
    }
};
```

要点：
- `BEFORE_ALL` 用 `CREATE`（带 reset），`TEST_METHOD` 用 `GET`（不 reset）
- `FCoverageModuleScope` 负责每个测试方法的模块隔离
- 断言函数传 `*TestRunner`（不是 `*this`）

## 选择哪种引擎

```
热重载 / 绑定环境 / GC 测试？  --> ASTEST_CREATE_ENGINE_FULL()
AngelScript SDK API 测试？      --> ASTEST_CREATE_ENGINE_NATIVE()
其他所有（绑定、语法、编译器）？ --> CREATE + GET 标准模式
```

## 详细说明和模板

- 完整英文指南：`TESTING_GUIDE.md`
- CQTest 教学模板：`Template/Template_CQTest.cpp`
- 宏定义：`Shared/AngelscriptTestMacros.h`
