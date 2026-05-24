#include "Shared/AngelscriptTestEngineHelper.h"
#include "Shared/AngelscriptTestMacros.h"

#include "Preprocessor/AngelscriptPreprocessor.h"

#include "HAL/FileManager.h"
#include "CQTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace CompilerPipelineFormatStringTest
{
	static const FName ModuleName(TEXT("Tests.Compiler.FormatStringRewriteProducesExpectedOutput"));
	static const FString RelativeScriptPath(TEXT("Tests/Compiler/FormatStringRewriteProducesExpectedOutput.as"));

	FString GetFixtureRoot()
	{
		return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("CompilerFormatStringFixtures"));
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

using namespace CompilerPipelineFormatStringTest;

TEST_CLASS_WITH_FLAGS(FCompilerPipelineFormatStringTests,
	"Angelscript.TestModule.Compiler.EndToEnd",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(FormatStringRewriteProducesExpectedOutput)
	{


		const FString ScriptSource = TEXT(R"AS(
	int Entry()
	{
		float Value = 12.34f;
		int Score = 0;

		if (f"{{Alpha}}" == "{Alpha}")
		{
			Score += 1000;
		}

		if (f"{20 + 1}" == "21")
		{
			Score += 100;
		}

		if (f"{21 =}" == "21 = 21")
		{
			Score += 10;
		}

		if (f"{255 :#06x}" == "0x00ff")
		{
			Score += 4;
		}

		if (f"{Value :.1f}" == "12.3")
		{
			Score += 2;
		}

		if (f"{Value =:.1f}" == "Value = 12.3")
		{
			Score += 1;
		}

		return Score;
	}
	)AS");

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine);

		const FString AbsoluteScriptPath = CompilerPipelineFormatStringTest::WriteFixture(
			CompilerPipelineFormatStringTest::RelativeScriptPath,
			ScriptSource);
		ON_SCOPE_EXIT
		{
			Engine.DiscardModule(*CompilerPipelineFormatStringTest::ModuleName.ToString());
			IFileManager::Get().Delete(*AbsoluteScriptPath, false, true);
		};

		Engine.ResetDiagnostics();

		FAngelscriptPreprocessor Preprocessor;
		Preprocessor.AddFile(CompilerPipelineFormatStringTest::RelativeScriptPath, AbsoluteScriptPath);

		const bool bPreprocessSucceeded = Preprocessor.Preprocess();
		const TArray<TSharedRef<FAngelscriptModuleDesc>> Modules = Preprocessor.GetModulesToCompile();

		int32 PreprocessErrorCount = 0;
		const TArray<FString> PreprocessMessages = CompilerPipelineFormatStringTest::CollectDiagnosticMessages(
			Engine,
			AbsoluteScriptPath,
			PreprocessErrorCount);
		const FString ProcessedCode = (Modules.Num() > 0 && Modules[0]->Code.Num() > 0)
			? Modules[0]->Code[0].Code
			: FString();

		TestRunner->TestTrue(
			TEXT("Format string rewrite test case should preprocess successfully"),
			bPreprocessSucceeded);
		TestRunner->TestEqual(
			TEXT("Format string rewrite test case should not emit preprocessing errors"),
			PreprocessErrorCount,
			0);
		TestRunner->TestEqual(
			TEXT("Format string rewrite test case should keep preprocessing diagnostics empty"),
			PreprocessMessages.Num(),
			0);
		TestRunner->TestEqual(
			TEXT("Format string rewrite test case should produce exactly one module descriptor"),
			Modules.Num(),
			1);
		if (Modules.Num() > 0)
		{
			TestRunner->TestEqual(
				TEXT("Format string rewrite test case should preserve the expected module name"),
				Modules[0]->ModuleName,
				CompilerPipelineFormatStringTest::ModuleName.ToString());
		}

		TestRunner->TestTrue(
			TEXT("Format string rewrite test case should materialize escaped opening braces in processed code"),
			ProcessedCode.Contains(TEXT(".AppendChar('{')")));
		TestRunner->TestTrue(
			TEXT("Format string rewrite test case should materialize escaped closing braces in processed code"),
			ProcessedCode.Contains(TEXT(".AppendChar('}')")));
		TestRunner->TestTrue(
			TEXT("Format string rewrite test case should rewrite equals expansion into an explicit label prefix"),
			ProcessedCode.Contains(TEXT("\"21 = \"+(21)")));
		TestRunner->TestEqual(
			TEXT("Format string rewrite test case should produce three ApplyFormat calls for both specifier paths"),
			CompilerPipelineFormatStringTest::CountOccurrences(ProcessedCode, TEXT("FString::ApplyFormat((")),
			3);
		TestRunner->TestTrue(
			TEXT("Format string rewrite test case should preserve the hex format specifier in processed code"),
			ProcessedCode.Contains(TEXT("\"#06x\"")));
		TestRunner->TestTrue(
			TEXT("Format string rewrite test case should preserve the decimal precision format specifier in processed code"),
			ProcessedCode.Contains(TEXT("\".1f\"")));

		Engine.ResetDiagnostics();

		FAngelscriptCompileTraceSummary Summary;
		const bool bCompiled = CompileModuleWithSummary(
			&Engine,
			ECompileType::SoftReloadOnly,
			CompilerPipelineFormatStringTest::ModuleName,
			CompilerPipelineFormatStringTest::RelativeScriptPath,
			ScriptSource,
			true,
			Summary,
			true);

		TestRunner->TestTrue(
			TEXT("Format string rewrite test case should compile through the normal preprocessor pipeline"),
			bCompiled);
		TestRunner->TestTrue(
			TEXT("Format string rewrite test case should report that it used the preprocessor"),
			Summary.bUsedPreprocessor);
		TestRunner->TestTrue(
			TEXT("Format string rewrite test case should mark compile succeeded in the summary"),
			Summary.bCompileSucceeded);
		TestRunner->TestEqual(
			TEXT("Format string rewrite test case should keep compile diagnostics empty"),
			Summary.Diagnostics.Num(),
			0);

		int32 EntryResult = 0;
		const bool bExecuted = bCompiled
			&& ExecuteIntFunction(
				&Engine,
				CompilerPipelineFormatStringTest::RelativeScriptPath,
				CompilerPipelineFormatStringTest::ModuleName,
				TEXT("int Entry()"),
				EntryResult);
		TestRunner->TestTrue(
			TEXT("Format string rewrite test case should execute the compiled Entry function"),
			bExecuted);
		if (bExecuted)
		{
			TestRunner->TestEqual(
				TEXT("Format string rewrite test case should keep escaped braces, plain interpolation, equals expansion, and every specifier branch executable"),
				EntryResult,
				1117);
		}

		}

	}

};

#endif
