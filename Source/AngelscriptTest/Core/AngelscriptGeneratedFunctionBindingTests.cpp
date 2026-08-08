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
	static int32 CountOccurrences(const FString& Source, const FString& Needle)
	{
		int32 Count = 0;
		int32 SearchFrom = 0;
		while (true)
		{
			const int32 FoundAt = Source.Find(Needle, ESearchCase::CaseSensitive, ESearchDir::FromStart, SearchFrom);
			if (FoundAt == INDEX_NONE)
			{
				return Count;
			}

			++Count;
			SearchFrom = FoundAt + Needle.Len();
		}
	}

public:
	TEST_METHOD(GeneratedRuntimeModulesUseSingleNamedBindingFile)
	{
		TArray<FString> GeneratedFiles;
		IFileManager::Get().FindFilesRecursive(GeneratedFiles, *GetGeneratedDirectory(), TEXT("AS_FunctionBinding_*.gen.cpp"), true, false);
		if (!TestRunner->TestTrue(TEXT("Function binding UHT should emit Runtime-linked module files"), GeneratedFiles.Num() > 0))
		{
			return;
		}

		TMap<FString, int32> ModuleFileCounts;
		bool bPassed = true;
		for (const FString& GeneratedFile : GeneratedFiles)
		{
			FString Contents;
			if (!FFileHelper::LoadFileToString(Contents, *GeneratedFile))
			{
				bPassed &= TestRunner->TestTrue(*FString::Printf(TEXT("Generated binding source should be readable: %s"), *GeneratedFile), false);
				continue;
			}

			FString ModuleName = FPaths::GetCleanFilename(GeneratedFile);
			ModuleName.RemoveFromStart(TEXT("AS_FunctionBinding_"));
			ModuleName.RemoveFromEnd(TEXT(".gen.cpp"));
			ModuleFileCounts.FindOrAdd(ModuleName)++;

			bool bLegacyShardSuffix = ModuleName.Len() >= 4 && ModuleName[ModuleName.Len() - 4] == TCHAR('_');
			for (int32 CharacterIndex = ModuleName.Len() - 3; bLegacyShardSuffix && CharacterIndex < ModuleName.Len(); ++CharacterIndex)
			{
				bLegacyShardSuffix &= FChar::IsDigit(ModuleName[CharacterIndex]);
			}

			bPassed &= TestRunner->TestFalse(*FString::Printf(TEXT("Generated Runtime-linked source should not use a numeric shard suffix: %s"), *GeneratedFile), bLegacyShardSuffix);
			bPassed &= TestRunner->TestTrue(*FString::Printf(TEXT("Generated Runtime-linked bind should have a stable module name: %s"), *GeneratedFile), Contents.Contains(*FString::Printf(TEXT("TEXT(\"UHT.FunctionBinding.%s\")"), *ModuleName)));
			bPassed &= TestRunner->TestFalse(*FString::Printf(TEXT("Generated Runtime-linked bind should not start an elapsed-time measurement: %s"), *GeneratedFile), Contents.Contains(TEXT("GeneratedFunctionBindingStartSeconds")));
			bPassed &= TestRunner->TestFalse(*FString::Printf(TEXT("Generated Runtime-linked bind should not record shard timing: %s"), *GeneratedFile), Contents.Contains(TEXT("RecordGeneratedFunctionBindingShardTiming")));
			bPassed &= TestRunner->TestFalse(*FString::Printf(TEXT("Generated Runtime-linked bind should not log registration timing: %s"), *GeneratedFile), Contents.Contains(TEXT("[UHT] Registered")));
		}

		for (const TPair<FString, int32>& ModuleFileCount : ModuleFileCounts)
		{
			bPassed &= TestRunner->TestEqual(*FString::Printf(TEXT("Each Runtime-linked module should emit one generated source: %s"), *ModuleFileCount.Key), ModuleFileCount.Value, 1);
		}
		TestRunner->TestTrue(TEXT("Generated Runtime-linked module output contract should pass"), bPassed);
	}

	TEST_METHOD(RuntimeLinkedShardUsesOneExplicitGeneratedCallback)
	{
		FString RuntimeShardContents;
		const FString RuntimeShardPath = FPaths::Combine(
			GetGeneratedDirectory(),
			TEXT("AS_FunctionBinding_Engine.gen.cpp"));
		ASSERT_THAT(IsTrue(
			FFileHelper::LoadFileToString(RuntimeShardContents, *RuntimeShardPath),
			*FString::Printf(TEXT("Runtime-linked shard should be readable: %s"), *RuntimeShardPath)));

		ASSERT_THAT(AreEqual(
			1,
			CountOccurrences(
				RuntimeShardContents,
				TEXT("AS_FORCE_LINK const FAngelscriptBind Bind_AS_FunctionBinding_")),
			TEXT("Runtime-linked shard should own exactly one direct bind record")));
		ASSERT_THAT(IsTrue(
			RuntimeShardContents.Contains(
				TEXT("static void BindGeneratedFunctionBindings_Engine(FAngelscriptBinds& Binds)")),
			TEXT("Runtime-linked shard should expose one named explicit-target callback")));
		ASSERT_THAT(IsTrue(
			RuntimeShardContents.Contains(
				TEXT("static void RegisterGeneratedFunctionBindingBatch_Engine_000(FAngelscriptBinds& Binds)")),
			TEXT("Runtime-linked shard batches should receive the explicit bind context")));
		ASSERT_THAT(IsTrue(
			RuntimeShardContents.Contains(
				TEXT("RegisterGeneratedFunctionBindingBatch_Engine_000(Binds);")),
			TEXT("Runtime-linked callback should forward its explicit context to each batch")));
		ASSERT_THAT(IsTrue(
			RuntimeShardContents.Contains(TEXT("EAngelscriptBindPhase::GeneratedBindings")),
			TEXT("Runtime-linked shard should execute in GeneratedBindings")));
		ASSERT_THAT(IsTrue(
			RuntimeShardContents.Contains(TEXT("Binds.RegisterGeneratedFunctionBindingForTarget(")),
			TEXT("Runtime-linked registrations should target the callback's engine")));
		ASSERT_THAT(IsFalse(
			RuntimeShardContents.Contains(TEXT("FAngelscriptBinds::FBind"))
				|| RuntimeShardContents.Contains(TEXT("FAngelscriptBinds::EOrder"))
				|| RuntimeShardContents.Contains(TEXT("FAngelscriptBinds::RegisterGeneratedFunctionBinding("))
				|| RuntimeShardContents.Contains(TEXT("[]()")),
			TEXT("Runtime-linked shard should not retain legacy provider or ambient registration syntax")));
	}

	TEST_METHOD(RuntimeLinkedShardPreservesNativeAndRpcRows)
	{
		FString RuntimeShardContents;
		ASSERT_THAT(IsTrue(
			LoadGeneratedFile(TEXT("AS_FunctionBinding_Engine.gen.cpp"), RuntimeShardContents, *TestRunner),
			TEXT("Runtime-linked Engine shard should be available")));

		ASSERT_THAT(IsTrue(
			RuntimeShardContents.Contains(TEXT(
				"Binds.RegisterGeneratedFunctionBindingForTarget(AGameModeBase::StaticClass(), \"GetNumPlayers\", { ERASE_AUTO_METHOD_PTR(AGameModeBase, GetNumPlayers) });")),
			TEXT("Runtime-linked shard should preserve direct native caller metadata")));
		ASSERT_THAT(IsTrue(
			RuntimeShardContents.Contains(TEXT(
				"Binds.RegisterGeneratedFunctionBindingForTarget(APlayerController::StaticClass(), \"ClientSetHUD\", { ERASE_NO_FUNCTION() });")),
			TEXT("Runtime-linked shard should preserve the RPC reflective-fallback placeholder")));
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
		bPassed &= TestRunner->TestTrue(TEXT("Diagnostics should preserve the RPC reflective-fallback decision"), DiagnosticsContents.Contains(TEXT("Engine,false,APlayerController,ClientSetHUD,ReflectiveFallback,rpc-net-function,ERASE_NO_FUNCTION(),AS_FunctionBinding_Engine.gen.cpp,1")));
		bPassed &= TestRunner->TestFalse(TEXT("Diagnostics should not contain legacy categories"), DiagnosticsContents.Contains(TEXT(",Direct,")) || DiagnosticsContents.Contains(TEXT(",Stub,")) || DiagnosticsContents.Contains(TEXT(",ModuleBinding,")));
		TestRunner->TestTrue(TEXT("Function binding diagnostics contract should pass"), bPassed);
	}
};

#endif
