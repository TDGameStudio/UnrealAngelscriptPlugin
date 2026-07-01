#include "CQTest.h"
#include "AngelscriptBindingsAssertions.h"
#include "AngelscriptTestUtilities.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"

// Test Layer: Runtime Integration
#if WITH_ANGELSCRIPT_UNITTESTS



bool CompileModuleWithResult(FAngelscriptEngine* Engine, ECompileType CompileType, FName ModuleName, FString Filename, FString Script, ECompileResult& OutCompileResult);
void ResetSharedCloneEngine(FAngelscriptEngine& Engine);


TEST_CLASS_WITH_FLAGS(
	FAngelscriptFunctionTests,
	"Angelscript.TestModule.Functional.Functions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
static const FAngelscriptCompileTraceDiagnosticSummary* FindDiagnosticContaining(
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

public:
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE_FULL();
	}

	AFTER_ALL() { FAngelscriptEngine& Engine = ASTEST_GET_ENGINE(); ASTEST_RESET_ENGINE(Engine); }

	TEST_METHOD(DefaultArguments)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASFunctionDefaultArguments"),
			TEXT("int Add(int A, int B = 5) { return A + B; } int Run() { return Add(7); }"));
		ASSERT_THAT(IsTrue(Module.IsValid()));

		ExpectGlobalInt(*TestRunner, Engine, Module.GetModule(), TEXT("int Run()"), TEXT("Default arguments should be applied when omitted"), 12);
	}

	TEST_METHOD(NamedArguments)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASFunctionNamedArguments"),
			TEXT("int Mix(int A, int B, int C) { return A + B * 10 + C * 100; } int Run() { return Mix(C: 3, A: 1, B: 2); }"));
		ASSERT_THAT(IsTrue(Module.IsValid()));

		ExpectGlobalInt(*TestRunner, Engine, Module.GetModule(), TEXT("int Run()"), TEXT("Named arguments should bind to the intended parameters"), 321);
	}

	TEST_METHOD(NamedArguments_MixedPartialOrder)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASFunctionNamedArgumentsMixedPartialOrder"),
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
)"));
		ASSERT_THAT(IsTrue(Module.IsValid()));

		ExpectGlobalInt(*TestRunner, Engine, Module.GetModule(), TEXT("int Run()"), TEXT("Mixed positional and named arguments should keep parameter-name binding for the reordered suffix"), 456789);
	}

	TEST_METHOD(NamedArguments_InvalidNameDiagnostics)
	{
FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		auto VerifyInvalidNamedArguments = [this, &Engine](
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

			ASSERT_THAT(IsFalse(bCompiled, *FString::Printf(TEXT("%s should fail to compile"), *TestCaseLabel)));
			ASSERT_THAT(IsFalse(Summary.bCompileSucceeded, *FString::Printf(TEXT("%s should report bCompileSucceeded=false"), *TestCaseLabel)));
			ASSERT_THAT(AreEqual(ECompileResult::Error, Summary.CompileResult, *FString::Printf(TEXT("%s should surface ECompileResult::Error"), *TestCaseLabel)));
			ASSERT_THAT(IsTrue(Summary.Diagnostics.Num() > 0, *FString::Printf(TEXT("%s should collect at least one diagnostic"), *TestCaseLabel)));
			ASSERT_THAT(IsNotNull(Diagnostic, *FString::Printf(TEXT("%s should emit a diagnostic containing '%s'"), *TestCaseLabel, *ExpectedMessage)));
			ASSERT_THAT(IsTrue(Diagnostic->Row > 0, *FString::Printf(TEXT("%s should report a non-zero diagnostic row"), *TestCaseLabel)));
			ASSERT_THAT(IsTrue(Diagnostic->Column > 0, *FString::Printf(TEXT("%s should report a non-zero diagnostic column"), *TestCaseLabel)));
			ASSERT_THAT(IsNull(RunFunction, *FString::Printf(TEXT("%s should not leave an executable Run() behind after compile failure"), *TestCaseLabel)));
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

	TEST_METHOD(DefaultArguments_OverrideAndNamedMix)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASFunctionDefaultArgumentsOverrideAndNamedMix"),
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
		ASSERT_THAT(IsTrue(Module.IsValid()));

		asIScriptFunction* DefaultFunction = GetFunctionByDecl(*TestRunner, Module.GetModule(), TEXT("int RunDefault()"));
		asIScriptFunction* OverrideFunction = GetFunctionByDecl(*TestRunner, Module.GetModule(), TEXT("int RunOverride()"));
		asIScriptFunction* NamedPartialFunction = GetFunctionByDecl(*TestRunner, Module.GetModule(), TEXT("int RunNamedPartial()"));
		ASSERT_THAT(IsNotNull(DefaultFunction));
		ASSERT_THAT(IsNotNull(OverrideFunction));
		ASSERT_THAT(IsNotNull(NamedPartialFunction));

		int32 DefaultResult = 0;
		int32 OverrideResult = 0;
		int32 NamedPartialResult = 0;
		ASSERT_THAT(IsTrue(ExecuteIntFunction(*TestRunner, Engine, *DefaultFunction, DefaultResult)));
		ASSERT_THAT(IsTrue(ExecuteIntFunction(*TestRunner, Engine, *OverrideFunction, OverrideResult)));
		ASSERT_THAT(IsTrue(ExecuteIntFunction(*TestRunner, Engine, *NamedPartialFunction, NamedPartialResult)));

		ASSERT_THAT(AreEqual(159, DefaultResult, TEXT("Default arguments should fill both omitted trailing parameters")));
		ASSERT_THAT(AreEqual(129, OverrideResult, TEXT("Explicit arguments should override the middle default while preserving the trailing default")));
		ASSERT_THAT(AreEqual(153, NamedPartialResult, TEXT("Named arguments should override only the addressed default parameter")));
	}

	TEST_METHOD(OverloadResolution)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASFunctionPointerAndOverload"),
			TEXT("int Convert(int Value) { return Value + 1; } int Convert(float Value) { return int(Value * 3.0f); } int Run() { return Convert(4) + Convert(2.0f); }"));
		ASSERT_THAT(IsTrue(Module.IsValid()));

		ExpectGlobalInt(*TestRunner, Engine, Module.GetModule(), TEXT("int Run()"), TEXT("Overload resolution should choose the expected function bodies"), 11);
	}

	TEST_METHOD(Pointer)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
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
		ASSERT_THAT(IsFalse(bCompiled, TEXT("Function pointer syntax should remain unsupported on the current branch")));
	}

	TEST_METHOD(Constructor)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASFunctionConstructor"),
			TEXT("class ConstructorCarrier { int Value; ConstructorCarrier() { Value = 42; } ConstructorCarrier(int InValue) { Value = InValue; } } int Run() { ConstructorCarrier DefaultCarrier; return DefaultCarrier.Value; }"));
		ASSERT_THAT(IsTrue(Module.IsValid()));
	}

	TEST_METHOD(Destructor)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		FScopedAngelscriptModule Module(*TestRunner, Engine, TEXT("ASFunctionDestructor"),
			TEXT("class DestructorCarrier { ~DestructorCarrier() {} } int Run() { DestructorCarrier Carrier; return 1; }"));
		ASSERT_THAT(IsTrue(Module.IsValid()));

		ExpectGlobalInt(*TestRunner, Engine, Module.GetModule(), TEXT("int Run()"), TEXT("Destructor declarations should compile and execute in local scope"), 1);
	}

	TEST_METHOD(Template)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
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
		ASSERT_THAT(IsFalse(bCompiled, TEXT("Template syntax should currently remain unsupported on this 2.33-based branch")));
	}

	TEST_METHOD(Factory)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);
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
		ASSERT_THAT(IsFalse(bCompiled, TEXT("Factory-style handle construction should remain unsupported on the current branch")));
	}
};

#endif
