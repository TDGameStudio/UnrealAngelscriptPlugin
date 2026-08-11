#include "Cache/AngelscriptCacheStore.h"
#include "Cache/AngelscriptCacheArchive.h"
#include "Cache/AngelscriptCacheManifestPack.h"
#include "Cache/AngelscriptCacheSemanticRecords.h"

#include "CQTest.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheStoreGenerationReadTests,
	"Angelscript.TestModule.Cache.StoreGenerationRead",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	class FHeldNamespaceLock final : public IAngelscriptCacheNamespaceLockHandle
	{
	};

	class FMemoryFileOps final : public IAngelscriptCacheAtomicFileOps
	{
	public:
		TMap<FString, TArray<uint8>> Files;
		TMap<FString, FAngelscriptCacheStoreResult> ReadFailures;
		TArray<FString> Reads;

		virtual FAngelscriptCacheStoreResult CanonicalizeAndValidateRoot(
			const FString&,
			FAngelscriptCanonicalCacheRoot&) override
		{
			return Unsupported();
		}

		virtual FAngelscriptCacheStoreResult EnsureDirectoryTree(
			const FString&) override
		{
			return Unsupported();
		}

		virtual FAngelscriptCacheStoreResult WriteFlushClose(
			const FString&,
			TConstArrayView<uint8>) override
		{
			return Unsupported();
		}

		virtual FAngelscriptCacheStoreResult ReopenReadAll(
			const FString& Path,
			const uint64 MaxBytes,
			TArray<uint8>& OutBytes) override
		{
			Reads.Add(Path);
			if (const FAngelscriptCacheStoreResult* Failure = ReadFailures.Find(Path))
			{
				OutBytes.Reset();
				return *Failure;
			}
			const TArray<uint8>* Existing = Files.Find(Path);
			if (Existing == nullptr)
			{
				OutBytes.Reset();
				return FAngelscriptCacheStoreResult::Failure(
					Path.EndsWith(TEXT(".asmanifest"))
						? EAngelscriptCacheStoreError::ManifestMissing
						: EAngelscriptCacheStoreError::PackMissing,
					EAngelscriptCacheStoreStage::None);
			}
			if (static_cast<uint64>(Existing->Num()) > MaxBytes)
			{
				OutBytes.Reset();
				return FAngelscriptCacheStoreResult::Failure(
					EAngelscriptCacheStoreError::ReadFailed,
					EAngelscriptCacheStoreStage::None);
			}
			OutBytes = *Existing;
			return FAngelscriptCacheStoreResult::Success();
		}

		virtual FAngelscriptCacheStoreResult EnumerateDirectFileNames(
			const FString&,
			TArray<FString>& OutFileNames) override
		{
			OutFileNames.Reset();
			return Unsupported();
		}

		virtual FAngelscriptCacheStoreResult RenameNewImmutable(
			const FString&,
			const FString&) override
		{
			return Unsupported();
		}

		virtual FAngelscriptCacheStoreResult AtomicInstallOrReplacePointer(
			const FString&,
			const FString&) override
		{
			return Unsupported();
		}

		virtual FAngelscriptCacheStoreResult AtomicRemovePointer(
			const FString&) override
		{
			return Unsupported();
		}

		virtual FAngelscriptCacheStoreResult RemoveOwnTemp(
			const FString&) override
		{
			return Unsupported();
		}

		virtual FAngelscriptCacheStoreResult RemoveFinalImmutable(
			const FString&) override
		{
			return Unsupported();
		}

		virtual FAngelscriptCacheStoreResult SyncDirectory(
			const FString&) override
		{
			return Unsupported();
		}

		virtual bool SupportsSharedAtomicCacheStore() const override
		{
			return true;
		}

	private:
		static FAngelscriptCacheStoreResult Unsupported()
		{
			return FAngelscriptCacheStoreResult::Failure(
				EAngelscriptCacheStoreError::UnsupportedPlatformAtomicity,
				EAngelscriptCacheStoreStage::None);
		}
	};

	struct FGenerationFixture
	{
		FAngelscriptEncodedPack Pack;
		FAngelscriptEncodedCacheGenerationManifest Manifest;
	};

	static FAngelscriptHash256 RepeatedByteHash(const uint8 Byte)
	{
		FBlake3Hash::ByteArray Bytes{};
		FMemory::Memset(Bytes, Byte, sizeof(Bytes));
		return FAngelscriptHash256{FBlake3Hash(Bytes)};
	}

	static FAngelscriptCacheStorePaths MakePaths()
	{
		FAngelscriptCacheStorePaths Paths;
		Paths.BaseRoot = TEXT("D:/Saved/Angelscript/CacheV2");
		Paths.BaseRootIdentity = TEXT("d:/saved/angelscript/cachev2");
		Paths.NamespaceRoot = Paths.BaseRoot / TEXT("compat/context");
		Paths.NamespaceIdentity = Paths.BaseRootIdentity / TEXT("compat/context");
		Paths.PacksDirectory = Paths.NamespaceRoot / TEXT("Packs");
		Paths.GenerationsDirectory = Paths.NamespaceRoot / TEXT("Generations");
		Paths.CurrentPointer = Paths.NamespaceRoot / TEXT("Current");
		Paths.PreviousPointer = Paths.NamespaceRoot / TEXT("Previous");
		Paths.PendingColdStartPointer = Paths.NamespaceRoot / TEXT("PendingColdStart");
		return Paths;
	}

	static FGenerationFixture MakeGenerationFixture()
	{
		FAngelscriptCachedSourceIndex Source;
		Source.PayloadSchemaVersion =
			FAngelscriptCacheSemanticArchive::SourceIndexPayloadSchemaVersion;
		Source.DiscoveryPolicy.PolicyVersion = 1;
		check(FAngelscriptCacheSemanticArchive::ComputeSourceSnapshot(
			Source, Source.SourceSnapshot).IsSuccess());

		FAngelscriptPreparedRecord SourceRecord;
		check(FAngelscriptCacheSemanticArchive::SerializeSourceIndex(
			Source, SourceRecord.CanonicalPayload).IsSuccess());
		check(FAngelscriptCacheRecordArchive::TryBuildRecordId(
			EAngelscriptCacheRecordKind::SourceIndex,
			SourceRecord.CanonicalPayload,
			SourceRecord.RecordId).IsSuccess());

		FAngelscriptCachePackPolicy Policy;
		Policy.CompressionPolicy =
			EAngelscriptCachePackCompressionPolicy::ForceNoneForTest;
		FAngelscriptUnrealZlibCacheStorageCodec Codec;
		TArray<FAngelscriptEncodedPack> Packs;
		check(BuildAngelscriptCachePacks(
			MakeArrayView(&SourceRecord, 1), Policy, Codec, Packs).IsSuccess());
		check(Packs.Num() == 1 && Packs[0].Index.Num() == 1);

		FGenerationFixture Fixture;
		Fixture.Pack = MoveTemp(Packs[0]);
		FAngelscriptCacheGenerationManifest Value;
		Value.ManifestSchemaVersion =
			FAngelscriptCacheManifestPackArchive::ManifestSchemaVersion;
		Value.Compatibility.Hash = RepeatedByteHash(0x31);
		Value.Context.Hash = RepeatedByteHash(0x52);
		Value.Profile = FAngelscriptArtifactIdentityBuilder::BuildArtifactProfileKey(
			Value.Compatibility, Value.Context);
		Value.SourceSnapshot = Source.SourceSnapshot;
		Value.SourceIndexRecordId = SourceRecord.RecordId;
		const FAngelscriptCachePackIndexEntry& PackIndex = Fixture.Pack.Index[0];
		FAngelscriptCacheRecordIndexEntry& Entry = Value.Records.AddDefaulted_GetRef();
		Entry.RecordId = SourceRecord.RecordId;
		Entry.Location.PackId = Fixture.Pack.PackId;
		Entry.Location.PackOffset = PackIndex.PackOffset;
		Entry.Location.StoredSize = PackIndex.StoredSize;
		Entry.Location.RawSize = PackIndex.RawSize;
		Entry.Location.Codec = PackIndex.Codec;
		Entry.Location.RawChecksum = PackIndex.RawChecksum;
		check(EncodeAngelscriptCacheGenerationManifest(
			Value, Fixture.Manifest).IsSuccess());
		return Fixture;
	}

	static void InstallFixture(
		const FGenerationFixture& Fixture,
		const FAngelscriptCacheStorePaths& Paths,
		FMemoryFileOps& FileOps)
	{
		FileOps.Files.Add(
			Paths.BuildManifestPath(Fixture.Manifest.ComputedGenerationId),
			Fixture.Manifest.CompleteBytes);
		FileOps.Files.Add(
			Paths.BuildPackPath(Fixture.Pack.PackId),
			Fixture.Pack.Bytes);
	}

	void AssertCandidateFailure(
		const FAngelscriptCacheStoreResult& Result,
		const EAngelscriptCacheStoreError ExpectedError,
		const EAngelscriptCacheStorePathCategory ExpectedPathCategory)
	{
		ASSERT_THAT(AreEqual(ExpectedError, Result.Error));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreStage::CandidateValidation, Result.Stage));
		ASSERT_THAT(AreEqual(ExpectedPathCategory, Result.PathCategory));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreCommitState::NotCommitted, Result.CommitState));
	}

public:
	TEST_METHOD(ReadsManifestAndEveryReferencedPackIntoOneValidatedGeneration)
	{
		const FGenerationFixture Fixture = MakeGenerationFixture();
		const FAngelscriptCacheStorePaths Paths = MakePaths();
		FMemoryFileOps FileOps;
		InstallFixture(Fixture, Paths, FileOps);
		FHeldNamespaceLock Lock;
		FAngelscriptUnrealZlibCacheStorageCodec Codec;
		FAngelscriptCacheReadBudget Budget;
		TOptional<FAngelscriptValidatedGeneration> Generation;

		const FAngelscriptCacheStoreResult Result =
			ReadAndValidateAngelscriptCacheGenerationUnderLock(
				Paths,
				Fixture.Manifest.ComputedGenerationId,
				FAngelscriptCacheReadLimits{},
				Budget,
				Codec,
				Lock,
				FileOps,
				Generation);

		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreError::None, Result.Error));
		ASSERT_THAT(IsTrue(Generation.IsSet()));
		ASSERT_THAT(AreEqual(1, Generation->ReachableRecords.Num()));
		ASSERT_THAT(IsTrue(
			Generation->Manifest.SourceIndexRecordId
				== Generation->ReachableRecords[0]->GetRecordId()));
		ASSERT_THAT(AreEqual(2, FileOps.Reads.Num()));
		ASSERT_THAT(AreEqual(
			Paths.BuildManifestPath(Fixture.Manifest.ComputedGenerationId),
			FileOps.Reads[0]));
		ASSERT_THAT(AreEqual(
			Paths.BuildPackPath(Fixture.Pack.PackId), FileOps.Reads[1]));
	}

	TEST_METHOD(MissingManifestIsAStoreFailureAndClearsOutput)
	{
		const FGenerationFixture Fixture = MakeGenerationFixture();
		const FAngelscriptCacheStorePaths Paths = MakePaths();
		FMemoryFileOps FileOps;
		FHeldNamespaceLock Lock;
		FAngelscriptUnrealZlibCacheStorageCodec Codec;
		FAngelscriptCacheReadBudget Budget;
		TOptional<FAngelscriptValidatedGeneration> Generation(
			FAngelscriptValidatedGeneration{});

		const FAngelscriptCacheStoreResult Result =
			ReadAndValidateAngelscriptCacheGenerationUnderLock(
				Paths, Fixture.Manifest.ComputedGenerationId,
				FAngelscriptCacheReadLimits{}, Budget, Codec, Lock, FileOps, Generation);

		AssertCandidateFailure(Result,
			EAngelscriptCacheStoreError::ManifestMissing,
			EAngelscriptCacheStorePathCategory::Manifest);
		ASSERT_THAT(IsFalse(Generation.IsSet()));
		ASSERT_THAT(IsFalse(Result.ContentValidation.IsSet()));
	}

	TEST_METHOD(MissingPackPreservesTheStoreFailureInsteadOfManufacturingArchiveCorruption)
	{
		const FGenerationFixture Fixture = MakeGenerationFixture();
		const FAngelscriptCacheStorePaths Paths = MakePaths();
		FMemoryFileOps FileOps;
		FileOps.Files.Add(
			Paths.BuildManifestPath(Fixture.Manifest.ComputedGenerationId),
			Fixture.Manifest.CompleteBytes);
		FHeldNamespaceLock Lock;
		FAngelscriptUnrealZlibCacheStorageCodec Codec;
		FAngelscriptCacheReadBudget Budget;
		TOptional<FAngelscriptValidatedGeneration> Generation(
			FAngelscriptValidatedGeneration{});

		const FAngelscriptCacheStoreResult Result =
			ReadAndValidateAngelscriptCacheGenerationUnderLock(
				Paths, Fixture.Manifest.ComputedGenerationId,
				FAngelscriptCacheReadLimits{}, Budget, Codec, Lock, FileOps, Generation);

		AssertCandidateFailure(Result,
			EAngelscriptCacheStoreError::PackMissing,
			EAngelscriptCacheStorePathCategory::Pack);
		ASSERT_THAT(IsFalse(Generation.IsSet()));
		ASSERT_THAT(IsFalse(Result.ContentValidation.IsSet()));
	}

	TEST_METHOD(CorruptManifestReturnsItsExactNestedValidationAndClearsOutput)
	{
		FGenerationFixture Fixture = MakeGenerationFixture();
		const FAngelscriptCacheStorePaths Paths = MakePaths();
		Fixture.Manifest.CompleteBytes[0] ^= 0xff;
		FMemoryFileOps FileOps;
		InstallFixture(Fixture, Paths, FileOps);
		FHeldNamespaceLock Lock;
		FAngelscriptUnrealZlibCacheStorageCodec Codec;
		FAngelscriptCacheReadBudget Budget;
		TOptional<FAngelscriptValidatedGeneration> Generation(
			FAngelscriptValidatedGeneration{});

		const FAngelscriptCacheStoreResult Result =
			ReadAndValidateAngelscriptCacheGenerationUnderLock(
				Paths, Fixture.Manifest.ComputedGenerationId,
				FAngelscriptCacheReadLimits{}, Budget, Codec, Lock, FileOps, Generation);

		AssertCandidateFailure(Result,
			EAngelscriptCacheStoreError::ContentValidationFailed,
			EAngelscriptCacheStorePathCategory::Manifest);
		ASSERT_THAT(IsTrue(Result.ContentValidation.IsSet()));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheValidationError::BadMagic,
			Result.ContentValidation->Error));
		ASSERT_THAT(IsFalse(Generation.IsSet()));
	}

	TEST_METHOD(CorruptPackReturnsItsExactNestedValidationAndClearsOutput)
	{
		FGenerationFixture Fixture = MakeGenerationFixture();
		const FAngelscriptCacheStorePaths Paths = MakePaths();
		Fixture.Pack.Bytes[0] ^= 0xff;
		FMemoryFileOps FileOps;
		InstallFixture(Fixture, Paths, FileOps);
		FHeldNamespaceLock Lock;
		FAngelscriptUnrealZlibCacheStorageCodec Codec;
		FAngelscriptCacheReadBudget Budget;
		TOptional<FAngelscriptValidatedGeneration> Generation(
			FAngelscriptValidatedGeneration{});

		const FAngelscriptCacheStoreResult Result =
			ReadAndValidateAngelscriptCacheGenerationUnderLock(
				Paths, Fixture.Manifest.ComputedGenerationId,
				FAngelscriptCacheReadLimits{}, Budget, Codec, Lock, FileOps, Generation);

		AssertCandidateFailure(Result,
			EAngelscriptCacheStoreError::ContentValidationFailed,
			EAngelscriptCacheStorePathCategory::Pack);
		ASSERT_THAT(IsTrue(Result.ContentValidation.IsSet()));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheValidationStage::PackDecode,
			Result.ContentValidation->Stage));
		ASSERT_THAT(IsFalse(Generation.IsSet()));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
