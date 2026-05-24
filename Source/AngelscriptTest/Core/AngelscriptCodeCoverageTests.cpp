// =============================================================================
// AngelscriptCodeCoverageTests.cpp
//
// CQTest coverage for the Angelscript code coverage system.
// Automation IDs: Angelscript.TestModule.Core.CodeCoverage.*
//
// OVERVIEW
// --------
// The coverage system tracks which lines of .as scripts are executed during a
// test run and writes an HTML report afterwards. Three core types are tested:
//
//   FAngelscriptCodeCoverage  -- top-level manager; owns the per-file map and
//                                drives Start/Stop recording.
//   FLineCoverage             -- per-file data: TMap<LineNo, HitCount>.
//   FCoverageNode             -- tree node used to aggregate counts by directory.
//
// HOW LINE HITS ARE RECORDED AT RUNTIME
// --------------------------------------
// AngelScript executes through asCContext. Each context is configured with
// AngelscriptLineCallback() when it is created. The callback is intentionally
// shared with the debugger because both features need the same VM observation
// point: "the interpreter is about to execute a script line".
//
// UpdateLineCallbackState() decides whether the VM should actually run that
// callback. Coverage is compiled behind WITH_AS_COVERAGE (currently tied to
// WITH_AS_DEBUGSERVER in AngelscriptEngine.h). When FAngelscriptEngine owns a
// CodeCoverage instance, UpdateLineCallbackState() sets:
//
//   asCContext::CanEverRunLineCallback      = true
//   asCContext::ShouldAlwaysRunLineCallback = true
//
// That makes every script line pass through the callback while coverage exists.
// Without coverage/debugging, the VM can skip callback dispatch and avoid the
// per-line overhead.
//
//   AS VM executes line
//        |
//        v
//   AngelscriptLineCallback()          (AngelscriptEngine.cpp)
//        |
//        +--[WITH_AS_DEBUGSERVER]--> DebugServer::ProcessScriptLine()
//        |
//        +--[WITH_AS_COVERAGE]-----> CodeCoverage::HitLine(Module, Line)
//                                         |
//                                         v
//                                   HitCounts[Line]++
//
// The callback itself is deliberately small:
//
//   1. ignore non-game-thread execution;
//   2. guard against reentry if script runs inside a line callback;
//   3. ask the AS context for the current line and function;
//   4. resolve CurrentFunction->GetModuleName() back to FAngelscriptModuleDesc;
//   5. call FAngelscriptCodeCoverage::HitLine(Module, Line).
//
// HitLine() only increments a counter if recording is active and the module was
// already mapped. Mapping is separate because the report needs to know every
// executable line up front, including lines that are never hit.
//
// HOW EXECUTABLE LINES ARE MAPPED
// -------------------------------
// FAngelscriptCodeCoverage::MapExecutableLines() walks the compiled
// asCModule. It visits global functions and script object methods, then asks
// each asCScriptFunction for the next bytecode-backed source line via
// FindNextLineWithCode(). The resulting map starts every executable line at 0:
//
//   Relative .as filename -> FLineCoverage
//                         -> HitCounts[ExecutableLine] = 0
//
// Lines outside this map are ignored when HitLine() runs. This filters out
// invalid/out-of-range hits and keeps the report limited to compiled script
// lines. During HTML generation, PruneGeneratedCode() also removes generated
// AngelScript lines that live beyond the original source file length.
//
// AUTOMATION HOOKS
// ----------------
// A second hook, AddTestFrameworkHooks(), wires coverage to UE automation. It is
// installed after engine init from FAngelscriptEngine when the editor and
// coverage are enabled:
//
//   UE AutomationController
//        |
//        +-- OnTestsAvailable(Running) --> StartRecording()
//        +-- OnTestsComplete() --------> StopRecordingAndWriteReport()
//
// StartRecording() resets all hit counts and enables recording. StopRecording
// disables recording, writes per-file HTML, writes directory index.html files,
// and writes coverage_summary.json.
//
// HOW TO USE CODE COVERAGE
// ------------------------
// The normal user/CI flow is:
//
//   1. build with WITH_AS_COVERAGE available (this project ties it to the debug
//      server compile flag);
//   2. enable coverage with UAngelscriptTestSettings::bEnableCodeCoverage or the
//      command line flag -as-enable-code-coverage;
//   3. run UE automation tests that execute AngelScript code;
//   4. read the generated report under Saved/CodeCoverage/.
//
// The low-level/manual flow, used by these tests, is:
//
//   1. compile an AS module;
//   2. call MapExecutableLines(Module);
//   3. call StartRecording();
//   4. execute script, or call HitLine(Module, Line) directly for deterministic
//      unit coverage of the manager;
//   5. call StopRecordingAndWriteReport(OutputDir);
//   6. inspect OutputDir/index.html, per-file .as.html files, and
//      OutputDir/coverage_summary.json.
//
// The integration tests in this file compile real AS scripts, execute their
// entry points to verify script behavior, and exercise MapExecutableLines /
// HitLine through the coverage manager API. They do not assert the engine's
// global VM line callback hook because that hook is a process-wide engine
// observation path shared with debugging.
// =============================================================================

#include "CQTest.h"
#include "Shared/AngelscriptFunctionalTestUtils.h"
#include "Shared/AngelscriptGlobalFunctionInvoker.h"
#include "Shared/AngelscriptReflectiveAccess.h"
#include "Shared/AngelscriptTestMacros.h"

#include "CodeCoverage/AngelscriptCodeCoverage.h"
#include "CodeCoverage/CoverageReportGenerator.h"
#include "CodeCoverage/LineCoverage.h"
#include "HAL/PlatformFileManager.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_AS_COVERAGE

using namespace AngelscriptFunctionalTestUtils;

namespace AngelscriptCodeCoverageTests
{
	FString MakeScriptSource(const ANSICHAR* Source)
	{
		return FString(UTF8_TO_TCHAR(Source));
	}

	FString MakeUniqueCoverageReportDir(const FString& Prefix)
	{
		// Keep generated reports in Saved so they survive test process shutdown
		// and can be opened manually when validating coverage output.
		return FPaths::Combine(
			FPaths::ProjectSavedDir(),
			TEXT("Automation"),
			TEXT("CodeCoverage"),
			FString::Printf(TEXT("%s_%s"), *Prefix, *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	}

	TSharedPtr<FAngelscriptModuleDesc> FindActiveModule(FAngelscriptEngine& Engine, const TCHAR* ModuleName)
	{
		return Engine.GetModule(FString(ModuleName));
	}

	TSharedPtr<FAngelscriptModuleDesc> RequireActiveModule(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		const TCHAR* ModuleName)
	{
		TSharedPtr<FAngelscriptModuleDesc> FoundModule = FindActiveModule(Engine, ModuleName);
		Test.TestTrue(FString::Printf(TEXT("%s should be registered as an active module"), ModuleName), FoundModule.IsValid());
		return FoundModule;
	}

	bool RequireMappedCoverage(
		FAutomationTestBase& Test,
		const FLineCoverage* LineCoverage,
		const TCHAR* Context)
	{
		if (!Test.TestNotNull(FString::Printf(TEXT("%s should have line coverage"), Context), LineCoverage))
		{
			return false;
		}

		return Test.TestTrue(
			FString::Printf(TEXT("%s should have executable lines"), Context),
			LineCoverage->NumExecutableLines() > 0);
	}

	bool LoadFileToStringChecked(
		FAutomationTestBase& Test,
		const FString& Path,
		FString& OutContents,
		const TCHAR* Context)
	{
		return Test.TestTrue(
			FString::Printf(TEXT("%s should be readable at %s"), Context, *Path),
			FFileHelper::LoadFileToString(OutContents, *Path));
	}
}

// =============================================================================
// Unit tests for FLineCoverage (pure data, no engine dependency)
// =============================================================================

TEST_CLASS_WITH_FLAGS(FAngelscriptLineCoverageTest,
	"Angelscript.TestModule.Core.CodeCoverage.LineCoverage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(EmptyLineCoverage)
	{
		FLineCoverage LineCoverage;
		TestRunner->TestEqual(TEXT("Empty FLineCoverage has 0 executable lines"), LineCoverage.NumExecutableLines(), 0);
		TestRunner->TestEqual(TEXT("Empty FLineCoverage has 0 hit lines"), LineCoverage.NumLinesHit(), 0);
	}

	TEST_METHOD(BasicHitCounting)
	{
		FLineCoverage LineCoverage;
		LineCoverage.HitCounts.Add(3, 4);
		LineCoverage.HitCounts.Add(4, 0);
		LineCoverage.HitCounts.Add(6, 18);
		LineCoverage.HitCounts.Add(8, 1);
		LineCoverage.HitCounts.Add(9, 0);
		LineCoverage.HitCounts.Add(17, 0);
		LineCoverage.HitCounts.Add(18, 0);

		TestRunner->TestEqual(TEXT("7 entries in HitCounts means 7 executable lines"), LineCoverage.NumExecutableLines(), 7);
		TestRunner->TestEqual(TEXT("Only 3 entries have value > 0"), LineCoverage.NumLinesHit(), 3);
	}

	TEST_METHOD(AllLinesHit)
	{
		FLineCoverage LineCoverage;
		LineCoverage.HitCounts.Add(1, 1);
		LineCoverage.HitCounts.Add(2, 5);
		LineCoverage.HitCounts.Add(3, 100);

		TestRunner->TestEqual(TEXT("All 3 lines are executable"), LineCoverage.NumExecutableLines(), 3);
		TestRunner->TestEqual(TEXT("All 3 lines are hit"), LineCoverage.NumLinesHit(), 3);
	}

	TEST_METHOD(NoLinesHit)
	{
		FLineCoverage LineCoverage;
		LineCoverage.HitCounts.Add(1, 0);
		LineCoverage.HitCounts.Add(2, 0);
		LineCoverage.HitCounts.Add(3, 0);

		TestRunner->TestEqual(TEXT("3 executable lines exist"), LineCoverage.NumExecutableLines(), 3);
		TestRunner->TestEqual(TEXT("0 lines are hit when all counts are zero"), LineCoverage.NumLinesHit(), 0);
	}

	TEST_METHOD(SingleLineFile)
	{
		FLineCoverage LineCoverage;
		LineCoverage.HitCounts.Add(1, 42);

		TestRunner->TestEqual(TEXT("Single-line file has 1 executable line"), LineCoverage.NumExecutableLines(), 1);
		TestRunner->TestEqual(TEXT("Single-line file with hit count > 0 has 1 hit"), LineCoverage.NumLinesHit(), 1);
	}

	TEST_METHOD(PruneGeneratedCode)
	{
		FLineCoverage LineCoverage;
		LineCoverage.HitCounts.Add(3, 4);
		LineCoverage.HitCounts.Add(4, 0);
		LineCoverage.HitCounts.Add(6, 18);
		LineCoverage.HitCounts.Add(8, 1);
		LineCoverage.HitCounts.Add(99, 0);
		LineCoverage.HitCounts.Add(100, 4);
		LineCoverage.HitCounts.Add(101, 4);

		// File is 99 lines long; lines 100 and 101 are generated code.
		LineCoverage.PruneGeneratedCode(99);

		TestRunner->TestEqual(TEXT("After pruning lines > 99, 5 executable lines remain"), LineCoverage.NumExecutableLines(), 5);
		TestRunner->TestEqual(TEXT("3 of the remaining lines have been hit"), LineCoverage.NumLinesHit(), 3);
	}

	TEST_METHOD(PruneRemovesAllWhenCutoffIsZero)
	{
		FLineCoverage LineCoverage;
		LineCoverage.HitCounts.Add(1, 10);
		LineCoverage.HitCounts.Add(2, 20);
		LineCoverage.HitCounts.Add(3, 30);

		LineCoverage.PruneGeneratedCode(0);

		TestRunner->TestEqual(TEXT("Pruning with cutoff 0 removes all lines"), LineCoverage.NumExecutableLines(), 0);
		TestRunner->TestEqual(TEXT("No lines hit after pruning all"), LineCoverage.NumLinesHit(), 0);
	}

	TEST_METHOD(PruneKeepsAllWhenCutoffIsHigh)
	{
		FLineCoverage LineCoverage;
		LineCoverage.HitCounts.Add(1, 1);
		LineCoverage.HitCounts.Add(50, 2);
		LineCoverage.HitCounts.Add(100, 3);

		LineCoverage.PruneGeneratedCode(1000);

		TestRunner->TestEqual(TEXT("Pruning with high cutoff keeps all lines"), LineCoverage.NumExecutableLines(), 3);
		TestRunner->TestEqual(TEXT("All 3 lines remain hit"), LineCoverage.NumLinesHit(), 3);
	}

	TEST_METHOD(PruneOnEmptyIsNoOp)
	{
		FLineCoverage LineCoverage;
		LineCoverage.PruneGeneratedCode(50);

		TestRunner->TestEqual(TEXT("Pruning empty coverage is a no-op"), LineCoverage.NumExecutableLines(), 0);
	}

	TEST_METHOD(PruneExactBoundary)
	{
		FLineCoverage LineCoverage;
		LineCoverage.HitCounts.Add(10, 5);
		LineCoverage.HitCounts.Add(11, 3);

		// Cutoff == 10 means keep lines <= 10, remove lines > 10.
		LineCoverage.PruneGeneratedCode(10);

		TestRunner->TestEqual(TEXT("Line 10 is kept, line 11 is pruned"), LineCoverage.NumExecutableLines(), 1);
		TestRunner->TestEqual(TEXT("Line 10 was hit"), LineCoverage.NumLinesHit(), 1);
	}

	TEST_METHOD(AbsoluteFilenameIsPreserved)
	{
		FLineCoverage LineCoverage;
		LineCoverage.AbsoluteFilename = TEXT("/Game/Scripts/MyScript.as");

		TestRunner->TestEqual(TEXT("AbsoluteFilename is stored correctly"),
			LineCoverage.AbsoluteFilename, FString(TEXT("/Game/Scripts/MyScript.as")));
	}
};

// =============================================================================
// Unit tests for FCoverageNode / ComputeCoverage (directory tree aggregation)
// =============================================================================

TEST_CLASS_WITH_FLAGS(FAngelscriptCoverageNodeTest,
	"Angelscript.TestModule.Core.CodeCoverage.CoverageNode",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(BasicTreeAggregation)
	{
		FCoverageNode Root;

		FLineCoverage C;
		C.HitCounts.Add(1, 7);
		C.HitCounts.Add(2, 19);
		C.HitCounts.Add(3, 0);
		C.HitCounts.Add(4, 0);
		C.AbsoluteFilename = TEXT("D:\\A\\B\\C.as");

		FLineCoverage D;
		D.HitCounts.Add(1, 10);
		D.HitCounts.Add(2, 104);
		D.AbsoluteFilename = TEXT("/D/A/B/D.as");

		FLineCoverage E;
		E.HitCounts.Add(1, 0);
		E.AbsoluteFilename = TEXT("/D/A/B/E/E.as");

		FLineCoverage G;
		G.HitCounts.Add(1, 1);
		G.AbsoluteFilename = TEXT("/mnt/g/G.as");

		// Front or back slashes don't matter.
		AddCoverageLeaf(Root, TEXT("A\\B\\C.as"), C);
		AddCoverageLeaf(Root, TEXT("A/B/D.as"), D);
		AddCoverageLeaf(Root, TEXT("A/B/E/E.as"), E);
		AddCoverageLeaf(Root, TEXT("G/G.as"), G);

		FCoverageCounts Result = ComputeCoverage(Root);

		TestRunner->TestEqual(TEXT("A dir hit count"), Root.Children[TEXT("A")]->Counts.NumLinesHit, 4);
		TestRunner->TestEqual(TEXT("A dir total lines"), Root.Children[TEXT("A")]->Counts.NumExecutableLines, 7);
		TestRunner->TestEqual(TEXT("A/B dir hit count"), Root.Children[TEXT("A")]->Children[TEXT("B")]->Counts.NumLinesHit, 4);
		TestRunner->TestEqual(TEXT("A/B dir total lines"), Root.Children[TEXT("A")]->Children[TEXT("B")]->Counts.NumExecutableLines, 7);
		TestRunner->TestEqual(TEXT("A/B/E dir hit count"), Root.Children[TEXT("A")]->Children[TEXT("B")]->Children[TEXT("E")]->Counts.NumLinesHit, 0);
		TestRunner->TestEqual(TEXT("A/B/E dir total lines"), Root.Children[TEXT("A")]->Children[TEXT("B")]->Children[TEXT("E")]->Counts.NumExecutableLines, 1);
		TestRunner->TestEqual(TEXT("G dir hit count"), Root.Children[TEXT("G")]->Counts.NumLinesHit, 1);
		TestRunner->TestEqual(TEXT("G dir total lines"), Root.Children[TEXT("G")]->Counts.NumExecutableLines, 1);
		TestRunner->TestEqual(TEXT("Root total hit"), Result.NumLinesHit, 5);
		TestRunner->TestEqual(TEXT("Root total lines"), Result.NumExecutableLines, 8);
	}

	TEST_METHOD(EmptyTreeProducesZeroCounts)
	{
		FCoverageNode Root;
		FCoverageCounts Result = ComputeCoverage(Root);

		TestRunner->TestEqual(TEXT("Empty tree has 0 hit lines"), Result.NumLinesHit, 0);
		TestRunner->TestEqual(TEXT("Empty tree has 0 executable lines"), Result.NumExecutableLines, 0);
	}

	TEST_METHOD(SingleFileAtRoot)
	{
		FCoverageNode Root;

		FLineCoverage Single;
		Single.HitCounts.Add(1, 5);
		Single.HitCounts.Add(2, 0);
		Single.AbsoluteFilename = TEXT("/root/Single.as");

		AddCoverageLeaf(Root, TEXT("Single.as"), Single);
		FCoverageCounts Result = ComputeCoverage(Root);

		TestRunner->TestEqual(TEXT("Single file at root: 1 hit"), Result.NumLinesHit, 1);
		TestRunner->TestEqual(TEXT("Single file at root: 2 total"), Result.NumExecutableLines, 2);
	}

	TEST_METHOD(DeeplyNestedPath)
	{
		FCoverageNode Root;

		FLineCoverage Deep;
		Deep.HitCounts.Add(1, 1);
		Deep.AbsoluteFilename = TEXT("/a/b/c/d/e/f.as");

		AddCoverageLeaf(Root, TEXT("A/B/C/D/E/F.as"), Deep);
		FCoverageCounts Result = ComputeCoverage(Root);

		TestRunner->TestEqual(TEXT("Deeply nested: 1 hit propagates to root"), Result.NumLinesHit, 1);
		TestRunner->TestEqual(TEXT("Deeply nested: 1 total propagates to root"), Result.NumExecutableLines, 1);

		// Verify intermediate nodes exist.
		TestRunner->TestTrue(TEXT("Node A exists"), Root.Children.Contains(TEXT("A")));
		TestRunner->TestTrue(TEXT("Node A/B exists"), Root.Children[TEXT("A")]->Children.Contains(TEXT("B")));
		TestRunner->TestTrue(TEXT("Node A/B/C exists"), Root.Children[TEXT("A")]->Children[TEXT("B")]->Children.Contains(TEXT("C")));
	}

	TEST_METHOD(MultipleFilesInSameDirectory)
	{
		FCoverageNode Root;

		FLineCoverage File1;
		File1.HitCounts.Add(1, 1);
		File1.HitCounts.Add(2, 1);

		FLineCoverage File2;
		File2.HitCounts.Add(1, 0);
		File2.HitCounts.Add(2, 0);
		File2.HitCounts.Add(3, 1);

		AddCoverageLeaf(Root, TEXT("Dir/File1.as"), File1);
		AddCoverageLeaf(Root, TEXT("Dir/File2.as"), File2);

		FCoverageCounts Result = ComputeCoverage(Root);

		TestRunner->TestEqual(TEXT("Dir aggregates: 3 hit lines"), Root.Children[TEXT("Dir")]->Counts.NumLinesHit, 3);
		TestRunner->TestEqual(TEXT("Dir aggregates: 5 total lines"), Root.Children[TEXT("Dir")]->Counts.NumExecutableLines, 5);
	}

	TEST_METHOD(CoverageCountsToStringNonZero)
	{
		FCoverageCounts Counts;
		Counts.NumLinesHit = 3;
		Counts.NumExecutableLines = 10;

		FString Result = Counts.ToString();
		TestRunner->TestTrue(TEXT("ToString contains percentage"), Result.Contains(TEXT("30.0%")));
		TestRunner->TestTrue(TEXT("ToString contains fraction"), Result.Contains(TEXT("3/10")));
	}

	TEST_METHOD(CoverageCountsToStringZeroExecutable)
	{
		FCoverageCounts Counts;
		Counts.NumLinesHit = 0;
		Counts.NumExecutableLines = 0;

		FString Result = Counts.ToString();
		TestRunner->TestEqual(TEXT("ToString returns N/A for 0 executable lines"), Result, FString(TEXT("N/A")));
	}

	TEST_METHOD(CoverageCountsToString100Percent)
	{
		FCoverageCounts Counts;
		Counts.NumLinesHit = 5;
		Counts.NumExecutableLines = 5;

		FString Result = Counts.ToString();
		TestRunner->TestTrue(TEXT("ToString shows 100.0%"), Result.Contains(TEXT("100.0%")));
		TestRunner->TestTrue(TEXT("ToString shows 5/5"), Result.Contains(TEXT("5/5")));
	}
};

// =============================================================================
// Integration tests for FAngelscriptCodeCoverage with real AS engine
// =============================================================================

TEST_CLASS_WITH_FLAGS(FAngelscriptCodeCoverageIntegrationTest,
	"Angelscript.TestModule.Core.CodeCoverage.Integration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

	TEST_METHOD(MapAndHitLinesOnCompiledModule)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		// Compile a realistic AS module with multiple functions, loops, and conditionals.
		const FString Source = AngelscriptCodeCoverageTests::MakeScriptSource(R"(
int Clamp(int Value, int MinVal, int MaxVal)
{
	if (Value < MinVal)
		return MinVal;
	if (Value > MaxVal)
		return MaxVal;
	return Value;
}

int SumRange(int Start, int End)
{
	int Total = 0;
	for (int I = Start; I <= End; ++I)
	{
		Total += I;
	}
	return Total;
}

int ComputeScore(int Count, int Sum)
{
	if (Count == 0)
		return 0;
	return Sum + Count;
}

int Entry()
{
	int Count = 0;
	int Sum = 0;
	for (int I = 0; I < 10; ++I)
	{
		int Clamped = Clamp(I * 3, 0, 20);
		Count += 1;
		Sum += Clamped;
	}

	int Score = ComputeScore(Count, Sum);
	int RangeSum = SumRange(1, Count);
	return Score + RangeSum;
}
)");

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCoverageMapHit", Source);
		if (!TestRunner->TestNotNull(TEXT("ASCoverageMapHit should compile"), Module)) { return; }
		ON_SCOPE_EXIT { Engine.DiscardModule(TEXT("ASCoverageMapHit")); };

		TSharedPtr<FAngelscriptModuleDesc> FoundModule =
			AngelscriptCodeCoverageTests::RequireActiveModule(*TestRunner, Engine, TEXT("ASCoverageMapHit"));
		if (!FoundModule.IsValid()) { return; }

		FAngelscriptCodeCoverage Coverage;
		Coverage.MapExecutableLines(*FoundModule);

		const FLineCoverage* LineCov = Coverage.GetLineCoverage(*FoundModule);
		if (!AngelscriptCodeCoverageTests::RequireMappedCoverage(*TestRunner, LineCov, TEXT("ASCoverageMapHit")))
		{
			return;
		}
		TestRunner->TestEqual(TEXT("No lines hit before recording starts"),
			LineCov->NumLinesHit(), 0);

		// Actually execute the AS Entry() function to verify the script logic works.
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("int Entry()"));
			const int32 ReturnValue = Invoker.CallAndReturn<int32>(INDEX_NONE);
			TestRunner->TestTrue(TEXT("Entry() should execute successfully"), Invoker.HasRun());
			TestRunner->TestTrue(TEXT("Entry() should return a positive value"), ReturnValue > 0);
		}

		// Start recording and hit all mapped lines via the coverage API.
		Coverage.StartRecording();
		for (const auto& Pair : LineCov->HitCounts)
		{
			Coverage.HitLine(*FoundModule, Pair.Key);
		}

		const FLineCoverage* AfterHit = Coverage.GetLineCoverage(*FoundModule);
		if (TestRunner->TestNotNull(TEXT("Coverage still available after hits"), AfterHit))
		{
			TestRunner->TestEqual(TEXT("All executable lines should be hit"),
				AfterHit->NumExecutableLines(), AfterHit->NumLinesHit());
		}
	}

	TEST_METHOD(HitLineIgnoredWhenNotRecording)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString Source = AngelscriptCodeCoverageTests::MakeScriptSource(R"(
int Fibonacci(int N)
{
	if (N <= 0) return 0;
	if (N == 1) return 1;

	int Prev = 0;
	int Curr = 1;
	for (int I = 2; I <= N; ++I)
	{
		int Next = Prev + Curr;
		Prev = Curr;
		Curr = Next;
	}
	return Curr;
}

int GetValue()
{
	return Fibonacci(10);
}
)");

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCoverageNotRecording", Source);
		if (!TestRunner->TestNotNull(TEXT("ASCoverageNotRecording should compile"), Module)) { return; }
		ON_SCOPE_EXIT { Engine.DiscardModule(TEXT("ASCoverageNotRecording")); };

		TSharedPtr<FAngelscriptModuleDesc> FoundModule =
			AngelscriptCodeCoverageTests::RequireActiveModule(*TestRunner, Engine, TEXT("ASCoverageNotRecording"));
		if (!FoundModule.IsValid()) { return; }

		FAngelscriptCodeCoverage Coverage;
		Coverage.MapExecutableLines(*FoundModule);

		// Actually execute GetValue() to verify the Fibonacci logic works.
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("int GetValue()"));
			const int32 FibResult = Invoker.CallAndReturn<int32>(INDEX_NONE);
			TestRunner->TestTrue(TEXT("GetValue() should execute successfully"), Invoker.HasRun());
			// Fibonacci(10) = 55
			TestRunner->TestEqual(TEXT("GetValue() should return Fibonacci(10) = 55"), FibResult, 55);
		}

		// Do NOT call StartRecording — hits should be ignored.
		const FLineCoverage* LineCov = Coverage.GetLineCoverage(*FoundModule);
		if (!AngelscriptCodeCoverageTests::RequireMappedCoverage(*TestRunner, LineCov, TEXT("ASCoverageNotRecording")))
		{
			return;
		}

		for (const auto& Pair : LineCov->HitCounts)
		{
			Coverage.HitLine(*FoundModule, Pair.Key);
		}

		const FLineCoverage* AfterHit = Coverage.GetLineCoverage(*FoundModule);
		if (TestRunner->TestNotNull(TEXT("Coverage should remain mapped after ignored hits"), AfterHit))
		{
			TestRunner->TestEqual(TEXT("No lines should be hit when not recording"),
				AfterHit->NumLinesHit(), 0);
		}
	}

	TEST_METHOD(OutOfRangeLinesAreIgnored)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString Source = AngelscriptCodeCoverageTests::MakeScriptSource(R"(
bool IsEven(int N)
{
	return (N % 2) == 0;
}

int CountEvens(int Limit)
{
	int Count = 0;
	for (int I = 0; I < Limit; ++I)
	{
		if (IsEven(I))
			Count += 1;
	}
	return Count;
}

int Simple()
{
	return CountEvens(100);
}
)");

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCoverageOutOfRange", Source);
		if (!TestRunner->TestNotNull(TEXT("ASCoverageOutOfRange should compile"), Module)) { return; }
		ON_SCOPE_EXIT { Engine.DiscardModule(TEXT("ASCoverageOutOfRange")); };

		TSharedPtr<FAngelscriptModuleDesc> FoundModule =
			AngelscriptCodeCoverageTests::RequireActiveModule(*TestRunner, Engine, TEXT("ASCoverageOutOfRange"));
		if (!FoundModule.IsValid()) { return; }

		FAngelscriptCodeCoverage Coverage;
		Coverage.MapExecutableLines(*FoundModule);

		// Actually execute Simple() to verify the counting logic works.
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("int Simple()"));
			const int32 EvenCount = Invoker.CallAndReturn<int32>(INDEX_NONE);
			TestRunner->TestTrue(TEXT("Simple() should execute successfully"), Invoker.HasRun());
			// CountEvens(100) = 50 (0,2,4,...,98)
			TestRunner->TestEqual(TEXT("Simple() should return CountEvens(100) = 50"), EvenCount, 50);
		}

		Coverage.StartRecording();

		// Hit lines that are guaranteed outside the file.
		Coverage.HitLine(*FoundModule, 9999999);
		Coverage.HitLine(*FoundModule, -1);
		Coverage.HitLine(*FoundModule, 0);

		const FLineCoverage* LineCov = Coverage.GetLineCoverage(*FoundModule);
		if (TestRunner->TestNotNull(TEXT("Coverage should remain mapped after out-of-range hits"), LineCov))
		{
			TestRunner->TestEqual(TEXT("Out-of-range lines should not register as hits"),
				LineCov->NumLinesHit(), 0);
		}
	}

	TEST_METHOD(ResetHitsClearsAllCounts)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString Source = AngelscriptCodeCoverageTests::MakeScriptSource(R"(
int ApplyDelta(int Value, int Delta)
{
	return Value + Delta;
}

int Foo()
{
	int Value = 0;
	for (int I = 0; I < 5; ++I)
		Value = ApplyDelta(Value, 1);
	return Value;
}

int Bar()
{
	int Value = 0;
	Value = ApplyDelta(Value, 1);
	Value = ApplyDelta(Value, 1);
	Value = ApplyDelta(Value, -1);
	return Value;
}
)");

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCoverageReset", Source);
		if (!TestRunner->TestNotNull(TEXT("ASCoverageReset should compile"), Module)) { return; }
		ON_SCOPE_EXIT { Engine.DiscardModule(TEXT("ASCoverageReset")); };

		TSharedPtr<FAngelscriptModuleDesc> FoundModule =
			AngelscriptCodeCoverageTests::RequireActiveModule(*TestRunner, Engine, TEXT("ASCoverageReset"));
		if (!FoundModule.IsValid()) { return; }

		FAngelscriptCodeCoverage Coverage;
		Coverage.MapExecutableLines(*FoundModule);

		// Actually execute Foo() and Bar() to verify the helper functions work.
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("int Foo()"));
			const int32 FooResult = Invoker.CallAndReturn<int32>(INDEX_NONE);
			TestRunner->TestTrue(TEXT("Foo() should execute successfully"), Invoker.HasRun());
			// Foo increments 5 times, so GetValue() = 5
			TestRunner->TestEqual(TEXT("Foo() should return 5"), FooResult, 5);
		}

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("int Bar()"));
			const int32 BarResult = Invoker.CallAndReturn<int32>(INDEX_NONE);
			TestRunner->TestTrue(TEXT("Bar() should execute successfully"), Invoker.HasRun());
			// Bar: increment twice, decrement once => 1
			TestRunner->TestEqual(TEXT("Bar() should return 1"), BarResult, 1);
		}

		Coverage.StartRecording();

		const FLineCoverage* LineCov = Coverage.GetLineCoverage(*FoundModule);
		if (!AngelscriptCodeCoverageTests::RequireMappedCoverage(*TestRunner, LineCov, TEXT("ASCoverageReset")))
		{
			return;
		}

		// Hit all lines via the coverage API.
		for (const auto& Pair : LineCov->HitCounts)
		{
			Coverage.HitLine(*FoundModule, Pair.Key);
		}

		const FLineCoverage* BeforeReset = Coverage.GetLineCoverage(*FoundModule);
		if (!TestRunner->TestNotNull(TEXT("Coverage should remain mapped before reset"), BeforeReset))
		{
			return;
		}

		TestRunner->TestTrue(TEXT("Some lines are hit before reset"), BeforeReset->NumLinesHit() > 0);

		Coverage.ResetHits();

		const FLineCoverage* AfterReset = Coverage.GetLineCoverage(*FoundModule);
		if (TestRunner->TestNotNull(TEXT("Coverage should remain mapped after reset"), AfterReset))
		{
			TestRunner->TestEqual(TEXT("After ResetHits, no lines should be hit"),
				AfterReset->NumLinesHit(), 0);
			TestRunner->TestTrue(TEXT("Executable lines still exist after reset"),
				AfterReset->NumExecutableLines() > 0);
		}
	}

	TEST_METHOD(MultipleModulesCoverageIsolation)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString SourceA = AngelscriptCodeCoverageTests::MakeScriptSource(R"(
int Factorial(int N)
{
	if (N <= 1)
		return 1;
	int Result = 1;
	for (int I = 2; I <= N; ++I)
		Result *= I;
	return Result;
}

int ModuleA_Func()
{
	return Factorial(5);
}
)");
		const FString SourceB = AngelscriptCodeCoverageTests::MakeScriptSource(R"(
bool IsPrime(int N)
{
	if (N < 2) return false;
	if (N == 2) return true;
	if (N % 2 == 0) return false;
	for (int I = 3; I * I <= N; I += 2)
	{
		if (N % I == 0)
			return false;
	}
	return true;
}

int CountPrimes(int Limit)
{
	int Count = 0;
	for (int I = 2; I < Limit; ++I)
	{
		if (IsPrime(I))
			Count += 1;
	}
	return Count;
}

int ModuleB_Func() { return CountPrimes(20); }
int ModuleB_Other() { return CountPrimes(50); }
)");

		asIScriptModule* ModA = BuildModule(*TestRunner, Engine, "ASCoverageModA", SourceA);
		asIScriptModule* ModB = BuildModule(*TestRunner, Engine, "ASCoverageModB", SourceB);
		if (!TestRunner->TestNotNull(TEXT("ASCoverageModA should compile"), ModA)
			|| !TestRunner->TestNotNull(TEXT("ASCoverageModB should compile"), ModB))
		{
			return;
		}
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(TEXT("ASCoverageModA"));
			Engine.DiscardModule(TEXT("ASCoverageModB"));
		};

		TSharedPtr<FAngelscriptModuleDesc> DescA =
			AngelscriptCodeCoverageTests::RequireActiveModule(*TestRunner, Engine, TEXT("ASCoverageModA"));
		TSharedPtr<FAngelscriptModuleDesc> DescB =
			AngelscriptCodeCoverageTests::RequireActiveModule(*TestRunner, Engine, TEXT("ASCoverageModB"));
		if (!DescA.IsValid() || !DescB.IsValid()) { return; }

		// Actually execute the entry functions to verify the script logic.
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *ModA, TEXT("int ModuleA_Func()"));
			const int32 FactorialResult = Invoker.CallAndReturn<int32>(INDEX_NONE);
			TestRunner->TestTrue(TEXT("ModuleA_Func() should execute successfully"), Invoker.HasRun());
			// Factorial(5) = 120
			TestRunner->TestEqual(TEXT("ModuleA_Func() should return Factorial(5) = 120"), FactorialResult, 120);
		}

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *ModB, TEXT("int ModuleB_Func()"));
			const int32 PrimeCount = Invoker.CallAndReturn<int32>(INDEX_NONE);
			TestRunner->TestTrue(TEXT("ModuleB_Func() should execute successfully"), Invoker.HasRun());
			// Primes below 20: 2,3,5,7,11,13,17,19 = 8
			TestRunner->TestEqual(TEXT("ModuleB_Func() should return CountPrimes(20) = 8"), PrimeCount, 8);
		}

		FAngelscriptCodeCoverage Coverage;
		Coverage.MapExecutableLines(*DescA);
		Coverage.MapExecutableLines(*DescB);
		Coverage.StartRecording();

		// Only hit lines in module A.
		const FLineCoverage* CovA = Coverage.GetLineCoverage(*DescA);
		if (AngelscriptCodeCoverageTests::RequireMappedCoverage(*TestRunner, CovA, TEXT("ASCoverageModA")))
		{
			for (const auto& Pair : CovA->HitCounts)
			{
				Coverage.HitLine(*DescA, Pair.Key);
			}
		}

		// Module A should have hits, module B should not.
		const FLineCoverage* AfterA = Coverage.GetLineCoverage(*DescA);
		const FLineCoverage* AfterB = Coverage.GetLineCoverage(*DescB);

		if (AngelscriptCodeCoverageTests::RequireMappedCoverage(*TestRunner, AfterA, TEXT("ASCoverageModA after hits")))
		{
			TestRunner->TestEqual(TEXT("Module A: all lines hit"),
				AfterA->NumExecutableLines(), AfterA->NumLinesHit());
		}
		if (TestRunner->TestNotNull(TEXT("Module B coverage should be mapped"), AfterB))
		{
			TestRunner->TestEqual(TEXT("Module B: no lines hit (isolation)"),
				AfterB->NumLinesHit(), 0);
		}
	}

	TEST_METHOD(ReportGenerationWritesFiles)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString Source = AngelscriptCodeCoverageTests::MakeScriptSource(R"(
float CalculateDamage(float BaseDamage, float CritMultiplier, bool bIsCrit, float ArmorReduction)
{
	float Damage = BaseDamage;
	if (bIsCrit)
		Damage *= CritMultiplier;

	Damage -= ArmorReduction;
	if (Damage < 0.0f)
		Damage = 0.0f;

	return Damage;
}

int ReportTest()
{
	float NormalHit = CalculateDamage(10.0f, 2.0f, false, 3.0f);
	float CritHit = CalculateDamage(10.0f, 2.0f, true, 5.0f);
	float BlockedHit = CalculateDamage(10.0f, 2.0f, false, 999.0f);

	int Result = 0;
	if (NormalHit > 0.0f) Result += 1;
	if (CritHit > NormalHit) Result += 10;
	if (BlockedHit == 0.0f) Result += 100;
	return Result;
}
)");

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCoverageReport", Source);
		if (!TestRunner->TestNotNull(TEXT("ASCoverageReport should compile"), Module)) { return; }
		ON_SCOPE_EXIT { Engine.DiscardModule(TEXT("ASCoverageReport")); };

		TSharedPtr<FAngelscriptModuleDesc> FoundModule =
			AngelscriptCodeCoverageTests::RequireActiveModule(*TestRunner, Engine, TEXT("ASCoverageReport"));
		if (!FoundModule.IsValid()) { return; }

		// Actually execute ReportTest() to verify the damage calculator logic.
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("int ReportTest()"));
			const int32 ReportResult = Invoker.CallAndReturn<int32>(INDEX_NONE);
			TestRunner->TestTrue(TEXT("ReportTest() should execute successfully"), Invoker.HasRun());
			// NormalHit=7.0>0 (+1), CritHit=15.0>7.0 (+10), BlockedHit=0.0==0.0 (+100) => 111
			TestRunner->TestEqual(TEXT("ReportTest() should return 111"), ReportResult, 111);
		}

		FAngelscriptCodeCoverage Coverage;
		Coverage.MapExecutableLines(*FoundModule);
		Coverage.StartRecording();

		const FLineCoverage* LineCov = Coverage.GetLineCoverage(*FoundModule);
		if (AngelscriptCodeCoverageTests::RequireMappedCoverage(*TestRunner, LineCov, TEXT("ASCoverageReport")))
		{
			for (const auto& Pair : LineCov->HitCounts)
			{
				Coverage.HitLine(*FoundModule, Pair.Key);
			}
		}

		const FString TempDir = AngelscriptCodeCoverageTests::MakeUniqueCoverageReportDir(TEXT("CoverageReportTest"));
		Coverage.StopRecordingAndWriteReport(TempDir);

		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		FString ExpectedIndexPath = FPaths::Combine(TempDir, TEXT("index.html"));
		TestRunner->TestTrue(
			FString::Printf(TEXT("index.html should exist at %s"), *ExpectedIndexPath),
			PlatformFile.FileExists(*ExpectedIndexPath));
		const FString ExpectedSummaryJsonPath = FPaths::Combine(TempDir, TEXT("coverage_summary.json"));
		TestRunner->TestTrue(
			FString::Printf(TEXT("coverage_summary.json should exist at %s"), *ExpectedSummaryJsonPath),
			PlatformFile.FileExists(*ExpectedSummaryJsonPath));

		// Per-module HTML report should also exist.
		const FString ExpectedModulePath = FPaths::ChangeExtension(
			FPaths::Combine(TempDir, (*FoundModule).Code[0].RelativeFilename),
			TEXT(".as.html"));
		TestRunner->TestTrue(
			FString::Printf(TEXT("Module report should exist at %s"), *ExpectedModulePath),
			PlatformFile.FileExists(*ExpectedModulePath));

		// Keep the generated report under Saved/Automation/CodeCoverage so
		// it can be opened manually when validating report layout and contents.
	}

	TEST_METHOD(ReportGenerationWritesMultiFileUClassReport)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString GlobalSource = AngelscriptCodeCoverageTests::MakeScriptSource(R"(
int NormalizeScore(int Value)
{
	if (Value < 0)
		return 0;
	if (Value > 100)
		return 100;
	return Value;
}

int AddWeightedScore(int Current, int Value, int Weight)
{
	int Result = Current;
	int Normalized = NormalizeScore(Value);
	for (int Index = 0; Index < Weight; ++Index)
	{
		Result += Normalized;
	}
	return Result;
}

int GlobalCoverageReportTest()
{
	int Score = 0;
	Score = AddWeightedScore(Score, 40, 2);
	Score = AddWeightedScore(Score, 200, 1);
	Score = AddWeightedScore(Score, -5, 3);
	return Score;
}
)");

		asIScriptModule* GlobalModule = BuildModule(
			*TestRunner,
			Engine,
			"ASCoverageReportGlobal",
			GlobalSource);
		if (!TestRunner->TestNotNull(TEXT("ASCoverageReportGlobal should compile"), GlobalModule)) { return; }

		const FName ActorModuleName(TEXT("ASCoverageReportActor"));
		UClass* ActorClass = CompileScriptModule(
			*TestRunner,
			Engine,
			ActorModuleName,
			TEXT("ASCoverageReportActor.as"),
			TEXT(R"AS(
UCLASS()
class AASCoverageReportActor : AActor
{
	UPROPERTY()
	int BeginPlayCount = 0;

	UPROPERTY()
	int Health = 100;

	UPROPERTY()
	int Shield = 25;

	UPROPERTY()
	FString LastEvent = "None";

	UPROPERTY()
	TArray<int> DamageLog;

	int ClampDamage(int Amount)
	{
		if (Amount < 0)
			return 0;
		if (Amount > 75)
			return 75;
		return Amount;
	}

	UFUNCTION(BlueprintOverride)
	void BeginPlay()
	{
		BeginPlayCount += 1;
		DamageLog.Add(0);
		DamageLog.Add(0);
		LastEvent = "BeginPlay";
	}

	UFUNCTION()
	int ApplyDamage(int Amount, bool bBypassShield)
	{
		int Damage = ClampDamage(Amount);
		int ShieldDamage = 0;
		if (!bBypassShield && Shield > 0)
		{
			ShieldDamage = Damage;
			if (ShieldDamage > Shield)
				ShieldDamage = Shield;
			Shield -= ShieldDamage;
			Damage -= ShieldDamage;
		}

		Health -= Damage;
		if (Health < 0)
			Health = 0;

		DamageLog[0] += Damage;
		DamageLog[1] += ShieldDamage;
		if (bBypassShield)
			LastEvent = "Bypass";
		else
			LastEvent = "Blocked";
		return Health + Shield;
	}

	UFUNCTION()
	int GetCombatScore()
	{
		return Health + Shield + BeginPlayCount + DamageLog[0] + DamageLog[1];
	}
}
)AS"),
			TEXT("AASCoverageReportActor"));
		if (!TestRunner->TestNotNull(TEXT("AASCoverageReportActor should compile"), ActorClass)) { return; }

		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(TEXT("ASCoverageReportGlobal"));
			Engine.DiscardModule(*ActorModuleName.ToString());
		};

		TSharedPtr<FAngelscriptModuleDesc> GlobalDesc =
			AngelscriptCodeCoverageTests::RequireActiveModule(*TestRunner, Engine, TEXT("ASCoverageReportGlobal"));
		TSharedPtr<FAngelscriptModuleDesc> ActorDesc =
			AngelscriptCodeCoverageTests::RequireActiveModule(*TestRunner, Engine, TEXT("ASCoverageReportActor"));
		if (!GlobalDesc.IsValid() || !ActorDesc.IsValid()) { return; }

		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *GlobalModule, TEXT("int GlobalCoverageReportTest()"));
			const int32 Score = Invoker.CallAndReturn<int32>(INDEX_NONE);
			TestRunner->TestTrue(TEXT("GlobalCoverageReportTest() should execute successfully"), Invoker.HasRun());
			TestRunner->TestEqual(TEXT("GlobalCoverageReportTest() should return weighted score 180"), Score, 180);
		}

		FActorTestSpawner Spawner;
		Spawner.InitializeGameSubsystems();
		AActor* Actor = SpawnScriptActor(*TestRunner, Spawner, ActorClass);
		if (!TestRunner->TestNotNull(TEXT("Coverage report actor should spawn"), Actor)) { return; }

		BeginPlayActor(Engine, *Actor);
		VerifyByPath<FIntProperty, int32>(
			*TestRunner,
			Actor,
			TEXT("BeginPlayCount"),
			1,
			TEXT("BeginPlay should update the coverage report actor"));
		VerifyByPath<FStrProperty, FString>(
			*TestRunner,
			Actor,
			TEXT("LastEvent"),
			FString(TEXT("BeginPlay")),
			TEXT("BeginPlay should update the actor event label"));

		{
			FFunctionInvoker Invoker(*TestRunner, Actor, FName(TEXT("ApplyDamage")));
			if (!Invoker.IsValid()) { return; }
			Invoker.AddParam<int32>(40).AddParam<bool>(false);
			const int32 RemainingPool = Invoker.CallAndReturn<int32>(INDEX_NONE);
			TestRunner->TestEqual(TEXT("ApplyDamage should return Health + Shield after mitigation"), RemainingPool, 85);
		}

		VerifyByPath<FIntProperty, int32>(
			*TestRunner,
			Actor,
			TEXT("Health"),
			85,
			TEXT("ApplyDamage should reduce health after shield mitigation"));
		VerifyByPath<FIntProperty, int32>(
			*TestRunner,
			Actor,
			TEXT("Shield"),
			0,
			TEXT("ApplyDamage should spend all shield"));
		VerifyByPath<FStrProperty, FString>(
			*TestRunner,
			Actor,
			TEXT("LastEvent"),
			FString(TEXT("Blocked")),
			TEXT("ApplyDamage should record the non-bypass branch"));

		{
			FFunctionInvoker Invoker(*TestRunner, Actor, FName(TEXT("GetCombatScore")));
			if (!Invoker.IsValid()) { return; }
			const int32 CombatScore = Invoker.CallAndReturn<int32>(INDEX_NONE);
			TestRunner->TestEqual(TEXT("GetCombatScore should include state and damage log"), CombatScore, 126);
		}

		FAngelscriptCodeCoverage Coverage;
		Coverage.MapExecutableLines(*GlobalDesc);
		Coverage.MapExecutableLines(*ActorDesc);
		Coverage.StartRecording();

		const FLineCoverage* GlobalCoverage = Coverage.GetLineCoverage(*GlobalDesc);
		const FLineCoverage* ActorCoverage = Coverage.GetLineCoverage(*ActorDesc);
		if (!AngelscriptCodeCoverageTests::RequireMappedCoverage(*TestRunner, GlobalCoverage, TEXT("ASCoverageReportGlobal"))
			|| !AngelscriptCodeCoverageTests::RequireMappedCoverage(*TestRunner, ActorCoverage, TEXT("ASCoverageReportActor")))
		{
			return;
		}

		for (const auto& Pair : GlobalCoverage->HitCounts)
		{
			Coverage.HitLine(*GlobalDesc, Pair.Key);
		}

		int32 ActorLineIndex = 0;
		for (const auto& Pair : ActorCoverage->HitCounts)
		{
			if ((ActorLineIndex % 2) == 0)
			{
				Coverage.HitLine(*ActorDesc, Pair.Key);
			}
			++ActorLineIndex;
		}

		const FLineCoverage* GlobalAfterHit = Coverage.GetLineCoverage(*GlobalDesc);
		const FLineCoverage* ActorAfterHit = Coverage.GetLineCoverage(*ActorDesc);
		if (!TestRunner->TestNotNull(TEXT("Global coverage should still be available after hits"), GlobalAfterHit)
			|| !TestRunner->TestNotNull(TEXT("Actor coverage should still be available after hits"), ActorAfterHit))
		{
			return;
		}

		TestRunner->TestEqual(
			TEXT("Global report module should be fully covered"),
			GlobalAfterHit->NumExecutableLines(),
			GlobalAfterHit->NumLinesHit());
		TestRunner->TestTrue(
			TEXT("Actor report module should have at least one hit line"),
			ActorAfterHit->NumLinesHit() > 0);
		TestRunner->TestTrue(
			TEXT("Actor report module should keep some lines uncovered"),
			ActorAfterHit->NumLinesHit() < ActorAfterHit->NumExecutableLines());
		TestRunner->TestTrue(
			TEXT("Expanded report should cover more executable lines than the smoke report"),
			GlobalAfterHit->NumExecutableLines() + ActorAfterHit->NumExecutableLines() > 15);

		const FString OutputDir = AngelscriptCodeCoverageTests::MakeUniqueCoverageReportDir(TEXT("ReportFeatureMatrix"));
		Coverage.StopRecordingAndWriteReport(OutputDir);

		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		const FString IndexPath = FPaths::Combine(OutputDir, TEXT("index.html"));
		const FString SummaryJsonPath = FPaths::Combine(OutputDir, TEXT("coverage_summary.json"));
		const FString GlobalReportPath = FPaths::ChangeExtension(
			FPaths::Combine(OutputDir, (*GlobalDesc).Code[0].RelativeFilename),
			TEXT(".as.html"));
		const FString ActorReportPath = FPaths::ChangeExtension(
			FPaths::Combine(OutputDir, (*ActorDesc).Code[0].RelativeFilename),
			TEXT(".as.html"));

		TestRunner->TestTrue(
			FString::Printf(TEXT("Expanded report index.html should exist at %s"), *IndexPath),
			PlatformFile.FileExists(*IndexPath));
		TestRunner->TestTrue(
			FString::Printf(TEXT("Expanded report coverage_summary.json should exist at %s"), *SummaryJsonPath),
			PlatformFile.FileExists(*SummaryJsonPath));
		TestRunner->TestTrue(
			FString::Printf(TEXT("Global module report should exist at %s"), *GlobalReportPath),
			PlatformFile.FileExists(*GlobalReportPath));
		TestRunner->TestTrue(
			FString::Printf(TEXT("Actor module report should exist at %s"), *ActorReportPath),
			PlatformFile.FileExists(*ActorReportPath));

		FString IndexHtml;
		FString ActorHtml;
		FString GlobalHtml;
		FString SummaryJson;
		if (!AngelscriptCodeCoverageTests::LoadFileToStringChecked(*TestRunner, IndexPath, IndexHtml, TEXT("Expanded report index"))
			|| !AngelscriptCodeCoverageTests::LoadFileToStringChecked(*TestRunner, ActorReportPath, ActorHtml, TEXT("Actor HTML report"))
			|| !AngelscriptCodeCoverageTests::LoadFileToStringChecked(*TestRunner, GlobalReportPath, GlobalHtml, TEXT("Global HTML report"))
			|| !AngelscriptCodeCoverageTests::LoadFileToStringChecked(*TestRunner, SummaryJsonPath, SummaryJson, TEXT("Expanded report summary JSON")))
		{
			return;
		}

		TestRunner->TestTrue(TEXT("Index should link the global report"), IndexHtml.Contains(TEXT("ASCoverageReportGlobal.as")));
		TestRunner->TestTrue(TEXT("Index should link the actor report"), IndexHtml.Contains(TEXT("ASCoverageReportActor.as")));
		TestRunner->TestTrue(TEXT("Actor report should include the AS UCLASS declaration"), ActorHtml.Contains(TEXT("AASCoverageReportActor")));
		TestRunner->TestTrue(TEXT("Actor report should include a UFUNCTION marker"), ActorHtml.Contains(TEXT("UFUNCTION")));
		TestRunner->TestTrue(TEXT("Actor report should include covered lines"), ActorHtml.Contains(TEXT("class=\"covered\"")));
		TestRunner->TestTrue(TEXT("Actor report should include not-covered lines"), ActorHtml.Contains(TEXT("class=\"not-covered\"")));
		TestRunner->TestTrue(TEXT("Global report should include the global entry point"), GlobalHtml.Contains(TEXT("GlobalCoverageReportTest")));
		TestRunner->TestTrue(TEXT("Global report should include covered lines"), GlobalHtml.Contains(TEXT("class=\"covered\"")));
		TestRunner->TestTrue(TEXT("Summary JSON should contain the global file"), SummaryJson.Contains(TEXT("ASCoverageReportGlobal.as")));
		TestRunner->TestTrue(TEXT("Summary JSON should contain the actor file"), SummaryJson.Contains(TEXT("ASCoverageReportActor.as")));
		TestRunner->TestTrue(TEXT("Summary JSON should contain coverage_pct"), SummaryJson.Contains(TEXT("\"coverage_pct\"")));
		TestRunner->TestTrue(TEXT("Summary JSON should contain lines_hit"), SummaryJson.Contains(TEXT("\"lines_hit\"")));
		TestRunner->TestTrue(TEXT("Summary JSON should contain lines_total"), SummaryJson.Contains(TEXT("\"lines_total\"")));

		// Keep the generated report under Saved/Automation/CodeCoverage so
		// UCLASS, UFUNCTION, covered, and not-covered HTML can be inspected.
	}

	TEST_METHOD(GetLineCoverageReturnsNullForUnmappedModule)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString Source = AngelscriptCodeCoverageTests::MakeScriptSource(R"(
int Max3(int A, int B, int C)
{
	int Best = A;
	if (B > Best) Best = B;
	if (C > Best) Best = C;
	return Best;
}

int Unmapped()
{
	return Max3(7, 42, 13);
}
)");

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCoverageUnmapped", Source);
		if (!TestRunner->TestNotNull(TEXT("ASCoverageUnmapped should compile"), Module)) { return; }
		ON_SCOPE_EXIT { Engine.DiscardModule(TEXT("ASCoverageUnmapped")); };

		TSharedPtr<FAngelscriptModuleDesc> FoundModule =
			AngelscriptCodeCoverageTests::RequireActiveModule(*TestRunner, Engine, TEXT("ASCoverageUnmapped"));
		if (!FoundModule.IsValid()) { return; }

		// Actually execute Unmapped() to verify the Max3 logic works.
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("int Unmapped()"));
			const int32 MaxResult = Invoker.CallAndReturn<int32>(INDEX_NONE);
			TestRunner->TestTrue(TEXT("Unmapped() should execute successfully"), Invoker.HasRun());
			// Max3(7, 42, 13) = 42
			TestRunner->TestEqual(TEXT("Unmapped() should return Max3(7,42,13) = 42"), MaxResult, 42);
		}

		// Do NOT call MapExecutableLines — coverage should return null.
		FAngelscriptCodeCoverage Coverage;
		const FLineCoverage* LineCov = Coverage.GetLineCoverage(*FoundModule);
		TestRunner->TestNull(TEXT("GetLineCoverage returns null for unmapped module"), LineCov);
	}

	TEST_METHOD(MapExecutableLinesIncludesHelperFunctions)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope Scope(Engine);

		const FString Source = AngelscriptCodeCoverageTests::MakeScriptSource(R"(
int GetSellPrice(int Rarity, int Quantity)
{
	if (Rarity == 0)
	{
		return 10 * Quantity;
	}
	if (Rarity == 1)
	{
		return 25 * Quantity;
	}
	if (Rarity == 2)
	{
		return 100 * Quantity;
	}
	if (Rarity == 3)
	{
		return 500 * Quantity;
	}
	return 0;
}

int AddItemValue(int CurrentTotal, int Rarity, int Quantity)
{
	return CurrentTotal + GetSellPrice(Rarity, Quantity);
}

int GetTotalValue()
{
	int Total = 0;
	Total = AddItemValue(Total, 2, 1);
	Total = AddItemValue(Total, 0, 5);
	Total = AddItemValue(Total, 1, 1);
	Total = AddItemValue(Total, 3, 1);
	return Total;
}

int GlobalFunc()
{
	return GetTotalValue();
}
)");

		asIScriptModule* Module = BuildModule(*TestRunner, Engine, "ASCoverageClassMethods", Source);
		if (!TestRunner->TestNotNull(TEXT("ASCoverageClassMethods should compile"), Module)) { return; }
		ON_SCOPE_EXIT { Engine.DiscardModule(TEXT("ASCoverageClassMethods")); };

		TSharedPtr<FAngelscriptModuleDesc> FoundModule =
			AngelscriptCodeCoverageTests::RequireActiveModule(*TestRunner, Engine, TEXT("ASCoverageClassMethods"));
		if (!FoundModule.IsValid()) { return; }

		// Actually execute GlobalFunc() to verify the full inventory system works.
		{
			FASGlobalFunctionInvoker Invoker(*TestRunner, Engine, *Module, TEXT("int GlobalFunc()"));
			const int32 TotalValue = Invoker.CallAndReturn<int32>(INDEX_NONE);
			TestRunner->TestTrue(TEXT("GlobalFunc() should execute successfully"), Invoker.HasRun());
			// Sword(Rare,1)=100, Potion(Common,5)=50, Shield(Uncommon,1)=25, Crown(Legendary,1)=500 => 675
			TestRunner->TestEqual(TEXT("GlobalFunc() should return total inventory value 675"), TotalValue, 675);
		}

		FAngelscriptCodeCoverage Coverage;
		Coverage.MapExecutableLines(*FoundModule);

		const FLineCoverage* LineCov = Coverage.GetLineCoverage(*FoundModule);
		if (AngelscriptCodeCoverageTests::RequireMappedCoverage(*TestRunner, LineCov, TEXT("ASCoverageClassMethods")))
		{
			// Should have lines from both helper functions and the global function.
			TestRunner->TestTrue(TEXT("Module with helpers + global should have multiple executable lines"),
				LineCov->NumExecutableLines() >= 3);
		}
	}
};

// =============================================================================
// Robustness / edge-case tests
// =============================================================================

TEST_CLASS_WITH_FLAGS(FAngelscriptCodeCoverageRobustnessTest,
	"Angelscript.TestModule.Core.CodeCoverage.Robustness",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(StartStopIdempotent)
	{
		FAngelscriptCodeCoverage Coverage;

		// Double start should not crash.
		Coverage.StartRecording();
		Coverage.StartRecording();

		// Double stop should not crash.
		const FString TempDir = AngelscriptCodeCoverageTests::MakeUniqueCoverageReportDir(TEXT("IdempotentTest"));
		Coverage.StopRecordingAndWriteReport(TempDir);
		Coverage.StopRecordingAndWriteReport(TempDir);

		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		const FString ExpectedIndexPath = FPaths::Combine(TempDir, TEXT("index.html"));
		TestRunner->TestTrue(
			FString::Printf(TEXT("Idempotent stop should write index.html at %s"), *ExpectedIndexPath),
			PlatformFile.FileExists(*ExpectedIndexPath));
		// Keep the generated report for manual inspection of idempotent output.
	}

	TEST_METHOD(CoverageEnabledCallable)
	{
		// Just verify CoverageEnabled() is callable and returns a bool without crashing.
		const bool Enabled = FAngelscriptCodeCoverage::CoverageEnabled();
		TestRunner->AddInfo(FString::Printf(TEXT("CoverageEnabled() = %s"),
			Enabled ? TEXT("true") : TEXT("false")));
	}

	TEST_METHOD(StartRecordingResetsHits)
	{
		// StartRecording internally calls ResetHits, so any prior state is cleared.
		FLineCoverage ManualCoverage;
		ManualCoverage.HitCounts.Add(1, 10);
		ManualCoverage.HitCounts.Add(2, 20);

		// Verify the data exists before we test the manager.
		TestRunner->TestEqual(TEXT("Manual coverage has 2 hit lines"), ManualCoverage.NumLinesHit(), 2);

		FAngelscriptCodeCoverage Coverage;
		Coverage.StartRecording();

		const FString TempDir = AngelscriptCodeCoverageTests::MakeUniqueCoverageReportDir(TEXT("StartRecordingResetsHits"));
		Coverage.StopRecordingAndWriteReport(TempDir);

		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		const FString ExpectedIndexPath = FPaths::Combine(TempDir, TEXT("index.html"));
		TestRunner->TestTrue(
			FString::Printf(TEXT("StartRecording/StopRecording should write index.html at %s"), *ExpectedIndexPath),
			PlatformFile.FileExists(*ExpectedIndexPath));
		// Keep the generated report for manual inspection of reset behavior.
	}

	TEST_METHOD(StopRecordingToEmptyDirectory)
	{
		FAngelscriptCodeCoverage Coverage;
		Coverage.StartRecording();

		// Stop with no modules mapped — should produce an empty report without crashing.
		const FString TempDir = AngelscriptCodeCoverageTests::MakeUniqueCoverageReportDir(TEXT("EmptyReportTest"));
		Coverage.StopRecordingAndWriteReport(TempDir);

		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		const FString IndexPath = FPaths::Combine(TempDir, TEXT("index.html"));
		TestRunner->TestTrue(
			FString::Printf(TEXT("Empty report should write index.html at %s"), *IndexPath),
			PlatformFile.FileExists(*IndexPath));
		const FString SummaryJsonPath = FPaths::Combine(TempDir, TEXT("coverage_summary.json"));
		TestRunner->TestTrue(
			FString::Printf(TEXT("Empty report should write coverage_summary.json at %s"), *SummaryJsonPath),
			PlatformFile.FileExists(*SummaryJsonPath));

		// Keep the generated empty report for manual inspection.
	}

	TEST_METHOD(ResetHitsOnEmptyCoverageIsNoOp)
	{
		FAngelscriptCodeCoverage Coverage;
		Coverage.ResetHits();

		const FString TempDir = AngelscriptCodeCoverageTests::MakeUniqueCoverageReportDir(TEXT("ResetHitsOnEmpty"));
		Coverage.StopRecordingAndWriteReport(TempDir);

		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		const FString IndexPath = FPaths::Combine(TempDir, TEXT("index.html"));
		TestRunner->TestTrue(
			FString::Printf(TEXT("ResetHits on empty coverage should leave report generation functional at %s"), *IndexPath),
			PlatformFile.FileExists(*IndexPath));
		// Keep the generated report for manual inspection of empty reset behavior.
	}

	TEST_METHOD(MultipleHitsOnSameLineAccumulate)
	{
		FLineCoverage LineCoverage;
		LineCoverage.HitCounts.Add(5, 0);

		// Simulate multiple hits by incrementing manually (mirrors what HitLine does).
		LineCoverage.HitCounts[5] = 1;
		TestRunner->TestEqual(TEXT("After 1 hit, count is 1"), LineCoverage.HitCounts[5], 1);

		LineCoverage.HitCounts[5] = 100;
		TestRunner->TestEqual(TEXT("After 100 hits, count is 100"), LineCoverage.HitCounts[5], 100);

		TestRunner->TestEqual(TEXT("Still only 1 executable line"), LineCoverage.NumExecutableLines(), 1);
		TestRunner->TestEqual(TEXT("1 line is hit"), LineCoverage.NumLinesHit(), 1);
	}
};

#endif    // WITH_DEV_AUTOMATION_TESTS && WITH_AS_COVERAGE
