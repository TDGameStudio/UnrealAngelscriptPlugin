#include "Cache/AngelscriptCacheCleanCapture.h"
#include "Cache/AngelscriptCacheStore.h"

#include "CQTest.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Shared/AngelscriptTestFixture.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptCacheColdGenerationTests_Private
{
	class FScopedDiskRoot final
	{
	public:
		FScopedDiskRoot()
		{
			Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(
				FPaths::ProjectSavedDir(),
				TEXT("Automation/AngelscriptCacheColdGeneration"),
				FGuid::NewGuid().ToString(EGuidFormats::Digits)));
			FPaths::NormalizeDirectoryName(Root);
			check(Root.Contains(
				TEXT("/Saved/Automation/AngelscriptCacheColdGeneration/")));
			check(IFileManager::Get().MakeDirectory(*Root, true));
		}

		~FScopedDiskRoot()
		{
			if (Root.Contains(
				TEXT("/Saved/Automation/AngelscriptCacheColdGeneration/")))
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
			TEXT("CacheV2ColdGenerationTest"),
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
		Options.Context = FAngelscriptArtifactIdentityBuilder::BuildContextKey(
			Context);
		Options.Profile =
			FAngelscriptArtifactIdentityBuilder::BuildArtifactProfileKey(
				Options.Compatibility, Options.Context);
		Options.CanonicalCompileOptions = {
			TEXT("AutomaticImports=false"),
		};
		return Options;
	}

	static bool CaptureOnce(
		FAutomationTestBase& Test,
		const FAngelscriptCacheCleanCaptureOptions& Options,
		FAngelscriptCacheCleanModuleArtifacts& OutArtifacts,
		uint32& OutValidatedGraphRecordCount)
	{
		OutValidatedGraphRecordCount = 0;
		FAngelscriptTestFixture Fixture(Test, ETestEngineMode::IsolatedFull);
		if (!Fixture.IsValid())
		{
			Test.AddError(TEXT("Failed to create an isolated full AngelScript engine"));
			return false;
		}

		const FString Source = TEXT(R"AS(
			enum ECacheV2ColdState
			{
				Ready = 7,
			}

			int Answer()
			{
				return 42;
			}
		)AS");
		asIScriptModule* ScriptModule = Fixture.BuildModule(
			"ASCacheV2ColdGeneration", Source);
		if (ScriptModule == nullptr)
		{
			return false;
		}

		TSharedPtr<FAngelscriptModuleDesc> Module =
			Fixture.GetEngine().GetModule(ScriptModule);
		if (!Test.TestTrue(
			TEXT("The normal compile should retain its module descriptor"),
			Module.IsValid()))
		{
			return false;
		}

		const FAngelscriptCacheCleanCaptureResult Capture =
			CaptureAngelscriptCleanCompiledModule(
				Module.ToSharedRef(), Options, OutArtifacts);
		Test.AddInfo(FString::Printf(
			TEXT("Cache V2 clean capture: Error=%u Module=%s Records=%d GraphRecords=%u Detail=%s"),
			static_cast<uint32>(Capture.Error),
			*Module->ModuleName,
			OutArtifacts.Records.Num(),
			Capture.ValidatedGraphRecordCount,
			*Capture.Detail));
		OutValidatedGraphRecordCount = Capture.ValidatedGraphRecordCount;
		return Test.TestTrue(
			TEXT("A normally compiled enum plus primitive global function should be cacheable"),
			Capture.IsSuccess());
	}

	static int32 CountRecordKind(
		const FAngelscriptCacheCleanModuleArtifacts& Artifacts,
		const EAngelscriptCacheRecordKind Kind)
	{
		int32 Count = 0;
		for (const FAngelscriptPreparedRecord& Record : Artifacts.Records)
		{
			Count += Record.RecordId.Kind == Kind ? 1 : 0;
		}
		return Count;
	}

	static bool AreArtifactsByteIdentical(
		const FAngelscriptCacheCleanModuleArtifacts& A,
		const FAngelscriptCacheCleanModuleArtifacts& B)
	{
		if (A.ModuleKey != B.ModuleKey
			|| A.SourceSnapshot != B.SourceSnapshot
			|| !(A.SourceIndexRecordId == B.SourceIndexRecordId)
			|| !(A.ModuleSnapshot.RecordId == B.ModuleSnapshot.RecordId)
			|| A.Records.Num() != B.Records.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < A.Records.Num(); ++Index)
		{
			if (!(A.Records[Index].RecordId == B.Records[Index].RecordId)
				|| A.Records[Index].CanonicalPayload
					!= B.Records[Index].CanonicalPayload)
			{
				return false;
			}
		}
		return true;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheColdGenerationTests,
	"Angelscript.TestModule.Cache.ColdGeneration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(NormalCompilePublishesAndFreshSessionReopensStableCompleteGeneration)
	{
		using namespace AngelscriptCacheColdGenerationTests_Private;

		const FAngelscriptCacheCleanCaptureOptions Options = MakeCaptureOptions();
		FAngelscriptCacheCleanModuleArtifacts FirstArtifacts;
		uint32 FirstValidatedGraphRecordCount = 0;
		ASSERT_THAT(IsTrue(CaptureOnce(*TestRunner, Options, FirstArtifacts,
			FirstValidatedGraphRecordCount)));
		FAngelscriptCacheCleanModuleArtifacts SecondArtifacts;
		uint32 SecondValidatedGraphRecordCount = 0;
		ASSERT_THAT(IsTrue(CaptureOnce(*TestRunner, Options, SecondArtifacts,
			SecondValidatedGraphRecordCount)));

		ASSERT_THAT(AreEqual(uint32(7), FirstValidatedGraphRecordCount,
			TEXT("The first isolated engine must validate the complete seven-record graph")));
		ASSERT_THAT(AreEqual(uint32(7), SecondValidatedGraphRecordCount,
			TEXT("The second isolated engine must validate the complete seven-record graph")));
		ASSERT_THAT(IsTrue(AreArtifactsByteIdentical(
			FirstArtifacts, SecondArtifacts),
			TEXT("Two isolated engines must freeze byte-identical pointer-free records")));
		ASSERT_THAT(AreEqual(7, FirstArtifacts.Records.Num()));
		ASSERT_THAT(AreEqual(1, CountRecordKind(
			FirstArtifacts, EAngelscriptCacheRecordKind::SourceIndex)));
		ASSERT_THAT(AreEqual(1, CountRecordKind(
			FirstArtifacts, EAngelscriptCacheRecordKind::ModuleInterface)));
		ASSERT_THAT(AreEqual(1, CountRecordKind(
			FirstArtifacts, EAngelscriptCacheRecordKind::TypeSchema)));
		ASSERT_THAT(AreEqual(1, CountRecordKind(
			FirstArtifacts, EAngelscriptCacheRecordKind::ModuleState)));
		ASSERT_THAT(AreEqual(1, CountRecordKind(
			FirstArtifacts, EAngelscriptCacheRecordKind::FunctionBody)));
		ASSERT_THAT(AreEqual(1, CountRecordKind(
			FirstArtifacts, EAngelscriptCacheRecordKind::DebugSidecar)));
		ASSERT_THAT(AreEqual(1, CountRecordKind(
			FirstArtifacts, EAngelscriptCacheRecordKind::ModuleSnapshot)));

		FAngelscriptCachePackPolicy PackPolicy;
		PackPolicy.CompressionPolicy =
			EAngelscriptCachePackCompressionPolicy::ForceNoneForTest;
		FAngelscriptUnrealZlibCacheStorageCodec Codec;
		FAngelscriptCachePreparedColdGeneration Generation;
		const FAngelscriptCacheCleanCaptureResult Prepare =
			PrepareAngelscriptCacheColdGeneration(
				FirstArtifacts, Options, PackPolicy, Codec, Generation);
		TestRunner->AddInfo(FString::Printf(
			TEXT("Cache V2 cold generation: Error=%u Packs=%d Generation=%s Detail=%s"),
			static_cast<uint32>(Prepare.Error),
			Generation.Packs.Num(),
			*Generation.EncodedManifest.ComputedGenerationId.ToHexString(),
			*Prepare.Detail));
		ASSERT_THAT(IsTrue(Prepare.IsSuccess()));
		ASSERT_THAT(IsTrue(Generation.Packs.Num() > 0));

		FScopedDiskRoot Disk;
		TUniquePtr<IAngelscriptCacheAtomicFileOps> FileOps =
			CreateAngelscriptCacheAtomicFileOps();
		TUniquePtr<IAngelscriptCacheNamespaceLockOps> LockOps =
			CreateAngelscriptCacheNamespaceLockOps();
		ASSERT_THAT(IsNotNull(FileOps.Get()));
		ASSERT_THAT(IsNotNull(LockOps.Get()));

		FAngelscriptCacheStorePaths Paths;
		ASSERT_THAT(IsTrue(BuildAngelscriptCacheStorePaths(
			Disk.Root / TEXT("CacheV2"),
			Options.Compatibility,
			Options.Context,
			*FileOps,
			Paths).IsSuccess()));
		ASSERT_THAT(IsTrue(EnsureAngelscriptCacheStoreDirectories(
			Paths, *FileOps).IsSuccess()));
		const TOptional<FAngelscriptCacheWriterToken> WriterToken =
			FAngelscriptCacheWriterToken::TryParse(
				TEXT("8201-00112233445566778899aabbccddeeff"));
		ASSERT_THAT(IsTrue(WriterToken.IsSet()));

		FAngelscriptCacheReadLimits Limits;
		FAngelscriptCacheReadBudget PublicationBudget;
		const FAngelscriptCacheStoreResult Publication =
			PublishAngelscriptCacheGeneration(
				Paths,
				EAngelscriptCachePublicationDisposition::Current,
				TOptional<FAngelscriptHash256>{},
				Generation.Packs,
				Generation.Manifest,
				Generation.EncodedManifest,
				WriterToken.GetValue(),
				Limits,
				PublicationBudget,
				FPlatformTime::Seconds() + 2.0,
				[]() { return false; },
				Codec,
				*LockOps,
				*FileOps);
		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreError::None,
			Publication.Error));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreCommitState::CurrentCommitted,
			Publication.CommitState));

		FAngelscriptCacheReadSelection Selection;
		Selection.Compatibility = Options.Compatibility;
		Selection.Context = Options.Context;
		Selection.Profile = Options.Profile;
		Selection.SourceSnapshot = FirstArtifacts.SourceSnapshot;
		TUniquePtr<FAngelscriptCacheReadSession> Session;
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
				Session);

		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreError::None, Open.Error));
		ASSERT_THAT(IsNotNull(Session.Get()));
		ASSERT_THAT(IsTrue(Session->GetGenerationId()
			== Generation.EncodedManifest.ComputedGenerationId));
		ASSERT_THAT(AreEqual(7,
			Session->GetGeneration().ReachableRecords.Num()));
		ASSERT_THAT(IsTrue(Session->GetPinnedPackCount() > 0));
		TestRunner->AddInfo(FString::Printf(
			TEXT("Cache V2 reopened: Pointer=%u Records=%d PinnedPacks=%d StoredBytes=%llu"),
			static_cast<uint32>(Session->GetPointerKind()),
			Session->GetGeneration().ReachableRecords.Num(),
			Session->GetPinnedPackCount(),
			Session->GetBudget().GetStoredBytes()));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
