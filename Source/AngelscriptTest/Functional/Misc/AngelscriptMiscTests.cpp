#include "CQTest.h"
#include "AngelscriptBindingsAssertions.h"
#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"

// Test Layer: Runtime Integration
#if WITH_DEV_AUTOMATION_TESTS


TEST_CLASS_WITH_FLAGS(
	FAngelscriptMiscTests,
	"Angelscript.TestModule.Functional.Misc",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	TEST_METHOD(Namespace)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Module(
			*TestRunner,
			Engine,
			TEXT("ASMiscNamespace"),
			TEXT("namespace MyNamespace { const int Value = 42; int GetValue() { return Value; } } int Test() { return MyNamespace::GetValue(); }"));
		ASSERT_THAT(IsTrue(Module.IsValid()));

		ExpectGlobalInt(
			*TestRunner,
			Engine,
			Module.GetModule(),
			TEXT("int Test()"),
			TEXT("Misc.Namespace should resolve namespace-qualified globals"),
			42);
	}


	TEST_METHOD(GlobalVar)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Module(
			*TestRunner,
			Engine,
			TEXT("ASMiscGlobalVar"),
			TEXT("const int GlobalValue = 42; int Test() { return GlobalValue; }"));
		ASSERT_THAT(IsTrue(Module.IsValid()));

		ExpectGlobalInt(
			*TestRunner,
			Engine,
			Module.GetModule(),
			TEXT("int Test()"),
			TEXT("Misc.GlobalVar should expose compiled global state"),
			42);
	}


	TEST_METHOD(MultiAssign)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Module(
			*TestRunner,
			Engine,
			TEXT("ASMiscMultiAssign"),
			TEXT("int Test() { int A = 0, B = 0, C = 0; A = B = C = 42; return A + B + C; }"));
		ASSERT_THAT(IsTrue(Module.IsValid()));

		ExpectGlobalInt(
			*TestRunner,
			Engine,
			Module.GetModule(),
			TEXT("int Test()"),
			TEXT("Misc.MultiAssign should evaluate chained assignments from right to left"),
			126);
	}


	TEST_METHOD(Assign)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Module(
			*TestRunner,
			Engine,
			TEXT("ASMiscAssign"),
			TEXT("int Test() { int Value = 10; Value += 5; Value -= 3; Value *= 2; Value /= 3; return Value; }"));
		ASSERT_THAT(IsTrue(Module.IsValid()));

		ExpectGlobalInt(
			*TestRunner,
			Engine,
			Module.GetModule(),
			TEXT("int Test()"),
			TEXT("Misc.Assign should apply compound assignments in sequence"),
			8);
	}


	TEST_METHOD(DuplicateFunction)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FAngelscriptEngineScope EngineScope(Engine);
		asIScriptEngine* ScriptEngine = Engine.GetScriptEngine();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Misc.DuplicateFunction should expose a script engine for the isolated compile-fail probe")));

		ScriptEngine->SetDefaultNamespace("");
		FScopedAutomaticImportsOverride AutomaticImportsOverride(ScriptEngine);
		asIScriptModule* Module = ScriptEngine->GetModule("ASMiscDuplicateFunctionRaw", asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(Module, TEXT("Misc.DuplicateFunction should create a raw script module")));

		static constexpr ANSICHAR Script[] = "int Test() { return 42; }\nint Test() { return 42; }\n";
		Module->AddScriptSection("ASMiscDuplicateFunctionRaw", Script, UE_ARRAY_COUNT(Script) - 1);
		TestRunner->AddExpectedErrorPlain(TEXT("ASMiscDuplicateFunctionRaw:"), EAutomationExpectedErrorFlags::Contains, 0);
		TestRunner->AddExpectedErrorPlain(TEXT("A function with the same name and parameters already exists"), EAutomationExpectedErrorFlags::Contains, 0);
		const int32 BuildResult = Module->Build();
		ASSERT_THAT(AreEqual(static_cast<int32>(asERROR), BuildResult, TEXT("Misc.DuplicateFunction should reject duplicate global function declarations on the raw AngelScript path")));
	}

	TEST_METHOD(DuplicateFunction_RawModuleRecreateAfterFailure)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FAngelscriptEngineScope EngineScope(Engine);
		asIScriptEngine* ScriptEngine = Engine.GetScriptEngine();
		ASSERT_THAT(IsNotNull(ScriptEngine, TEXT("Misc.DuplicateFunction.RawModuleRecreateAfterFailure should expose a script engine for the isolated raw-module probe")));

		ScriptEngine->SetDefaultNamespace("");
		FScopedAutomaticImportsOverride AutomaticImportsOverride(ScriptEngine);

		static constexpr ANSICHAR ModuleName[] = "ASMiscDuplicateFunctionRawIsolation";
		static constexpr ANSICHAR DuplicateScript[] = "int Test() { return 1; }\nint Test() { return 2; }\n";
		static constexpr ANSICHAR ValidScript[] = "int Test() { return 42; }\n";

		asIScriptModule* FailingModule = ScriptEngine->GetModule(ModuleName, asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(FailingModule, TEXT("Misc.DuplicateFunction.RawModuleRecreateAfterFailure should create the first raw module")));

		FailingModule->AddScriptSection(ModuleName, DuplicateScript, UE_ARRAY_COUNT(DuplicateScript) - 1);
		TestRunner->AddExpectedErrorPlain(TEXT("ASMiscDuplicateFunctionRawIsolation:"), EAutomationExpectedErrorFlags::Contains, 0);
		TestRunner->AddExpectedErrorPlain(TEXT("A function with the same name and parameters already exists"), EAutomationExpectedErrorFlags::Contains, 0);
		const int32 FirstBuildResult = FailingModule->Build();
		ASSERT_THAT(AreEqual(static_cast<int32>(asERROR), FirstBuildResult, TEXT("Misc.DuplicateFunction.RawModuleRecreateAfterFailure should reject duplicate declarations on the first raw build")));

		ASSERT_THAT(AreEqual(0, static_cast<int32>(FailingModule->GetFunctionCount()), TEXT("Misc.DuplicateFunction.RawModuleRecreateAfterFailure should not retain failed function entries after the duplicate build")));
		ASSERT_THAT(IsNull(FailingModule->GetFunctionByDecl("int Test()"), TEXT("Misc.DuplicateFunction.RawModuleRecreateAfterFailure should not expose a callable Test() after the duplicate build fails")));

		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), ScriptEngine->DiscardModule(ModuleName), TEXT("Misc.DuplicateFunction.RawModuleRecreateAfterFailure should discard the failed raw module before recreating it")));

		ASSERT_THAT(IsNull(ScriptEngine->GetModule(ModuleName, asGM_ONLY_IF_EXISTS), TEXT("Misc.DuplicateFunction.RawModuleRecreateAfterFailure should remove the failed raw module from the engine registry")));

		asIScriptModule* RecreatedModule = ScriptEngine->GetModule(ModuleName, asGM_ALWAYS_CREATE);
		ASSERT_THAT(IsNotNull(RecreatedModule, TEXT("Misc.DuplicateFunction.RawModuleRecreateAfterFailure should recreate the raw module after discard")));

		RecreatedModule->AddScriptSection(ModuleName, ValidScript, UE_ARRAY_COUNT(ValidScript) - 1);
		ASSERT_THAT(AreEqual(static_cast<int32>(asSUCCESS), RecreatedModule->Build(), TEXT("Misc.DuplicateFunction.RawModuleRecreateAfterFailure should rebuild the discarded raw module name with clean state")));

		ASSERT_THAT(AreEqual(1, static_cast<int32>(RecreatedModule->GetFunctionCount()), TEXT("Misc.DuplicateFunction.RawModuleRecreateAfterFailure should expose exactly one function after rebuilding the discarded module")));

		asIScriptFunction* TestFunction = GetFunctionByDecl(*TestRunner, *RecreatedModule, TEXT("int Test()"));
		ASSERT_THAT(IsNotNull(TestFunction, TEXT("Misc.DuplicateFunction.RawModuleRecreateAfterFailure should expose the recreated Test() function")));

		int32 Result = 0;
		ASSERT_THAT(IsTrue(ExecuteIntFunction(*TestRunner, Engine, *TestFunction, Result), TEXT("Misc.DuplicateFunction.RawModuleRecreateAfterFailure should execute the recreated module entry after discard")));

		ASSERT_THAT(AreEqual(42, Result, TEXT("Misc.DuplicateFunction.RawModuleRecreateAfterFailure should execute the recreated module entry after discard")));
	}
};

#endif
