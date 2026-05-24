#include "Shared/AngelscriptTestUtilities.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
// Test Layer: Runtime Integration
#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;

namespace AngelscriptTestSupport
{
	bool CompileModuleWithResult(FAngelscriptEngine* Engine, ECompileType CompileType, FName ModuleName, FString Filename, FString Script, ECompileResult& OutCompileResult);
	bool CompileModuleFromMemory(FAngelscriptEngine* Engine, FName ModuleName, FString Filename, FString Script);
	void ResetSharedCloneEngine(FAngelscriptEngine& Engine);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptInheritanceBasicTest,
	"Angelscript.TestModule.Functional.Inheritance.Basic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptInheritanceBasicTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);
	const bool bCompiled = CompileModuleFromMemory(
		&Engine,
		TEXT("ASInheritanceBasic"),
		TEXT("ASInheritanceBasic.as"),
		TEXT("class Base { int baseValue; void SetBase(int Value) { baseValue = Value; } } class Derived : Base { int derivedValue; void SetDerived(int Value) { derivedValue = Value; } } int Test() { Derived Instance; Instance.SetBase(10); Instance.SetDerived(20); return Instance.baseValue + Instance.derivedValue; }"));
	if (!TestTrue(TEXT("Inheritance.Basic should compile through the shared non-preprocessor path"), bCompiled))
	{
		return false;
	}

	TSharedPtr<FAngelscriptModuleDesc> ModuleDesc = Engine.GetModuleByModuleName(TEXT("ASInheritanceBasic"));
	asIScriptModule* Module = ModuleDesc.IsValid() ? ModuleDesc->ScriptModule : nullptr;
	if (!TestNotNull(TEXT("Inheritance.Basic should expose the compiled module"), Module))
	{
		return false;
	}

	asIScriptFunction* Function = GetFunctionByDecl(*this, *Module, TEXT("int Test()"));
	if (Function == nullptr)
	{
		return false;
	}

	if (!ExecuteIntFunctionExpectingScriptException(
		*this,
		Engine,
		*Function,
		TEXT("Inheritance.Basic"),
		TEXT("Null pointer access"),
		TEXT("int Test() | Line 1 | Col 227")))
	{
		return false;
	}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptInheritanceInterfaceTest,
	"Angelscript.TestModule.Functional.Inheritance.Interface",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptInheritanceInterfaceTest::RunTest(const FString& Parameters)
{
	bool bPassed = false;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
	{
		FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			const TArray<TSharedRef<FAngelscriptModuleDesc>> _ActiveModules = Engine.GetActiveModules();
			for (const TSharedRef<FAngelscriptModuleDesc>& _Module : _ActiveModules)
			{
				Engine.DiscardModule(*_Module->ModuleName);
			}
		};
	const FString ScriptFilename = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("NegativeCompileIsolation"), TEXT("ASInheritanceInterface.as"));
	ECompileResult CompileResult = ECompileResult::FullyHandled;
	UE_SET_LOG_VERBOSITY(Angelscript, Fatal);
	const bool bCompiled = CompileModuleWithResult(
		&Engine,
		ECompileType::SoftReloadOnly,
		TEXT("ASInheritanceInterface"),
		ScriptFilename,
		TEXT("interface IValueProvider { int GetValue(); } class Provider : IValueProvider { int GetValue() { return 42; } } int Test() { Provider Instance; return 42; }"),
		CompileResult);
	UE_SET_LOG_VERBOSITY(Angelscript, Log);
	if (!TestFalse(TEXT("Inheritance.Interface should remain unsupported on this branch"), bCompiled))
	{
		return false;
	}
	bPassed = CompileResult == ECompileResult::Error;
	}

	return bPassed;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptInheritanceVirtualMethodTest,
	"Angelscript.TestModule.Functional.Inheritance.VirtualMethod",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptInheritanceVirtualMethodTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);
	const bool bCompiled = CompileModuleFromMemory(
		&Engine,
		TEXT("ASInheritanceVirtualMethod"),
		TEXT("ASInheritanceVirtualMethod.as"),
		TEXT("class Base { int GetValue() { return 1; } } class Derived : Base { int GetValue() { return 2; } } int Test() { Derived Instance; return Instance.GetValue(); }"));
	if (!TestTrue(TEXT("Inheritance.VirtualMethod should compile through the shared non-preprocessor path"), bCompiled))
	{
		return false;
	}

	TSharedPtr<FAngelscriptModuleDesc> ModuleDesc = Engine.GetModuleByModuleName(TEXT("ASInheritanceVirtualMethod"));
	asIScriptModule* Module = ModuleDesc.IsValid() ? ModuleDesc->ScriptModule : nullptr;
	if (!TestNotNull(TEXT("Inheritance.VirtualMethod should expose the compiled module"), Module))
	{
		return false;
	}

	asIScriptFunction* Function = GetFunctionByDecl(*this, *Module, TEXT("int Test()"));
	if (Function == nullptr)
	{
		return false;
	}

	if (!ExecuteIntFunctionExpectingScriptException(
		*this,
		Engine,
		*Function,
		TEXT("Inheritance.VirtualMethod"),
		TEXT("Null pointer access"),
		TEXT("int Test() | Line 1 | Col 130")))
	{
		return false;
	}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptInheritanceCastOpTest,
	"Angelscript.TestModule.Functional.Inheritance.CastOp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptInheritanceCastOpTest::RunTest(const FString& Parameters)
{
	bool bPassed = false;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
	{
		FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			const TArray<TSharedRef<FAngelscriptModuleDesc>> _ActiveModules = Engine.GetActiveModules();
			for (const TSharedRef<FAngelscriptModuleDesc>& _Module : _ActiveModules)
			{
				Engine.DiscardModule(*_Module->ModuleName);
			}
		};
	const FString ScriptFilename = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("NegativeCompileIsolation"), TEXT("ASInheritanceCastOp.as"));
	ECompileResult CompileResult = ECompileResult::FullyHandled;
	UE_SET_LOG_VERBOSITY(Angelscript, Fatal);
	const bool bCompiled = CompileModuleWithResult(
		&Engine,
		ECompileType::SoftReloadOnly,
		TEXT("ASInheritanceCastOp"),
		ScriptFilename,
		TEXT("class CastOpClass { int Value; CastOpClass(int InValue) { Value = InValue; } } int Test() { CastOpClass@ Instance = CastOpClass(42); return Instance.Value; }"),
		CompileResult);
	UE_SET_LOG_VERBOSITY(Angelscript, Log);
	if (!TestFalse(TEXT("Inheritance.CastOp should remain unsupported because script-class handle construction is not available on this branch"), bCompiled))
	{
		return false;
	}
	bPassed = CompileResult == ECompileResult::Error;
	}

	return bPassed;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptInheritanceMixinTest,
	"Angelscript.TestModule.Functional.Inheritance.Mixin",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptInheritanceMixinTest::RunTest(const FString& Parameters)
{
	bool bPassed = false;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
	{
		FAngelscriptEngineScope _AutoEngineScope(Engine);
		ON_SCOPE_EXIT
		{
			const TArray<TSharedRef<FAngelscriptModuleDesc>> _ActiveModules = Engine.GetActiveModules();
			for (const TSharedRef<FAngelscriptModuleDesc>& _Module : _ActiveModules)
			{
				Engine.DiscardModule(*_Module->ModuleName);
			}
		};
	const FString ScriptFilename = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("NegativeCompileIsolation"), TEXT("ASInheritanceMixin.as"));
	ECompileResult CompileResult = ECompileResult::FullyHandled;
	UE_SET_LOG_VERBOSITY(Angelscript, Fatal);
	const bool bCompiled = CompileModuleWithResult(
		&Engine,
		ECompileType::SoftReloadOnly,
		TEXT("ASInheritanceMixin"),
		ScriptFilename,
		TEXT("mixin class SharedValueMixin { int GetValue() { return 42; } } class Consumer : SharedValueMixin {} int Test() { Consumer Instance; return Instance.GetValue(); }"),
		CompileResult);
	UE_SET_LOG_VERBOSITY(Angelscript, Log);
	TestFalse(TEXT("Inheritance.Mixin should remain unsupported on this branch because the parser does not accept mixin-class syntax"), bCompiled);
	bPassed = !bCompiled;
	}

	return bPassed;
}

#endif
