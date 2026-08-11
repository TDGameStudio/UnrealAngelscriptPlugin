#include "Cache/AngelscriptCacheStore.h"
#include "Cache/AngelscriptCacheArchive.h"
#include "Cache/AngelscriptCacheSemanticRecords.h"

#include "CQTest.h"
#include "Hash/Blake3.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheStoreCompactionTests,
	"Angelscript.TestModule.Cache.StoreCompaction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FCompactionFixture
	{
		FAngelscriptEncodedPack OriginalPack;
		FAngelscriptCacheGenerationManifest ManifestValue;
		FAngelscriptEncodedCacheGenerationManifest Manifest;
	};

	struct FMemoryLockState
	{
		bool bHeld = false;
		int32 AcquisitionCount = 0;
	};

	class FMemoryLockHandle final : public IAngelscriptCacheNamespaceLockHandle
	{
	public:
		explicit FMemoryLockHandle(FMemoryLockState& InState)
			: State(InState)
		{
			check(!State.bHeld);
			State.bHeld = true;
			++State.AcquisitionCount;
		}

		virtual ~FMemoryLockHandle() override
		{
			check(State.bHeld);
			State.bHeld = false;
		}

	private:
		FMemoryLockState& State;
	};

	class FMemoryLockOps final : public IAngelscriptCacheNamespaceLockOps
	{
	public:
		TFunction<void(int32)> BeforeAcquire;

		explicit FMemoryLockOps(FMemoryLockState& InState)
			: State(InState)
		{
		}

		virtual double MonotonicSeconds() const override
		{
			return 0.0;
		}

		virtual TUniquePtr<IAngelscriptCacheNamespaceLockHandle> TryAcquire(
			const FString&,
			const FTimespan) override
		{
			if (State.bHeld)
			{
				return nullptr;
			}
			if (BeforeAcquire)
			{
				BeforeAcquire(State.AcquisitionCount + 1);
			}
			return MakeUnique<FMemoryLockHandle>(State);
		}

	private:
		FMemoryLockState& State;
	};

	class FMemoryFileOps final : public IAngelscriptCacheAtomicFileOps
	{
	public:
		TMap<FString, TArray<uint8>> Files;
		TArray<FString> Calls;
		TSet<FString> DeferredFinalPaths;
		bool bFailSyncAfterFinalRemoval = false;
		TOptional<FString> FailPointerReplacePath;
		bool bInstallPointerBeforeReplaceFailure = false;

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
			const FString& TempPath,
			const TConstArrayView<uint8> Bytes) override
		{
			Calls.Add(TEXT("Write:") + TempPath);
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
				return FAngelscriptCacheStoreResult::Failure(
					Path.EndsWith(TEXT(".aspack"), ESearchCase::CaseSensitive)
						? EAngelscriptCacheStoreError::PackMissing
						: Path.EndsWith(TEXT(".asmanifest"), ESearchCase::CaseSensitive)
							? EAngelscriptCacheStoreError::ManifestMissing
							: EAngelscriptCacheStoreError::PointerInvalid,
					EAngelscriptCacheStoreStage::None);
			}
			if (static_cast<uint64>(Existing->Num()) > MaxBytes)
			{
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
			if (FailPointerReplacePath.IsSet()
				&& FailPointerReplacePath.GetValue().Equals(
					PointerPath, ESearchCase::CaseSensitive))
			{
				if (bInstallPointerBeforeReplaceFailure)
				{
					Files.Add(PointerPath, MoveTemp(Bytes));
				}
				else
				{
					Files.Add(TempPath, MoveTemp(Bytes));
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
			Files.Remove(TempPath);
			return FAngelscriptCacheStoreResult::Success();
		}

		virtual FAngelscriptCacheStoreResult RemoveFinalImmutable(
			const FString& FinalPath) override
		{
			Calls.Add(TEXT("RemoveFinal:") + FinalPath);
			if (DeferredFinalPaths.Contains(FinalPath))
			{
				return FAngelscriptCacheStoreResult::Failure(
					EAngelscriptCacheStoreError::DeleteDeferred,
					EAngelscriptCacheStoreStage::None);
			}
			Files.Remove(FinalPath);
			return FAngelscriptCacheStoreResult::Success();
		}

		virtual FAngelscriptCacheStoreResult SyncDirectory(
			const FString& DirectoryPath) override
		{
			Calls.Add(TEXT("Sync:") + DirectoryPath);
			if (bFailSyncAfterFinalRemoval
				&& Calls.ContainsByPredicate([](const FString& Call)
				{
					return Call.StartsWith(TEXT("RemoveFinal:"));
				}))
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

	private:
		static FAngelscriptCacheStoreResult Unsupported()
		{
			return FAngelscriptCacheStoreResult::Failure(
				EAngelscriptCacheStoreError::UnsupportedPlatformAtomicity,
				EAngelscriptCacheStoreStage::None);
		}
	};

	class FNoMutationLockOps final : public IAngelscriptCacheNamespaceLockOps
	{
	public:
		virtual double MonotonicSeconds() const override
		{
			return 0.0;
		}

		virtual TUniquePtr<IAngelscriptCacheNamespaceLockHandle> TryAcquire(
			const FString&,
			const FTimespan) override
		{
			++TryAcquireCount;
			return nullptr;
		}

		int32 TryAcquireCount = 0;
	};

	class FNoMutationFileOps final : public IAngelscriptCacheAtomicFileOps
	{
	public:
		virtual FAngelscriptCacheStoreResult CanonicalizeAndValidateRoot(
			const FString&,
			FAngelscriptCanonicalCacheRoot&) override
		{
			return Called();
		}

		virtual FAngelscriptCacheStoreResult EnsureDirectoryTree(
			const FString&) override
		{
			return Called();
		}

		virtual FAngelscriptCacheStoreResult WriteFlushClose(
			const FString&,
			TConstArrayView<uint8>) override
		{
			return Called();
		}

		virtual FAngelscriptCacheStoreResult ReopenReadAll(
			const FString&,
			uint64,
			TArray<uint8>&) override
		{
			return Called();
		}

		virtual FAngelscriptCacheStoreResult EnumerateDirectFileNames(
			const FString&,
			TArray<FString>&) override
		{
			return Called();
		}

		virtual FAngelscriptCacheStoreResult RenameNewImmutable(
			const FString&,
			const FString&) override
		{
			return Called();
		}

		virtual FAngelscriptCacheStoreResult AtomicInstallOrReplacePointer(
			const FString&,
			const FString&) override
		{
			return Called();
		}

		virtual FAngelscriptCacheStoreResult AtomicRemovePointer(
			const FString&) override
		{
			return Called();
		}

		virtual FAngelscriptCacheStoreResult RemoveOwnTemp(
			const FString&) override
		{
			return Called();
		}

		virtual FAngelscriptCacheStoreResult RemoveFinalImmutable(
			const FString&) override
		{
			return Called();
		}

		virtual FAngelscriptCacheStoreResult SyncDirectory(
			const FString&) override
		{
			return Called();
		}

		virtual bool SupportsSharedAtomicCacheStore() const override
		{
			return true;
		}

		FAngelscriptCacheStoreResult Called()
		{
			++CallCount;
			return FAngelscriptCacheStoreResult::Failure(
				EAngelscriptCacheStoreError::UnsupportedPlatformAtomicity,
				EAngelscriptCacheStoreStage::None);
		}

		int32 CallCount = 0;
	};

	static FAngelscriptCacheWriterToken MakeWriterToken()
	{
		return FAngelscriptCacheWriterToken::TryParse(
			TEXT("8201-00112233445566778899aabbccddeeff")).GetValue();
	}

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

	static FCompactionFixture MakeCompactionFixture(
		const uint32 DiscoveryPolicyVersion = 1)
	{
		FAngelscriptCachedSourceIndex Source;
		Source.PayloadSchemaVersion =
			FAngelscriptCacheSemanticArchive::SourceIndexPayloadSchemaVersion;
		Source.DiscoveryPolicy.PolicyVersion = DiscoveryPolicyVersion;
		check(FAngelscriptCacheSemanticArchive::ComputeSourceSnapshot(
			Source, Source.SourceSnapshot).IsSuccess());

		FAngelscriptPreparedRecord SourceRecord;
		check(FAngelscriptCacheSemanticArchive::SerializeSourceIndex(
			Source, SourceRecord.CanonicalPayload).IsSuccess());
		check(FAngelscriptCacheRecordArchive::TryBuildRecordId(
			EAngelscriptCacheRecordKind::SourceIndex,
			SourceRecord.CanonicalPayload,
			SourceRecord.RecordId).IsSuccess());

		FAngelscriptPreparedRecord UnreachableRecord;
		UnreachableRecord.CanonicalPayload = {0x91, 0x82, 0x73, 0x64};
		check(FAngelscriptCacheRecordArchive::TryBuildRecordId(
			EAngelscriptCacheRecordKind::FunctionBody,
			UnreachableRecord.CanonicalPayload,
			UnreachableRecord.RecordId).IsSuccess());

		const FAngelscriptPreparedRecord Records[] = {
			MoveTemp(SourceRecord),
			MoveTemp(UnreachableRecord),
		};
		FAngelscriptCachePackPolicy Policy;
		Policy.CompressionPolicy =
			EAngelscriptCachePackCompressionPolicy::ForceNoneForTest;
		FAngelscriptUnrealZlibCacheStorageCodec Codec;
		TArray<FAngelscriptEncodedPack> Packs;
		check(BuildAngelscriptCachePacks(
			Records, Policy, Codec, Packs).IsSuccess());
		check(Packs.Num() == 1 && Packs[0].Index.Num() == 2);

		FCompactionFixture Fixture;
		Fixture.OriginalPack = MoveTemp(Packs[0]);
		FAngelscriptCacheGenerationManifest& Manifest = Fixture.ManifestValue;
		Manifest.ManifestSchemaVersion =
			FAngelscriptCacheManifestPackArchive::ManifestSchemaVersion;
		Manifest.Compatibility.Hash = RepeatedByteHash(0x31);
		Manifest.Context.Hash = RepeatedByteHash(0x42);
		Manifest.Profile = FAngelscriptArtifactIdentityBuilder::BuildArtifactProfileKey(
			Manifest.Compatibility, Manifest.Context);
		Manifest.SourceSnapshot = Source.SourceSnapshot;
		Manifest.SourceIndexRecordId = Records[0].RecordId;

		const FAngelscriptCachePackIndexEntry* SourceIndex =
			Fixture.OriginalPack.Index.FindByPredicate(
				[&Records](const FAngelscriptCachePackIndexEntry& Entry)
				{
					return Entry.RecordId == Records[0].RecordId;
				});
		check(SourceIndex != nullptr);
		FAngelscriptCacheRecordIndexEntry& ManifestEntry =
			Manifest.Records.AddDefaulted_GetRef();
		ManifestEntry.RecordId = SourceIndex->RecordId;
		ManifestEntry.Location.PackId = Fixture.OriginalPack.PackId;
		ManifestEntry.Location.PackOffset = SourceIndex->PackOffset;
		ManifestEntry.Location.StoredSize = SourceIndex->StoredSize;
		ManifestEntry.Location.RawSize = SourceIndex->RawSize;
		ManifestEntry.Location.Codec = SourceIndex->Codec;
		ManifestEntry.Location.RawChecksum = SourceIndex->RawChecksum;
		check(EncodeAngelscriptCacheGenerationManifest(
			Manifest, Fixture.Manifest).IsSuccess());
		return Fixture;
	}

	static void InstallFixture(
		const FCompactionFixture& Fixture,
		const FAngelscriptCacheStorePaths& Paths,
		FMemoryFileOps& FileOps,
		const EAngelscriptCachePointerKind PointerKind =
			EAngelscriptCachePointerKind::Current)
	{
		FileOps.Files.Add(
			Paths.BuildPackPath(Fixture.OriginalPack.PackId),
			Fixture.OriginalPack.Bytes);
		FileOps.Files.Add(
			Paths.BuildManifestPath(Fixture.Manifest.ComputedGenerationId),
			Fixture.Manifest.CompleteBytes);
		TArray<uint8> PointerBytes;
		check(EncodeAngelscriptCachePointer(
			{PointerKind, Fixture.Manifest.ComputedGenerationId},
			PointerBytes).IsSuccess());
		const FString* PointerPath = nullptr;
		switch (PointerKind)
		{
		case EAngelscriptCachePointerKind::Current:
			PointerPath = &Paths.CurrentPointer;
			break;
		case EAngelscriptCachePointerKind::Previous:
			PointerPath = &Paths.PreviousPointer;
			break;
		case EAngelscriptCachePointerKind::PendingColdStart:
			PointerPath = &Paths.PendingColdStartPointer;
			break;
		default:
			checkNoEntry();
			break;
		}
		check(PointerPath != nullptr);
		FileOps.Files.Add(*PointerPath, MoveTemp(PointerBytes));
	}

	static FAngelscriptHash256 ReadPointerGeneration(
		const FAngelscriptCacheStorePaths& Paths,
		const EAngelscriptCachePointerKind PointerKind,
		const FMemoryFileOps& FileOps)
	{
		const FString* PointerPath = nullptr;
		switch (PointerKind)
		{
		case EAngelscriptCachePointerKind::Current:
			PointerPath = &Paths.CurrentPointer;
			break;
		case EAngelscriptCachePointerKind::Previous:
			PointerPath = &Paths.PreviousPointer;
			break;
		case EAngelscriptCachePointerKind::PendingColdStart:
			PointerPath = &Paths.PendingColdStartPointer;
			break;
		default:
			checkNoEntry();
			break;
		}
		check(PointerPath != nullptr);
		const TArray<uint8>* PointerBytes = FileOps.Files.Find(*PointerPath);
		check(PointerBytes != nullptr);
		FAngelscriptCachePointerValue Pointer;
		check(DecodeAngelscriptCachePointer(
			*PointerBytes, PointerKind, Pointer).IsSuccess());
		return Pointer.GenerationId;
	}

public:
	TEST_METHOD(MissingAuthorityFailsBeforeLockOrFilesystemMutation)
	{
		FNoMutationLockOps LockOps;
		FNoMutationFileOps FileOps;
		FAngelscriptUnrealZlibCacheStorageCodec Codec;
		FAngelscriptCacheReadBudget Budget;

		const FAngelscriptCacheStoreResult Result = CompactAngelscriptCacheStore(
			FAngelscriptCacheStorePaths{},
			FAngelscriptCacheCompactionAuthority{},
			MakeWriterToken(),
			FAngelscriptCachePackPolicy{},
			FAngelscriptCacheReadLimits{},
			Budget,
			1.0,
			[]() { return false; },
			Codec,
			LockOps,
			FileOps);

		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreError::NeedsSourceRevalidation, Result.Error));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreStage::CompactionRewrite, Result.Stage));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreCommitState::NotCommitted, Result.CommitState));
		ASSERT_THAT(AreEqual(0, LockOps.TryAcquireCount));
		ASSERT_THAT(AreEqual(0, FileOps.CallCount));
	}

	TEST_METHOD(PhaseARewritesCurrentBeforeCancellablePhaseBSweep)
	{
		const FCompactionFixture Fixture = MakeCompactionFixture();
		const FAngelscriptCacheStorePaths Paths = MakePaths();
		FMemoryLockState LockState;
		FMemoryLockOps LockOps(LockState);
		FMemoryFileOps FileOps;
		InstallFixture(Fixture, Paths, FileOps);
		FAngelscriptCacheCompactionAuthority Authority;
		Authority.Profile = Fixture.ManifestValue.Profile;
		Authority.SourceSnapshot = Fixture.ManifestValue.SourceSnapshot;
		FAngelscriptCachePackPolicy Policy;
		Policy.CompressionPolicy =
			EAngelscriptCachePackCompressionPolicy::ForceNoneForTest;
		FAngelscriptCacheReadBudget Budget;
		FAngelscriptUnrealZlibCacheStorageCodec Codec;

		const FAngelscriptCacheStoreResult Result = CompactAngelscriptCacheStore(
			Paths,
			Authority,
			MakeWriterToken(),
			Policy,
			FAngelscriptCacheReadLimits{},
			Budget,
			1.0,
			[&LockState]() { return LockState.AcquisitionCount >= 2; },
			Codec,
			LockOps,
			FileOps);

		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreError::Cancelled, Result.Error));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreStage::CompactionSweep, Result.Stage));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreCommitState::CompactionCommitted,
			Result.CommitState));
		ASSERT_THAT(AreEqual(2, LockState.AcquisitionCount));
		ASSERT_THAT(IsFalse(LockState.bHeld));

		const TArray<uint8>* PointerBytes = FileOps.Files.Find(Paths.CurrentPointer);
		ASSERT_THAT(IsNotNull(PointerBytes));
		FAngelscriptCachePointerValue Pointer;
		ASSERT_THAT(IsTrue(DecodeAngelscriptCachePointer(
			*PointerBytes, EAngelscriptCachePointerKind::Current, Pointer).IsSuccess()));
		ASSERT_THAT(IsTrue(
			Pointer.GenerationId != Fixture.Manifest.ComputedGenerationId,
			TEXT("Dropping the Pack-only unreachable record must rewrite Current")));
		ASSERT_THAT(IsTrue(FileOps.Files.Contains(
			Paths.BuildManifestPath(Pointer.GenerationId))));
		ASSERT_THAT(IsTrue(FileOps.Files.Contains(
			Paths.BuildManifestPath(Fixture.Manifest.ComputedGenerationId))));
		ASSERT_THAT(IsTrue(FileOps.Files.Contains(
			Paths.BuildPackPath(Fixture.OriginalPack.PackId))));
		ASSERT_THAT(IsFalse(FileOps.Calls.ContainsByPredicate(
			[](const FString& Call)
			{
				return Call.StartsWith(TEXT("RemoveFinal:"));
			})));
	}

	TEST_METHOD(PhaseBMarksRewrittenCurrentAndSweepsOnlyStrictUnmarkedFinals)
	{
		const FCompactionFixture Fixture = MakeCompactionFixture();
		const FAngelscriptCacheStorePaths Paths = MakePaths();
		FMemoryLockState LockState;
		FMemoryLockOps LockOps(LockState);
		FMemoryFileOps FileOps;
		InstallFixture(Fixture, Paths, FileOps);

		const FAngelscriptHash256 OrphanPackId = RepeatedByteHash(0xe1);
		const FAngelscriptHash256 OrphanManifestId = RepeatedByteHash(0xe2);
		const FString OrphanPackPath = Paths.BuildPackPath(OrphanPackId);
		const FString OrphanManifestPath =
			Paths.BuildManifestPath(OrphanManifestId);
		const FString UppercasePackPath = Paths.PacksDirectory /
			(RepeatedByteHash(0xe4).ToHexString().ToUpper() + TEXT(".aspack"));
		const FString ExtraSuffixManifestPath = Paths.GenerationsDirectory /
			(OrphanManifestId.ToHexString() + TEXT(".asmanifest.backup"));
		const FString NonHashPackPath =
			Paths.PacksDirectory / TEXT("not-a-hash.aspack");
		const FString NestedStrictPackPath = Paths.PacksDirectory / TEXT("Nested") /
			(RepeatedByteHash(0xe3).ToHexString() + TEXT(".aspack"));
		FileOps.Files.Add(OrphanPackPath, {0x11});
		FileOps.Files.Add(OrphanManifestPath, {0x22});
		FileOps.Files.Add(UppercasePackPath, {0x33});
		FileOps.Files.Add(ExtraSuffixManifestPath, {0x44});
		FileOps.Files.Add(NonHashPackPath, {0x55});
		FileOps.Files.Add(NestedStrictPackPath, {0x66});

		FAngelscriptCacheCompactionAuthority Authority;
		Authority.Profile = Fixture.ManifestValue.Profile;
		Authority.SourceSnapshot = Fixture.ManifestValue.SourceSnapshot;
		FAngelscriptCachePackPolicy Policy;
		Policy.CompressionPolicy =
			EAngelscriptCachePackCompressionPolicy::ForceNoneForTest;
		FAngelscriptCacheReadBudget Budget;
		FAngelscriptUnrealZlibCacheStorageCodec Codec;

		const FAngelscriptCacheStoreResult Result = CompactAngelscriptCacheStore(
			Paths,
			Authority,
			MakeWriterToken(),
			Policy,
			FAngelscriptCacheReadLimits{},
			Budget,
			1.0,
			[]() { return false; },
			Codec,
			LockOps,
			FileOps);

		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreError::None, Result.Error));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreStage::CompactionSweep, Result.Stage));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreCommitState::CompactionCommitted,
			Result.CommitState));
		ASSERT_THAT(AreEqual(2, LockState.AcquisitionCount));
		ASSERT_THAT(IsFalse(LockState.bHeld));

		ASSERT_THAT(IsFalse(FileOps.Files.Contains(
			Paths.BuildPackPath(Fixture.OriginalPack.PackId))));
		ASSERT_THAT(IsFalse(FileOps.Files.Contains(
			Paths.BuildManifestPath(Fixture.Manifest.ComputedGenerationId))));
		ASSERT_THAT(IsFalse(FileOps.Files.Contains(OrphanPackPath)));
		ASSERT_THAT(IsFalse(FileOps.Files.Contains(OrphanManifestPath)));
		ASSERT_THAT(IsTrue(FileOps.Files.Contains(UppercasePackPath)));
		ASSERT_THAT(IsTrue(FileOps.Files.Contains(ExtraSuffixManifestPath)));
		ASSERT_THAT(IsTrue(FileOps.Files.Contains(NonHashPackPath)));
		ASSERT_THAT(IsTrue(FileOps.Files.Contains(NestedStrictPackPath)));
		ASSERT_THAT(IsTrue(FileOps.Calls.Contains(
			TEXT("Sync:") + Paths.PacksDirectory)));
		ASSERT_THAT(IsTrue(FileOps.Calls.Contains(
			TEXT("Sync:") + Paths.GenerationsDirectory)));

		const TArray<uint8>* PointerBytes = FileOps.Files.Find(Paths.CurrentPointer);
		ASSERT_THAT(IsNotNull(PointerBytes));
		FAngelscriptCachePointerValue Pointer;
		ASSERT_THAT(IsTrue(DecodeAngelscriptCachePointer(
			*PointerBytes, EAngelscriptCachePointerKind::Current, Pointer).IsSuccess()));
		ASSERT_THAT(IsTrue(
			Pointer.GenerationId != Fixture.Manifest.ComputedGenerationId));

		{
			FMemoryLockHandle ReadLock(LockState);
			FAngelscriptCacheReadBudget ReopenBudget;
			TOptional<FAngelscriptValidatedGeneration> Reopened;
			const FAngelscriptCacheStoreResult ReopenResult =
				ReadAndValidateAngelscriptCacheGenerationUnderLock(
					Paths,
					Pointer.GenerationId,
					FAngelscriptCacheReadLimits{},
					ReopenBudget,
					Codec,
					ReadLock,
					FileOps,
					Reopened);
			ASSERT_THAT(IsTrue(ReopenResult.IsSuccess()));
			ASSERT_THAT(IsTrue(Reopened.IsSet()));
		}
		ASSERT_THAT(IsFalse(LockState.bHeld));
	}

	TEST_METHOD(PhaseBRereadsAndRetainsAnInterveningPublication)
	{
		const FCompactionFixture Initial = MakeCompactionFixture(1);
		const FCompactionFixture Intervening = MakeCompactionFixture(2);
		const FAngelscriptCacheStorePaths Paths = MakePaths();
		FMemoryLockState LockState;
		FMemoryLockOps LockOps(LockState);
		FMemoryFileOps FileOps;
		InstallFixture(Initial, Paths, FileOps);
		bool bPublishedBetweenPhases = false;
		LockOps.BeforeAcquire = [&](const int32 AcquisitionNumber)
		{
			if (AcquisitionNumber == 2)
			{
				check(!LockState.bHeld);
				InstallFixture(
					Intervening,
					Paths,
					FileOps,
					EAngelscriptCachePointerKind::PendingColdStart);
				bPublishedBetweenPhases = true;
			}
		};

		FAngelscriptCacheCompactionAuthority Authority;
		Authority.Profile = Initial.ManifestValue.Profile;
		Authority.SourceSnapshot = Initial.ManifestValue.SourceSnapshot;
		FAngelscriptCachePackPolicy Policy;
		Policy.CompressionPolicy =
			EAngelscriptCachePackCompressionPolicy::ForceNoneForTest;
		FAngelscriptCacheReadBudget Budget;
		FAngelscriptUnrealZlibCacheStorageCodec Codec;

		const FAngelscriptCacheStoreResult Result = CompactAngelscriptCacheStore(
			Paths,
			Authority,
			MakeWriterToken(),
			Policy,
			FAngelscriptCacheReadLimits{},
			Budget,
			1.0,
			[]() { return false; },
			Codec,
			LockOps,
			FileOps);

		ASSERT_THAT(IsTrue(bPublishedBetweenPhases));
		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreError::None, Result.Error));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreCommitState::CompactionCommitted,
			Result.CommitState));
		ASSERT_THAT(AreEqual(2, LockState.AcquisitionCount));
		ASSERT_THAT(IsFalse(LockState.bHeld));
		ASSERT_THAT(IsTrue(
			ReadPointerGeneration(
				Paths,
				EAngelscriptCachePointerKind::PendingColdStart,
				FileOps)
			== Intervening.Manifest.ComputedGenerationId));
		ASSERT_THAT(IsTrue(FileOps.Files.Contains(Paths.BuildManifestPath(
			Intervening.Manifest.ComputedGenerationId))));
		ASSERT_THAT(IsTrue(FileOps.Files.Contains(Paths.BuildPackPath(
			Intervening.OriginalPack.PackId))));
	}

	TEST_METHOD(PhaseBDefersOneDeleteContinuesAndRetriesLater)
	{
		const FCompactionFixture Fixture = MakeCompactionFixture();
		const FAngelscriptCacheStorePaths Paths = MakePaths();
		FMemoryLockState LockState;
		FMemoryLockOps LockOps(LockState);
		FMemoryFileOps FileOps;
		InstallFixture(Fixture, Paths, FileOps);

		const FString DeferredManifestPath = Paths.BuildManifestPath(
			RepeatedByteHash(0xe5));
		const FString OtherOrphanPackPath = Paths.BuildPackPath(
			RepeatedByteHash(0xe6));
		FileOps.Files.Add(DeferredManifestPath, {0x15});
		FileOps.Files.Add(OtherOrphanPackPath, {0x16});
		FileOps.DeferredFinalPaths.Add(DeferredManifestPath);

		FAngelscriptCacheCompactionAuthority Authority;
		Authority.Profile = Fixture.ManifestValue.Profile;
		Authority.SourceSnapshot = Fixture.ManifestValue.SourceSnapshot;
		FAngelscriptCachePackPolicy Policy;
		Policy.CompressionPolicy =
			EAngelscriptCachePackCompressionPolicy::ForceNoneForTest;
		FAngelscriptCacheReadBudget FirstBudget;
		FAngelscriptUnrealZlibCacheStorageCodec Codec;

		const FAngelscriptCacheStoreResult FirstResult =
			CompactAngelscriptCacheStore(
				Paths,
				Authority,
				MakeWriterToken(),
				Policy,
				FAngelscriptCacheReadLimits{},
				FirstBudget,
				1.0,
				[]() { return false; },
				Codec,
				LockOps,
				FileOps);

		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreError::DeleteDeferred, FirstResult.Error));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreStage::CompactionSweep, FirstResult.Stage));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreCommitState::CompactionCommitted,
			FirstResult.CommitState));
		ASSERT_THAT(IsTrue(FileOps.Files.Contains(DeferredManifestPath)));
		ASSERT_THAT(IsFalse(FileOps.Files.Contains(OtherOrphanPackPath),
			TEXT("A deferred manifest must not stop later unmarked Pack deletion")));

		FileOps.DeferredFinalPaths.Reset();
		FAngelscriptCacheReadBudget RetryBudget;
		const FAngelscriptCacheStoreResult RetryResult =
			CompactAngelscriptCacheStore(
				Paths,
				Authority,
				MakeWriterToken(),
				Policy,
				FAngelscriptCacheReadLimits{},
				RetryBudget,
				1.0,
				[]() { return false; },
				Codec,
				LockOps,
				FileOps);

		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreError::None, RetryResult.Error));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreCommitState::CompactionCommitted,
			RetryResult.CommitState));
		ASSERT_THAT(IsFalse(FileOps.Files.Contains(DeferredManifestPath)));
		ASSERT_THAT(IsFalse(LockState.bHeld));
	}

	TEST_METHOD(PhaseAUnionsAllThreePhysicalRootsAndSwitchesInFrozenOrder)
	{
		const FCompactionFixture Current = MakeCompactionFixture(1);
		const FCompactionFixture Previous = MakeCompactionFixture(2);
		const FCompactionFixture Pending = MakeCompactionFixture(3);
		const FAngelscriptCacheStorePaths Paths = MakePaths();
		FMemoryLockState LockState;
		FMemoryLockOps LockOps(LockState);
		FMemoryFileOps FileOps;
		InstallFixture(Current, Paths, FileOps);
		InstallFixture(
			Previous,
			Paths,
			FileOps,
			EAngelscriptCachePointerKind::Previous);
		InstallFixture(
			Pending,
			Paths,
			FileOps,
			EAngelscriptCachePointerKind::PendingColdStart);

		FAngelscriptCacheCompactionAuthority Authority;
		Authority.Profile = Pending.ManifestValue.Profile;
		Authority.SourceSnapshot = Pending.ManifestValue.SourceSnapshot;
		FAngelscriptCachePackPolicy Policy;
		Policy.CompressionPolicy =
			EAngelscriptCachePackCompressionPolicy::ForceNoneForTest;
		FAngelscriptCacheReadBudget Budget;
		FAngelscriptUnrealZlibCacheStorageCodec Codec;

		const FAngelscriptCacheStoreResult Result = CompactAngelscriptCacheStore(
			Paths,
			Authority,
			MakeWriterToken(),
			Policy,
			FAngelscriptCacheReadLimits{},
			Budget,
			1.0,
			[]() { return false; },
			Codec,
			LockOps,
			FileOps);

		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreError::None, Result.Error));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreCommitState::CompactionCommitted,
			Result.CommitState));
		ASSERT_THAT(AreEqual(2, LockState.AcquisitionCount));
		ASSERT_THAT(IsFalse(LockState.bHeld));

		const FAngelscriptHash256 NewCurrent = ReadPointerGeneration(
			Paths, EAngelscriptCachePointerKind::Current, FileOps);
		const FAngelscriptHash256 NewPrevious = ReadPointerGeneration(
			Paths, EAngelscriptCachePointerKind::Previous, FileOps);
		const FAngelscriptHash256 NewPending = ReadPointerGeneration(
			Paths, EAngelscriptCachePointerKind::PendingColdStart, FileOps);
		ASSERT_THAT(IsTrue(NewCurrent != Current.Manifest.ComputedGenerationId));
		ASSERT_THAT(IsTrue(NewPrevious != Previous.Manifest.ComputedGenerationId));
		ASSERT_THAT(IsTrue(NewPending != Pending.Manifest.ComputedGenerationId));

		const auto FindPointerReplace = [&FileOps](const FString& PointerPath)
		{
			return FileOps.Calls.IndexOfByPredicate(
				[&PointerPath](const FString& Call)
				{
					return Call.StartsWith(TEXT("Replace:"))
						&& Call.EndsWith(TEXT("->") + PointerPath);
				});
		};
		const int32 PreviousReplace = FindPointerReplace(Paths.PreviousPointer);
		const int32 PendingReplace =
			FindPointerReplace(Paths.PendingColdStartPointer);
		const int32 CurrentReplace = FindPointerReplace(Paths.CurrentPointer);
		ASSERT_THAT(IsTrue(
			PreviousReplace >= 0
			&& PreviousReplace < PendingReplace
			&& PendingReplace < CurrentReplace,
			TEXT("Compaction same-slot switch order must be Previous, Pending, Current")));

		const FAngelscriptHash256 NewGenerationIds[] = {
			NewCurrent,
			NewPrevious,
			NewPending,
		};
		TArray<FAngelscriptCacheRecordId> SourceRecordIds;
		TOptional<FAngelscriptHash256> SharedPackId;
		{
			FMemoryLockHandle ReadLock(LockState);
			for (const FAngelscriptHash256& GenerationId : NewGenerationIds)
			{
				FAngelscriptCacheReadBudget ReopenBudget;
				TOptional<FAngelscriptValidatedGeneration> Reopened;
				const FAngelscriptCacheStoreResult ReopenResult =
					ReadAndValidateAngelscriptCacheGenerationUnderLock(
						Paths,
						GenerationId,
						FAngelscriptCacheReadLimits{},
						ReopenBudget,
						Codec,
						ReadLock,
						FileOps,
						Reopened);
				ASSERT_THAT(IsTrue(ReopenResult.IsSuccess()));
				ASSERT_THAT(IsTrue(Reopened.IsSet()));
				ASSERT_THAT(AreEqual(1, Reopened->Manifest.Records.Num()));
				SourceRecordIds.Add(Reopened->Manifest.Records[0].RecordId);
				const FAngelscriptHash256& PackId =
					Reopened->Manifest.Records[0].Location.PackId;
				if (!SharedPackId.IsSet())
				{
					SharedPackId = PackId;
				}
				else
				{
					ASSERT_THAT(IsTrue(SharedPackId.GetValue() == PackId));
				}
			}
		}
		ASSERT_THAT(IsFalse(LockState.bHeld));
		ASSERT_THAT(IsTrue(
			SourceRecordIds[0] != SourceRecordIds[1]
			&& SourceRecordIds[0] != SourceRecordIds[2]
			&& SourceRecordIds[1] != SourceRecordIds[2]));
		ASSERT_THAT(IsTrue(SharedPackId.IsSet()));
		const TArray<uint8>* SharedPackBytes = FileOps.Files.Find(
			Paths.BuildPackPath(SharedPackId.GetValue()));
		ASSERT_THAT(IsNotNull(SharedPackBytes));
		FAngelscriptCacheReadBudget PackBudget;
		TArray<FAngelscriptCachePackIndexEntry> UnionIndex;
		ASSERT_THAT(IsTrue(ValidateAngelscriptCachePack(
			*SharedPackBytes,
			SharedPackId.GetValue(),
			FAngelscriptCacheReadLimits{},
			PackBudget,
			UnionIndex).IsSuccess()));
		ASSERT_THAT(AreEqual(3, UnionIndex.Num(),
			TEXT("The rebuilt Pack must contain the three-root semantic union")));
	}

	TEST_METHOD(PhaseARemovesIneligiblePendingBeforeRewritingOtherRoots)
	{
		const FCompactionFixture Current = MakeCompactionFixture(1);
		const FCompactionFixture IneligiblePending = MakeCompactionFixture(2);
		const FAngelscriptCacheStorePaths Paths = MakePaths();
		FMemoryLockState LockState;
		FMemoryLockOps LockOps(LockState);
		FMemoryFileOps FileOps;
		InstallFixture(Current, Paths, FileOps);
		InstallFixture(
			IneligiblePending,
			Paths,
			FileOps,
			EAngelscriptCachePointerKind::PendingColdStart);

		FAngelscriptCacheCompactionAuthority Authority;
		Authority.Profile = Current.ManifestValue.Profile;
		Authority.SourceSnapshot = Current.ManifestValue.SourceSnapshot;
		FAngelscriptCachePackPolicy Policy;
		Policy.CompressionPolicy =
			EAngelscriptCachePackCompressionPolicy::ForceNoneForTest;
		FAngelscriptCacheReadBudget Budget;
		FAngelscriptUnrealZlibCacheStorageCodec Codec;

		const FAngelscriptCacheStoreResult Result = CompactAngelscriptCacheStore(
			Paths,
			Authority,
			MakeWriterToken(),
			Policy,
			FAngelscriptCacheReadLimits{},
			Budget,
			1.0,
			[]() { return false; },
			Codec,
			LockOps,
			FileOps);

		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreError::None, Result.Error));
		ASSERT_THAT(IsFalse(FileOps.Files.Contains(Paths.PendingColdStartPointer)));
		ASSERT_THAT(IsFalse(FileOps.Files.Contains(Paths.BuildManifestPath(
			IneligiblePending.Manifest.ComputedGenerationId))));
		ASSERT_THAT(IsFalse(FileOps.Files.Contains(Paths.BuildPackPath(
			IneligiblePending.OriginalPack.PackId))));
		const int32 RemovePending = FileOps.Calls.Find(
			TEXT("RemovePointer:") + Paths.PendingColdStartPointer);
		const int32 ReplaceCurrent = FileOps.Calls.IndexOfByPredicate(
			[&Paths](const FString& Call)
			{
				return Call.StartsWith(TEXT("Replace:"))
					&& Call.EndsWith(TEXT("->") + Paths.CurrentPointer);
			});
		ASSERT_THAT(IsTrue(
			RemovePending >= 0 && RemovePending < ReplaceCurrent,
			TEXT("Pending must be confirmed absent before retained slots switch")));
		ASSERT_THAT(IsFalse(LockState.bHeld));
	}

	TEST_METHOD(PhaseBCancellationAfterOneDeletePreservesCommittedRewrite)
	{
		const FCompactionFixture Fixture = MakeCompactionFixture();
		const FAngelscriptCacheStorePaths Paths = MakePaths();
		FMemoryLockState LockState;
		FMemoryLockOps LockOps(LockState);
		FMemoryFileOps FileOps;
		InstallFixture(Fixture, Paths, FileOps);
		FileOps.Files.Add(
			Paths.BuildManifestPath(RepeatedByteHash(0xe7)), {0x17});
		FileOps.Files.Add(
			Paths.BuildManifestPath(RepeatedByteHash(0xe8)), {0x18});

		FAngelscriptCacheCompactionAuthority Authority;
		Authority.Profile = Fixture.ManifestValue.Profile;
		Authority.SourceSnapshot = Fixture.ManifestValue.SourceSnapshot;
		FAngelscriptCachePackPolicy Policy;
		Policy.CompressionPolicy =
			EAngelscriptCachePackCompressionPolicy::ForceNoneForTest;
		FAngelscriptCacheReadBudget Budget;
		FAngelscriptUnrealZlibCacheStorageCodec Codec;

		const FAngelscriptCacheStoreResult Result = CompactAngelscriptCacheStore(
			Paths,
			Authority,
			MakeWriterToken(),
			Policy,
			FAngelscriptCacheReadLimits{},
			Budget,
			1.0,
			[&FileOps]()
			{
				return FileOps.Calls.ContainsByPredicate([](const FString& Call)
				{
					return Call.StartsWith(TEXT("RemoveFinal:"));
				});
			},
			Codec,
			LockOps,
			FileOps);

		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreError::Cancelled, Result.Error));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreStage::CompactionSweep, Result.Stage));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreCommitState::CompactionCommitted,
			Result.CommitState));
		const int32 FirstRemove = FileOps.Calls.IndexOfByPredicate(
			[](const FString& Call)
			{
				return Call.StartsWith(TEXT("RemoveFinal:"));
			});
		ASSERT_THAT(IsTrue(FirstRemove >= 0));
		ASSERT_THAT(IsTrue(FileOps.Calls.IsValidIndex(FirstRemove + 1)));
		ASSERT_THAT(AreEqual(
			TEXT("Sync:") + Paths.GenerationsDirectory,
			FileOps.Calls[FirstRemove + 1],
			TEXT("The destructive step must be directory-synced before cancellation")));
		ASSERT_THAT(AreEqual(1, FileOps.Calls.FilterByPredicate(
			[](const FString& Call)
			{
				return Call.StartsWith(TEXT("RemoveFinal:"));
			}).Num()));
		const FAngelscriptHash256 NewCurrent = ReadPointerGeneration(
			Paths, EAngelscriptCachePointerKind::Current, FileOps);
		ASSERT_THAT(IsTrue(NewCurrent != Fixture.Manifest.ComputedGenerationId));
		ASSERT_THAT(IsTrue(FileOps.Files.Contains(
			Paths.BuildManifestPath(NewCurrent))));
		ASSERT_THAT(IsFalse(LockState.bHeld));
	}

	TEST_METHOD(PhaseBDirectorySyncFailureAfterDeleteRemainsCommitted)
	{
		const FCompactionFixture Fixture = MakeCompactionFixture();
		const FAngelscriptCacheStorePaths Paths = MakePaths();
		FMemoryLockState LockState;
		FMemoryLockOps LockOps(LockState);
		FMemoryFileOps FileOps;
		InstallFixture(Fixture, Paths, FileOps);
		FileOps.Files.Add(
			Paths.BuildManifestPath(RepeatedByteHash(0xe9)), {0x19});
		FileOps.bFailSyncAfterFinalRemoval = true;

		FAngelscriptCacheCompactionAuthority Authority;
		Authority.Profile = Fixture.ManifestValue.Profile;
		Authority.SourceSnapshot = Fixture.ManifestValue.SourceSnapshot;
		FAngelscriptCachePackPolicy Policy;
		Policy.CompressionPolicy =
			EAngelscriptCachePackCompressionPolicy::ForceNoneForTest;
		FAngelscriptCacheReadBudget Budget;
		FAngelscriptUnrealZlibCacheStorageCodec Codec;

		const FAngelscriptCacheStoreResult Result = CompactAngelscriptCacheStore(
			Paths,
			Authority,
			MakeWriterToken(),
			Policy,
			FAngelscriptCacheReadLimits{},
			Budget,
			1.0,
			[]() { return false; },
			Codec,
			LockOps,
			FileOps);

		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreError::DirectorySyncFailed, Result.Error));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreStage::CompactionSweep, Result.Stage));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreCommitState::CompactionCommitted,
			Result.CommitState));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStorePathCategory::Manifest, Result.PathCategory));
		const FAngelscriptHash256 NewCurrent = ReadPointerGeneration(
			Paths, EAngelscriptCachePointerKind::Current, FileOps);
		ASSERT_THAT(IsTrue(FileOps.Files.Contains(
			Paths.BuildManifestPath(NewCurrent))));
		ASSERT_THAT(IsFalse(LockState.bHeld));
	}

	TEST_METHOD(PhaseASwitchFailureLeavesAValidOldNewPointerMixtureWithoutSweep)
	{
		const FCompactionFixture Current = MakeCompactionFixture(1);
		const FCompactionFixture Previous = MakeCompactionFixture(2);
		const FCompactionFixture Pending = MakeCompactionFixture(3);
		const FAngelscriptCacheStorePaths Paths = MakePaths();
		FMemoryLockState LockState;
		FMemoryLockOps LockOps(LockState);
		FMemoryFileOps FileOps;
		InstallFixture(Current, Paths, FileOps);
		InstallFixture(
			Previous,
			Paths,
			FileOps,
			EAngelscriptCachePointerKind::Previous);
		InstallFixture(
			Pending,
			Paths,
			FileOps,
			EAngelscriptCachePointerKind::PendingColdStart);
		FileOps.FailPointerReplacePath = Paths.PendingColdStartPointer;

		FAngelscriptCacheCompactionAuthority Authority;
		Authority.Profile = Pending.ManifestValue.Profile;
		Authority.SourceSnapshot = Pending.ManifestValue.SourceSnapshot;
		FAngelscriptCachePackPolicy Policy;
		Policy.CompressionPolicy =
			EAngelscriptCachePackCompressionPolicy::ForceNoneForTest;
		FAngelscriptCacheReadBudget Budget;
		FAngelscriptUnrealZlibCacheStorageCodec Codec;

		const FAngelscriptCacheStoreResult Result = CompactAngelscriptCacheStore(
			Paths,
			Authority,
			MakeWriterToken(),
			Policy,
			FAngelscriptCacheReadLimits{},
			Budget,
			1.0,
			[]() { return false; },
			Codec,
			LockOps,
			FileOps);

		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreError::AtomicReplaceFailed, Result.Error));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreStage::CompactionSwitch, Result.Stage));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreCommitState::NotCommitted,
			Result.CommitState));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStorePathCategory::PendingColdStartPointer,
			Result.PathCategory));
		ASSERT_THAT(IsTrue(ReadPointerGeneration(
			Paths, EAngelscriptCachePointerKind::Previous, FileOps)
			!= Previous.Manifest.ComputedGenerationId));
		ASSERT_THAT(IsTrue(ReadPointerGeneration(
			Paths, EAngelscriptCachePointerKind::PendingColdStart, FileOps)
			== Pending.Manifest.ComputedGenerationId));
		ASSERT_THAT(IsTrue(ReadPointerGeneration(
			Paths, EAngelscriptCachePointerKind::Current, FileOps)
			== Current.Manifest.ComputedGenerationId));
		ASSERT_THAT(IsTrue(FileOps.Files.Contains(Paths.BuildManifestPath(
			Previous.Manifest.ComputedGenerationId))));
		ASSERT_THAT(IsTrue(FileOps.Files.Contains(Paths.BuildPackPath(
			Previous.OriginalPack.PackId))));
		ASSERT_THAT(IsFalse(FileOps.Calls.ContainsByPredicate(
			[](const FString& Call)
			{
				return Call.StartsWith(TEXT("RemoveFinal:"));
			})));
		ASSERT_THAT(AreEqual(1, LockState.AcquisitionCount));
		ASSERT_THAT(IsFalse(LockState.bHeld));
	}

	TEST_METHOD(PhaseAFinalIndeterminateReplaceRereadReportsCommittedWithoutSweep)
	{
		const FCompactionFixture Fixture = MakeCompactionFixture();
		const FAngelscriptCacheStorePaths Paths = MakePaths();
		FMemoryLockState LockState;
		FMemoryLockOps LockOps(LockState);
		FMemoryFileOps FileOps;
		InstallFixture(Fixture, Paths, FileOps);
		FileOps.FailPointerReplacePath = Paths.CurrentPointer;
		FileOps.bInstallPointerBeforeReplaceFailure = true;

		FAngelscriptCacheCompactionAuthority Authority;
		Authority.Profile = Fixture.ManifestValue.Profile;
		Authority.SourceSnapshot = Fixture.ManifestValue.SourceSnapshot;
		FAngelscriptCachePackPolicy Policy;
		Policy.CompressionPolicy =
			EAngelscriptCachePackCompressionPolicy::ForceNoneForTest;
		FAngelscriptCacheReadBudget Budget;
		FAngelscriptUnrealZlibCacheStorageCodec Codec;

		const FAngelscriptCacheStoreResult Result = CompactAngelscriptCacheStore(
			Paths,
			Authority,
			MakeWriterToken(),
			Policy,
			FAngelscriptCacheReadLimits{},
			Budget,
			1.0,
			[]() { return false; },
			Codec,
			LockOps,
			FileOps);

		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreError::AtomicReplaceFailed, Result.Error));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreStage::CompactionSwitch, Result.Stage));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreCommitState::CompactionCommitted,
			Result.CommitState));
		const FAngelscriptHash256 NewCurrent = ReadPointerGeneration(
			Paths, EAngelscriptCachePointerKind::Current, FileOps);
		ASSERT_THAT(IsTrue(NewCurrent != Fixture.Manifest.ComputedGenerationId));
		ASSERT_THAT(IsTrue(FileOps.Files.Contains(
			Paths.BuildManifestPath(NewCurrent))));
		ASSERT_THAT(IsTrue(FileOps.Files.Contains(Paths.BuildManifestPath(
			Fixture.Manifest.ComputedGenerationId))));
		ASSERT_THAT(IsFalse(FileOps.Calls.ContainsByPredicate(
			[](const FString& Call)
			{
				return Call.StartsWith(TEXT("RemoveFinal:"));
			})));
		ASSERT_THAT(AreEqual(1, LockState.AcquisitionCount));
		ASSERT_THAT(IsFalse(LockState.bHeld));
	}

	TEST_METHOD(PhaseBDeduplicatesDuplicatePhysicalGenerationRoots)
	{
		const FCompactionFixture Fixture = MakeCompactionFixture();
		const FAngelscriptCacheStorePaths Paths = MakePaths();
		FMemoryLockState LockState;
		FMemoryLockOps LockOps(LockState);
		FMemoryFileOps FileOps;
		InstallFixture(Fixture, Paths, FileOps);
		InstallFixture(
			Fixture,
			Paths,
			FileOps,
			EAngelscriptCachePointerKind::Previous);
		InstallFixture(
			Fixture,
			Paths,
			FileOps,
			EAngelscriptCachePointerKind::PendingColdStart);
		int32 PhaseBCallStart = INDEX_NONE;
		LockOps.BeforeAcquire = [&](const int32 AcquisitionNumber)
		{
			if (AcquisitionNumber == 2)
			{
				PhaseBCallStart = FileOps.Calls.Num();
			}
		};

		FAngelscriptCacheCompactionAuthority Authority;
		Authority.Profile = Fixture.ManifestValue.Profile;
		Authority.SourceSnapshot = Fixture.ManifestValue.SourceSnapshot;
		FAngelscriptCachePackPolicy Policy;
		Policy.CompressionPolicy =
			EAngelscriptCachePackCompressionPolicy::ForceNoneForTest;
		FAngelscriptCacheReadBudget Budget;
		FAngelscriptUnrealZlibCacheStorageCodec Codec;

		const FAngelscriptCacheStoreResult Result = CompactAngelscriptCacheStore(
			Paths,
			Authority,
			MakeWriterToken(),
			Policy,
			FAngelscriptCacheReadLimits{},
			Budget,
			1.0,
			[]() { return false; },
			Codec,
			LockOps,
			FileOps);

		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreError::None, Result.Error));
		ASSERT_THAT(IsTrue(PhaseBCallStart >= 0));
		const FAngelscriptHash256 RewrittenGeneration = ReadPointerGeneration(
			Paths, EAngelscriptCachePointerKind::Current, FileOps);
		const FString ExpectedRead = TEXT("Read:")
			+ Paths.BuildManifestPath(RewrittenGeneration);
		int32 PhaseBManifestReadCount = 0;
		for (int32 Index = PhaseBCallStart; Index < FileOps.Calls.Num(); ++Index)
		{
			if (FileOps.Calls[Index] == ExpectedRead)
			{
				++PhaseBManifestReadCount;
			}
		}
		ASSERT_THAT(AreEqual(
			1,
			PhaseBManifestReadCount,
			TEXT("Duplicate physical pointers must share one Phase B mark decode")));
		ASSERT_THAT(IsFalse(LockState.bHeld));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
