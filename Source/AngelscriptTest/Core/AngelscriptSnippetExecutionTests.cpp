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

		TestRunner->TestTrue(TEXT("Statement snippet should execute successfully"), Result.bSucceeded);
		TestRunner->TestEqual(TEXT("Statement snippet should report success"), Result.ResultCode, EAngelscriptSnippetResultCode::Succeeded);
		TestRunner->TestTrue(TEXT("Statement snippet should expose a void entry point"), Result.EntryPointDeclaration.StartsWith(TEXT("void ")));
		TestRunner->TestTrue(TEXT("Statement snippet should use Immediate memory virtual path"), Result.VirtualPath.StartsWith(TEXT("/Angelscript/Memory/Immediate/Snippet_")));
		TestRunner->TestTrue(TEXT("Statement snippet should use isolated Immediate module name"), Result.ModuleName.StartsWith(TEXT("Angelscript.Memory.Immediate.Snippet_")));
		TestRunner->TestFalse(TEXT("Statement snippet should discard module by default"), Engine.GetModule(Result.ModuleName).IsValid());

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

		TestRunner->TestTrue(TEXT("First repeated-label statement snippet should execute successfully"), FirstResult.bSucceeded);
		TestRunner->TestTrue(TEXT("Second repeated-label statement snippet should execute successfully"), SecondResult.bSucceeded);
		TestRunner->TestEqual(TEXT("Second repeated-label statement snippet should report success"), SecondResult.ResultCode, EAngelscriptSnippetResultCode::Succeeded);
		TestRunner->TestNotEqual(TEXT("Repeated-label statement snippets should use distinct module names"), FirstResult.ModuleName, SecondResult.ModuleName);
		TestRunner->TestFalse(TEXT("First repeated-label statement snippet should discard module"), Engine.GetModule(FirstResult.ModuleName).IsValid());
		TestRunner->TestFalse(TEXT("Second repeated-label statement snippet should discard module"), Engine.GetModule(SecondResult.ModuleName).IsValid());

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

		TestRunner->TestTrue(TEXT("First kept repeated-label statement snippet should execute successfully"), FirstResult.bSucceeded);
		TestRunner->TestTrue(TEXT("Second kept repeated-label statement snippet should execute successfully"), SecondResult.bSucceeded);
		TestRunner->TestNotEqual(TEXT("Kept repeated-label statement snippets should use distinct module names"), FirstResult.ModuleName, SecondResult.ModuleName);
		TestRunner->TestNotEqual(TEXT("Kept repeated-label statement snippets should use distinct entry points"), FirstResult.EntryPointDeclaration, SecondResult.EntryPointDeclaration);
		TestRunner->TestTrue(TEXT("First kept repeated-label statement snippet should keep module"), Engine.GetModule(FirstResult.ModuleName).IsValid());
		TestRunner->TestTrue(TEXT("Second kept repeated-label statement snippet should keep module"), Engine.GetModule(SecondResult.ModuleName).IsValid());

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

		TestRunner->TestTrue(TEXT("Full-source snippet should execute explicit Main"), Result.bSucceeded);
		TestRunner->TestEqual(TEXT("Full-source snippet should report success"), Result.ResultCode, EAngelscriptSnippetResultCode::Succeeded);
		TestRunner->TestEqual(TEXT("Full-source snippet diagnostics should be empty on success"), Result.Diagnostics.Num(), 0);

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

		TestRunner->TestTrue(TEXT("First repeated full-source snippet should execute successfully"), FirstResult.bSucceeded);
		TestRunner->TestTrue(TEXT("Second repeated full-source snippet should execute successfully"), SecondResult.bSucceeded);
		TestRunner->TestEqual(TEXT("Repeated full-source snippets should keep explicit Main entry"), SecondResult.EntryPointDeclaration, FString(TEXT("void Main()")));
		TestRunner->TestNotEqual(TEXT("Repeated full-source snippets should use distinct module names"), FirstResult.ModuleName, SecondResult.ModuleName);
		TestRunner->TestFalse(TEXT("First repeated full-source snippet should discard module"), Engine.GetModule(FirstResult.ModuleName).IsValid());
		TestRunner->TestFalse(TEXT("Second repeated full-source snippet should discard module"), Engine.GetModule(SecondResult.ModuleName).IsValid());

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

		TestRunner->TestFalse(TEXT("Broken snippet should fail"), Result.bSucceeded);
		TestRunner->TestEqual(TEXT("Broken snippet should fail during compile"), Result.ResultCode, EAngelscriptSnippetResultCode::CompileFailed);
		TestRunner->TestTrue(TEXT("Broken snippet should report at least one diagnostic"), Result.Diagnostics.Num() > 0);
		if (Result.Diagnostics.Num() > 0)
		{
			TestRunner->TestEqual(TEXT("Broken snippet diagnostic should use virtual path section"), Result.Diagnostics[0].Section, Result.VirtualPath);
			TestRunner->TestTrue(TEXT("Broken snippet diagnostic should map to user row"), Result.Diagnostics[0].UserRow >= 1);
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

		TestRunner->TestFalse(TEXT("Throwing snippet should fail"), Result.bSucceeded);
		TestRunner->TestEqual(TEXT("Throwing snippet should report execution exception"), Result.ResultCode, EAngelscriptSnippetResultCode::ExecutionException);
		TestRunner->TestTrue(TEXT("Throwing statement snippet should report generated entry point"), Result.EntryPointDeclaration.StartsWith(TEXT("void __SnippetMain_")));
		TestRunner->TestTrue(TEXT("Throwing snippet should preserve exception text"), Result.ExceptionMessage.Contains(TEXT("SnippetFailure")));
		TestRunner->TestEqual(TEXT("Throwing snippet should report virtual section"), Result.ExceptionSection, Result.VirtualPath);
		TestRunner->TestTrue(TEXT("Throwing snippet should report user-visible line"), Result.ExceptionLine >= 1);

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

		TestRunner->TestTrue(TEXT("Kept snippet should execute successfully"), Result.bSucceeded);
		TestRunner->TestTrue(TEXT("Kept snippet should leave module descriptor available"), Engine.GetModule(Result.ModuleName).IsValid());

		}
	}

	TEST_METHOD(RejectsEmptySource)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		FAngelscriptSnippetRequest Request;
		Request.SourceText = TEXT("  \r\n\t  ");

		const FAngelscriptSnippetResult Result = FAngelscriptSnippetRunner::Execute(Engine, Request);

		TestRunner->TestFalse(TEXT("Empty snippet should fail"), Result.bSucceeded);
		TestRunner->TestEqual(TEXT("Empty snippet should be invalid request"), Result.ResultCode, EAngelscriptSnippetResultCode::InvalidRequest);
		TestRunner->TestTrue(TEXT("Empty snippet should explain the failure"), !Result.ErrorMessage.IsEmpty());

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

		TestRunner->TestFalse(TEXT("Full-source snippet without Main should fail"), Result.bSucceeded);
		TestRunner->TestEqual(TEXT("Full-source snippet without Main should report missing entry"), Result.ResultCode, EAngelscriptSnippetResultCode::EntryPointMissing);
		TestRunner->TestTrue(TEXT("Full-source snippet without Main should discard module"), !Result.ModuleName.IsEmpty() && !Engine.GetModule(Result.ModuleName).IsValid());

		}
	}
};

#endif
