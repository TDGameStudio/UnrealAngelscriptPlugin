#include "Cache/AngelscriptCacheStore.h"
#include "Cache/AngelscriptCacheArchive.h"
#include "Cache/AngelscriptCacheManifestPack.h"
#include "Cache/AngelscriptCacheSemanticRecords.h"

#include "CQTest.h"
#include "Async/Async.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptCacheStoreDiskTests_Private
{
	class FScopedDiskRoot final
	{
	public:
		FScopedDiskRoot()
		{
			Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(
				FPaths::ProjectSavedDir(),
				TEXT("Automation/AngelscriptCacheStoreDisk"),
				FGuid::NewGuid().ToString(EGuidFormats::Digits)));
			FPaths::NormalizeDirectoryName(Root);
			check(Root.Contains(TEXT("/Saved/Automation/AngelscriptCacheStoreDisk/")));
			check(IFileManager::Get().MakeDirectory(*Root, true));
		}

		~FScopedDiskRoot()
		{
			if (Root.Contains(TEXT("/Saved/Automation/AngelscriptCacheStoreDisk/")))
			{
				IFileManager::Get().DeleteDirectory(*Root, false, true);
			}
		}

		FString Root;
	};

	static TArray<uint8> MakeBytes(const uint8 Seed)
	{
		return TArray<uint8>{Seed, static_cast<uint8>(Seed + 1),
			static_cast<uint8>(Seed + 2), static_cast<uint8>(Seed + 3)};
	}

	struct FGenerationFixture
	{
		FAngelscriptEncodedPack Pack;
		FAngelscriptCacheGenerationManifest ManifestValue;
		FAngelscriptEncodedCacheGenerationManifest Manifest;
		FAngelscriptCacheCompatibilityKey Compatibility;
		FAngelscriptCacheContextKey Context;
	};

	static FAngelscriptHash256 RepeatedByteHash(const uint8 Byte)
	{
		FBlake3Hash::ByteArray Bytes{};
		FMemory::Memset(Bytes, Byte, sizeof(Bytes));
		return FAngelscriptHash256{FBlake3Hash(Bytes)};
	}

	static FGenerationFixture MakeGenerationFixture(const uint32 PolicyVersion = 1)
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
		Policy.CompressionPolicy = EAngelscriptCachePackCompressionPolicy::ForceNoneForTest;
		FAngelscriptUnrealZlibCacheStorageCodec Codec;
		TArray<FAngelscriptEncodedPack> Packs;
		check(BuildAngelscriptCachePacks(
			MakeArrayView(&SourceRecord, 1), Policy, Codec, Packs).IsSuccess());
		check(Packs.Num() == 1 && Packs[0].Index.Num() == 1);

		FGenerationFixture Fixture;
		Fixture.Compatibility.Hash = RepeatedByteHash(0x35);
		Fixture.Context.Hash = RepeatedByteHash(0x57);
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
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheStoreDiskTests,
	"Angelscript.TestModule.Cache.StoreDisk",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(ProductionFileOpsFlushReopenAndNoReplaceImmutableBytes)
	{
		using namespace AngelscriptCacheStoreDiskTests_Private;
		FScopedDiskRoot Disk;
		TUniquePtr<IAngelscriptCacheAtomicFileOps> FileOps =
			CreateAngelscriptCacheAtomicFileOps();
		ASSERT_THAT(IsNotNull(FileOps.Get()));
		ASSERT_THAT(IsTrue(FileOps->SupportsSharedAtomicCacheStore()));

		FAngelscriptCanonicalCacheRoot Canonical;
		const FAngelscriptCacheStoreResult RootResult =
			FileOps->CanonicalizeAndValidateRoot(Disk.Root, Canonical);
		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreError::None, RootResult.Error));
		ASSERT_THAT(IsFalse(FPaths::IsRelative(Canonical.AbsolutePath)));
		ASSERT_THAT(AreEqual(Canonical.IdentityPath.ToLower(), Canonical.IdentityPath));

		const FString PacksDirectory = Canonical.AbsolutePath / TEXT("Packs");
		ASSERT_THAT(IsTrue(IFileManager::Get().MakeDirectory(*PacksDirectory, true)));
		const FString TempPath = PacksDirectory / TEXT("object.aspack.tmp.1-0123456789abcdef0123456789abcdef");
		const FString FinalPath = PacksDirectory / TEXT("object.aspack");
		const TArray<uint8> Expected = MakeBytes(0x21);

		ASSERT_THAT(IsTrue(FileOps->WriteFlushClose(TempPath, Expected).IsSuccess()));
		TArray<uint8> ReopenedTemp;
		ASSERT_THAT(IsTrue(FileOps->ReopenReadAll(
			TempPath, Expected.Num(), ReopenedTemp).IsSuccess()));
		ASSERT_THAT(AreEqual(Expected, ReopenedTemp));
		ASSERT_THAT(IsTrue(FileOps->RenameNewImmutable(TempPath, FinalPath).IsSuccess()));
		ASSERT_THAT(IsTrue(FileOps->SyncDirectory(PacksDirectory).IsSuccess()));

		TArray<uint8> ReopenedFinal;
		ASSERT_THAT(IsTrue(FileOps->ReopenReadAll(
			FinalPath, Expected.Num(), ReopenedFinal).IsSuccess()));
		ASSERT_THAT(AreEqual(Expected, ReopenedFinal));

		const FString CollisionTemp = PacksDirectory
			/ TEXT("collision.aspack.tmp.1-fedcba9876543210fedcba9876543210");
		const TArray<uint8> Different = MakeBytes(0x71);
		ASSERT_THAT(IsTrue(FileOps->WriteFlushClose(CollisionTemp, Different).IsSuccess()));
		const FAngelscriptCacheStoreResult Collision =
			FileOps->RenameNewImmutable(CollisionTemp, FinalPath);
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreError::ImmutableObjectCollisionOrCorruption,
			Collision.Error));
		ASSERT_THAT(IsTrue(FileOps->RemoveOwnTemp(CollisionTemp).IsSuccess()));

		ReopenedFinal.Reset();
		ASSERT_THAT(IsTrue(FileOps->ReopenReadAll(
			FinalPath, Expected.Num(), ReopenedFinal).IsSuccess()));
		ASSERT_THAT(AreEqual(Expected, ReopenedFinal,
			TEXT("A no-replace collision must leave the first immutable bytes unchanged")));
	}

	TEST_METHOD(FirstLaunchCreatesTheCompleteSavedNamespaceDirectoryTree)
	{
		using namespace AngelscriptCacheStoreDiskTests_Private;
		FScopedDiskRoot Disk;
		const FString MissingBaseRoot = Disk.Root / TEXT("FirstLaunch/CacheV2");
		TUniquePtr<IAngelscriptCacheAtomicFileOps> FileOps =
			CreateAngelscriptCacheAtomicFileOps();
		ASSERT_THAT(IsNotNull(FileOps.Get()));

		FBlake3Hash::ByteArray CompatibilityBytes{};
		FMemory::Memset(CompatibilityBytes, 0x25, sizeof(CompatibilityBytes));
		FBlake3Hash::ByteArray ContextBytes{};
		FMemory::Memset(ContextBytes, 0x47, sizeof(ContextBytes));
		FAngelscriptCacheStorePaths Paths;
		ASSERT_THAT(IsTrue(BuildAngelscriptCacheStorePaths(
			MissingBaseRoot,
			FAngelscriptCacheCompatibilityKey{
				FAngelscriptHash256{FBlake3Hash(CompatibilityBytes)}},
			FAngelscriptCacheContextKey{
				FAngelscriptHash256{FBlake3Hash(ContextBytes)}},
			*FileOps,
			Paths).IsSuccess()));

		const FAngelscriptCacheStoreResult Result =
			EnsureAngelscriptCacheStoreDirectories(Paths, *FileOps);

		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreError::None, Result.Error));
		ASSERT_THAT(IsTrue(IFileManager::Get().DirectoryExists(*Paths.BaseRoot)));
		ASSERT_THAT(IsTrue(IFileManager::Get().DirectoryExists(*Paths.NamespaceRoot)));
		ASSERT_THAT(IsTrue(IFileManager::Get().DirectoryExists(*Paths.PacksDirectory)));
		ASSERT_THAT(IsTrue(IFileManager::Get().DirectoryExists(*Paths.GenerationsDirectory)));
	}

	TEST_METHOD(ProductionStoreInstallsAndReusesACompleteGenerationOnSavedDisk)
	{
		using namespace AngelscriptCacheStoreDiskTests_Private;
		FScopedDiskRoot Disk;
		const FGenerationFixture Fixture = MakeGenerationFixture();
		TUniquePtr<IAngelscriptCacheAtomicFileOps> FileOps =
			CreateAngelscriptCacheAtomicFileOps();
		ASSERT_THAT(IsNotNull(FileOps.Get()));
		FAngelscriptCacheStorePaths Paths;
		ASSERT_THAT(IsTrue(BuildAngelscriptCacheStorePaths(
			Disk.Root / TEXT("CompleteGeneration/CacheV2"),
			Fixture.Compatibility,
			Fixture.Context,
			*FileOps,
			Paths).IsSuccess()));
		ASSERT_THAT(IsTrue(EnsureAngelscriptCacheStoreDirectories(
			Paths, *FileOps).IsSuccess()));
		const TOptional<FAngelscriptCacheWriterToken> Token =
			FAngelscriptCacheWriterToken::TryParse(
				TEXT("5197-00112233445566778899aabbccddeeff"));
		ASSERT_THAT(IsTrue(Token.IsSet()));
		FAngelscriptCacheReadLimits Limits;
		FAngelscriptUnrealZlibCacheStorageCodec Codec;

		ASSERT_THAT(IsTrue(PutAngelscriptCachePackIfAbsent(
			Paths,
			Fixture.Pack.PackId,
			Fixture.Pack.Bytes,
			Token.GetValue(),
			Limits,
			*FileOps).IsSuccess()));
		ASSERT_THAT(IsTrue(PutAngelscriptCacheManifestIfAbsent(
			Paths,
			Fixture.Manifest.ComputedGenerationId,
			Fixture.Manifest.CompleteBytes,
			Token.GetValue(),
			Limits,
			Codec,
			*FileOps).IsSuccess()));

		const FString PackPath = Paths.BuildPackPath(Fixture.Pack.PackId);
		const FString ManifestPath = Paths.BuildManifestPath(
			Fixture.Manifest.ComputedGenerationId);
		TArray<uint8> PhysicalPack;
		TArray<uint8> PhysicalManifest;
		ASSERT_THAT(IsTrue(FileOps->ReopenReadAll(
			PackPath, Limits.MaxPackBytes, PhysicalPack).IsSuccess()));
		ASSERT_THAT(IsTrue(FileOps->ReopenReadAll(
			ManifestPath, Limits.MaxManifestBytes, PhysicalManifest).IsSuccess()));
		ASSERT_THAT(AreEqual(Fixture.Pack.Bytes, PhysicalPack));
		ASSERT_THAT(AreEqual(Fixture.Manifest.CompleteBytes, PhysicalManifest));

		ASSERT_THAT(IsTrue(PutAngelscriptCachePackIfAbsent(
			Paths,
			Fixture.Pack.PackId,
			Fixture.Pack.Bytes,
			Token.GetValue(),
			Limits,
			*FileOps).IsSuccess()));
		ASSERT_THAT(IsTrue(PutAngelscriptCacheManifestIfAbsent(
			Paths,
			Fixture.Manifest.ComputedGenerationId,
			Fixture.Manifest.CompleteBytes,
			Token.GetValue(),
			Limits,
			Codec,
			*FileOps).IsSuccess()));

		TArray<FString> TempFiles;
		IFileManager::Get().FindFilesRecursive(
			TempFiles, *Paths.NamespaceRoot, TEXT("*.tmp.*"), true, false, false);
		ASSERT_THAT(AreEqual(0, TempFiles.Num(),
			TEXT("Successful install and reuse must leave no publication temps")));
	}

	TEST_METHOD(ProductionFileOpsAtomicallyReplaceAndRemovePointer)
	{
		using namespace AngelscriptCacheStoreDiskTests_Private;
		FScopedDiskRoot Disk;
		TUniquePtr<IAngelscriptCacheAtomicFileOps> FileOps =
			CreateAngelscriptCacheAtomicFileOps();
		ASSERT_THAT(IsNotNull(FileOps.Get()));

		const FString PointerPath = Disk.Root / TEXT("Current.ascurrent");
		const FString FirstTemp = PointerPath
			+ TEXT(".tmp.1-0123456789abcdef0123456789abcdef");
		const FString SecondTemp = PointerPath
			+ TEXT(".tmp.1-fedcba9876543210fedcba9876543210");
		const TArray<uint8> First = MakeBytes(0x13);
		const TArray<uint8> Second = MakeBytes(0x63);

		ASSERT_THAT(IsTrue(FileOps->WriteFlushClose(FirstTemp, First).IsSuccess()));
		ASSERT_THAT(IsTrue(FileOps->AtomicInstallOrReplacePointer(
			FirstTemp, PointerPath).IsSuccess()));
		ASSERT_THAT(IsTrue(FileOps->WriteFlushClose(SecondTemp, Second).IsSuccess()));
		ASSERT_THAT(IsTrue(FileOps->AtomicInstallOrReplacePointer(
			SecondTemp, PointerPath).IsSuccess()));
		ASSERT_THAT(IsTrue(FileOps->SyncDirectory(Disk.Root).IsSuccess()));

		TArray<uint8> Reopened;
		ASSERT_THAT(IsTrue(FileOps->ReopenReadAll(
			PointerPath, Second.Num(), Reopened).IsSuccess()));
		ASSERT_THAT(AreEqual(Second, Reopened));
		ASSERT_THAT(IsTrue(FileOps->AtomicRemovePointer(PointerPath).IsSuccess()));
		ASSERT_THAT(IsTrue(FileOps->SyncDirectory(Disk.Root).IsSuccess()));
		ASSERT_THAT(IsFalse(IFileManager::Get().FileExists(*PointerPath)));
	}

	TEST_METHOD(ProductionCleanupRemovesOnlyStrictDirectStaleTemps)
	{
		using namespace AngelscriptCacheStoreDiskTests_Private;
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
			Disk.Root / TEXT("StaleTempCleanup/CacheV2"),
			Fixture.Compatibility,
			Fixture.Context,
			*FileOps,
			Paths).IsSuccess()));
		ASSERT_THAT(IsTrue(EnsureAngelscriptCacheStoreDirectories(
			Paths, *FileOps).IsSuccess()));
		const TOptional<FAngelscriptCacheWriterToken> Token =
			FAngelscriptCacheWriterToken::TryParse(
				TEXT("6201-00112233445566778899aabbccddeeff"));
		ASSERT_THAT(IsTrue(Token.IsSet()));
		const FString TokenText = Token->ToString();
		const TArray<uint8> Marker = MakeBytes(0x4d);

		const TArray<FString> ValidTemps = {
			Paths.BuildPackTempPath(Fixture.Pack.PackId, Token.GetValue()),
			Paths.BuildManifestTempPath(
				Fixture.Manifest.ComputedGenerationId, Token.GetValue()),
			Paths.BuildCurrentPointerTempPath(Token.GetValue()),
			Paths.BuildPreviousPointerTempPath(Token.GetValue()),
			Paths.BuildPendingColdStartPointerTempPath(Token.GetValue()),
		};
		for (const FString& Path : ValidTemps)
		{
			ASSERT_THAT(IsTrue(FileOps->WriteFlushClose(Path, Marker).IsSuccess()));
		}

		const FAngelscriptHash256 PreservedPackId = RepeatedByteHash(0x71);
		const FString FinalPack = Paths.BuildPackPath(PreservedPackId);
		const FString InvalidToken = Paths.PacksDirectory
			/ (RepeatedByteHash(0x72).ToHexString()
				+ TEXT(".aspack.tmp.6201-00112233445566778899AABBCCDDEEFF"));
		const FString WrongDirectory = Paths.NamespaceRoot
			/ (RepeatedByteHash(0x73).ToHexString()
				+ TEXT(".aspack.tmp.") + TokenText);
		const FString NestedDirectory = Paths.PacksDirectory / TEXT("Nested");
		ASSERT_THAT(IsTrue(IFileManager::Get().MakeDirectory(
			*NestedDirectory, true)));
		const FString NestedTemp = NestedDirectory
			/ (RepeatedByteHash(0x74).ToHexString()
				+ TEXT(".aspack.tmp.") + TokenText);
		const FString TempShapedDirectory = Paths.GenerationsDirectory
			/ (RepeatedByteHash(0x75).ToHexString()
				+ TEXT(".asmanifest.tmp.") + TokenText);
		ASSERT_THAT(IsTrue(IFileManager::Get().MakeDirectory(
			*TempShapedDirectory, true)));
		const TArray<FString> PreservedFiles = {
			FinalPack,
			InvalidToken,
			WrongDirectory,
			NestedTemp,
		};
		for (const FString& Path : PreservedFiles)
		{
			ASSERT_THAT(IsTrue(FileOps->WriteFlushClose(Path, Marker).IsSuccess()));
		}

		TUniquePtr<IAngelscriptCacheNamespaceLockHandle> NamespaceLock;
		ASSERT_THAT(IsTrue(AcquireAngelscriptCacheNamespaceLock(
			Paths,
			FPlatformTime::Seconds() + 2.0,
			[]() { return false; },
			*LockOps,
			NamespaceLock).IsSuccess()));
		ASSERT_THAT(IsNotNull(NamespaceLock.Get()));
		const FAngelscriptCacheStoreResult Result =
			CleanupAngelscriptCacheStaleTempsUnderLock(
				Paths, *NamespaceLock, *FileOps);
		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreError::None, Result.Error));

		for (const FString& Path : ValidTemps)
		{
			ASSERT_THAT(IsFalse(IFileManager::Get().FileExists(*Path),
				*FString::Printf(TEXT("Expected stale temp removal: %s"), *Path)));
		}
		for (const FString& Path : PreservedFiles)
		{
			ASSERT_THAT(IsTrue(IFileManager::Get().FileExists(*Path),
				*FString::Printf(TEXT("Unexpected cleanup removal: %s"), *Path)));
		}
		ASSERT_THAT(IsTrue(IFileManager::Get().DirectoryExists(
			*TempShapedDirectory)));
	}

	TEST_METHOD(ProductionNamespaceLockSerializesThreadsAndCanBeReacquired)
	{
		using namespace AngelscriptCacheStoreDiskTests_Private;
		FScopedDiskRoot Disk;
		TUniquePtr<IAngelscriptCacheAtomicFileOps> FileOps =
			CreateAngelscriptCacheAtomicFileOps();
		TUniquePtr<IAngelscriptCacheNamespaceLockOps> FirstLockOps =
			CreateAngelscriptCacheNamespaceLockOps();
		TUniquePtr<IAngelscriptCacheNamespaceLockOps> SecondLockOps =
			CreateAngelscriptCacheNamespaceLockOps();
		ASSERT_THAT(IsNotNull(FileOps.Get()));
		ASSERT_THAT(IsNotNull(FirstLockOps.Get()));
		ASSERT_THAT(IsNotNull(SecondLockOps.Get()));

		FAngelscriptCacheStorePaths Paths;
		ASSERT_THAT(IsTrue(BuildAngelscriptCacheStorePaths(
			Disk.Root / TEXT("NamespaceLock/CacheV2"),
			FAngelscriptCacheCompatibilityKey{RepeatedByteHash(0x29)},
			FAngelscriptCacheContextKey{RepeatedByteHash(0x4b)},
			*FileOps,
			Paths).IsSuccess()));
		TUniquePtr<IAngelscriptCacheNamespaceLockHandle> FirstLock;
		ASSERT_THAT(IsTrue(AcquireAngelscriptCacheNamespaceLock(
			Paths,
			FPlatformTime::Seconds() + 1.0,
			[]() { return false; },
			*FirstLockOps,
			FirstLock).IsSuccess()));
		ASSERT_THAT(IsNotNull(FirstLock.Get()));

		TFuture<FAngelscriptCacheStoreResult> ContendedResult = Async(
			EAsyncExecution::Thread,
			[&Paths, &SecondLockOps]()
			{
				TUniquePtr<IAngelscriptCacheNamespaceLockHandle> ContendedLock;
				return AcquireAngelscriptCacheNamespaceLock(
					Paths,
					FPlatformTime::Seconds() + 0.2,
					[]() { return false; },
					*SecondLockOps,
					ContendedLock);
			});
		const FAngelscriptCacheStoreResult TimeoutResult = ContendedResult.Get();
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreError::LockTimeout, TimeoutResult.Error));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreStage::LockAcquisition, TimeoutResult.Stage));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreCommitState::NotCommitted,
			TimeoutResult.CommitState));

		FirstLock.Reset();
		TUniquePtr<IAngelscriptCacheNamespaceLockHandle> ReacquiredLock;
		ASSERT_THAT(IsTrue(AcquireAngelscriptCacheNamespaceLock(
			Paths,
			FPlatformTime::Seconds() + 1.0,
			[]() { return false; },
			*SecondLockOps,
			ReacquiredLock).IsSuccess()));
		ASSERT_THAT(IsNotNull(ReacquiredLock.Get()));
	}

	TEST_METHOD(ProductionWriterPublishesRotatesReopensAndReusesCompleteGenerations)
	{
		using namespace AngelscriptCacheStoreDiskTests_Private;
		FScopedDiskRoot Disk;
		const FGenerationFixture First = MakeGenerationFixture(1);
		const FGenerationFixture Second = MakeGenerationFixture(2);
		TUniquePtr<IAngelscriptCacheAtomicFileOps> FileOps =
			CreateAngelscriptCacheAtomicFileOps();
		TUniquePtr<IAngelscriptCacheNamespaceLockOps> LockOps =
			CreateAngelscriptCacheNamespaceLockOps();
		ASSERT_THAT(IsNotNull(FileOps.Get()));
		ASSERT_THAT(IsNotNull(LockOps.Get()));

		FAngelscriptCacheStorePaths Paths;
		ASSERT_THAT(IsTrue(BuildAngelscriptCacheStorePaths(
			Disk.Root / TEXT("WriterTransaction/CacheV2"),
			First.Compatibility,
			First.Context,
			*FileOps,
			Paths).IsSuccess()));
		const TOptional<FAngelscriptCacheWriterToken> FirstToken =
			FAngelscriptCacheWriterToken::TryParse(
				TEXT("6201-00112233445566778899aabbccddeeff"));
		const TOptional<FAngelscriptCacheWriterToken> SecondToken =
			FAngelscriptCacheWriterToken::TryParse(
				TEXT("6201-fedcba98765432100123456789abcdef"));
		const TOptional<FAngelscriptCacheWriterToken> NoOpToken =
			FAngelscriptCacheWriterToken::TryParse(
				TEXT("6201-11223344556677889900aabbccddeeff"));
		ASSERT_THAT(IsTrue(FirstToken.IsSet()));
		ASSERT_THAT(IsTrue(SecondToken.IsSet()));
		ASSERT_THAT(IsTrue(NoOpToken.IsSet()));
		FAngelscriptCacheReadLimits Limits;
		FAngelscriptUnrealZlibCacheStorageCodec Codec;

		FAngelscriptCacheReadBudget FirstBudget;
		const FAngelscriptCacheStoreResult FirstResult =
			PublishAngelscriptCacheGeneration(
				Paths,
				EAngelscriptCachePublicationDisposition::Current,
				TOptional<FAngelscriptHash256>{},
				MakeArrayView(&First.Pack, 1),
				First.ManifestValue,
				First.Manifest,
				FirstToken.GetValue(),
				Limits,
				FirstBudget,
				FPlatformTime::Seconds() + 2.0,
				[]() { return false; },
				Codec,
				*LockOps,
				*FileOps);
		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreError::None, FirstResult.Error));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreCommitState::CurrentCommitted,
			FirstResult.CommitState));

		FAngelscriptCacheReadBudget SecondBudget;
		const FAngelscriptCacheStoreResult SecondResult =
			PublishAngelscriptCacheGeneration(
				Paths,
				EAngelscriptCachePublicationDisposition::Current,
				First.Manifest.ComputedGenerationId,
				MakeArrayView(&Second.Pack, 1),
				Second.ManifestValue,
				Second.Manifest,
				SecondToken.GetValue(),
				Limits,
				SecondBudget,
				FPlatformTime::Seconds() + 2.0,
				[]() { return false; },
				Codec,
				*LockOps,
				*FileOps);
		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreError::None, SecondResult.Error));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreCommitState::CurrentCommitted,
			SecondResult.CommitState));
		ASSERT_THAT(IsTrue(SecondResult.GenerationBefore.IsSet()));
		ASSERT_THAT(IsTrue(SecondResult.GenerationBefore.GetValue()
			== First.Manifest.ComputedGenerationId));
		ASSERT_THAT(IsTrue(SecondResult.GenerationAfter.IsSet()));
		ASSERT_THAT(IsTrue(SecondResult.GenerationAfter.GetValue()
			== Second.Manifest.ComputedGenerationId));

		TUniquePtr<IAngelscriptCacheNamespaceLockHandle> ReadLock;
		ASSERT_THAT(IsTrue(AcquireAngelscriptCacheNamespaceLock(
			Paths,
			FPlatformTime::Seconds() + 2.0,
			[]() { return false; },
			*LockOps,
			ReadLock).IsSuccess()));
		TOptional<FAngelscriptHash256> CurrentId;
		TOptional<FAngelscriptHash256> PreviousId;
		ASSERT_THAT(IsTrue(ReadAngelscriptCachePointerSlot(
			Paths, EAngelscriptCachePointerKind::Current,
			*FileOps, CurrentId).IsSuccess()));
		ASSERT_THAT(IsTrue(ReadAngelscriptCachePointerSlot(
			Paths, EAngelscriptCachePointerKind::Previous,
			*FileOps, PreviousId).IsSuccess()));
		ASSERT_THAT(IsTrue(CurrentId.IsSet()));
		ASSERT_THAT(IsTrue(PreviousId.IsSet()));
		ASSERT_THAT(IsTrue(CurrentId.GetValue()
			== Second.Manifest.ComputedGenerationId));
		ASSERT_THAT(IsTrue(PreviousId.GetValue()
			== First.Manifest.ComputedGenerationId));

		FAngelscriptCacheReadBudget ReopenBudget;
		TOptional<FAngelscriptValidatedGeneration> CurrentGeneration;
		TOptional<FAngelscriptValidatedGeneration> PreviousGeneration;
		ASSERT_THAT(IsTrue(ReadAndValidateAngelscriptCacheGenerationUnderLock(
			Paths, CurrentId.GetValue(), Limits, ReopenBudget, Codec,
			*ReadLock, *FileOps, CurrentGeneration).IsSuccess()));
		ASSERT_THAT(IsTrue(ReadAndValidateAngelscriptCacheGenerationUnderLock(
			Paths, PreviousId.GetValue(), Limits, ReopenBudget, Codec,
			*ReadLock, *FileOps, PreviousGeneration).IsSuccess()));
		ASSERT_THAT(IsTrue(CurrentGeneration.IsSet()));
		ASSERT_THAT(IsTrue(PreviousGeneration.IsSet()));
		ReadLock.Reset();

		FAngelscriptCacheReadBudget NoOpBudget;
		const FAngelscriptCacheStoreResult NoOpResult =
			PublishAngelscriptCacheGeneration(
				Paths,
				EAngelscriptCachePublicationDisposition::Current,
				Second.Manifest.ComputedGenerationId,
				MakeArrayView(&Second.Pack, 1),
				Second.ManifestValue,
				Second.Manifest,
				NoOpToken.GetValue(),
				Limits,
				NoOpBudget,
				FPlatformTime::Seconds() + 2.0,
				[]() { return false; },
				Codec,
				*LockOps,
				*FileOps);
		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreError::None, NoOpResult.Error));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreCommitState::NotCommitted,
			NoOpResult.CommitState));

		TArray<FString> TempFiles;
		IFileManager::Get().FindFilesRecursive(
			TempFiles, *Paths.NamespaceRoot, TEXT("*.tmp.*"), true, false, false);
		ASSERT_THAT(AreEqual(0, TempFiles.Num(),
			TEXT("Physical writer transactions must leave no temp files")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
