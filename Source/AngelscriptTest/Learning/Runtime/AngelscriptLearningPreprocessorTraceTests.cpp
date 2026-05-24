#include "AngelscriptLearningTrace.h"
#include "AngelscriptTestUtilities.h"
#include "AngelscriptTestMacros.h"

#include "Preprocessor/AngelscriptPreprocessor.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS


namespace AngelscriptTest_Learning_Runtime_AngelscriptLearningPreprocessorTraceTests_Private
{
	FString GetLearningPreprocessorFixtureRoot()
	{
		return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("LearningPreprocessorFixtures"));
	}

	FString WriteLearningPreprocessorFixture(const FString& RelativeScriptPath, const FString& Contents)
	{
		const FString AbsolutePath = FPaths::Combine(GetLearningPreprocessorFixtureRoot(), RelativeScriptPath);
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(AbsolutePath), true);
		FFileHelper::SaveStringToFile(Contents, *AbsolutePath);
		return AbsolutePath;
	}

	const FAngelscriptModuleDesc* FindLearningPreprocessorModuleByName(const TArray<TSharedRef<FAngelscriptModuleDesc>>& Modules, const FString& ModuleName)
	{
		for (const TSharedRef<FAngelscriptModuleDesc>& Module : Modules)
		{
			if (Module->ModuleName == ModuleName)
			{
				return &Module.Get();
			}
		}

		return nullptr;
	}

	TArray<const FAngelscriptPreprocessor::FMacro*> GatherLearningPreprocessorMacros(const FAngelscriptPreprocessor& Preprocessor)
	{
		TArray<const FAngelscriptPreprocessor::FMacro*> Macros;
		for (const FAngelscriptPreprocessor::FFile& File : Preprocessor.Files)
		{
			for (const FAngelscriptPreprocessor::FChunk& Chunk : File.ChunkedCode)
			{
				for (const FAngelscriptPreprocessor::FMacro& Macro : Chunk.Macros)
				{
					Macros.Add(&Macro);
				}
			}
		}
		return Macros;
	}

	FString GetChunkTypeLabel(FAngelscriptPreprocessor::EChunkType ChunkType)
	{
		switch (ChunkType)
		{
		case FAngelscriptPreprocessor::EChunkType::Global:
			return TEXT("Global");
		case FAngelscriptPreprocessor::EChunkType::Class:
			return TEXT("Class");
		case FAngelscriptPreprocessor::EChunkType::Struct:
			return TEXT("Struct");
		case FAngelscriptPreprocessor::EChunkType::Enum:
			return TEXT("Enum");
		default:
			return TEXT("Unknown");
		}
	}
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptLearningPreprocessorTraceTest,
	"Angelscript.TestModule.Learning.Runtime.Preprocessor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptLearningPreprocessorTraceTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Learning_Runtime_AngelscriptLearningPreprocessorTraceTests_Private;
	FAngelscriptLearningTraceSinkConfig SinkConfig;
	SinkConfig.bEmitToAutomation = true;
	SinkConfig.bEmitToLog = true;
	SinkConfig.bEmitToFile = false;

	FAngelscriptLearningTraceSession Trace(TEXT("LearningPreprocessor"), SinkConfig);
	Trace.BeginPhase(EAngelscriptLearningTracePhase::Compile);

	const FString SharedRelativePath = TEXT("Tests/Learning/Preprocessor/Shared.as");
	const FString SharedAbsolutePath = WriteLearningPreprocessorFixture(
		SharedRelativePath,
		TEXT("int SharedValue()\n")
		TEXT("{\n")
		TEXT("    return 11;\n")
		TEXT("}\n"));

	const FString ImportingRelativePath = TEXT("Tests/Learning/Preprocessor/UsesImport.as");
	const FString ImportingAbsolutePath = WriteLearningPreprocessorFixture(
		ImportingRelativePath,
		TEXT("import Tests.Learning.Preprocessor.Shared;\n")
		TEXT("class ALearningMacroActor : AActor\n")
		TEXT("{\n")
		TEXT("    UPROPERTY(EditAnywhere, BlueprintReadWrite)\n")
		TEXT("    UStaticMesh Mesh;\n\n")
		TEXT("    UFUNCTION(BlueprintOverride)\n")
		TEXT("    int UseShared()\n")
		TEXT("    {\n")
		TEXT("        return SharedValue();\n")
		TEXT("    }\n")
		TEXT("}\n"));

	FAngelscriptPreprocessor Preprocessor;
	FAngelscriptEngine* CurrentEngine = FAngelscriptEngine::TryGetCurrentEngine();
	TOptional<TGuardValue<bool>> AutomaticImportGuard;
	if (CurrentEngine) { AutomaticImportGuard.Emplace(CurrentEngine->bUseAutomaticImportMethod, false); }
	TArray<FString> HookEvents;
	const FDelegateHandle ProcessChunksHandle = FAngelscriptPreprocessor::OnProcessChunks.AddLambda([&HookEvents](FAngelscriptPreprocessor& HookPreprocessor)
	{
		HookEvents.Add(FString::Printf(TEXT("OnProcessChunks:%d"), HookPreprocessor.Files.Num()));
	});
	const FDelegateHandle PostProcessHandle = FAngelscriptPreprocessor::OnPostProcessCode.AddLambda([&HookEvents](FAngelscriptPreprocessor& HookPreprocessor)
	{
		HookEvents.Add(FString::Printf(TEXT("OnPostProcessCode:%d"), HookPreprocessor.Files.Num()));
	});
	ON_SCOPE_EXIT
	{
		FAngelscriptPreprocessor::OnProcessChunks.Remove(ProcessChunksHandle);
		FAngelscriptPreprocessor::OnPostProcessCode.Remove(PostProcessHandle);
	};

	Preprocessor.AddFile(SharedRelativePath, SharedAbsolutePath);
	Preprocessor.AddFile(ImportingRelativePath, ImportingAbsolutePath);

	const FString NormalizedModuleName = Preprocessor.FilenameToModuleName(ImportingRelativePath);
	Trace.AddStep(TEXT("FilenameToModuleName"), TEXT("Normalized the relative script path into the module name that later compilation will use"));
	Trace.AddKeyValue(TEXT("RelativePath"), ImportingRelativePath);
	Trace.AddKeyValue(TEXT("ModuleName"), NormalizedModuleName);

	const bool bPreprocessed = Preprocessor.Preprocess();
	Trace.AddStep(TEXT("Preprocess"), bPreprocessed ? TEXT("Chunk parsing, macro detection, import stripping and code condensation all completed") : TEXT("Preprocess failed before producing stable module output"));
	Trace.AddKeyValue(TEXT("PreprocessResult"), bPreprocessed ? TEXT("true") : TEXT("false"));

	const TArray<TSharedRef<FAngelscriptModuleDesc>> Modules = Preprocessor.GetModulesToCompile();
	Trace.AddKeyValue(TEXT("ModuleCount"), FString::FromInt(Modules.Num()));
	if (HookEvents.Num() > 0)
	{
		Trace.AddCodeBlock(FormatLearningTraceStringList(HookEvents, TEXT("PreprocessHooks")));
	}

	const FAngelscriptModuleDesc* ImportingModule = FindLearningPreprocessorModuleByName(Modules, TEXT("Tests.Learning.Preprocessor.UsesImport"));
	TArray<FString> MacroSummaries;
	TArray<FString> ChunkSummaries;
	for (const FAngelscriptPreprocessor::FFile& File : Preprocessor.Files)
	{
		for (const FAngelscriptPreprocessor::FChunk& Chunk : File.ChunkedCode)
		{
			ChunkSummaries.Add(FString::Printf(TEXT("%s:%s"), *File.RelativeFilename, *GetChunkTypeLabel(Chunk.Type)));
		}
		for (const FAngelscriptPreprocessor::FChunk& Chunk : File.ChunkedCode)
		{
			for (const FAngelscriptPreprocessor::FMacro& Macro : Chunk.Macros)
			{
				MacroSummaries.Add(FString::Printf(TEXT("%s:%s"), *Macro.Name, *GetChunkTypeLabel(Chunk.Type)));
			}
		}
	}

	Trace.AddStep(TEXT("ChunkAndMacroSummary"), TEXT("Collected chunk types and macro names after preprocessing the importing file"));
	Trace.AddKeyValue(TEXT("ChunkCount"), FString::FromInt(ChunkSummaries.Num()));
	Trace.AddKeyValue(TEXT("MacroCount"), FString::FromInt(MacroSummaries.Num()));
	if (ChunkSummaries.Num() > 0)
	{
		Trace.AddCodeBlock(FormatLearningTraceStringList(ChunkSummaries, TEXT("ChunkTypes")));
	}
	if (MacroSummaries.Num() > 0)
	{
		Trace.AddCodeBlock(FormatLearningTraceStringList(MacroSummaries, TEXT("Macros")));
	}

	const bool bTracksImport = ImportingModule != nullptr && ImportingModule->ImportedModules.Contains(TEXT("Tests.Learning.Preprocessor.Shared"));
	const bool bImportRemovedFromCode = ImportingModule != nullptr && ImportingModule->Code.Num() > 0 && !ImportingModule->Code[0].Code.Contains(TEXT("import Tests.Learning.Preprocessor.Shared;"));
	Trace.AddStep(TEXT("ImportResolution"), TEXT("Verified that import statements become ImportedModules metadata instead of remaining in the processed code"));
	Trace.AddKeyValue(TEXT("TracksImport"), bTracksImport ? TEXT("true") : TEXT("false"));
	Trace.AddKeyValue(TEXT("ImportRemovedFromCode"), bImportRemovedFromCode ? TEXT("true") : TEXT("false"));
	if (ImportingModule != nullptr)
	{
		Trace.AddCodeBlock(FormatLearningTraceStringList(ImportingModule->ImportedModules, TEXT("ImportedModules")));
	}

	const TArray<const FAngelscriptPreprocessor::FMacro*> Macros = GatherLearningPreprocessorMacros(Preprocessor);
	const bool bHasPropertyMacro = Macros.ContainsByPredicate([](const FAngelscriptPreprocessor::FMacro* Macro)
	{
		return Macro->Type == FAngelscriptPreprocessor::EMacroType::Property && Macro->Name == TEXT("Mesh");
	});
	const bool bHasFunctionMacro = Macros.ContainsByPredicate([](const FAngelscriptPreprocessor::FMacro* Macro)
	{
		return Macro->Type == FAngelscriptPreprocessor::EMacroType::Function && Macro->Name == TEXT("UseShared");
	});
	const bool bSawProcessChunksHook = HookEvents.ContainsByPredicate([](const FString& Event)
	{
		return Event.StartsWith(TEXT("OnProcessChunks:"));
	});
	const bool bSawPostProcessHook = HookEvents.ContainsByPredicate([](const FString& Event)
	{
		return Event.StartsWith(TEXT("OnPostProcessCode:"));
	});

	const bool bPreprocessSucceeded = TestTrue(TEXT("Learning preprocessor fixture should preprocess successfully"), bPreprocessed);
	const bool bModuleNameNormalized = TestEqual(TEXT("FilenameToModuleName should normalize the importing file path"), NormalizedModuleName, FString(TEXT("Tests.Learning.Preprocessor.UsesImport")));
	const bool bModulesAvailable = TestEqual(TEXT("Preprocessor should keep both modules available for later compilation"), Modules.Num(), 2);
	const bool bPropertyMacroCaptured = TestTrue(TEXT("Learning preprocessor trace should capture the UPROPERTY macro"), bHasPropertyMacro);
	const bool bFunctionMacroCaptured = TestTrue(TEXT("Learning preprocessor trace should capture the UFUNCTION macro"), bHasFunctionMacro);
	const bool bImportTracked = TestTrue(TEXT("Learning preprocessor trace should record the imported module name"), bTracksImport);
	const bool bImportRemoved = TestTrue(TEXT("Learning preprocessor trace should strip the import line from processed code"), bImportRemovedFromCode);
	const bool bHooksObserved = TestTrue(TEXT("Learning preprocessor trace should observe both preprocess hook stages"), bSawProcessChunksHook && bSawPostProcessHook);
	const bool bPhaseSequenceOk = AssertLearningTracePhaseSequence(*this, Trace.GetEvents(), {
		EAngelscriptLearningTracePhase::Compile,
	});
	const bool bContainsImportKeyword = AssertLearningTraceContainsKeyword(*this, Trace.GetEvents(), TEXT("ImportedModules"));
	const bool bContainsModuleNameKeyword = AssertLearningTraceContainsKeyword(*this, Trace.GetEvents(), TEXT("ModuleName"));
	const bool bMinimumEventsOk = AssertLearningTraceMinimumEventCount(*this, Trace.GetEvents(), 4);

	Trace.FlushToAutomation(*this);
	Trace.FlushToLog();
	return bPreprocessSucceeded
		&& bModuleNameNormalized
		&& bModulesAvailable
		&& bPropertyMacroCaptured
		&& bFunctionMacroCaptured
		&& bImportTracked
		&& bImportRemoved
		&& bHooksObserved
		&& bPhaseSequenceOk
		&& bContainsImportKeyword
		&& bContainsModuleNameKeyword
		&& bMinimumEventsOk;
}

#endif
