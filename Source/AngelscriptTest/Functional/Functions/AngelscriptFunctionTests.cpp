#include "Shared/AngelscriptTestUtilities.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Shared/AngelscriptTestMacros.h"
#include "Shared/AngelscriptTestLegacyHelpers.h"

// Test Layer: Runtime Integration
#if WITH_DEV_AUTOMATION_TESTS

using namespace AngelscriptTestSupport;

namespace AngelscriptTest_Angelscript_AngelscriptFunctionTests_Private
{
	const FAngelscriptCompileTraceDiagnosticSummary* FindDiagnosticContaining(
		const FAngelscriptCompileTraceSummary& Summary,
		const FString& Needle)
	{
		for (const FAngelscriptCompileTraceDiagnosticSummary& Diagnostic : Summary.Diagnostics)
		{
			if (Diagnostic.Message.Contains(Needle))
			{
				return &Diagnostic;
			}
		}

		return nullptr;
	}
}


namespace AngelscriptTestSupport
{
	bool CompileModuleWithResult(FAngelscriptEngine* Engine, ECompileType CompileType, FName ModuleName, FString Filename, FString Script, ECompileResult& OutCompileResult);
	void ResetSharedCloneEngine(FAngelscriptEngine& Engine);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptFunctionDefaultArgumentsTest,
	"Angelscript.TestModule.Functional.Functions.DefaultArguments",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptFunctionDefaultArgumentsTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Angelscript_AngelscriptFunctionTests_Private;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	int32 Result = 0;
	ASTEST_COMPILE_RUN_INT(Engine,
		"ASFunctionDefaultArguments",
		TEXT("int Add(int A, int B = 5) { return A + B; } int Run() { return Add(7); }"),
		TEXT("int Run()"),
		Result);

	TestEqual(TEXT("Default arguments should be applied when omitted"), Result, 12);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptFunctionNamedArgumentsTest,
	"Angelscript.TestModule.Functional.Functions.NamedArguments",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptFunctionNamedArgumentsTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Angelscript_AngelscriptFunctionTests_Private;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	int32 Result = 0;
	ASTEST_COMPILE_RUN_INT(Engine,
		"ASFunctionNamedArguments",
		TEXT("int Mix(int A, int B, int C) { return A + B * 10 + C * 100; } int Run() { return Mix(C: 3, A: 1, B: 2); }"),
		TEXT("int Run()"),
		Result);

	TestEqual(TEXT("Named arguments should bind to the intended parameters"), Result, 321);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptFunctionNamedArgumentsMixedPartialOrderTest,
	"Angelscript.TestModule.Functional.Functions.NamedArguments.MixedPartialOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptFunctionNamedArgumentsMixedPartialOrderTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Angelscript_AngelscriptFunctionTests_Private;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	int32 Result = 0;
	ASTEST_COMPILE_RUN_INT(Engine,
		"ASFunctionNamedArgumentsMixedPartialOrder",
		TEXT(R"(
int Mix(int A, int B, int C)
{
	return A * 100 + B * 10 + C;
}

int RunMixed()
{
	return Mix(4, C: 6, B: 5);
}

int RunPartial()
{
	return Mix(A: 7, C: 9, B: 8);
}

int Run()
{
	return RunMixed() * 1000 + RunPartial();
}
)"),
		TEXT("int Run()"),
		Result);

	TestEqual(TEXT("Mixed positional and named arguments should keep parameter-name binding for the reordered suffix"), Result, 456789);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptFunctionNamedArgumentsInvalidNameDiagnosticsTest,
	"Angelscript.TestModule.Functional.Functions.NamedArguments.InvalidNameDiagnostics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptFunctionNamedArgumentsInvalidNameDiagnosticsTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Angelscript_AngelscriptFunctionTests_Private;
	bool bPassed = true;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	auto VerifyInvalidNamedArguments = [this, &Engine, &bPassed](
		const FName ModuleName,
		const FString& ScriptFilename,
		const FString& ScriptSource,
		const FString& TestCaseLabel,
		const FString& ExpectedMessage)
	{
		FAngelscriptCompileTraceSummary Summary;
		const bool bCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::SoftReloadOnly,
			ModuleName,
			ScriptFilename,
			ScriptSource,
			false,
			Summary,
			true);
		const FAngelscriptCompileTraceDiagnosticSummary* Diagnostic = FindDiagnosticContaining(Summary, ExpectedMessage);
		asIScriptModule* FailedModule = Engine.GetScriptEngine()->GetModule(TCHAR_TO_UTF8(*ModuleName.ToString()), asGM_ONLY_IF_EXISTS);
		asIScriptFunction* RunFunction = FailedModule != nullptr ? FailedModule->GetFunctionByDecl("int Run()") : nullptr;

		bPassed &= TestFalse(
			*FString::Printf(TEXT("%s should fail to compile"), *TestCaseLabel),
			bCompiled);
		bPassed &= TestFalse(
			*FString::Printf(TEXT("%s should report bCompileSucceeded=false"), *TestCaseLabel),
			Summary.bCompileSucceeded);
		bPassed &= TestEqual(
			*FString::Printf(TEXT("%s should surface ECompileResult::Error"), *TestCaseLabel),
			Summary.CompileResult,
			ECompileResult::Error);
		bPassed &= TestTrue(
			*FString::Printf(TEXT("%s should collect at least one diagnostic"), *TestCaseLabel),
			Summary.Diagnostics.Num() > 0);
		bPassed &= TestNotNull(
			*FString::Printf(TEXT("%s should emit a diagnostic containing '%s'"), *TestCaseLabel, *ExpectedMessage),
			Diagnostic);
		if (Diagnostic != nullptr)
		{
			bPassed &= TestTrue(
				*FString::Printf(TEXT("%s should report a non-zero diagnostic row"), *TestCaseLabel),
				Diagnostic->Row > 0);
			bPassed &= TestTrue(
				*FString::Printf(TEXT("%s should report a non-zero diagnostic column"), *TestCaseLabel),
				Diagnostic->Column > 0);
		}
		bPassed &= TestNull(
			*FString::Printf(TEXT("%s should not leave an executable Run() behind after compile failure"), *TestCaseLabel),
			RunFunction);
	};

	VerifyInvalidNamedArguments(
		TEXT("ASFunctionNamedArgumentsDuplicateDiagnostic"),
		FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("NegativeCompileIsolation"), TEXT("ASFunctionNamedArgumentsDuplicateDiagnostic.as")),
		TEXT(R"(
int Mix(int A, int B, int C)
{
	return 0;
}

int Run()
{
	return Mix(A: 1, A: 2, C: 3);
}
)"),
		TEXT("Duplicate named argument"),
		TEXT("Duplicate named argument"));

	VerifyInvalidNamedArguments(
		TEXT("ASFunctionNamedArgumentsUnknownDiagnostic"),
		FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("NegativeCompileIsolation"), TEXT("ASFunctionNamedArgumentsUnknownDiagnostic.as")),
		TEXT(R"(
int Mix(int A, int B, int C)
{
	return 0;
}

int Run()
{
	return Mix(A: 1, D: 2, C: 3);
}
)"),
		TEXT("Unknown named argument"),
		TEXT("Unknown parameter 'D'"));

	}
	return bPassed;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptFunctionDefaultArgumentsOverrideAndNamedMixTest,
	"Angelscript.TestModule.Functional.Functions.DefaultArguments.OverrideAndNamedMix",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptFunctionDefaultArgumentsOverrideAndNamedMixTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Angelscript_AngelscriptFunctionTests_Private;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		"ASFunctionDefaultArgumentsOverrideAndNamedMix",
		TEXT(R"(
int Format(int A, int B = 5, int C = 9)
{
	return A * 100 + B * 10 + C;
}

int RunDefault()
{
	return Format(1);
}

int RunOverride()
{
	return Format(1, 2);
}

int RunNamedPartial()
{
	return Format(A: 1, C: 3);
}
)"));
	if (Module == nullptr)
	{
		return false;
	}

	asIScriptFunction* DefaultFunction = GetFunctionByDecl(*this, *Module, TEXT("int RunDefault()"));
	asIScriptFunction* OverrideFunction = GetFunctionByDecl(*this, *Module, TEXT("int RunOverride()"));
	asIScriptFunction* NamedPartialFunction = GetFunctionByDecl(*this, *Module, TEXT("int RunNamedPartial()"));
	if (DefaultFunction == nullptr || OverrideFunction == nullptr || NamedPartialFunction == nullptr)
	{
		return false;
	}

	int32 DefaultResult = 0;
	int32 OverrideResult = 0;
	int32 NamedPartialResult = 0;
	if (!ExecuteIntFunction(*this, Engine, *DefaultFunction, DefaultResult)
		|| !ExecuteIntFunction(*this, Engine, *OverrideFunction, OverrideResult)
		|| !ExecuteIntFunction(*this, Engine, *NamedPartialFunction, NamedPartialResult))
	{
		return false;
	}

	TestEqual(TEXT("Default arguments should fill both omitted trailing parameters"), DefaultResult, 159);
	TestEqual(TEXT("Explicit arguments should override the middle default while preserving the trailing default"), OverrideResult, 129);
	TestEqual(TEXT("Named arguments should override only the addressed default parameter"), NamedPartialResult, 153);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptFunctionPointerAndOverloadTest,
	"Angelscript.TestModule.Functional.Functions.OverloadResolution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptFunctionPointerAndOverloadTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Angelscript_AngelscriptFunctionTests_Private;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	int32 Result = 0;
	ASTEST_COMPILE_RUN_INT(Engine,
		"ASFunctionPointerAndOverload",
		TEXT("int Convert(int Value) { return Value + 1; } int Convert(float Value) { return int(Value * 3.0f); } int Run() { return Convert(4) + Convert(2.0f); }"),
		TEXT("int Run()"),
		Result);

	TestEqual(TEXT("Overload resolution should choose the expected function bodies"), Result, 11);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptFunctionPointerTest,
	"Angelscript.TestModule.Functional.Functions.Pointer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptFunctionPointerTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Angelscript_AngelscriptFunctionTests_Private;
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

	const FString ScriptFilename = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("NegativeCompileIsolation"), TEXT("ASFunctionPointer.as"));
	ECompileResult CompileResult = ECompileResult::FullyHandled;
	UE_SET_LOG_VERBOSITY(Angelscript, Fatal);
	const bool bCompiled = CompileModuleWithResult(
		&Engine,
		ECompileType::SoftReloadOnly,
		TEXT("ASFunctionPointer"),
		ScriptFilename,
		TEXT("funcdef int FUNC(int Value); int Callback(int Value) { return Value * 2; } int Run() { FUNC@ FunctionRef = @Callback; return FunctionRef(21); }"),
		CompileResult);
	UE_SET_LOG_VERBOSITY(Angelscript, Log);
	TestFalse(TEXT("Function pointer syntax should remain unsupported on the current branch"), bCompiled);
	bPassed = !bCompiled;
	}

	return bPassed;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptFunctionConstructorTest,
	"Angelscript.TestModule.Functional.Functions.Constructor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptFunctionConstructorTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Angelscript_AngelscriptFunctionTests_Private;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	asIScriptModule* Module = BuildModule(
		*this,
		Engine,
		"ASFunctionConstructor",
		TEXT("class ConstructorCarrier { int Value; ConstructorCarrier() { Value = 42; } ConstructorCarrier(int InValue) { Value = InValue; } } int Run() { ConstructorCarrier DefaultCarrier; return DefaultCarrier.Value; }"));
	if (Module == nullptr)
	{
		return false;
	}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptFunctionDestructorTest,
	"Angelscript.TestModule.Functional.Functions.Destructor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptFunctionDestructorTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Angelscript_AngelscriptFunctionTests_Private;
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);

	int32 Result = 0;
	ASTEST_COMPILE_RUN_INT(Engine,
		"ASFunctionDestructor",
		TEXT("class DestructorCarrier { ~DestructorCarrier() {} } int Run() { DestructorCarrier Carrier; return 1; }"),
		TEXT("int Run()"),
		Result);

	TestEqual(TEXT("Destructor declarations should compile and execute in local scope"), Result, 1);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptFunctionTemplateTest,
	"Angelscript.TestModule.Functional.Functions.Template",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptFunctionTemplateTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Angelscript_AngelscriptFunctionTests_Private;
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

	const FString ScriptFilename = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("NegativeCompileIsolation"), TEXT("ASFunctionTemplate.as"));
	ECompileResult CompileResult = ECompileResult::FullyHandled;
	UE_SET_LOG_VERBOSITY(Angelscript, Fatal);
	const bool bCompiled = CompileModuleWithResult(
		&Engine,
		ECompileType::SoftReloadOnly,
		TEXT("ASFunctionTemplate"),
		ScriptFilename,
		TEXT("class TemplateCarrier<T> { T Value; void Set(T InValue) { Value = InValue; } T Get() { return Value; } } int Run() { TemplateCarrier<int> Carrier; Carrier.Set(42); return Carrier.Get(); }"),
		CompileResult);
	UE_SET_LOG_VERBOSITY(Angelscript, Log);
	TestFalse(TEXT("Template syntax should currently remain unsupported on this 2.33-based branch"), bCompiled);
	bPassed = !bCompiled;
	}

	return bPassed;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptFunctionFactoryTest,
	"Angelscript.TestModule.Functional.Functions.Factory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptFunctionFactoryTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Angelscript_AngelscriptFunctionTests_Private;
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

	const FString ScriptFilename = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("NegativeCompileIsolation"), TEXT("ASFunctionFactory.as"));
	ECompileResult CompileResult = ECompileResult::FullyHandled;
	UE_SET_LOG_VERBOSITY(Angelscript, Fatal);
	const bool bCompiled = CompileModuleWithResult(
		&Engine,
		ECompileType::SoftReloadOnly,
		TEXT("ASFunctionFactory"),
		ScriptFilename,
		TEXT("class FactoryCarrier { int Value; } FactoryCarrier @CreateCarrier(int InValue) { FactoryCarrier Carrier; Carrier.Value = InValue; return Carrier; } int Run() { FactoryCarrier@ Carrier = CreateCarrier(42); return Carrier.Value; }"),
		CompileResult);
	UE_SET_LOG_VERBOSITY(Angelscript, Log);
	TestFalse(TEXT("Factory-style handle construction should remain unsupported on the current branch"), bCompiled);
	bPassed = !bCompiled;
	}

	return bPassed;
}

#endif
