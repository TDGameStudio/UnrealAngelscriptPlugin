#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"

#include "CQTest.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/OutputDevice.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptSnippetConsoleTests,
	"Angelscript.TestModule.Core.SnippetConsole",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	struct FSnippetConsoleOutputDevice : FOutputDevice
	{
		FString Text;

		void Serialize(const TCHAR* Data, ELogVerbosity::Type Verbosity, const FName& Category) override
		{
			Text += Data;
			Text += TEXT("\n");
		}
	};

	TEST_METHOD(ExecuteFileCommandIsRegistered)
	{
		IConsoleObject* ConsoleObject = IConsoleManager::Get().FindConsoleObject(TEXT("as.Snippet.ExecuteFile"));
		ASSERT_THAT(IsNotNull(ConsoleObject, TEXT("Snippet ExecuteFile command should be registered")));
		if (ConsoleObject != nullptr)
		{
			ASSERT_THAT(IsNotNull(ConsoleObject->AsCommand(), TEXT("Snippet ExecuteFile object should be a command")));
		}
	}

	TEST_METHOD(ExecuteFileRunsStatementSnippet)
	{
		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		IConsoleObject* ConsoleObject = IConsoleManager::Get().FindConsoleObject(TEXT("as.Snippet.ExecuteFile"));
		IConsoleCommand* Command = ConsoleObject != nullptr ? ConsoleObject->AsCommand() : nullptr;
		ASSERT_THAT(IsNotNull(Command, TEXT("Snippet ExecuteFile command should exist")));

		const FString SnippetPath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("SnippetConsole"), TEXT("ExecuteFileRunsStatementSnippet.as"));
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(SnippetPath), true);
		ON_SCOPE_EXIT
		{
			IFileManager::Get().Delete(*SnippetPath, false, true, true);
		};

		ASSERT_THAT(IsTrue(
			FFileHelper::SaveStringToFile(TEXT("int Value = 1 + 1;"), *SnippetPath),
			TEXT("Snippet ExecuteFile test should write source file")));

		FSnippetConsoleOutputDevice Output;
		const bool bExecuted = Command->Execute({ SnippetPath }, nullptr, Output);

		ASSERT_THAT(IsTrue(bExecuted, TEXT("Snippet ExecuteFile command delegate should execute")));
		ASSERT_THAT(IsTrue(Output.Text.Contains(TEXT("succeeded")), TEXT("Snippet ExecuteFile output should report success")));

		}
	}

	TEST_METHOD(ExecuteFileReportsReadFailure)
	{
		IConsoleObject* ConsoleObject = IConsoleManager::Get().FindConsoleObject(TEXT("as.Snippet.ExecuteFile"));
		IConsoleCommand* Command = ConsoleObject != nullptr ? ConsoleObject->AsCommand() : nullptr;
		ASSERT_THAT(IsNotNull(Command, TEXT("Snippet ExecuteFile command should exist for read failure")));

		const FString MissingPath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("SnippetConsole"), TEXT("Missing.as"));

		FSnippetConsoleOutputDevice Output;
		const bool bExecuted = Command->Execute({ MissingPath }, nullptr, Output);

		ASSERT_THAT(IsTrue(bExecuted, TEXT("Snippet ExecuteFile read-failure command delegate should execute")));
		ASSERT_THAT(IsTrue(Output.Text.Contains(TEXT("failed to read")), TEXT("Snippet ExecuteFile output should report read failure")));
	}
};

#endif
