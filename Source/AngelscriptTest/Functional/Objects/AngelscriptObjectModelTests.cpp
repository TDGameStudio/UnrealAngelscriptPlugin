#include "CQTest.h"
#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptFunctionalTestUtils.h"
#include "AngelscriptReflectiveAccess.h"

#include "Misc/ScopeExit.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"

// Test Layer: Runtime Integration
#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

bool CompileModuleWithResult(FAngelscriptEngine* Engine, ECompileType CompileType, FName ModuleName, FString Filename, FString Script, ECompileResult& OutCompileResult);
bool CompileModuleFromMemory(FAngelscriptEngine* Engine, FName ModuleName, FString Filename, FString Script);

TEST_CLASS_WITH_FLAGS(
	FAngelscriptObjectModelTests,
	"Angelscript.TestModule.Functional.Objects",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static void CompileRunInt(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		const ANSICHAR* ModuleName,
		const TCHAR* Source,
		const TCHAR* FuncDecl,
		int32& OutResult)
	{
		asIScriptModule* Module = BuildModule(Test, Engine, ModuleName, Source);
		if (!Test.TestNotNull(TEXT("Object model helper should compile the test module"), Module))
		{
			return;
		}

		asIScriptFunction* Function = GetFunctionByDecl(Test, *Module, FuncDecl);
		if (Function == nullptr)
		{
			return;
		}

		ExecuteIntFunction(Test, Engine, *Function, OutResult);
	}

public:
	TEST_METHOD(ValueTypeConstruction)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		int32 Result = 0;
		CompileRunInt(
			*TestRunner,
			Engine,
			"ASObjectModelInheritance",
			TEXT("int Run() { FIntPoint Point(3, 4); return Point.X + Point.Y; }"),
			TEXT("int Run()"),
			Result);

		ASSERT_THAT(AreEqual(7, Result, TEXT("Value-type construction and member access should preserve field values")));
	}

	TEST_METHOD(ValueTypeCopyAndArithmetic)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		int32 Result = 0;
		CompileRunInt(
			*TestRunner,
			Engine,
			"ASObjectModelDestructor",
			TEXT("int Run() { FIntPoint Original(5, 6); FIntPoint Copy(Original); Copy = Copy + FIntPoint(2, 0); return Original.X * 10 + Copy.X; }"),
			TEXT("int Run()"),
			Result);

		ASSERT_THAT(AreEqual(57, Result, TEXT("Value-type copies should preserve the original and apply arithmetic to the copy")));
	}

	TEST_METHOD(Basic)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const bool bCompiled = CompileModuleFromMemory(
			&Engine,
			TEXT("ASObjectBasic"),
			TEXT("ASObjectBasic.as"),
			TEXT("class ObjectCarrier { int Value; void Set(int InValue) { Value = InValue; } int Get() { return Value; } } int Run() { ObjectCarrier Carrier; Carrier.Set(42); return Carrier.Get(); }"));
		ASSERT_THAT(IsTrue(bCompiled, TEXT("Objects.Basic should compile through the shared non-preprocessor path")));

		TSharedPtr<FAngelscriptModuleDesc> ModuleDesc = Engine.GetModuleByModuleName(TEXT("ASObjectBasic"));
		asIScriptModule* Module = ModuleDesc.IsValid() ? ModuleDesc->ScriptModule : nullptr;
		ASSERT_THAT(IsNotNull(Module, TEXT("Objects.Basic should expose the compiled module")));

		asIScriptFunction* Function = GetFunctionByDecl(*TestRunner, *Module, TEXT("int Run()"));
		if (Function == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(ExecuteIntFunctionExpectingScriptException(
			*TestRunner,
			Engine,
			*Function,
			TEXT("Objects.Basic"),
			TEXT("Null pointer access"))));
	}

	TEST_METHOD(ReflectedDefaultsAndFunction)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
		static const FName ModuleName(TEXT("ASObjectReflectedDefaultsAndFunction"));
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*ModuleName.ToString());
			ASTEST_RESET_ENGINE(Engine);
		};

		UClass* ScriptClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ModuleName,
			TEXT("ASObjectReflectedDefaultsAndFunction.as"),
			TEXT(R"AS(
UCLASS()
class UObjectReflectedDefaultsAndFunction : UObject
{
	UPROPERTY()
	int Counter = 9;

	UPROPERTY()
	FString ObjectLabel = "FunctionalObject";

	UFUNCTION()
	int ComputeMarker()
	{
		return Counter + 5;
	}
}
)AS"),
			TEXT("UObjectReflectedDefaultsAndFunction"));
		if (ScriptClass == nullptr)
		{
			return;
		}

		UObject* Object = NewObject<UObject>(GetTransientPackage(), ScriptClass);
		ASSERT_THAT(IsNotNull(Object, TEXT("Script UObject should instantiate")));

		VerifyByPath<FIntProperty, int32>(*TestRunner, Object, TEXT("Counter"), 9,
			TEXT("Script UObject should preserve reflected int defaults"));
		VerifyByPath<FStrProperty, FString>(*TestRunner, Object, TEXT("ObjectLabel"), FString(TEXT("FunctionalObject")),
			TEXT("Script UObject should preserve reflected string defaults"));

		FFunctionInvoker Invoker(*TestRunner, Object, FName(TEXT("ComputeMarker")));
		if (!Invoker.IsValid())
		{
			return;
		}

		ASSERT_THAT(AreEqual(14, Invoker.CallAndReturn<int32>(INDEX_NONE), TEXT("Script UObject helper UFUNCTION should read reflected state")));
	}

	TEST_METHOD(Composition)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const bool bCompiled = CompileModuleFromMemory(
			&Engine,
			TEXT("ASObjectComposition"),
			TEXT("ASObjectComposition.as"),
			TEXT("class InnerObject { int Value; } class OuterObject { InnerObject Inner; } int Run() { OuterObject Carrier; Carrier.Inner.Value = 42; return Carrier.Inner.Value; }"));
		ASSERT_THAT(IsTrue(bCompiled, TEXT("Objects.Composition should compile through the shared non-preprocessor path")));

		TSharedPtr<FAngelscriptModuleDesc> ModuleDesc = Engine.GetModuleByModuleName(TEXT("ASObjectComposition"));
		asIScriptModule* Module = ModuleDesc.IsValid() ? ModuleDesc->ScriptModule : nullptr;
		ASSERT_THAT(IsNotNull(Module, TEXT("Objects.Composition should expose the compiled module")));

		asIScriptFunction* Function = GetFunctionByDecl(*TestRunner, *Module, TEXT("int Run()"));
		if (Function == nullptr)
		{
			return;
		}

		ASSERT_THAT(IsTrue(ExecuteIntFunctionExpectingScriptException(
			*TestRunner,
			Engine,
			*Function,
			TEXT("Objects.Composition"),
			TEXT("Null pointer access"))));
	}

	TEST_METHOD(Singleton)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		ECompileResult CompileResult = ECompileResult::FullyHandled;
		UE_SET_LOG_VERBOSITY(Angelscript, Fatal);
		ON_SCOPE_EXIT { UE_SET_LOG_VERBOSITY(Angelscript, Log); };
		const bool bCompiled = CompileModuleWithResult(
			&Engine,
			ECompileType::SoftReloadOnly,
			TEXT("ASObjectSingletonBoundary"),
			TEXT("ASObjectSingletonBoundary.as"),
			TEXT("class SingletonCarrier { int Value; } SingletonCarrier GlobalInstance; int Run() { return 1; }"),
			CompileResult);

		ASSERT_THAT(IsFalse(bCompiled, TEXT("Objects.Singleton should reject mutable global class variables on this branch")));
		ASSERT_THAT(AreEqual(ECompileResult::Error, CompileResult, TEXT("Objects.Singleton should surface the mutable-global compile error")));
	}

	TEST_METHOD(ZeroSize)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		int32 Result = 0;
		CompileRunInt(
			*TestRunner,
			Engine,
			"ASObjectZeroSize",
			TEXT("class EmptyObject {} int Run() { EmptyObject Instance; return 1; }"),
			TEXT("int Run()"),
			Result);

		ASSERT_THAT(AreEqual(1, Result, TEXT("Zero-size script objects should still be instantiable")));
	}
};

TEST_CLASS_WITH_FLAGS(
	FAngelscriptObjectZeroSizeTests,
	"Angelscript.TestModule.Functional.Objects.ZeroSize",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static void CompileRunInt(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		const ANSICHAR* ModuleName,
		const TCHAR* Source,
		const TCHAR* FuncDecl,
		int32& OutResult)
	{
		asIScriptModule* Module = BuildModule(Test, Engine, ModuleName, Source);
		if (!Test.TestNotNull(TEXT("Object model helper should compile the test module"), Module))
		{
			return;
		}

		asIScriptFunction* Function = GetFunctionByDecl(Test, *Module, FuncDecl);
		if (Function == nullptr)
		{
			return;
		}

		ExecuteIntFunction(Test, Engine, *Function, OutResult);
	}

public:
	TEST_METHOD(ByValueAndLocalLayout)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		int32 Result = 0;
		CompileRunInt(
			*TestRunner,
			Engine,
			"ASObjectZeroSizeByValueAndLocalLayout",
			TEXT("class EmptyObject {} int Accept(EmptyObject Value) { return 2; } int Run() { int Prefix = 5; EmptyObject First; int Middle = 6; EmptyObject Second; return Prefix * 1000 + Middle * 100 + Accept(First) * 10 + Accept(Second); }"),
			TEXT("int Run()"),
			Result);

		ASSERT_THAT(AreEqual(5622, Result, TEXT("Zero-size script objects should preserve adjacent locals and pass by value twice")));
	}
};

#endif
