#include "Shared/AngelscriptLearningTrace.h"

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptLearningTraceSequenceTest,
	"Angelscript.TestModule.Shared.LearningTrace.Sequence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptLearningTraceFileExportTest,
	"Angelscript.TestModule.Shared.LearningTrace.FileExport",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptLearningTracePrettyPrinterTest,
	"Angelscript.TestModule.Shared.LearningTrace.PrettyPrinters",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptLearningTraceAssertionHelpersTest,
	"Angelscript.TestModule.Shared.LearningTrace.Assertions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptLearningTraceSequenceTest::RunTest(const FString& Parameters)
{
	FAngelscriptLearningTraceSinkConfig SinkConfig;
	SinkConfig.bEmitToAutomation = false;
	SinkConfig.bEmitToLog = false;
	SinkConfig.bEmitToFile = false;

	FAngelscriptLearningTraceSession Trace(TEXT("LearningTraceSequence"), SinkConfig);
	Trace.BeginPhase(EAngelscriptLearningTracePhase::EngineBootstrap);
	Trace.AddStep(TEXT("CreateEngine"), TEXT("Created a native script engine instance"));
	Trace.AddKeyValue(TEXT("ReturnCode"), TEXT("0"));
	Trace.BeginPhase(EAngelscriptLearningTracePhase::Compile);
	Trace.AddStep(TEXT("BuildModule"), TEXT("Built a module from in-memory source"));

	const TArray<FAngelscriptLearningTraceEvent>& Events = Trace.GetEvents();
	if (!TestEqual(TEXT("Trace session should capture two events"), Events.Num(), 2))
	{
		return false;
	}

	TestEqual(TEXT("The first event should use the current phase as its step prefix"), Events[0].StepId, FString(TEXT("EngineBootstrap.01")));
	TestEqual(TEXT("The second phase should reset numbering within its own prefix"), Events[1].StepId, FString(TEXT("Compile.01")));
	TestEqual(TEXT("The first event should capture one evidence key/value pair"), Events[0].Evidence.Num(), 1);

	const TArray<FString> SummaryLines = Trace.BuildSummaryLines();
	if (!TestEqual(TEXT("Summary lines should mirror the number of events"), SummaryLines.Num(), 2))
	{
		return false;
	}

	TestTrue(TEXT("Summary output should mention the generated step id"), SummaryLines[0].Contains(TEXT("EngineBootstrap.01")));
	TestTrue(TEXT("Summary output should mention the recorded action"), SummaryLines[0].Contains(TEXT("CreateEngine")));
	TestTrue(TEXT("Default summary output should include readable key/value evidence"), SummaryLines[0].Contains(TEXT("ReturnCode=0")));
	return true;
}

bool FAngelscriptLearningTraceFileExportTest::RunTest(const FString& Parameters)
{
	const FString OutputRoot = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/AngelscriptLearningTraceTests"));
	IFileManager::Get().DeleteDirectory(*OutputRoot, false, true);

	FAngelscriptLearningTraceSinkConfig SinkConfig;
	SinkConfig.bEmitToAutomation = false;
	SinkConfig.bEmitToLog = false;
	SinkConfig.bEmitToFile = true;
	SinkConfig.OutputRoot = OutputRoot;

	FAngelscriptLearningTraceSession Trace(TEXT("LearningTraceFileExport"), SinkConfig);
	Trace.BeginPhase(EAngelscriptLearningTracePhase::Execution);
	Trace.AddStep(TEXT("ExecuteFunction"), TEXT("Executed a function and captured the result"));
	Trace.AddKeyValue(TEXT("Result"), TEXT("42"));

	const bool bFlushed = Trace.FlushToFile();
	if (!TestTrue(TEXT("FlushToFile should create a trace artifact"), bFlushed))
	{
		return false;
	}

	const FString ExpectedFile = FPaths::Combine(OutputRoot, TEXT("LearningTraceFileExport.trace.txt"));
	if (!TestTrue(TEXT("Trace file should exist after FlushToFile"), IFileManager::Get().FileExists(*ExpectedFile)))
	{
		return false;
	}

	FString SavedText;
	if (!FFileHelper::LoadFileToString(SavedText, *ExpectedFile))
	{
		AddError(FString::Printf(TEXT("Expected trace file '%s' should be readable"), *ExpectedFile));
		return false;
	}

	TestTrue(TEXT("Saved trace file should contain the step id"), SavedText.Contains(TEXT("Execution.01")));
	TestTrue(TEXT("Saved trace file should contain the recorded evidence"), SavedText.Contains(TEXT("Result: 42")));
	return true;
}

bool FAngelscriptLearningTracePrettyPrinterTest::RunTest(const FString& Parameters)
{
	TArray<FAngelscriptLearningTraceKeyValue> EngineProperties;
	EngineProperties.Add({TEXT("asEP_OPTIMIZE_BYTECODE"), TEXT("1")});
	EngineProperties.Add({TEXT("asEP_ALLOW_DOUBLE_TYPE"), TEXT("1")});

	const FString EnginePropertySummary = FormatLearningTraceKeyValueList(EngineProperties, TEXT("EngineProperties"));
	TestTrue(TEXT("Engine property formatter should render the section label"), EnginePropertySummary.Contains(TEXT("EngineProperties")));
	TestTrue(TEXT("Engine property formatter should render key/value pairs"), EnginePropertySummary.Contains(TEXT("asEP_OPTIMIZE_BYTECODE=1")));

	TArray<FAngelscriptLearningTraceDiagnostic> Diagnostics;
	Diagnostics.Add({TEXT("Sample.as"), 7, 3, TEXT("Error"), TEXT("Expected ';' before identifier")});

	const FString DiagnosticSummary = FormatLearningTraceDiagnostics(Diagnostics);
	TestTrue(TEXT("Diagnostic formatter should render file and line info"), DiagnosticSummary.Contains(TEXT("Sample.as:7:3")));
	TestTrue(TEXT("Diagnostic formatter should render severity and message"), DiagnosticSummary.Contains(TEXT("[Error] Expected ';' before identifier")));

	TArray<uint32> Bytecode = {0x2Au, 0x10u, 0x33u};
	const FString BytecodeSummary = FormatLearningTraceBytecode(Bytecode, 2);
	TestTrue(TEXT("Bytecode formatter should include the dword count"), BytecodeSummary.Contains(TEXT("3 dwords")));
	TestTrue(TEXT("Bytecode formatter should truncate to the requested preview size"), BytecodeSummary.Contains(TEXT("0x0000002A, 0x00000010")));
	TestFalse(TEXT("Bytecode formatter should omit values beyond the preview count"), BytecodeSummary.Contains(TEXT("0x00000033")));

	TArray<FString> CallstackLines = {
		TEXT("Compile.01 | BuildModule | Built a module"),
		TEXT("Execution.01 | Execute | Returned 42")
	};
	const FString CallstackSummary = FormatLearningTraceStringList(CallstackLines, TEXT("Callstack"));
	TestTrue(TEXT("Callstack formatter should render the list title"), CallstackSummary.Contains(TEXT("Callstack")));
	TestTrue(TEXT("Callstack formatter should include each line item"), CallstackSummary.Contains(TEXT("Execution.01 | Execute | Returned 42")));
	return true;
}

bool FAngelscriptLearningTraceAssertionHelpersTest::RunTest(const FString& Parameters)
{
	FAngelscriptLearningTraceSinkConfig SinkConfig;
	SinkConfig.bEmitToAutomation = false;
	SinkConfig.bEmitToLog = false;
	SinkConfig.bEmitToFile = false;

	FAngelscriptLearningTraceSession Trace(TEXT("LearningTraceAssertions"), SinkConfig);
	Trace.BeginPhase(EAngelscriptLearningTracePhase::EngineBootstrap);
	Trace.AddStep(TEXT("CreateEngine"), TEXT("Created the engine"));
	Trace.BeginPhase(EAngelscriptLearningTracePhase::Compile);
	Trace.AddStep(TEXT("BuildModule"), TEXT("Compiled a module"));

	const TArray<FAngelscriptLearningTraceEvent>& Events = Trace.GetEvents();
	const TArray<EAngelscriptLearningTracePhase> ExpectedPhases = {
		EAngelscriptLearningTracePhase::EngineBootstrap,
		EAngelscriptLearningTracePhase::Compile,
	};

	if (!TestTrue(TEXT("Phase order assertion helper should accept the expected phase sequence"),
		AssertLearningTracePhaseSequence(*this, Events, ExpectedPhases)))
	{
		return false;
	}

	TestTrue(TEXT("Keyword assertion helper should find a known action token"),
		AssertLearningTraceContainsKeyword(*this, Events, TEXT("BuildModule")));
	TestTrue(TEXT("Minimum observation assertion helper should pass when enough events exist"),
		AssertLearningTraceMinimumEventCount(*this, Events, 2));
	TestFalse(TEXT("Phase order assertion helper should reject a wrong sequence"),
		AssertLearningTracePhaseSequence(*this, Events, {EAngelscriptLearningTracePhase::Compile, EAngelscriptLearningTracePhase::EngineBootstrap}, false));
	return true;
}

#endif
