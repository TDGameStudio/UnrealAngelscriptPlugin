#include "Cache/AngelscriptCacheStore.h"

#include "CQTest.h"
#include "Hash/Blake3.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheStorePointerPublicationTests,
	"Angelscript.TestModule.Cache.StorePointerPublication",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	class FTestLockHandle final : public IAngelscriptCacheNamespaceLockHandle
	{
	};

	class FPointerFileOps final : public IAngelscriptCacheAtomicFileOps
	{
	public:
		virtual FAngelscriptCacheStoreResult CanonicalizeAndValidateRoot(
			const FString& RequestedBaseRoot,
			FAngelscriptCanonicalCacheRoot& OutRoot) override
		{
			OutRoot.AbsolutePath = RequestedBaseRoot;
			OutRoot.IdentityPath = RequestedBaseRoot.ToLower();
			return FAngelscriptCacheStoreResult::Success();
		}

		virtual FAngelscriptCacheStoreResult EnsureDirectoryTree(
			const FString& DirectoryPath) override
		{
			return FAngelscriptCacheStoreResult::Success();
		}

		virtual FAngelscriptCacheStoreResult WriteFlushClose(
			const FString& TempPath,
			const TConstArrayView<uint8> Bytes) override
		{
			Calls.Add(TEXT("Write:") + TempPath);
			TArray<uint8> Stored(Bytes);
			if (bCorruptNextWrite && !Stored.IsEmpty())
			{
				Stored[0] ^= 1;
				bCorruptNextWrite = false;
			}
			Files.Add(TempPath, MoveTemp(Stored));
			return FAngelscriptCacheStoreResult::Success();
		}

		virtual FAngelscriptCacheStoreResult ReopenReadAll(
			const FString& Path,
			const uint64 MaxBytes,
			TArray<uint8>& OutBytes) override
		{
			Calls.Add(TEXT("Read:") + Path);
			const TArray<uint8>* Bytes = Files.Find(Path);
			if (Bytes == nullptr)
			{
				return FAngelscriptCacheStoreResult::Failure(
					EAngelscriptCacheStoreError::PointerInvalid,
					EAngelscriptCacheStoreStage::None);
			}
			if (static_cast<uint64>(Bytes->Num()) > MaxBytes)
			{
				return FAngelscriptCacheStoreResult::Failure(
					EAngelscriptCacheStoreError::ReadFailed,
					EAngelscriptCacheStoreStage::None);
			}
			OutBytes = *Bytes;
			return FAngelscriptCacheStoreResult::Success();
		}

		virtual FAngelscriptCacheStoreResult EnumerateDirectFileNames(
			const FString& DirectoryPath,
			TArray<FString>& OutFileNames) override
		{
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
			return Unsupported();
		}

		virtual FAngelscriptCacheStoreResult AtomicInstallOrReplacePointer(
			const FString& TempPath,
			const FString& PointerPath) override
		{
			Calls.Add(TEXT("Replace:") + TempPath + TEXT("->") + PointerPath);
			if (PointerPath == FailReplacePath)
			{
				if (bInstallBeforeReplaceFailure)
				{
					MoveTempToPointer(TempPath, PointerPath);
				}
				return FAngelscriptCacheStoreResult::Failure(
					EAngelscriptCacheStoreError::AtomicReplaceFailed,
					EAngelscriptCacheStoreStage::None);
			}
			if (!MoveTempToPointer(TempPath, PointerPath))
			{
				return FAngelscriptCacheStoreResult::Failure(
					EAngelscriptCacheStoreError::AtomicReplaceFailed,
					EAngelscriptCacheStoreStage::None);
			}
			return FAngelscriptCacheStoreResult::Success();
		}

		virtual FAngelscriptCacheStoreResult AtomicRemovePointer(
			const FString& PointerPath) override
		{
			Calls.Add(TEXT("RemovePointer:") + PointerPath);
			if (PointerPath == FailRemovePath)
			{
				return FAngelscriptCacheStoreResult::Failure(
					EAngelscriptCacheStoreError::PointerRemoveFailed,
					EAngelscriptCacheStoreStage::None);
			}
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
			const FString&) override
		{
			return Unsupported();
		}

		virtual FAngelscriptCacheStoreResult SyncDirectory(
			const FString& DirectoryPath) override
		{
			Calls.Add(TEXT("Sync:") + DirectoryPath);
			++SyncCallCount;
			if (SyncCallCount == FailSyncCall)
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

		bool MoveTempToPointer(const FString& TempPath, const FString& PointerPath)
		{
			TArray<uint8> Bytes;
			if (!Files.RemoveAndCopyValue(TempPath, Bytes))
			{
				return false;
			}
			Files.Add(PointerPath, MoveTemp(Bytes));
			return true;
		}

		static FAngelscriptCacheStoreResult Unsupported()
		{
			return FAngelscriptCacheStoreResult::Failure(
				EAngelscriptCacheStoreError::UnsupportedPlatformAtomicity,
				EAngelscriptCacheStoreStage::None);
		}

		TMap<FString, TArray<uint8>> Files;
		TArray<FString> Calls;
		FString FailReplacePath;
		FString FailRemovePath;
		bool bInstallBeforeReplaceFailure = false;
		bool bCorruptNextWrite = false;
		int32 FailSyncCall = INDEX_NONE;
		int32 SyncCallCount = 0;
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
		Paths.NamespaceRoot = TEXT("D:/Saved/Angelscript/CacheV2/compat/context");
		Paths.CurrentPointer = Paths.NamespaceRoot / TEXT("Current.ascurrent");
		Paths.PreviousPointer = Paths.NamespaceRoot / TEXT("Previous.ascurrent");
		Paths.PendingColdStartPointer =
			Paths.NamespaceRoot / TEXT("PendingColdStart.ascurrent");
		return Paths;
	}

	static FAngelscriptCacheWriterToken MakeWriterToken()
	{
		return FAngelscriptCacheWriterToken::TryParse(
			TEXT("7031-0123456789abcdef0123456789abcdef")).GetValue();
	}

	static void PutPointer(
		FPointerFileOps& FileOps,
		const FString& Path,
		const EAngelscriptCachePointerKind Kind,
		const FAngelscriptHash256& GenerationId)
	{
		TArray<uint8> Bytes;
		check(EncodeAngelscriptCachePointer({Kind, GenerationId}, Bytes).IsSuccess());
		FileOps.Files.Add(Path, MoveTemp(Bytes));
	}

	void AssertStoredPointer(
		FPointerFileOps& FileOps,
		const FString& Path,
		const EAngelscriptCachePointerKind Kind,
		const FAngelscriptHash256& GenerationId)
	{
		const TArray<uint8>* Bytes = FileOps.Files.Find(Path);
		ASSERT_THAT(IsNotNull(Bytes));
		FAngelscriptCachePointerValue Value;
		ASSERT_THAT(IsTrue(DecodeAngelscriptCachePointer(*Bytes, Kind, Value).IsSuccess()));
		ASSERT_THAT(IsTrue(Value.GenerationId == GenerationId));
	}

public:
	TEST_METHOD(RereadDistinguishesAbsentValidAndCorruptPointerSlots)
	{
		const FAngelscriptCacheStorePaths Paths = MakePaths();
		FPointerFileOps FileOps;
		TOptional<FAngelscriptHash256> GenerationId = RepeatedByteHash(0x20);

		const FAngelscriptCacheStoreResult AbsentResult =
			ReadAngelscriptCachePointerSlot(
				Paths,
				EAngelscriptCachePointerKind::Current,
				FileOps,
				GenerationId);
		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreError::None, AbsentResult.Error));
		ASSERT_THAT(IsFalse(GenerationId.IsSet()));

		const FAngelscriptHash256 ExpectedGeneration = RepeatedByteHash(0x30);
		PutPointer(FileOps, Paths.CurrentPointer,
			EAngelscriptCachePointerKind::Current, ExpectedGeneration);
		const FAngelscriptCacheStoreResult ValidResult =
			ReadAngelscriptCachePointerSlot(
				Paths,
				EAngelscriptCachePointerKind::Current,
				FileOps,
				GenerationId);
		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreError::None, ValidResult.Error));
		ASSERT_THAT(IsTrue(GenerationId.IsSet()));
		ASSERT_THAT(IsTrue(GenerationId.GetValue() == ExpectedGeneration));

		FileOps.Files[Paths.CurrentPointer][0] ^= 1;
		const FAngelscriptCacheStoreResult CorruptResult =
			ReadAngelscriptCachePointerSlot(
				Paths,
				EAngelscriptCachePointerKind::Current,
				FileOps,
				GenerationId);
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreError::PointerInvalid, CorruptResult.Error));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreStage::CurrentPointer, CorruptResult.Stage));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStorePathCategory::CurrentPointer,
			CorruptResult.PathCategory));
		ASSERT_THAT(IsFalse(GenerationId.IsSet()));
	}

	TEST_METHOD(FirstCurrentPublicationCommitsWithoutCreatingPrevious)
	{
		const FAngelscriptCacheStorePaths Paths = MakePaths();
		FPointerFileOps FileOps;
		FTestLockHandle Lock;
		const FAngelscriptHash256 NewGeneration = RepeatedByteHash(0x31);

		const FAngelscriptCacheStoreResult Result = PublishAngelscriptCachePointers(
			Paths,
			EAngelscriptCachePublicationDisposition::Current,
			NewGeneration,
			TOptional<FAngelscriptHash256>{},
			MakeWriterToken(),
			[]() { return false; },
			Lock,
			FileOps);

		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreError::None, Result.Error));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreCommitState::CurrentCommitted, Result.CommitState));
		ASSERT_THAT(IsFalse(Result.GenerationBefore.IsSet()));
		ASSERT_THAT(IsTrue(Result.GenerationAfter.IsSet()));
		ASSERT_THAT(IsTrue(Result.GenerationAfter.GetValue() == NewGeneration));
		AssertStoredPointer(FileOps, Paths.CurrentPointer,
			EAngelscriptCachePointerKind::Current, NewGeneration);
		ASSERT_THAT(IsFalse(FileOps.Files.Contains(Paths.PreviousPointer)));
		ASSERT_THAT(AreEqual(5, FileOps.Calls.Num()));
		ASSERT_THAT(IsTrue(FileOps.Calls[0].StartsWith(TEXT("Write:"))));
		ASSERT_THAT(IsTrue(FileOps.Calls[1].StartsWith(TEXT("Read:"))));
		ASSERT_THAT(IsTrue(FileOps.Calls[2].StartsWith(TEXT("Replace:"))));
		ASSERT_THAT(AreEqual(TEXT("Sync:") + Paths.NamespaceRoot, FileOps.Calls[3]));
		ASSERT_THAT(AreEqual(
			TEXT("Read:") + Paths.PendingColdStartPointer, FileOps.Calls[4]));
	}

	TEST_METHOD(CurrentPublicationRotatesOnlyAValidatedOldCurrentToPrevious)
	{
		const FAngelscriptCacheStorePaths Paths = MakePaths();
		FPointerFileOps FileOps;
		FTestLockHandle Lock;
		const FAngelscriptHash256 OldGeneration = RepeatedByteHash(0x42);
		const FAngelscriptHash256 NewGeneration = RepeatedByteHash(0x53);
		PutPointer(FileOps, Paths.CurrentPointer,
			EAngelscriptCachePointerKind::Current, OldGeneration);

		const FAngelscriptCacheStoreResult Result = PublishAngelscriptCachePointers(
			Paths,
			EAngelscriptCachePublicationDisposition::Current,
			NewGeneration,
			OldGeneration,
			MakeWriterToken(),
			[]() { return false; },
			Lock,
			FileOps);

		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreError::None, Result.Error));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreCommitState::CurrentCommitted, Result.CommitState));
		ASSERT_THAT(IsTrue(Result.GenerationBefore.GetValue() == OldGeneration));
		ASSERT_THAT(IsTrue(Result.GenerationAfter.GetValue() == NewGeneration));
		AssertStoredPointer(FileOps, Paths.PreviousPointer,
			EAngelscriptCachePointerKind::Previous, OldGeneration);
		AssertStoredPointer(FileOps, Paths.CurrentPointer,
			EAngelscriptCachePointerKind::Current, NewGeneration);
	}

	TEST_METHOD(CurrentPromotionRemovesMatchingPendingOnlyAfterCurrentCommit)
	{
		const FAngelscriptCacheStorePaths Paths = MakePaths();
		FPointerFileOps FileOps;
		FTestLockHandle Lock;
		const FAngelscriptHash256 PromotedGeneration = RepeatedByteHash(0x5f);
		PutPointer(FileOps, Paths.PendingColdStartPointer,
			EAngelscriptCachePointerKind::PendingColdStart, PromotedGeneration);

		const FAngelscriptCacheStoreResult Result = PublishAngelscriptCachePointers(
			Paths,
			EAngelscriptCachePublicationDisposition::Current,
			PromotedGeneration,
			TOptional<FAngelscriptHash256>{},
			MakeWriterToken(),
			[]() { return false; },
			Lock,
			FileOps);

		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreError::None, Result.Error));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreCommitState::CurrentCommitted, Result.CommitState));
		AssertStoredPointer(FileOps, Paths.CurrentPointer,
			EAngelscriptCachePointerKind::Current, PromotedGeneration);
		ASSERT_THAT(IsFalse(FileOps.Files.Contains(Paths.PendingColdStartPointer),
			TEXT("A Pending pointer selecting the promoted generation must be removed")));

		const int32 CurrentReplaceIndex = FileOps.Calls.IndexOfByPredicate(
			[&Paths](const FString& Call)
			{
				return Call.StartsWith(TEXT("Replace:"))
					&& Call.EndsWith(Paths.CurrentPointer);
			});
		const int32 PendingRemoveIndex = FileOps.Calls.IndexOfByKey(
			TEXT("RemovePointer:") + Paths.PendingColdStartPointer);
		ASSERT_THAT(IsTrue(CurrentReplaceIndex != INDEX_NONE));
		ASSERT_THAT(IsTrue(PendingRemoveIndex > CurrentReplaceIndex,
			TEXT("Pending removal is legal only after the Current commit point")));
		ASSERT_THAT(AreEqual(
			TEXT("Sync:") + Paths.NamespaceRoot, FileOps.Calls.Last()));
	}

	TEST_METHOD(CurrentPromotionPreservesPendingForAnotherGeneration)
	{
		const FAngelscriptCacheStorePaths Paths = MakePaths();
		FPointerFileOps FileOps;
		FTestLockHandle Lock;
		const FAngelscriptHash256 PromotedGeneration = RepeatedByteHash(0x60);
		const FAngelscriptHash256 NewerPendingGeneration = RepeatedByteHash(0x61);
		PutPointer(FileOps, Paths.PendingColdStartPointer,
			EAngelscriptCachePointerKind::PendingColdStart, NewerPendingGeneration);

		const FAngelscriptCacheStoreResult Result = PublishAngelscriptCachePointers(
			Paths,
			EAngelscriptCachePublicationDisposition::Current,
			PromotedGeneration,
			TOptional<FAngelscriptHash256>{},
			MakeWriterToken(),
			[]() { return false; },
			Lock,
			FileOps);

		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreError::None, Result.Error));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreCommitState::CurrentCommitted, Result.CommitState));
		AssertStoredPointer(FileOps, Paths.CurrentPointer,
			EAngelscriptCachePointerKind::Current, PromotedGeneration);
		AssertStoredPointer(FileOps, Paths.PendingColdStartPointer,
			EAngelscriptCachePointerKind::PendingColdStart, NewerPendingGeneration);
		ASSERT_THAT(IsFalse(FileOps.Calls.Contains(
			TEXT("RemovePointer:") + Paths.PendingColdStartPointer)));
	}

	TEST_METHOD(PendingCleanupFailureCannotDowngradeCurrentCommit)
	{
		const FAngelscriptCacheStorePaths Paths = MakePaths();
		FPointerFileOps FileOps;
		FTestLockHandle Lock;
		const FAngelscriptHash256 PromotedGeneration = RepeatedByteHash(0x62);
		PutPointer(FileOps, Paths.PendingColdStartPointer,
			EAngelscriptCachePointerKind::PendingColdStart, PromotedGeneration);
		FileOps.FailRemovePath = Paths.PendingColdStartPointer;

		const FAngelscriptCacheStoreResult Result = PublishAngelscriptCachePointers(
			Paths,
			EAngelscriptCachePublicationDisposition::Current,
			PromotedGeneration,
			TOptional<FAngelscriptHash256>{},
			MakeWriterToken(),
			[]() { return false; },
			Lock,
			FileOps);

		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreError::PointerRemoveFailed, Result.Error));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreStage::PendingPointer, Result.Stage));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStorePathCategory::PendingColdStartPointer,
			Result.PathCategory));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreCommitState::CurrentCommitted, Result.CommitState));
		ASSERT_THAT(IsTrue(Result.GenerationAfter.IsSet()));
		ASSERT_THAT(IsTrue(Result.GenerationAfter.GetValue() == PromotedGeneration));
		AssertStoredPointer(FileOps, Paths.CurrentPointer,
			EAngelscriptCachePointerKind::Current, PromotedGeneration);
		AssertStoredPointer(FileOps, Paths.PendingColdStartPointer,
			EAngelscriptCachePointerKind::PendingColdStart, PromotedGeneration);
	}

	TEST_METHOD(PendingCleanupSyncFailureCannotDowngradeCurrentCommit)
	{
		const FAngelscriptCacheStorePaths Paths = MakePaths();
		FPointerFileOps FileOps;
		FTestLockHandle Lock;
		const FAngelscriptHash256 PromotedGeneration = RepeatedByteHash(0x63);
		PutPointer(FileOps, Paths.PendingColdStartPointer,
			EAngelscriptCachePointerKind::PendingColdStart, PromotedGeneration);
		FileOps.FailSyncCall = 2;

		const FAngelscriptCacheStoreResult Result = PublishAngelscriptCachePointers(
			Paths,
			EAngelscriptCachePublicationDisposition::Current,
			PromotedGeneration,
			TOptional<FAngelscriptHash256>{},
			MakeWriterToken(),
			[]() { return false; },
			Lock,
			FileOps);

		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreError::DirectorySyncFailed, Result.Error));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreStage::PendingPointer, Result.Stage));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStorePathCategory::PendingColdStartPointer,
			Result.PathCategory));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreCommitState::CurrentCommitted, Result.CommitState));
		ASSERT_THAT(IsTrue(Result.GenerationAfter.IsSet()));
		ASSERT_THAT(IsTrue(Result.GenerationAfter.GetValue() == PromotedGeneration));
		AssertStoredPointer(FileOps, Paths.CurrentPointer,
			EAngelscriptCachePointerKind::Current, PromotedGeneration);
		ASSERT_THAT(IsFalse(FileOps.Files.Contains(Paths.PendingColdStartPointer)));
	}

	TEST_METHOD(FailedCurrentReplaceLeavesOldCurrentSelectedAndReportsNotCommitted)
	{
		const FAngelscriptCacheStorePaths Paths = MakePaths();
		FPointerFileOps FileOps;
		FTestLockHandle Lock;
		const FAngelscriptHash256 OldGeneration = RepeatedByteHash(0x64);
		const FAngelscriptHash256 NewGeneration = RepeatedByteHash(0x75);
		PutPointer(FileOps, Paths.CurrentPointer,
			EAngelscriptCachePointerKind::Current, OldGeneration);
		FileOps.FailReplacePath = Paths.CurrentPointer;

		const FAngelscriptCacheStoreResult Result = PublishAngelscriptCachePointers(
			Paths,
			EAngelscriptCachePublicationDisposition::Current,
			NewGeneration,
			OldGeneration,
			MakeWriterToken(),
			[]() { return false; },
			Lock,
			FileOps);

		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreError::AtomicReplaceFailed, Result.Error));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreCommitState::NotCommitted, Result.CommitState));
		ASSERT_THAT(IsFalse(Result.GenerationAfter.IsSet()));
		AssertStoredPointer(FileOps, Paths.CurrentPointer,
			EAngelscriptCachePointerKind::Current, OldGeneration);
		AssertStoredPointer(FileOps, Paths.PreviousPointer,
			EAngelscriptCachePointerKind::Previous, OldGeneration);
	}

	TEST_METHOD(IndeterminateCurrentReplaceRereadsAndReportsCommittedNewGeneration)
	{
		const FAngelscriptCacheStorePaths Paths = MakePaths();
		FPointerFileOps FileOps;
		FTestLockHandle Lock;
		const FAngelscriptHash256 OldGeneration = RepeatedByteHash(0x86);
		const FAngelscriptHash256 NewGeneration = RepeatedByteHash(0x97);
		PutPointer(FileOps, Paths.CurrentPointer,
			EAngelscriptCachePointerKind::Current, OldGeneration);
		FileOps.FailReplacePath = Paths.CurrentPointer;
		FileOps.bInstallBeforeReplaceFailure = true;

		const FAngelscriptCacheStoreResult Result = PublishAngelscriptCachePointers(
			Paths,
			EAngelscriptCachePublicationDisposition::Current,
			NewGeneration,
			OldGeneration,
			MakeWriterToken(),
			[]() { return false; },
			Lock,
			FileOps);

		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreError::AtomicReplaceFailed, Result.Error));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreCommitState::CurrentCommitted, Result.CommitState));
		ASSERT_THAT(IsTrue(Result.GenerationAfter.GetValue() == NewGeneration));
		AssertStoredPointer(FileOps, Paths.CurrentPointer,
			EAngelscriptCachePointerKind::Current, NewGeneration);
	}

	TEST_METHOD(SyncFailureAfterCurrentReplaceCannotDowngradeTheCommitState)
	{
		const FAngelscriptCacheStorePaths Paths = MakePaths();
		FPointerFileOps FileOps;
		FTestLockHandle Lock;
		const FAngelscriptHash256 NewGeneration = RepeatedByteHash(0xa8);
		FileOps.FailSyncCall = 1;

		const FAngelscriptCacheStoreResult Result = PublishAngelscriptCachePointers(
			Paths,
			EAngelscriptCachePublicationDisposition::Current,
			NewGeneration,
			TOptional<FAngelscriptHash256>{},
			MakeWriterToken(),
			[]() { return false; },
			Lock,
			FileOps);

		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreError::DirectorySyncFailed, Result.Error));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreCommitState::CurrentCommitted, Result.CommitState));
		ASSERT_THAT(IsTrue(Result.GenerationAfter.GetValue() == NewGeneration));
		AssertStoredPointer(FileOps, Paths.CurrentPointer,
			EAngelscriptCachePointerKind::Current, NewGeneration);
	}

	TEST_METHOD(PendingPublicationNeverChangesCurrentOrPrevious)
	{
		const FAngelscriptCacheStorePaths Paths = MakePaths();
		FPointerFileOps FileOps;
		FTestLockHandle Lock;
		const FAngelscriptHash256 CurrentGeneration = RepeatedByteHash(0xb9);
		const FAngelscriptHash256 PreviousGeneration = RepeatedByteHash(0xca);
		const FAngelscriptHash256 PendingGeneration = RepeatedByteHash(0xdb);
		PutPointer(FileOps, Paths.CurrentPointer,
			EAngelscriptCachePointerKind::Current, CurrentGeneration);
		PutPointer(FileOps, Paths.PreviousPointer,
			EAngelscriptCachePointerKind::Previous, PreviousGeneration);

		const FAngelscriptCacheStoreResult Result = PublishAngelscriptCachePointers(
			Paths,
			EAngelscriptCachePublicationDisposition::PendingColdStart,
			PendingGeneration,
			TOptional<FAngelscriptHash256>{},
			MakeWriterToken(),
			[]() { return false; },
			Lock,
			FileOps);

		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreError::None, Result.Error));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreCommitState::PendingCommitted, Result.CommitState));
		AssertStoredPointer(FileOps, Paths.CurrentPointer,
			EAngelscriptCachePointerKind::Current, CurrentGeneration);
		AssertStoredPointer(FileOps, Paths.PreviousPointer,
			EAngelscriptCachePointerKind::Previous, PreviousGeneration);
		AssertStoredPointer(FileOps, Paths.PendingColdStartPointer,
			EAngelscriptCachePointerKind::PendingColdStart, PendingGeneration);
	}

	TEST_METHOD(CancellationAfterPreviousButBeforeCurrentLeavesCurrentUnchanged)
	{
		const FAngelscriptCacheStorePaths Paths = MakePaths();
		FPointerFileOps FileOps;
		FTestLockHandle Lock;
		const FAngelscriptHash256 OldGeneration = RepeatedByteHash(0xec);
		const FAngelscriptHash256 NewGeneration = RepeatedByteHash(0xfd);
		PutPointer(FileOps, Paths.CurrentPointer,
			EAngelscriptCachePointerKind::Current, OldGeneration);

		const FAngelscriptCacheStoreResult Result = PublishAngelscriptCachePointers(
			Paths,
			EAngelscriptCachePublicationDisposition::Current,
			NewGeneration,
			OldGeneration,
			MakeWriterToken(),
			[&FileOps]()
			{
				return FileOps.Files.Contains(MakePaths().PreviousPointer);
			},
			Lock,
			FileOps);

		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreError::Cancelled, Result.Error));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreCommitState::NotCommitted, Result.CommitState));
		AssertStoredPointer(FileOps, Paths.CurrentPointer,
			EAngelscriptCachePointerKind::Current, OldGeneration);
		AssertStoredPointer(FileOps, Paths.PreviousPointer,
			EAngelscriptCachePointerKind::Previous, OldGeneration);
	}

	TEST_METHOD(CorruptReopenedPointerTempFailsBeforeAnyAtomicReplace)
	{
		const FAngelscriptCacheStorePaths Paths = MakePaths();
		FPointerFileOps FileOps;
		FTestLockHandle Lock;
		FileOps.bCorruptNextWrite = true;

		const FAngelscriptCacheStoreResult Result = PublishAngelscriptCachePointers(
			Paths,
			EAngelscriptCachePublicationDisposition::Current,
			RepeatedByteHash(0x1e),
			TOptional<FAngelscriptHash256>{},
			MakeWriterToken(),
			[]() { return false; },
			Lock,
			FileOps);

		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreError::PointerInvalid, Result.Error));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreStage::CurrentPointer, Result.Stage));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreCommitState::NotCommitted, Result.CommitState));
		ASSERT_THAT(IsFalse(FileOps.Files.Contains(Paths.CurrentPointer)));
		ASSERT_THAT(IsFalse(FileOps.Calls.ContainsByPredicate(
			[](const FString& Call) { return Call.StartsWith(TEXT("Replace:")); })));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
