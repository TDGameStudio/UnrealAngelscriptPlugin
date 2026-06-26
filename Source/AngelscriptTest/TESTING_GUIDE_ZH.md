# Angelscript 测试指南

本文是 `Plugins/Angelscript/Source/AngelscriptTest/` 下新增或重构 C++ 自动化测试的本地速查。`Documents/UnitTest/UnitTest.md` 是最新的单元测试风格规范。更完整的测试层级、运行方式和内联 AS 代码格式分别见：

- `Documents/Guides/Test.md`
- `Documents/Guides/TestConventions.md`
- `Documents/Rules/ASInlineFormattingRule.md`
- `Plugins/Angelscript/Source/AngelscriptTest/Template/`

## 宏速查

所有宏定义在 `Shared/AngelscriptTestMacros.h`：

| 宏 | 返回值 | 用途 |
|---|---|---|
| `ASTEST_CREATE_ENGINE()` | `FAngelscriptEngine&` | 共享引擎，重置到干净状态。用于 `BEFORE_ALL()` |
| `ASTEST_GET_ENGINE()` | `FAngelscriptEngine&` | 已存在的共享引擎，不 reset。用于 `TEST_METHOD()` |
| `ASTEST_CREATE_ENGINE_FULL()` | `FAngelscriptEngine&` | 独立完整引擎。用于需要隔离的热重载、绑定环境或 GC 测试 |
| `ASTEST_CREATE_ENGINE_NATIVE()` | `asIScriptEngine*` | 原生 AngelScript SDK 引擎。用于 SDK API 测试 |
| `ASTEST_RESET_ENGINE(Engine)` | void | 重置共享引擎。用于 `AFTER_ALL()` |

## CQTest 结构规则

新增测试默认使用 CQTest（`TEST_CLASS_WITH_FLAGS`），除非相邻现有测试有明确理由继续使用 `IMPLEMENT_SIMPLE_AUTOMATION_TEST`。

测试实现应留在所属 test class 内：

- `TEST_METHOD` 直接表达测试流程。
- 只被该测试类使用的常量、窄 helper、观察结构体放在同一个测试类的 `private:` 下。
- 不要为了单个 CQTest class 创建匿名 namespace。
- 不要把主要测试流程移到类外 static 函数，再让 `TEST_METHOD` 只做一层转发。
- 不要用文件级 `#define TestTrue(...)` / `#define TestEqual(...)` 这类断言别名包裹 CQTest 断言。

推荐形态：

```cpp
#include "CQTest.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestExecute.h"

TEST_CLASS_WITH_FLAGS(FExampleHotReloadTest,
	"Angelscript.TestModule.HotReload.Example",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FReloadObservation
	{
		int32 PostReloadCount = 0;
	};

public:
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

	TEST_METHOD(BroadcastsPostReload)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FReloadObservation Observation;
		// 编译、reload、断言。
	}
};
```

避免：

```cpp
namespace ExampleTest_Private
{
	static bool RunBroadcastsPostReload(FAutomationTestBase& Test);
}

TEST_METHOD(BroadcastsPostReload)
{
	ASSERT_THAT(IsTrue(ExampleTest_Private::RunBroadcastsPostReload(*TestRunner)));
}
```

### Engine 生命周期

每个 CQTest class 应在 `BEFORE_ALL()` 创建共享测试引擎，在 `AFTER_ALL()` 重置。单个 `TEST_METHOD` 只获取已创建的引擎：

```cpp
BEFORE_ALL()
{
	ASTEST_CREATE_ENGINE();
}

AFTER_ALL()
{
	FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
	ASTEST_RESET_ENGINE(Engine);
}

TEST_METHOD(MyCase)
{
	FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
	FAngelscriptEngineScope Scope(Engine);
}
```

规则：

- 不要在每个 `TEST_METHOD` 中调用 `ASTEST_CREATE_ENGINE()`。
- 不要在每个 `TEST_METHOD` 的 `ON_SCOPE_EXIT` 中调用 `ASTEST_RESET_ENGINE(Engine)`。
- 每个 `TEST_METHOD` 自己清理它编译的 AS module、注册的 delegate handle、创建的 transient object。
- 如果测试确实需要独立 full engine，使用 `ASTEST_CREATE_ENGINE_FULL()`，并显式 drain 测试期间创建的 module。
- 如果测试类中有 `private:` helper，必须在 `BEFORE_ALL()`、`AFTER_ALL()` 和 `TEST_METHOD` 前恢复 `public:`，保证 CQTest 能注册这些 hook。

### Full Engine 写法

需要完整隔离时使用，常见于热重载、绑定环境和 GC：

```cpp
TEST_METHOD(IsolatedTest)
{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
	FAngelscriptEngineScope Scope(Engine);
	ON_SCOPE_EXIT
	{
		for (const auto& Module : Engine.GetActiveModules())
		{
			Engine.DiscardModule(*Module->ModuleName);
		}
	};

	// 测试代码。
}
```

### Native SDK 写法

只用于直接验证 AngelScript SDK API。`AngelScriptSDK/` 下的 native 测试不要引入 `FAngelscriptEngine`。

```cpp
TEST_METHOD(SDKTest)
{
	asIScriptEngine* NativeEngine = ASTEST_CREATE_ENGINE_NATIVE();
	ASSERT_THAT(IsNotNull(NativeEngine, TEXT("Native AngelScript engine should be created")));
	ON_SCOPE_EXIT { NativeEngine->ShutDownAndRelease(); };

	asCScriptEngine* BareEngine = reinterpret_cast<asCScriptEngine*>(NativeEngine);
	// Raw SDK 测试代码。
}
```

选择规则：

```text
热重载 / 绑定环境 / GC 测试？
  -> ASTEST_CREATE_ENGINE_FULL()

AngelScript SDK API 测试？
  -> ASTEST_CREATE_ENGINE_NATIVE()

其他所有，包括 bindings、syntax、compiler、functional？
  -> BEFORE_ALL() 中 ASTEST_CREATE_ENGINE() + TEST_METHOD() 中 ASTEST_GET_ENGINE()
```

## Bindings 测试组织

Bindings/CQTest 文件里如果一个类型有多个独立覆盖面，应拆成多个场景化 `TEST_METHOD`，不要把所有 section 聚合到一个 `Compat` / `OptionalCompat` 方法里。

推荐拆分维度：

- baseline 或 compat 行为。
- type matrix。
- API entry-point coverage。
- null / boundary / exception 场景。
- return-type 或 log diagnostic 这类专门路径。

规则：

- `TEST_METHOD` 名称必须说明场景。
- `FScopedAngelscriptModule` 应在对应 `TEST_METHOD` 内创建，module name 与场景名对应。
- `ExpectGlobalInts` / `Execute...` 等 helper 的返回值必须被 `ASSERT_THAT(IsTrue(...))` 或同等级断言消费。
- 允许保留文件级 native bind 注册对象，例如 `AS_FORCE_LINK const FAngelscriptBinds::FBind ...`，因为这类对象必须在 AS bind 初始化期注册；但测试流程、fixture 和断言仍应留在 `TEST_CLASS_WITH_FLAGS` 内。

## 内联 AngelScript Fixture 规则

C++ 测试中的内联 AngelScript 代码必须使用 `ASTEST_AS(R"AS(... )AS")` 包裹。ASSDK/raw SDK 需要 `const char*` 或 `std::string` 时使用 `ASTEST_AS_ANSI(...)`。

不要把视觉缩进的 raw string 直接传给编译 helper：

```cpp
const FString Source = TEXT(R"AS(
	UCLASS()
	class AMyActor : AActor
	{
	}
	)AS");
```

应写成：

```cpp
const FString Source = ASTEST_AS(R"AS(
	UCLASS()
	class AMyActor : AActor
	{
	}
	)AS");
```

### Fixture 靠近测试

测试专用 AS 源码优先作为局部变量放在使用它的 `TEST_METHOD` 内，而不是做成文件级 `GetXxxScriptV1()` / `GetXxxScriptV2()` helper。这样 fixture、编译/reload 步骤和断言能保持阅读顺序。

例外：

- 多个 `TEST_METHOD` 共享同一大段 fixture，且共享语义稳定。
- 需要按参数生成 fixture，且生成逻辑本身比重复源码更清楚。
- helper 名称表达的是测试领域概念，而不只是版本化源码 getter。

一个测试里有多个 AS 片段时，变量名必须说明版本或场景，例如：

- `ReloadV1Source`
- `ReloadV2Source`
- `DelegateSignatureV1Source`
- `DelegateSignatureV2Source`
- `TypeReloadV1Source`
- `TypeReloadV2Source`

避免 `Script1`、`Script2`、`Text` 和泛泛的 `Source`。`ScriptSource` 只适合一个测试里只有一段明显脚本源码的场景。

内联 AS 代码也要保持可读：

- AS 内容和 closing delimiter 都不能从 column 0 开始。
- `UCLASS()` / `USTRUCT()` / `delegate` 代码跟随 C++ 嵌入缩进。
- `{` 使用 Allman brace，独占一行。
- 多个 `UCLASS` / `USTRUCT` / 函数之间保留一个空行。
- `UPROPERTY()` + 声明之后如果还有下一个成员，保留一个空行。

## Hot Reload 测试规则

HotReload 测试不要只验证“编译成功”，应覆盖 reload 过程的外部可观察行为。至少明确断言以下一种：

- reload delegate 是否广播。
- old/new class、struct、delegate、enum 是否可见且不同。
- reload 后 generated class / struct / enum / delegate 是否仍可查询。
- Blueprint 子类、实例、CDO 或 property 是否仍指向正确的新类型。
- reload 后 property、function、delegate signature 是否完成 retarget。

每个 `TEST_METHOD` 必须管理自己创建的 module 和 delegate handle：

```cpp
ON_SCOPE_EXIT
{
	Engine.GetOnDelegateReload().Remove(DelegateReloadHandle);
	Engine.DiscardModule(*ModuleName.ToString());
};
```

清理应在可能 early-return 的成功路径操作之前注册。

### Delegate Hot Reload

AngelScript 中 `delegate` 声明本身不是 `UPROPERTY`。可以标记为 `UPROPERTY` 的是使用该 delegate 类型的类成员：

```angelscript
delegate void FHotReloadSignal(int Value);

UCLASS()
class UHotReloadDelegateSignatureCarrier : UObject
{
	UPROPERTY()
	FHotReloadSignal Signal;
}
```

这个成员会生成 `FDelegateProperty`，其 `SignatureFunction` 指向对应的 `UDelegateFunction`。因此 delegate signature hot reload 测试可以验证两层行为：

- `GetOnDelegateReload()` 广播 old/new `UDelegateFunction`。
- 使用该 delegate 的 `FDelegateProperty::SignatureFunction` reload 后 retarget 到新的 signature function。

如果测试目标只是 delegate reload 广播，可以不放 `UPROPERTY` 成员；如果测试目标包含属性 retarget，则必须保留。

Delegate hot reload runtime 覆盖应按测试目标覆盖相关路径：

- `UPROPERTY` delegate 成员：创建 AS 父类、创建 transient Blueprint 子类、生成 Blueprint 实例，reload 后验证 `FDelegateProperty::SignatureFunction`、property flags 和实例行为。
- 运行前后对比：先执行 V1 行为，再 reload，再执行 V2/V3 行为，断言返回值或 reflected state 变化。
- Blueprint 运行态：参考 `Template_BlueprintWorldTick.cpp` / `FAngelscriptTestWorld`，让 Blueprint child 经历 `BeginPlay` / `Tick`，在 actor 已经运行后 reload，再继续 tick 或调用函数。
- 全局函数运行态：参考 `Template_GlobalFunctions.cpp`，使用 `FAngelscriptTestExecutor::ExecuteAndGet` 调用 AS global function，覆盖 reload 前后全局函数绑定 delegate 的行为。
- property flag 切换：`NotEditable` / `EditAnywhere` / `BlueprintReadWrite` 等 specifier 应走 full reload，并断言 `CPF_Edit` / `CPF_BlueprintVisible` 等实际 flag。
- 参数或签名变化：delegate 参数变化应使用 full reload，断言新的 `UDelegateFunction` 参数存在，并在 reload 后实际执行新签名路径。

注意：当前 AS property 的默认 Blueprint specifier 是 `BlueprintReadWrite`。`UPROPERTY(NotEditable)` 只关闭 `CPF_Edit` 对应的编辑能力，不会关闭 `CPF_BlueprintVisible`；如果测试要验证 Blueprint 不可见，需要显式使用对应的 Blueprint specifier 或调整默认配置。

推荐把不同 reload 语义拆开：

- V1：baseline，创建对象/Blueprint/运行态实例。
- V2：只改函数体或 delegate handler body，使用 `ECompileType::SoftReloadOnly`，验证 live actor 继续运行新行为。
- V3：改 `UPROPERTY` flags 或 reflected surface，使用 `ECompileType::FullReload`，验证 Blueprint 可重新编译并能创建新实例。
- V4：改 delegate signature 或参数形状，使用 `ECompileType::FullReload`，验证 property signature retarget 和新签名运行行为。

Blueprint 子类 hot reload 测试要包含普通 `UBlueprintGeneratedClass`，不能只覆盖 `UASClass`。之前的崩溃模式是 hot reload 遍历 Blueprint generated class 时把普通 Blueprint generated class 当成 `UASClass` 使用。

相关回归形态：

- 创建 AS class。
- 创建 transient Blueprint 子类。
- 修改 AS class 的 reflected surface，例如新增 `UPROPERTY(EditAnywhere)`。
- 执行 reload。
- 断言不会崩溃，Blueprint generated class 仍是 AS class 的子类。

## AS USTRUCT 参数回归规则

AS 定义的 `USTRUCT` 作为 delegate 或 `UFUNCTION` 参数时，不只验证编译成功或 reload 元数据，必须至少有一个真实执行测试覆盖参数传递路径：

- 创建 AS `USTRUCT`。
- 定义 delegate 或 `UFUNCTION` 使用该 struct 参数。
- 绑定真实 receiver。
- 执行 delegate 或反射调用函数。
- 断言 struct 字段值真实传入并返回预期结果。

这个路径会同时经过 `FScriptCall::PushArgument`、UE event argument buffer、`FFrame::StepCompiledInRef`、`FUStructType::SetArgument`、`UScriptStruct::InitializeStruct` 和 `UASStruct` 的 CppStructOps fake-vtable 回调。只做编译测试无法覆盖 Unreal 拥有目标内存时的 struct 生命周期。

`fix-script-struct-delegate-argument-crash` 记录了一个 AS `USTRUCT` delegate 参数崩溃根因：

- AS 源码里的 by-value struct 参数会在 UE 反射签名中表现为 `const FStructName&`。
- 当 delegate 执行进入 `FUStructType::SetArgument` 时，会调用 `UScriptStruct::InitializeStruct` 构造 event 参数 buffer。
- UE 5.8 的 fake-vtable 回调签名是 `Construct(void*)`、`Destruct(void*)`、`Copy(void*, const void*, int32)` 等，不会把 `ICppStructOps*` 作为第一个参数传入。
- 旧的 `FASStructOps` 回调把第一个参数声明成 `FASStructOps*`，导致 Unreal 传入的目标地址被误当成 ops 指针，后续访问崩溃。
- 相关修复使用 AS struct value header 记录 `ScriptType` / `CppStructOps`，并通过 `UASStruct::InitializeStruct` 注入第一次构造所需的 ops 上下文。

新增此类测试时，要优先写 focused runtime regression，再让更大的 hot reload 测试复用同类参数形态。focused 用例确认 runtime 生命周期，hot reload 用例确认重载后类型迁移。

清理规则：

- 注册 delegate handle 后必须 remove。
- 编译 module 后必须 discard。
- 多个 module 用多个明确的 module name，或在 full engine 测试中统一 drain。
- 清理写在成功路径之前，避免中途 `return` 泄漏状态。

## CQTest 断言和 helper 边界

新增或重构 CQTest 时，`TEST_METHOD` 主流程优先使用 matcher 断言：

- `ASSERT_THAT(AreEqual(Expected, Actual, TEXT("...")))`
- `ASSERT_THAT(AreNotEqual(Expected, Actual, TEXT("...")))`
- `ASSERT_THAT(IsTrue(Value, TEXT("...")))`
- `ASSERT_THAT(IsFalse(Value, TEXT("...")))`
- `ASSERT_THAT(IsNotNull(Value, TEXT("...")))`
- `ASSERT_THAT(IsNull(Value, TEXT("...")))`

避免在新 CQTest 主流程中继续使用：

- `TestRunner->TestEqual`
- `TestRunner->TestTrue`
- `TestRunner->TestFalse`
- `TestRunner->TestNotNull`
- `TestRunner->TestNull`
- `TestRunner->TestNotEqual`

如果 helper 必须返回 `bool` 给 `ASSERT_THAT(IsTrue(...))`，可以在 helper 内创建局部 `FNoDiscardAsserter`：

```cpp
static bool ExpectNotNull(FAutomationTestBase& Test, UObject* Value, const TCHAR* Message)
{
	FNoDiscardAsserter LocalAssert(Test);
	return LocalAssert.IsNotNull(Value, Message);
}
```

这类 helper 只用于消除重复噪音，不应隐藏测试主流程。

CQTest 中 `TestRunner` 是静态指针。传给需要 `FAutomationTestBase&` 的 helper 时必须使用 `*TestRunner`：

```cpp
CompileScriptModule(*TestRunner, Engine, ModuleName, Filename, Source);
```

不要传：

```cpp
CompileScriptModule(TestRunner, Engine, ModuleName, Filename, Source);
```

可以抽 helper 的情况：

- 纯查找或转换逻辑。
- 观察结构体，例如 reload delegate 计数。
- 多处重复的安全清理。
- 单个测试类私有、不会跨主题复用的小函数。

不建议抽 helper 的情况：

- 把完整 `TEST_METHOD` 主流程移到类外。
- 把 V1/V2 AS fixture 移到 `Get...ScriptV1()` 这类 getter。
- helper 同时做 compile、reload、assert，导致 `TEST_METHOD` 只剩一行。

测试读者应能在 `TEST_METHOD` 里直接看到：准备 fixture、编译 V1、建立观察点、reload V2、断言结果。

## 测试 helper 放哪里

| 条件 | 位置 | 示例 |
|---|---|---|
| 两个或更多主题目录共用 | `Shared/*.h`；include 时直接写 `AngelscriptTestExecute.h` 等 | `BuildModule`、`FAngelscriptTestExecutor`、`ExecuteAndExpectInt` |
| 仅 `Bindings/` 内两个或更多 `.cpp` 共用 | `Bindings/Angelscript*TestHelpers.h` | `Bindings/AngelscriptTArrayBindingsTestHelpers.h` |
| 仅单个 CQTest `.cpp` 使用 | 所属 `TEST_CLASS_WITH_FLAGS` 的 `private:` 区域 | 嵌套 fixture、观察结构体、本地常量 |
| 大型 bindings 文件按 section 拆分 | `Bindings/*Sections.h` + 主 `.cpp` | Console 簇 |

`AngelscriptTest.Build.cs` 已把 `Shared/` 加入 `PrivateIncludePaths`，所以新测试直接 `#include "AngelscriptTestExecute.h"`，不要写 `#include "Shared/..."`。`Bindings/` 下的 header 仍使用 `Bindings/` include 前缀。

## 命名约定

| 类别 | 模式 | 示例 |
|---|---|---|
| 文件名 | `Angelscript[Theme]Tests.cpp` | `AngelscriptControlFlowTests.cpp` |
| 测试路径 | `Angelscript.TestModule.[Theme].[Feature]` | `Angelscript.TestModule.Syntax.CQTest` |
| Module 前缀 | `AS[Theme][Feature]` | `ASControlFlowForLoop` |

## 基础设施文件

| 文件 | 用途 |
|---|---|
| `AngelscriptTestMacros.h` | 新测试使用的 engine 宏和内联 AS source 包装 |
| `AngelscriptTestLegacyHelpers.h` | 旧 `COMPILE_RUN` / `BUILD_MODULE` 宏；新测试不再使用 |
| `AngelscriptTestUtilities.h` | Engine 创建/销毁工具 |
| `AngelscriptTestEnginePool.h` | Module-clean engine pool 和 `FScopedModuleCleanEngine` |
| `AngelscriptTestEngineHelper.h` | Compile/reload helper 函数 |
| `AngelscriptTestExecute.h` | 标准 `FAngelscriptTestExecutor`、`ExecuteAndExpect*`、`ExpectGlobalInt` 和 `FASGlobalFunctionInvoker` alias |
| `AngelscriptTestModuleScope.h` | 显式 module name + source 的 `FScopedAngelscriptModule` |
| `AngelscriptBindingsAssertions.h` | 转发到 `AngelscriptTestExecute.h` 的兼容 shim |
| `AngelscriptBindingsModuleBuilder.h` | 转发到 `AngelscriptTestModuleScope.h` 的兼容 shim |
| `AngelscriptGlobalFunctionInvoker.h` | 转发到 `AngelscriptTestExecute.h` 的兼容 shim |
| `Bindings/Angelscript*TestHelpers.h` | 仅 Bindings 共享的 helper |
| `Shared/AngelscriptReflectiveAccess.h` | Property/function reflective access helper |

## 模板

`Template/` 文件是教学夹具和起点，不是功能测试的最终落点。

| 模板 | 用途 |
|---|---|
| `Template_CQTest.cpp` | CQTest 编译/执行、断言、struct 返回、参数传递、negative path、early return |
| `Template_GlobalFunctions.cpp` | 通过 `FASGlobalFunctionInvoker` 从 C++ 调 AS global function |
| `Template_ReflectionAccess.cpp` | 大类型矩阵下的 UPROPERTY path 读写和 UFUNCTION invoke |
| `Template_WorldTick.cpp` | World.Tick、Actor.Tick、Component.Tick 三种驱动路径 |
| `Template_GameLifetime.cpp` | 完整 Actor 生命周期：Construction、`BeginPlay`、Tick、`EndPlay`、`Destroyed` |
| `Template_Blueprint.cpp` | 以 AS class 为父的 transient Blueprint 子类 |
| `Template_BlueprintWorldTick.cpp` | 通过 `FAngelscriptTestWorld` 驱动 Blueprint actor child 回调链 |
| `Template_PIE.cpp` | Editor PIE 教学模板：transient map、AS GameMode、AS-parent Level Blueprint、显式 EndPIE 清理 |
| `Template_MultiplayerPIE.cpp` | Multiplayer PIE 模板：2/3/4 player listen server 会话、client worlds、AS GameMode、AS-parent Level Blueprint、NetDriver 和清理 |

## 验证规则

修改 CQTest 或 HotReload 测试后，优先运行最窄的 Automation prefix：

```powershell
Tools\RunTests.ps1 -TestPrefix "Angelscript.TestModule.HotReload.ReloadDelegates" -Label hotreload-reload-delegates -TimeoutMs 600000
Tools\RunTests.ps1 -TestPrefix "Angelscript.TestModule.HotReload.Delegates" -Label hotreload-delegates -TimeoutMs 600000
```

如果改动影响编译结构、include、unity build 或 module dependencies，再运行：

```powershell
Tools\RunBuild.ps1 -ExtraArgs -NoHotReloadFromIDE -TimeoutMs 1800000
```

验证结果必须记录 pass/fail 数字。不要用“应该能过”替代实际运行结果。
