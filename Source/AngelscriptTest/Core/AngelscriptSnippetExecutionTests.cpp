#include "AngelscriptSnippet.h"
#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"

#if WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptSnippetExecutionTests,
	"Angelscript.TestModule.Core.SnippetExecution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(StatementModeWrapsAndExecutes)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		FAngelscriptSnippetRequest Request;
		Request.SourceText = TEXT("int Value = 40 + 2;");
		Request.Label = TEXT("StatementWrap");

		const FAngelscriptSnippetResult Result = FAngelscriptSnippetRunner::Execute(Engine, Request);

		ASSERT_THAT(IsTrue(Result.bSucceeded, TEXT("Statement snippet should execute successfully")));
		ASSERT_THAT(AreEqual(EAngelscriptSnippetResultCode::Succeeded, Result.ResultCode, TEXT("Statement snippet should report success")));
		ASSERT_THAT(IsTrue(Result.EntryPointDeclaration.StartsWith(TEXT("void ")), TEXT("Statement snippet should expose a void entry point")));
		ASSERT_THAT(IsTrue(Result.VirtualPath.StartsWith(TEXT("/Angelscript/Memory/Immediate/Snippet_")), TEXT("Statement snippet should use Immediate memory virtual path")));
		ASSERT_THAT(IsTrue(Result.ModuleName.StartsWith(TEXT("Angelscript.Memory.Immediate.Snippet_")), TEXT("Statement snippet should use isolated Immediate module name")));
		ASSERT_THAT(IsFalse(Engine.GetModule(Result.ModuleName).IsValid(), TEXT("Statement snippet should discard module by default")));

		}
	}

	TEST_METHOD(StatementModeCanRunRepeatedlyWithSameLabel)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		FAngelscriptSnippetRequest Request;
		Request.SourceText = TEXT("Log(\"Repeated snippet run\");");
		Request.Label = TEXT("Editor");

		const FAngelscriptSnippetResult FirstResult = FAngelscriptSnippetRunner::Execute(Engine, Request);
		const FAngelscriptSnippetResult SecondResult = FAngelscriptSnippetRunner::Execute(Engine, Request);

		ASSERT_THAT(IsTrue(FirstResult.bSucceeded, TEXT("First repeated-label statement snippet should execute successfully")));
		ASSERT_THAT(IsTrue(SecondResult.bSucceeded, TEXT("Second repeated-label statement snippet should execute successfully")));
		ASSERT_THAT(AreEqual(EAngelscriptSnippetResultCode::Succeeded, SecondResult.ResultCode, TEXT("Second repeated-label statement snippet should report success")));
		ASSERT_THAT(AreNotEqual(SecondResult.ModuleName, FirstResult.ModuleName, TEXT("Repeated-label statement snippets should use distinct module names")));
		ASSERT_THAT(IsFalse(Engine.GetModule(FirstResult.ModuleName).IsValid(), TEXT("First repeated-label statement snippet should discard module")));
		ASSERT_THAT(IsFalse(Engine.GetModule(SecondResult.ModuleName).IsValid(), TEXT("Second repeated-label statement snippet should discard module")));

		}
	}

	TEST_METHOD(StatementModeCanRunRepeatedlyWhenKeepingModules)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		FAngelscriptSnippetRequest Request;
		Request.SourceText = TEXT("Log(\"Repeated kept snippet run\");");
		Request.Label = TEXT("Editor");
		Request.bKeepModuleForDebugging = true;

		const FAngelscriptSnippetResult FirstResult = FAngelscriptSnippetRunner::Execute(Engine, Request);
		const FAngelscriptSnippetResult SecondResult = FAngelscriptSnippetRunner::Execute(Engine, Request);
		ON_SCOPE_EXIT
		{
			if (!FirstResult.ModuleName.IsEmpty())
			{
				Engine.DiscardModule(*FirstResult.ModuleName);
			}
			if (!SecondResult.ModuleName.IsEmpty())
			{
				Engine.DiscardModule(*SecondResult.ModuleName);
			}
		};

		ASSERT_THAT(IsTrue(FirstResult.bSucceeded, TEXT("First kept repeated-label statement snippet should execute successfully")));
		ASSERT_THAT(IsTrue(SecondResult.bSucceeded, TEXT("Second kept repeated-label statement snippet should execute successfully")));
		ASSERT_THAT(AreNotEqual(SecondResult.ModuleName, FirstResult.ModuleName, TEXT("Kept repeated-label statement snippets should use distinct module names")));
		ASSERT_THAT(AreNotEqual(SecondResult.EntryPointDeclaration, FirstResult.EntryPointDeclaration, TEXT("Kept repeated-label statement snippets should use distinct entry points")));
		ASSERT_THAT(IsTrue(Engine.GetModule(FirstResult.ModuleName).IsValid(), TEXT("First kept repeated-label statement snippet should keep module")));
		ASSERT_THAT(IsTrue(Engine.GetModule(SecondResult.ModuleName).IsValid(), TEXT("Second kept repeated-label statement snippet should keep module")));

		}
	}

	TEST_METHOD(FullSourceModeExecutesExplicitMain)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		FAngelscriptSnippetRequest Request;
		Request.SourceMode = EAngelscriptSnippetSourceMode::FullSource;
		Request.SourceText = TEXT(R"(
void Helper()
{
}

void Main()
{
	Helper();
}
)");

		const FAngelscriptSnippetResult Result = FAngelscriptSnippetRunner::Execute(Engine, Request);

		ASSERT_THAT(IsTrue(Result.bSucceeded, TEXT("Full-source snippet should execute explicit Main")));
		ASSERT_THAT(AreEqual(EAngelscriptSnippetResultCode::Succeeded, Result.ResultCode, TEXT("Full-source snippet should report success")));
		ASSERT_THAT(AreEqual(0, Result.Diagnostics.Num(), TEXT("Full-source snippet diagnostics should be empty on success")));

		}
	}

	TEST_METHOD(FullSourceModeCanRunRepeatedlyWithExplicitMain)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		FAngelscriptSnippetRequest Request;
		Request.SourceMode = EAngelscriptSnippetSourceMode::FullSource;
		Request.Label = TEXT("FullSourceRepeated");
		Request.SourceText = TEXT(R"(
void Main()
{
	Log("Repeated full-source snippet run");
}
)");

		const FAngelscriptSnippetResult FirstResult = FAngelscriptSnippetRunner::Execute(Engine, Request);
		const FAngelscriptSnippetResult SecondResult = FAngelscriptSnippetRunner::Execute(Engine, Request);

		ASSERT_THAT(IsTrue(FirstResult.bSucceeded, TEXT("First repeated full-source snippet should execute successfully")));
		ASSERT_THAT(IsTrue(SecondResult.bSucceeded, TEXT("Second repeated full-source snippet should execute successfully")));
		ASSERT_THAT(AreEqual(FString(TEXT("void Main()")), SecondResult.EntryPointDeclaration, TEXT("Repeated full-source snippets should keep explicit Main entry")));
		ASSERT_THAT(AreNotEqual(SecondResult.ModuleName, FirstResult.ModuleName, TEXT("Repeated full-source snippets should use distinct module names")));
		ASSERT_THAT(IsFalse(Engine.GetModule(FirstResult.ModuleName).IsValid(), TEXT("First repeated full-source snippet should discard module")));
		ASSERT_THAT(IsFalse(Engine.GetModule(SecondResult.ModuleName).IsValid(), TEXT("Second repeated full-source snippet should discard module")));

		}
	}

	TEST_METHOD(ReportsCompileDiagnosticsWithVirtualPath)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		FAngelscriptSnippetRequest Request;
		Request.SourceText = TEXT("int Broken = ;");
		Request.Label = TEXT("BrokenCompile");

		TestRunner->AddExpectedError(TEXT("/Angelscript/Memory/Immediate/Snippet_"), EAutomationExpectedErrorFlags::Contains, 0);
		TestRunner->AddExpectedError(TEXT("Expected expression value"), EAutomationExpectedErrorFlags::Contains, 0);
		TestRunner->AddExpectedError(TEXT("Instead found ';'"), EAutomationExpectedErrorFlags::Contains, 0);
		TestRunner->AddExpectedError(TEXT("Hot reload failed due to script compile errors"), EAutomationExpectedErrorFlags::Contains, 0);
		const FAngelscriptSnippetResult Result = FAngelscriptSnippetRunner::Execute(Engine, Request);

		ASSERT_THAT(IsFalse(Result.bSucceeded, TEXT("Broken snippet should fail")));
		ASSERT_THAT(AreEqual(EAngelscriptSnippetResultCode::CompileFailed, Result.ResultCode, TEXT("Broken snippet should fail during compile")));
		ASSERT_THAT(IsTrue(Result.Diagnostics.Num() > 0, TEXT("Broken snippet should report at least one diagnostic")));
		if (Result.Diagnostics.Num() > 0)
		{
			ASSERT_THAT(AreEqual(Result.VirtualPath, Result.Diagnostics[0].Section, TEXT("Broken snippet diagnostic should use virtual path section")));
			ASSERT_THAT(IsTrue(Result.Diagnostics[0].UserRow >= 1, TEXT("Broken snippet diagnostic should map to user row")));
		}

		}
	}

	TEST_METHOD(ReportsExecutionException)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		FAngelscriptSnippetRequest Request;
		Request.SourceText = TEXT("throw(\"SnippetFailure\");");
		Request.Label = TEXT("Exception");

		TestRunner->AddExpectedError(TEXT("SnippetFailure"), EAutomationExpectedErrorFlags::Contains, 0);
		TestRunner->AddExpectedError(TEXT("Angelscript.Memory.Immediate.Snippet_"), EAutomationExpectedErrorFlags::Contains, 0);
		TestRunner->AddExpectedError(TEXT("void __SnippetMain_"), EAutomationExpectedErrorFlags::Contains, 0);
		const FAngelscriptSnippetResult Result = FAngelscriptSnippetRunner::Execute(Engine, Request);

		ASSERT_THAT(IsFalse(Result.bSucceeded, TEXT("Throwing snippet should fail")));
		ASSERT_THAT(AreEqual(EAngelscriptSnippetResultCode::ExecutionException, Result.ResultCode, TEXT("Throwing snippet should report execution exception")));
		ASSERT_THAT(IsTrue(Result.EntryPointDeclaration.StartsWith(TEXT("void __SnippetMain_")), TEXT("Throwing statement snippet should report generated entry point")));
		ASSERT_THAT(IsTrue(Result.ExceptionMessage.Contains(TEXT("SnippetFailure")), TEXT("Throwing snippet should preserve exception text")));
		ASSERT_THAT(AreEqual(Result.VirtualPath, Result.ExceptionSection, TEXT("Throwing snippet should report virtual section")));
		ASSERT_THAT(IsTrue(Result.ExceptionLine >= 1, TEXT("Throwing snippet should report user-visible line")));

		}
	}

	TEST_METHOD(CanKeepModuleForDebugging)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		FAngelscriptSnippetRequest Request;
		Request.SourceText = TEXT("int Value = 1;");
		Request.Label = TEXT("KeepModule");
		Request.bKeepModuleForDebugging = true;

		const FAngelscriptSnippetResult Result = FAngelscriptSnippetRunner::Execute(Engine, Request);
		ON_SCOPE_EXIT
		{
			if (!Result.ModuleName.IsEmpty())
			{
				Engine.DiscardModule(*Result.ModuleName);
			}
		};

		ASSERT_THAT(IsTrue(Result.bSucceeded, TEXT("Kept snippet should execute successfully")));
		ASSERT_THAT(IsTrue(Engine.GetModule(Result.ModuleName).IsValid(), TEXT("Kept snippet should leave module descriptor available")));

		}
	}

	TEST_METHOD(RejectsEmptySource)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		FAngelscriptSnippetRequest Request;
		Request.SourceText = TEXT("  \r\n\t  ");

		const FAngelscriptSnippetResult Result = FAngelscriptSnippetRunner::Execute(Engine, Request);

		ASSERT_THAT(IsFalse(Result.bSucceeded, TEXT("Empty snippet should fail")));
		ASSERT_THAT(AreEqual(EAngelscriptSnippetResultCode::InvalidRequest, Result.ResultCode, TEXT("Empty snippet should be invalid request")));
		ASSERT_THAT(IsTrue(!Result.ErrorMessage.IsEmpty(), TEXT("Empty snippet should explain the failure")));

		}
	}

	TEST_METHOD(RejectsMissingMainInFullSource)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		FAngelscriptSnippetRequest Request;
		Request.SourceMode = EAngelscriptSnippetSourceMode::FullSource;
		Request.SourceText = TEXT("void NotMain() {}");

		const FAngelscriptSnippetResult Result = FAngelscriptSnippetRunner::Execute(Engine, Request);

		ASSERT_THAT(IsFalse(Result.bSucceeded, TEXT("Full-source snippet without Main should fail")));
		ASSERT_THAT(AreEqual(EAngelscriptSnippetResultCode::EntryPointMissing, Result.ResultCode, TEXT("Full-source snippet without Main should report missing entry")));
		ASSERT_THAT(IsTrue(!Result.ModuleName.IsEmpty() && !Engine.GetModule(Result.ModuleName).IsValid(), TEXT("Full-source snippet without Main should discard module")));

		}
	}
};

#endif
