#include "Dump/AngelscriptStateDiff.h"
#include "Dump/AngelscriptStateDump.h"
#include "Dump/AngelscriptStateSnapshot.h"

#include "AngelscriptTestMacros.h"
#include "AngelscriptTestModuleScope.h"
#include "CQTest.h"

#if WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptStateDumpDiffTests,
	"Angelscript.TestModule.Dump.StateDiff",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static bool ContainsRow(
		const FAngelscriptStateSnapshot& Snapshot,
		const FString& Category,
		const FString& Identity,
		const FString& Field)
	{
		return Snapshot.Rows.ContainsByPredicate(
			[&Category, &Identity, &Field](const FAngelscriptStateSnapshotRow& Row)
			{
				return Row.Category == Category && Row.Identity == Identity && Row.Field == Field;
			});
	}

	static bool ContainsDiff(
		const FAngelscriptStateDiff& Diff,
		const FString& Category,
		const FString& Identity,
		const FString& Field,
		EAngelscriptStateDiffChangeType ChangeType)
	{
		return Diff.Rows.ContainsByPredicate(
			[&Category, &Identity, &Field, ChangeType](const FAngelscriptStateDiffRow& Row)
			{
				return Row.Category == Category
					&& Row.Identity == Identity
					&& Row.Field == Field
					&& Row.ChangeType == ChangeType;
			});
	}

public:
	BEFORE_ALL()
	{
		ASTEST_CREATE_ENGINE();
	}

	AFTER_ALL()
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		ASTEST_RESET_ENGINE(Engine);
	}

	TEST_METHOD(CaptureSnapshotProducesDeterministicRows)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		const FAngelscriptStateSnapshot Snapshot = FAngelscriptStateDump::CaptureSnapshot(Engine);
		ASSERT_THAT(IsTrue(Snapshot.Rows.Num() > 0, TEXT("State snapshot should contain diagnostic rows")));
		ASSERT_THAT(IsFalse(Snapshot.HasDuplicateKeys(), TEXT("State snapshot should not contain duplicate comparable row keys")));
		ASSERT_THAT(IsTrue(Snapshot.AreRowsSorted(), TEXT("State snapshot rows should be sorted deterministically")));

		ASSERT_THAT(IsTrue(ContainsRow(Snapshot, TEXT("EngineMember"), TEXT("Lifecycle"), TEXT("bIsInitialCompileFinished")), TEXT("Snapshot should include engine lifecycle rows")));
		ASSERT_THAT(IsTrue(ContainsRow(Snapshot, TEXT("EngineCollection"), TEXT("ActiveModules"), TEXT("Count")), TEXT("Snapshot should include active module collection count")));
		ASSERT_THAT(IsTrue(ContainsRow(Snapshot, TEXT("AsEngineInternal"), TEXT("ScriptEngine"), TEXT("ModuleCount")), TEXT("Snapshot should include AS engine module count")));
	}

	TEST_METHOD(DiffSnapshotsReportsAddedRemovedAndChangedRows)
	{
		FAngelscriptStateSnapshot Before;
		Before.AddRow(TEXT("Synthetic"), TEXT("Scalar"), TEXT("Value"), TEXT("1"), TEXT("Integer"), TEXT("Test"));
		Before.AddRow(TEXT("Synthetic"), TEXT("Removed"), TEXT("Value"), TEXT("Before"), TEXT("String"), TEXT("Test"));
		Before.SortRows();

		FAngelscriptStateSnapshot After;
		After.AddRow(TEXT("Synthetic"), TEXT("Scalar"), TEXT("Value"), TEXT("2"), TEXT("Integer"), TEXT("Test"));
		After.AddRow(TEXT("Synthetic"), TEXT("Added"), TEXT("Value"), TEXT("After"), TEXT("String"), TEXT("Test"));
		After.SortRows();

		const FAngelscriptStateDiff Diff = FAngelscriptStateDump::DiffSnapshots(Before, After);
		ASSERT_THAT(IsTrue(ContainsDiff(Diff, TEXT("Synthetic"), TEXT("Scalar"), TEXT("Value"), EAngelscriptStateDiffChangeType::Changed), TEXT("Diff should report changed rows")));
		ASSERT_THAT(IsTrue(ContainsDiff(Diff, TEXT("Synthetic"), TEXT("Removed"), TEXT("Value"), EAngelscriptStateDiffChangeType::Removed), TEXT("Diff should report removed rows")));
		ASSERT_THAT(IsTrue(ContainsDiff(Diff, TEXT("Synthetic"), TEXT("Added"), TEXT("Value"), EAngelscriptStateDiffChangeType::Added), TEXT("Diff should report added rows")));
	}

	TEST_METHOD(CompileImpactAppearsInStateDiff)
	{
		FAngelscriptEngine& Engine = ASTEST_GET_ENGINE();
		FAngelscriptEngineScope EngineScope(Engine);

		const FAngelscriptStateSnapshot Before = FAngelscriptStateDump::CaptureSnapshot(Engine);

		const FString ScriptSource = ASTEST_AS(R"AS(
			int DumpDiffEntry()
			{
				return 31;
			}
			)AS");

		FScopedAngelscriptModule ModuleScope(*TestRunner, Engine, TEXT("ASStateDumpDiff_CompileImpact"), ScriptSource);
		ASSERT_THAT(IsTrue(ModuleScope.IsValid(), TEXT("Compile impact module should compile")));

		const FAngelscriptStateSnapshot After = FAngelscriptStateDump::CaptureSnapshot(Engine);
		const FAngelscriptStateDiff Diff = FAngelscriptStateDump::DiffSnapshots(Before, After);

		ASSERT_THAT(IsTrue(ContainsDiff(Diff, TEXT("EngineCollection"), TEXT("ActiveModules"), TEXT("Count"), EAngelscriptStateDiffChangeType::Changed), TEXT("Diff should report FAS active module count change")));
		ASSERT_THAT(IsTrue(ContainsDiff(Diff, TEXT("AsEngineInternal"), TEXT("ScriptEngine"), TEXT("ModuleCount"), EAngelscriptStateDiffChangeType::Changed), TEXT("Diff should report AS engine module count change")));
		ASSERT_THAT(IsTrue(ContainsRow(After, TEXT("AsModuleInternal"), TEXT("ASStateDumpDiff_CompileImpact"), TEXT("ScriptFunctionCount")), TEXT("Snapshot should include AS module internals for compiled module")));
		ASSERT_THAT(IsTrue(ContainsRow(After, TEXT("AsFunctionInternal"), TEXT("ASStateDumpDiff_CompileImpact::DumpDiffEntry"), TEXT("Declaration")), TEXT("Snapshot should include compiled function internals")));
	}
};

#endif
