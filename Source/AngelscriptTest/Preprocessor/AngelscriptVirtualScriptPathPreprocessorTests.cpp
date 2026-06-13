#include "CQTest.h"
#include "AngelscriptScriptSource.h"
#include "Preprocessor/AngelscriptPreprocessorTestHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace PreprocessorTestHelpers;

TEST_CLASS_WITH_FLAGS(FAngelscriptVirtualScriptPathPreprocessorTest,
	"Angelscript.TestModule.Preprocessor.VirtualScriptPaths",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(AddFileEmitsGameVirtualPathMetadata)
	{
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

		TestRunner->TestTrue(TEXT("AddFile source should preprocess"), Preprocessor.Preprocess());
		TArray<TSharedRef<FAngelscriptModuleDesc>> Modules = Preprocessor.GetModulesToCompile();
		FAngelscriptPreprocessorSummary Summary = Preprocessor.GetSummary();

		TestRunner->TestEqual(TEXT("Summary should contain one file"), Summary.Files.Num(), 1);
		if (Summary.Files.Num() == 1)
		{
			TestRunner->TestEqual(
				TEXT("AddFile should synthesize a game virtual path"),
				Summary.Files[0].VirtualPath,
				FString(TEXT("/Angelscript/Game/Tests/VirtualPath/AddFile.as")));
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
		TestRunner->TestNotNull(TEXT("AddFile module should be emitted"), Module);
		if (Module != nullptr)
		{
			TestRunner->TestEqual(TEXT("Module should contain one code section"), Module->Code.Num(), 1);
			if (Module->Code.Num() == 1)
			{
				TestRunner->TestEqual(
					TEXT("Code section should carry virtual path"),
					Module->Code[0].VirtualPath,
					FString(TEXT("/Angelscript/Game/Tests/VirtualPath/AddFile.as")));
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

		TestRunner->TestTrue(TEXT("Memory source should preprocess"), Preprocessor.Preprocess());
		TArray<TSharedRef<FAngelscriptModuleDesc>> Modules = Preprocessor.GetModulesToCompile();
		TestRunner->TestEqual(TEXT("Memory source should emit one module"), Modules.Num(), 1);

		if (Modules.Num() == 1)
		{
			const FAngelscriptModuleDesc& Module = Modules[0].Get();
			TestRunner->TestEqual(
				TEXT("Memory module name should be isolated"),
				Module.ModuleName,
				FString(TEXT("Angelscript.Memory.Immediate.Snippet_001")));
			TestRunner->TestEqual(TEXT("Memory module should contain one code section"), Module.Code.Num(), 1);
			if (Module.Code.Num() == 1)
			{
				TestRunner->TestEqual(
					TEXT("Memory section should use virtual path metadata"),
					Module.Code[0].VirtualPath,
					FString(TEXT("/Angelscript/Memory/Immediate/Snippet_001.as")));
				TestRunner->TestEqual(
					TEXT("Memory section should have no physical filename"),
					Module.Code[0].AbsoluteFilename,
					FString());
			}
		}

		FAngelscriptPreprocessorSummary Summary = Preprocessor.GetSummary();
		TestRunner->TestEqual(TEXT("Memory summary should contain one file"), Summary.Files.Num(), 1);
		if (Summary.Files.Num() == 1)
		{
			TestRunner->TestEqual(
				TEXT("Memory summary should carry virtual path"),
				Summary.Files[0].VirtualPath,
				FString(TEXT("/Angelscript/Memory/Immediate/Snippet_001.as")));
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

		TestRunner->TestFalse(TEXT("Invalid source descriptor should fail preprocessing before source text is compiled"), Preprocessor.Preprocess());
		TestRunner->TestEqual(TEXT("Invalid source descriptor should not emit modules"), Preprocessor.GetModulesToCompile().Num(), 0);

		const FAngelscriptPreprocessorSummary Summary = Preprocessor.GetSummary();
		TestRunner->TestTrue(TEXT("Invalid source descriptor should mark the preprocessor as failed"), Summary.bHasError);
		TestRunner->TestEqual(TEXT("Invalid source descriptor should not appear in the file summary"), Summary.Files.Num(), 0);
	}
};

#endif
