#include "Dump/AngelscriptCSVWriter.h"
#include "Dump/AngelscriptStateDump.h"

#include "AngelscriptTestUtilities.h"

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptCSVWriterBasicTest,
	"Angelscript.TestModule.Dump.CSVWriter.Basic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptCSVWriterEscapingTest,
	"Angelscript.TestModule.Dump.CSVWriter.SpecialCharacters",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptStateDumpEndToEndTest,
	"Angelscript.TestModule.Dump.DumpAll.EndToEnd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptStateDumpSummaryTest,
	"Angelscript.TestModule.Dump.DumpAll.Summary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace AngelscriptTest_Dump_AngelscriptDumpTests_Private
{
	FString MakeUniqueDumpTestPath(const FString& Prefix)
	{
		return FPaths::Combine(
			FPaths::ProjectSavedDir(),
			TEXT("Automation"),
			TEXT("StateDump"),
			FString::Printf(TEXT("%s_%s"), *Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	}

	TArray<FString> GetExpectedPhaseOneCsvFiles()
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
			TEXT("EditorReloadState.csv"),
			TEXT("EditorMenuExtensions.csv"),
			TEXT("DumpSummary.csv")
		};
	}

	FString GetExpectedSummaryStatus(const FString& TableName)
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

	bool LoadFileContents(FAutomationTestBase& Test, const FString& Filename, FString& OutContents)
	{
		if (!FFileHelper::LoadFileToString(OutContents, *Filename))
		{
			Test.AddError(FString::Printf(TEXT("Failed to load '%s'"), *Filename));
			return false;
		}

		return true;
	}

	bool RunDumpAll(FAutomationTestBase& Test, FString& OutOutputDir)
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

	TMap<FString, TPair<int32, FString>> ParseDumpSummary(const FString& SummaryContents)
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
}


bool FAngelscriptCSVWriterBasicTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Dump_AngelscriptDumpTests_Private;
	const FString OutputFilename = FPaths::Combine(MakeUniqueDumpTestPath(TEXT("CSVWriterBasic")), TEXT("Basic.csv"));

	FCSVWriter Writer;
	Writer.AddHeader({ TEXT("Name"), TEXT("Value") });
	Writer.AddRow({ TEXT("Alpha"), TEXT("42") });

	FString ErrorMessage;
	if (!TestTrue(TEXT("FCSVWriter basic save should succeed"), Writer.SaveToFile(OutputFilename, &ErrorMessage)))
	{
		AddError(ErrorMessage);
		return false;
	}

	FString FileContents;
	if (!LoadFileContents(*this, OutputFilename, FileContents))
	{
		return false;
	}

	TestTrue(TEXT("CSV output should contain the header line"), FileContents.Contains(TEXT("Name,Value")));
	TestTrue(TEXT("CSV output should contain the written row"), FileContents.Contains(TEXT("Alpha,42")));
	return true;
}

bool FAngelscriptCSVWriterEscapingTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Dump_AngelscriptDumpTests_Private;
	const FString OutputFilename = FPaths::Combine(MakeUniqueDumpTestPath(TEXT("CSVWriterEscape")), TEXT("Escaped.csv"));

	FCSVWriter Writer;
	Writer.AddHeader({ TEXT("One"), TEXT("Two"), TEXT("Three") });
	Writer.AddRow({ TEXT("Comma,Value"), TEXT("Quote \"Here\""), TEXT("Line1\nLine2") });

	FString ErrorMessage;
	if (!TestTrue(TEXT("FCSVWriter escape save should succeed"), Writer.SaveToFile(OutputFilename, &ErrorMessage)))
	{
		AddError(ErrorMessage);
		return false;
	}

	FString FileContents;
	if (!LoadFileContents(*this, OutputFilename, FileContents))
	{
		return false;
	}

	TestTrue(TEXT("CSV should quote comma-containing fields"), FileContents.Contains(TEXT("\"Comma,Value\"")));
	TestTrue(TEXT("CSV should double embedded quotes"), FileContents.Contains(TEXT("\"Quote \"\"Here\"\"\"")));
	TestTrue(TEXT("CSV should preserve multiline fields inside quotes"), FileContents.Contains(TEXT("\"Line1\nLine2\"")));
	return true;
}

bool FAngelscriptStateDumpEndToEndTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Dump_AngelscriptDumpTests_Private;
	FString OutputDir;
	if (!RunDumpAll(*this, OutputDir))
	{
		return false;
	}

	for (const FString& ExpectedFilename : GetExpectedPhaseOneCsvFiles())
	{
		const FString CsvPath = FPaths::Combine(OutputDir, ExpectedFilename);
		TestTrue(*FString::Printf(TEXT("DumpAll should create '%s'"), *ExpectedFilename), IFileManager::Get().FileExists(*CsvPath));
	}

	return true;
}

bool FAngelscriptStateDumpSummaryTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Dump_AngelscriptDumpTests_Private;
	FString OutputDir;
	if (!RunDumpAll(*this, OutputDir))
	{
		return false;
	}

	const FString SummaryPath = FPaths::Combine(OutputDir, TEXT("DumpSummary.csv"));
	FString SummaryContents;
	if (!LoadFileContents(*this, SummaryPath, SummaryContents))
	{
		return false;
	}

	const TMap<FString, TPair<int32, FString>> SummaryRows = ParseDumpSummary(SummaryContents);
	for (const FString& ExpectedFilename : GetExpectedPhaseOneCsvFiles())
	{
		const TPair<int32, FString>* SummaryRow = SummaryRows.Find(ExpectedFilename);
		if (!TestNotNull(*FString::Printf(TEXT("DumpSummary should contain a row for '%s'"), *ExpectedFilename), SummaryRow))
		{
			return false;
		}

		const FString ExpectedStatus = GetExpectedSummaryStatus(ExpectedFilename);
		if (ExpectedStatus.IsEmpty())
		{
			// Sentinel empty expected-status means "Success or Skipped both acceptable"
			// (see GetExpectedSummaryStatus — UE 5.7 headless runs may legitimately
			// skip Debug* tables when no DebugServer is attached).
			const bool bAcceptable = SummaryRow->Value == TEXT("Success") || SummaryRow->Value == TEXT("Skipped");
			TestTrue(
				*FString::Printf(TEXT("'%s' should report either Success or Skipped (actual: '%s')"), *ExpectedFilename, *SummaryRow->Value),
				bAcceptable);
		}
		else
		{
			TestEqual(
				*FString::Printf(TEXT("'%s' should report the expected summary status"), *ExpectedFilename),
				SummaryRow->Value,
				ExpectedStatus);
		}
		TestTrue(*FString::Printf(TEXT("'%s' should report a non-negative row count"), *ExpectedFilename), SummaryRow->Key >= 0);
	}

	return true;
}

#endif
