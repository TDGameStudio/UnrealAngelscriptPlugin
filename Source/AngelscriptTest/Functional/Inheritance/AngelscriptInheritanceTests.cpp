#include "CQTest.h"
#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestMacros.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
// Test Layer: Runtime Integration
#if WITH_DEV_AUTOMATION_TESTS


bool CompileModuleWithResult(FAngelscriptEngine* Engine, ECompileType CompileType, FName ModuleName, FString Filename, FString Script, ECompileResult& OutCompileResult);
bool CompileModuleFromMemory(FAngelscriptEngine* Engine, FName ModuleName, FString Filename, FString Script);
void ResetSharedCloneEngine(FAngelscriptEngine& Engine);


TEST_CLASS_WITH_FLAGS(
	FAngelscriptInheritanceTests,
	"Angelscript.TestModule.Functional.Inheritance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(Basic)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		const bool bCompiled = CompileModuleFromMemory(
			&Engine,
			TEXT("ASInheritanceBasic"),
			TEXT("ASInheritanceBasic.as"),
			TEXT("class Base { int baseValue; void SetBase(int Value) { baseValue = Value; } } class Derived : Base { int derivedValue; void SetDerived(int Value) { derivedValue = Value; } } int Test() { Derived Instance; Instance.SetBase(10); Instance.SetDerived(20); return Instance.baseValue + Instance.derivedValue; }"));
		ASSERT_THAT(IsTrue(bCompiled, TEXT("Inheritance.Basic should compile through the shared non-preprocessor path")));

		TSharedPtr<FAngelscriptModuleDesc> ModuleDesc = Engine.GetModuleByModuleName(TEXT("ASInheritanceBasic"));
		asIScriptModule* Module = ModuleDesc.IsValid() ? ModuleDesc->ScriptModule : nullptr;
		ASSERT_THAT(IsNotNull(Module, TEXT("Inheritance.Basic should expose the compiled module")));

		asIScriptFunction* Function = GetFunctionByDecl(*TestRunner, *Module, TEXT("int Test()"));
		if (Function == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(ExecuteIntFunctionExpectingScriptException(
			*TestRunner,
			Engine,
			*Function,
			TEXT("Inheritance.Basic"),
			TEXT("Null pointer access"),
			TEXT("int Test() | Line 1 | Col 227"))));
	}

	TEST_METHOD(Interface)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
		FAngelscriptEngineScope Scope(Engine);
		ON_SCOPE_EXIT
		{
			const TArray<TSharedRef<FAngelscriptModuleDesc>> ActiveModules = Engine.GetActiveModules();
			for (const TSharedRef<FAngelscriptModuleDesc>& Module : ActiveModules)
			{
				Engine.DiscardModule(*Module->ModuleName);
			}
		};

		const FString ScriptFilename = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("NegativeCompileIsolation"), TEXT("ASInheritanceInterface.as"));
		ECompileResult CompileResult = ECompileResult::FullyHandled;
		UE_SET_LOG_VERBOSITY(Angelscript, Fatal);
		ON_SCOPE_EXIT { UE_SET_LOG_VERBOSITY(Angelscript, Log); };
		const bool bCompiled = CompileModuleWithResult(
			&Engine,
			ECompileType::SoftReloadOnly,
			TEXT("ASInheritanceInterface"),
			ScriptFilename,
			TEXT("interface IValueProvider { int GetValue(); } class Provider : IValueProvider { int GetValue() { return 42; } } int Test() { Provider Instance; return 42; }"),
			CompileResult);

		ASSERT_THAT(IsFalse(bCompiled, TEXT("Inheritance.Interface should remain unsupported on this branch")));
		ASSERT_THAT(AreEqual(ECompileResult::Error, CompileResult, TEXT("Inheritance.Interface should surface a compile error")));
	}

	TEST_METHOD(VirtualMethod)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		const bool bCompiled = CompileModuleFromMemory(
			&Engine,
			TEXT("ASInheritanceVirtualMethod"),
			TEXT("ASInheritanceVirtualMethod.as"),
			TEXT("class Base { int GetValue() { return 1; } } class Derived : Base { int GetValue() { return 2; } } int Test() { Derived Instance; return Instance.GetValue(); }"));
		ASSERT_THAT(IsTrue(bCompiled, TEXT("Inheritance.VirtualMethod should compile through the shared non-preprocessor path")));

		TSharedPtr<FAngelscriptModuleDesc> ModuleDesc = Engine.GetModuleByModuleName(TEXT("ASInheritanceVirtualMethod"));
		asIScriptModule* Module = ModuleDesc.IsValid() ? ModuleDesc->ScriptModule : nullptr;
		ASSERT_THAT(IsNotNull(Module, TEXT("Inheritance.VirtualMethod should expose the compiled module")));

		asIScriptFunction* Function = GetFunctionByDecl(*TestRunner, *Module, TEXT("int Test()"));
		if (Function == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(ExecuteIntFunctionExpectingScriptException(
			*TestRunner,
			Engine,
			*Function,
			TEXT("Inheritance.VirtualMethod"),
			TEXT("Null pointer access"),
			TEXT("int Test() | Line 1 | Col 130"))));
	}

	TEST_METHOD(CastOp)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
		FAngelscriptEngineScope Scope(Engine);
		ON_SCOPE_EXIT
		{
			const TArray<TSharedRef<FAngelscriptModuleDesc>> ActiveModules = Engine.GetActiveModules();
			for (const TSharedRef<FAngelscriptModuleDesc>& Module : ActiveModules)
			{
				Engine.DiscardModule(*Module->ModuleName);
			}
		};

		const FString ScriptFilename = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("NegativeCompileIsolation"), TEXT("ASInheritanceCastOp.as"));
		ECompileResult CompileResult = ECompileResult::FullyHandled;
		UE_SET_LOG_VERBOSITY(Angelscript, Fatal);
		ON_SCOPE_EXIT { UE_SET_LOG_VERBOSITY(Angelscript, Log); };
		const bool bCompiled = CompileModuleWithResult(
			&Engine,
			ECompileType::SoftReloadOnly,
			TEXT("ASInheritanceCastOp"),
			ScriptFilename,
			TEXT("class CastOpClass { int Value; CastOpClass(int InValue) { Value = InValue; } } int Test() { CastOpClass@ Instance = CastOpClass(42); return Instance.Value; }"),
			CompileResult);

		ASSERT_THAT(IsFalse(bCompiled, TEXT("Inheritance.CastOp should remain unsupported because script-class handle construction is not available on this branch")));
		ASSERT_THAT(AreEqual(ECompileResult::Error, CompileResult, TEXT("Inheritance.CastOp should surface a compile error")));
	}

	TEST_METHOD(Mixin)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE_FULL();
		FAngelscriptEngineScope Scope(Engine);
		ON_SCOPE_EXIT
		{
			const TArray<TSharedRef<FAngelscriptModuleDesc>> ActiveModules = Engine.GetActiveModules();
			for (const TSharedRef<FAngelscriptModuleDesc>& Module : ActiveModules)
			{
				Engine.DiscardModule(*Module->ModuleName);
			}
		};

		const FString ScriptFilename = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("NegativeCompileIsolation"), TEXT("ASInheritanceMixin.as"));
		ECompileResult CompileResult = ECompileResult::FullyHandled;
		UE_SET_LOG_VERBOSITY(Angelscript, Fatal);
		ON_SCOPE_EXIT { UE_SET_LOG_VERBOSITY(Angelscript, Log); };
		const bool bCompiled = CompileModuleWithResult(
			&Engine,
			ECompileType::SoftReloadOnly,
			TEXT("ASInheritanceMixin"),
			ScriptFilename,
			TEXT("mixin class SharedValueMixin { int GetValue() { return 42; } } class Consumer : SharedValueMixin {} int Test() { Consumer Instance; return Instance.GetValue(); }"),
			CompileResult);

		ASSERT_THAT(IsFalse(bCompiled, TEXT("Inheritance.Mixin should remain unsupported on this branch because the parser does not accept mixin-class syntax")));
	}
};

#endif
