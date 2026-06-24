#include "CQTest.h"
#include "AngelscriptSource.h"
#include "Preprocessor/AngelscriptPreprocessorTestHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptVirtualScriptPathPreprocessorTest,
	"Angelscript.TestModule.Preprocessor.VirtualScriptPaths",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(AddFileEmitsGameVirtualPathMetadata)
	{
		using namespace PreprocessorTestHelpers;

		FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
		{ FAngelscriptEngineScope _AutoEngineScope(Engine); FScopedModuleCleanEngine _AutoModuleClean(Engine);

		FFixtureFile File(TEXT("Tests/VirtualPath/AddFile.as"), TEXT(R"(
int Entry()
{
	return 9;
}
)"));

		FAngelscriptPreprocessor Preprocessor;
		Preprocessor.AddFile(File.RelativePath, File.AbsolutePath);

		ASSERT_THAT(IsTrue(Preprocessor.Preprocess(), TEXT("AddFile source should preprocess")));
		TArray<TSharedRef<FAngelscriptModuleDesc>> Modules = Preprocessor.GetModulesToCompile();
		FAngelscriptPreprocessorSummary Summary = Preprocessor.GetSummary();

		ASSERT_THAT(AreEqual(1, Summary.Files.Num(), TEXT("Summary should contain one file")));
		if (Summary.Files.Num() == 1)
		{
			ASSERT_THAT(AreEqual(
				FString(TEXT("/Angelscript/Game/Tests/VirtualPath/AddFile.as")),
				Summary.Files[0].VirtualPath,
				TEXT("AddFile should synthesize a game virtual path")));
		}

		const FAngelscriptModuleDesc* Module = nullptr;
		for (const TSharedRef<FAngelscriptModuleDesc>& Candidate : Modules)
		{
			if (Candidate->ModuleName == TEXT("Tests.VirtualPath.AddFile"))
			{
				Module = &Candidate.Get();
				break;
			}
		}
		ASSERT_THAT(IsNotNull(Module, TEXT("AddFile module should be emitted")));
		if (Module != nullptr)
		{
			ASSERT_THAT(AreEqual(1, Module->Code.Num(), TEXT("Module should contain one code section")));
			if (Module->Code.Num() == 1)
			{
				ASSERT_THAT(AreEqual(
					FString(TEXT("/Angelscript/Game/Tests/VirtualPath/AddFile.as")),
					Module->Code[0].VirtualPath,
					TEXT("Code section should carry virtual path")));
			}
		}

		}
	}

	TEST_METHOD(AddSourcePreprocessesMemoryText)
	{
		FAngelscriptPreprocessor Preprocessor;
		Preprocessor.AddSource(FAngelscriptSource::FromMemorySource(
			TEXT("/Angelscript/Memory/Immediate/Snippet_001.as"),
			TEXT(R"(
int Entry()
{
	return 11;
}
)")));

		ASSERT_THAT(IsTrue(Preprocessor.Preprocess(), TEXT("Memory source should preprocess")));
		TArray<TSharedRef<FAngelscriptModuleDesc>> Modules = Preprocessor.GetModulesToCompile();
		ASSERT_THAT(AreEqual(1, Modules.Num(), TEXT("Memory source should emit one module")));

		if (Modules.Num() == 1)
		{
			const FAngelscriptModuleDesc& Module = Modules[0].Get();
			ASSERT_THAT(AreEqual(
				FString(TEXT("Angelscript.Memory.Immediate.Snippet_001")),
				Module.ModuleName,
				TEXT("Memory module name should be isolated")));
			ASSERT_THAT(AreEqual(1, Module.Code.Num(), TEXT("Memory module should contain one code section")));
			if (Module.Code.Num() == 1)
			{
				ASSERT_THAT(AreEqual(
					FString(TEXT("/Angelscript/Memory/Immediate/Snippet_001.as")),
					Module.Code[0].VirtualPath,
					TEXT("Memory section should use virtual path metadata")));
				ASSERT_THAT(AreEqual(
					FString(),
					Module.Code[0].AbsoluteFilename,
					TEXT("Memory section should have no physical filename")));
			}
		}

		FAngelscriptPreprocessorSummary Summary = Preprocessor.GetSummary();
		ASSERT_THAT(AreEqual(1, Summary.Files.Num(), TEXT("Memory summary should contain one file")));
		if (Summary.Files.Num() == 1)
		{
			ASSERT_THAT(AreEqual(
				FString(TEXT("/Angelscript/Memory/Immediate/Snippet_001.as")),
				Summary.Files[0].VirtualPath,
				TEXT("Memory summary should carry virtual path")));
		}
	}

	TEST_METHOD(AddSourceRejectsInvalidVirtualPathDescriptor)
	{
		FAngelscriptSource InvalidSource;
		InvalidSource.SourceText = TEXT(R"(
int Entry()
{
	return 13;
}
)");
		InvalidSource.bHasSourceText = true;

		TestRunner->AddExpectedError(TEXT("Invalid Angelscript source descriptor"), EAutomationExpectedErrorFlags::Contains, 1);

		FAngelscriptPreprocessor Preprocessor;
		Preprocessor.AddSource(InvalidSource);

		ASSERT_THAT(IsFalse(Preprocessor.Preprocess(), TEXT("Invalid source descriptor should fail preprocessing before source text is compiled")));
		ASSERT_THAT(AreEqual(0, Preprocessor.GetModulesToCompile().Num(), TEXT("Invalid source descriptor should not emit modules")));

		const FAngelscriptPreprocessorSummary Summary = Preprocessor.GetSummary();
		ASSERT_THAT(IsTrue(Summary.bHasError, TEXT("Invalid source descriptor should mark the preprocessor as failed")));
		ASSERT_THAT(AreEqual(0, Summary.Files.Num(), TEXT("Invalid source descriptor should not appear in the file summary")));
	}
};

#endif
