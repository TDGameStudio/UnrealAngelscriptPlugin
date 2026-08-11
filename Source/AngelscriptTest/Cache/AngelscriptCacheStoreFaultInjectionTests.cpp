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

namespace AngelscriptCacheStoreFaultInjectionTests_Private
{
	class FScopedDiskRoot final
	{
	public:
		FScopedDiskRoot()
		{
			Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(
				FPaths::ProjectSavedDir(),
				TEXT("Automation/AngelscriptCacheStoreFaultInjection"),
				FGuid::NewGuid().ToString(EGuidFormats::Digits)));
			FPaths::NormalizeDirectoryName(Root);
			check(Root.Contains(
				TEXT("/Saved/Automation/AngelscriptCacheStoreFaultInjection/")));
			check(IFileManager::Get().MakeDirectory(*Root, true));
		}

		~FScopedDiskRoot()
		{
			if (Root.Contains(
				TEXT("/Saved/Automation/AngelscriptCacheStoreFaultInjection/")))
			{
				IFileManager::Get().DeleteDirectory(*Root, false, true);
			}
		}

		FString Root;
	};

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

	class FStopAtFaultPoint final : public IAngelscriptCacheStoreFaultInjector
	{
	public:
		explicit FStopAtFaultPoint(const EAngelscriptCacheStoreFaultPoint InTarget)
			: Target(InTarget)
		{
		}

		virtual bool ShouldStopAt(
			const EAngelscriptCacheStoreFaultPoint Point) override
		{
			Seen.Add(Point);
			if (!bStopped && Point == Target)
			{
				bStopped = true;
				return true;
			}
			return false;
		}

		EAngelscriptCacheStoreFaultPoint Target;
		TArray<EAngelscriptCacheStoreFaultPoint> Seen;
		bool bStopped = false;
	};

	static const TCHAR* FaultPointName(
		const EAngelscriptCacheStoreFaultPoint Point)
	{
		switch (Point)
		{
		case EAngelscriptCacheStoreFaultPoint::BeforePackTempWrite:
			return TEXT("BeforePackTempWrite");
		case EAngelscriptCacheStoreFaultPoint::AfterPackTempFlush:
			return TEXT("AfterPackTempFlush");
		case EAngelscriptCacheStoreFaultPoint::AfterPackRename:
			return TEXT("AfterPackRename");
		case EAngelscriptCacheStoreFaultPoint::AfterManifestTempFlush:
			return TEXT("AfterManifestTempFlush");
		case EAngelscriptCacheStoreFaultPoint::AfterManifestRename:
			return TEXT("AfterManifestRename");
		case EAngelscriptCacheStoreFaultPoint::AfterPointerTempsFlush:
			return TEXT("AfterPointerTempsFlush");
		case EAngelscriptCacheStoreFaultPoint::BeforePreviousReplace:
			return TEXT("BeforePreviousReplace");
		case EAngelscriptCacheStoreFaultPoint::AfterPreviousReplace:
			return TEXT("AfterPreviousReplace");
		case EAngelscriptCacheStoreFaultPoint::BeforeCurrentReplace:
			return TEXT("BeforeCurrentReplace");
		case EAngelscriptCacheStoreFaultPoint::AfterCurrentReplace:
			return TEXT("AfterCurrentReplace");
		case EAngelscriptCacheStoreFaultPoint::BeforePendingReplace:
			return TEXT("BeforePendingReplace");
		case EAngelscriptCacheStoreFaultPoint::AfterPendingReplace:
			return TEXT("AfterPendingReplace");
		default:
			return TEXT("Invalid");
		}
	}

	static FAngelscriptCacheWriterToken MakeWriterToken(const bool bRecovery)
	{
		return FAngelscriptCacheWriterToken::TryParse(bRecovery
			? TEXT("7301-fedcba98765432100123456789abcdef")
			: TEXT("7301-00112233445566778899aabbccddeeff")).GetValue();
	}

	static int32 CountTemps(const FAngelscriptCacheStorePaths& Paths)
	{
		TArray<FString> Temps;
		IFileManager::Get().FindFilesRecursive(
			Temps, *Paths.NamespaceRoot, TEXT("*.tmp.*"), true, false, false);
		return Temps.Num();
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheStoreFaultInjectionTests,
	"Angelscript.TestModule.Cache.StoreFaultInjection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	using FGenerationFixture =
		AngelscriptCacheStoreFaultInjectionTests_Private::FGenerationFixture;

	FAngelscriptCacheStoreResult Publish(
		const FAngelscriptCacheStorePaths& Paths,
		const FGenerationFixture& Fixture,
		const EAngelscriptCachePublicationDisposition Disposition,
		const TOptional<FAngelscriptHash256>& ObservedGeneration,
		const bool bRecovery,
		IAngelscriptCacheStoreFaultInjector* FaultInjector,
		IAngelscriptCacheAtomicFileOps& FileOps,
		IAngelscriptCacheNamespaceLockOps& LockOps)
	{
		using namespace AngelscriptCacheStoreFaultInjectionTests_Private;
		FAngelscriptCacheReadLimits Limits;
		FAngelscriptCacheReadBudget Budget;
		FAngelscriptUnrealZlibCacheStorageCodec Codec;
		return PublishAngelscriptCacheGeneration(
			Paths,
			Disposition,
			ObservedGeneration,
			MakeArrayView(&Fixture.Pack, 1),
			Fixture.ManifestValue,
			Fixture.Manifest,
			MakeWriterToken(bRecovery),
			Limits,
			Budget,
			FPlatformTime::Seconds() + 5.0,
			[]() { return false; },
			Codec,
			LockOps,
			FileOps,
			FaultInjector);
	}

	TOptional<FAngelscriptHash256> ReadPointer(
		const FAngelscriptCacheStorePaths& Paths,
		const EAngelscriptCachePointerKind Kind,
		IAngelscriptCacheAtomicFileOps& FileOps)
	{
		TOptional<FAngelscriptHash256> GenerationId;
		const FAngelscriptCacheStoreResult Result = ReadAngelscriptCachePointerSlot(
			Paths, Kind, FileOps, GenerationId);
		if (Result.Error != EAngelscriptCacheStoreError::None)
		{
			TestRunner->AddError(FString::Printf(
				TEXT("CacheV2 fault-state pointer read failed kind=%u error=%u stage=%u"),
				static_cast<uint32>(Kind),
				static_cast<uint32>(Result.Error),
				static_cast<uint32>(Result.Stage)));
			return {};
		}
		return GenerationId;
	}

	void AssertInjectedStop(
		const FAngelscriptCacheStoreResult& Result,
		const EAngelscriptCacheStoreCommitState ExpectedCommit,
		const AngelscriptCacheStoreFaultInjectionTests_Private::FStopAtFaultPoint& Injector)
	{
		using namespace AngelscriptCacheStoreFaultInjectionTests_Private;
		const FString Context = FString::Printf(
			TEXT("FaultPoint=%s"), FaultPointName(Injector.Target));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreError::FaultInjected,
			Result.Error,
			*Context));
		ASSERT_THAT(AreEqual(ExpectedCommit, Result.CommitState, *Context));
		ASSERT_THAT(IsTrue(Injector.bStopped, *Context));
		int32 TargetCount = 0;
		for (const EAngelscriptCacheStoreFaultPoint SeenPoint : Injector.Seen)
		{
			TargetCount += SeenPoint == Injector.Target ? 1 : 0;
		}
		ASSERT_THAT(AreEqual(1, TargetCount, *Context));
	}

	void AssertRecoveryPublishes(
		const FAngelscriptCacheStorePaths& Paths,
		const FGenerationFixture& Fixture,
		const EAngelscriptCachePublicationDisposition Disposition,
		const TOptional<FAngelscriptHash256>& ObservedGeneration,
		const EAngelscriptCachePointerKind TargetKind,
		IAngelscriptCacheAtomicFileOps& FileOps,
		IAngelscriptCacheNamespaceLockOps& LockOps)
	{
		using namespace AngelscriptCacheStoreFaultInjectionTests_Private;
		const FAngelscriptCacheStoreResult Recovery = Publish(
			Paths,
			Fixture,
			Disposition,
			ObservedGeneration,
			true,
			nullptr,
			FileOps,
			LockOps);
		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreError::None, Recovery.Error));
		const TOptional<FAngelscriptHash256> Selected =
			ReadPointer(Paths, TargetKind, FileOps);
		ASSERT_THAT(IsTrue(Selected.IsSet()));
		ASSERT_THAT(IsTrue(
			Selected.GetValue() == Fixture.Manifest.ComputedGenerationId));
		ASSERT_THAT(AreEqual(0, CountTemps(Paths),
			TEXT("Recovery must clean only recognized stale temps and finish clean")));
	}

public:
	TEST_METHOD(ImmutableFaultPointsExposeExactTempFinalStateAndRecover)
	{
		using namespace AngelscriptCacheStoreFaultInjectionTests_Private;
		const TArray<EAngelscriptCacheStoreFaultPoint> Points = {
			EAngelscriptCacheStoreFaultPoint::BeforePackTempWrite,
			EAngelscriptCacheStoreFaultPoint::AfterPackTempFlush,
			EAngelscriptCacheStoreFaultPoint::AfterPackRename,
			EAngelscriptCacheStoreFaultPoint::AfterManifestTempFlush,
			EAngelscriptCacheStoreFaultPoint::AfterManifestRename,
		};
		for (const EAngelscriptCacheStoreFaultPoint Point : Points)
		{
			FScopedDiskRoot Disk;
			const FGenerationFixture Fixture = MakeGenerationFixture(101);
			TUniquePtr<IAngelscriptCacheAtomicFileOps> FileOps =
				CreateAngelscriptCacheAtomicFileOps();
			TUniquePtr<IAngelscriptCacheNamespaceLockOps> LockOps =
				CreateAngelscriptCacheNamespaceLockOps();
			ASSERT_THAT(IsNotNull(FileOps.Get()));
			ASSERT_THAT(IsNotNull(LockOps.Get()));
			FAngelscriptCacheStorePaths Paths;
			ASSERT_THAT(IsTrue(BuildAngelscriptCacheStorePaths(
				Disk.Root / TEXT("CacheV2"),
				Fixture.Compatibility,
				Fixture.Context,
				*FileOps,
				Paths).IsSuccess()));

			FStopAtFaultPoint Injector(Point);
			const FAngelscriptCacheStoreResult Result = Publish(
				Paths,
				Fixture,
				EAngelscriptCachePublicationDisposition::Current,
				TOptional<FAngelscriptHash256>{},
				false,
				&Injector,
				*FileOps,
				*LockOps);
			AssertInjectedStop(
				Result, EAngelscriptCacheStoreCommitState::NotCommitted, Injector);

			const FString PackTemp = Paths.BuildPackTempPath(
				Fixture.Pack.PackId, MakeWriterToken(false));
			const FString PackFinal = Paths.BuildPackPath(Fixture.Pack.PackId);
			const FString ManifestTemp = Paths.BuildManifestTempPath(
				Fixture.Manifest.ComputedGenerationId, MakeWriterToken(false));
			const FString ManifestFinal = Paths.BuildManifestPath(
				Fixture.Manifest.ComputedGenerationId);
			const bool bPackTemp =
				Point == EAngelscriptCacheStoreFaultPoint::AfterPackTempFlush;
			const bool bPackFinal =
				Point >= EAngelscriptCacheStoreFaultPoint::AfterPackRename;
			const bool bManifestTemp =
				Point == EAngelscriptCacheStoreFaultPoint::AfterManifestTempFlush;
			const bool bManifestFinal =
				Point >= EAngelscriptCacheStoreFaultPoint::AfterManifestRename;
			ASSERT_THAT(AreEqual(
				bPackTemp, IFileManager::Get().FileExists(*PackTemp)));
			ASSERT_THAT(AreEqual(
				bPackFinal, IFileManager::Get().FileExists(*PackFinal)));
			ASSERT_THAT(AreEqual(
				bManifestTemp, IFileManager::Get().FileExists(*ManifestTemp)));
			ASSERT_THAT(AreEqual(
				bManifestFinal, IFileManager::Get().FileExists(*ManifestFinal)));
			ASSERT_THAT(IsFalse(ReadPointer(
				Paths, EAngelscriptCachePointerKind::Current, *FileOps).IsSet()));

			TestRunner->AddInfo(FString::Printf(
				TEXT("CacheV2Fault point=%s commit=%u packTemp=%d packFinal=%d manifestTemp=%d manifestFinal=%d"),
				FaultPointName(Point),
				static_cast<uint32>(Result.CommitState),
				bPackTemp,
				bPackFinal,
				bManifestTemp,
				bManifestFinal));
			AssertRecoveryPublishes(
				Paths,
				Fixture,
				EAngelscriptCachePublicationDisposition::Current,
				TOptional<FAngelscriptHash256>{},
				EAngelscriptCachePointerKind::Current,
				*FileOps,
				*LockOps);
		}
	}

	TEST_METHOD(CurrentPointerFaultPointsPreserveOldOrNewSelectionAndRecover)
	{
		using namespace AngelscriptCacheStoreFaultInjectionTests_Private;
		const TArray<EAngelscriptCacheStoreFaultPoint> Points = {
			EAngelscriptCacheStoreFaultPoint::AfterPointerTempsFlush,
			EAngelscriptCacheStoreFaultPoint::BeforePreviousReplace,
			EAngelscriptCacheStoreFaultPoint::AfterPreviousReplace,
			EAngelscriptCacheStoreFaultPoint::BeforeCurrentReplace,
			EAngelscriptCacheStoreFaultPoint::AfterCurrentReplace,
		};
		for (const EAngelscriptCacheStoreFaultPoint Point : Points)
		{
			FScopedDiskRoot Disk;
			const FGenerationFixture Old = MakeGenerationFixture(201);
			const FGenerationFixture New = MakeGenerationFixture(202);
			TUniquePtr<IAngelscriptCacheAtomicFileOps> FileOps =
				CreateAngelscriptCacheAtomicFileOps();
			TUniquePtr<IAngelscriptCacheNamespaceLockOps> LockOps =
				CreateAngelscriptCacheNamespaceLockOps();
			ASSERT_THAT(IsNotNull(FileOps.Get()));
			ASSERT_THAT(IsNotNull(LockOps.Get()));
			FAngelscriptCacheStorePaths Paths;
			ASSERT_THAT(IsTrue(BuildAngelscriptCacheStorePaths(
				Disk.Root / TEXT("CacheV2"),
				Old.Compatibility,
				Old.Context,
				*FileOps,
				Paths).IsSuccess()));

			ASSERT_THAT(IsTrue(Publish(
				Paths,
				Old,
				EAngelscriptCachePublicationDisposition::Current,
				TOptional<FAngelscriptHash256>{},
				true,
				nullptr,
				*FileOps,
				*LockOps).IsSuccess()));
			FStopAtFaultPoint Injector(Point);
			const FAngelscriptCacheStoreResult Result = Publish(
				Paths,
				New,
				EAngelscriptCachePublicationDisposition::Current,
				Old.Manifest.ComputedGenerationId,
				false,
				&Injector,
				*FileOps,
				*LockOps);
			const bool bCurrentCommitted =
				Point == EAngelscriptCacheStoreFaultPoint::AfterCurrentReplace;
			AssertInjectedStop(
				Result,
				bCurrentCommitted
					? EAngelscriptCacheStoreCommitState::CurrentCommitted
					: EAngelscriptCacheStoreCommitState::NotCommitted,
				Injector);

			const TOptional<FAngelscriptHash256> Current = ReadPointer(
				Paths, EAngelscriptCachePointerKind::Current, *FileOps);
			ASSERT_THAT(IsTrue(Current.IsSet()));
			ASSERT_THAT(IsTrue(Current.GetValue() == (bCurrentCommitted
				? New.Manifest.ComputedGenerationId
				: Old.Manifest.ComputedGenerationId)));
			const bool bPreviousInstalled =
				Point == EAngelscriptCacheStoreFaultPoint::AfterPreviousReplace
				|| Point == EAngelscriptCacheStoreFaultPoint::BeforeCurrentReplace
				|| Point == EAngelscriptCacheStoreFaultPoint::AfterCurrentReplace;
			const TOptional<FAngelscriptHash256> Previous = ReadPointer(
				Paths, EAngelscriptCachePointerKind::Previous, *FileOps);
			ASSERT_THAT(AreEqual(bPreviousInstalled, Previous.IsSet()));
			if (Previous.IsSet())
			{
				ASSERT_THAT(IsTrue(
					Previous.GetValue() == Old.Manifest.ComputedGenerationId));
			}
			const int32 ExpectedTemps = bCurrentCommitted
				? 0
				: (bPreviousInstalled ? 1 : 2);
			ASSERT_THAT(AreEqual(ExpectedTemps, CountTemps(Paths)));
			ASSERT_THAT(IsTrue(IFileManager::Get().FileExists(
				*Paths.BuildPackPath(New.Pack.PackId))));
			ASSERT_THAT(IsTrue(IFileManager::Get().FileExists(
				*Paths.BuildManifestPath(New.Manifest.ComputedGenerationId))));

			TestRunner->AddInfo(FString::Printf(
				TEXT("CacheV2Fault point=%s commit=%u current=%s previous=%s temps=%d"),
				FaultPointName(Point),
				static_cast<uint32>(Result.CommitState),
				*Current->ToHexString(),
				Previous.IsSet() ? *Previous->ToHexString() : TEXT("absent"),
				ExpectedTemps));
			AssertRecoveryPublishes(
				Paths,
				New,
				EAngelscriptCachePublicationDisposition::Current,
				Old.Manifest.ComputedGenerationId,
				EAngelscriptCachePointerKind::Current,
				*FileOps,
				*LockOps);
		}
	}

	TEST_METHOD(PendingFaultPointsNeverChangeActivePointersAndRecover)
	{
		using namespace AngelscriptCacheStoreFaultInjectionTests_Private;
		const TArray<EAngelscriptCacheStoreFaultPoint> Points = {
			EAngelscriptCacheStoreFaultPoint::BeforePendingReplace,
			EAngelscriptCacheStoreFaultPoint::AfterPendingReplace,
		};
		for (const EAngelscriptCacheStoreFaultPoint Point : Points)
		{
			FScopedDiskRoot Disk;
			const FGenerationFixture Fixture = MakeGenerationFixture(301);
			TUniquePtr<IAngelscriptCacheAtomicFileOps> FileOps =
				CreateAngelscriptCacheAtomicFileOps();
			TUniquePtr<IAngelscriptCacheNamespaceLockOps> LockOps =
				CreateAngelscriptCacheNamespaceLockOps();
			ASSERT_THAT(IsNotNull(FileOps.Get()));
			ASSERT_THAT(IsNotNull(LockOps.Get()));
			FAngelscriptCacheStorePaths Paths;
			ASSERT_THAT(IsTrue(BuildAngelscriptCacheStorePaths(
				Disk.Root / TEXT("CacheV2"),
				Fixture.Compatibility,
				Fixture.Context,
				*FileOps,
				Paths).IsSuccess()));

			FStopAtFaultPoint Injector(Point);
			const FAngelscriptCacheStoreResult Result = Publish(
				Paths,
				Fixture,
				EAngelscriptCachePublicationDisposition::PendingColdStart,
				TOptional<FAngelscriptHash256>{},
				false,
				&Injector,
				*FileOps,
				*LockOps);
			const bool bPendingCommitted =
				Point == EAngelscriptCacheStoreFaultPoint::AfterPendingReplace;
			AssertInjectedStop(
				Result,
				bPendingCommitted
					? EAngelscriptCacheStoreCommitState::PendingCommitted
					: EAngelscriptCacheStoreCommitState::NotCommitted,
				Injector);
			ASSERT_THAT(IsFalse(ReadPointer(
				Paths, EAngelscriptCachePointerKind::Current, *FileOps).IsSet()));
			ASSERT_THAT(IsFalse(ReadPointer(
				Paths, EAngelscriptCachePointerKind::Previous, *FileOps).IsSet()));
			const TOptional<FAngelscriptHash256> Pending = ReadPointer(
				Paths, EAngelscriptCachePointerKind::PendingColdStart, *FileOps);
			ASSERT_THAT(AreEqual(bPendingCommitted, Pending.IsSet()));
			ASSERT_THAT(AreEqual(bPendingCommitted ? 0 : 1, CountTemps(Paths)));

			TestRunner->AddInfo(FString::Printf(
				TEXT("CacheV2Fault point=%s commit=%u pending=%s temps=%d"),
				FaultPointName(Point),
				static_cast<uint32>(Result.CommitState),
				Pending.IsSet() ? *Pending->ToHexString() : TEXT("absent"),
				CountTemps(Paths)));
			AssertRecoveryPublishes(
				Paths,
				Fixture,
				EAngelscriptCachePublicationDisposition::PendingColdStart,
				TOptional<FAngelscriptHash256>{},
				EAngelscriptCachePointerKind::PendingColdStart,
				*FileOps,
				*LockOps);
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
