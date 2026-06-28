#include "Dump/AngelscriptCSVWriter.h"
#include "Dump/AngelscriptDumpCommand.h"
#include "Dump/AngelscriptStateDump.h"

#include "AngelscriptTestUtilities.h"
#include "CQTest.h"

#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

struct FAngelscriptDumpTestHelpers
{
	static FString MakeUniqueDumpTestPath(const FString& Prefix)
	{
		return FPaths::Combine(
			FPaths::ProjectSavedDir(),
			TEXT("Automation"),
			TEXT("StateDump"),
			FString::Printf(TEXT("%s_%s"), *Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	}

	static TArray<FString> GetExpectedPhaseOneCsvFiles()
	{
		return {
			TEXT("EngineOverview.csv"),
			TEXT("RuntimeConfig.csv"),
			TEXT("Modules.csv"),
			TEXT("Classes.csv"),
			TEXT("Properties.csv"),
			TEXT("Functions.csv"),
			TEXT("Enums.csv"),
			TEXT("Delegates.csv"),
			TEXT("RegisteredTypes.csv"),
			TEXT("Diagnostics.csv"),
			TEXT("ScriptEngineState.csv"),
			TEXT("BindRegistrations.csv"),
			TEXT("BindDatabase_Structs.csv"),
			TEXT("BindDatabase_Classes.csv"),
			TEXT("ToStringTypes.csv"),
			TEXT("DocumentationStats.csv"),
			TEXT("EngineSettings.csv"),
			TEXT("HotReloadState.csv"),
			TEXT("JITDatabase.csv"),
			TEXT("PrecompiledData.csv"),
			TEXT("StaticJITState.csv"),
			TEXT("DebugServerState.csv"),
			TEXT("DebugBreakpoints.csv"),
			TEXT("CodeCoverage.csv"),
			TEXT("EngineStateSnapshot.csv"),
			TEXT("EngineMemberState.csv"),
			TEXT("EngineCollections.csv"),
			TEXT("AsEngineInternalState.csv"),
			TEXT("AsModuleInternalState.csv"),
			TEXT("AsTypeInternalState.csv"),
			TEXT("AsFunctionInternalState.csv"),
			TEXT("EditorReloadState.csv"),
			TEXT("EditorMenuExtensions.csv"),
			TEXT("DumpSummary.csv")
		};
	}

	static FString GetExpectedSummaryStatus(const FString& TableName)
	{
		if (TableName == TEXT("ToStringTypes.csv"))
		{
			return TEXT("NotAvailable");
		}

		if (TableName == TEXT("HotReloadState.csv"))
		{
			return TEXT("PartialExport");
		}

		if (TableName == TEXT("CodeCoverage.csv"))
		{
			return TEXT("Skipped");
		}

		// UE 5.7: headless shared test engine has no DebugServer attached, so
		// DebugServerState/DebugBreakpoints legitimately report "Skipped". An
		// empty string sentinel here signals the caller to accept either
		// "Success" or "Skipped" (see ParseDumpSummary consumers below).
		if (TableName == TEXT("DebugServerState.csv")
			|| TableName == TEXT("DebugBreakpoints.csv"))
		{
			return FString();
		}

		return TEXT("Success");
	}

	static bool LoadFileContents(FAutomationTestBase& Test, const FString& Filename, FString& OutContents)
	{
		if (!FFileHelper::LoadFileToString(OutContents, *Filename))
		{
			Test.AddError(FString::Printf(TEXT("Failed to load '%s'"), *Filename));
			return false;
		}

		return true;
	}

	static bool RunDumpAll(FAutomationTestBase& Test, FString& OutOutputDir)
	{
		FResolvedProductionLikeEngine ResolvedEngine;
		if (!AcquireProductionLikeEngine(Test, TEXT("Expected a production-like engine for dump tests"), ResolvedEngine))
		{
			return false;
		}

		OutOutputDir = MakeUniqueDumpTestPath(TEXT("DumpAll"));
		OutOutputDir = FAngelscriptStateDump::DumpAll(ResolvedEngine.Get(), OutOutputDir);
		if (!Test.TestFalse(TEXT("DumpAll should return a non-empty output directory"), OutOutputDir.IsEmpty()))
		{
			return false;
		}

		return Test.TestTrue(TEXT("DumpAll should create the output directory"), IFileManager::Get().DirectoryExists(*OutOutputDir));
	}

	static TMap<FString, TPair<int32, FString>> ParseDumpSummary(const FString& SummaryContents)
	{
		TArray<FString> Lines;
		SummaryContents.ParseIntoArrayLines(Lines, true);

		TMap<FString, TPair<int32, FString>> SummaryRows;
		for (int32 LineIndex = 1; LineIndex < Lines.Num(); ++LineIndex)
		{
			TArray<FString> Columns;
			Lines[LineIndex].ParseIntoArray(Columns, TEXT(","), false);
			if (Columns.Num() < 4)
			{
				continue;
			}

			SummaryRows.Add(Columns[0], TPair<int32, FString>(FCString::Atoi(*Columns[1]), Columns[2]));
		}

		return SummaryRows;
	}
};

TEST_CLASS_WITH_FLAGS(FAngelscriptCSVWriterTest,
	"Angelscript.TestModule.Dump.CSVWriter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(Basic)
	{
		const FString OutputFilename = FPaths::Combine(FAngelscriptDumpTestHelpers::MakeUniqueDumpTestPath(TEXT("CSVWriterBasic")), TEXT("Basic.csv"));

		FCSVWriter Writer;
		Writer.AddHeader({ TEXT("Name"), TEXT("Value") });
		Writer.AddRow({ TEXT("Alpha"), TEXT("42") });

		FString ErrorMessage;
		const bool bSaved = Writer.SaveToFile(OutputFilename, &ErrorMessage);
		if (!bSaved)
		{
			TestRunner->AddError(ErrorMessage);
		}
		ASSERT_THAT(IsTrue(bSaved, TEXT("FCSVWriter basic save should succeed")));

		FString FileContents;
		ASSERT_THAT(IsTrue(FAngelscriptDumpTestHelpers::LoadFileContents(*TestRunner, OutputFilename, FileContents), TEXT("CSV output should be readable")));

		ASSERT_THAT(IsTrue(FileContents.Contains(TEXT("Name,Value")), TEXT("CSV output should contain the header line")));
		ASSERT_THAT(IsTrue(FileContents.Contains(TEXT("Alpha,42")), TEXT("CSV output should contain the written row")));
	}

	TEST_METHOD(SpecialCharacters)
	{
		const FString OutputFilename = FPaths::Combine(FAngelscriptDumpTestHelpers::MakeUniqueDumpTestPath(TEXT("CSVWriterEscape")), TEXT("Escaped.csv"));

		FCSVWriter Writer;
		Writer.AddHeader({ TEXT("One"), TEXT("Two"), TEXT("Three") });
		Writer.AddRow({ TEXT("Comma,Value"), TEXT("Quote \"Here\""), TEXT("Line1\nLine2") });

		FString ErrorMessage;
		const bool bSaved = Writer.SaveToFile(OutputFilename, &ErrorMessage);
		if (!bSaved)
		{
			TestRunner->AddError(ErrorMessage);
		}
		ASSERT_THAT(IsTrue(bSaved, TEXT("FCSVWriter escape save should succeed")));

		FString FileContents;
		ASSERT_THAT(IsTrue(FAngelscriptDumpTestHelpers::LoadFileContents(*TestRunner, OutputFilename, FileContents), TEXT("Escaped CSV output should be readable")));

		ASSERT_THAT(IsTrue(FileContents.Contains(TEXT("\"Comma,Value\"")), TEXT("CSV should quote comma-containing fields")));
		ASSERT_THAT(IsTrue(FileContents.Contains(TEXT("\"Quote \"\"Here\"\"\"")), TEXT("CSV should double embedded quotes")));
		ASSERT_THAT(IsTrue(FileContents.Contains(TEXT("\"Line1\nLine2\"")), TEXT("CSV should preserve multiline fields inside quotes")));
	}
};

TEST_CLASS_WITH_FLAGS(FAngelscriptStateDumpTest,
	"Angelscript.TestModule.Dump.DumpAll",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ConsoleCommandIsRegistered)
	{
		IConsoleObject* ConsoleObject = IConsoleManager::Get().FindConsoleObject(FAngelscriptDumpCommand::GetCommandName());
		ASSERT_THAT(IsNotNull(ConsoleObject, TEXT("DumpEngineState console command should be registered")));
		if (ConsoleObject != nullptr)
		{
			ASSERT_THAT(IsNotNull(ConsoleObject->AsCommand(), TEXT("DumpEngineState console object should be a command")));
		}
	}

	TEST_METHOD(EndToEnd)
	{
		FString OutputDir;
		ASSERT_THAT(IsTrue(FAngelscriptDumpTestHelpers::RunDumpAll(*TestRunner, OutputDir), TEXT("DumpAll should complete")));

		for (const FString& ExpectedFilename : FAngelscriptDumpTestHelpers::GetExpectedPhaseOneCsvFiles())
		{
			const FString CsvPath = FPaths::Combine(OutputDir, ExpectedFilename);
			ASSERT_THAT(IsTrue(IFileManager::Get().FileExists(*CsvPath), *FString::Printf(TEXT("DumpAll should create '%s'"), *ExpectedFilename)));
		}
	}

	TEST_METHOD(Summary)
	{
		FString OutputDir;
		ASSERT_THAT(IsTrue(FAngelscriptDumpTestHelpers::RunDumpAll(*TestRunner, OutputDir), TEXT("DumpAll should complete for summary validation")));

		const FString SummaryPath = FPaths::Combine(OutputDir, TEXT("DumpSummary.csv"));
		FString SummaryContents;
		ASSERT_THAT(IsTrue(FAngelscriptDumpTestHelpers::LoadFileContents(*TestRunner, SummaryPath, SummaryContents), TEXT("DumpSummary should be readable")));

		const TMap<FString, TPair<int32, FString>> SummaryRows = FAngelscriptDumpTestHelpers::ParseDumpSummary(SummaryContents);
		for (const FString& ExpectedFilename : FAngelscriptDumpTestHelpers::GetExpectedPhaseOneCsvFiles())
		{
			const TPair<int32, FString>* SummaryRow = SummaryRows.Find(ExpectedFilename);
			ASSERT_THAT(IsNotNull(SummaryRow, *FString::Printf(TEXT("DumpSummary should contain a row for '%s'"), *ExpectedFilename)));

			const FString ExpectedStatus = FAngelscriptDumpTestHelpers::GetExpectedSummaryStatus(ExpectedFilename);
			if (ExpectedStatus.IsEmpty())
			{
				const bool bAcceptable = SummaryRow->Value == TEXT("Success") || SummaryRow->Value == TEXT("Skipped");
				ASSERT_THAT(IsTrue(
					bAcceptable,
					*FString::Printf(TEXT("'%s' should report either Success or Skipped (actual: '%s')"), *ExpectedFilename, *SummaryRow->Value)));
			}
			else
			{
				ASSERT_THAT(AreEqual(
					ExpectedStatus,
					SummaryRow->Value,
					*FString::Printf(TEXT("'%s' should report the expected summary status"), *ExpectedFilename)));
			}
			ASSERT_THAT(IsTrue(SummaryRow->Key >= 0, *FString::Printf(TEXT("'%s' should report a non-negative row count"), *ExpectedFilename)));
		}
	}
};

#endif
