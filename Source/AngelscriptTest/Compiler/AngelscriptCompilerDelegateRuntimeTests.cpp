#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"

#include "Containers/StringConv.h"
#include "CQTest.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CompilerDelegateRuntimeTest
{
	static const FName ModuleName(TEXT("Tests.Compiler.DelegateExecuteReportsUnboundRuntimeError"));
	static const FString ScriptFilename(TEXT("Tests/Compiler/DelegateExecuteReportsUnboundRuntimeError.as"));
	static const TCHAR* EntryFunctionDeclaration(TEXT("int Entry()"));
	static const TCHAR* ExpectedExceptionString(TEXT("Executing unbound delegate."));
	static const TCHAR* ExpectedExceptionFunctionDeclaration(TEXT("int FRuntimeValueDelegate::Execute() const"));
	static constexpr int32 ExpectedExceptionLine = 11;

	struct FExecutionExceptionResult
	{
		int32 PrepareResult = MIN_int32;
		int32 ExecuteResult = MIN_int32;
		FString ExceptionString;
		int32 ExceptionLine = 0;
		FString ExceptionFunctionDeclaration;
	};

	static asIScriptModule* GetCompiledModule(FAutomationTestBase& Test, FAngelscriptEngine& Engine)
	{
		const auto ModuleNameUtf8 = StringCast<ANSICHAR>(*ModuleName.ToString());
		asIScriptModule* Module = Engine.GetScriptEngine()->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS);
		FNoDiscardAsserter LocalAssert(Test);
		if (!LocalAssert.IsNotNull(Module, TEXT("Delegate execute runtime error compile should publish a script module")))
		{
			return nullptr;
		}
		return Module;
	}

	static bool ExecuteEntryAndCaptureException(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		asIScriptModule& Module,
		FExecutionExceptionResult& OutResult)
	{
		asIScriptFunction* EntryFunction = GetFunctionByDecl(Test, Module, EntryFunctionDeclaration);
		if (EntryFunction == nullptr)
		{
			return false;
		}

		FAngelscriptEngineScope EngineScope(Engine);
		asIScriptContext* Context = Engine.CreateContext();
		FNoDiscardAsserter LocalAssert(Test);
		if (!LocalAssert.IsNotNull(Context, TEXT("Delegate execute runtime error test case should create an execution context")))
		{
			return false;
		}

		ON_SCOPE_EXIT
		{
			Context->Release();
		};

		OutResult.PrepareResult = Context->Prepare(EntryFunction);
		OutResult.ExecuteResult = OutResult.PrepareResult == asSUCCESS ? Context->Execute() : OutResult.PrepareResult;
		OutResult.ExceptionString = Context->GetExceptionString() != nullptr ? UTF8_TO_TCHAR(Context->GetExceptionString()) : TEXT("");
		OutResult.ExceptionLine = Context->GetExceptionLineNumber();

		if (asIScriptFunction* ExceptionFunction = Context->GetExceptionFunction())
		{
			OutResult.ExceptionFunctionDeclaration = UTF8_TO_TCHAR(ExceptionFunction->GetDeclaration());
		}

		return true;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCompilerDelegateRuntimeTests,
	"Angelscript.TestModule.Compiler.EndToEnd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(DelegateExecuteReportsUnboundRuntimeError)
	{


		const FString ScriptSource = TEXT(R"AS(
	delegate int FRuntimeValueDelegate();

	int Entry()
	{
		FRuntimeValueDelegate Delegate;
		return Delegate.Execute();
	}
	)AS");

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*CompilerDelegateRuntimeTest::ModuleName.ToString());
		};

		FAngelscriptCompileTraceSummary Summary;
		const bool bCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::FullReload,
			CompilerDelegateRuntimeTest::ModuleName,
			CompilerDelegateRuntimeTest::ScriptFilename,
			ScriptSource,
			true,
			Summary);

		ASSERT_THAT(IsTrue(
			bCompiled,
			TEXT("Delegate execute runtime error test case should compile successfully")));
		ASSERT_THAT(IsTrue(
			Summary.bUsedPreprocessor,
			TEXT("Delegate execute runtime error test case should run through the preprocessor pipeline")));
		ASSERT_THAT(IsTrue(
			Summary.bCompileSucceeded,
			TEXT("Delegate execute runtime error test case should report compile success before execution")));
		ASSERT_THAT(AreEqual(
			ECompileResult::FullyHandled,
			Summary.CompileResult,
			TEXT("Delegate execute runtime error test case should finish with FullyHandled")));
		ASSERT_THAT(AreEqual(
			0,
			Summary.Diagnostics.Num(),
			TEXT("Delegate execute runtime error test case should keep compile diagnostics empty")));
		if (!bCompiled)
		{
			return;
		}

		asIScriptModule* Module = CompilerDelegateRuntimeTest::GetCompiledModule(*TestRunner, Engine);
		if (Module == nullptr)
		{
			return;
		}

		TestRunner->AddExpectedError(TEXT("Executing unbound delegate."), EAutomationExpectedErrorFlags::Contains, 1);
		TestRunner->AddExpectedError(*CompilerDelegateRuntimeTest::ModuleName.ToString(), EAutomationExpectedErrorFlags::Contains, 0);
		TestRunner->AddExpectedError(TEXT("int FRuntimeValueDelegate::Execute() const | Line 11"), EAutomationExpectedErrorFlags::Contains, 1, false);
		TestRunner->AddExpectedError(TEXT("int Entry() | Line 7"), EAutomationExpectedErrorFlags::Contains, 1, false);

		CompilerDelegateRuntimeTest::FExecutionExceptionResult ExecutionResult;
		const bool bExecuted = CompilerDelegateRuntimeTest::ExecuteEntryAndCaptureException(
			*TestRunner,
			Engine,
			*Module,
			ExecutionResult);
		ASSERT_THAT(IsTrue(
			bExecuted,
			TEXT("Delegate execute runtime error test case should reach the manual execution path")));
		if (!bExecuted)
		{
			return;
		}

		ASSERT_THAT(AreEqual(
			static_cast<int32>(asSUCCESS),
			ExecutionResult.PrepareResult,
			TEXT("Delegate execute runtime error test case should prepare the entry function successfully")));
		ASSERT_THAT(AreEqual(
			static_cast<int32>(asEXECUTION_EXCEPTION),
			ExecutionResult.ExecuteResult,
			TEXT("Delegate execute runtime error test case should fail during execution with a script exception")));
		ASSERT_THAT(AreEqual(
			FString(CompilerDelegateRuntimeTest::ExpectedExceptionString),
			ExecutionResult.ExceptionString,
			TEXT("Delegate execute runtime error test case should report the generated unbound delegate message")));
		ASSERT_THAT(AreEqual(
			CompilerDelegateRuntimeTest::ExpectedExceptionLine,
			ExecutionResult.ExceptionLine,
			TEXT("Delegate execute runtime error test case should report the generated delegate Execute wrapper line")));
		ASSERT_THAT(AreEqual(
			FString(CompilerDelegateRuntimeTest::ExpectedExceptionFunctionDeclaration),
			ExecutionResult.ExceptionFunctionDeclaration,
			TEXT("Delegate execute runtime error test case should attribute the exception to the generated delegate Execute wrapper")));

		}

	}

	TEST_METHOD(DelegateExecuteIfBoundReturnsDefaultValue)
	{


		const FString ScriptSource = TEXT(R"AS(
	delegate int FRuntimeValueDelegate();
	delegate bool FRuntimeBoolDelegate();

	int Entry()
	{
		FRuntimeValueDelegate Delegate;
		return Delegate.ExecuteIfBound();
	}

	int EntryBool()
	{
		FRuntimeBoolDelegate Delegate;
		return Delegate.ExecuteIfBound() ? 1 : 0;
	}
	)AS");

		const FName LocalModuleName(TEXT("Tests.Compiler.DelegateExecuteIfBoundReturnsDefaultValue"));
		const FString LocalScriptFilename(TEXT("Tests/Compiler/DelegateExecuteIfBoundReturnsDefaultValue.as"));

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*LocalModuleName.ToString());
		};

		FAngelscriptCompileTraceSummary Summary;
		const bool bCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::FullReload,
			LocalModuleName,
			LocalScriptFilename,
			ScriptSource,
			true,
			Summary);

		ASSERT_THAT(IsTrue(
			bCompiled,
			TEXT("Delegate ExecuteIfBound default-value test case should compile successfully")));
		ASSERT_THAT(IsTrue(
			Summary.bUsedPreprocessor,
			TEXT("Delegate ExecuteIfBound default-value test case should run through the preprocessor pipeline")));
		ASSERT_THAT(IsTrue(
			Summary.bCompileSucceeded,
			TEXT("Delegate ExecuteIfBound default-value test case should report compile success before execution")));
		ASSERT_THAT(AreEqual(
			ECompileResult::FullyHandled,
			Summary.CompileResult,
			TEXT("Delegate ExecuteIfBound default-value test case should finish with FullyHandled")));
		ASSERT_THAT(AreEqual(
			0,
			Summary.Diagnostics.Num(),
			TEXT("Delegate ExecuteIfBound default-value test case should keep compile diagnostics empty")));
		if (!bCompiled)
		{
			return;
		}

		const auto ModuleNameUtf8 = StringCast<ANSICHAR>(*LocalModuleName.ToString());
		asIScriptModule* Module = Engine.GetScriptEngine()->GetModule(ModuleNameUtf8.Get(), asGM_ONLY_IF_EXISTS);
		if (!this->Assert.IsNotNull(Module, TEXT("Delegate ExecuteIfBound default-value test case should publish a script module")))
		{
			return;
		}

		asIScriptFunction* EntryFunction = GetFunctionByDecl(*TestRunner, *Module, TEXT("int Entry()"));
		asIScriptFunction* EntryBoolFunction = GetFunctionByDecl(*TestRunner, *Module, TEXT("int EntryBool()"));
		if (EntryFunction == nullptr || EntryBoolFunction == nullptr)
		{
			return;
		}

		int32 IntResult = MIN_int32;
		int32 BoolResult = MIN_int32;
		ExecuteIntFunction(*TestRunner, Engine, *EntryFunction, IntResult);
		ExecuteIntFunction(*TestRunner, Engine, *EntryBoolFunction, BoolResult);

		ASSERT_THAT(AreEqual(
			0,
			IntResult,
			TEXT("Delegate ExecuteIfBound default-value test case should return 0 for unbound int delegates")));
		ASSERT_THAT(AreEqual(
			0,
			BoolResult,
			TEXT("Delegate ExecuteIfBound default-value test case should return 0 for unbound bool delegates")));

		}

	}

};

#endif
