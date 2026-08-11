#include "Cache/AngelscriptCacheStore.h"
#include "Cache/AngelscriptCacheArchive.h"
#include "Cache/AngelscriptCacheManifestPack.h"
#include "Cache/AngelscriptCacheSemanticRecords.h"

#include "CQTest.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheStoreTests,
	"Angelscript.TestModule.Cache.Store",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static FAngelscriptHash256 RepeatedByteHash(const uint8 Byte)
	{
		FBlake3Hash::ByteArray Bytes{};
		FMemory::Memset(Bytes, Byte, sizeof(Bytes));
		return FAngelscriptHash256{FBlake3Hash(Bytes)};
	}

	class FCanonicalRootFileOps final : public IAngelscriptCacheAtomicFileOps
	{
	public:
		FString CanonicalAbsoluteRoot;
		FString LastRequestedRoot;
		TMap<FString, TArray<uint8>> Files;
		TArray<FString> Calls;
		bool bTruncateNextWrite = false;

		virtual FAngelscriptCacheStoreResult CanonicalizeAndValidateRoot(
			const FString& RequestedBaseRoot,
			FAngelscriptCanonicalCacheRoot& OutRoot) override
		{
			LastRequestedRoot = RequestedBaseRoot;
			Calls.Add(TEXT("Canonicalize:") + RequestedBaseRoot);
			OutRoot.AbsolutePath = CanonicalAbsoluteRoot;
			OutRoot.IdentityPath = CanonicalAbsoluteRoot;
			return FAngelscriptCacheStoreResult::Success();
		}

		virtual FAngelscriptCacheStoreResult EnsureDirectoryTree(
			const FString& DirectoryPath) override
		{
			Calls.Add(TEXT("EnsureDirectory:") + DirectoryPath);
			return FAngelscriptCacheStoreResult::Success();
		}

		virtual FAngelscriptCacheStoreResult WriteFlushClose(
			const FString& TempPath,
			TConstArrayView<uint8> Bytes) override
		{
			Calls.Add(TEXT("Write:") + TempPath);
			TArray<uint8> StoredBytes(Bytes);
			if (bTruncateNextWrite && !StoredBytes.IsEmpty())
			{
				StoredBytes.Pop(EAllowShrinking::No);
				bTruncateNextWrite = false;
			}
			Files.Add(TempPath, MoveTemp(StoredBytes));
			return FAngelscriptCacheStoreResult::Success();
		}

		virtual FAngelscriptCacheStoreResult ReopenReadAll(
			const FString& Path,
			uint64 MaxBytes,
			TArray<uint8>& OutBytes) override
		{
			Calls.Add(TEXT("Read:") + Path);
			const TArray<uint8>* Existing = Files.Find(Path);
			if (Existing == nullptr)
			{
				return FAngelscriptCacheStoreResult::Failure(
					Path.EndsWith(TEXT(".asmanifest"))
						? EAngelscriptCacheStoreError::ManifestMissing
						: EAngelscriptCacheStoreError::PackMissing,
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
			return UnsupportedCall();
		}

		virtual FAngelscriptCacheStoreResult AtomicRemovePointer(
			const FString& PointerPath) override
		{
			return UnsupportedCall();
		}

		virtual FAngelscriptCacheStoreResult RemoveOwnTemp(
			const FString& TempPath) override
		{
			Calls.Add(TEXT("RemoveTemp:") + TempPath);
			Files.Remove(TempPath);
			return FAngelscriptCacheStoreResult::Success();
		}

		virtual FAngelscriptCacheStoreResult RemoveFinalImmutable(
			const FString&) override
		{
			return UnsupportedCall();
		}

		virtual FAngelscriptCacheStoreResult SyncDirectory(
			const FString& DirectoryPath) override
		{
			Calls.Add(TEXT("Sync:") + DirectoryPath);
			return FAngelscriptCacheStoreResult::Success();
		}

		virtual bool SupportsSharedAtomicCacheStore() const override
		{
			return true;
		}

	private:
		static FAngelscriptCacheStoreResult UnsupportedCall()
		{
			return FAngelscriptCacheStoreResult::Failure(
				EAngelscriptCacheStoreError::UnsupportedPlatformAtomicity,
				EAngelscriptCacheStoreStage::None);
		}
	};

	static FAngelscriptEncodedPack MakeEmptySourcePack()
	{
		FAngelscriptPreparedRecord Record;
		check(FAngelscriptCacheRecordArchive::TryBuildRecordId(
			EAngelscriptCacheRecordKind::SourceIndex, {}, Record.RecordId).IsSuccess());
		FAngelscriptCachePackPolicy Policy;
		Policy.CompressionPolicy = EAngelscriptCachePackCompressionPolicy::ForceNoneForTest;
		FAngelscriptUnrealZlibCacheStorageCodec Codec;
		TArray<FAngelscriptEncodedPack> Packs;
		check(BuildAngelscriptCachePacks(MakeArrayView(&Record, 1), Policy, Codec, Packs).IsSuccess());
		check(Packs.Num() == 1);
		return MoveTemp(Packs[0]);
	}

	struct FZeroModuleGenerationFixture
	{
		FAngelscriptEncodedPack Pack;
		FAngelscriptEncodedCacheGenerationManifest Manifest;
		FAngelscriptCacheCompatibilityKey Compatibility;
		FAngelscriptCacheContextKey Context;
	};

	static FZeroModuleGenerationFixture MakeZeroModuleGeneration()
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
		Policy.CompressionPolicy = EAngelscriptCachePackCompressionPolicy::ForceNoneForTest;
		FAngelscriptUnrealZlibCacheStorageCodec Codec;
		TArray<FAngelscriptEncodedPack> Packs;
		check(BuildAngelscriptCachePacks(
			MakeArrayView(&SourceRecord, 1), Policy, Codec, Packs).IsSuccess());
		check(Packs.Num() == 1 && Packs[0].Index.Num() == 1);

		FZeroModuleGenerationFixture Fixture;
		Fixture.Compatibility.Hash = RepeatedByteHash(0x31);
		Fixture.Context.Hash = RepeatedByteHash(0x52);
		Fixture.Pack = MoveTemp(Packs[0]);
		FAngelscriptCacheGenerationManifest Value;
		Value.ManifestSchemaVersion =
			FAngelscriptCacheManifestPackArchive::ManifestSchemaVersion;
		Value.Compatibility = Fixture.Compatibility;
		Value.Context = Fixture.Context;
		Value.Profile = FAngelscriptArtifactIdentityBuilder::BuildArtifactProfileKey(
			Value.Compatibility, Value.Context);
		Value.SourceSnapshot = Source.SourceSnapshot;
		Value.SourceIndexRecordId = SourceRecord.RecordId;
		const FAngelscriptCachePackIndexEntry& PackIndex = Fixture.Pack.Index[0];
		FAngelscriptCacheRecordIndexEntry& ManifestEntry = Value.Records.AddDefaulted_GetRef();
		ManifestEntry.RecordId = SourceRecord.RecordId;
		ManifestEntry.Location.PackId = Fixture.Pack.PackId;
		ManifestEntry.Location.PackOffset = PackIndex.PackOffset;
		ManifestEntry.Location.StoredSize = PackIndex.StoredSize;
		ManifestEntry.Location.RawSize = PackIndex.RawSize;
		ManifestEntry.Location.Codec = PackIndex.Codec;
		ManifestEntry.Location.RawChecksum = PackIndex.RawChecksum;
		check(EncodeAngelscriptCacheGenerationManifest(
			Value, Fixture.Manifest).IsSuccess());
		return Fixture;
	}

public:
	TEST_METHOD(DefaultRootLivesExactlyUnderProjectSaved)
	{
		FAngelscriptCacheRootSelectionInputs Inputs;
		Inputs.ProjectSavedDirectory = TEXT("D:/Project/Saved");
		Inputs.LaunchWorkingDirectory = TEXT("D:/Launch");
		FString RequestedRoot;

		const FAngelscriptCacheStoreResult Result =
			ResolveAngelscriptCacheRequestedBaseRoot(Inputs, RequestedRoot);

		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreError::None, Result.Error));
		ASSERT_THAT(AreEqual(
			FString(TEXT("D:/Project/Saved/Angelscript/CacheV2")), RequestedRoot,
			TEXT("The default root must be exactly the Saved CacheV2 root")));
	}

	TEST_METHOD(RelativeOverrideReplacesDefaultAndResolvesAgainstLaunchDirectory)
	{
		FAngelscriptCacheRootSelectionInputs Inputs;
		Inputs.ProjectSavedDirectory = TEXT("D:/Project/Saved");
		Inputs.LaunchWorkingDirectory = TEXT("E:/LaunchRoot");
		Inputs.Override = FString(TEXT("Diagnostics/CacheScratch"));
		FString RequestedRoot;

		const FAngelscriptCacheStoreResult Result =
			ResolveAngelscriptCacheRequestedBaseRoot(Inputs, RequestedRoot);

		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreError::None, Result.Error));
		ASSERT_THAT(AreEqual(
			FString(TEXT("E:/LaunchRoot/Diagnostics/CacheScratch")), RequestedRoot,
			TEXT("A relative override must replace the full default and resolve once against launch cwd")));
		ASSERT_THAT(IsFalse(RequestedRoot.Contains(TEXT("Angelscript/CacheV2")),
			TEXT("The service must not append the default suffix below an override")));
	}

	TEST_METHOD(WriterTokenRequiresCanonicalPidAndLowercaseNonce)
	{
		const TOptional<FAngelscriptCacheWriterToken> Valid =
			FAngelscriptCacheWriterToken::TryParse(
				TEXT("4275-0123456789abcdef0123456789abcdef"));

		ASSERT_THAT(IsTrue(Valid.IsSet(), TEXT("A canonical writer token should parse")));
		ASSERT_THAT(AreEqual(
			FString(TEXT("4275-0123456789abcdef0123456789abcdef")),
			Valid->ToString()));
		ASSERT_THAT(IsFalse(FAngelscriptCacheWriterToken::TryParse(
			TEXT("04275-0123456789abcdef0123456789abcdef")).IsSet(),
			TEXT("PID spelling must be canonical decimal without leading zeros")));
		ASSERT_THAT(IsFalse(FAngelscriptCacheWriterToken::TryParse(
			TEXT("4275-0123456789ABCDEF0123456789abcdef")).IsSet(),
			TEXT("The 128-bit nonce must use lower-case hex")));
		ASSERT_THAT(IsFalse(FAngelscriptCacheWriterToken::TryParse(
			TEXT("4275-0123456789abcdef0123456789abcde")).IsSet(),
			TEXT("The nonce must contain exactly 32 hex characters")));
		ASSERT_THAT(IsFalse(FAngelscriptCacheWriterToken::TryParse(
			TEXT("pid-0123456789abcdef0123456789abcdef")).IsSet(),
			TEXT("The process component must be decimal")));
	}

	TEST_METHOD(BuildsOnlyTheFiveSameDirectoryTempNameShapes)
	{
		FCanonicalRootFileOps FileOps;
		FileOps.CanonicalAbsoluteRoot = TEXT("D:/Saved/Angelscript/CacheV2");
		FAngelscriptCacheStorePaths Paths;
		const FAngelscriptCacheStoreResult PathResult = BuildAngelscriptCacheStorePaths(
			TEXT("D:/Saved/Angelscript/CacheV2"),
			FAngelscriptCacheCompatibilityKey{RepeatedByteHash(0x21)},
			FAngelscriptCacheContextKey{RepeatedByteHash(0x43)},
			FileOps,
			Paths);
		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreError::None, PathResult.Error));

		const TOptional<FAngelscriptCacheWriterToken> Token =
			FAngelscriptCacheWriterToken::TryParse(
				TEXT("4275-0123456789abcdef0123456789abcdef"));
		ASSERT_THAT(IsTrue(Token.IsSet()));
		const FString Suffix = TEXT(".tmp.4275-0123456789abcdef0123456789abcdef");
		const FAngelscriptHash256 PackId = RepeatedByteHash(0x65);
		const FAngelscriptHash256 GenerationId = RepeatedByteHash(0x87);

		ASSERT_THAT(AreEqual(
			Paths.BuildPackPath(PackId) + Suffix,
			Paths.BuildPackTempPath(PackId, Token.GetValue())));
		ASSERT_THAT(AreEqual(
			Paths.BuildManifestPath(GenerationId) + Suffix,
			Paths.BuildManifestTempPath(GenerationId, Token.GetValue())));
		ASSERT_THAT(AreEqual(
			Paths.CurrentPointer + Suffix,
			Paths.BuildCurrentPointerTempPath(Token.GetValue())));
		ASSERT_THAT(AreEqual(
			Paths.PreviousPointer + Suffix,
			Paths.BuildPreviousPointerTempPath(Token.GetValue())));
		ASSERT_THAT(AreEqual(
			Paths.PendingColdStartPointer + Suffix,
			Paths.BuildPendingColdStartPointerTempPath(Token.GetValue())));
	}

	TEST_METHOD(ExistingIdenticalPackIsValidatedAndReusedWithoutMutation)
	{
		FCanonicalRootFileOps FileOps;
		FileOps.CanonicalAbsoluteRoot = TEXT("D:/Saved/Angelscript/CacheV2");
		FAngelscriptCacheStorePaths Paths;
		ASSERT_THAT(IsTrue(BuildAngelscriptCacheStorePaths(
			TEXT("D:/Saved/Angelscript/CacheV2"),
			FAngelscriptCacheCompatibilityKey{RepeatedByteHash(0x21)},
			FAngelscriptCacheContextKey{RepeatedByteHash(0x43)},
			FileOps,
			Paths).IsSuccess()));
		const FAngelscriptEncodedPack Pack = MakeEmptySourcePack();
		const FString FinalPath = Paths.BuildPackPath(Pack.PackId);
		FileOps.Files.Add(FinalPath, Pack.Bytes);
		FileOps.Calls.Reset();
		const TOptional<FAngelscriptCacheWriterToken> Token =
			FAngelscriptCacheWriterToken::TryParse(
				TEXT("4275-0123456789abcdef0123456789abcdef"));
		ASSERT_THAT(IsTrue(Token.IsSet()));

		const FAngelscriptCacheStoreResult Result = PutAngelscriptCachePackIfAbsent(
			Paths,
			Pack.PackId,
			Pack.Bytes,
			Token.GetValue(),
			FAngelscriptCacheReadLimits{},
			FileOps);

		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreError::None, Result.Error));
		ASSERT_THAT(AreEqual(1, FileOps.Calls.Num(),
			TEXT("An existing immutable object should require exactly one complete reread")));
		ASSERT_THAT(AreEqual(TEXT("Read:") + FinalPath, FileOps.Calls[0]));
		ASSERT_THAT(AreEqual(Pack.Bytes, FileOps.Files[FinalPath]));
	}

	TEST_METHOD(MissingPackFlushesReopensRenamesSyncsAndRevalidatesFinal)
	{
		FCanonicalRootFileOps FileOps;
		FileOps.CanonicalAbsoluteRoot = TEXT("D:/Saved/Angelscript/CacheV2");
		FAngelscriptCacheStorePaths Paths;
		ASSERT_THAT(IsTrue(BuildAngelscriptCacheStorePaths(
			TEXT("D:/Saved/Angelscript/CacheV2"),
			FAngelscriptCacheCompatibilityKey{RepeatedByteHash(0x21)},
			FAngelscriptCacheContextKey{RepeatedByteHash(0x43)},
			FileOps,
			Paths).IsSuccess()));
		const FAngelscriptEncodedPack Pack = MakeEmptySourcePack();
		const FString FinalPath = Paths.BuildPackPath(Pack.PackId);
		const TOptional<FAngelscriptCacheWriterToken> Token =
			FAngelscriptCacheWriterToken::TryParse(
				TEXT("4275-0123456789abcdef0123456789abcdef"));
		ASSERT_THAT(IsTrue(Token.IsSet()));
		const FString TempPath = Paths.BuildPackTempPath(Pack.PackId, Token.GetValue());
		FileOps.Calls.Reset();

		const FAngelscriptCacheStoreResult Result = PutAngelscriptCachePackIfAbsent(
			Paths,
			Pack.PackId,
			Pack.Bytes,
			Token.GetValue(),
			FAngelscriptCacheReadLimits{},
			FileOps);

		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreError::None, Result.Error));
		ASSERT_THAT(AreEqual(6, FileOps.Calls.Num(),
			TEXT("A missing Pack must cross every durable immutable-install boundary")));
		ASSERT_THAT(AreEqual(TEXT("Read:") + FinalPath, FileOps.Calls[0]));
		ASSERT_THAT(AreEqual(TEXT("Write:") + TempPath, FileOps.Calls[1]));
		ASSERT_THAT(AreEqual(TEXT("Read:") + TempPath, FileOps.Calls[2]));
		ASSERT_THAT(AreEqual(
			TEXT("Rename:") + TempPath + TEXT("->") + FinalPath, FileOps.Calls[3]));
		ASSERT_THAT(AreEqual(TEXT("Sync:") + Paths.PacksDirectory, FileOps.Calls[4]));
		ASSERT_THAT(AreEqual(TEXT("Read:") + FinalPath, FileOps.Calls[5]));
		ASSERT_THAT(IsFalse(FileOps.Files.Contains(TempPath)));
		ASSERT_THAT(AreEqual(Pack.Bytes, FileOps.Files[FinalPath]));
	}

	TEST_METHOD(CorruptReopenedTempFailsBeforeRenameAndRemovesOnlyOwnTemp)
	{
		FCanonicalRootFileOps FileOps;
		FileOps.CanonicalAbsoluteRoot = TEXT("D:/Saved/Angelscript/CacheV2");
		FAngelscriptCacheStorePaths Paths;
		ASSERT_THAT(IsTrue(BuildAngelscriptCacheStorePaths(
			TEXT("D:/Saved/Angelscript/CacheV2"),
			FAngelscriptCacheCompatibilityKey{RepeatedByteHash(0x21)},
			FAngelscriptCacheContextKey{RepeatedByteHash(0x43)},
			FileOps,
			Paths).IsSuccess()));
		const FAngelscriptEncodedPack Pack = MakeEmptySourcePack();
		const FString FinalPath = Paths.BuildPackPath(Pack.PackId);
		const TOptional<FAngelscriptCacheWriterToken> Token =
			FAngelscriptCacheWriterToken::TryParse(
				TEXT("4275-0123456789abcdef0123456789abcdef"));
		ASSERT_THAT(IsTrue(Token.IsSet()));
		const FString TempPath = Paths.BuildPackTempPath(Pack.PackId, Token.GetValue());
		FileOps.bTruncateNextWrite = true;
		FileOps.Calls.Reset();

		const FAngelscriptCacheStoreResult Result = PutAngelscriptCachePackIfAbsent(
			Paths,
			Pack.PackId,
			Pack.Bytes,
			Token.GetValue(),
			FAngelscriptCacheReadLimits{},
			FileOps);

		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreError::ContentValidationFailed, Result.Error));
		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreStage::PackTemp, Result.Stage));
		ASSERT_THAT(IsTrue(Result.ContentValidation.IsSet()));
		ASSERT_THAT(AreEqual(4, FileOps.Calls.Num()));
		ASSERT_THAT(AreEqual(TEXT("Read:") + FinalPath, FileOps.Calls[0]));
		ASSERT_THAT(AreEqual(TEXT("Write:") + TempPath, FileOps.Calls[1]));
		ASSERT_THAT(AreEqual(TEXT("Read:") + TempPath, FileOps.Calls[2]));
		ASSERT_THAT(AreEqual(TEXT("RemoveTemp:") + TempPath, FileOps.Calls[3]));
		ASSERT_THAT(IsFalse(FileOps.Files.Contains(TempPath)));
		ASSERT_THAT(IsFalse(FileOps.Files.Contains(FinalPath)));
	}

	TEST_METHOD(MissingManifestValidatesFinalPacksBeforeAndAfterImmutableInstall)
	{
		const FZeroModuleGenerationFixture Fixture = MakeZeroModuleGeneration();
		FCanonicalRootFileOps FileOps;
		FileOps.CanonicalAbsoluteRoot = TEXT("D:/Saved/Angelscript/CacheV2");
		FAngelscriptCacheStorePaths Paths;
		ASSERT_THAT(IsTrue(BuildAngelscriptCacheStorePaths(
			TEXT("D:/Saved/Angelscript/CacheV2"),
			Fixture.Compatibility,
			Fixture.Context,
			FileOps,
			Paths).IsSuccess()));
		const FString PackPath = Paths.BuildPackPath(Fixture.Pack.PackId);
		const FString FinalPath = Paths.BuildManifestPath(
			Fixture.Manifest.ComputedGenerationId);
		FileOps.Files.Add(PackPath, Fixture.Pack.Bytes);
		const TOptional<FAngelscriptCacheWriterToken> Token =
			FAngelscriptCacheWriterToken::TryParse(
				TEXT("4275-0123456789abcdef0123456789abcdef"));
		ASSERT_THAT(IsTrue(Token.IsSet()));
		const FString TempPath = Paths.BuildManifestTempPath(
			Fixture.Manifest.ComputedGenerationId, Token.GetValue());
		FileOps.Calls.Reset();
		FAngelscriptUnrealZlibCacheStorageCodec Codec;

		const FAngelscriptCacheStoreResult Result = PutAngelscriptCacheManifestIfAbsent(
			Paths,
			Fixture.Manifest.ComputedGenerationId,
			Fixture.Manifest.CompleteBytes,
			Token.GetValue(),
			FAngelscriptCacheReadLimits{},
			Codec,
			FileOps);

		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreError::None, Result.Error));
		ASSERT_THAT(AreEqual(8, FileOps.Calls.Num()));
		ASSERT_THAT(AreEqual(TEXT("Read:") + FinalPath, FileOps.Calls[0]));
		ASSERT_THAT(AreEqual(TEXT("Write:") + TempPath, FileOps.Calls[1]));
		ASSERT_THAT(AreEqual(TEXT("Read:") + TempPath, FileOps.Calls[2]));
		ASSERT_THAT(AreEqual(TEXT("Read:") + PackPath, FileOps.Calls[3]));
		ASSERT_THAT(AreEqual(
			TEXT("Rename:") + TempPath + TEXT("->") + FinalPath, FileOps.Calls[4]));
		ASSERT_THAT(AreEqual(TEXT("Sync:") + Paths.GenerationsDirectory, FileOps.Calls[5]));
		ASSERT_THAT(AreEqual(TEXT("Read:") + FinalPath, FileOps.Calls[6]));
		ASSERT_THAT(AreEqual(TEXT("Read:") + PackPath, FileOps.Calls[7]));
		ASSERT_THAT(IsFalse(FileOps.Files.Contains(TempPath)));
		ASSERT_THAT(AreEqual(Fixture.Manifest.CompleteBytes, FileOps.Files[FinalPath]));
	}

	TEST_METHOD(BuildsExactFullHashNamespaceAndFinalPaths)
	{
		FCanonicalRootFileOps FileOps;
		FileOps.CanonicalAbsoluteRoot = TEXT("D:/Project/Saved/Angelscript/CacheV2");

		const FAngelscriptCacheCompatibilityKey Compatibility{RepeatedByteHash(0x0a)};
		const FAngelscriptCacheContextKey Context{RepeatedByteHash(0xbc)};
		const FAngelscriptHash256 PackId = RepeatedByteHash(0x12);
		const FAngelscriptHash256 GenerationId = RepeatedByteHash(0xef);
		FAngelscriptCacheStorePaths Paths;

		const FAngelscriptCacheStoreResult Result = BuildAngelscriptCacheStorePaths(
			TEXT("D:/RequestedCacheRoot"), Compatibility, Context, FileOps, Paths);

		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreError::None, Result.Error,
			*FString::Printf(
				TEXT("A canonical Saved root should construct a writable Cache V2 namespace; error=%u stage=%u"),
				static_cast<uint32>(Result.Error), static_cast<uint32>(Result.Stage))));
		ASSERT_THAT(AreEqual(FString(TEXT("D:/RequestedCacheRoot")), FileOps.LastRequestedRoot,
			TEXT("The complete requested root should pass to the platform seam unchanged")));

		const FString ExpectedNamespace = FString::Printf(
			TEXT("D:/Project/Saved/Angelscript/CacheV2/%s/%s"),
			*Compatibility.Hash.ToHexString(), *Context.Hash.ToHexString());
		ASSERT_THAT(AreEqual(ExpectedNamespace, Paths.NamespaceRoot));
		ASSERT_THAT(AreEqual(ExpectedNamespace / TEXT("Packs"), Paths.PacksDirectory));
		ASSERT_THAT(AreEqual(ExpectedNamespace / TEXT("Generations"), Paths.GenerationsDirectory));
		ASSERT_THAT(AreEqual(ExpectedNamespace / TEXT("Current.ascurrent"), Paths.CurrentPointer));
		ASSERT_THAT(AreEqual(ExpectedNamespace / TEXT("Previous.ascurrent"), Paths.PreviousPointer));
		ASSERT_THAT(AreEqual(
			ExpectedNamespace / TEXT("PendingColdStart.ascurrent"), Paths.PendingColdStartPointer));
		ASSERT_THAT(AreEqual(
			Paths.PacksDirectory / (PackId.ToHexString() + TEXT(".aspack")),
			Paths.BuildPackPath(PackId)));
		ASSERT_THAT(AreEqual(
			Paths.GenerationsDirectory / (GenerationId.ToHexString() + TEXT(".asmanifest")),
			Paths.BuildManifestPath(GenerationId)));
		ASSERT_THAT(AreEqual(64, Compatibility.Hash.ToHexString().Len()));
		ASSERT_THAT(AreEqual(64, Context.Hash.ToHexString().Len()));
		ASSERT_THAT(AreEqual(Compatibility.Hash.ToHexString().ToLower(), Compatibility.Hash.ToHexString()));
		ASSERT_THAT(AreEqual(Context.Hash.ToHexString().ToLower(), Context.Hash.ToHexString()));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
