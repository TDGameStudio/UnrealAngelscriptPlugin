#include "Dump/AngelscriptCSVWriter.h"
#include "Dump/AngelscriptDumpCommand.h"
#include "Dump/AngelscriptStateDump.h"

#include "Core/AngelscriptBinds.h"
#include "Testing/AngelscriptBindExecutionObservation.h"

#include "AngelscriptTestUtilities.h"
#include "CQTest.h"

#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_UNITTESTS

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

	TEST_METHOD(BindRegistrationsAreDeterministicAndReadOnly)
	{
		FResolvedProductionLikeEngine ResolvedEngine;
		ASSERT_THAT(IsTrue(
			AcquireProductionLikeEngine(
				*TestRunner,
				TEXT("Expected a production-like engine for bind registration dump tests"),
				ResolvedEngine),
			TEXT("Bind registration dump tests should acquire a production-like engine")));
		ASSERT_THAT(IsTrue(
			FAngelscriptBind::IsRegisteredCollectionSealedForTesting(),
			TEXT("Bind registration dump should observe the sealed process collection")));

		const int32 ExecutionInvocationCountBeforeDump =
			FAngelscriptBindExecutionObservation::GetInvocationCount();

		const FString FirstOutputDir = FAngelscriptStateDump::DumpAll(
			ResolvedEngine.Get(),
			FAngelscriptDumpTestHelpers::MakeUniqueDumpTestPath(TEXT("BindRegistrationsFirst")));
		const FString SecondOutputDir = FAngelscriptStateDump::DumpAll(
			ResolvedEngine.Get(),
			FAngelscriptDumpTestHelpers::MakeUniqueDumpTestPath(TEXT("BindRegistrationsSecond")));

		FString FirstContents;
		FString SecondContents;
		ASSERT_THAT(IsTrue(
			FAngelscriptDumpTestHelpers::LoadFileContents(
				*TestRunner,
				FPaths::Combine(FirstOutputDir, TEXT("BindRegistrations.csv")),
				FirstContents),
			TEXT("First BindRegistrations.csv should be readable")));
		ASSERT_THAT(IsTrue(
			FAngelscriptDumpTestHelpers::LoadFileContents(
				*TestRunner,
				FPaths::Combine(SecondOutputDir, TEXT("BindRegistrations.csv")),
				SecondContents),
			TEXT("Second BindRegistrations.csv should be readable")));

		ASSERT_THAT(AreEqual(
			FirstContents,
			SecondContents,
			TEXT("BindRegistrations.csv should be byte-deterministic for one sealed collection")));
		ASSERT_THAT(AreEqual(
			ExecutionInvocationCountBeforeDump,
			FAngelscriptBindExecutionObservation::GetInvocationCount(),
			TEXT("DumpAll should not execute bind callbacks while observing registrations")));

		TArray<FString> Lines;
		FirstContents.ParseIntoArrayLines(Lines, true);
		ASSERT_THAT(IsTrue(Lines.Num() > 1, TEXT("BindRegistrations.csv should contain registered bind rows")));
		ASSERT_THAT(AreEqual(
			FString(TEXT("OwnerModule,BindName,Phase,SourceFile,SourceLine,ExecutionStatus,DurationSeconds,FailureDiagnostic,ExecutionEpoch,PublicationEligibility,PublicationResult")),
			Lines[0],
			TEXT("BindRegistrations.csv should preserve its exact metadata and engine-result schema")));
		ASSERT_THAT(AreEqual(
			FAngelscriptBind::GetRegisteredBindCountForTesting(),
			Lines.Num() - 1,
			TEXT("BindRegistrations.csv should contain one row per sealed callback record")));

		const TMap<FString, int32> PhaseRanks = {
			{ TEXT("TypeDeclarations"), 0 },
			{ TEXT("TypeInfrastructure"), 1 },
			{ TEXT("ManualBindings"), 2 },
			{ TEXT("GeneratedBindings"), 3 },
			{ TEXT("ReflectionBindings"), 4 },
			{ TEXT("PostReflectionBindings"), 5 },
			{ TEXT("Finalization"), 6 },
		};

		int32 PreviousPhaseRank = INDEX_NONE;
		int32 ObservedExecutionRowCount = 0;
		for (int32 LineIndex = 1; LineIndex < Lines.Num(); ++LineIndex)
		{
			TArray<FString> Columns;
			Lines[LineIndex].ParseIntoArray(Columns, TEXT(","), false);
			ASSERT_THAT(AreEqual(
				11,
				Columns.Num(),
				*FString::Printf(TEXT("Bind registration row %d should match the exact schema"), LineIndex)));
			ASSERT_THAT(IsFalse(Columns[0].IsEmpty(), TEXT("Bind registration owner module should not be empty")));
			ASSERT_THAT(IsFalse(Columns[1].IsEmpty(), TEXT("Bind registration name should not be empty")));
			ASSERT_THAT(IsFalse(Columns[3].IsEmpty(), TEXT("Bind registration source file should not be empty")));
			ASSERT_THAT(IsTrue(
				FCString::Atoi(*Columns[4]) > 0,
				TEXT("Bind registration source line should be positive")));

			const int32* PhaseRank = PhaseRanks.Find(Columns[2]);
			ASSERT_THAT(IsNotNull(
				PhaseRank,
				*FString::Printf(TEXT("Bind registration row %d should use a known semantic phase"), LineIndex)));
			ASSERT_THAT(IsTrue(
				*PhaseRank >= PreviousPhaseRank,
				TEXT("Bind registration rows should preserve sealed semantic phase order")));
			PreviousPhaseRank = *PhaseRank;

			const bool bExecutionStatusIsKnown = Columns[5] == TEXT("Succeeded")
				|| Columns[5] == TEXT("Failed")
				|| Columns[5] == TEXT("NotExecuted")
				|| Columns[5] == TEXT("NotObserved");
			ASSERT_THAT(IsTrue(
				bExecutionStatusIsKnown,
				TEXT("Bind registration execution status should use the stable diagnostic vocabulary")));
			if (Columns[5] == TEXT("Succeeded") || Columns[5] == TEXT("Failed"))
			{
				++ObservedExecutionRowCount;
				ASSERT_THAT(IsTrue(
					FCString::Atod(*Columns[6]) >= 0.0,
					TEXT("Observed bind callback duration should be non-negative")));
				ASSERT_THAT(IsTrue(
					FCString::Atoi64(*Columns[8]) > 0,
					TEXT("Observed bind callback should retain its execution epoch")));
			}

			const bool bPublicationEligibilityIsKnown = Columns[9] == TEXT("Pending")
				|| Columns[9] == TEXT("Eligible")
				|| Columns[9] == TEXT("Blocked");
			ASSERT_THAT(IsTrue(
				bPublicationEligibilityIsKnown,
				TEXT("Bind registration publication eligibility should use the stable diagnostic vocabulary")));
			const bool bPublicationResultIsKnown = Columns[10] == TEXT("Published")
				|| Columns[10] == TEXT("NotPublished");
			ASSERT_THAT(IsTrue(
				bPublicationResultIsKnown,
				TEXT("Bind registration publication result should use the stable diagnostic vocabulary")));
		}

		const FAngelscriptBindExecutionSnapshot Snapshot =
			FAngelscriptBindExecutionObservation::GetLastSnapshot();
		if (Snapshot.EngineIdentity == reinterpret_cast<UPTRINT>(&ResolvedEngine.Get())
			&& !Snapshot.ProviderRecords.IsEmpty())
		{
			ASSERT_THAT(AreEqual(
				Snapshot.ProviderRecords.Num(),
				ObservedExecutionRowCount,
				TEXT("Matching engine observations should be joined to deterministic provider rows")));
		}
	}
};

#endif
