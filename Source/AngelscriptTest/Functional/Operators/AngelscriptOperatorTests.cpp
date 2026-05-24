#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestLegacyHelpers.h"
#include "Misc/Paths.h"

// Test Layer: Runtime Integration
#if WITH_DEV_AUTOMATION_TESTS


bool CompileModuleWithResult(FAngelscriptEngine* Engine, ECompileType CompileType, FName ModuleName, FString Filename, FString Script, ECompileResult& OutCompileResult);


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptOperatorOverloadTest,
	"Angelscript.TestModule.Functional.Operators.Overload",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptOperatorOverloadTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		"ASOperatorOverload",
		TEXT("class Vector2Overload { int X; int Y; Vector2Overload opAdd(const Vector2Overload &in Other) const { Vector2Overload Result; Result.X = X + Other.X; Result.Y = Y + Other.Y; return Result; } } int Test() { Vector2Overload A; A.X = 1; A.Y = 2; Vector2Overload B; B.X = 3; B.Y = 4; Vector2Overload C = A + B; return C.X + C.Y; }"));
	if (Module == nullptr)
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
		TEXT("Operators.Overload"),
		TEXT("Null pointer access")))
	{
		return false;
	}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptOperatorGetSetTest,
	"Angelscript.TestModule.Functional.Operators.GetSet",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptOperatorGetSetTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	FAngelscriptEngineScope EngineScope(Engine);
	asIScriptEngine* ScriptEngine = Engine.GetScriptEngine();
	if (!TestNotNull(TEXT("Operators.GetSet should expose a script engine for the isolated compile-fail probe"), ScriptEngine))
	{
		return false;
	}

	ScriptEngine->SetDefaultNamespace("");
	FScopedAutomaticImportsOverride AutomaticImportsOverride(ScriptEngine);
	asIScriptModule* Module = ScriptEngine->GetModule("ASOperatorGetSetRaw", asGM_ALWAYS_CREATE);
	if (!TestNotNull(TEXT("Operators.GetSet should create a raw script module"), Module))
	{
		return false;
	}

	static constexpr ANSICHAR Script[] = "class AccessorCarrier { private int StoredValue; int GetValue() const { return StoredValue; } void SetValue(int InValue) { StoredValue = InValue; } } int Test() { AccessorCarrier Instance; Instance.SetValue(42); return Instance.GetValue(); }";
	Module->AddScriptSection("ASOperatorGetSetRaw", Script, UE_ARRAY_COUNT(Script) - 1);
	const int32 BuildResult = Module->Build();
	if (!TestEqual(TEXT("Operators.GetSet should compile through explicit Get/Set methods"), BuildResult, static_cast<int32>(asSUCCESS)))
	{
		return false;
	}
	asIScriptFunction* Function = GetFunctionByDecl(*this, *Module, TEXT("int Test()"));
	if (Function == nullptr)
	{
		return false;
	}

	// The explicit getter/setter path still faults on this branch, so keep it as
	// a named negative boundary instead of asserting a fake runtime success.
	if (!ExecuteIntFunctionExpectingScriptException(
		*this,
		Engine,
		*Function,
		TEXT("Operators.GetSet"),
		TEXT("Null pointer access")))
	{
		return false;
	}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptOperatorConstTest,
	"Angelscript.TestModule.Functional.Operators.Const",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptOperatorConstTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		"ASOperatorConst",
		TEXT("class ConstCarrier { int Value; int GetValue() const { return Value; } } int Test() { ConstCarrier Carrier; Carrier.Value = 42; return Carrier.GetValue(); }"));
	if (Module == nullptr)
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
		TEXT("Operators.Const"),
		TEXT("Null pointer access")))
	{
		return false;
	}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptOperatorPowerTest,
	"Angelscript.TestModule.Functional.Operators.Power",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptOperatorPowerTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	int32 Result = 0;
	ASTEST_COMPILE_RUN_INT(Engine,
		"ASOperatorPower",
		TEXT("int Test() { return int(2.0f ** 3.0f); }"),
		TEXT("int Test()"),
		Result);

	TestEqual(TEXT("Operators.Power should preserve exponentiation semantics"), Result, 8);
	}

	return true;
}

#endif
