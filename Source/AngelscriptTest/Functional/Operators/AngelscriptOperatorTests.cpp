#include "CQTest.h"
#include "AngelscriptBindingsAssertions.h"
#include "AngelscriptTestExecute.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "AngelscriptTestUtilities.h"

// Test Layer: Runtime Integration
#if WITH_ANGELSCRIPT_UNITTESTS


TEST_CLASS_WITH_FLAGS(
	FAngelscriptOperatorTests,
	"Angelscript.TestModule.Functional.Operators",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	TEST_METHOD(Overload)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Module(
			*TestRunner,
			Engine,
			TEXT("ASOperatorOverload"),
			TEXT("class Vector2Overload { int X; int Y; Vector2Overload opAdd(const Vector2Overload &in Other) const { Vector2Overload Result; Result.X = X + Other.X; Result.Y = Y + Other.Y; return Result; } } int Test() { Vector2Overload A; A.X = 1; A.Y = 2; Vector2Overload B; B.X = 3; B.Y = 4; Vector2Overload C = A + B; return C.X + C.Y; }"));
		ASSERT_THAT(IsTrue(Module.IsValid()));

		asIScriptFunction* Function = GetFunctionByDecl(*TestRunner, Module.GetModule(), TEXT("int Test()"));
		ASSERT_THAT(IsNotNull(Function, TEXT("Operators.Overload should expose Test()")));

		ASSERT_THAT(IsTrue(ExecuteIntFunctionExpectingScriptException(
			*TestRunner,
			Engine,
			*Function,
			TEXT("Operators.Overload"),
			TEXT("Null pointer access"))));
	}

	TEST_METHOD(GetSet)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FAngelscriptEngineScope EngineScope(Engine);
		asIScriptEngine* ScriptEngine = Engine.GetScriptEngine();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Operators.GetSet should expose a script engine for the isolated compile-fail probe")));

		ScriptEngine->SetDefaultNamespace("");
		FScopedAutomaticImportsOverride AutomaticImportsOverride(ScriptEngine);
		asIScriptModule* Module = ScriptEngine->GetModule("ASOperatorGetSetRaw", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module, TEXT("Operators.GetSet should create a raw script module")));

		static constexpr ANSICHAR Script[] = "class AccessorCarrier { private int StoredValue; int GetValue() const { return StoredValue; } void SetValue(int InValue) { StoredValue = InValue; } } int Test() { AccessorCarrier Instance; Instance.SetValue(42); return Instance.GetValue(); }";
		Module->AddScriptSection("ASOperatorGetSetRaw", Script, UE_ARRAY_COUNT(Script) - 1);
		const int32 BuildResult = Module->Build();
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), BuildResult, TEXT("Operators.GetSet should compile through explicit Get/Set methods")));

		asIScriptFunction* Function = GetFunctionByDecl(*TestRunner, *Module, TEXT("int Test()"));
		ASSERT_THAT(IsNotNull(Function, TEXT("Operators.GetSet should expose Test()")));

		ASSERT_THAT(IsTrue(ExecuteIntFunctionExpectingScriptException(
			*TestRunner,
			Engine,
			*Function,
			TEXT("Operators.GetSet"),
			TEXT("Null pointer access"))));
	}

	TEST_METHOD(Const)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Module(
			*TestRunner,
			Engine,
			TEXT("ASOperatorConst"),
			TEXT("class ConstCarrier { int Value; int GetValue() const { return Value; } } int Test() { ConstCarrier Carrier; Carrier.Value = 42; return Carrier.GetValue(); }"));
		ASSERT_THAT(IsTrue(Module.IsValid()));

		asIScriptFunction* Function = GetFunctionByDecl(*TestRunner, Module.GetModule(), TEXT("int Test()"));
		ASSERT_THAT(IsNotNull(Function, TEXT("Operators.Const should expose Test()")));

		ASSERT_THAT(IsTrue(ExecuteIntFunctionExpectingScriptException(
			*TestRunner,
			Engine,
			*Function,
			TEXT("Operators.Const"),
			TEXT("Null pointer access"))));
	}

	TEST_METHOD(Power)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Module(
			*TestRunner,
			Engine,
			TEXT("ASOperatorPower"),
			TEXT("int Test() { return int(2.0f ** 3.0f); }"));
		ASSERT_THAT(IsTrue(Module.IsValid()));

		ExpectGlobalInt(
			*TestRunner,
			Engine,
			Module.GetModule(),
			TEXT("int Test()"),
			TEXT("Operators.Power should preserve exponentiation semantics"),
			8);
	}
};

#endif
