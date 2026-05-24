#include "Shared/AngelscriptTestUtilities.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Shared/AngelscriptTestLegacyHelpers.h"

// Test Layer: Runtime Integration
#if WITH_DEV_AUTOMATION_TESTS


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptMiscNamespaceTest,
	"Angelscript.TestModule.Functional.Misc.Namespace",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptMiscNamespaceTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	int32 Result = 0;
	ASTEST_COMPILE_RUN_INT(Engine,
		"ASMiscNamespace",
		TEXT("namespace MyNamespace { const int Value = 42; int GetValue() { return Value; } } int Test() { return MyNamespace::GetValue(); }"),
		TEXT("int Test()"),
		Result);

	TestEqual(TEXT("Misc.Namespace should resolve namespace-qualified globals"), Result, 42);
	}

	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptMiscGlobalVarTest,
	"Angelscript.TestModule.Functional.Misc.GlobalVar",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptMiscGlobalVarTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	int32 Result = 0;
	ASTEST_COMPILE_RUN_INT(Engine,
		"ASMiscGlobalVar",
		TEXT("const int GlobalValue = 42; int Test() { return GlobalValue; }"),
		TEXT("int Test()"),
		Result);

	TestEqual(TEXT("Misc.GlobalVar should expose compiled global state"), Result, 42);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptMiscMultiAssignTest,
	"Angelscript.TestModule.Functional.Misc.MultiAssign",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptMiscMultiAssignTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	int32 Result = 0;
	ASTEST_COMPILE_RUN_INT(Engine,
		"ASMiscMultiAssign",
		TEXT("int Test() { int A = 0, B = 0, C = 0; A = B = C = 42; return A + B + C; }"),
		TEXT("int Test()"),
		Result);

	TestEqual(TEXT("Misc.MultiAssign should evaluate chained assignments from right to left"), Result, 126);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptMiscAssignTest,
	"Angelscript.TestModule.Functional.Misc.Assign",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptMiscAssignTest::RunTest(const FString& Parameters)
{
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	int32 Result = 0;
	ASTEST_COMPILE_RUN_INT(Engine,
		"ASMiscAssign",
		TEXT("int Test() { int Value = 10; Value += 5; Value -= 3; Value *= 2; Value /= 3; return Value; }"),
		TEXT("int Test()"),
		Result);

	TestEqual(TEXT("Misc.Assign should apply compound assignments in sequence"), Result, 8);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptMiscDuplicateFunctionTest,
	"Angelscript.TestModule.Functional.Misc.DuplicateFunction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptMiscDuplicateFunctionTest::RunTest(const FString& Parameters)
{
	bool bPassed = false;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	FAngelscriptEngineScope EngineScope(Engine);
	asIScriptEngine* ScriptEngine = Engine.GetScriptEngine();
	if (!TestNotNull(TEXT("Misc.DuplicateFunction should expose a script engine for the isolated compile-fail probe"), ScriptEngine))
	{
		return false;
	}

	ScriptEngine->SetDefaultNamespace("");
	FScopedAutomaticImportsOverride AutomaticImportsOverride(ScriptEngine);
	asIScriptModule* Module = ScriptEngine->GetModule("ASMiscDuplicateFunctionRaw", asGM_ALWAYS_CREATE);
	if (!TestNotNull(TEXT("Misc.DuplicateFunction should create a raw script module"), Module))
	{
		return false;
	}

	static constexpr ANSICHAR Script[] = "int Test() { return 42; }\nint Test() { return 42; }\n";
	Module->AddScriptSection("ASMiscDuplicateFunctionRaw", Script, UE_ARRAY_COUNT(Script) - 1);
	AddExpectedErrorPlain(TEXT("ASMiscDuplicateFunctionRaw:"), EAutomationExpectedErrorFlags::Contains, 0);
	AddExpectedErrorPlain(TEXT("A function with the same name and parameters already exists"), EAutomationExpectedErrorFlags::Contains, 0);
	const int32 BuildResult = Module->Build();
	TestEqual(TEXT("Misc.DuplicateFunction should reject duplicate global function declarations on the raw AngelScript path"), BuildResult, static_cast<int32>(asERROR));
	bPassed = BuildResult == asERROR;
	}

	return bPassed;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptMiscDuplicateFunctionRawModuleRecreateAfterFailureTest,
	"Angelscript.TestModule.Functional.Misc.DuplicateFunction.RawModuleRecreateAfterFailure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptMiscDuplicateFunctionRawModuleRecreateAfterFailureTest::RunTest(const FString& Parameters)
{
	bool bPassed = false;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	FAngelscriptEngineScope EngineScope(Engine);
	asIScriptEngine* ScriptEngine = Engine.GetScriptEngine();
	if (!TestNotNull(TEXT("Misc.DuplicateFunction.RawModuleRecreateAfterFailure should expose a script engine for the isolated raw-module probe"), ScriptEngine))
	{
		return false;
	}

	ScriptEngine->SetDefaultNamespace("");
	FScopedAutomaticImportsOverride AutomaticImportsOverride(ScriptEngine);

	static constexpr ANSICHAR ModuleName[] = "ASMiscDuplicateFunctionRawIsolation";
	static constexpr ANSICHAR DuplicateScript[] = "int Test() { return 1; }\nint Test() { return 2; }\n";
	static constexpr ANSICHAR ValidScript[] = "int Test() { return 42; }\n";

	asIScriptModule* FailingModule = ScriptEngine->GetModule(ModuleName, asGM_ALWAYS_CREATE);
	if (!TestNotNull(TEXT("Misc.DuplicateFunction.RawModuleRecreateAfterFailure should create the first raw module"), FailingModule))
	{
		return false;
	}

	FailingModule->AddScriptSection(ModuleName, DuplicateScript, UE_ARRAY_COUNT(DuplicateScript) - 1);
	AddExpectedErrorPlain(TEXT("ASMiscDuplicateFunctionRawIsolation:"), EAutomationExpectedErrorFlags::Contains, 0);
	AddExpectedErrorPlain(TEXT("A function with the same name and parameters already exists"), EAutomationExpectedErrorFlags::Contains, 0);
	const int32 FirstBuildResult = FailingModule->Build();
	if (!TestEqual(TEXT("Misc.DuplicateFunction.RawModuleRecreateAfterFailure should reject duplicate declarations on the first raw build"), FirstBuildResult, static_cast<int32>(asERROR)))
	{
		return false;
	}

	TestEqual(TEXT("Misc.DuplicateFunction.RawModuleRecreateAfterFailure should not retain failed function entries after the duplicate build"), static_cast<int32>(FailingModule->GetFunctionCount()), 0);
	TestNull(TEXT("Misc.DuplicateFunction.RawModuleRecreateAfterFailure should not expose a callable Test() after the duplicate build fails"), FailingModule->GetFunctionByDecl("int Test()"));

	if (!TestEqual(TEXT("Misc.DuplicateFunction.RawModuleRecreateAfterFailure should discard the failed raw module before recreating it"), ScriptEngine->DiscardModule(ModuleName), static_cast<int32>(asSUCCESS)))
	{
		return false;
	}

	TestNull(TEXT("Misc.DuplicateFunction.RawModuleRecreateAfterFailure should remove the failed raw module from the engine registry"), ScriptEngine->GetModule(ModuleName, asGM_ONLY_IF_EXISTS));

	asIScriptModule* RecreatedModule = ScriptEngine->GetModule(ModuleName, asGM_ALWAYS_CREATE);
	if (!TestNotNull(TEXT("Misc.DuplicateFunction.RawModuleRecreateAfterFailure should recreate the raw module after discard"), RecreatedModule))
	{
		return false;
	}

	RecreatedModule->AddScriptSection(ModuleName, ValidScript, UE_ARRAY_COUNT(ValidScript) - 1);
	if (!TestEqual(TEXT("Misc.DuplicateFunction.RawModuleRecreateAfterFailure should rebuild the discarded raw module name with clean state"), RecreatedModule->Build(), static_cast<int32>(asSUCCESS)))
	{
		return false;
	}

	TestEqual(TEXT("Misc.DuplicateFunction.RawModuleRecreateAfterFailure should expose exactly one function after rebuilding the discarded module"), static_cast<int32>(RecreatedModule->GetFunctionCount()), 1);

	asIScriptFunction* TestFunction = GetFunctionByDecl(*this, *RecreatedModule, TEXT("int Test()"));
	if (TestFunction == nullptr)
	{
		return false;
	}

	int32 Result = 0;
	if (!ExecuteIntFunction(*this, Engine, *TestFunction, Result))
	{
		return false;
	}

	TestEqual(TEXT("Misc.DuplicateFunction.RawModuleRecreateAfterFailure should execute the recreated module entry after discard"), Result, 42);
	bPassed = Result == 42;
	}

	return bPassed;
}

#endif
