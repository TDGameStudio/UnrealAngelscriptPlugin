#include "Cache/AngelscriptCacheCleanCapture.h"
#include "Cache/AngelscriptCacheRestore.h"
#include "Cache/AngelscriptCacheStore.h"

#include "AngelscriptTestMacros.h"
#include "CQTest.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Shared/AngelscriptTestFixture.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptCacheFreshEngineRestoreTests_Private
{
	class FScopedDiskRoot final
	{
	public:
		FScopedDiskRoot()
		{
			Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(
				FPaths::ProjectSavedDir(),
				TEXT("Automation/AngelscriptCacheFreshEngineRestore"),
				FGuid::NewGuid().ToString(EGuidFormats::Digits)));
			FPaths::NormalizeDirectoryName(Root);
			check(Root.Contains(
				TEXT("/Saved/Automation/AngelscriptCacheFreshEngineRestore/")));
			check(IFileManager::Get().MakeDirectory(*Root, true));
		}

		~FScopedDiskRoot()
		{
			if (Root.Contains(
				TEXT("/Saved/Automation/AngelscriptCacheFreshEngineRestore/")))
			{
				IFileManager::Get().DeleteDirectory(*Root, false, true);
			}
		}

		FString Root;
	};

	static FAngelscriptCacheCleanCaptureOptions MakeCaptureOptions()
	{
		FAngelscriptCacheCleanCaptureOptions Options;
		FAngelscriptCompatibilityDescriptor Compatibility;
		Compatibility.CanonicalInputs = {
			TEXT("CacheV2FreshEngineRestoreTest"),
			TEXT("VmExecutionCodec=2"),
		};
		Options.Compatibility =
			FAngelscriptArtifactIdentityBuilder::BuildCompatibilityKey(
				Compatibility);

		FAngelscriptContextDescriptor Context;
		Context.CanonicalInputs = {
			TEXT("SourceMount=Game"),
			TEXT("DebugSidecar=Enabled"),
		};
		Options.Context =
			FAngelscriptArtifactIdentityBuilder::BuildContextKey(Context);
		Options.Profile =
			FAngelscriptArtifactIdentityBuilder::BuildArtifactProfileKey(
				Options.Compatibility, Options.Context);
		Options.CanonicalCompileOptions = {
			TEXT("AutomaticImports=false"),
		};
		return Options;
	}

	static bool CaptureAndPrepareAfterProducerDestruction(
		FAutomationTestBase& Test,
		const FAngelscriptCacheCleanCaptureOptions& Options,
		FAngelscriptCachePreparedColdGeneration& OutGeneration,
		FAngelscriptStableModuleKey& OutModuleKey)
	{
		FAngelscriptCacheCleanModuleArtifacts Artifacts;
		{
			FAngelscriptTestFixture Producer(
				Test, ETestEngineMode::IsolatedFull);
			if (!Producer.IsValid())
			{
				Test.AddError(TEXT(
					"Failed to create the producer isolated full Engine"));
				return false;
			}

			const FString Source = ASTEST_AS(R"AS(
				enum ECacheV2RestoreState
				{
					Ready = 7,
				}

				int Answer()
				{
					return 42;
				}
			)AS");
			asIScriptModule* ScriptModule = Producer.BuildModule(
				"ASCacheV2FreshEngineRestore", Source);
			if (ScriptModule == nullptr)
			{
				return false;
			}

			TSharedPtr<FAngelscriptModuleDesc> Module =
				Producer.GetEngine().GetModule(ScriptModule);
			if (!Module.IsValid())
			{
				Test.AddError(TEXT(
					"Producer compile did not retain its module descriptor"));
				return false;
			}

			const FAngelscriptCacheCleanCaptureResult Capture =
				CaptureAngelscriptCleanCompiledModule(
					Module.ToSharedRef(), Options, Artifacts);
			Test.AddInfo(FString::Printf(
				TEXT("Fresh restore producer: Error=%u Records=%d GraphRecords=%u Detail=%s"),
				static_cast<uint32>(Capture.Error),
				Artifacts.Records.Num(),
				Capture.ValidatedGraphRecordCount,
				*Capture.Detail));
			if (!Capture.IsSuccess())
			{
				return false;
			}
			OutModuleKey = Artifacts.ModuleKey;
		}

		// The producer Engine and every live pointer it owned are gone here.
		FAngelscriptCachePackPolicy PackPolicy;
		PackPolicy.CompressionPolicy =
			EAngelscriptCachePackCompressionPolicy::ForceNoneForTest;
		FAngelscriptUnrealZlibCacheStorageCodec Codec;
		const FAngelscriptCacheCleanCaptureResult Prepare =
			PrepareAngelscriptCacheColdGeneration(
				Artifacts, Options, PackPolicy, Codec, OutGeneration);
		Test.AddInfo(FString::Printf(
			TEXT("Fresh restore cold generation: Error=%u Generation=%s Packs=%d Detail=%s"),
			static_cast<uint32>(Prepare.Error),
			*OutGeneration.EncodedManifest.ComputedGenerationId.ToHexString(),
			OutGeneration.Packs.Num(),
			*Prepare.Detail));
		return Prepare.IsSuccess();
	}

	static bool PublishAndOpenPreparedGeneration(
		FAutomationTestBase& Test,
		const FString& DiskRoot,
		const FAngelscriptCacheCleanCaptureOptions& Options,
		const FAngelscriptCachePreparedColdGeneration& Prepared,
		TUniquePtr<FAngelscriptCacheReadSession>& OutSession)
	{
		OutSession.Reset();
		TUniquePtr<IAngelscriptCacheAtomicFileOps> FileOps =
			CreateAngelscriptCacheAtomicFileOps();
		TUniquePtr<IAngelscriptCacheNamespaceLockOps> LockOps =
			CreateAngelscriptCacheNamespaceLockOps();
		if (!FileOps.IsValid() || !LockOps.IsValid())
		{
			return false;
		}

		FAngelscriptCacheStorePaths Paths;
		if (!BuildAngelscriptCacheStorePaths(
			DiskRoot / TEXT("CacheV2"),
			Options.Compatibility,
			Options.Context,
			*FileOps,
			Paths).IsSuccess()
			|| !EnsureAngelscriptCacheStoreDirectories(
				Paths, *FileOps).IsSuccess())
		{
			return false;
		}

		const TOptional<FAngelscriptCacheWriterToken> WriterToken =
			FAngelscriptCacheWriterToken::TryParse(
				TEXT("8301-11223344556677889900aabbccddeeff"));
		if (!WriterToken.IsSet())
		{
			return false;
		}

		FAngelscriptCacheReadLimits Limits;
		FAngelscriptCacheReadBudget PublicationBudget;
		FAngelscriptUnrealZlibCacheStorageCodec Codec;
		const FAngelscriptCacheStoreResult Publication =
			PublishAngelscriptCacheGeneration(
				Paths,
				EAngelscriptCachePublicationDisposition::Current,
				TOptional<FAngelscriptHash256>{},
				Prepared.Packs,
				Prepared.Manifest,
				Prepared.EncodedManifest,
				WriterToken.GetValue(),
				Limits,
				PublicationBudget,
				FPlatformTime::Seconds() + 2.0,
				[]() { return false; },
				Codec,
				*LockOps,
				*FileOps);
		if (Publication.Error != EAngelscriptCacheStoreError::None
			|| Publication.CommitState
				!= EAngelscriptCacheStoreCommitState::CurrentCommitted)
		{
			return false;
		}

		FAngelscriptCacheReadSelection Selection;
		Selection.Compatibility = Options.Compatibility;
		Selection.Context = Options.Context;
		Selection.Profile = Options.Profile;
		Selection.SourceSnapshot = Prepared.Manifest.SourceSnapshot;
		const FAngelscriptCacheStoreResult Open =
			OpenBestAngelscriptCacheReadSession(
				Paths,
				Selection,
				Limits,
				FPlatformTime::Seconds() + 2.0,
				[]() { return false; },
				Codec,
				*LockOps,
				*FileOps,
				OutSession);
		Test.AddInfo(FString::Printf(
			TEXT("Fresh restore disk reopen: PublishError=%u Commit=%u OpenError=%u Generation=%s PinnedPacks=%d"),
			static_cast<uint32>(Publication.Error),
			static_cast<uint32>(Publication.CommitState),
			static_cast<uint32>(Open.Error),
			*Prepared.EncodedManifest.ComputedGenerationId.ToHexString(),
			OutSession.IsValid() ? OutSession->GetPinnedPackCount() : 0));
		return Open.Error == EAngelscriptCacheStoreError::None
			&& OutSession.IsValid()
			&& OutSession->GetGenerationId()
				== Prepared.EncodedManifest.ComputedGenerationId;
	}

	static const FAngelscriptCachedFunctionBody* FindOnlyFunctionBody(
		const FAngelscriptValidatedGeneration& Generation)
	{
		const FAngelscriptCachedFunctionBody* FunctionBody = nullptr;
		for (const FAngelscriptDecodedCacheRecordHandle& Record
			: Generation.ReachableRecords)
		{
			if (const FAngelscriptCachedFunctionBody* Candidate =
				Record->TryGetFunctionBody())
			{
				if (FunctionBody != nullptr)
				{
					return nullptr;
				}
				FunctionBody = Candidate;
			}
		}
		return FunctionBody;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheFreshEngineRestoreTests,
	"Angelscript.TestModule.Cache.FreshEngineRestore",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(ColdGenerationRestoresExecutableFunctionIntoFreshEngine)
	{
		using namespace AngelscriptCacheFreshEngineRestoreTests_Private;

		const FAngelscriptCacheCleanCaptureOptions Options =
			MakeCaptureOptions();
		FAngelscriptCachePreparedColdGeneration Prepared;
		FAngelscriptStableModuleKey ModuleKey;
		ASSERT_THAT(IsTrue(CaptureAndPrepareAfterProducerDestruction(
			*TestRunner, Options, Prepared, ModuleKey)));

		FScopedDiskRoot Disk;
		TUniquePtr<FAngelscriptCacheReadSession> Session;
		ASSERT_THAT(IsTrue(PublishAndOpenPreparedGeneration(
			*TestRunner, Disk.Root, Options, Prepared, Session)));
		const FAngelscriptValidatedGeneration& Generation =
			Session->GetGeneration();
		const FAngelscriptCachedFunctionBody* FunctionBody =
			FindOnlyFunctionBody(Generation);
		ASSERT_THAT(IsNotNull(FunctionBody));

		FAngelscriptTestFixture Consumer(
			*TestRunner, ETestEngineMode::IsolatedFull);
		ASSERT_THAT(IsTrue(Consumer.IsValid()));
		ASSERT_THAT(IsNull(Consumer.GetEngine().GetModuleByModuleName(
			TEXT("ASCacheV2FreshEngineRestore")).Get()));

		FAngelscriptCacheReadLimits Limits;
		const FAngelscriptCacheRestoreResult Restore =
			RestoreAngelscriptCacheModule(
				Consumer.GetEngine(), Generation, ModuleKey, Limits);
		TestRunner->AddInfo(FString::Printf(
			TEXT("Fresh restore consumer: Error=%u Stage=%u Functions=%u Detail=%s"),
			static_cast<uint32>(Restore.Error),
			static_cast<uint32>(Restore.Stage),
			Restore.RestoredFunctionCount,
			*Restore.Detail));
		ASSERT_THAT(IsTrue(Restore.IsSuccess()));

		TSharedPtr<FAngelscriptModuleDesc> RestoredModule =
			Consumer.GetEngine().GetModuleByModuleName(
				TEXT("ASCacheV2FreshEngineRestore"));
		ASSERT_THAT(IsTrue(RestoredModule.IsValid()));
		ASSERT_THAT(IsNotNull(RestoredModule->ScriptModule));
		ASSERT_THAT(AreEqual(1, RestoredModule->Enums.Num()));
		ASSERT_THAT(AreEqual(
			FString(TEXT("ECacheV2RestoreState")),
			RestoredModule->Enums[0]->EnumName));
		ASSERT_THAT(IsNotNull(RestoredModule->Enums[0]->ScriptType));

		asIScriptFunction* Answer =
			RestoredModule->ScriptModule->GetFunctionByDecl("int Answer()");
		ASSERT_THAT(IsNotNull(Answer));
		int32 AnswerValue = 0;
		ASSERT_THAT(IsTrue(Consumer.ExecuteInt(*Answer, AnswerValue)));
		ASSERT_THAT(AreEqual(42, AnswerValue));

		FAngelscriptCacheLiveFunctionRoute Route;
		ASSERT_THAT(IsTrue(Consumer.GetEngine().ResolveCacheFunctionRoute(
			FunctionBody->Identity.FunctionKey, Route)));
		ASSERT_THAT(IsTrue(Route.Function == Answer));
		ASSERT_THAT(AreEqual(Answer->GetId(), Route.NumericFunctionId));
		ASSERT_THAT(IsTrue(Route.ModuleKey == ModuleKey));
		ASSERT_THAT(IsTrue(Route.Identity.FunctionKey
			== FunctionBody->Identity.FunctionKey));
		ASSERT_THAT(IsTrue(Route.Identity.Content.Execution
			== FunctionBody->Identity.Content.Execution));
		ASSERT_THAT(IsTrue(Route.Identity.Profile.Hash
			== FunctionBody->Identity.Profile.Hash));

		TestRunner->AddInfo(FString::Printf(
			TEXT("Fresh restore executable: Module=%s StableKey=%s FunctionId=%d Result=%d"),
			*RestoredModule->ModuleName,
			*FunctionBody->Identity.FunctionKey.Hash.ToHexString(),
			Route.NumericFunctionId,
			AnswerValue));
	}

	TEST_METHOD(DuplicateStableRouteIsRejectedAndDiscardRemovesPublishedRoute)
	{
		using namespace AngelscriptCacheFreshEngineRestoreTests_Private;

		const FAngelscriptCacheCleanCaptureOptions Options =
			MakeCaptureOptions();
		FAngelscriptCachePreparedColdGeneration Prepared;
		FAngelscriptStableModuleKey ModuleKey;
		ASSERT_THAT(IsTrue(CaptureAndPrepareAfterProducerDestruction(
			*TestRunner, Options, Prepared, ModuleKey)));

		FScopedDiskRoot Disk;
		TUniquePtr<FAngelscriptCacheReadSession> Session;
		ASSERT_THAT(IsTrue(PublishAndOpenPreparedGeneration(
			*TestRunner, Disk.Root, Options, Prepared, Session)));
		const FAngelscriptValidatedGeneration& Generation =
			Session->GetGeneration();
		const FAngelscriptCachedFunctionBody* FunctionBody =
			FindOnlyFunctionBody(Generation);
		ASSERT_THAT(IsNotNull(FunctionBody));

		FAngelscriptTestFixture Consumer(
			*TestRunner, ETestEngineMode::IsolatedFull);
		ASSERT_THAT(IsTrue(Consumer.IsValid()));
		FAngelscriptCacheReadLimits Limits;
		ASSERT_THAT(IsTrue(RestoreAngelscriptCacheModule(
			Consumer.GetEngine(), Generation, ModuleKey, Limits).IsSuccess()));

		FAngelscriptCacheLiveFunctionRoute OriginalRoute;
		ASSERT_THAT(IsTrue(Consumer.GetEngine().ResolveCacheFunctionRoute(
			FunctionBody->Identity.FunctionKey, OriginalRoute)));
		const FAngelscriptCacheRestoreResult Duplicate =
			RestoreAngelscriptCacheModule(
				Consumer.GetEngine(), Generation, ModuleKey, Limits);
		TestRunner->AddInfo(FString::Printf(
			TEXT("Fresh restore duplicate route: Error=%u Stage=%u Detail=%s"),
			static_cast<uint32>(Duplicate.Error),
			static_cast<uint32>(Duplicate.Stage),
			*Duplicate.Detail));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheRestoreError::ActivationFailed, Duplicate.Error));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheRestoreStage::ActivateModule, Duplicate.Stage));
		ASSERT_THAT(IsTrue(Duplicate.Detail.Contains(
			TEXT("stable function route"))));

		FAngelscriptCacheLiveFunctionRoute RouteAfterRejection;
		ASSERT_THAT(IsTrue(Consumer.GetEngine().ResolveCacheFunctionRoute(
			FunctionBody->Identity.FunctionKey, RouteAfterRejection)));
		ASSERT_THAT(IsTrue(
			RouteAfterRejection.Function == OriginalRoute.Function));
		ASSERT_THAT(AreEqual(
			OriginalRoute.NumericFunctionId,
			RouteAfterRejection.NumericFunctionId));
		const int32 RemovedFunctionId = OriginalRoute.NumericFunctionId;

		ASSERT_THAT(IsTrue(Consumer.GetEngine().DiscardModule(
			TEXT("ASCacheV2FreshEngineRestore"))));
		ASSERT_THAT(IsFalse(Consumer.GetEngine().GetModuleByModuleName(
			TEXT("ASCacheV2FreshEngineRestore")).IsValid()));
		ASSERT_THAT(IsFalse(Consumer.GetEngine().GetEnum(
			TEXT("ECacheV2RestoreState")).IsValid()));
		FAngelscriptCacheLiveFunctionRoute RemovedRoute;
		ASSERT_THAT(IsFalse(Consumer.GetEngine().ResolveCacheFunctionRoute(
			FunctionBody->Identity.FunctionKey, RemovedRoute)));
		TestRunner->AddInfo(FString::Printf(
			TEXT("Fresh restore discard: ModuleRemoved=1 RouteRemoved=1 PreviousFunctionId=%d"),
			RemovedFunctionId));
	}

	TEST_METHOD(IndependentEnginesOwnIndependentStableRoutes)
	{
		using namespace AngelscriptCacheFreshEngineRestoreTests_Private;

		const FAngelscriptCacheCleanCaptureOptions Options =
			MakeCaptureOptions();
		FAngelscriptCachePreparedColdGeneration Prepared;
		FAngelscriptStableModuleKey ModuleKey;
		ASSERT_THAT(IsTrue(CaptureAndPrepareAfterProducerDestruction(
			*TestRunner, Options, Prepared, ModuleKey)));
		FScopedDiskRoot Disk;
		TUniquePtr<FAngelscriptCacheReadSession> Session;
		ASSERT_THAT(IsTrue(PublishAndOpenPreparedGeneration(
			*TestRunner, Disk.Root, Options, Prepared, Session)));
		const FAngelscriptValidatedGeneration& Generation =
			Session->GetGeneration();
		const FAngelscriptCachedFunctionBody* FunctionBody =
			FindOnlyFunctionBody(Generation);
		ASSERT_THAT(IsNotNull(FunctionBody));

		FAngelscriptTestFixture ConsumerA(
			*TestRunner, ETestEngineMode::IsolatedFull);
		FAngelscriptTestFixture ConsumerB(
			*TestRunner, ETestEngineMode::IsolatedFull);
		ASSERT_THAT(IsTrue(ConsumerA.IsValid()));
		ASSERT_THAT(IsTrue(ConsumerB.IsValid()));
		FAngelscriptCacheReadLimits Limits;
		ASSERT_THAT(IsTrue(RestoreAngelscriptCacheModule(
			ConsumerA.GetEngine(), Generation, ModuleKey, Limits).IsSuccess()));
		ASSERT_THAT(IsTrue(RestoreAngelscriptCacheModule(
			ConsumerB.GetEngine(), Generation, ModuleKey, Limits).IsSuccess()));

		FAngelscriptCacheLiveFunctionRoute RouteA;
		FAngelscriptCacheLiveFunctionRoute RouteB;
		ASSERT_THAT(IsTrue(ConsumerA.GetEngine().ResolveCacheFunctionRoute(
			FunctionBody->Identity.FunctionKey, RouteA)));
		ASSERT_THAT(IsTrue(ConsumerB.GetEngine().ResolveCacheFunctionRoute(
			FunctionBody->Identity.FunctionKey, RouteB)));
		ASSERT_THAT(IsTrue(RouteA.Function != RouteB.Function));
		ASSERT_THAT(IsTrue(RouteA.Function->GetEngine()
			== ConsumerA.GetEngine().GetScriptEngine()));
		ASSERT_THAT(IsTrue(RouteB.Function->GetEngine()
			== ConsumerB.GetEngine().GetScriptEngine()));
		ASSERT_THAT(IsTrue(
			ConsumerA.GetEngine().GetScriptEngine()->GetFunctionById(
				RouteA.NumericFunctionId) == RouteA.Function));
		ASSERT_THAT(IsTrue(
			ConsumerB.GetEngine().GetScriptEngine()->GetFunctionById(
				RouteB.NumericFunctionId) == RouteB.Function));

		FAngelscriptTestFixture UnrelatedConsumer(
			*TestRunner, ETestEngineMode::IsolatedFull);
		ASSERT_THAT(IsTrue(UnrelatedConsumer.IsValid()));
		FAngelscriptCacheLiveFunctionRoute UnrelatedRoute;
		ASSERT_THAT(IsFalse(
			UnrelatedConsumer.GetEngine().ResolveCacheFunctionRoute(
				FunctionBody->Identity.FunctionKey, UnrelatedRoute)));
		TestRunner->AddInfo(FString::Printf(
			TEXT("Fresh restore Engine route isolation: FunctionIdA=%d FunctionIdB=%d PointerA=%p PointerB=%p"),
			RouteA.NumericFunctionId,
			RouteB.NumericFunctionId,
			RouteA.Function,
			RouteB.Function));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
