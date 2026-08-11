#include "Cache/AngelscriptCacheDiagnostics.h"

#include "AngelscriptEngine.h"
#include "CQTest.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/StringOutputDevice.h"
#include "Shared/AngelscriptTestEngineAcquisition.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheFlushApiTests,
	"Angelscript.TestModule.Cache.FlushApi",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	class FScopedProjectRoot final
	{
	public:
		FScopedProjectRoot()
		{
			Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(
				FPaths::ProjectSavedDir(),
				TEXT("Automation/AngelscriptCacheFlushApi"),
				FGuid::NewGuid().ToString(EGuidFormats::Digits)));
			FPaths::NormalizeDirectoryName(Root);
			check(Root.Contains(
				TEXT("/Saved/Automation/AngelscriptCacheFlushApi/")));
			ScriptRoot = Root / TEXT("Script");
			CacheRoot = Root / TEXT("CacheV2");
			check(IFileManager::Get().MakeDirectory(*ScriptRoot, true));
		}

		~FScopedProjectRoot()
		{
			if (Root.Contains(
				TEXT("/Saved/Automation/AngelscriptCacheFlushApi/")))
			{
				IFileManager::Get().DeleteDirectory(*Root, false, true);
			}
		}

		bool WriteSource(const FString& Source) const
		{
			return FFileHelper::SaveStringToFile(
				Source, *(ScriptRoot / TEXT("FlushApi.as")));
		}

		FString Root;
		FString ScriptRoot;
		FString CacheRoot;
	};

	static TUniquePtr<FAngelscriptEngine> CreateEngine(
		const FScopedProjectRoot& Project)
	{
		FAngelscriptEngineConfig Config;
		Config.bIsEditor = true;
		Config.bDevelopmentMode = true;
		Config.bSkipThreadedInitialize = true;
		Config.CacheV2RootOverride = Project.CacheRoot;

		FAngelscriptEngineDependencies Dependencies =
			FAngelscriptEngineDependencies::CreateDefault();
		Dependencies.GetProjectDir = [&Project]()
		{
			return Project.Root;
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

public:
	TEST_METHOD(ExplicitEngineFlushCommitsCurrentAndConsoleUsesSameControl)
	{
		FScopedProjectRoot Project;
		const FString Unique = FGuid::NewGuid().ToString(EGuidFormats::Digits);
		const FString Source = FString::Printf(TEXT(R"AS(
enum ECacheFlushApi%sState
{
	Ready = 1,
}

int ReadCacheFlushApi%sValue()
{
	return 401;
}
)AS"), *Unique, *Unique);
		ASSERT_THAT(IsTrue(Project.WriteSource(Source)));

		TUniquePtr<FAngelscriptEngine> Engine = CreateEngine(Project);
		ASSERT_THAT(IsNotNull(Engine.Get()));
		FAngelscriptEngineScope EngineScope(*Engine);
		Engine->InitialCompile();

		const FAngelscriptCacheFlushApiResult Flush =
			FlushAngelscriptCacheToStore(Engine.Get());
		TestRunner->AddInfo(FString::Printf(
			TEXT("V6.3 public Cache flush: ApiError=%u LifecycleError=%u CurrentAttempted=%d CurrentCommit=%u CurrentGeneration=%s Detail=%s"),
			static_cast<uint32>(Flush.Error),
			static_cast<uint32>(Flush.Flush.Error),
			Flush.Flush.Current.bAttempted ? 1 : 0,
			static_cast<uint32>(Flush.Flush.Current.Publication.CommitState),
			*Flush.Flush.Current.GenerationId.ToHexString(),
			*Flush.Detail));
		ASSERT_THAT(IsTrue(Flush.IsSuccess()));
		ASSERT_THAT(IsTrue(Flush.Flush.Current.bAttempted));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreCommitState::CurrentCommitted,
			Flush.Flush.Current.Publication.CommitState));

		TUniquePtr<IAngelscriptCacheAtomicFileOps> FileOps =
			CreateAngelscriptCacheAtomicFileOps();
		ASSERT_THAT(IsNotNull(FileOps.Get()));
		const FAngelscriptCacheLifecyclePublications Publications =
			Engine->GetCacheService()->GetLifecyclePublications();
		ASSERT_THAT(IsTrue(Publications.Current.IsValid()));
		FAngelscriptCacheStorePaths Paths;
		ASSERT_THAT(IsTrue(BuildAngelscriptCacheStorePaths(
			Project.CacheRoot,
			Publications.Current->Compatibility,
			Publications.Current->Context,
			*FileOps,
			Paths).IsSuccess()));
		TOptional<FAngelscriptHash256> CurrentGeneration;
		ASSERT_THAT(IsTrue(ReadAngelscriptCachePointerSlot(
			Paths,
			EAngelscriptCachePointerKind::Current,
			*FileOps,
			CurrentGeneration).IsSuccess()));
		ASSERT_THAT(IsTrue(CurrentGeneration.IsSet()));
		ASSERT_THAT(IsTrue(
			CurrentGeneration.GetValue() == Flush.Flush.Current.GenerationId));

		FStringOutputDevice ConsoleOutput;
		ASSERT_THAT(IsTrue(
			IConsoleManager::Get().ProcessUserConsoleInput(
				TEXT("as.Cache.Flush"), ConsoleOutput, nullptr)));
		ASSERT_THAT(IsTrue(ConsoleOutput.Contains(
			TEXT("as.Cache.Flush succeeded"))));
	}

	TEST_METHOD(MissingEngineIsTypedAndFlushCommandIsRegistered)
	{
		const FAngelscriptCacheFlushApiResult Missing =
			FlushAngelscriptCacheToStore(nullptr);
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheDiagnosticApiError::EngineUnavailable,
			Missing.Error));
		ASSERT_THAT(IsFalse(Missing.IsSuccess()));
		ASSERT_THAT(IsFalse(Missing.Detail.IsEmpty()));

		IConsoleObject* FlushCommand =
			IConsoleManager::Get().FindConsoleObject(TEXT("as.Cache.Flush"));
		ASSERT_THAT(IsNotNull(FlushCommand));
		ASSERT_THAT(IsNotNull(FlushCommand->AsCommand()));
	}
};

#endif
