#include "CQTest.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace
{
	static FString GetGeneratedDirectory()
	{
		return FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("Angelscript/Intermediate/Build/Win64/UnrealEditor/Inc/AngelscriptRuntime/UHT"));
	}

	static bool LoadGeneratedFile(const TCHAR* FileName, FString& Contents, FAutomationTestBase& Test)
	{
		const FString Path = FPaths::Combine(GetGeneratedDirectory(), FileName);
		return Test.TestTrue(*FString::Printf(TEXT("Generated binding file should exist: %s"), *Path), FFileHelper::LoadFileToString(Contents, *Path));
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptGeneratedFunctionBindingTests,
	"Angelscript.TestModule.Engine.GeneratedFunctionBinding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(GeneratedRuntimeShardsUseFunctionBindingPrefix)
	{
		TArray<FString> GeneratedFiles;
		IFileManager::Get().FindFilesRecursive(GeneratedFiles, *GetGeneratedDirectory(), TEXT("AS_FunctionBinding_*.gen.cpp"), true, false);
		if (!TestRunner->TestTrue(TEXT("Function binding UHT should emit runtime shards"), GeneratedFiles.Num() > 0))
		{
			return;
		}

		int32 RegistrationCount = 0;
		for (const FString& GeneratedFile : GeneratedFiles)
		{
			FString Contents;
			if (!FFileHelper::LoadFileToString(Contents, *GeneratedFile))
			{
				continue;
			}
			TArray<FString> Lines;
			Contents.ParseIntoArrayLines(Lines);
			for (const FString& Line : Lines)
			{
				RegistrationCount += Line.Contains(TEXT("FAngelscriptBinds::RegisterFunctionBinding(")) ? 1 : 0;
			}
		}
		TestRunner->TestTrue(TEXT("Generated runtime shards should contain registrations"), RegistrationCount > 0);
	}

	TEST_METHOD(StatisticsUseAnalyzedFunctionDenominator)
	{
		FString StatisticsContents;
		if (!LoadGeneratedFile(TEXT("AS_FunctionBindingStatistics.json"), StatisticsContents, *TestRunner))
		{
			return;
		}

		bool bPassed = true;
		bPassed &= TestRunner->TestTrue(TEXT("Statistics should expose the selected binding method"), StatisticsContents.Contains(TEXT("functionBindingMethod")));
		bPassed &= TestRunner->TestTrue(TEXT("Statistics should expose total analyzed functions"), StatisticsContents.Contains(TEXT("totalAnalyzedFunctions")));
		bPassed &= TestRunner->TestTrue(TEXT("Statistics should expose Runtime-linked count"), StatisticsContents.Contains(TEXT("totalNativeRuntimeLinkedCount")));
		bPassed &= TestRunner->TestTrue(TEXT("Statistics should expose reflective fallback count"), StatisticsContents.Contains(TEXT("totalReflectiveFallbackCount")));
		bPassed &= TestRunner->TestFalse(TEXT("Statistics should not expose the retired generic categories"), StatisticsContents.Contains(TEXT("totalDirectBindEntries")) || StatisticsContents.Contains(TEXT("totalStubEntries")) || StatisticsContents.Contains(TEXT("totalModuleBindingEntries")));
		TestRunner->TestTrue(TEXT("Function binding statistics contract should pass"), bPassed);
	}

	TEST_METHOD(DiagnosticsUseFunctionBindingCategories)
	{
		FString DiagnosticsContents;
		if (!LoadGeneratedFile(TEXT("AS_FunctionBindingDiagnostics.csv"), DiagnosticsContents, *TestRunner))
		{
			return;
		}

		bool bPassed = true;
		bPassed &= TestRunner->TestTrue(TEXT("Diagnostics should name FunctionBindingCategory"), DiagnosticsContents.StartsWith(TEXT("ModuleName,EditorOnly,ClassName,FunctionName,FunctionBindingCategory")));
		bPassed &= TestRunner->TestTrue(TEXT("Diagnostics should contain Runtime-linked category"), DiagnosticsContents.Contains(TEXT(",NativeRuntimeLinked,")));
		bPassed &= TestRunner->TestTrue(TEXT("Diagnostics should contain reflective fallback category"), DiagnosticsContents.Contains(TEXT(",ReflectiveFallback,")));
		bPassed &= TestRunner->TestFalse(TEXT("Diagnostics should not contain legacy categories"), DiagnosticsContents.Contains(TEXT(",Direct,")) || DiagnosticsContents.Contains(TEXT(",Stub,")) || DiagnosticsContents.Contains(TEXT(",ModuleBinding,")));
		TestRunner->TestTrue(TEXT("Function binding diagnostics contract should pass"), bPassed);
	}
};

#endif
