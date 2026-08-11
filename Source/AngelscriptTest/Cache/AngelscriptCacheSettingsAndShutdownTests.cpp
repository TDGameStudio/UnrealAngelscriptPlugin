#include "Cache/AngelscriptCacheSettings.h"
#include "Cache/AngelscriptCacheService.h"
#include "Cache/AngelscriptCacheDiagnostics.h"

#include "CQTest.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Shared/AngelscriptTestEngineAcquisition.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptCacheSettingsAndShutdownTests_Private
{
	class FScopedCommandLine final
	{
	public:
		explicit FScopedCommandLine(const TCHAR* Replacement)
			: Original(FCommandLine::Get())
		{
			FCommandLine::Set(Replacement);
		}

		~FScopedCommandLine()
		{
			FCommandLine::Set(*Original);
		}

	private:
		FString Original;
	};

	class FScopedProjectRoot final
	{
	public:
		FScopedProjectRoot()
		{
			Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(
				FPaths::ProjectSavedDir(),
				TEXT("Automation/AngelscriptCacheSettingsAndShutdown"),
				FGuid::NewGuid().ToString(EGuidFormats::Digits)));
			FPaths::NormalizeDirectoryName(Root);
			check(Root.Contains(
				TEXT("/Saved/Automation/AngelscriptCacheSettingsAndShutdown/")));
			ScriptRoot = Root / TEXT("Script");
			CacheRoot = Root / TEXT("CacheV2");
			check(IFileManager::Get().MakeDirectory(*ScriptRoot, true));
		}

		~FScopedProjectRoot()
		{
			if (Root.Contains(
				TEXT("/Saved/Automation/AngelscriptCacheSettingsAndShutdown/")))
			{
				IFileManager::Get().DeleteDirectory(*Root, false, true);
			}
		}

		bool WriteSource(const FString& Source) const
		{
			return FFileHelper::SaveStringToFile(
				Source, *(ScriptRoot / TEXT("Shutdown.as")));
		}

		FString Root;
		FString ScriptRoot;
		FString CacheRoot;
	};

	static TUniquePtr<FAngelscriptEngine> CreateEngine(
		const FString& ProjectRoot,
		const FString& CacheRoot,
		const FString& ReportPath = FString())
	{
		FAngelscriptEngineConfig Config;
		Config.bIsEditor = true;
		Config.bDevelopmentMode = true;
		Config.bSkipThreadedInitialize = true;
		Config.CacheV2RootOverride = CacheRoot;
		Config.CacheV2ReportPathOverride = ReportPath;

		FAngelscriptEngineDependencies Dependencies =
			FAngelscriptEngineDependencies::CreateDefault();
		Dependencies.GetProjectDir = [ProjectRoot]()
		{
			return ProjectRoot;
		};
		Dependencies.GetEnabledPluginScriptRoots = []()
		{
			return TArray<FString>();
		};
		Dependencies.GetEnabledPluginScriptRootDescriptors = []()
		{
			return TArray<FAngelscriptPluginScriptRoot>();
		};
		return CreateScriptScanFreeFullEngineForTesting(Config, Dependencies);
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheSettingsAndShutdownTests,
	"Angelscript.TestModule.Cache.SettingsAndShutdown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(DefaultSettingsEnableCacheButDisablePackagedAutomaticReload)
	{
		const UAngelscriptCacheSettings* Settings =
			GetDefault<UAngelscriptCacheSettings>();
		ASSERT_THAT(IsNotNull(Settings));
		ASSERT_THAT(IsTrue(Settings->bEnableCacheV2));
		ASSERT_THAT(AreEqual(
			EAngelscriptPackagedRuntimeReloadMode::Disabled,
			Settings->PackagedRuntimeReloadMode));
		ASSERT_THAT(IsNear(1.0,
			static_cast<double>(Settings->RuntimeReloadScanIntervalSeconds),
			0.0001));
		ASSERT_THAT(IsNear(5.0,
			static_cast<double>(Settings->ShutdownFlushTimeoutSeconds),
			0.0001));
	}

	TEST_METHOD(ProcessConfigCanEnableBoundedTraceBeforeStartupSelection)
	{
		using namespace AngelscriptCacheSettingsAndShutdownTests_Private;
		FScopedCommandLine CommandLine(
			TEXT("-as-cache-trace -as-cache-trace-capacity=37"));
		const FAngelscriptEngineConfig Config =
			FAngelscriptEngineConfig::FromCurrentProcess();
		ASSERT_THAT(IsTrue(Config.bForceEnableCacheV2DecisionTrace));
		ASSERT_THAT(AreEqual(
			static_cast<uint32>(37),
			Config.CacheV2DecisionTraceCapacityOverride));

		FAngelscriptEngineDependencies Dependencies =
			FAngelscriptEngineDependencies::CreateDefault();
		FAngelscriptEngine Engine(Config, Dependencies);
		const FAngelscriptCacheDecisionTraceSnapshot Trace =
			Engine.GetCacheService()->CaptureDecisionTrace();
		ASSERT_THAT(IsTrue(Trace.bEnabled));
		ASSERT_THAT(AreEqual(static_cast<uint32>(37), Trace.Capacity));
	}

	TEST_METHOD(EngineShutdownPublishesFrozenCurrentToConfiguredIsolatedStore)
	{
		using namespace AngelscriptCacheSettingsAndShutdownTests_Private;
		FScopedProjectRoot Project;
		const FString Unique = FGuid::NewGuid().ToString(EGuidFormats::Digits);
		const FString Source = FString::Printf(TEXT(R"AS(
enum ECacheShutdown%sState
{
	Ready = 1,
}

int ReadCacheShutdown%sValue()
{
	return 301;
}
)AS"), *Unique, *Unique);
		ASSERT_THAT(IsTrue(Project.WriteSource(Source)));

		TUniquePtr<FAngelscriptEngine> Engine =
			CreateEngine(Project.Root, Project.CacheRoot);
		ASSERT_THAT(IsNotNull(Engine.Get()));
		FAngelscriptEngineScope EngineScope(*Engine);
		Engine->InitialCompile();
		FAngelscriptCacheService* Service = Engine->GetCacheService();
		ASSERT_THAT(IsNotNull(Service));
		const FAngelscriptCacheLifecyclePublications Before =
			Service->GetLifecyclePublications();
		ASSERT_THAT(IsTrue(Before.Current.IsValid()));

		Engine->Shutdown();

		TUniquePtr<IAngelscriptCacheAtomicFileOps> FileOps =
			CreateAngelscriptCacheAtomicFileOps();
		ASSERT_THAT(IsNotNull(FileOps.Get()));
		FAngelscriptCacheStorePaths Paths;
		ASSERT_THAT(IsTrue(BuildAngelscriptCacheStorePaths(
			Project.CacheRoot,
			Before.Current->Compatibility,
			Before.Current->Context,
			*FileOps,
			Paths).IsSuccess()));
		TOptional<FAngelscriptHash256> CurrentGeneration;
		ASSERT_THAT(IsTrue(ReadAngelscriptCachePointerSlot(
			Paths,
			EAngelscriptCachePointerKind::Current,
			*FileOps,
			CurrentGeneration).IsSuccess()));
		ASSERT_THAT(IsTrue(CurrentGeneration.IsSet()));
		TestRunner->AddInfo(FString::Printf(
			TEXT("V6.3 Engine shutdown Store publication: Generation=%s"),
			*CurrentGeneration->ToHexString()));
	}

	TEST_METHOD(EngineShutdownWritesRequestedStableSessionJsonAfterFlush)
	{
		using namespace AngelscriptCacheSettingsAndShutdownTests_Private;
		FScopedProjectRoot Project;
		const FString Unique = FGuid::NewGuid().ToString(EGuidFormats::Digits);
		ASSERT_THAT(IsTrue(Project.WriteSource(FString::Printf(TEXT(R"AS(
enum ECacheShutdownReport%sState
{
	Ready = 1,
}

int ReadCacheShutdownReport%sValue()
{
	return 302;
}
)AS"), *Unique, *Unique))));

		const FString ReportPath = Project.Root / TEXT("Reports/session.json");
		TUniquePtr<FAngelscriptEngine> Engine =
			CreateEngine(Project.Root, Project.CacheRoot, ReportPath);
		ASSERT_THAT(IsNotNull(Engine.Get()));
		FAngelscriptEngineScope EngineScope(*Engine);
		Engine->InitialCompile();
		Engine->Shutdown();

		FString PersistedJson;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(
			PersistedJson, *ReportPath)));
		ASSERT_THAT(IsTrue(PersistedJson.Contains(
			TEXT("\"schemaVersion\":1"))));
		ASSERT_THAT(IsTrue(PersistedJson.Contains(
			TEXT("\"mutationPhaseName\":\"ShuttingDown\""))));
		ASSERT_THAT(IsTrue(PersistedJson.Contains(
			TEXT("\"current\":{\"present\":true"))));
		ASSERT_THAT(IsFalse(PersistedJson.Contains(TEXT("functionId"))));
		TestRunner->AddInfo(FString::Printf(
			TEXT("[CacheV2][ProcessReport] Path=%s Bytes=%d"),
			*ReportPath, PersistedJson.Len()));
	}

	TEST_METHOD(UninitializedEngineCannotOverwriteExistingProcessReport)
	{
		using namespace AngelscriptCacheSettingsAndShutdownTests_Private;
		FScopedProjectRoot Project;
		const FString ReportPath = Project.Root / TEXT("Reports/session.json");
		const FString ExistingReport =
			TEXT("{\"owner\":\"initialized-primary-engine\"}");
		ASSERT_THAT(IsTrue(FFileHelper::SaveStringToFile(
			ExistingReport, *ReportPath)));

		FAngelscriptEngineConfig Config;
		Config.bIsEditor = true;
		Config.bDevelopmentMode = true;
		Config.bSkipThreadedInitialize = true;
		Config.bDisableCacheV2Persistence = true;
		Config.CacheV2RootOverride = Project.CacheRoot;
		Config.CacheV2ReportPathOverride = ReportPath;
		FAngelscriptEngineDependencies Dependencies =
			FAngelscriptEngineDependencies::CreateDefault();
		FAngelscriptEngine UninitializedEngine(Config, Dependencies);
		ASSERT_THAT(IsNull(UninitializedEngine.GetScriptEngine()));

		UninitializedEngine.Shutdown();

		FString PersistedJson;
		ASSERT_THAT(IsTrue(FFileHelper::LoadFileToString(
			PersistedJson, *ReportPath)));
		ASSERT_THAT(AreEqual(ExistingReport, PersistedJson,
			TEXT("an Engine that never initialized cannot own the process report")));
	}
};

#endif
