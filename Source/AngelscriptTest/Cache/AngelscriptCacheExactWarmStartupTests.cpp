#include "Cache/AngelscriptCacheCleanCapture.h"
#include "Cache/AngelscriptCacheExactStartup.h"
#include "Cache/AngelscriptCacheSourceDiscovery.h"
#include "Cache/AngelscriptCacheStore.h"

#include "CQTest.h"
#include "Compilation/AngelscriptCompilationEvents.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Hash/Blake3.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Preprocessor/AngelscriptPreprocessor.h"
#include "Shared/AngelscriptTestFixture.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptCacheExactWarmStartupTests_Private
{
	static constexpr const TCHAR* RelativeSourcePath = TEXT("ExactWarm.as");
	static constexpr const TCHAR* ModuleName = TEXT("ExactWarm");

	class FScopedDiskRoot final
	{
	public:
		FScopedDiskRoot()
		{
			Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(
				FPaths::ProjectSavedDir(),
				TEXT("Automation/AngelscriptCacheExactWarmStartup"),
				FGuid::NewGuid().ToString(EGuidFormats::Digits)));
			FPaths::NormalizeDirectoryName(Root);
			check(Root.Contains(
				TEXT("/Saved/Automation/AngelscriptCacheExactWarmStartup/")));
			check(IFileManager::Get().MakeDirectory(*Root, true));
		}

		~FScopedDiskRoot()
		{
			if (Root.Contains(
				TEXT("/Saved/Automation/AngelscriptCacheExactWarmStartup/")))
			{
				IFileManager::Get().DeleteDirectory(*Root, false, true);
			}
		}

		FString MakeSourceRoot(const TCHAR* Leaf) const
		{
			const FString Result = Root / Leaf;
			check(IFileManager::Get().MakeDirectory(*Result, true));
			return Result;
		}

		FString Root;
	};

	static FString MakeSource(const int32 Answer)
	{
		return FString::Printf(TEXT(R"AS(
enum EExactWarmState
{
	Ready = 7,
}

int Answer()
{
	return %d;
}
)AS"), Answer);
	}

	static bool WriteSource(
		const FString& SourceRoot,
		const FString& Source)
	{
		return FFileHelper::SaveStringToFile(
			Source,
			*(SourceRoot / RelativeSourcePath),
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}

	static FAngelscriptCacheCleanCaptureOptions MakeCaptureOptions()
	{
		FAngelscriptCacheCleanCaptureOptions Options;
		FAngelscriptCompatibilityDescriptor Compatibility;
		Compatibility.CanonicalInputs = {
			TEXT("CacheV2ExactWarmStartupTest"),
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

	static FAngelscriptCacheProductionSourceDiscoveryConfig
	MakeDiscoveryConfig(const FAngelscriptCacheCleanCaptureOptions& Options)
	{
		FAngelscriptCacheProductionSourceDiscoveryConfig Config;
		Config.Profile = Options.Profile;
		Config.DiscoveryPolicyVersion = 1;
		Config.bObserveLegacyGlobalPreprocessHooks = false;
		FAngelscriptCacheDirectOptionInput& CompileOption =
			Config.Options.AddDefaulted_GetRef();
		CompileOption.Kind = EAngelscriptCacheDirectOptionKind::Compiler;
		CompileOption.CanonicalKey = TEXT("AutomaticImports");
		CompileOption.CanonicalValue = TEXT("false");
		return Config;
	}

	static bool Discover(
		FAutomationTestBase& Test,
		const FString& SourceRoot,
		const FAngelscriptCacheCleanCaptureOptions& Options,
		FAngelscriptCacheProductionSourceDiscoveryResult& OutDiscovery)
	{
		FAngelscriptDiskSourceProvider Provider;
		const TArray<FAngelscriptSourceRoot> Roots{
			FAngelscriptSourceRoot::FromGameRoot(SourceRoot)};
		const FAngelscriptCacheSourceDiscoveryStatus Status =
			FAngelscriptCacheSourceDiscovery::DiscoverProductionSources(
				Provider,
				Roots,
				false,
				false,
				MakeDiscoveryConfig(Options),
				{},
				OutDiscovery);
		Test.AddInfo(FString::Printf(
			TEXT("Exact warm discovery: Success=%d Sources=%d Modules=%d Direct=%s Detail=%s"),
			Status.IsSuccess() ? 1 : 0,
			OutDiscovery.LoadedSourceCount,
			OutDiscovery.Modules.Num(),
			*OutDiscovery.DirectPlan.DirectInputDigest.ToHexString(),
			*Status.Detail));
		return Status.IsSuccess();
	}

	static TSharedPtr<FAngelscriptModuleDesc> CompileDiskModule(
		FAutomationTestBase& Test,
		FAngelscriptEngine& Engine,
		const FString& SourceRoot)
	{
		FAngelscriptPreprocessor Preprocessor;
		Preprocessor.AddFile(
			RelativeSourcePath,
			SourceRoot / RelativeSourcePath);
		if (!Preprocessor.Preprocess())
		{
			Test.AddError(TEXT("Exact warm producer preprocessing failed"));
			return {};
		}

		TArray<TSharedRef<FAngelscriptModuleDesc>> CompiledModules;
		TGuardValue<bool> AutomaticImportGuard(
			Engine.bUseAutomaticImportMethod, false);
		FScopedAutomaticImportsOverride AutomaticImportsOverride(
			Engine.GetScriptEngine());
		const ECompileResult CompileResult = Engine.CompileModules(
			ECompileType::Initial,
			Preprocessor.GetModulesToCompile(),
			CompiledModules);
		if (CompileResult == ECompileResult::Error
			|| CompileResult == ECompileResult::ErrorNeedFullReload
			|| CompiledModules.Num() != 1)
		{
			Test.AddError(FString::Printf(
				TEXT("Exact warm producer compile failed: Result=%u Modules=%d"),
				static_cast<uint32>(CompileResult),
				CompiledModules.Num()));
			return {};
		}
		return CompiledModules[0];
	}

	struct FPreparedFixture
	{
		FAngelscriptCachePreparedColdGeneration Generation;
		FAngelscriptStableModuleKey ModuleKey;
		FAngelscriptHash256 ProducerDirectDigest;
	};

	static bool PrepareColdFixture(
		FAutomationTestBase& Test,
		const FString& SourceRoot,
		const FAngelscriptCacheCleanCaptureOptions& Options,
		FPreparedFixture& OutFixture)
	{
		FAngelscriptCacheProductionSourceDiscoveryResult Discovery;
		if (!Discover(Test, SourceRoot, Options, Discovery))
		{
			return false;
		}
		OutFixture.ProducerDirectDigest =
			Discovery.DirectPlan.DirectInputDigest;

		FAngelscriptCachedSourceIndex PersistedSourceIndex;
		const FAngelscriptCacheValidationResult CandidateResult =
			FAngelscriptCacheSourcePlanner::BuildPersistedDependencyCandidate(
				Discovery.DirectPlan,
				Options.Profile,
				{},
				{},
				{},
				PersistedSourceIndex);
		if (!CandidateResult.IsSuccess())
		{
			Test.AddError(FString::Printf(
				TEXT("Exact warm SourceIndex candidate failed: Error=%u"),
				static_cast<uint32>(CandidateResult.Error)));
			return false;
		}

		FAngelscriptCacheCleanModuleArtifacts Artifacts;
		{
			FAngelscriptTestFixture Producer(
				Test, ETestEngineMode::IsolatedFull);
			if (!Producer.IsValid())
			{
				return false;
			}
			TSharedPtr<FAngelscriptModuleDesc> Module = CompileDiskModule(
				Test, Producer.GetEngine(), SourceRoot);
			if (!Module.IsValid())
			{
				return false;
			}
			const FAngelscriptCacheCleanCaptureResult Capture =
				CaptureAngelscriptCleanCompiledModule(
					Module.ToSharedRef(),
					Options,
					PersistedSourceIndex,
					Artifacts);
			Test.AddInfo(FString::Printf(
				TEXT("Exact warm producer capture: Error=%u Records=%d Graph=%u Snapshot=%s Detail=%s"),
				static_cast<uint32>(Capture.Error),
				Artifacts.Records.Num(),
				Capture.ValidatedGraphRecordCount,
				*Artifacts.SourceSnapshot.ToHexString(),
				*Capture.Detail));
			if (!Capture.IsSuccess())
			{
				return false;
			}
			OutFixture.ModuleKey = Artifacts.ModuleKey;
		}

		FAngelscriptCachePackPolicy PackPolicy;
		PackPolicy.CompressionPolicy =
			EAngelscriptCachePackCompressionPolicy::ForceNoneForTest;
		FAngelscriptUnrealZlibCacheStorageCodec Codec;
		const FAngelscriptCacheCleanCaptureResult Prepare =
			PrepareAngelscriptCacheColdGeneration(
				Artifacts,
				Options,
				PackPolicy,
				Codec,
				OutFixture.Generation);
		Test.AddInfo(FString::Printf(
			TEXT("Exact warm cold generation: Error=%u Generation=%s Packs=%d Detail=%s"),
			static_cast<uint32>(Prepare.Error),
			*OutFixture.Generation.EncodedManifest.ComputedGenerationId.ToHexString(),
			OutFixture.Generation.Packs.Num(),
			*Prepare.Detail));
		return Prepare.IsSuccess();
	}

	static bool PublishAndOpen(
		FAutomationTestBase& Test,
		const FString& DiskRoot,
		const FAngelscriptCacheCleanCaptureOptions& Options,
		const FPreparedFixture& Fixture,
		FAngelscriptCacheStorePaths& OutPaths,
		TUniquePtr<FAngelscriptCacheReadSession>& OutSession)
	{
		TUniquePtr<IAngelscriptCacheAtomicFileOps> FileOps =
			CreateAngelscriptCacheAtomicFileOps();
		TUniquePtr<IAngelscriptCacheNamespaceLockOps> LockOps =
			CreateAngelscriptCacheNamespaceLockOps();
		if (!FileOps.IsValid() || !LockOps.IsValid()
			|| !BuildAngelscriptCacheStorePaths(
				DiskRoot / TEXT("CacheV2"),
				Options.Compatibility,
				Options.Context,
				*FileOps,
				OutPaths).IsSuccess()
			|| !EnsureAngelscriptCacheStoreDirectories(
				OutPaths, *FileOps).IsSuccess())
		{
			return false;
		}

		const TOptional<FAngelscriptCacheWriterToken> WriterToken =
			FAngelscriptCacheWriterToken::TryParse(
				TEXT("8501-aabbccddeeff00112233445566778899"));
		if (!WriterToken.IsSet())
		{
			return false;
		}

		FAngelscriptCacheReadLimits Limits;
		FAngelscriptCacheReadBudget PublicationBudget;
		FAngelscriptUnrealZlibCacheStorageCodec Codec;
		const FAngelscriptCacheStoreResult Publication =
			PublishAngelscriptCacheGeneration(
				OutPaths,
				EAngelscriptCachePublicationDisposition::Current,
				{},
				Fixture.Generation.Packs,
				Fixture.Generation.Manifest,
				Fixture.Generation.EncodedManifest,
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
		Selection.SourceSnapshot = Fixture.Generation.Manifest.SourceSnapshot;
		const FAngelscriptCacheStoreResult Open =
			OpenBestAngelscriptCacheReadSession(
				OutPaths,
				Selection,
				Limits,
				FPlatformTime::Seconds() + 2.0,
				[]() { return false; },
				Codec,
				*LockOps,
				*FileOps,
				OutSession);
		Test.AddInfo(FString::Printf(
			TEXT("Exact warm Store reopen: OpenError=%u Generation=%s PinnedPacks=%d"),
			static_cast<uint32>(Open.Error),
			*Fixture.Generation.EncodedManifest.ComputedGenerationId.ToHexString(),
			OutSession.IsValid() ? OutSession->GetPinnedPackCount() : 0));
		return Open.Error == EAngelscriptCacheStoreError::None
			&& OutSession.IsValid();
	}

	static FAngelscriptHash256 SnapshotPhysicalStore(
		const FString& NamespaceRoot,
		int32& OutFileCount)
	{
		TArray<FString> Files;
		IFileManager::Get().FindFilesRecursive(
			Files, *NamespaceRoot, TEXT("*"), true, false);
		Files.Sort();
		FAngelscriptArtifactCanonicalWriter Writer(
			TEXT("cache-v2-exact-warm-physical-store-v1"));
		Writer.WriteUInt32(static_cast<uint32>(Files.Num()));
		for (const FString& File : Files)
		{
			TArray<uint8> Bytes;
			check(FFileHelper::LoadFileToArray(Bytes, *File));
			FString Relative = File;
			FPaths::MakePathRelativeTo(Relative, *NamespaceRoot);
			Writer.WriteString(Relative);
			Writer.WriteUInt64(static_cast<uint64>(Bytes.Num()));
			Writer.WriteHash(FAngelscriptHash256{FBlake3::HashBuffer(
				Bytes.GetData(), static_cast<uint64>(Bytes.Num()))});
		}
		OutFileCount = Files.Num();
		return Writer.FinalizeHash();
	}

	static int32 CountFrontendEvents(
		const TArray<FAngelscriptCompilationEvent>& Events)
	{
		int32 Count = 0;
		for (const FAngelscriptCompilationEvent& Event : Events)
		{
			switch (Event.Type)
			{
			case EAngelscriptCompilationEventType::PreprocessProcessChunks:
			case EAngelscriptCompilationEventType::PreprocessPostProcessCode:
			case EAngelscriptCompilationEventType::CompileModuleParse:
			case EAngelscriptCompilationEventType::CompileModuleCompileCode:
				++Count;
				break;
			default:
				break;
			}
		}
		return Count;
	}

	static const FAngelscriptCachedFunctionBody* FindOnlyFunctionBody(
		const FAngelscriptValidatedGeneration& Generation)
	{
		const FAngelscriptCachedFunctionBody* Result = nullptr;
		for (const FAngelscriptDecodedCacheRecordHandle& Record
			: Generation.ReachableRecords)
		{
			if (const FAngelscriptCachedFunctionBody* Candidate =
				Record->TryGetFunctionBody())
			{
				if (Result != nullptr)
				{
					return nullptr;
				}
				Result = Candidate;
			}
		}
		return Result;
	}

	static void AssertZeroWork(
		FAutomationTestBase& Test,
		const FAngelscriptCacheExactStartupResult& Result,
		const TArray<FAngelscriptCompilationEvent>& Events)
	{
		Test.TestEqual(TEXT("warm preprocess calls"),
			Result.Counters.PreprocessCalls, uint32(0));
		Test.TestEqual(TEXT("warm parse calls"),
			Result.Counters.ParseCalls, uint32(0));
		Test.TestEqual(TEXT("warm module compiler calls"),
			Result.Counters.ModuleCompilerCalls, uint32(0));
		Test.TestEqual(TEXT("warm function compiler calls"),
			Result.Counters.FunctionCompilerCalls, uint32(0));
		Test.TestEqual(TEXT("warm publication attempts"),
			Result.Counters.PublicationAttempts, uint32(0));
		Test.TestEqual(TEXT("observed warm frontend events"),
			CountFrontendEvents(Events), 0);
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheExactWarmStartupTests,
	"Angelscript.TestModule.Cache.ExactWarmStartup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(UnchangedSecondLaunchRestoresWithZeroFrontendWorkAndNoPublication)
	{
		using namespace AngelscriptCacheExactWarmStartupTests_Private;
		FScopedDiskRoot Disk;
		const FString SourceRoot = Disk.MakeSourceRoot(TEXT("SourceA"));
		ASSERT_THAT(IsTrue(WriteSource(SourceRoot, MakeSource(42))));
		const FAngelscriptCacheCleanCaptureOptions Options =
			MakeCaptureOptions();
		FPreparedFixture Prepared;
		ASSERT_THAT(IsTrue(PrepareColdFixture(
			*TestRunner, SourceRoot, Options, Prepared)));

		FAngelscriptCacheStorePaths Paths;
		TUniquePtr<FAngelscriptCacheReadSession> Session;
		ASSERT_THAT(IsTrue(PublishAndOpen(
			*TestRunner, Disk.Root, Options, Prepared, Paths, Session)));
		int32 BeforeFileCount = 0;
		const FAngelscriptHash256 BeforeStore = SnapshotPhysicalStore(
			Paths.NamespaceRoot, BeforeFileCount);

		FAngelscriptTestFixture Consumer(
			*TestRunner, ETestEngineMode::IsolatedFull);
		ASSERT_THAT(IsTrue(Consumer.IsValid()));
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

		FAngelscriptCacheProductionSourceDiscoveryResult Current;
		ASSERT_THAT(IsTrue(Discover(
			*TestRunner, SourceRoot, Options, Current)));
		const FAngelscriptCacheExactStartupResult Warm =
			RestoreAngelscriptCacheExactStartup(
				Consumer.GetEngine(),
				Session->GetGeneration(),
				Options.Profile,
				Current,
				{},
				{});
		TestRunner->AddInfo(FString::Printf(
			TEXT("Exact warm restore: Disposition=%u Reason=%u Modules=%u Types=%u Functions=%u Detail=%s"),
			static_cast<uint32>(Warm.Disposition),
			static_cast<uint32>(Warm.Reason),
			Warm.RestoredModuleCount,
			Warm.RestoredTypeCount,
			Warm.RestoredFunctionCount,
			*Warm.Detail));
		ASSERT_THAT(IsTrue(Warm.IsRestored(), *Warm.Detail));
		AssertZeroWork(*TestRunner, Warm, Events);

		TSharedPtr<FAngelscriptModuleDesc> Restored =
			Consumer.GetEngine().GetModuleByModuleName(ModuleName);
		ASSERT_THAT(IsTrue(Restored.IsValid()));
		ASSERT_THAT(AreEqual(1, Restored->Code.Num()));
		ASSERT_THAT(AreEqual(
			FPaths::ConvertRelativePathToFull(SourceRoot / RelativeSourcePath),
			FPaths::ConvertRelativePathToFull(Restored->Code[0].AbsoluteFilename)));
		ASSERT_THAT(IsTrue(Restored->Code[0].Code.IsEmpty(),
			TEXT("raw source must not masquerade as preprocessed module code")));

		asIScriptFunction* Answer =
			Restored->ScriptModule->GetFunctionByDecl("int Answer()");
		ASSERT_THAT(IsNotNull(Answer));
		int32 Value = 0;
		ASSERT_THAT(IsTrue(Consumer.ExecuteInt(*Answer, Value)));
		ASSERT_THAT(AreEqual(42, Value));
		const FAngelscriptCachedFunctionBody* FunctionBody =
			FindOnlyFunctionBody(Session->GetGeneration());
		ASSERT_THAT(IsNotNull(FunctionBody));
		FAngelscriptCacheLiveFunctionRoute Route;
		ASSERT_THAT(IsTrue(Consumer.GetEngine().ResolveCacheFunctionRoute(
			FunctionBody->Identity.FunctionKey, Route)));
		ASSERT_THAT(AreEqual(Answer->GetId(), Route.NumericFunctionId));

		int32 AfterFileCount = 0;
		const FAngelscriptHash256 AfterStore = SnapshotPhysicalStore(
			Paths.NamespaceRoot, AfterFileCount);
		ASSERT_THAT(AreEqual(BeforeFileCount, AfterFileCount));
		ASSERT_THAT(AreEqual(
			BeforeStore.ToHexString(), AfterStore.ToHexString()));
		TestRunner->AddInfo(FString::Printf(
			TEXT("Exact warm accepted: Generation=%s SourceSnapshot=%s FunctionId=%d Result=%d StoreFiles=%d StoreDigest=%s"),
			*Session->GetGenerationId().ToHexString(),
			*Session->GetGeneration().Manifest.SourceSnapshot.ToHexString(),
			Route.NumericFunctionId,
			Value,
			AfterFileCount,
			*AfterStore.ToHexString()));
	}

	TEST_METHOD(ChangedRawSourceIsSafeMissBeforeEngineMutation)
	{
		using namespace AngelscriptCacheExactWarmStartupTests_Private;
		FScopedDiskRoot Disk;
		const FString SourceRoot = Disk.MakeSourceRoot(TEXT("SourceA"));
		ASSERT_THAT(IsTrue(WriteSource(SourceRoot, MakeSource(42))));
		const FAngelscriptCacheCleanCaptureOptions Options =
			MakeCaptureOptions();
		FPreparedFixture Prepared;
		ASSERT_THAT(IsTrue(PrepareColdFixture(
			*TestRunner, SourceRoot, Options, Prepared)));
		FAngelscriptCacheStorePaths Paths;
		TUniquePtr<FAngelscriptCacheReadSession> Session;
		ASSERT_THAT(IsTrue(PublishAndOpen(
			*TestRunner, Disk.Root, Options, Prepared, Paths, Session)));
		int32 BeforeFileCount = 0;
		const FAngelscriptHash256 BeforeStore = SnapshotPhysicalStore(
			Paths.NamespaceRoot, BeforeFileCount);
		ASSERT_THAT(IsTrue(WriteSource(SourceRoot, MakeSource(99))));

		FAngelscriptTestFixture Consumer(
			*TestRunner, ETestEngineMode::IsolatedFull);
		ASSERT_THAT(IsTrue(Consumer.IsValid()));
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
		FAngelscriptCacheProductionSourceDiscoveryResult Current;
		ASSERT_THAT(IsTrue(Discover(
			*TestRunner, SourceRoot, Options, Current)));
		const FAngelscriptCacheExactStartupResult Warm =
			RestoreAngelscriptCacheExactStartup(
				Consumer.GetEngine(), Session->GetGeneration(),
				Options.Profile, Current, {}, {});
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheExactStartupDisposition::Miss,
			Warm.Disposition));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheExactStartupReason::DirectInputMismatch,
			Warm.Reason));
		AssertZeroWork(*TestRunner, Warm, Events);
		ASSERT_THAT(IsFalse(Consumer.GetEngine().GetModuleByModuleName(
			ModuleName).IsValid()));
		ASSERT_THAT(IsFalse(Consumer.GetEngine().GetEnum(
			TEXT("EExactWarmState")).IsValid()));
		int32 AfterFileCount = 0;
		const FAngelscriptHash256 AfterStore = SnapshotPhysicalStore(
			Paths.NamespaceRoot, AfterFileCount);
		ASSERT_THAT(AreEqual(BeforeFileCount, AfterFileCount));
		ASSERT_THAT(AreEqual(
			BeforeStore.ToHexString(), AfterStore.ToHexString()));
		TestRunner->AddInfo(FString::Printf(
			TEXT("Exact warm changed-source miss: Reason=%u DirectBefore=%s DirectNow=%s Detail=%s"),
			static_cast<uint32>(Warm.Reason),
			*Prepared.ProducerDirectDigest.ToHexString(),
			*Current.DirectPlan.DirectInputDigest.ToHexString(),
			*Warm.Detail));
	}

	TEST_METHOD(MismatchedTransientProjectionIsSafeMissBeforeActivation)
	{
		using namespace AngelscriptCacheExactWarmStartupTests_Private;
		FScopedDiskRoot Disk;
		const FString SourceRoot = Disk.MakeSourceRoot(TEXT("SourceA"));
		ASSERT_THAT(IsTrue(WriteSource(SourceRoot, MakeSource(42))));
		const FAngelscriptCacheCleanCaptureOptions Options =
			MakeCaptureOptions();
		FPreparedFixture Prepared;
		ASSERT_THAT(IsTrue(PrepareColdFixture(
			*TestRunner, SourceRoot, Options, Prepared)));
		FAngelscriptCacheStorePaths Paths;
		TUniquePtr<FAngelscriptCacheReadSession> Session;
		ASSERT_THAT(IsTrue(PublishAndOpen(
			*TestRunner, Disk.Root, Options, Prepared, Paths, Session)));

		FAngelscriptTestFixture Consumer(
			*TestRunner, ETestEngineMode::IsolatedFull);
		ASSERT_THAT(IsTrue(Consumer.IsValid()));
		FAngelscriptCacheProductionSourceDiscoveryResult Current;
		ASSERT_THAT(IsTrue(Discover(
			*TestRunner, SourceRoot, Options, Current)));
		ASSERT_THAT(AreEqual(1, Current.CurrentSources.Num()));
		ASSERT_THAT(IsTrue(!Current.CurrentSources[0].RawSourceBytes.IsEmpty()));
		Current.CurrentSources[0].RawSourceBytes[0] ^= 0x1;

		const FAngelscriptCacheExactStartupResult Warm =
			RestoreAngelscriptCacheExactStartup(
				Consumer.GetEngine(), Session->GetGeneration(),
				Options.Profile, Current, {}, {});
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheExactStartupDisposition::Miss,
			Warm.Disposition));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheExactStartupReason::CurrentSourceProjectionMismatch,
			Warm.Reason));
		ASSERT_THAT(IsFalse(Consumer.GetEngine().GetModuleByModuleName(
			ModuleName).IsValid()));
		ASSERT_THAT(IsFalse(Consumer.GetEngine().GetEnum(
			TEXT("EExactWarmState")).IsValid()));
		TestRunner->AddInfo(FString::Printf(
			TEXT("Exact warm transient projection miss: Reason=%u Detail=%s"),
			static_cast<uint32>(Warm.Reason), *Warm.Detail));
	}

	TEST_METHOD(RelocatedAbsoluteRootStillRestoresFromSameGeneration)
	{
		using namespace AngelscriptCacheExactWarmStartupTests_Private;
		FScopedDiskRoot Disk;
		const FString SourceRootA = Disk.MakeSourceRoot(TEXT("SourceA"));
		const FString SourceRootB = Disk.MakeSourceRoot(TEXT("SourceB"));
		const FString Source = MakeSource(42);
		ASSERT_THAT(IsTrue(WriteSource(SourceRootA, Source)));
		ASSERT_THAT(IsTrue(WriteSource(SourceRootB, Source)));
		const FAngelscriptCacheCleanCaptureOptions Options =
			MakeCaptureOptions();
		FPreparedFixture Prepared;
		ASSERT_THAT(IsTrue(PrepareColdFixture(
			*TestRunner, SourceRootA, Options, Prepared)));
		FAngelscriptCacheStorePaths Paths;
		TUniquePtr<FAngelscriptCacheReadSession> Session;
		ASSERT_THAT(IsTrue(PublishAndOpen(
			*TestRunner, Disk.Root, Options, Prepared, Paths, Session)));

		FAngelscriptTestFixture Consumer(
			*TestRunner, ETestEngineMode::IsolatedFull);
		ASSERT_THAT(IsTrue(Consumer.IsValid()));
		FAngelscriptCacheProductionSourceDiscoveryResult Current;
		ASSERT_THAT(IsTrue(Discover(
			*TestRunner, SourceRootB, Options, Current)));
		ASSERT_THAT(AreEqual(
			Prepared.ProducerDirectDigest.ToHexString(),
			Current.DirectPlan.DirectInputDigest.ToHexString()));
		const FAngelscriptCacheExactStartupResult Warm =
			RestoreAngelscriptCacheExactStartup(
				Consumer.GetEngine(), Session->GetGeneration(),
				Options.Profile, Current, {}, {});
		ASSERT_THAT(IsTrue(Warm.IsRestored(), *Warm.Detail));
		TSharedPtr<FAngelscriptModuleDesc> Restored =
			Consumer.GetEngine().GetModuleByModuleName(ModuleName);
		ASSERT_THAT(IsTrue(Restored.IsValid()));
		ASSERT_THAT(AreEqual(1, Restored->Code.Num()));
		ASSERT_THAT(AreEqual(
			FPaths::ConvertRelativePathToFull(SourceRootB / RelativeSourcePath),
			FPaths::ConvertRelativePathToFull(Restored->Code[0].AbsoluteFilename)));
		ASSERT_THAT(IsFalse(Restored->Code[0].AbsoluteFilename.Contains(
			TEXT("SourceA"))));
		TestRunner->AddInfo(FString::Printf(
			TEXT("Exact warm relocation: Direct=%s OldRoot=%s CurrentPath=%s"),
			*Current.DirectPlan.DirectInputDigest.ToHexString(),
			*SourceRootA,
			*Restored->Code[0].AbsoluteFilename));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
