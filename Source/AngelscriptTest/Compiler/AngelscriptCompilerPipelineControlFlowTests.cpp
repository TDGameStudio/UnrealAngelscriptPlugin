#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"

#include "Preprocessor/AngelscriptPreprocessor.h"

#include "HAL/FileManager.h"
#include "CQTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CompilerPipelineControlFlowTest
{
	static const FName ModuleName(TEXT("Tests.Compiler.RangeBasedForRewriteSupportsBlockAndSingleLine"));
	static const FString RelativeScriptPath(TEXT("Tests/Compiler/RangeBasedForRewriteSupportsBlockAndSingleLine.as"));

	FString GetFixtureRoot()
	{
		return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("CompilerControlFlowFixtures"));
	}

	FString WriteFixture(const FString& RelativePath, const FString& Contents)
	{
		const FString AbsolutePath = FPaths::Combine(GetFixtureRoot(), RelativePath);
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(AbsolutePath), true);
		FFileHelper::SaveStringToFile(Contents, *AbsolutePath);
		return AbsolutePath;
	}

	TArray<FString> CollectDiagnosticMessages(
		const FAngelscriptEngine& Engine,
		const FString& AbsoluteFilename,
		int32& OutErrorCount)
	{
		OutErrorCount = 0;

		const FAngelscriptEngine::FDiagnostics* Diagnostics = Engine.Diagnostics.Find(AbsoluteFilename);
		if (Diagnostics == nullptr)
		{
			return {};
		}

		TArray<FString> Messages;
		for (const FAngelscriptEngine::FDiagnostic& Diagnostic : Diagnostics->Diagnostics)
		{
			Messages.Add(Diagnostic.Message);
			if (Diagnostic.bIsError)
			{
				++OutErrorCount;
			}
		}

		return Messages;
	}

	int32 CountOccurrences(const FString& Haystack, const FString& Needle)
	{
		if (Needle.IsEmpty())
		{
			return 0;
		}

		int32 Count = 0;
		int32 SearchFrom = 0;
		while (true)
		{
			const int32 FoundAt = Haystack.Find(Needle, ESearchCase::CaseSensitive, ESearchDir::FromStart, SearchFrom);
			if (FoundAt == INDEX_NONE)
			{
				break;
			}

			++Count;
			SearchFrom = FoundAt + Needle.Len();
		}

		return Count;
	}
}

using namespace CompilerPipelineControlFlowTest;

TEST_CLASS_WITH_FLAGS(FCompilerPipelineControlFlowTests,
	"Angelscript.TestModule.Compiler.EndToEnd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(RangeBasedForRewriteSupportsBlockAndSingleLine)
	{


		const FString ScriptSource = TEXT(R"AS(
	int Entry()
	{
		TArray<int> Values;
		Values.Add(20);
		Values.Add(22);

		int BlockSum = 0;
		for (const int Value : Values)
		{
			BlockSum += Value;
		}

		int SingleLineSum = 0;
		for (const int Value : Values) SingleLineSum += Value;

		return BlockSum * 100 + SingleLineSum;
	}
	)AS");

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		const FString AbsoluteScriptPath = CompilerPipelineControlFlowTest::WriteFixture(
			CompilerPipelineControlFlowTest::RelativeScriptPath,
			ScriptSource);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*CompilerPipelineControlFlowTest::ModuleName.ToString());
			IFileManager::Get().Delete(*AbsoluteScriptPath, false, true);
		};

		Engine.ResetDiagnostics();

		FAngelscriptPreprocessor Preprocessor;
		Preprocessor.AddFile(CompilerPipelineControlFlowTest::RelativeScriptPath, AbsoluteScriptPath);

		const bool bPreprocessSucceeded = Preprocessor.Preprocess();
		const TArray<TSharedRef<FAngelscriptModuleDesc>> Modules = Preprocessor.GetModulesToCompile();

		int32 PreprocessErrorCount = 0;
		const TArray<FString> PreprocessMessages = CompilerPipelineControlFlowTest::CollectDiagnosticMessages(
			Engine,
			AbsoluteScriptPath,
			PreprocessErrorCount);
		const FString ProcessedCode = (Modules.Num() > 0 && Modules[0]->Code.Num() > 0)
			? Modules[0]->Code[0].Code
			: FString();

		ASSERT_THAT(IsTrue(
			bPreprocessSucceeded,
			TEXT("Range-based for rewrite test case should preprocess successfully")));
		ASSERT_THAT(AreEqual(
			0,
			PreprocessErrorCount,
			TEXT("Range-based for rewrite test case should not emit preprocessing errors")));
		ASSERT_THAT(AreEqual(
			0,
			PreprocessMessages.Num(),
			TEXT("Range-based for rewrite test case should keep preprocessing diagnostics empty")));
		ASSERT_THAT(AreEqual(
			1,
			Modules.Num(),
			TEXT("Range-based for rewrite test case should produce exactly one module descriptor")));
		ASSERT_THAT(AreEqual(
			CompilerPipelineControlFlowTest::ModuleName.ToString(),
			Modules[0]->ModuleName,
			TEXT("Range-based for rewrite test case should preserve the expected module name")));

		ASSERT_THAT(AreEqual(
			2,
			CompilerPipelineControlFlowTest::CountOccurrences(ProcessedCode, TEXT("_Iterator.CanProceed; )")),
			TEXT("Range-based for rewrite test case should rewrite both loops into iterator advance conditions")));
		ASSERT_THAT(AreEqual(
			2,
			CompilerPipelineControlFlowTest::CountOccurrences(ProcessedCode, TEXT("_Iterator.Proceed();")),
			TEXT("Range-based for rewrite test case should rewrite both loops into iterator proceed calls")));
		ASSERT_THAT(IsTrue(
			ProcessedCode.Contains(TEXT("__auto_constref_type")),
			TEXT("Range-based for rewrite test case should materialize the const-ref storage marker in processed code")));

		Engine.ResetDiagnostics();

		FAngelscriptCompileTraceSummary Summary;
		const bool bCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::SoftReloadOnly,
			CompilerPipelineControlFlowTest::ModuleName,
			CompilerPipelineControlFlowTest::RelativeScriptPath,
			ScriptSource,
			true,
			Summary,
			true);

		ASSERT_THAT(IsTrue(
			bCompiled,
			TEXT("Range-based for rewrite test case should compile through the normal preprocessor pipeline")));
		ASSERT_THAT(IsTrue(
			Summary.bUsedPreprocessor,
			TEXT("Range-based for rewrite test case should report that it used the preprocessor")));
		ASSERT_THAT(IsTrue(
			Summary.bCompileSucceeded,
			TEXT("Range-based for rewrite test case should mark compile succeeded in the summary")));
		ASSERT_THAT(AreEqual(
			0,
			Summary.Diagnostics.Num(),
			TEXT("Range-based for rewrite test case should keep compile diagnostics empty")));

		int32 EntryResult = 0;
		const bool bExecuted = bCompiled
			&& ExecuteIntFunction(
				&Engine,
				CompilerPipelineControlFlowTest::RelativeScriptPath,
				CompilerPipelineControlFlowTest::ModuleName,
				TEXT("int Entry()"),
				EntryResult);
		ASSERT_THAT(IsTrue(
			bExecuted,
			TEXT("Range-based for rewrite test case should execute the compiled Entry function")));
		ASSERT_THAT(AreEqual(
			4242,
			EntryResult,
			TEXT("Range-based for rewrite test case should keep both block and single-line loop sums executable")));

		}

	}

};

#endif
