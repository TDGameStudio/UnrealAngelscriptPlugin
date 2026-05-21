#include "Shared/AngelscriptTestUtilities.h"
#include "Shared/AngelscriptTestEngineHelper.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Shared/AngelscriptTestLegacyHelpers.h"
#include "Shared/AngelscriptTestUtilities.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
// Test Layer: Runtime Integration
#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;

namespace AngelscriptTest_Angelscript_AngelscriptCoreExecutionTests_Private
{
	struct FCoreEngineContextStackGuard
	{
		TArray<FAngelscriptEngine*> SavedStack;

		FCoreEngineContextStackGuard()
		{
			SavedStack = FAngelscriptEngineContextStack::SnapshotAndClear();
		}

		~FCoreEngineContextStackGuard()
		{
			FAngelscriptEngineContextStack::RestoreSnapshot(MoveTemp(SavedStack));
		}

		void DiscardSavedStack()
		{
			SavedStack.Reset();
		}
	};

	const FAngelscriptEngine::FDiagnostics* FindDiagnosticsByFilenameSuffix(const FAngelscriptEngine& Engine, const FString& FilenameSuffix)
	{
		for (const TPair<FString, FAngelscriptEngine::FDiagnostics>& Pair : Engine.Diagnostics)
		{
			if (Pair.Key.EndsWith(FilenameSuffix))
			{
				return &Pair.Value;
			}
		}

		return nullptr;
	}

	const FAngelscriptEngine::FDiagnostic* FindFirstErrorDiagnostic(const FAngelscriptEngine::FDiagnostics* FileDiagnostics)
	{
		if (FileDiagnostics == nullptr)
		{
			return nullptr;
		}

		for (const FAngelscriptEngine::FDiagnostic& Diagnostic : FileDiagnostics->Diagnostics)
		{
			if (Diagnostic.bIsError)
			{
				return &Diagnostic;
			}
		}

		return nullptr;
	}
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptCoreCreateCompileExecuteTest,
	"Angelscript.TestModule.Functional.Core.CreateCompileExecute",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptCoreCreateCompileExecuteTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Angelscript_AngelscriptCoreExecutionTests_Private;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	int32 Result = 0;
	ASTEST_COMPILE_RUN_INT(Engine, "ASCoreCreateCompileExecute",
		TEXT("int DoubleValue(int Value) { return Value * 2; } int Run() { return DoubleValue(21); }"),
		TEXT("int Run()"), Result);

	TestEqual(TEXT("Core create/compile/execute should return the expected value"), Result, 42);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptCoreCreateCompileExecuteFreshEngineBootstrapTest,
	"Angelscript.TestModule.Functional.Core.CreateCompileExecute.FreshEngineBootstrap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptCoreCreateCompileExecuteFreshEngineBootstrapTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Angelscript_AngelscriptCoreExecutionTests_Private;
	static constexpr ANSICHAR ModuleNameAnsi[] = "ASCoreFreshBootstrap";
	static const FName ModuleName(TEXT("ASCoreFreshBootstrap"));
	static const FString Script = TEXT("int DoubleValue(int Value) { return Value * 2; } int Run() { return DoubleValue(21); }");

	FCoreEngineContextStackGuard ContextGuard;

	const FAngelscriptEngineConfig Config;
	const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
	TUniquePtr<FAngelscriptEngine> LocalEngine = AngelscriptTestSupport::CreateScriptScanFreeEngineForTesting(
		Config,
		Dependencies);
	if (!TestNotNull(TEXT("Core.CreateCompileExecute.FreshEngineBootstrap should create a fresh full test engine"), LocalEngine.Get()))
	{
		return false;
	}

	bool bPassed = true;
	bPassed &= TestNotNull(
		TEXT("Core.CreateCompileExecute.FreshEngineBootstrap should initialize an underlying script engine"),
		LocalEngine->GetScriptEngine());
	bPassed &= TestEqual(
		TEXT("Core.CreateCompileExecute.FreshEngineBootstrap should start with zero active modules"),
		LocalEngine->GetActiveModules().Num(),
		0);

	FAngelscriptEngineScope EngineScope(*LocalEngine);
	bPassed &= TestTrue(
		TEXT("Core.CreateCompileExecute.FreshEngineBootstrap should use the fresh engine as the active scope"),
		FAngelscriptEngine::TryGetCurrentEngine() == LocalEngine.Get());

	asIScriptModule* Module = BuildModule(*this, *LocalEngine, ModuleNameAnsi, Script);
	if (!TestNotNull(TEXT("Core.CreateCompileExecute.FreshEngineBootstrap should compile the first module on the fresh engine"), Module))
	{
		return false;
	}

	bPassed &= TestEqual(
		TEXT("Core.CreateCompileExecute.FreshEngineBootstrap should register exactly one active module after compile"),
		LocalEngine->GetActiveModules().Num(),
		1);
	bPassed &= TestTrue(
		TEXT("Core.CreateCompileExecute.FreshEngineBootstrap should expose the compiled module through module lookup"),
		LocalEngine->GetModuleByModuleName(ModuleName.ToString()).IsValid());

	asIScriptFunction* RunFunction = GetFunctionByDecl(*this, *Module, TEXT("int Run()"));
	if (RunFunction == nullptr)
	{
		return false;
	}

	asIScriptContext* Context = LocalEngine->CreateContext();
	if (!TestNotNull(TEXT("Core.CreateCompileExecute.FreshEngineBootstrap should create the first execution context on the fresh engine"), Context))
	{
		return false;
	}

	ON_SCOPE_EXIT
	{
		if (Context != nullptr)
		{
			Context->Release();
		}
	};

	const int PrepareResult = Context->Prepare(RunFunction);
	bPassed &= TestEqual(
		TEXT("Core.CreateCompileExecute.FreshEngineBootstrap should prepare the first context successfully"),
		PrepareResult,
		asSUCCESS);
	if (PrepareResult != asSUCCESS)
	{
		return false;
	}

	const int ExecuteResult = Context->Execute();
	bPassed &= TestEqual(
		TEXT("Core.CreateCompileExecute.FreshEngineBootstrap should execute the first context successfully"),
		ExecuteResult,
		asEXECUTION_FINISHED);
	if (ExecuteResult != asEXECUTION_FINISHED)
	{
		return false;
	}

	bPassed &= TestEqual(
		TEXT("Core.CreateCompileExecute.FreshEngineBootstrap should return the expected bootstrap result"),
		static_cast<int32>(Context->GetReturnDWord()),
		42);

	Context->Release();
	Context = nullptr;

	bPassed &= TestTrue(
		TEXT("Core.CreateCompileExecute.FreshEngineBootstrap should discard the bootstrap module"),
		LocalEngine->DiscardModule(*ModuleName.ToString()));
	bPassed &= TestEqual(
		TEXT("Core.CreateCompileExecute.FreshEngineBootstrap should return to zero active modules after discard"),
		LocalEngine->GetActiveModules().Num(),
		0);
	bPassed &= TestFalse(
		TEXT("Core.CreateCompileExecute.FreshEngineBootstrap should clear module lookup after discard"),
		LocalEngine->GetModuleByModuleName(ModuleName.ToString()).IsValid());

	return bPassed;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptCoreGlobalStateTest,
	"Angelscript.TestModule.Functional.Core.GlobalState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptCoreGlobalStateTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Angelscript_AngelscriptCoreExecutionTests_Private;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	int32 Result = 0;
	ASTEST_COMPILE_RUN_INT(Engine, "ASCoreGlobalState",
		TEXT("const int g_Count = 3; int Step(int Value) { return Value + 4; } int Run() { return Step(g_Count); }"),
		TEXT("int Run()"), Result);

	TestEqual(TEXT("Const globals and helper calls should evaluate as expected"), Result, 7);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptCoreCreateEngineTest,
	"Angelscript.TestModule.Functional.Core.CreateEngine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptCoreCreateEngineTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Angelscript_AngelscriptCoreExecutionTests_Private;
	FAngelscriptEngineConfig Config;
	FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
	TUniquePtr<FAngelscriptEngine> LocalEngineA = AngelscriptTestSupport::CreateScriptScanFreeEngineForTesting(Config, Dependencies);
	TUniquePtr<FAngelscriptEngine> LocalEngineB = AngelscriptTestSupport::CreateScriptScanFreeEngineForTesting(Config, Dependencies);
	if (!TestNotNull(TEXT("Core.CreateEngine should create a first test engine wrapper"), LocalEngineA.Get()))
	{
		return false;
	}
	if (!TestNotNull(TEXT("Core.CreateEngine should create a second test engine wrapper"), LocalEngineB.Get()))
	{
		return false;
	}

	asIScriptEngine* ScriptEngineA = LocalEngineA->GetScriptEngine();
	asIScriptEngine* ScriptEngineB = LocalEngineB->GetScriptEngine();
	TestNotNull(TEXT("Core.CreateEngine should create the first asIScriptEngine for the returned wrapper"), ScriptEngineA);
	TestNotNull(TEXT("Core.CreateEngine should create the second asIScriptEngine for the returned wrapper"), ScriptEngineB);

	TestEqual(TEXT("Core.CreateEngine should preserve the embedded AngelScript version"), ANGELSCRIPT_VERSION, 23300);
	return ScriptEngineA != nullptr && ScriptEngineB != nullptr;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptCoreCreateEngineIsolatedModuleRegistriesTest,
	"Angelscript.TestModule.Functional.Core.CreateEngine.IsolatedModuleRegistries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAngelscriptCoreCreateEngineIsolatedModuleRegistriesTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Angelscript_AngelscriptCoreExecutionTests_Private;
	const FName ModuleName(TEXT("ASCoreCreateEngineIsolationA"));
	const FAngelscriptEngineConfig Config;
	const FAngelscriptEngineDependencies Dependencies = FAngelscriptEngineDependencies::CreateDefault();
	TUniquePtr<FAngelscriptEngine> EngineA = AngelscriptTestSupport::CreateScriptScanFreeFullEngineForTesting(Config, Dependencies);
	TUniquePtr<FAngelscriptEngine> EngineB = AngelscriptTestSupport::CreateScriptScanFreeFullEngineForTesting(Config, Dependencies);
	int32 Result = 0;
	if (!TestNotNull(TEXT("Core.CreateEngine.IsolatedModuleRegistries should create engine A"), EngineA.Get()) || !TestNotNull(TEXT("Core.CreateEngine.IsolatedModuleRegistries should create engine B"), EngineB.Get())) return false;
	if (!TestTrue(TEXT("Core.CreateEngine.IsolatedModuleRegistries should create distinct script engines"), EngineA->GetScriptEngine() != nullptr && EngineB->GetScriptEngine() != nullptr && EngineA->GetScriptEngine() != EngineB->GetScriptEngine())
		|| !TestTrue(TEXT("Core.CreateEngine.IsolatedModuleRegistries should compile the module only on engine A"), CompileModuleFromMemory(EngineA.Get(), ModuleName, TEXT("ASCoreCreateEngineIsolationA.as"), TEXT("int Run() { return 42; }")))
		|| !TestTrue(TEXT("Core.CreateEngine.IsolatedModuleRegistries should execute Run() on engine A"), ExecuteIntFunction(EngineA.Get(), ModuleName, TEXT("int Run()"), Result))
		|| !TestEqual(TEXT("Core.CreateEngine.IsolatedModuleRegistries should return the compiled value on engine A"), Result, 42)
		|| !TestTrue(TEXT("Core.CreateEngine.IsolatedModuleRegistries should register the module on engine A"), EngineA->GetModuleByModuleName(ModuleName.ToString()).IsValid())
		|| !TestFalse(TEXT("Core.CreateEngine.IsolatedModuleRegistries should keep engine B module lookup empty"), EngineB->GetModuleByModuleName(ModuleName.ToString()).IsValid())
		|| !TestTrue(TEXT("Core.CreateEngine.IsolatedModuleRegistries should discard the module from engine A"), EngineA->DiscardModule(*ModuleName.ToString()))
		|| !TestFalse(TEXT("Core.CreateEngine.IsolatedModuleRegistries should keep engine B empty after engine A discard"), EngineB->GetModuleByModuleName(ModuleName.ToString()).IsValid())) return false;
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptCoreModuleLookupFilenameThenModuleFallbackTest,
	"Angelscript.TestModule.Functional.Core.ModuleLookup.FilenameThenModuleFallback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptCoreModuleLookupFilenameThenModuleFallbackTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Angelscript_AngelscriptCoreExecutionTests_Private;
	static const FName ModuleName(TEXT("ASCoreModuleLookupProbe"));
	static const FString RelativeFilename(TEXT("Lookup/ModuleLookup/FilenameFallback.as"));
	static const FString MissingFilename(TEXT("Z:/DefinitelyMissing/ModuleLookupProbe.as"));
	static const FString WrongModuleName(TEXT("DefinitelyWrongName"));
	static const FString Script(TEXT("int Run() { return 42; }"));

	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	bool bPassed = true;
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	const FString AbsoluteFilename = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), RelativeFilename);
	const FName WrongModuleFName(*WrongModuleName);
	if (!TestTrue(
			TEXT("Core.ModuleLookup.FilenameThenModuleFallback should compile the probe module"),
			CompileModuleFromMemory(&Engine, ModuleName, RelativeFilename, Script)))
	{
		return false;
	}

	const TSharedPtr<FAngelscriptModuleDesc> FilenameModule = Engine.GetModuleByFilename(AbsoluteFilename);
	const TSharedPtr<FAngelscriptModuleDesc> FilenameHitModule = Engine.GetModuleByFilenameOrModuleName(AbsoluteFilename, WrongModuleName);
	const TSharedPtr<FAngelscriptModuleDesc> FallbackModule = Engine.GetModuleByFilenameOrModuleName(MissingFilename, ModuleName.ToString());
	const TSharedPtr<FAngelscriptModuleDesc> MissingModule = Engine.GetModuleByFilenameOrModuleName(MissingFilename, WrongModuleName);

	bPassed &= TestNotNull(
		TEXT("Core.ModuleLookup.FilenameThenModuleFallback should find the module by absolute filename"),
		FilenameModule.Get());
	bPassed &= TestNotNull(
		TEXT("Core.ModuleLookup.FilenameThenModuleFallback should prefer filename hits even when the module name argument is wrong"),
		FilenameHitModule.Get());
	bPassed &= TestNotNull(
		TEXT("Core.ModuleLookup.FilenameThenModuleFallback should fall back to module-name lookup when filename misses"),
		FallbackModule.Get());
	bPassed &= TestFalse(
		TEXT("Core.ModuleLookup.FilenameThenModuleFallback should return null when both filename and module name miss"),
		MissingModule.IsValid());

	if (FilenameModule.IsValid())
	{
		bPassed &= TestEqual(
			TEXT("Core.ModuleLookup.FilenameThenModuleFallback should keep the explicit module name on the compiled descriptor"),
			FilenameModule->ModuleName,
			ModuleName.ToString());
		bPassed &= TestEqual(
			TEXT("Core.ModuleLookup.FilenameThenModuleFallback should keep a single code section on the compiled descriptor"),
			FilenameModule->Code.Num(),
			1);
		if (FilenameModule->Code.Num() == 1)
		{
			bPassed &= TestTrue(
				TEXT("Core.ModuleLookup.FilenameThenModuleFallback should preserve the absolute automation filename on the code section"),
				FilenameModule->Code[0].AbsoluteFilename.Equals(AbsoluteFilename, ESearchCase::IgnoreCase));
		}
		bPassed &= TestNotNull(
			TEXT("Core.ModuleLookup.FilenameThenModuleFallback should keep the active script module on filename lookup"),
			FilenameModule->ScriptModule);
	}

	if (FilenameModule.IsValid() && FilenameHitModule.IsValid())
	{
		bPassed &= TestTrue(
			TEXT("Core.ModuleLookup.FilenameThenModuleFallback should return the same descriptor for direct filename lookup and filename-first combined lookup"),
			FilenameHitModule.Get() == FilenameModule.Get());
	}

	if (FilenameModule.IsValid() && FallbackModule.IsValid())
	{
		bPassed &= TestTrue(
			TEXT("Core.ModuleLookup.FilenameThenModuleFallback should return the same descriptor after module-name fallback"),
			FallbackModule.Get() == FilenameModule.Get());
		bPassed &= TestTrue(
			TEXT("Core.ModuleLookup.FilenameThenModuleFallback should keep the same active script module after fallback"),
			FallbackModule->ScriptModule == FilenameModule->ScriptModule);
	}

	int32 FilenameResult = 0;
	bPassed &= TestTrue(
		TEXT("Core.ModuleLookup.FilenameThenModuleFallback should execute through the filename-hit path even when the module name argument is wrong"),
		ExecuteIntFunction(&Engine, AbsoluteFilename, WrongModuleFName, TEXT("int Run()"), FilenameResult));
	bPassed &= TestEqual(
		TEXT("Core.ModuleLookup.FilenameThenModuleFallback should return the probe value through the filename-hit path"),
		FilenameResult,
		42);

	int32 FallbackResult = 0;
	bPassed &= TestTrue(
		TEXT("Core.ModuleLookup.FilenameThenModuleFallback should execute through the module-name fallback path"),
		ExecuteIntFunction(&Engine, MissingFilename, ModuleName, TEXT("int Run()"), FallbackResult));
	bPassed &= TestEqual(
		TEXT("Core.ModuleLookup.FilenameThenModuleFallback should return the probe value through the fallback path"),
		FallbackResult,
		42);

	}
	return bPassed;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptCompilerBasicTest,
	"Angelscript.TestModule.Functional.Core.CompilerBasic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptCompilerBasicTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Angelscript_AngelscriptCoreExecutionTests_Private;
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
	const bool bCompiledSimple = CompileModuleFromMemory(
		&Engine,
		TEXT("ASCoreCompilerBasicSimple"),
		TEXT("ASCoreCompilerBasicSimple.as"),

		TEXT("void Main() { int Value = 1; }"));
	if (!TestTrue(TEXT("Core.CompilerBasic should compile a simple function"), bCompiledSimple))
	{
		return false;
	}
	const bool bCompiledMulti = CompileModuleFromMemory(
		&Engine,
		TEXT("ASCoreCompilerBasicMulti"),
		TEXT("ASCoreCompilerBasicMulti.as"),

		TEXT("void Func1() {} void Func2() {} void Func3() {}"));
	if (!TestTrue(TEXT("Core.CompilerBasic should compile a module with multiple functions"), bCompiledMulti))
	{
		return false;
	}

	TSharedPtr<FAngelscriptModuleDesc> MultiModuleDesc = Engine.GetModuleByModuleName(TEXT("ASCoreCompilerBasicMulti"));
	asIScriptModule* MultiModule = MultiModuleDesc.IsValid() ? MultiModuleDesc->ScriptModule : nullptr;
	if (!TestNotNull(TEXT("Core.CompilerBasic should register the multi-function module"), MultiModule))
	{
		return false;
	}
	if (!TestEqual(TEXT("Core.CompilerBasic should expose all compiled functions"), static_cast<int32>(MultiModule->GetFunctionCount()), 3))
	{
		return false;
	}
	const bool bCompiledGlobals = CompileModuleFromMemory(
		&Engine,
		TEXT("ASCoreCompilerBasicGlobals"),
		TEXT("ASCoreCompilerBasicGlobals.as"),

		TEXT("const int GlobalInt = 42; const float GlobalFloat = 3.14f; void Main() {}"));
	if (!TestTrue(TEXT("Core.CompilerBasic should compile global declarations"), bCompiledGlobals))
	{
		return false;
	}

	TSharedPtr<FAngelscriptModuleDesc> GlobalsModuleDesc = Engine.GetModuleByModuleName(TEXT("ASCoreCompilerBasicGlobals"));
	asIScriptModule* GlobalsModule = GlobalsModuleDesc.IsValid() ? GlobalsModuleDesc->ScriptModule : nullptr;
	if (!TestNotNull(TEXT("Core.CompilerBasic should register the globals module"), GlobalsModule))
	{
		return false;
	}
	if (!TestEqual(TEXT("Core.CompilerBasic should preserve both global declarations"), static_cast<int32>(GlobalsModule->GetGlobalVarCount()), 2))
	{
		return false;
	}

	ECompileResult ErrorCompileResult = ECompileResult::FullyHandled;
	UE_SET_LOG_VERBOSITY(Angelscript, Fatal);
	const bool bCompiledInvalid = CompileModuleWithResult(
		&Engine,
		ECompileType::SoftReloadOnly,
		TEXT("ASCoreCompilerBasicInvalid"),
		TEXT("ASCoreCompilerBasicInvalid.as"),
		TEXT("void Main( { int Value = 1; }"),
		ErrorCompileResult);
	UE_SET_LOG_VERBOSITY(Angelscript, Log);
	if (!TestFalse(TEXT("Core.CompilerBasic should fail to compile invalid syntax"), bCompiledInvalid))
	{
		return false;
	}
	TestEqual(TEXT("Core.CompilerBasic should report an error compile result for invalid syntax"), ErrorCompileResult, ECompileResult::Error);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptCompilerParserTest,
	"Angelscript.TestModule.Functional.Core.Parser",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptCompilerParserInvalidSyntaxDiagnosticsAndCleanupTest,
	"Angelscript.TestModule.Functional.Core.Parser.InvalidSyntaxDiagnosticsAndCleanup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptCompilerParserTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Angelscript_AngelscriptCoreExecutionTests_Private;
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
	const bool bCompiledValid = CompileModuleFromMemory(
		&Engine,
		TEXT("ASCoreParserValid"),
		TEXT("ASCoreParserValid.as"),

		TEXT("void Test() { int A = 1 + 2; bool bFlag = true && false; if (A > 0) { A = A + 1; } }"));
	if (!TestTrue(TEXT("Core.Parser should compile valid syntax constructs"), bCompiledValid))
	{
		return false;
	}
	const bool bCompiledNested = CompileModuleFromMemory(
		&Engine,
		TEXT("ASCoreParserNested"),
		TEXT("ASCoreParserNested.as"),

		TEXT("void Test() { { int A = 1; { int B = 2; } } }"));
	if (!TestTrue(TEXT("Core.Parser should compile nested blocks"), bCompiledNested))
	{
		return false;
	}

	ECompileResult InvalidCompileResult = ECompileResult::FullyHandled;
	UE_SET_LOG_VERBOSITY(Angelscript, Fatal);
	const bool bCompiledInvalid = CompileModuleWithResult(
		&Engine,
		ECompileType::SoftReloadOnly,
		TEXT("ASCoreParserInvalid"),
		TEXT("ASCoreParserInvalid.as"),
		TEXT("void Test( { int A = 1; }"),
		InvalidCompileResult);
	UE_SET_LOG_VERBOSITY(Angelscript, Log);
	if (!TestFalse(TEXT("Core.Parser should reject invalid syntax"), bCompiledInvalid))
	{
		return false;
	}
	TestEqual(TEXT("Core.Parser should report an error compile result for invalid syntax"), InvalidCompileResult, ECompileResult::Error);
	}

	return true;
}

bool FAngelscriptCompilerParserInvalidSyntaxDiagnosticsAndCleanupTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Angelscript_AngelscriptCoreExecutionTests_Private;
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
	static const FName ModuleName(TEXT("ASCoreParserInvalidCleanup"));
	static const FString Filename(TEXT("ASCoreParserInvalidCleanup.as"));

	Engine.ResetDiagnostics();
	Engine.LastEmittedDiagnostics.Empty();

	ECompileResult InvalidCompileResult = ECompileResult::FullyHandled;
	UE_SET_LOG_VERBOSITY(Angelscript, Fatal);
	const bool bCompiledInvalid = CompileModuleWithResult(
		&Engine,
		ECompileType::SoftReloadOnly,
		ModuleName,
		Filename,
		TEXT("void Test( { int A = 1; }"),
		InvalidCompileResult);
	UE_SET_LOG_VERBOSITY(Angelscript, Log);

	if (!TestFalse(TEXT("Core.Parser invalid-syntax recovery should fail the broken compile"), bCompiledInvalid))
	{
		return false;
	}
	if (!TestEqual(TEXT("Core.Parser invalid-syntax recovery should surface an error compile result"), InvalidCompileResult, ECompileResult::Error))
	{
		return false;
	}

	const FAngelscriptEngine::FDiagnostics* InvalidDiagnostics = FindDiagnosticsByFilenameSuffix(Engine, Filename);
	if (!TestNotNull(TEXT("Core.Parser invalid-syntax recovery should capture diagnostics for the broken file"), InvalidDiagnostics))
	{
		return false;
	}

	const FAngelscriptEngine::FDiagnostic* InvalidDiagnostic = FindFirstErrorDiagnostic(InvalidDiagnostics);
	if (!TestNotNull(TEXT("Core.Parser invalid-syntax recovery should capture at least one error diagnostic"), InvalidDiagnostic))
	{
		return false;
	}

	TestTrue(TEXT("Core.Parser invalid-syntax recovery should preserve the failing filename in diagnostics"), InvalidDiagnostics->Filename.EndsWith(Filename));
	TestTrue(TEXT("Core.Parser invalid-syntax recovery should report a non-zero diagnostic row"), InvalidDiagnostic->Row > 0);
	TestTrue(TEXT("Core.Parser invalid-syntax recovery should report a non-zero diagnostic column"), InvalidDiagnostic->Column > 0);
	TestTrue(
		TEXT("Core.Parser invalid-syntax recovery should keep a syntax-oriented diagnostic message"),
		InvalidDiagnostic->Message.Contains(TEXT("Expected"))
			|| InvalidDiagnostic->Message.Contains(TEXT("Unexpected"))
			|| InvalidDiagnostic->Message.Contains(TEXT("instead")));

	const TSharedPtr<FAngelscriptModuleDesc> FailedModuleDesc = Engine.GetModuleByModuleName(ModuleName.ToString());
	if (FailedModuleDesc.IsValid() && FailedModuleDesc->ScriptModule != nullptr)
	{
		TestEqual(
			TEXT("Core.Parser invalid-syntax recovery should leave zero functions on a failed module record"),
			static_cast<int32>(FailedModuleDesc->ScriptModule->GetFunctionCount()),
			0);
		TestEqual(
			TEXT("Core.Parser invalid-syntax recovery should leave zero globals on a failed module record"),
			static_cast<int32>(FailedModuleDesc->ScriptModule->GetGlobalVarCount()),
			0);
	}
	else
	{
		TestFalse(TEXT("Core.Parser invalid-syntax recovery should not keep a live module record after failure"), FailedModuleDesc.IsValid());
	}

	Engine.ResetDiagnostics();
	Engine.LastEmittedDiagnostics.Empty();

	ECompileResult FixedCompileResult = ECompileResult::Error;
	const bool bCompiledFixed = CompileModuleWithResult(
		&Engine,
		ECompileType::SoftReloadOnly,
		ModuleName,
		Filename,
		TEXT("int Test() { return 42; }"),
		FixedCompileResult);
	if (!TestTrue(TEXT("Core.Parser invalid-syntax recovery should compile the fixed script after failure"), bCompiledFixed))
	{
		return false;
	}
	if (!TestTrue(
			TEXT("Core.Parser invalid-syntax recovery should report a handled compile result after retry"),
			FixedCompileResult == ECompileResult::FullyHandled || FixedCompileResult == ECompileResult::PartiallyHandled))
	{
		return false;
	}

	int32 Result = 0;
	if (!TestTrue(TEXT("Core.Parser invalid-syntax recovery should execute the fixed function after retry"), ExecuteIntFunction(&Engine, ModuleName, TEXT("int Test()"), Result)))
	{
		return false;
	}
	TestEqual(TEXT("Core.Parser invalid-syntax recovery should return the fixed result after retry"), Result, 42);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptCompilerOptimizeTest,
	"Angelscript.TestModule.Functional.Core.Optimize",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptCompilerOptimizeTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Angelscript_AngelscriptCoreExecutionTests_Private;
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
	const bool bCompiledConstant = CompileModuleFromMemory(
		&Engine,
		TEXT("ASCoreOptimizeConstant"),
		TEXT("ASCoreOptimizeConstant.as"),

		TEXT("int Test() { return 1 + 2 + 3; }"));
	if (!TestTrue(TEXT("Core.Optimize should compile the constant-folding case"), bCompiledConstant))
	{
		return false;
	}

	int32 ConstantResult = 0;
	if (!TestTrue(TEXT("Core.Optimize should execute the constant-folding case"), AngelscriptTestSupport::ExecuteIntFunction(&Engine, TEXT("ASCoreOptimizeConstant"), TEXT("int Test()"), ConstantResult)))
	{
		return false;
	}
	TestEqual(TEXT("Core.Optimize should preserve constant-folded results"), ConstantResult, 6);
	const bool bCompiledDeadCode = CompileModuleFromMemory(
		&Engine,
		TEXT("ASCoreOptimizeDeadCode"),
		TEXT("ASCoreOptimizeDeadCode.as"),

		TEXT("int Test() { int Value = 1; return Value; Value = 2; }"));
	if (!TestTrue(TEXT("Core.Optimize should compile the dead-code case"), bCompiledDeadCode))
	{
		return false;
	}

	int32 DeadCodeResult = 0;
	if (!TestTrue(TEXT("Core.Optimize should execute the dead-code case"), AngelscriptTestSupport::ExecuteIntFunction(&Engine, TEXT("ASCoreOptimizeDeadCode"), TEXT("int Test()"), DeadCodeResult)))
	{
		return false;
	}
	TestEqual(TEXT("Core.Optimize should keep reachable results stable when dead code is present"), DeadCodeResult, 1);
	}

	return true;
}

#endif
