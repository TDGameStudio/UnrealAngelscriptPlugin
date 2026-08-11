#include "Cache/AngelscriptCacheStore.h"
#include "Cache/AngelscriptCacheArchive.h"
#include "Cache/AngelscriptCacheManifestPack.h"
#include "Cache/AngelscriptCacheSemanticRecords.h"

#include "CQTest.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheStoreReadSessionTests,
	"Angelscript.TestModule.Cache.StoreReadSession",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	struct FGenerationFixture
	{
		FAngelscriptEncodedPack Pack;
		FAngelscriptCacheGenerationManifest ManifestValue;
		FAngelscriptEncodedCacheGenerationManifest Manifest;
		FAngelscriptCacheCompatibilityKey Compatibility;
		FAngelscriptCacheContextKey Context;
	};

	struct FLockState
	{
		bool bHeld = false;
		int32 AcquisitionCount = 0;
	};

	class FMemoryLockHandle final : public IAngelscriptCacheNamespaceLockHandle
	{
	public:
		explicit FMemoryLockHandle(FLockState& InState)
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
		FLockState& State;
	};

	class FMemoryLockOps final : public IAngelscriptCacheNamespaceLockOps
	{
	public:
		explicit FMemoryLockOps(FLockState& InState)
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
			return State.bHeld
				? nullptr
				: MakeUnique<FMemoryLockHandle>(State);
		}

	private:
		FLockState& State;
	};

	class FMemoryPinnedHandle final : public IAngelscriptCachePinnedFileHandle
	{
	public:
		FMemoryPinnedHandle(
			TArray<uint8>&& InBytes,
			FLockState& InLockState,
			bool& InPackReadWhileLocked,
			const bool bInPack)
			: Bytes(MoveTemp(InBytes))
			, LockState(InLockState)
			, bPackReadWhileLocked(InPackReadWhileLocked)
			, bPack(bInPack)
		{
		}

		virtual uint64 GetSize() const override
		{
			return static_cast<uint64>(Bytes.Num());
		}

		virtual FAngelscriptCacheStoreResult ReadAll(
			TArray<uint8>& OutBytes) override
		{
			if (bPack && LockState.bHeld)
			{
				bPackReadWhileLocked = true;
			}
			OutBytes = Bytes;
			return FAngelscriptCacheStoreResult::Success();
		}

	private:
		TArray<uint8> Bytes;
		FLockState& LockState;
		bool& bPackReadWhileLocked;
		bool bPack = false;
	};

	class FMemoryFileOps final : public IAngelscriptCacheAtomicFileOps
	{
	public:
		explicit FMemoryFileOps(FLockState& InLockState)
			: LockState(InLockState)
		{
		}

		TMap<FString, TArray<uint8>> Files;
		int32 ManifestOpenCount = 0;
		int32 PackOpenCount = 0;
		bool bPackReadWhileLocked = false;

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
			const TArray<uint8>* Existing = Files.Find(Path);
			if (Existing == nullptr
				|| static_cast<uint64>(Existing->Num()) > MaxBytes)
			{
				OutBytes.Reset();
				return FAngelscriptCacheStoreResult::Failure(
					EAngelscriptCacheStoreError::PointerInvalid,
					EAngelscriptCacheStoreStage::None);
			}
			OutBytes = *Existing;
			return FAngelscriptCacheStoreResult::Success();
		}

		virtual FAngelscriptCacheStoreResult OpenReadPinned(
			const FString& Path,
			const uint64 MaxBytes,
			TUniquePtr<IAngelscriptCachePinnedFileHandle>& OutHandle) override
		{
			OutHandle.Reset();
			const bool bPack = Path.EndsWith(
				TEXT(".aspack"), ESearchCase::CaseSensitive);
			const bool bManifest = Path.EndsWith(
				TEXT(".asmanifest"), ESearchCase::CaseSensitive);
			const TArray<uint8>* Existing = Files.Find(Path);
			if (Existing == nullptr)
			{
				return FAngelscriptCacheStoreResult::Failure(
					bPack
						? EAngelscriptCacheStoreError::PackMissing
						: EAngelscriptCacheStoreError::ManifestMissing,
					EAngelscriptCacheStoreStage::None);
			}
			if (static_cast<uint64>(Existing->Num()) > MaxBytes)
			{
				return FAngelscriptCacheStoreResult::Failure(
					EAngelscriptCacheStoreError::ReadFailed,
					EAngelscriptCacheStoreStage::None);
			}
			if (bPack)
			{
				++PackOpenCount;
			}
			else if (bManifest)
			{
				++ManifestOpenCount;
			}
			OutHandle = MakeUnique<FMemoryPinnedHandle>(
				TArray<uint8>(*Existing),
				LockState,
				bPackReadWhileLocked,
				bPack);
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

		FLockState& LockState;
	};

	class FScopedDiskRoot final
	{
	public:
		FScopedDiskRoot()
		{
			Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(
				FPaths::ProjectSavedDir(),
				TEXT("Automation/AngelscriptCacheStoreReadSession"),
				FGuid::NewGuid().ToString(EGuidFormats::Digits)));
			FPaths::NormalizeDirectoryName(Root);
			check(Root.Contains(
				TEXT("/Saved/Automation/AngelscriptCacheStoreReadSession/")));
			check(IFileManager::Get().MakeDirectory(*Root, true));
		}

		~FScopedDiskRoot()
		{
			if (Root.Contains(
				TEXT("/Saved/Automation/AngelscriptCacheStoreReadSession/")))
			{
				IFileManager::Get().DeleteDirectory(*Root, false, true);
			}
		}

		FString Root;
	};

	static FAngelscriptHash256 RepeatedByteHash(const uint8 Byte)
	{
		FBlake3Hash::ByteArray Bytes{};
		FMemory::Memset(Bytes, Byte, sizeof(Bytes));
		return FAngelscriptHash256{FBlake3Hash(Bytes)};
	}

	static FGenerationFixture MakeGenerationFixture(
		const uint32 PolicyVersion = 1)
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
		Fixture.Compatibility.Hash = RepeatedByteHash(0x39);
		Fixture.Context.Hash = RepeatedByteHash(0x5b);
		Fixture.Pack = MoveTemp(Packs[0]);
		FAngelscriptCacheGenerationManifest& Value = Fixture.ManifestValue;
		Value.ManifestSchemaVersion =
			FAngelscriptCacheManifestPackArchive::ManifestSchemaVersion;
		Value.Compatibility = Fixture.Compatibility;
		Value.Context = Fixture.Context;
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

	static FGenerationFixture MakeTwoPackManifestFixture()
	{
		FGenerationFixture Fixture = MakeGenerationFixture();
		FAngelscriptCacheRecordIndexEntry& Extra =
			Fixture.ManifestValue.Records.AddDefaulted_GetRef();
		Extra.RecordId.Kind = EAngelscriptCacheRecordKind::FunctionBody;
		Extra.RecordId.ContentHash = RepeatedByteHash(0x91);
		Extra.Location.PackId = RepeatedByteHash(0xa2);
		Extra.Location.PackOffset =
			FAngelscriptCacheManifestPackArchive::PackHeaderWireSize;
		Extra.Location.StoredSize = 1;
		Extra.Location.RawSize = 1;
		Extra.Location.Codec = EAngelscriptCacheCodec::None;
		Extra.Location.RawChecksum = RepeatedByteHash(0xb3);
		Fixture.ManifestValue.Records.Sort(
			[](const FAngelscriptCacheRecordIndexEntry& Left,
				const FAngelscriptCacheRecordIndexEntry& Right)
			{
				return Left.RecordId < Right.RecordId;
			});
		check(EncodeAngelscriptCacheGenerationManifest(
			Fixture.ManifestValue, Fixture.Manifest).IsSuccess());
		return Fixture;
	}

	static FAngelscriptCacheStorePaths MakeMemoryPaths()
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

	static void InstallMemoryGeneration(
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

	static void PutMemoryPointer(
		const FAngelscriptCacheStorePaths& Paths,
		const EAngelscriptCachePointerKind Kind,
		const FAngelscriptHash256& GenerationId,
		FMemoryFileOps& FileOps)
	{
		FAngelscriptCachePointerValue Value;
		Value.Kind = Kind;
		Value.GenerationId = GenerationId;
		TArray<uint8> Bytes;
		check(EncodeAngelscriptCachePointer(Value, Bytes).IsSuccess());
		const FString* PointerPath = Kind == EAngelscriptCachePointerKind::Current
			? &Paths.CurrentPointer
			: Kind == EAngelscriptCachePointerKind::Previous
				? &Paths.PreviousPointer
				: &Paths.PendingColdStartPointer;
		FileOps.Files.Add(*PointerPath, MoveTemp(Bytes));
	}

	static FAngelscriptCacheReadSelection MakeSelection(
		const FGenerationFixture& Fixture)
	{
		FAngelscriptCacheReadSelection Selection;
		Selection.Compatibility = Fixture.ManifestValue.Compatibility;
		Selection.Context = Fixture.ManifestValue.Context;
		Selection.Profile = Fixture.ManifestValue.Profile;
		Selection.SourceSnapshot = Fixture.ManifestValue.SourceSnapshot;
		return Selection;
	}

public:
	TEST_METHOD(ProductionPinnedHandleKeepsOldBytesAfterPathReplacement)
	{
		FScopedDiskRoot Disk;
		TUniquePtr<IAngelscriptCacheAtomicFileOps> FileOps =
			CreateAngelscriptCacheAtomicFileOps();
		ASSERT_THAT(IsNotNull(FileOps.Get()));

		const FString FinalPath = Disk.Root / TEXT("generation.asmanifest");
		const FString FirstTemp = FinalPath
			+ TEXT(".tmp.8101-00112233445566778899aabbccddeeff");
		const FString SecondTemp = FinalPath
			+ TEXT(".tmp.8101-fedcba9876543210fedcba9876543210");
		const TArray<uint8> FirstBytes{0x11, 0x22, 0x33, 0x44};
		const TArray<uint8> SecondBytes{0xa1, 0xb2, 0xc3, 0xd4};

		ASSERT_THAT(IsTrue(FileOps->WriteFlushClose(
			FirstTemp, FirstBytes).IsSuccess()));
		ASSERT_THAT(IsTrue(FileOps->RenameNewImmutable(
			FirstTemp, FinalPath).IsSuccess()));

		TUniquePtr<IAngelscriptCachePinnedFileHandle> PinnedHandle;
		const FAngelscriptCacheStoreResult PinResult = FileOps->OpenReadPinned(
			FinalPath, FirstBytes.Num(), PinnedHandle);
		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreError::None, PinResult.Error));
		ASSERT_THAT(IsNotNull(PinnedHandle.Get()));
		ASSERT_THAT(AreEqual(
			static_cast<uint64>(FirstBytes.Num()), PinnedHandle->GetSize()));

		const FAngelscriptCacheStoreResult RemoveResult =
			FileOps->RemoveFinalImmutable(FinalPath);
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreError::None, RemoveResult.Error,
			TEXT("Production compaction unlink must accept a delete-sharing pin")));
		ASSERT_THAT(IsTrue(FileOps->WriteFlushClose(
			SecondTemp, SecondBytes).IsSuccess()));
		ASSERT_THAT(IsTrue(FileOps->RenameNewImmutable(
			SecondTemp, FinalPath).IsSuccess()));

		TArray<uint8> PinnedBytes;
		ASSERT_THAT(IsTrue(PinnedHandle->ReadAll(PinnedBytes).IsSuccess()));
		ASSERT_THAT(AreEqual(FirstBytes, PinnedBytes,
			TEXT("The pinned handle must remain bound to the unlinked old object")));

		TArray<uint8> PathBytes;
		ASSERT_THAT(IsTrue(FileOps->ReopenReadAll(
			FinalPath, SecondBytes.Num(), PathBytes).IsSuccess()));
		ASSERT_THAT(AreEqual(SecondBytes, PathBytes,
			TEXT("A new path open must observe the replacement object")));
	}

	TEST_METHOD(ProductionSessionPinsAndValidatesCurrentGeneration)
	{
		FScopedDiskRoot Disk;
		const FGenerationFixture Fixture = MakeGenerationFixture();
		TUniquePtr<IAngelscriptCacheAtomicFileOps> FileOps =
			CreateAngelscriptCacheAtomicFileOps();
		TUniquePtr<IAngelscriptCacheNamespaceLockOps> LockOps =
			CreateAngelscriptCacheNamespaceLockOps();
		ASSERT_THAT(IsNotNull(FileOps.Get()));
		ASSERT_THAT(IsNotNull(LockOps.Get()));

		FAngelscriptCacheStorePaths Paths;
		ASSERT_THAT(IsTrue(BuildAngelscriptCacheStorePaths(
			Disk.Root / TEXT("Session/CacheV2"),
			Fixture.Compatibility,
			Fixture.Context,
			*FileOps,
			Paths).IsSuccess()));
		const TOptional<FAngelscriptCacheWriterToken> Token =
			FAngelscriptCacheWriterToken::TryParse(
				TEXT("8102-00112233445566778899aabbccddeeff"));
		ASSERT_THAT(IsTrue(Token.IsSet()));
		FAngelscriptCacheReadLimits Limits;
		FAngelscriptUnrealZlibCacheStorageCodec Codec;
		FAngelscriptCacheReadBudget PublicationBudget;
		const FAngelscriptCacheStoreResult Publication =
			PublishAngelscriptCacheGeneration(
				Paths,
				EAngelscriptCachePublicationDisposition::Current,
				TOptional<FAngelscriptHash256>{},
				MakeArrayView(&Fixture.Pack, 1),
				Fixture.ManifestValue,
				Fixture.Manifest,
				Token.GetValue(),
				Limits,
				PublicationBudget,
				FPlatformTime::Seconds() + 2.0,
				[]() { return false; },
				Codec,
				*LockOps,
				*FileOps);
		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreError::None, Publication.Error));

		FAngelscriptCacheReadSelection Selection;
		Selection.Compatibility = Fixture.ManifestValue.Compatibility;
		Selection.Context = Fixture.ManifestValue.Context;
		Selection.Profile = Fixture.ManifestValue.Profile;
		Selection.SourceSnapshot = Fixture.ManifestValue.SourceSnapshot;
		Selection.bAllowPendingColdStart = false;
		TUniquePtr<FAngelscriptCacheReadSession> Session;

		const FAngelscriptCacheStoreResult OpenResult =
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

		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreError::None, OpenResult.Error));
		ASSERT_THAT(IsNotNull(Session.Get()));
		ASSERT_THAT(AreEqual(
			EAngelscriptCachePointerKind::Current, Session->GetPointerKind()));
		ASSERT_THAT(IsTrue(
			Session->GetGenerationId() == Fixture.Manifest.ComputedGenerationId));
		ASSERT_THAT(AreEqual(1, Session->GetPinnedPackCount()));
		ASSERT_THAT(AreEqual(1, Session->GetGeneration().ReachableRecords.Num()));
		ASSERT_THAT(IsTrue(Session->GetBudget().GetStoredBytes() > 0));
	}

	TEST_METHOD(PackValidationRunsOnlyAfterEveryHandleIsPinnedAndTheLockIsReleased)
	{
		const FGenerationFixture Fixture = MakeGenerationFixture();
		const FAngelscriptCacheStorePaths Paths = MakeMemoryPaths();
		FLockState LockState;
		FMemoryLockOps LockOps(LockState);
		FMemoryFileOps FileOps(LockState);
		InstallMemoryGeneration(Fixture, Paths, FileOps);
		PutMemoryPointer(
			Paths,
			EAngelscriptCachePointerKind::Current,
			Fixture.Manifest.ComputedGenerationId,
			FileOps);
		FAngelscriptUnrealZlibCacheStorageCodec Codec;
		TUniquePtr<FAngelscriptCacheReadSession> Session;

		const FAngelscriptCacheStoreResult Result =
			OpenBestAngelscriptCacheReadSession(
				Paths,
				MakeSelection(Fixture),
				FAngelscriptCacheReadLimits{},
				1.0,
				[]() { return false; },
				Codec,
				LockOps,
				FileOps,
				Session);

		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreError::None, Result.Error));
		ASSERT_THAT(IsNotNull(Session.Get()));
		ASSERT_THAT(AreEqual(1, LockState.AcquisitionCount));
		ASSERT_THAT(AreEqual(1, FileOps.ManifestOpenCount));
		ASSERT_THAT(AreEqual(1, FileOps.PackOpenCount));
		ASSERT_THAT(IsFalse(FileOps.bPackReadWhileLocked,
			TEXT("Pack bytes and graph validation must run after namespace unlock")));
		ASSERT_THAT(IsFalse(LockState.bHeld));
	}

	TEST_METHOD(IneligibleCurrentFallsBackToPreviousWithoutOpeningCurrentPacks)
	{
		const FGenerationFixture Current = MakeGenerationFixture(2);
		const FGenerationFixture Previous = MakeGenerationFixture(1);
		const FAngelscriptCacheStorePaths Paths = MakeMemoryPaths();
		FLockState LockState;
		FMemoryLockOps LockOps(LockState);
		FMemoryFileOps FileOps(LockState);
		InstallMemoryGeneration(Current, Paths, FileOps);
		InstallMemoryGeneration(Previous, Paths, FileOps);
		PutMemoryPointer(Paths, EAngelscriptCachePointerKind::Current,
			Current.Manifest.ComputedGenerationId, FileOps);
		PutMemoryPointer(Paths, EAngelscriptCachePointerKind::Previous,
			Previous.Manifest.ComputedGenerationId, FileOps);
		FAngelscriptUnrealZlibCacheStorageCodec Codec;
		TUniquePtr<FAngelscriptCacheReadSession> Session;

		const FAngelscriptCacheStoreResult Result =
			OpenBestAngelscriptCacheReadSession(
				Paths,
				MakeSelection(Previous),
				FAngelscriptCacheReadLimits{},
				1.0,
				[]() { return false; },
				Codec,
				LockOps,
				FileOps,
				Session);

		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreError::None, Result.Error));
		ASSERT_THAT(IsNotNull(Session.Get()));
		ASSERT_THAT(AreEqual(
			EAngelscriptCachePointerKind::Previous, Session->GetPointerKind()));
		ASSERT_THAT(AreEqual(2, LockState.AcquisitionCount));
		ASSERT_THAT(AreEqual(2, FileOps.ManifestOpenCount));
		ASSERT_THAT(AreEqual(1, FileOps.PackOpenCount,
			TEXT("The ineligible Current must be rejected before Pack pinning")));
		ASSERT_THAT(IsTrue(
			Session->GetBudget().GetDecodedBytes()
				> Session->GetBudget().GetResidentDecodedBytes(),
			TEXT("Released candidate reservations must not refund total attempt Budget")));
	}

	TEST_METHOD(ActiveSelectionSkipsPendingColdStart)
	{
		const FGenerationFixture Pending = MakeGenerationFixture();
		const FAngelscriptCacheStorePaths Paths = MakeMemoryPaths();
		FLockState LockState;
		FMemoryLockOps LockOps(LockState);
		FMemoryFileOps FileOps(LockState);
		InstallMemoryGeneration(Pending, Paths, FileOps);
		PutMemoryPointer(Paths, EAngelscriptCachePointerKind::PendingColdStart,
			Pending.Manifest.ComputedGenerationId, FileOps);
		FAngelscriptCacheReadSelection Selection = MakeSelection(Pending);
		Selection.bAllowPendingColdStart = false;
		FAngelscriptUnrealZlibCacheStorageCodec Codec;
		TUniquePtr<FAngelscriptCacheReadSession> Session;

		const FAngelscriptCacheStoreResult Result =
			OpenBestAngelscriptCacheReadSession(
				Paths, Selection, FAngelscriptCacheReadLimits{}, 1.0,
				[]() { return false; }, Codec, LockOps, FileOps, Session);

		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreError::None, Result.Error));
		ASSERT_THAT(IsNull(Session.Get()));
		ASSERT_THAT(AreEqual(2, LockState.AcquisitionCount));
		ASSERT_THAT(AreEqual(0, FileOps.ManifestOpenCount));
		ASSERT_THAT(AreEqual(0, FileOps.PackOpenCount));
	}

	TEST_METHOD(FreshSelectionFallsBackToPendingColdStart)
	{
		const FGenerationFixture Pending = MakeGenerationFixture();
		const FAngelscriptCacheStorePaths Paths = MakeMemoryPaths();
		FLockState LockState;
		FMemoryLockOps LockOps(LockState);
		FMemoryFileOps FileOps(LockState);
		InstallMemoryGeneration(Pending, Paths, FileOps);
		PutMemoryPointer(Paths, EAngelscriptCachePointerKind::PendingColdStart,
			Pending.Manifest.ComputedGenerationId, FileOps);
		FAngelscriptCacheReadSelection Selection = MakeSelection(Pending);
		Selection.bAllowPendingColdStart = true;
		FAngelscriptUnrealZlibCacheStorageCodec Codec;
		TUniquePtr<FAngelscriptCacheReadSession> Session;

		const FAngelscriptCacheStoreResult Result =
			OpenBestAngelscriptCacheReadSession(
				Paths, Selection, FAngelscriptCacheReadLimits{}, 1.0,
				[]() { return false; }, Codec, LockOps, FileOps, Session);

		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreError::None, Result.Error));
		ASSERT_THAT(IsNotNull(Session.Get()));
		ASSERT_THAT(AreEqual(
			EAngelscriptCachePointerKind::PendingColdStart,
			Session->GetPointerKind()));
		ASSERT_THAT(IsTrue(
			Session->GetGenerationId() == Pending.Manifest.ComputedGenerationId));
		ASSERT_THAT(AreEqual(3, LockState.AcquisitionCount));
		ASSERT_THAT(AreEqual(1, FileOps.ManifestOpenCount));
		ASSERT_THAT(AreEqual(1, FileOps.PackOpenCount));
	}

	TEST_METHOD(FallbackCandidatesShareOneCumulativeDecodedBudget)
	{
		const FGenerationFixture Current = MakeGenerationFixture(2);
		const FGenerationFixture Previous = MakeGenerationFixture(1);
		const FAngelscriptCacheStorePaths Paths = MakeMemoryPaths();
		FAngelscriptUnrealZlibCacheStorageCodec Codec;

		FLockState BaselineLockState;
		FMemoryLockOps BaselineLockOps(BaselineLockState);
		FMemoryFileOps BaselineFiles(BaselineLockState);
		InstallMemoryGeneration(Current, Paths, BaselineFiles);
		InstallMemoryGeneration(Previous, Paths, BaselineFiles);
		PutMemoryPointer(Paths, EAngelscriptCachePointerKind::Current,
			Current.Manifest.ComputedGenerationId, BaselineFiles);
		PutMemoryPointer(Paths, EAngelscriptCachePointerKind::Previous,
			Previous.Manifest.ComputedGenerationId, BaselineFiles);
		TUniquePtr<FAngelscriptCacheReadSession> BaselineSession;
		ASSERT_THAT(IsTrue(OpenBestAngelscriptCacheReadSession(
			Paths, MakeSelection(Previous), FAngelscriptCacheReadLimits{}, 1.0,
			[]() { return false; }, Codec, BaselineLockOps, BaselineFiles,
			BaselineSession).IsSuccess()));
		ASSERT_THAT(IsNotNull(BaselineSession.Get()));
		const uint64 CombinedDecoded =
			BaselineSession->GetBudget().GetDecodedBytes();
		ASSERT_THAT(IsTrue(CombinedDecoded > 1));
		BaselineSession.Reset();

		FLockState LimitedLockState;
		FMemoryLockOps LimitedLockOps(LimitedLockState);
		FMemoryFileOps LimitedFiles(LimitedLockState);
		InstallMemoryGeneration(Current, Paths, LimitedFiles);
		InstallMemoryGeneration(Previous, Paths, LimitedFiles);
		PutMemoryPointer(Paths, EAngelscriptCachePointerKind::Current,
			Current.Manifest.ComputedGenerationId, LimitedFiles);
		PutMemoryPointer(Paths, EAngelscriptCachePointerKind::Previous,
			Previous.Manifest.ComputedGenerationId, LimitedFiles);
		FAngelscriptCacheReadLimits Limited;
		Limited.MaxTotalDecodedBytes = CombinedDecoded - 1;
		TUniquePtr<FAngelscriptCacheReadSession> LimitedSession;

		const FAngelscriptCacheStoreResult Result =
			OpenBestAngelscriptCacheReadSession(
				Paths, MakeSelection(Previous), Limited, 1.0,
				[]() { return false; }, Codec, LimitedLockOps, LimitedFiles,
				LimitedSession);

		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreError::ContentValidationFailed, Result.Error));
		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreStage::SessionPin, Result.Stage));
		ASSERT_THAT(IsTrue(Result.ContentValidation.IsSet()));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheValidationError::BudgetExceeded,
			Result.ContentValidation->Error));
		ASSERT_THAT(IsNull(LimitedSession.Get()));
	}

	TEST_METHOD(PackCountLimitFailsBeforeOpeningAnyPackHandle)
	{
		const FGenerationFixture Fixture = MakeTwoPackManifestFixture();
		const FAngelscriptCacheStorePaths Paths = MakeMemoryPaths();
		FLockState LockState;
		FMemoryLockOps LockOps(LockState);
		FMemoryFileOps FileOps(LockState);
		FileOps.Files.Add(
			Paths.BuildManifestPath(Fixture.Manifest.ComputedGenerationId),
			Fixture.Manifest.CompleteBytes);
		PutMemoryPointer(Paths, EAngelscriptCachePointerKind::Current,
			Fixture.Manifest.ComputedGenerationId, FileOps);
		FAngelscriptCacheReadLimits Limits;
		Limits.MaxGenerationPacks = 1;
		FAngelscriptUnrealZlibCacheStorageCodec Codec;
		TUniquePtr<FAngelscriptCacheReadSession> Session;

		const FAngelscriptCacheStoreResult Result =
			OpenBestAngelscriptCacheReadSession(
				Paths, MakeSelection(Fixture), Limits, 1.0,
				[]() { return false; }, Codec, LockOps, FileOps, Session);

		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreError::ContentValidationFailed, Result.Error));
		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreStage::SessionPin, Result.Stage));
		ASSERT_THAT(IsTrue(Result.ContentValidation.IsSet()));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheValidationError::BudgetExceeded,
			Result.ContentValidation->Error));
		ASSERT_THAT(AreEqual(0, FileOps.PackOpenCount));
		ASSERT_THAT(IsNull(Session.Get()));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
