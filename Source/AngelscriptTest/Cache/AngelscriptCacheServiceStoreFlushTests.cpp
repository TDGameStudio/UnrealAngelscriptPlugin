#include "Cache/AngelscriptCacheService.h"

#include "CQTest.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Shared/AngelscriptTestEngineAcquisition.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptCacheServiceStoreFlushTests_Private
{
	class FScopedProjectRoot final
	{
	public:
		FScopedProjectRoot()
		{
			Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(
				FPaths::ProjectSavedDir(),
				TEXT("Automation/AngelscriptCacheServiceStoreFlush"),
				FGuid::NewGuid().ToString(EGuidFormats::Digits)));
			FPaths::NormalizeDirectoryName(Root);
			check(Root.Contains(
				TEXT("/Saved/Automation/AngelscriptCacheServiceStoreFlush/")));
			ScriptRoot = Root / TEXT("Script");
			CacheRoot = Root / TEXT("CacheV2");
			check(IFileManager::Get().MakeDirectory(*ScriptRoot, true));
		}

		~FScopedProjectRoot()
		{
			if (Root.Contains(
				TEXT("/Saved/Automation/AngelscriptCacheServiceStoreFlush/")))
			{
				IFileManager::Get().DeleteDirectory(*Root, false, true);
			}
		}

		bool WriteSource(const FString& Source) const
		{
			return FFileHelper::SaveStringToFile(
				Source, *(ScriptRoot / TEXT("Flush.as")));
		}

		FString Root;
		FString ScriptRoot;
		FString CacheRoot;
	};

	static TUniquePtr<FAngelscriptEngine> CreateEngine(
		const FString& ProjectRoot)
	{
		FAngelscriptEngineConfig Config;
		Config.bIsEditor = true;
		Config.bDevelopmentMode = true;
		Config.bSkipThreadedInitialize = true;

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

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheServiceStoreFlushTests,
	"Angelscript.TestModule.Cache.ServiceStoreFlush",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(FrozenCurrentAndPendingPublishToTheirStoreSlotsIdempotently)
	{
		using namespace AngelscriptCacheServiceStoreFlushTests_Private;
		FScopedProjectRoot Project;
		const FString Unique = FGuid::NewGuid().ToString(EGuidFormats::Digits);
		const FString Source = FString::Printf(TEXT(R"AS(
enum ECacheServiceFlush%sState
{
	Ready = 1,
}

int ReadCacheServiceFlush%sValue()
{
	return 201;
}
)AS"), *Unique, *Unique);
		ASSERT_THAT(IsTrue(Project.WriteSource(Source)));

		TUniquePtr<FAngelscriptEngine> Engine = CreateEngine(Project.Root);
		ASSERT_THAT(IsNotNull(Engine.Get()));
		FAngelscriptEngineScope EngineScope(*Engine);
		Engine->InitialCompile();
		FAngelscriptCacheService* Service = Engine->GetCacheService();
		ASSERT_THAT(IsNotNull(Service));
		const FAngelscriptCacheLifecyclePublications Initial =
			Service->GetLifecyclePublications();
		ASSERT_THAT(IsTrue(Initial.Current.IsValid()));

		{
			FAngelscriptCacheMutationGuard Guard = Service->EnterMutation(
				EAngelscriptCacheMutationKind::RuntimeReload);
			ASSERT_THAT(IsTrue(Guard.IsEntered()));
			FAngelscriptCacheSuccessfulPublicationInput Pending;
			Pending.Kind = EAngelscriptCacheSuccessfulCompileKind::SoftReload;
			Pending.Disposition =
				EAngelscriptCachePublicationDisposition::PendingColdStart;
			Pending.Compatibility = Initial.Current->Compatibility;
			Pending.Context = Initial.Current->Context;
			Pending.Profile = Initial.Current->Profile;
			Pending.Modules = Initial.Current->Modules;
			const FAngelscriptCacheFreezePublicationResult Freeze =
				Service->FreezeSuccessfulCompileArtifacts(
					Guard.GetToken(), MoveTemp(Pending));
			ASSERT_THAT(IsTrue(Freeze.IsSuccess()));
		}

		const FAngelscriptCacheLifecycleFlushResult First =
			Service->FlushLifecyclePublicationsToStore(Project.CacheRoot, 5.0);
		TestRunner->AddInfo(FString::Printf(
			TEXT("V6.3 service Store flush first: Error=%u CurrentAttempted=%d CurrentPrepare=%u CurrentStore=%u CurrentCommit=%u CurrentGeneration=%s PendingAttempted=%d PendingPrepare=%u PendingStore=%u PendingCommit=%u PendingGeneration=%s Detail=%s"),
			static_cast<uint32>(First.Error),
			First.Current.bAttempted ? 1 : 0,
			static_cast<uint32>(First.Current.Preparation.Error),
			static_cast<uint32>(First.Current.Publication.Error),
			static_cast<uint32>(First.Current.Publication.CommitState),
			*First.Current.GenerationId.ToHexString(),
			First.PendingColdStart.bAttempted ? 1 : 0,
			static_cast<uint32>(First.PendingColdStart.Preparation.Error),
			static_cast<uint32>(First.PendingColdStart.Publication.Error),
			static_cast<uint32>(First.PendingColdStart.Publication.CommitState),
			*First.PendingColdStart.GenerationId.ToHexString(),
			*First.Detail));
		ASSERT_THAT(IsTrue(First.IsSuccess()));
		ASSERT_THAT(IsTrue(First.Current.bAttempted));
		ASSERT_THAT(IsTrue(First.PendingColdStart.bAttempted));
		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreCommitState::CurrentCommitted,
			First.Current.Publication.CommitState));
		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreCommitState::PendingCommitted,
			First.PendingColdStart.Publication.CommitState));
		ASSERT_THAT(IsTrue(
			First.Current.GenerationId == First.PendingColdStart.GenerationId));

		TUniquePtr<IAngelscriptCacheAtomicFileOps> FileOps =
			CreateAngelscriptCacheAtomicFileOps();
		ASSERT_THAT(IsNotNull(FileOps.Get()));
		FAngelscriptCacheStorePaths Paths;
		ASSERT_THAT(IsTrue(BuildAngelscriptCacheStorePaths(
			Project.CacheRoot,
			Initial.Current->Compatibility,
			Initial.Current->Context,
			*FileOps,
			Paths).IsSuccess()));
		TOptional<FAngelscriptHash256> CurrentGeneration;
		TOptional<FAngelscriptHash256> PendingGeneration;
		ASSERT_THAT(IsTrue(ReadAngelscriptCachePointerSlot(
			Paths,
			EAngelscriptCachePointerKind::Current,
			*FileOps,
			CurrentGeneration).IsSuccess()));
		ASSERT_THAT(IsTrue(ReadAngelscriptCachePointerSlot(
			Paths,
			EAngelscriptCachePointerKind::PendingColdStart,
			*FileOps,
			PendingGeneration).IsSuccess()));
		ASSERT_THAT(IsTrue(CurrentGeneration.IsSet()));
		ASSERT_THAT(IsTrue(PendingGeneration.IsSet()));
		ASSERT_THAT(IsTrue(
			CurrentGeneration.GetValue() == First.Current.GenerationId));
		ASSERT_THAT(IsTrue(
			PendingGeneration.GetValue()
				== First.PendingColdStart.GenerationId));

		const FAngelscriptCacheLifecycleFlushResult Second =
			Service->FlushLifecyclePublicationsToStore(Project.CacheRoot, 5.0);
		ASSERT_THAT(IsTrue(Second.IsSuccess()));
		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreCommitState::NotCommitted,
			Second.Current.Publication.CommitState));
		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreCommitState::NotCommitted,
			Second.PendingColdStart.Publication.CommitState));

		const FAngelscriptCacheLifecycleFlushResult ShutdownFlush =
			Service->BeginEngineShutdownAndFlushToStore(
				Project.CacheRoot, 5.0);
		TestRunner->AddInfo(FString::Printf(
			TEXT("V6.3 bounded shutdown flush: Error=%u Phase=%u CurrentCommit=%u PendingCommit=%u Detail=%s"),
			static_cast<uint32>(ShutdownFlush.Error),
			static_cast<uint32>(Service->GetMutationPhase()),
			static_cast<uint32>(
				ShutdownFlush.Current.Publication.CommitState),
			static_cast<uint32>(
				ShutdownFlush.PendingColdStart.Publication.CommitState),
			*ShutdownFlush.Detail));
		ASSERT_THAT(IsTrue(ShutdownFlush.IsSuccess()));
		ASSERT_THAT(AreEqual(EAngelscriptCacheMutationPhase::ShuttingDown,
			Service->GetMutationPhase()));
		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreCommitState::NotCommitted,
			ShutdownFlush.Current.Publication.CommitState));
		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreCommitState::NotCommitted,
			ShutdownFlush.PendingColdStart.Publication.CommitState));
	}
};

#endif
