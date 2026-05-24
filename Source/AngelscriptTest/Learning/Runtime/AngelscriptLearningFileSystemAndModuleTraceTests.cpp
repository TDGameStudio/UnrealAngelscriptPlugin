#include "Shared/AngelscriptLearningTrace.h"
#include "Shared/AngelscriptTestEngineHelper.h"
#include "Shared/AngelscriptTestUtilities.h"
#include "Shared/AngelscriptTestMacros.h"

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS


namespace AngelscriptTest_Learning_Runtime_AngelscriptLearningFileSystemAndModuleTraceTests_Private
{
	FString GetLearningFileSystemRoot()
	{
		return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Automation") / TEXT("LearningFileSystem"));
	}

	void CleanLearningFileSystemRoot()
	{
		IFileManager::Get().DeleteDirectory(*GetLearningFileSystemRoot(), false, true);
	}

	bool WriteLearningFile(const FString& RelativePath, const FString& Content, FString& OutAbsolutePath)
	{
		OutAbsolutePath = FPaths::Combine(GetLearningFileSystemRoot(), RelativePath);
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(OutAbsolutePath), true);
		return FFileHelper::SaveStringToFile(Content, *OutAbsolutePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}

	void AppendDiscoveredFiles(TArray<FString>& OutLines, const TArray<FAngelscriptEngine::FFilenamePair>& Files)
	{
		for (const FAngelscriptEngine::FFilenamePair& File : Files)
		{
			OutLines.Add(FString::Printf(TEXT("%s -> %s"), *File.RelativePath.Replace(TEXT("\\"), TEXT("/")), *File.AbsolutePath));
		}
	}
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAngelscriptLearningFileSystemAndModuleTraceTest,
	"Angelscript.TestModule.Learning.Runtime.FileSystemAndModuleResolution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FAngelscriptLearningFileSystemAndModuleTraceTest::RunTest(const FString& Parameters)
{
	using namespace AngelscriptTest_Learning_Runtime_AngelscriptLearningFileSystemAndModuleTraceTests_Private;
	CleanLearningFileSystemRoot();
	FAngelscriptEngine& Engine = ASTEST_CREATE_ENGINE();
	{ FAngelscriptEngineScope _AutoEngineScope(Engine);
	ON_SCOPE_EXIT
	{
		ASTEST_RESET_ENGINE(Engine);
		CleanLearningFileSystemRoot();
	};

	FAngelscriptLearningTraceSinkConfig SinkConfig;
	SinkConfig.bEmitToAutomation = true;
	SinkConfig.bEmitToLog = true;
	SinkConfig.bEmitToFile = false;

	FAngelscriptLearningTraceSession Trace(TEXT("LearningFileSystemAndModuleResolution"), SinkConfig);
	Trace.BeginPhase(EAngelscriptLearningTracePhase::Compile);

	const FString GoodSource = TEXT(R"AS(
int SurvivorEntry()
{
	return 99;
}
)AS");
	const FString BrokenSource = TEXT(R"AS(
int BrokenEntry()
{
	return MissingValue;
}
)AS");

	FString GoodAbsolutePath;
	FString BrokenAbsolutePath;
	FString GameplayAbsolutePath;
	FString ExampleAbsolutePath;
	FString EditorAbsolutePath;
	if (!TestTrue(TEXT("Write good learning file-system module should succeed"), WriteLearningFile(TEXT("Game/AI/Survivor.as"), GoodSource, GoodAbsolutePath))
		|| !TestTrue(TEXT("Write broken learning file-system module should succeed"), WriteLearningFile(TEXT("Bad/Broken.as"), BrokenSource, BrokenAbsolutePath))
		|| !TestTrue(TEXT("Write gameplay discovery file should succeed"), WriteLearningFile(TEXT("Gameplay/Main.as"), TEXT("int GameplayEntry() { return 1; }"), GameplayAbsolutePath))
		|| !TestTrue(TEXT("Write examples discovery file should succeed"), WriteLearningFile(TEXT("Examples/ExampleOnly.as"), TEXT("int ExampleEntry() { return 2; }"), ExampleAbsolutePath))
		|| !TestTrue(TEXT("Write editor discovery file should succeed"), WriteLearningFile(TEXT("Editor/EditorOnly.as"), TEXT("int EditorEntry() { return 3; }"), EditorAbsolutePath)))
	{
		return false;
	}

	const FString GoodModuleName = TEXT("Game.AI.Survivor");
	const bool bGoodCompiled = CompileModuleFromMemory(&Engine, *GoodModuleName, GoodAbsolutePath, GoodSource);
	Trace.AddStep(TEXT("CompileFromDiskPath"), bGoodCompiled ? TEXT("Loaded the on-disk script and compiled it into a named runtime module") : TEXT("Failed to compile the on-disk survivor module"));
	Trace.AddKeyValue(TEXT("AbsolutePath"), GoodAbsolutePath);
	Trace.AddKeyValue(TEXT("ModuleName"), GoodModuleName);
	Trace.AddKeyValue(TEXT("CompileResult"), bGoodCompiled ? TEXT("true") : TEXT("false"));

	TSharedPtr<FAngelscriptModuleDesc> ModuleByName = Engine.GetModule(GoodModuleName);
	TSharedPtr<FAngelscriptModuleDesc> ModuleByFilename = Engine.GetModuleByFilename(GoodAbsolutePath);
	TSharedPtr<FAngelscriptModuleDesc> ModuleByEither = Engine.GetModuleByFilenameOrModuleName(GoodAbsolutePath, GoodModuleName);
	Trace.AddStep(TEXT("ModuleLookup"), TEXT("Queried the active module table by logical module name, by absolute filename, and by the fallback helper that accepts both"));
	Trace.AddKeyValue(TEXT("LookupByName"), ModuleByName.IsValid() ? TEXT("true") : TEXT("false"));
	Trace.AddKeyValue(TEXT("LookupByFilename"), ModuleByFilename.IsValid() ? TEXT("true") : TEXT("false"));
	Trace.AddKeyValue(TEXT("LookupByEither"), ModuleByEither.IsValid() ? TEXT("true") : TEXT("false"));

	int32 SurvivorResult = 0;
	const bool bSurvivorExecuted = ExecuteIntFunction(&Engine, *GoodModuleName, TEXT("int SurvivorEntry()"), SurvivorResult);
	Trace.AddKeyValue(TEXT("SurvivorResult"), bSurvivorExecuted ? FString::FromInt(SurvivorResult) : TEXT("<execute failed>"));

	FAngelscriptCompileTraceSummary BrokenSummary;
	const bool bBrokenCompiled = CompileModuleWithSummary(&Engine, ECompileType::SoftReloadOnly, TEXT("Bad.Broken"), BrokenAbsolutePath, BrokenSource, false, BrokenSummary, true);
	Trace.AddStep(TEXT("PartialFailureIsolation"), TEXT("Attempted to compile a broken second module while keeping the already-loaded survivor module available for lookup and execution"));
	Trace.AddKeyValue(TEXT("BrokenCompileSucceeded"), bBrokenCompiled ? TEXT("true") : TEXT("false"));
	Trace.AddKeyValue(TEXT("BrokenDiagnostics"), FString::FromInt(BrokenSummary.Diagnostics.Num()));

	TSharedPtr<FAngelscriptModuleDesc> SurvivorAfterFailure = Engine.GetModuleByFilenameOrModuleName(GoodAbsolutePath, GoodModuleName);
	Trace.AddKeyValue(TEXT("SurvivorStillDiscoverable"), SurvivorAfterFailure.IsValid() ? TEXT("true") : TEXT("false"));

	TArray<FAngelscriptEngine::FFilenamePair> DiscoveryWithEditorScripts;
	TArray<FAngelscriptEngine::FFilenamePair> DiscoveryWithoutEditorScripts;
	const TArray<FString> PreviousRoots = Engine.AllRootPaths;
	{
		TGuardValue<bool> UseEditorScriptsGuard(Engine.bUseEditorScripts, true);
		Engine.AllRootPaths = {GetLearningFileSystemRoot()};
		Engine.FindAllScriptFilenames(DiscoveryWithEditorScripts);
	}
	{
		TGuardValue<bool> UseEditorScriptsGuard(Engine.bUseEditorScripts, false);
		Engine.AllRootPaths = {GetLearningFileSystemRoot()};
		Engine.FindAllScriptFilenames(DiscoveryWithoutEditorScripts);
	}
	Engine.AllRootPaths = PreviousRoots;

	Trace.AddStep(TEXT("DiscoveryAndSkipRules"), TEXT("Enumerated script roots once with editor scripts enabled and once with skip rules active so the filtered relative paths become visible"));
	Trace.AddKeyValue(TEXT("DiscoveredWithEditorScripts"), FString::FromInt(DiscoveryWithEditorScripts.Num()));
	Trace.AddKeyValue(TEXT("DiscoveredWithoutEditorScripts"), FString::FromInt(DiscoveryWithoutEditorScripts.Num()));

	TArray<FString> DiscoveryWithEditorLines;
	TArray<FString> DiscoveryWithoutEditorLines;
	AppendDiscoveredFiles(DiscoveryWithEditorLines, DiscoveryWithEditorScripts);
	AppendDiscoveredFiles(DiscoveryWithoutEditorLines, DiscoveryWithoutEditorScripts);
	if (DiscoveryWithEditorLines.Num() > 0)
	{
		Trace.AddCodeBlock(FormatLearningTraceStringList(DiscoveryWithEditorLines, TEXT("DiscoveryWithEditorScripts")));
	}
	if (DiscoveryWithoutEditorLines.Num() > 0)
	{
		Trace.AddCodeBlock(FormatLearningTraceStringList(DiscoveryWithoutEditorLines, TEXT("DiscoveryWithoutEditorScripts")));
	}

	const bool bGoodModuleCompiled = TestTrue(TEXT("Good file-system module should compile from its disk path"), bGoodCompiled);
	const bool bLookupByName = TestTrue(TEXT("Module lookup by name should succeed"), ModuleByName.IsValid());
	const bool bLookupByFilename = TestTrue(TEXT("Module lookup by filename should succeed"), ModuleByFilename.IsValid());
	const bool bLookupByEither = TestTrue(TEXT("Module lookup by filename-or-module should succeed"), ModuleByEither.IsValid());
	const bool bExecuteGoodModule = TestTrue(TEXT("Good file-system module should still execute after compile"), bSurvivorExecuted);
	const bool bExpectedReturnValue = TestEqual(TEXT("Good file-system module should return the expected value"), SurvivorResult, 99);
	const bool bBrokenModuleRejected = TestFalse(TEXT("Broken file-system module should fail to compile"), bBrokenCompiled);
	const bool bBrokenDiagnosticsPresent = TestTrue(TEXT("Broken file-system module should produce diagnostics"), BrokenSummary.Diagnostics.Num() > 0);
	const bool bSurvivorPreserved = TestTrue(TEXT("Good module should still be discoverable after the broken module attempt"), SurvivorAfterFailure.IsValid());
	const bool bDiscoveryCounts = TestTrue(TEXT("Discovery with editor scripts should find more files than skip-rule discovery"), DiscoveryWithEditorScripts.Num() > DiscoveryWithoutEditorScripts.Num());
	const bool bSkipRulesKeepGameplayOnly = TestTrue(TEXT("Skip rules should keep Gameplay/Main.as when editor scripts are disabled"), DiscoveryWithoutEditorLines.ContainsByPredicate([](const FString& Line)
	{
		return Line.StartsWith(TEXT("Gameplay/Main.as ->"));
	}));
	const bool bSkipRulesFilterEditorExamples = TestTrue(TEXT("Skip rules should filter Examples and Editor scripts when editor scripts are disabled"), !DiscoveryWithoutEditorLines.ContainsByPredicate([](const FString& Line)
	{
		return Line.StartsWith(TEXT("Examples/")) || Line.StartsWith(TEXT("Editor/"));
	}));
	const bool bPhaseSequenceOk = AssertLearningTracePhaseSequence(*this, Trace.GetEvents(), {
		EAngelscriptLearningTracePhase::Compile,
	});
	const bool bContainsLookupKeyword = AssertLearningTraceContainsKeyword(*this, Trace.GetEvents(), TEXT("ModuleLookup"));
	const bool bContainsSkipRulesKeyword = AssertLearningTraceContainsKeyword(*this, Trace.GetEvents(), TEXT("DiscoveredWithoutEditorScripts"));
	const bool bMinimumEventsOk = AssertLearningTraceMinimumEventCount(*this, Trace.GetEvents(), 4);

	Trace.FlushToAutomation(*this);
	Trace.FlushToLog();
	return bGoodModuleCompiled
		&& bLookupByName
		&& bLookupByFilename
		&& bLookupByEither
		&& bExecuteGoodModule
		&& bExpectedReturnValue
		&& bBrokenModuleRejected
		&& bBrokenDiagnosticsPresent
		&& bSurvivorPreserved
		&& bDiscoveryCounts
		&& bSkipRulesKeepGameplayOnly
		&& bSkipRulesFilterEditorExamples
		&& bPhaseSequenceOk
		&& bContainsLookupKeyword
		&& bContainsSkipRulesKeyword
		&& bMinimumEventsOk;

	}
}

#endif
