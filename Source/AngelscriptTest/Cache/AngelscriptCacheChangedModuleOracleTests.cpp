#include "CQTest.h"

#include "Compilation/AngelscriptCompilationEvents.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Preprocessor/AngelscriptPreprocessor.h"
#include "Shared/AngelscriptTestFixture.h"

#if WITH_ANGELSCRIPT_UNITTESTS

struct FAngelscriptCacheChangedModuleTestAccess
{
	static bool PerformHotReload(
		FAngelscriptEngine& Engine,
		const ECompileType CompileType,
		const TArray<FAngelscriptEngine::FFilenamePair>& Files)
	{
		return Engine.PerformHotReload(CompileType, Files);
	}
};

namespace AngelscriptCacheChangedModuleOracleTests_Private
{
	class FScopedDiskRoot final
	{
	public:
		FScopedDiskRoot()
		{
			Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(
				FPaths::ProjectSavedDir(),
				TEXT("Automation/AngelscriptCacheChangedModuleOracle"),
				FGuid::NewGuid().ToString(EGuidFormats::Digits)));
			FPaths::NormalizeDirectoryName(Root);
			check(Root.Contains(
				TEXT("/Saved/Automation/AngelscriptCacheChangedModuleOracle/")));
			check(IFileManager::Get().MakeDirectory(*Root, true));
		}

		~FScopedDiskRoot()
		{
			if (Root.Contains(
				TEXT("/Saved/Automation/AngelscriptCacheChangedModuleOracle/")))
			{
				IFileManager::Get().DeleteDirectory(*Root, false, true);
			}
		}

		FString Root;
	};

	static FString MakeSource(const int32 Answer)
	{
		return FString::Printf(TEXT(R"AS(
int Answer()
{
	return %d;
}
)AS"), Answer);
	}

	static bool WriteSource(
		const FString& AbsoluteFilename,
		const int32 Answer)
	{
		return FFileHelper::SaveStringToFile(
			MakeSource(Answer),
			*AbsoluteFilename,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}

	static bool IsHandled(const ECompileResult Result)
	{
		return Result == ECompileResult::FullyHandled
			|| Result == ECompileResult::PartiallyHandled;
	}

	static TSharedPtr<FAngelscriptModuleDesc> CompileDiskModule(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		const FString& RelativeFilename,
		const FString& AbsoluteFilename,
		const FAngelscriptCompileOptions& CompileOptions = {})
	{
		FAngelscriptPreprocessor Preprocessor;
		Preprocessor.AddFile(RelativeFilename, AbsoluteFilename);
		if (!Preprocessor.Preprocess())
		{
			Test.AddError(TEXT("Changed-module oracle preprocessing failed"));
			return {};
		}

		TArray<TSharedRef<FAngelscriptModuleDesc>> Modules =
			Preprocessor.GetModulesToCompile();
		if (Modules.Num() != 1)
		{
			Test.AddError(FString::Printf(
				TEXT("Changed-module oracle expected one module, got %d"),
				Modules.Num()));
			return {};
		}

		TArray<TSharedRef<FAngelscriptModuleDesc>> CompiledModules;
		TGuardValue<bool> AutomaticImportGuard(
			Engine.bUseAutomaticImportMethod, false);
		FScopedAutomaticImportsOverride AutomaticImportsOverride(
			Engine.GetScriptEngine());
		const ECompileResult Result = Engine.CompileModules(
			ECompileType::Initial,
			Modules,
			CompiledModules,
			CompileOptions);
		Test.AddInfo(FString::Printf(
			TEXT("Changed-module clean compile: Result=%u Policy=%u Modules=%d LoadedLegacy=%d LoadedIncremental=%d"),
			static_cast<uint32>(Result),
			static_cast<uint32>(CompileOptions.CachePolicy),
			CompiledModules.Num(),
			CompiledModules.Num() == 1
				&& CompiledModules[0]->bLoadedPrecompiledCode ? 1 : 0,
			CompiledModules.Num() == 1
				&& CompiledModules[0]->bLoadedIncrementalCache ? 1 : 0));
		if (!IsHandled(Result) || CompiledModules.Num() != 1)
		{
			return {};
		}
		return CompiledModules[0];
	}

	static bool ExecuteAnswer(
		FAutomationTestBase& Test,
		FAngelscriptTestFixture& Fixture,
		const FString& ModuleName,
		int32& OutAnswer)
	{
		const TSharedPtr<FAngelscriptModuleDesc> Module =
			Fixture.GetEngine().GetModuleByModuleName(ModuleName);
		if (!Module.IsValid() || Module->ScriptModule == nullptr)
		{
			Test.AddError(FString::Printf(
				TEXT("Changed-module oracle cannot find active module %s"),
				*ModuleName));
			return false;
		}
		asIScriptFunction* Function =
			Module->ScriptModule->GetFunctionByDecl("int Answer()");
		if (Function == nullptr)
		{
			Test.AddError(TEXT("Changed-module oracle cannot find int Answer()"));
			return false;
		}
		return Fixture.ExecuteInt(*Function, OutAnswer);
	}

	static int32 CountEvents(
		const TArray<FAngelscriptCompilationEvent>& Events,
		const EAngelscriptCompilationEventType Type)
	{
		int32 Count = 0;
		for (const FAngelscriptCompilationEvent& Event : Events)
		{
			if (Event.Type == Type)
			{
				++Count;
			}
		}
		return Count;
	}

	static bool HasForcedCleanCompileRun(
		const TArray<FAngelscriptCompilationEvent>& Events)
	{
		for (const FAngelscriptCompilationEvent& Event : Events)
		{
			if (Event.Type == EAngelscriptCompilationEventType::CompileBegin
				&& Event.CachePolicy
					== EAngelscriptCompileCachePolicy::ForceClean)
			{
				return true;
			}
		}
		return false;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheChangedModuleOracleTests,
	"Angelscript.TestModule.Cache.ChangedModuleOracle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(ChangedDiskSourceUsesHotReloadOwnedForcedCleanPipeline)
	{
		using namespace AngelscriptCacheChangedModuleOracleTests_Private;
		FScopedDiskRoot Disk;
		const FString RelativeFilename = TEXT("ChangedModuleOracle.as");
		const FString AbsoluteFilename = Disk.Root / RelativeFilename;
		ASSERT_THAT(IsTrue(WriteSource(AbsoluteFilename, 41)));

		FAngelscriptTestFixture Fixture(
			*TestRunner, ETestEngineMode::IsolatedFull);
		ASSERT_THAT(IsTrue(Fixture.IsValid()));
		FAngelscriptEngine& Engine = Fixture.GetEngine();
		const TSharedPtr<FAngelscriptModuleDesc> Initial = CompileDiskModule(
			*TestRunner, Engine, RelativeFilename, AbsoluteFilename);
		ASSERT_THAT(IsTrue(Initial.IsValid()));
		const FString ModuleName = Initial->ModuleName;
		int32 Answer = 0;
		ASSERT_THAT(IsTrue(ExecuteAnswer(
			*TestRunner, Fixture, ModuleName, Answer)));
		ASSERT_THAT(AreEqual(41, Answer));

		Initial->bLoadedIncrementalCache = true;
		ASSERT_THAT(IsTrue(WriteSource(AbsoluteFilename, 42)));
		TArray<FAngelscriptCompilationEvent> Events;
		const FDelegateHandle Listener =
			FAngelscriptCompilationEvents::RegisterListener(
				[&Events](const FAngelscriptCompilationEvent& Event)
				{
					Events.Add(Event);
				});
		ON_SCOPE_EXIT
		{
			FAngelscriptCompilationEvents::UnregisterListener(Listener);
		};

		TGuardValue<bool> AutomaticImportGuard(
			Engine.bUseAutomaticImportMethod, false);
		FScopedAutomaticImportsOverride AutomaticImportsOverride(
			Engine.GetScriptEngine());
		const TArray<FAngelscriptEngine::FFilenamePair> ChangedFiles{
			FAngelscriptEngine::FFilenamePair{
				AbsoluteFilename,
				RelativeFilename,
				TEXT("/Angelscript/Game/ChangedModuleOracle.as")}};
		ASSERT_THAT(IsTrue(
			FAngelscriptCacheChangedModuleTestAccess::PerformHotReload(
				Engine,
				ECompileType::SoftReloadOnly,
				ChangedFiles)));

		Answer = 0;
		ASSERT_THAT(IsTrue(ExecuteAnswer(
			*TestRunner, Fixture, ModuleName, Answer)));
		ASSERT_THAT(AreEqual(42, Answer));
		const TSharedPtr<FAngelscriptModuleDesc> Reloaded =
			Engine.GetModuleByModuleName(ModuleName);
		ASSERT_THAT(IsTrue(Reloaded.IsValid()));
		ASSERT_THAT(IsFalse(Reloaded->bLoadedPrecompiledCode));
		ASSERT_THAT(IsFalse(Reloaded->bLoadedIncrementalCache));
		ASSERT_THAT(IsTrue(HasForcedCleanCompileRun(Events)));
		ASSERT_THAT(IsTrue(CountEvents(
			Events,
			EAngelscriptCompilationEventType::PreprocessProcessChunks) > 0));
		ASSERT_THAT(IsTrue(CountEvents(
			Events,
			EAngelscriptCompilationEventType::CompileModuleParse) > 0));
		ASSERT_THAT(IsTrue(CountEvents(
			Events,
			EAngelscriptCompilationEventType::CompileModuleCompileCode) > 0));
		TestRunner->AddInfo(FString::Printf(
			TEXT("Changed-module hot reload: Module=%s Answer=%d Preprocess=%d Parse=%d CompileCode=%d ForcedClean=%d"),
			*ModuleName,
			Answer,
			CountEvents(Events,
				EAngelscriptCompilationEventType::PreprocessProcessChunks),
			CountEvents(Events,
				EAngelscriptCompilationEventType::CompileModuleParse),
			CountEvents(Events,
				EAngelscriptCompilationEventType::CompileModuleCompileCode),
			HasForcedCleanCompileRun(Events) ? 1 : 0));
	}

	TEST_METHOD(ForcedCleanCompileClearsIncrementalArtifactHint)
	{
		using namespace AngelscriptCacheChangedModuleOracleTests_Private;
		FScopedDiskRoot Disk;
		const FString RelativeFilename = TEXT("ForcedCleanHint.as");
		const FString AbsoluteFilename = Disk.Root / RelativeFilename;
		ASSERT_THAT(IsTrue(WriteSource(AbsoluteFilename, 73)));

		FAngelscriptTestFixture Fixture(
			*TestRunner, ETestEngineMode::IsolatedFull);
		ASSERT_THAT(IsTrue(Fixture.IsValid()));
		FAngelscriptPreprocessor Preprocessor;
		Preprocessor.AddFile(RelativeFilename, AbsoluteFilename);
		ASSERT_THAT(IsTrue(Preprocessor.Preprocess()));
		TArray<TSharedRef<FAngelscriptModuleDesc>> Modules =
			Preprocessor.GetModulesToCompile();
		ASSERT_THAT(AreEqual(1, Modules.Num()));
		Modules[0]->bLoadedIncrementalCache = true;

		TArray<FAngelscriptCompilationEvent> Events;
		const FDelegateHandle Listener =
			FAngelscriptCompilationEvents::RegisterListener(
				[&Events](const FAngelscriptCompilationEvent& Event)
				{
					Events.Add(Event);
				});
		ON_SCOPE_EXIT
		{
			FAngelscriptCompilationEvents::UnregisterListener(Listener);
		};
		FAngelscriptCompileOptions CompileOptions;
		CompileOptions.CachePolicy =
			EAngelscriptCompileCachePolicy::ForceClean;
		TArray<TSharedRef<FAngelscriptModuleDesc>> CompiledModules;
		TGuardValue<bool> AutomaticImportGuard(
			Fixture.GetEngine().bUseAutomaticImportMethod, false);
		FScopedAutomaticImportsOverride AutomaticImportsOverride(
			Fixture.GetEngine().GetScriptEngine());
		const ECompileResult Result = Fixture.GetEngine().CompileModules(
			ECompileType::Initial,
			Modules,
			CompiledModules,
			CompileOptions);
		ASSERT_THAT(IsTrue(IsHandled(Result)));
		ASSERT_THAT(AreEqual(1, CompiledModules.Num()));
		ASSERT_THAT(IsFalse(CompiledModules[0]->bLoadedIncrementalCache));
		ASSERT_THAT(IsFalse(CompiledModules[0]->bLoadedPrecompiledCode));
		ASSERT_THAT(IsTrue(HasForcedCleanCompileRun(Events)));
		ASSERT_THAT(IsTrue(CountEvents(
			Events,
			EAngelscriptCompilationEventType::CompileModuleParse) > 0));
		ASSERT_THAT(IsTrue(CountEvents(
			Events,
			EAngelscriptCompilationEventType::CompileModuleCompileCode) > 0));

		int32 Answer = 0;
		ASSERT_THAT(IsTrue(ExecuteAnswer(
			*TestRunner,
			Fixture,
			CompiledModules[0]->ModuleName,
			Answer)));
		ASSERT_THAT(AreEqual(73, Answer));
	}
};

#endif
