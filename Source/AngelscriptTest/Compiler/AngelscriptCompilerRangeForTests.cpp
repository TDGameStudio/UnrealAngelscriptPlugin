#include "AngelscriptTestEngineHelper.h"
#include "AngelscriptTestMacros.h"

#include "Preprocessor/AngelscriptPreprocessor.h"

#include "HAL/FileManager.h"
#include "CQTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace CompilerRangeForTest
{
	static const FName ModuleName(TEXT("Tests.Compiler.RangeBasedForRewriteSkipsStringAndCommentLiterals"));
	static const FString RelativeScriptPath(TEXT("Tests/Compiler/RangeBasedForRewriteSkipsStringAndCommentLiterals.as"));
	static const FString RawLoopText(TEXT("for (const int Value : Values)"));
	static const FString StringLiteralToken(TEXT("\"for (const int Value : Values)\""));
	static const FString LineCommentToken(TEXT("// for (const int Value : Values)"));
	static const FString BlockCommentToken(TEXT("/* for (const int Value : Values) */"));
	static const int32 ExpectedRawLoopTextOccurrences = 4;
	static const int32 ExpectedEntryResult = 42;

	FString GetFixtureRoot()
	{
		return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("CompilerRangeForFixtures"));
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

	FString JoinMessages(const TArray<FString>& Messages)
	{
		return FString::Join(Messages, TEXT(" | "));
	}

	FString JoinDiagnostics(const TArray<FAngelscriptCompileTraceDiagnosticSummary>& Diagnostics)
	{
		TArray<FString> Lines;
		for (const FAngelscriptCompileTraceDiagnosticSummary& Diagnostic : Diagnostics)
		{
			Lines.Add(FString::Printf(
				TEXT("[%s] %s(%d:%d) %s"),
				Diagnostic.bIsError ? TEXT("Error") : (Diagnostic.bIsInfo ? TEXT("Info") : TEXT("Warning")),
				*Diagnostic.Section,
				Diagnostic.Row,
				Diagnostic.Column,
				*Diagnostic.Message));
		}

		return FString::Join(Lines, TEXT(" | "));
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

TEST_CLASS_WITH_FLAGS(FAngelscriptCompilerRangeForTests,
	"Angelscript.TestModule.Compiler.EndToEnd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(RangeBasedForRewriteSkipsStringAndCommentLiterals)
	{


		const FString TestScriptSource = TEXT(R"AS(
	int Entry()
	{
		TArray<int> Values;
		Values.Add(20);
		Values.Add(22);

		FString LoopText = "for (const int Value : Values)";
		// for (const int Value : Values)
		/* for (const int Value : Values) */

		int Sum = 0;
		for (const int Value : Values)
		{
			Sum += Value;
		}

		if (LoopText != "for (const int Value : Values)")
			return 10;

		return Sum;
	}
	)AS");

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		const FString AbsoluteScriptPath = CompilerRangeForTest::WriteFixture(
			CompilerRangeForTest::RelativeScriptPath,
			TestScriptSource);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*CompilerRangeForTest::ModuleName.ToString());
			IFileManager::Get().Delete(*AbsoluteScriptPath, false, true);
		};

		Engine.ResetDiagnostics();

		FAngelscriptPreprocessor Preprocessor;
		Preprocessor.AddFile(CompilerRangeForTest::RelativeScriptPath, AbsoluteScriptPath);

		const bool bPreprocessSucceeded = Preprocessor.Preprocess();
		const TArray<TSharedRef<FAngelscriptModuleDesc>> Modules = Preprocessor.GetModulesToCompile();

		int32 PreprocessErrorCount = 0;
		const TArray<FString> PreprocessMessages = CompilerRangeForTest::CollectDiagnosticMessages(
			Engine,
			AbsoluteScriptPath,
			PreprocessErrorCount);
		if (PreprocessMessages.Num() > 0)
		{
			TestRunner->AddInfo(FString::Printf(
				TEXT("Range-for preprocess diagnostics: %s"),
				*CompilerRangeForTest::JoinMessages(PreprocessMessages)));
		}

		const FString ProcessedCode = (Modules.Num() > 0 && Modules[0]->Code.Num() > 0)
			? Modules[0]->Code[0].Code
			: FString();

		ASSERT_THAT(IsTrue(
			bPreprocessSucceeded,
			TEXT("Range-based for literal/comment guard test case should preprocess successfully")));
		ASSERT_THAT(AreEqual(
			0,
			PreprocessErrorCount,
			TEXT("Range-based for literal/comment guard test case should not emit preprocessing errors")));
		ASSERT_THAT(AreEqual(
			0,
			PreprocessMessages.Num(),
			TEXT("Range-based for literal/comment guard test case should keep preprocessing diagnostics empty")));
		ASSERT_THAT(AreEqual(
			1,
			Modules.Num(),
			TEXT("Range-based for literal/comment guard test case should produce exactly one module descriptor")));
		if (Modules.Num() > 0)
		{
			ASSERT_THAT(AreEqual(
				CompilerRangeForTest::ModuleName.ToString(),
				Modules[0]->ModuleName,
				TEXT("Range-based for literal/comment guard test case should preserve the expected module name")));
		}

		ASSERT_THAT(AreEqual(
			1,
			CompilerRangeForTest::CountOccurrences(ProcessedCode, TEXT("_Iterator.CanProceed; )")),
			TEXT("Range-based for literal/comment guard test case should rewrite exactly one real loop into iterator advance form")));
		ASSERT_THAT(AreEqual(
			1,
			CompilerRangeForTest::CountOccurrences(ProcessedCode, TEXT("_Iterator.Proceed();")),
			TEXT("Range-based for literal/comment guard test case should rewrite exactly one real loop into iterator proceed form")));
		ASSERT_THAT(AreEqual(
			CompilerRangeForTest::ExpectedRawLoopTextOccurrences,
			CompilerRangeForTest::CountOccurrences(ProcessedCode, CompilerRangeForTest::RawLoopText),
			TEXT("Range-based for literal/comment guard test case should preserve the raw loop text only inside the two strings and two comments")));
		ASSERT_THAT(IsTrue(
			ProcessedCode.Contains(CompilerRangeForTest::StringLiteralToken),
			TEXT("Range-based for literal/comment guard test case should preserve the exact string literal payload")));
		ASSERT_THAT(IsTrue(
			ProcessedCode.Contains(CompilerRangeForTest::LineCommentToken),
			TEXT("Range-based for literal/comment guard test case should preserve the single-line comment payload")));
		ASSERT_THAT(IsTrue(
			ProcessedCode.Contains(CompilerRangeForTest::BlockCommentToken),
			TEXT("Range-based for literal/comment guard test case should preserve the block comment payload")));

		Engine.ResetDiagnostics();

		FAngelscriptCompileTraceSummary Summary;
		const bool bCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::SoftReloadOnly,
			CompilerRangeForTest::ModuleName,
			CompilerRangeForTest::RelativeScriptPath,
			TestScriptSource,
			true,
			Summary,
			true);
		if (Summary.Diagnostics.Num() > 0)
		{
			TestRunner->AddInfo(FString::Printf(
				TEXT("Range-for compile diagnostics: %s"),
				*CompilerRangeForTest::JoinDiagnostics(Summary.Diagnostics)));
		}

		ASSERT_THAT(IsTrue(
			bCompiled,
			TEXT("Range-based for literal/comment guard test case should compile through the normal preprocessor pipeline")));
		ASSERT_THAT(IsTrue(
			Summary.bUsedPreprocessor,
			TEXT("Range-based for literal/comment guard test case should report that it used the preprocessor")));
		ASSERT_THAT(IsTrue(
			Summary.bCompileSucceeded,
			TEXT("Range-based for literal/comment guard test case should mark compile succeeded in the summary")));
		ASSERT_THAT(AreEqual(
			0,
			Summary.Diagnostics.Num(),
			TEXT("Range-based for literal/comment guard test case should keep compile diagnostics empty")));

		int32 EntryResult = 0;
		const bool bExecuted = bCompiled
			&& ExecuteIntFunction(
				&Engine,
				CompilerRangeForTest::RelativeScriptPath,
				CompilerRangeForTest::ModuleName,
				TEXT("int Entry()"),
				EntryResult);
		ASSERT_THAT(IsTrue(
			bExecuted,
			TEXT("Range-based for literal/comment guard test case should execute the compiled Entry function")));
		if (bExecuted)
		{
			ASSERT_THAT(AreEqual(
				CompilerRangeForTest::ExpectedEntryResult,
				EntryResult,
				TEXT("Range-based for literal/comment guard test case should preserve the string literal while keeping the real loop executable")));
		}

		}

	}

};

#endif
