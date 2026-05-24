#include "Shared/AngelscriptTestUtilities.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Shared/AngelscriptTestLegacyHelpers.h"
#include "Shared/AngelscriptFunctionalTestUtils.h"
#include "Shared/AngelscriptReflectiveAccess.h"

#include "Misc/ScopeExit.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"

// Test Layer: Runtime Integration
#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptFunctionalTestUtils;

bool CompileModuleWithResult(FAngelscriptEngine* Engine, ECompileType CompileType, FName ModuleName, FString Filename, FString Script, ECompileResult& OutCompileResult);
bool CompileModuleFromMemory(FAngelscriptEngine* Engine, FName ModuleName, FString Filename, FString Script);


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptObjectModelInheritanceTest,
	"Angelscript.TestModule.Functional.Objects.ValueTypeConstruction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptObjectModelInheritanceTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	int32 Result = 0;
	ASTEST_COMPILE_RUN_INT(Engine,
		"ASObjectModelInheritance",
		TEXT("int Run() { FIntPoint Point(3, 4); return Point.X + Point.Y; }"),
		TEXT("int Run()"),
		Result);

	TestEqual(TEXT("Value-type construction and member access should preserve field values"), Result, 7);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptObjectModelDestructorTest,
	"Angelscript.TestModule.Functional.Objects.ValueTypeCopyAndArithmetic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptObjectModelDestructorTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	int32 Result = 0;
	ASTEST_COMPILE_RUN_INT(Engine,
		"ASObjectModelDestructor",
		TEXT("int Run() { FIntPoint Original(5, 6); FIntPoint Copy(Original); Copy = Copy + FIntPoint(2, 0); return Original.X * 10 + Copy.X; }"),
		TEXT("int Run()"),
		Result);

	TestEqual(TEXT("Value-type copies should preserve the original and apply arithmetic to the copy"), Result, 57);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptObjectBasicTest,
	"Angelscript.TestModule.Functional.Objects.Basic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptObjectBasicTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	const bool bCompiled = CompileModuleFromMemory(
		&Engine,
		TEXT("ASObjectBasic"),
		TEXT("ASObjectBasic.as"),
		TEXT("class ObjectCarrier { int Value; void Set(int InValue) { Value = InValue; } int Get() { return Value; } } int Run() { ObjectCarrier Carrier; Carrier.Set(42); return Carrier.Get(); }"));
	if (!TestTrue(TEXT("Objects.Basic should compile through the shared non-preprocessor path"), bCompiled))
	{
		return false;
	}

	TSharedPtr<FAngelscriptModuleDesc> ModuleDesc = Engine.GetModuleByModuleName(TEXT("ASObjectBasic"));
	asIScriptModule* Module = ModuleDesc.IsValid() ? ModuleDesc->ScriptModule : nullptr;
	if (!TestNotNull(TEXT("Objects.Basic should expose the compiled module"), Module))
	{
		return false;
	}

	asIScriptFunction* Function = GetFunctionByDecl(*this, *Module, TEXT("int Run()"));
	if (Function == nullptr)
	{
		return false;
	}

	if (!ExecuteIntFunctionExpectingScriptException(
		*this,
		Engine,
		*Function,
		TEXT("Objects.Basic"),
		TEXT("Null pointer access")))
	{
		return false;
	}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptObjectReflectedDefaultsAndFunctionTest,
	"Angelscript.TestModule.Functional.Objects.ReflectedDefaultsAndFunction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptObjectReflectedDefaultsAndFunctionTest::RunTest(const FString& Parameters)
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
		*this,
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
		return false;
	}

	UObject* Object = NewObject<UObject>(GetTransientPackage(), ScriptClass);
	if (!TestNotNull(TEXT("Script UObject should instantiate"), Object))
	{
		return false;
	}

	VerifyByPath<FIntProperty, int32>(*this, Object, TEXT("Counter"), 9,
		TEXT("Script UObject should preserve reflected int defaults"));
	VerifyByPath<FStrProperty, FString>(*this, Object, TEXT("ObjectLabel"), FString(TEXT("FunctionalObject")),
		TEXT("Script UObject should preserve reflected string defaults"));

	FFunctionInvoker Invoker(*this, Object, FName(TEXT("ComputeMarker")));
	if (!Invoker.IsValid())
	{
		return false;
	}

	TestEqual(TEXT("Script UObject helper UFUNCTION should read reflected state"),
		Invoker.CallAndReturn<int32>(INDEX_NONE), 14);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptObjectCompositionTest,
	"Angelscript.TestModule.Functional.Objects.Composition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptObjectCompositionTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	const bool bCompiled = CompileModuleFromMemory(
		&Engine,
		TEXT("ASObjectComposition"),
		TEXT("ASObjectComposition.as"),
		TEXT("class InnerObject { int Value; } class OuterObject { InnerObject Inner; } int Run() { OuterObject Carrier; Carrier.Inner.Value = 42; return Carrier.Inner.Value; }"));
	if (!TestTrue(TEXT("Objects.Composition should compile through the shared non-preprocessor path"), bCompiled))
	{
		return false;
	}

	TSharedPtr<FAngelscriptModuleDesc> ModuleDesc = Engine.GetModuleByModuleName(TEXT("ASObjectComposition"));
	asIScriptModule* Module = ModuleDesc.IsValid() ? ModuleDesc->ScriptModule : nullptr;
	if (!TestNotNull(TEXT("Objects.Composition should expose the compiled module"), Module))
	{
		return false;
	}

	asIScriptFunction* Function = GetFunctionByDecl(*this, *Module, TEXT("int Run()"));
	if (Function == nullptr)
	{
		return false;
	}

	if (!ExecuteIntFunctionExpectingScriptException(
		*this,
		Engine,
		*Function,
		TEXT("Objects.Composition"),
		TEXT("Null pointer access")))
	{
		return false;
	}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptObjectSingletonTest,
	"Angelscript.TestModule.Functional.Objects.Singleton",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptObjectSingletonTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	ECompileResult CompileResult = ECompileResult::FullyHandled;
	UE_SET_LOG_VERBOSITY(Angelscript, Fatal);
	const bool bCompiled = CompileModuleWithResult(
		&Engine,
		ECompileType::SoftReloadOnly,
		TEXT("ASObjectSingletonBoundary"),
		TEXT("ASObjectSingletonBoundary.as"),
		TEXT("class SingletonCarrier { int Value; } SingletonCarrier GlobalInstance; int Run() { return 1; }"),
		CompileResult);
	UE_SET_LOG_VERBOSITY(Angelscript, Log);
	if (!TestFalse(TEXT("Objects.Singleton should reject mutable global class variables on this branch"), bCompiled))
	{
		return false;
	}

	TestEqual(TEXT("Objects.Singleton should surface the mutable-global compile error"), CompileResult, ECompileResult::Error);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptObjectZeroSizeTest,
	"Angelscript.TestModule.Functional.Objects.ZeroSize",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptObjectZeroSizeTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	int32 Result = 0;
	ASTEST_COMPILE_RUN_INT(Engine,
		"ASObjectZeroSize",
		TEXT("class EmptyObject {} int Run() { EmptyObject Instance; return 1; }"),
		TEXT("int Run()"),
		Result);

	TestEqual(TEXT("Zero-size script objects should still be instantiable"), Result, 1);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptObjectZeroSizeByValueAndLocalLayoutTest,
	"Angelscript.TestModule.Functional.Objects.ZeroSize.ByValueAndLocalLayout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptObjectZeroSizeByValueAndLocalLayoutTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	int32 Result = 0;
	ASTEST_COMPILE_RUN_INT(Engine,
		"ASObjectZeroSizeByValueAndLocalLayout",
		TEXT("class EmptyObject {} int Accept(EmptyObject Value) { return 2; } int Run() { int Prefix = 5; EmptyObject First; int Middle = 6; EmptyObject Second; return Prefix * 1000 + Middle * 100 + Accept(First) * 10 + Accept(Second); }"),
		TEXT("int Run()"),
		Result);

	TestEqual(TEXT("Zero-size script objects should preserve adjacent locals and pass by value twice"), Result, 5622);
	}

	return true;
}

#endif
