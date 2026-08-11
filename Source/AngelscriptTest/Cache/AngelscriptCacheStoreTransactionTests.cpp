#include "Cache/AngelscriptCacheStore.h"
#include "Cache/AngelscriptCacheArchive.h"
#include "Cache/AngelscriptCacheManifestPack.h"
#include "Cache/AngelscriptCacheSemanticRecords.h"

#include "CQTest.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheStoreTransactionTests,
	"Angelscript.TestModule.Cache.StoreTransaction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	class FLockHandle final : public IAngelscriptCacheNamespaceLockHandle
	{
	};

	class FLockOps final : public IAngelscriptCacheNamespaceLockOps
	{
	public:
		virtual double MonotonicSeconds() const override
		{
			return Now;
		}

		virtual TUniquePtr<IAngelscriptCacheNamespaceLockHandle> TryAcquire(
			const FString& LockName,
			const FTimespan WaitSlice) override
		{
			AcquiredNames.Add(LockName);
			WaitSlices.Add(WaitSlice);
			return MakeUnique<FLockHandle>();
		}

		double Now = 1.0;
		TArray<FString> AcquiredNames;
		TArray<FTimespan> WaitSlices;
	};

	class FTransactionFileOps final : public IAngelscriptCacheAtomicFileOps
	{
	public:
		virtual FAngelscriptCacheStoreResult CanonicalizeAndValidateRoot(
			const FString& RequestedRoot,
			FAngelscriptCanonicalCacheRoot& OutRoot) override
		{
			Calls.Add(TEXT("Canonicalize:") + RequestedRoot);
			OutRoot.AbsolutePath = RequestedRoot;
			OutRoot.IdentityPath = RequestedRoot.ToLower();
			return FAngelscriptCacheStoreResult::Success();
		}

		virtual FAngelscriptCacheStoreResult EnsureDirectoryTree(
			const FString& DirectoryPath) override
		{
			Calls.Add(TEXT("Ensure:") + DirectoryPath);
			return FAngelscriptCacheStoreResult::Success();
		}

		virtual FAngelscriptCacheStoreResult WriteFlushClose(
			const FString& TempPath,
			const TConstArrayView<uint8> Bytes) override
		{
			Calls.Add(TEXT("Write:") + TempPath);
			if (TempPath == FailWritePath)
			{
				return FAngelscriptCacheStoreResult::Failure(
					EAngelscriptCacheStoreError::WriteFailed,
					EAngelscriptCacheStoreStage::None);
			}
			Files.Add(TempPath, TArray<uint8>(Bytes));
			return FAngelscriptCacheStoreResult::Success();
		}

		virtual FAngelscriptCacheStoreResult ReopenReadAll(
			const FString& Path,
			const uint64 MaxBytes,
			TArray<uint8>& OutBytes) override
		{
			Calls.Add(TEXT("Read:") + Path);
			const TArray<uint8>* Existing = Files.Find(Path);
			if (Existing == nullptr)
			{
				OutBytes.Reset();
				EAngelscriptCacheStoreError Error =
					EAngelscriptCacheStoreError::PointerInvalid;
				if (Path.EndsWith(TEXT(".asmanifest")))
				{
					Error = EAngelscriptCacheStoreError::ManifestMissing;
				}
				else if (Path.EndsWith(TEXT(".aspack")))
				{
					Error = EAngelscriptCacheStoreError::PackMissing;
				}
				return FAngelscriptCacheStoreResult::Failure(
					Error, EAngelscriptCacheStoreStage::None);
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
			const FString& DirectoryPath,
			TArray<FString>& OutFileNames) override
		{
			Calls.Add(TEXT("Enumerate:") + DirectoryPath);
			OutFileNames.Reset();
			for (const TPair<FString, TArray<uint8>>& File : Files)
			{
				if (FPaths::GetPath(File.Key).Equals(
					DirectoryPath, ESearchCase::CaseSensitive))
				{
					OutFileNames.Add(FPaths::GetCleanFilename(File.Key));
				}
			}
			return FAngelscriptCacheStoreResult::Success();
		}

		virtual FAngelscriptCacheStoreResult RenameNewImmutable(
			const FString& TempPath,
			const FString& FinalPath) override
		{
			Calls.Add(TEXT("Rename:") + TempPath + TEXT("->") + FinalPath);
			if (Files.Contains(FinalPath))
			{
				return FAngelscriptCacheStoreResult::Failure(
					EAngelscriptCacheStoreError::ImmutableObjectCollisionOrCorruption,
					EAngelscriptCacheStoreStage::None);
			}
			TArray<uint8> Bytes;
			if (!Files.RemoveAndCopyValue(TempPath, Bytes))
			{
				return FAngelscriptCacheStoreResult::Failure(
					EAngelscriptCacheStoreError::RenameFailed,
					EAngelscriptCacheStoreStage::None);
			}
			Files.Add(FinalPath, MoveTemp(Bytes));
			return FAngelscriptCacheStoreResult::Success();
		}

		virtual FAngelscriptCacheStoreResult AtomicInstallOrReplacePointer(
			const FString& TempPath,
			const FString& PointerPath) override
		{
			Calls.Add(TEXT("Replace:") + TempPath + TEXT("->") + PointerPath);
			TArray<uint8> Bytes;
			if (!Files.RemoveAndCopyValue(TempPath, Bytes))
			{
				return FAngelscriptCacheStoreResult::Failure(
					EAngelscriptCacheStoreError::AtomicReplaceFailed,
					EAngelscriptCacheStoreStage::None);
			}
			if (PointerPath == FailReplacePath)
			{
				if (bInstallBeforeReplaceFailure)
				{
					Files.Add(PointerPath, MoveTemp(Bytes));
				}
				return FAngelscriptCacheStoreResult::Failure(
					EAngelscriptCacheStoreError::AtomicReplaceFailed,
					EAngelscriptCacheStoreStage::None);
			}
			Files.Add(PointerPath, MoveTemp(Bytes));
			return FAngelscriptCacheStoreResult::Success();
		}

		virtual FAngelscriptCacheStoreResult AtomicRemovePointer(
			const FString& PointerPath) override
		{
			Calls.Add(TEXT("RemovePointer:") + PointerPath);
			Files.Remove(PointerPath);
			return FAngelscriptCacheStoreResult::Success();
		}

		virtual FAngelscriptCacheStoreResult RemoveOwnTemp(
			const FString& TempPath) override
		{
			Calls.Add(TEXT("RemoveTemp:") + TempPath);
			if (TempRemovalFailures.Contains(TempPath))
			{
				FAngelscriptCacheStoreResult Result =
					FAngelscriptCacheStoreResult::Failure(
						EAngelscriptCacheStoreError::WriteFailed,
						EAngelscriptCacheStoreStage::None);
				Result.PlatformErrorCode = 32;
				return Result;
			}
			Files.Remove(TempPath);
			return FAngelscriptCacheStoreResult::Success();
		}

		virtual FAngelscriptCacheStoreResult RemoveFinalImmutable(
			const FString&) override
		{
			return Unsupported();
		}

		virtual FAngelscriptCacheStoreResult SyncDirectory(
			const FString& DirectoryPath) override
		{
			Calls.Add(TEXT("Sync:") + DirectoryPath);
			if (DirectoryPath == FailSyncDirectory)
			{
				return FAngelscriptCacheStoreResult::Failure(
					EAngelscriptCacheStoreError::DirectorySyncFailed,
					EAngelscriptCacheStoreStage::None);
			}
			return FAngelscriptCacheStoreResult::Success();
		}

		virtual bool SupportsSharedAtomicCacheStore() const override
		{
			return true;
		}

		TMap<FString, TArray<uint8>> Files;
		TSet<FString> TempRemovalFailures;
		TArray<FString> Calls;
		FString FailWritePath;
		FString FailSyncDirectory;
		FString FailReplacePath;
		bool bInstallBeforeReplaceFailure = false;

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
		FAngelscriptCacheGenerationManifest ManifestValue;
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
		Paths.CurrentPointer = Paths.NamespaceRoot / TEXT("Current.ascurrent");
		Paths.PreviousPointer = Paths.NamespaceRoot / TEXT("Previous.ascurrent");
		Paths.PendingColdStartPointer =
			Paths.NamespaceRoot / TEXT("PendingColdStart.ascurrent");
		return Paths;
	}

	static FAngelscriptCacheWriterToken MakeWriterToken()
	{
		return FAngelscriptCacheWriterToken::TryParse(
			TEXT("8117-0123456789abcdef0123456789abcdef")).GetValue();
	}

	static FGenerationFixture MakeGenerationFixture(const uint32 PolicyVersion)
	{
		FAngelscriptCachedSourceIndex Source;
		Source.PayloadSchemaVersion =
			FAngelscriptCacheSemanticArchive::SourceIndexPayloadSchemaVersion;
		Source.DiscoveryPolicy.PolicyVersion = PolicyVersion;
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
		Fixture.ManifestValue.ManifestSchemaVersion =
			FAngelscriptCacheManifestPackArchive::ManifestSchemaVersion;
		Fixture.ManifestValue.Compatibility.Hash = RepeatedByteHash(0x31);
		Fixture.ManifestValue.Context.Hash = RepeatedByteHash(0x52);
		Fixture.ManifestValue.Profile =
			FAngelscriptArtifactIdentityBuilder::BuildArtifactProfileKey(
				Fixture.ManifestValue.Compatibility,
				Fixture.ManifestValue.Context);
		Fixture.ManifestValue.SourceSnapshot = Source.SourceSnapshot;
		Fixture.ManifestValue.SourceIndexRecordId = SourceRecord.RecordId;
		const FAngelscriptCachePackIndexEntry& PackIndex = Fixture.Pack.Index[0];
		FAngelscriptCacheRecordIndexEntry& Entry =
			Fixture.ManifestValue.Records.AddDefaulted_GetRef();
		Entry.RecordId = SourceRecord.RecordId;
		Entry.Location.PackId = Fixture.Pack.PackId;
		Entry.Location.PackOffset = PackIndex.PackOffset;
		Entry.Location.StoredSize = PackIndex.StoredSize;
		Entry.Location.RawSize = PackIndex.RawSize;
		Entry.Location.Codec = PackIndex.Codec;
		Entry.Location.RawChecksum = PackIndex.RawChecksum;
		check(EncodeAngelscriptCacheGenerationManifest(
			Fixture.ManifestValue, Fixture.Manifest).IsSuccess());
		return Fixture;
	}

	static FGenerationFixture MakeTwoPackReferenceFixture(
		const uint32 PolicyVersion)
	{
		FGenerationFixture Fixture = MakeGenerationFixture(PolicyVersion);
		FAngelscriptCacheRecordIndexEntry Extra = Fixture.ManifestValue.Records[0];
		Extra.RecordId = {
			EAngelscriptCacheRecordKind::FunctionBody,
			RepeatedByteHash(0x6a)};
		Extra.Location.PackId = RepeatedByteHash(0x6b);
		Fixture.ManifestValue.Records.Add(MoveTemp(Extra));
		Fixture.ManifestValue.Records.Sort([](
			const FAngelscriptCacheRecordIndexEntry& Left,
			const FAngelscriptCacheRecordIndexEntry& Right)
		{
			return Left.RecordId < Right.RecordId;
		});
		check(EncodeAngelscriptCacheGenerationManifest(
			Fixture.ManifestValue, Fixture.Manifest).IsSuccess());
		return Fixture;
	}

	static FGenerationFixture MakeSameSemanticDifferentPackFixture(
		const uint32 PolicyVersion)
	{
		FGenerationFixture Fixture = MakeGenerationFixture(PolicyVersion);
		FAngelscriptCachedSourceIndex Source;
		Source.PayloadSchemaVersion =
			FAngelscriptCacheSemanticArchive::SourceIndexPayloadSchemaVersion;
		Source.DiscoveryPolicy.PolicyVersion = PolicyVersion;
		check(FAngelscriptCacheSemanticArchive::ComputeSourceSnapshot(
			Source, Source.SourceSnapshot).IsSuccess());
		FAngelscriptPreparedRecord SourceRecord;
		check(FAngelscriptCacheSemanticArchive::SerializeSourceIndex(
			Source, SourceRecord.CanonicalPayload).IsSuccess());
		check(FAngelscriptCacheRecordArchive::TryBuildRecordId(
			EAngelscriptCacheRecordKind::SourceIndex,
			SourceRecord.CanonicalPayload,
			SourceRecord.RecordId).IsSuccess());

		FAngelscriptPreparedRecord HistoricalExtra;
		HistoricalExtra.CanonicalPayload = {0x73, 0x61, 0x6d, 0x65};
		check(FAngelscriptCacheRecordArchive::TryBuildRecordId(
			EAngelscriptCacheRecordKind::FunctionBody,
			HistoricalExtra.CanonicalPayload,
			HistoricalExtra.RecordId).IsSuccess());
		TArray<FAngelscriptPreparedRecord> Records = {
			MoveTemp(SourceRecord),
			MoveTemp(HistoricalExtra),
		};
		FAngelscriptCachePackPolicy Policy;
		Policy.CompressionPolicy =
			EAngelscriptCachePackCompressionPolicy::ForceNoneForTest;
		FAngelscriptUnrealZlibCacheStorageCodec Codec;
		TArray<FAngelscriptEncodedPack> Packs;
		check(BuildAngelscriptCachePacks(Records, Policy, Codec, Packs).IsSuccess());
		check(Packs.Num() == 1 && Packs[0].Index.Num() == 2);
		const FAngelscriptCachePackIndexEntry* SourceIndex =
			Packs[0].Index.FindByPredicate([&Fixture](
				const FAngelscriptCachePackIndexEntry& Entry)
			{
				return Entry.RecordId
					== Fixture.ManifestValue.SourceIndexRecordId;
			});
		check(SourceIndex != nullptr);
		FAngelscriptCacheRecordIndexEntry& ManifestEntry =
			Fixture.ManifestValue.Records[0];
		ManifestEntry.Location.PackId = Packs[0].PackId;
		ManifestEntry.Location.PackOffset = SourceIndex->PackOffset;
		ManifestEntry.Location.StoredSize = SourceIndex->StoredSize;
		ManifestEntry.Location.RawSize = SourceIndex->RawSize;
		ManifestEntry.Location.Codec = SourceIndex->Codec;
		ManifestEntry.Location.RawChecksum = SourceIndex->RawChecksum;
		Fixture.Pack = MoveTemp(Packs[0]);
		check(EncodeAngelscriptCacheGenerationManifest(
			Fixture.ManifestValue, Fixture.Manifest).IsSuccess());
		return Fixture;
	}

	static void PutPointer(
		FTransactionFileOps& FileOps,
		const FString& Path,
		const EAngelscriptCachePointerKind Kind,
		const FAngelscriptHash256& GenerationId)
	{
		TArray<uint8> Bytes;
		check(EncodeAngelscriptCachePointer({Kind, GenerationId}, Bytes).IsSuccess());
		FileOps.Files.Add(Path, MoveTemp(Bytes));
	}

	static void InstallGeneration(
		const FGenerationFixture& Fixture,
		const FAngelscriptCacheStorePaths& Paths,
		FTransactionFileOps& FileOps)
	{
		FileOps.Files.Add(
			Paths.BuildPackPath(Fixture.Pack.PackId), Fixture.Pack.Bytes);
		FileOps.Files.Add(
			Paths.BuildManifestPath(Fixture.Manifest.ComputedGenerationId),
			Fixture.Manifest.CompleteBytes);
	}

	FAngelscriptCacheStoreResult PublishWithLimits(
		const FGenerationFixture& Fixture,
		const EAngelscriptCachePublicationDisposition Disposition,
		const TOptional<FAngelscriptHash256>& ObservedGeneration,
		const FAngelscriptCacheReadLimits& Limits,
		FLockOps& LockOps,
		FTransactionFileOps& FileOps,
		FAngelscriptCacheReadBudget& Budget,
		TFunctionRef<bool()> IsCancellationRequested = []() { return false; })
	{
		FAngelscriptUnrealZlibCacheStorageCodec Codec;
		return PublishAngelscriptCacheGeneration(
			MakePaths(),
			Disposition,
			ObservedGeneration,
			MakeArrayView(&Fixture.Pack, 1),
			Fixture.ManifestValue,
			Fixture.Manifest,
			MakeWriterToken(),
			Limits,
			Budget,
			10.0,
			IsCancellationRequested,
			Codec,
			LockOps,
			FileOps);
	}

	FAngelscriptCacheStoreResult Publish(
		const FGenerationFixture& Fixture,
		const EAngelscriptCachePublicationDisposition Disposition,
		const TOptional<FAngelscriptHash256>& ObservedGeneration,
		FLockOps& LockOps,
		FTransactionFileOps& FileOps,
		FAngelscriptCacheReadBudget& Budget,
		TFunctionRef<bool()> IsCancellationRequested = []() { return false; })
	{
		return PublishWithLimits(
			Fixture,
			Disposition,
			ObservedGeneration,
			FAngelscriptCacheReadLimits{},
			LockOps,
			FileOps,
			Budget,
			IsCancellationRequested);
	}

	void AssertPointer(
		FTransactionFileOps& FileOps,
		const FString& Path,
		const EAngelscriptCachePointerKind Kind,
		const FAngelscriptHash256& ExpectedGeneration)
	{
		const TArray<uint8>* Bytes = FileOps.Files.Find(Path);
		ASSERT_THAT(IsNotNull(Bytes));
		FAngelscriptCachePointerValue Value;
		ASSERT_THAT(IsTrue(
			DecodeAngelscriptCachePointer(*Bytes, Kind, Value).IsSuccess()));
		ASSERT_THAT(IsTrue(Value.GenerationId == ExpectedGeneration));
	}

	static bool HasMutationCall(const TArray<FString>& Calls)
	{
		for (const FString& Call : Calls)
		{
			if (Call.StartsWith(TEXT("Write:"))
				|| Call.StartsWith(TEXT("Rename:"))
				|| Call.StartsWith(TEXT("Replace:"))
				|| Call.StartsWith(TEXT("RemovePointer:")))
			{
				return true;
			}
		}
		return false;
	}

public:
	TEST_METHOD(PublisherRejectsManifestAbovePackLimitBeforeLockOrFilesystemCalls)
	{
		const FGenerationFixture Fixture = MakeTwoPackReferenceFixture(1);
		FAngelscriptCacheReadLimits Limits;
		Limits.MaxGenerationPacks = 1;
		FLockOps LockOps;
		FTransactionFileOps FileOps;
		FAngelscriptCacheReadBudget Budget;

		const FAngelscriptCacheStoreResult Result = PublishWithLimits(
			Fixture,
			EAngelscriptCachePublicationDisposition::Current,
			TOptional<FAngelscriptHash256>{},
			Limits,
			LockOps,
			FileOps,
			Budget);

		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreError::ContentValidationFailed,
			Result.Error));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreStage::SessionPin,
			Result.Stage));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreCommitState::NotCommitted,
			Result.CommitState));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStorePathCategory::Manifest,
			Result.PathCategory));
		ASSERT_THAT(IsTrue(Result.ContentValidation.IsSet()));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheValidationError::BudgetExceeded,
			Result.ContentValidation->Error));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheValidationStage::ManifestDecode,
			Result.ContentValidation->Stage));
		ASSERT_THAT(AreEqual(UINT64_C(181),
			Result.ContentValidation->ByteOffset));
		ASSERT_THAT(AreEqual(0, LockOps.AcquiredNames.Num()));
		ASSERT_THAT(AreEqual(0, FileOps.Calls.Num(),
			TEXT("Pack-count refusal must precede every filesystem call")));
		ASSERT_THAT(AreEqual(0, FileOps.Files.Num()));
	}

	TEST_METHOD(PublisherAcceptsExactlyOnePackWhenPositiveLimitIsOne)
	{
		const FGenerationFixture Fixture = MakeGenerationFixture(1);
		FAngelscriptCacheReadLimits Limits;
		Limits.MaxGenerationPacks = 1;
		FLockOps LockOps;
		FTransactionFileOps FileOps;
		FAngelscriptCacheReadBudget Budget;

		const FAngelscriptCacheStoreResult Result = PublishWithLimits(
			Fixture,
			EAngelscriptCachePublicationDisposition::Current,
			TOptional<FAngelscriptHash256>{},
			Limits,
			LockOps,
			FileOps,
			Budget);

		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreError::None, Result.Error));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreCommitState::CurrentCommitted,
			Result.CommitState));
		ASSERT_THAT(AreEqual(1, LockOps.AcquiredNames.Num()));
	}

	TEST_METHOD(CancellationAfterPackCommitLeavesOnlyAnOrphanPackFinal)
	{
		const FGenerationFixture Fixture = MakeGenerationFixture(1);
		const FAngelscriptCacheStorePaths Paths = MakePaths();
		const FString PackFinal = Paths.BuildPackPath(Fixture.Pack.PackId);
		FLockOps LockOps;
		FTransactionFileOps FileOps;
		FAngelscriptCacheReadBudget Budget;

		const FAngelscriptCacheStoreResult Result = Publish(
			Fixture,
			EAngelscriptCachePublicationDisposition::Current,
			TOptional<FAngelscriptHash256>{},
			LockOps,
			FileOps,
			Budget,
			[&]() { return FileOps.Files.Contains(PackFinal); });

		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreError::Cancelled, Result.Error));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreStage::ManifestTemp, Result.Stage));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreCommitState::NotCommitted,
			Result.CommitState));
		ASSERT_THAT(IsTrue(FileOps.Files.Contains(PackFinal)));
		ASSERT_THAT(IsFalse(FileOps.Files.Contains(Paths.BuildManifestPath(
			Fixture.Manifest.ComputedGenerationId))));
		ASSERT_THAT(IsFalse(FileOps.Files.Contains(Paths.CurrentPointer)));
		for (const TPair<FString, TArray<uint8>>& File : FileOps.Files)
		{
			ASSERT_THAT(IsFalse(File.Key.Contains(TEXT(".tmp."))));
		}
	}

	TEST_METHOD(ManifestWriteFailureLeavesOnlyAnOrphanPackAndCleansItsTemp)
	{
		const FGenerationFixture Fixture = MakeGenerationFixture(1);
		const FAngelscriptCacheStorePaths Paths = MakePaths();
		FLockOps LockOps;
		FTransactionFileOps FileOps;
		FileOps.FailWritePath = Paths.BuildManifestTempPath(
			Fixture.Manifest.ComputedGenerationId, MakeWriterToken());
		FAngelscriptCacheReadBudget Budget;

		const FAngelscriptCacheStoreResult Result = Publish(
			Fixture, EAngelscriptCachePublicationDisposition::Current,
			TOptional<FAngelscriptHash256>{}, LockOps, FileOps, Budget);

		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreError::WriteFailed, Result.Error));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreStage::ManifestTemp, Result.Stage));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreCommitState::NotCommitted,
			Result.CommitState));
		ASSERT_THAT(IsTrue(FileOps.Files.Contains(
			Paths.BuildPackPath(Fixture.Pack.PackId))));
		ASSERT_THAT(IsFalse(FileOps.Files.Contains(
			Paths.BuildManifestPath(Fixture.Manifest.ComputedGenerationId))));
		ASSERT_THAT(IsFalse(FileOps.Files.Contains(FileOps.FailWritePath)));
		ASSERT_THAT(IsFalse(FileOps.Files.Contains(Paths.CurrentPointer)));
	}

	TEST_METHOD(ManifestSyncFailureLeavesBothImmutableFinalsButNoPointer)
	{
		const FGenerationFixture Fixture = MakeGenerationFixture(1);
		const FAngelscriptCacheStorePaths Paths = MakePaths();
		FLockOps LockOps;
		FTransactionFileOps FileOps;
		FileOps.FailSyncDirectory = Paths.GenerationsDirectory;
		FAngelscriptCacheReadBudget Budget;

		const FAngelscriptCacheStoreResult Result = Publish(
			Fixture, EAngelscriptCachePublicationDisposition::Current,
			TOptional<FAngelscriptHash256>{}, LockOps, FileOps, Budget);

		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreError::DirectorySyncFailed, Result.Error));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreStage::ManifestFinal, Result.Stage));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreCommitState::NotCommitted,
			Result.CommitState));
		ASSERT_THAT(IsTrue(FileOps.Files.Contains(
			Paths.BuildPackPath(Fixture.Pack.PackId))));
		ASSERT_THAT(IsTrue(FileOps.Files.Contains(
			Paths.BuildManifestPath(Fixture.Manifest.ComputedGenerationId))));
		ASSERT_THAT(IsFalse(FileOps.Files.Contains(Paths.CurrentPointer)));
		for (const TPair<FString, TArray<uint8>>& File : FileOps.Files)
		{
			ASSERT_THAT(IsFalse(File.Key.Contains(TEXT(".tmp."))));
		}
	}

	TEST_METHOD(FailedCurrentReplaceKeepsOldCurrentAndNewFinalsAsOrphans)
	{
		const FGenerationFixture Old = MakeGenerationFixture(1);
		const FGenerationFixture Prepared = MakeGenerationFixture(2);
		const FAngelscriptCacheStorePaths Paths = MakePaths();
		FLockOps LockOps;
		FTransactionFileOps FileOps;
		InstallGeneration(Old, Paths, FileOps);
		PutPointer(FileOps, Paths.CurrentPointer,
			EAngelscriptCachePointerKind::Current,
			Old.Manifest.ComputedGenerationId);
		FileOps.FailReplacePath = Paths.CurrentPointer;
		FAngelscriptCacheReadBudget Budget;

		const FAngelscriptCacheStoreResult Result = Publish(
			Prepared, EAngelscriptCachePublicationDisposition::Current,
			Old.Manifest.ComputedGenerationId, LockOps, FileOps, Budget);

		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreError::AtomicReplaceFailed, Result.Error));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreCommitState::NotCommitted,
			Result.CommitState));
		AssertPointer(FileOps, Paths.CurrentPointer,
			EAngelscriptCachePointerKind::Current,
			Old.Manifest.ComputedGenerationId);
		AssertPointer(FileOps, Paths.PreviousPointer,
			EAngelscriptCachePointerKind::Previous,
			Old.Manifest.ComputedGenerationId);
		ASSERT_THAT(IsTrue(FileOps.Files.Contains(
			Paths.BuildPackPath(Prepared.Pack.PackId))));
		ASSERT_THAT(IsTrue(FileOps.Files.Contains(
			Paths.BuildManifestPath(Prepared.Manifest.ComputedGenerationId))));
	}

	TEST_METHOD(IndeterminateCurrentReplaceRereadsAndReportsCommittedWinner)
	{
		const FGenerationFixture Old = MakeGenerationFixture(1);
		const FGenerationFixture Prepared = MakeGenerationFixture(2);
		const FAngelscriptCacheStorePaths Paths = MakePaths();
		FLockOps LockOps;
		FTransactionFileOps FileOps;
		InstallGeneration(Old, Paths, FileOps);
		PutPointer(FileOps, Paths.CurrentPointer,
			EAngelscriptCachePointerKind::Current,
			Old.Manifest.ComputedGenerationId);
		FileOps.FailReplacePath = Paths.CurrentPointer;
		FileOps.bInstallBeforeReplaceFailure = true;
		FAngelscriptCacheReadBudget Budget;

		const FAngelscriptCacheStoreResult Result = Publish(
			Prepared, EAngelscriptCachePublicationDisposition::Current,
			Old.Manifest.ComputedGenerationId, LockOps, FileOps, Budget);

		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreError::AtomicReplaceFailed, Result.Error));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreCommitState::CurrentCommitted,
			Result.CommitState));
		ASSERT_THAT(IsTrue(Result.GenerationAfter.IsSet()));
		ASSERT_THAT(IsTrue(Result.GenerationAfter.GetValue()
			== Prepared.Manifest.ComputedGenerationId));
		AssertPointer(FileOps, Paths.CurrentPointer,
			EAngelscriptCachePointerKind::Current,
			Prepared.Manifest.ComputedGenerationId);
		AssertPointer(FileOps, Paths.PreviousPointer,
			EAngelscriptCachePointerKind::Previous,
			Old.Manifest.ComputedGenerationId);
	}

	TEST_METHOD(PublisherCleansStrictStaleTempsBeforeReadingAnyRoot)
	{
		const FGenerationFixture Fixture = MakeGenerationFixture(1);
		const FAngelscriptCacheStorePaths Paths = MakePaths();
		const FAngelscriptCacheWriterToken OldToken =
			FAngelscriptCacheWriterToken::TryParse(
				TEXT("7001-fedcba9876543210fedcba9876543210")).GetValue();
		const TArray<FString> StaleTemps = {
			Paths.BuildPackTempPath(RepeatedByteHash(0x61), OldToken),
			Paths.BuildManifestTempPath(RepeatedByteHash(0x62), OldToken),
			Paths.BuildCurrentPointerTempPath(OldToken),
			Paths.BuildPreviousPointerTempPath(OldToken),
			Paths.BuildPendingColdStartPointerTempPath(OldToken),
		};
		const FString FinalPack = Paths.BuildPackPath(RepeatedByteHash(0x63));
		const FString InvalidToken = Paths.PacksDirectory
			/ (RepeatedByteHash(0x64).ToHexString()
				+ TEXT(".aspack.tmp.7001-FEDCBA9876543210FEDCBA9876543210"));
		FLockOps LockOps;
		FTransactionFileOps FileOps;
		for (const FString& Path : StaleTemps)
		{
			FileOps.Files.Add(Path, TArray<uint8>{0x41});
		}
		FileOps.Files.Add(FinalPack, TArray<uint8>{0x51});
		FileOps.Files.Add(InvalidToken, TArray<uint8>{0x61});
		FAngelscriptCacheReadBudget Budget;

		const FAngelscriptCacheStoreResult Result = Publish(
			Fixture, EAngelscriptCachePublicationDisposition::Current,
			TOptional<FAngelscriptHash256>{}, LockOps, FileOps, Budget);

		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreError::None, Result.Error));
		ASSERT_THAT(AreEqual(1, LockOps.AcquiredNames.Num()));
		for (const FString& Path : StaleTemps)
		{
			ASSERT_THAT(IsFalse(FileOps.Files.Contains(Path),
				*FString::Printf(TEXT("Publisher left stale temp: %s"), *Path)));
		}
		ASSERT_THAT(IsTrue(FileOps.Files.Contains(FinalPack)));
		ASSERT_THAT(IsTrue(FileOps.Files.Contains(InvalidToken)));

		int32 EnumerationCount = 0;
		int32 LastEnsureIndex = INDEX_NONE;
		int32 FirstEnumerationIndex = INDEX_NONE;
		int32 LastRemovalIndex = INDEX_NONE;
		int32 FirstReadIndex = INDEX_NONE;
		for (int32 Index = 0; Index < FileOps.Calls.Num(); ++Index)
		{
			const FString& Call = FileOps.Calls[Index];
			if (Call.StartsWith(TEXT("Ensure:")))
			{
				LastEnsureIndex = Index;
			}
			else if (Call.StartsWith(TEXT("Enumerate:")))
			{
				++EnumerationCount;
				if (FirstEnumerationIndex == INDEX_NONE)
				{
					FirstEnumerationIndex = Index;
				}
			}
			else if (Call.StartsWith(TEXT("RemoveTemp:")))
			{
				LastRemovalIndex = Index;
			}
			else if (Call.StartsWith(TEXT("Read:")) && FirstReadIndex == INDEX_NONE)
			{
				FirstReadIndex = Index;
			}
		}
		ASSERT_THAT(AreEqual(3, EnumerationCount));
		ASSERT_THAT(IsTrue(LastEnsureIndex < FirstEnumerationIndex));
		ASSERT_THAT(IsTrue(FirstEnumerationIndex <= LastRemovalIndex));
		ASSERT_THAT(IsTrue(LastRemovalIndex < FirstReadIndex));
	}

	TEST_METHOD(StaleTempRemovalFailureIsDiagnosticAndDoesNotBlockPublication)
	{
		const FGenerationFixture Fixture = MakeGenerationFixture(1);
		const FAngelscriptCacheStorePaths Paths = MakePaths();
		const FAngelscriptCacheWriterToken OldToken =
			FAngelscriptCacheWriterToken::TryParse(
				TEXT("7002-fedcba9876543210fedcba9876543210")).GetValue();
		const FString StaleTemp =
			Paths.BuildPackTempPath(RepeatedByteHash(0x65), OldToken);
		FLockOps LockOps;
		FTransactionFileOps FileOps;
		FileOps.Files.Add(StaleTemp, TArray<uint8>{0x41});
		FileOps.TempRemovalFailures.Add(StaleTemp);
		FAngelscriptCacheReadBudget Budget;
		TestRunner->AddExpectedErrorPlain(
			TEXT("Cache V2 stale-temp cleanup failed; publication will continue."),
			EAutomationExpectedErrorFlags::Contains,
			1);

		const FAngelscriptCacheStoreResult Result = Publish(
			Fixture, EAngelscriptCachePublicationDisposition::Current,
			TOptional<FAngelscriptHash256>{}, LockOps, FileOps, Budget);

		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreError::None, Result.Error));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreCommitState::CurrentCommitted,
			Result.CommitState));
		ASSERT_THAT(IsTrue(FileOps.Files.Contains(StaleTemp),
			TEXT("A failed best-effort cleanup must leave the stale temp intact")));
		AssertPointer(
			FileOps,
			Paths.CurrentPointer,
			EAngelscriptCachePointerKind::Current,
			Fixture.Manifest.ComputedGenerationId);
	}

	TEST_METHOD(FirstCurrentTransactionInstallsImmutableGenerationBeforeCommittingPointer)
	{
		const FGenerationFixture Fixture = MakeGenerationFixture(1);
		const FAngelscriptCacheStorePaths Paths = MakePaths();
		FLockOps LockOps;
		FTransactionFileOps FileOps;
		FAngelscriptCacheReadBudget Budget;

		const FAngelscriptCacheStoreResult Result = Publish(
			Fixture, EAngelscriptCachePublicationDisposition::Current,
			TOptional<FAngelscriptHash256>{}, LockOps, FileOps, Budget);

		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreError::None, Result.Error));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreCommitState::CurrentCommitted,
			Result.CommitState));
		ASSERT_THAT(AreEqual(1, LockOps.AcquiredNames.Num()));
		ASSERT_THAT(IsTrue(FileOps.Files.Contains(
			Paths.BuildPackPath(Fixture.Pack.PackId))));
		ASSERT_THAT(IsTrue(FileOps.Files.Contains(
			Paths.BuildManifestPath(Fixture.Manifest.ComputedGenerationId))));
		AssertPointer(FileOps, Paths.CurrentPointer,
			EAngelscriptCachePointerKind::Current,
			Fixture.Manifest.ComputedGenerationId);
		ASSERT_THAT(IsFalse(FileOps.Files.Contains(Paths.PreviousPointer)));
		for (const TPair<FString, TArray<uint8>>& File : FileOps.Files)
		{
			ASSERT_THAT(IsFalse(File.Key.Contains(TEXT(".tmp."))));
		}
	}

	TEST_METHOD(ExactCurrentIsFullyValidatedThenReturnsAWriteFreeNoOp)
	{
		const FGenerationFixture Fixture = MakeGenerationFixture(1);
		const FAngelscriptCacheStorePaths Paths = MakePaths();
		FLockOps LockOps;
		FTransactionFileOps FileOps;
		InstallGeneration(Fixture, Paths, FileOps);
		PutPointer(FileOps, Paths.CurrentPointer,
			EAngelscriptCachePointerKind::Current,
			Fixture.Manifest.ComputedGenerationId);
		FAngelscriptCacheReadBudget Budget;

		const FAngelscriptCacheStoreResult Result = Publish(
			Fixture, EAngelscriptCachePublicationDisposition::Current,
			Fixture.Manifest.ComputedGenerationId, LockOps, FileOps, Budget);

		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreError::None, Result.Error));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreCommitState::NotCommitted, Result.CommitState));
		ASSERT_THAT(IsTrue(Result.GenerationBefore.IsSet()));
		ASSERT_THAT(IsTrue(Result.GenerationAfter.IsSet()));
		ASSERT_THAT(IsTrue(Result.GenerationBefore.GetValue()
			== Fixture.Manifest.ComputedGenerationId));
		ASSERT_THAT(IsTrue(Result.GenerationAfter.GetValue()
			== Fixture.Manifest.ComputedGenerationId));
		ASSERT_THAT(IsFalse(HasMutationCall(FileOps.Calls)));
		ASSERT_THAT(AreEqual(5, FileOps.Calls.FilterByPredicate(
			[](const FString& Call) { return Call.StartsWith(TEXT("Read:")); }).Num()));
	}

	TEST_METHOD(ConcurrentSameSemanticDifferentPackWinnerReturnsWriteFreeNoOp)
	{
		const FGenerationFixture Prepared = MakeGenerationFixture(1);
		const FGenerationFixture Concurrent =
			MakeSameSemanticDifferentPackFixture(1);
		const FAngelscriptCacheStorePaths Paths = MakePaths();
		ASSERT_THAT(IsFalse(Prepared.Manifest.ComputedGenerationId
			== Concurrent.Manifest.ComputedGenerationId));
		FLockOps LockOps;
		FTransactionFileOps FileOps;
		InstallGeneration(Concurrent, Paths, FileOps);
		PutPointer(FileOps, Paths.CurrentPointer,
			EAngelscriptCachePointerKind::Current,
			Concurrent.Manifest.ComputedGenerationId);
		FileOps.Calls.Reset();
		FAngelscriptCacheReadBudget Budget;

		const FAngelscriptCacheStoreResult Result = Publish(
			Prepared,
			EAngelscriptCachePublicationDisposition::Current,
			TOptional<FAngelscriptHash256>{},
			LockOps,
			FileOps,
			Budget);

		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreError::None, Result.Error));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreCommitState::NotCommitted,
			Result.CommitState));
		ASSERT_THAT(IsTrue(Result.GenerationBefore.IsSet()));
		ASSERT_THAT(IsTrue(Result.GenerationAfter.IsSet()));
		ASSERT_THAT(IsTrue(Result.GenerationBefore.GetValue()
			== Concurrent.Manifest.ComputedGenerationId));
		ASSERT_THAT(IsTrue(Result.GenerationAfter.GetValue()
			== Concurrent.Manifest.ComputedGenerationId));
		ASSERT_THAT(IsFalse(HasMutationCall(FileOps.Calls)));
		ASSERT_THAT(IsFalse(FileOps.Files.Contains(
			Paths.BuildPackPath(Prepared.Pack.PackId))));
		ASSERT_THAT(IsFalse(FileOps.Files.Contains(
			Paths.BuildManifestPath(Prepared.Manifest.ComputedGenerationId))));
		AssertPointer(FileOps, Paths.CurrentPointer,
			EAngelscriptCachePointerKind::Current,
			Concurrent.Manifest.ComputedGenerationId);
	}

	TEST_METHOD(CurrentPublicationRereadsEveryPhysicalRootBeforeTheFirstWrite)
	{
		const FGenerationFixture Current = MakeGenerationFixture(1);
		const FGenerationFixture Previous = MakeGenerationFixture(2);
		const FGenerationFixture Pending = MakeGenerationFixture(3);
		const FGenerationFixture Prepared = MakeGenerationFixture(4);
		const FAngelscriptCacheStorePaths Paths = MakePaths();
		FLockOps LockOps;
		FTransactionFileOps FileOps;
		InstallGeneration(Current, Paths, FileOps);
		InstallGeneration(Previous, Paths, FileOps);
		InstallGeneration(Pending, Paths, FileOps);
		PutPointer(FileOps, Paths.CurrentPointer,
			EAngelscriptCachePointerKind::Current,
			Current.Manifest.ComputedGenerationId);
		PutPointer(FileOps, Paths.PreviousPointer,
			EAngelscriptCachePointerKind::Previous,
			Previous.Manifest.ComputedGenerationId);
		PutPointer(FileOps, Paths.PendingColdStartPointer,
			EAngelscriptCachePointerKind::PendingColdStart,
			Pending.Manifest.ComputedGenerationId);
		FAngelscriptCacheReadBudget Budget;

		const FAngelscriptCacheStoreResult Result = Publish(
			Prepared,
			EAngelscriptCachePublicationDisposition::Current,
			Current.Manifest.ComputedGenerationId,
			LockOps,
			FileOps,
			Budget);

		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreError::None, Result.Error));
		const int32 FirstWrite = FileOps.Calls.IndexOfByPredicate(
			[](const FString& Call)
			{
				return Call.StartsWith(TEXT("Write:"));
			});
		ASSERT_THAT(AreNotEqual(INDEX_NONE, FirstWrite));

		const TArray<FString> RequiredReads = {
			Paths.CurrentPointer,
			Paths.BuildManifestPath(Current.Manifest.ComputedGenerationId),
			Paths.BuildPackPath(Current.Pack.PackId),
			Paths.PreviousPointer,
			Paths.BuildManifestPath(Previous.Manifest.ComputedGenerationId),
			Paths.BuildPackPath(Previous.Pack.PackId),
			Paths.PendingColdStartPointer,
			Paths.BuildManifestPath(Pending.Manifest.ComputedGenerationId),
			Paths.BuildPackPath(Pending.Pack.PackId),
		};
		for (const FString& RequiredPath : RequiredReads)
		{
			const FString ExpectedCall = TEXT("Read:") + RequiredPath;
			const int32 ReadIndex = FileOps.Calls.IndexOfByKey(ExpectedCall);
			ASSERT_THAT(AreNotEqual(INDEX_NONE, ReadIndex,
				FString::Printf(TEXT("Missing locked root read: %s"), *RequiredPath)));
			if (ReadIndex != INDEX_NONE)
			{
				ASSERT_THAT(IsTrue(ReadIndex < FirstWrite,
					FString::Printf(TEXT("Root read occurred after mutation: %s"), *RequiredPath)));
			}
		}
	}

	TEST_METHOD(DuplicatePhysicalRootsValidateOneDistinctGenerationOnlyOnce)
	{
		const FGenerationFixture Fixture = MakeGenerationFixture(1);
		const FAngelscriptCacheStorePaths Paths = MakePaths();
		FLockOps LockOps;
		FTransactionFileOps FileOps;
		InstallGeneration(Fixture, Paths, FileOps);
		PutPointer(FileOps, Paths.CurrentPointer,
			EAngelscriptCachePointerKind::Current,
			Fixture.Manifest.ComputedGenerationId);
		PutPointer(FileOps, Paths.PreviousPointer,
			EAngelscriptCachePointerKind::Previous,
			Fixture.Manifest.ComputedGenerationId);
		PutPointer(FileOps, Paths.PendingColdStartPointer,
			EAngelscriptCachePointerKind::PendingColdStart,
			Fixture.Manifest.ComputedGenerationId);
		FAngelscriptCacheReadBudget Budget;

		const FAngelscriptCacheStoreResult Result = Publish(
			Fixture,
			EAngelscriptCachePublicationDisposition::Current,
			Fixture.Manifest.ComputedGenerationId,
			LockOps,
			FileOps,
			Budget);

		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreError::None, Result.Error));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreCommitState::NotCommitted,
			Result.CommitState));
		const FString ManifestRead = TEXT("Read:")
			+ Paths.BuildManifestPath(Fixture.Manifest.ComputedGenerationId);
		const FString PackRead = TEXT("Read:")
			+ Paths.BuildPackPath(Fixture.Pack.PackId);
		ASSERT_THAT(AreEqual(1, FileOps.Calls.FilterByPredicate(
			[&](const FString& Call) { return Call == ManifestRead; }).Num()));
		ASSERT_THAT(AreEqual(1, FileOps.Calls.FilterByPredicate(
			[&](const FString& Call) { return Call == PackRead; }).Num()));
		ASSERT_THAT(IsFalse(HasMutationCall(FileOps.Calls)));
	}

	TEST_METHOD(ConcurrentDifferentSourceRequiresRevalidationBeforeAnyWrite)
	{
		const FGenerationFixture Prepared = MakeGenerationFixture(1);
		const FGenerationFixture Concurrent = MakeGenerationFixture(2);
		const FAngelscriptCacheStorePaths Paths = MakePaths();
		FLockOps LockOps;
		FTransactionFileOps FileOps;
		InstallGeneration(Concurrent, Paths, FileOps);
		PutPointer(FileOps, Paths.CurrentPointer,
			EAngelscriptCachePointerKind::Current,
			Concurrent.Manifest.ComputedGenerationId);
		FAngelscriptCacheReadBudget Budget;

		const FAngelscriptCacheStoreResult Result = Publish(
			Prepared,
			EAngelscriptCachePublicationDisposition::Current,
			RepeatedByteHash(0x88),
			LockOps,
			FileOps,
			Budget);

		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreError::NeedsSourceRevalidation, Result.Error));
		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreStage::Rebase, Result.Stage));
		ASSERT_THAT(IsFalse(HasMutationCall(FileOps.Calls)));
		AssertPointer(FileOps, Paths.CurrentPointer,
			EAngelscriptCachePointerKind::Current,
			Concurrent.Manifest.ComputedGenerationId);
	}

	TEST_METHOD(CorruptSelectedCurrentIsNotRotatedAndNewGenerationRepairsCurrent)
	{
		const FGenerationFixture Prepared = MakeGenerationFixture(1);
		FGenerationFixture Selected = MakeGenerationFixture(2);
		const FGenerationFixture Previous = MakeGenerationFixture(4);
		const FAngelscriptCacheStorePaths Paths = MakePaths();
		FLockOps LockOps;
		FTransactionFileOps FileOps;
		InstallGeneration(Selected, Paths, FileOps);
		InstallGeneration(Previous, Paths, FileOps);
		FileOps.Files[Paths.BuildPackPath(Selected.Pack.PackId)][0] ^= 0xff;
		PutPointer(FileOps, Paths.CurrentPointer,
			EAngelscriptCachePointerKind::Current,
			Selected.Manifest.ComputedGenerationId);
		PutPointer(FileOps, Paths.PreviousPointer,
			EAngelscriptCachePointerKind::Previous,
			Previous.Manifest.ComputedGenerationId);
		FAngelscriptCacheReadBudget Budget;

		const FAngelscriptCacheStoreResult Result = Publish(
			Prepared,
			EAngelscriptCachePublicationDisposition::Current,
			Selected.Manifest.ComputedGenerationId,
			LockOps,
			FileOps,
			Budget);

		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreError::None, Result.Error));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreCommitState::CurrentCommitted,
			Result.CommitState));
		ASSERT_THAT(IsFalse(Result.GenerationBefore.IsSet(),
			TEXT("A corrupt old Current must not be reported as a validated base")));
		ASSERT_THAT(IsTrue(Result.GenerationAfter.IsSet()));
		ASSERT_THAT(IsTrue(Result.GenerationAfter.GetValue()
			== Prepared.Manifest.ComputedGenerationId));
		AssertPointer(FileOps, Paths.CurrentPointer,
			EAngelscriptCachePointerKind::Current,
			Prepared.Manifest.ComputedGenerationId);
		AssertPointer(FileOps, Paths.PreviousPointer,
			EAngelscriptCachePointerKind::Previous,
			Previous.Manifest.ComputedGenerationId);
		const FString PreviousReplacePrefix = TEXT("Replace:")
			+ Paths.BuildPreviousPointerTempPath(MakeWriterToken());
		ASSERT_THAT(IsFalse(FileOps.Calls.ContainsByPredicate(
			[&](const FString& Call)
			{
				return Call.StartsWith(PreviousReplacePrefix);
			}), TEXT("A corrupt Current must never be rotated into Previous")));
	}

	TEST_METHOD(FirstPendingTransactionCommitsOnlyPendingColdStart)
	{
		const FGenerationFixture Fixture = MakeGenerationFixture(3);
		const FAngelscriptCacheStorePaths Paths = MakePaths();
		FLockOps LockOps;
		FTransactionFileOps FileOps;
		FAngelscriptCacheReadBudget Budget;

		const FAngelscriptCacheStoreResult Result = Publish(
			Fixture,
			EAngelscriptCachePublicationDisposition::PendingColdStart,
			TOptional<FAngelscriptHash256>{},
			LockOps,
			FileOps,
			Budget);

		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreError::None, Result.Error));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreCommitState::PendingCommitted,
			Result.CommitState));
		AssertPointer(FileOps, Paths.PendingColdStartPointer,
			EAngelscriptCachePointerKind::PendingColdStart,
			Fixture.Manifest.ComputedGenerationId);
		ASSERT_THAT(IsFalse(FileOps.Files.Contains(Paths.CurrentPointer)));
		ASSERT_THAT(IsFalse(FileOps.Files.Contains(Paths.PreviousPointer)));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
