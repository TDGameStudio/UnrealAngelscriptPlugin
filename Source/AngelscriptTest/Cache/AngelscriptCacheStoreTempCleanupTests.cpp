#include "CQTest.h"

#include "Cache/AngelscriptCacheStore.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheStoreTempCleanupTests,
	"Angelscript.TestModule.Cache.StoreTempCleanup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	class FHeldNamespaceLock final : public IAngelscriptCacheNamespaceLockHandle
	{
	};

	class FMemoryFileOps final : public IAngelscriptCacheAtomicFileOps
	{
	public:
		virtual FAngelscriptCacheStoreResult CanonicalizeAndValidateRoot(
			const FString& RequestedRoot,
			FAngelscriptCanonicalCacheRoot& OutRoot) override
		{
			OutRoot.AbsolutePath = RequestedRoot;
			OutRoot.IdentityPath = RequestedRoot.ToLower();
			return FAngelscriptCacheStoreResult::Success();
		}

		virtual FAngelscriptCacheStoreResult EnsureDirectoryTree(
			const FString& DirectoryPath) override
		{
			return FAngelscriptCacheStoreResult::Success();
		}

		virtual FAngelscriptCacheStoreResult WriteFlushClose(
			const FString& TempPath,
			TConstArrayView<uint8> Bytes) override
		{
			return Unsupported();
		}

		virtual FAngelscriptCacheStoreResult ReopenReadAll(
			const FString& Path,
			uint64 MaxBytes,
			TArray<uint8>& OutBytes) override
		{
			OutBytes.Reset();
			return Unsupported();
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
			return Unsupported();
		}

		virtual FAngelscriptCacheStoreResult AtomicRemovePointer(
			const FString& PointerPath) override
		{
			return Unsupported();
		}

		virtual FAngelscriptCacheStoreResult RemoveOwnTemp(
			const FString& TempPath) override
		{
			RemovedPaths.Add(TempPath);
			Files.RemoveAll([&](const FString& ExistingPath)
			{
				return ExistingPath.Equals(TempPath, ESearchCase::CaseSensitive);
			});
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
			return FAngelscriptCacheStoreResult::Success();
		}

		virtual bool SupportsSharedAtomicCacheStore() const override
		{
			return true;
		}

		virtual FAngelscriptCacheStoreResult EnumerateDirectFileNames(
			const FString& DirectoryPath,
			TArray<FString>& OutFileNames) override
		{
			EnumerationCalls.Add(DirectoryPath);
			OutFileNames.Reset();
			if (FailEnumerationDirectory.IsSet()
				&& FailEnumerationDirectory.GetValue() == DirectoryPath)
			{
				return FAngelscriptCacheStoreResult::Failure(
					EAngelscriptCacheStoreError::ReadFailed,
					EAngelscriptCacheStoreStage::None);
			}
			for (const FString& File : Files)
			{
				if (FPaths::GetPath(File).Equals(
					DirectoryPath, ESearchCase::CaseSensitive))
				{
					OutFileNames.Add(FPaths::GetCleanFilename(File));
				}
			}
			return FAngelscriptCacheStoreResult::Success();
		}

		TArray<FString> Files;
		TArray<FString> EnumerationCalls;
		TArray<FString> RemovedPaths;
		TOptional<FString> FailEnumerationDirectory;

	private:
		static FAngelscriptCacheStoreResult Unsupported()
		{
			return FAngelscriptCacheStoreResult::Failure(
				EAngelscriptCacheStoreError::UnsupportedPlatformAtomicity,
				EAngelscriptCacheStoreStage::None);
		}
	};

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

	static void AddFile(FMemoryFileOps& FileOps, const FString& Path)
	{
		FileOps.Files.Add(Path);
	}

	static bool ContainsFile(const FMemoryFileOps& FileOps, const FString& Path)
	{
		return FileOps.Files.ContainsByPredicate([&](const FString& ExistingPath)
		{
			return ExistingPath.Equals(Path, ESearchCase::CaseSensitive);
		});
	}

	void AssertContainsExactly(
		const TArray<FString>& Actual,
		const TArray<FString>& Expected)
	{
		ASSERT_THAT(AreEqual(Expected.Num(), Actual.Num()));
		for (const FString& ExpectedValue : Expected)
		{
			ASSERT_THAT(IsTrue(Actual.Contains(ExpectedValue), ExpectedValue));
		}
	}

public:
	TEST_METHOD(RemovesOnlyTheFiveStrictDirectTempPatternsUnderTheHeldLock)
	{
		const FAngelscriptCacheStorePaths Paths = MakePaths();
		const FString Hash = FString::ChrN(64, TEXT('a'));
		const FString Token = TEXT("41-0123456789abcdef0123456789abcdef");
		const FString PackTemp = Paths.PacksDirectory /
			(Hash + TEXT(".aspack.tmp.") + Token);
		const FString ManifestTemp = Paths.GenerationsDirectory /
			(Hash + TEXT(".asmanifest.tmp.") + Token);
		const TArray<FString> ExpectedRemoved = {
			PackTemp,
			ManifestTemp,
			Paths.NamespaceRoot / (TEXT("Current.ascurrent.tmp.") + Token),
			Paths.NamespaceRoot / (TEXT("Previous.ascurrent.tmp.") + Token),
			Paths.NamespaceRoot / (TEXT("PendingColdStart.ascurrent.tmp.") + Token),
		};

		FMemoryFileOps FileOps;
		for (const FString& Path : ExpectedRemoved)
		{
			AddFile(FileOps, Path);
		}
		const TArray<FString> ExpectedPreserved = {
			Paths.PacksDirectory / (FString::ChrN(64, TEXT('A'))
				+ TEXT(".aspack.tmp.") + Token),
			Paths.PacksDirectory / (TEXT("abcd.aspack.tmp.") + Token),
			Paths.PacksDirectory / (Hash + TEXT(".aspack.tmp.041-")
				+ TEXT("0123456789abcdef0123456789abcdef")),
			Paths.PacksDirectory / (Hash + TEXT(".aspack.tmp.41-")
				+ TEXT("0123456789ABCDEF0123456789ABCDEF")),
			Paths.PacksDirectory / (Hash + TEXT(".aspack")),
			Paths.NamespaceRoot / (Hash + TEXT(".aspack.tmp.") + Token),
			Paths.PacksDirectory / (TEXT("Current.ascurrent.tmp.") + Token),
			Paths.PacksDirectory / TEXT("Nested") /
				(Hash + TEXT(".aspack.tmp.") + Token),
			Paths.CurrentPointer,
		};
		for (const FString& Path : ExpectedPreserved)
		{
			AddFile(FileOps, Path);
		}
		FHeldNamespaceLock Lock;

		const FAngelscriptCacheStoreResult Result =
			CleanupAngelscriptCacheStaleTempsUnderLock(Paths, Lock, FileOps);

		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreError::None, Result.Error));
		AssertContainsExactly(FileOps.EnumerationCalls, {
			Paths.NamespaceRoot, Paths.PacksDirectory, Paths.GenerationsDirectory});
		AssertContainsExactly(FileOps.RemovedPaths, ExpectedRemoved);
		for (const FString& Path : ExpectedRemoved)
		{
			ASSERT_THAT(IsFalse(ContainsFile(FileOps, Path), Path));
		}
		for (const FString& Path : ExpectedPreserved)
		{
			ASSERT_THAT(IsTrue(ContainsFile(FileOps, Path), Path));
		}
	}

	TEST_METHOD(EnumerationFailureReturnsTempCleanupBeforeDeletingAnything)
	{
		const FAngelscriptCacheStorePaths Paths = MakePaths();
		const FString Hash = FString::ChrN(64, TEXT('b'));
		const FString Token = TEXT("52-fedcba9876543210fedcba9876543210");
		const FString PointerTemp = Paths.NamespaceRoot /
			(TEXT("Current.ascurrent.tmp.") + Token);
		const FString PackTemp = Paths.PacksDirectory /
			(Hash + TEXT(".aspack.tmp.") + Token);
		FMemoryFileOps FileOps;
		AddFile(FileOps, PointerTemp);
		AddFile(FileOps, PackTemp);
		FileOps.FailEnumerationDirectory = Paths.PacksDirectory;
		FHeldNamespaceLock Lock;

		const FAngelscriptCacheStoreResult Result =
			CleanupAngelscriptCacheStaleTempsUnderLock(Paths, Lock, FileOps);

		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreError::ReadFailed, Result.Error));
		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreStage::TempCleanup, Result.Stage));
		ASSERT_THAT(AreEqual(0, FileOps.RemovedPaths.Num()));
		ASSERT_THAT(IsTrue(ContainsFile(FileOps, PointerTemp)));
		ASSERT_THAT(IsTrue(ContainsFile(FileOps, PackTemp)));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
