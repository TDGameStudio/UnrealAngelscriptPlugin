#include "Cache/AngelscriptCacheStore.h"
#include "Cache/AngelscriptCacheArchive.h"
#include "Cache/AngelscriptCacheManifestPack.h"
#include "Cache/AngelscriptCacheSemanticRecords.h"

#include "CQTest.h"
#include "Async/Async.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptCacheStoreConcurrencyTests_Private
{
	class FScopedDiskRoot final
	{
	public:
		FScopedDiskRoot()
		{
			Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(
				FPaths::ProjectSavedDir(),
				TEXT("Automation/AngelscriptCacheStoreConcurrency"),
				FGuid::NewGuid().ToString(EGuidFormats::Digits)));
			FPaths::NormalizeDirectoryName(Root);
			check(Root.Contains(
				TEXT("/Saved/Automation/AngelscriptCacheStoreConcurrency/")));
			check(IFileManager::Get().MakeDirectory(*Root, true));
		}

		~FScopedDiskRoot()
		{
			if (Root.Contains(
				TEXT("/Saved/Automation/AngelscriptCacheStoreConcurrency/")))
			{
				IFileManager::Get().DeleteDirectory(*Root, false, true);
			}
		}

		FString Root;
	};

	class FPoolEvent final
	{
	public:
		explicit FPoolEvent(const bool bManualReset = false)
			: Event(FPlatformProcess::GetSynchEventFromPool(bManualReset))
		{
		}

		~FPoolEvent()
		{
			if (Event != nullptr)
			{
				FPlatformProcess::ReturnSynchEventToPool(Event);
			}
		}

		FEvent* Get() const
		{
			return Event;
		}

	private:
		FEvent* Event = nullptr;
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

	static FAngelscriptCacheWriterToken MakeWriterToken(const int32 Ordinal)
	{
		switch (Ordinal)
		{
		case 1:
			return FAngelscriptCacheWriterToken::TryParse(
				TEXT("7401-00112233445566778899aabbccddeeff")).GetValue();
		case 2:
			return FAngelscriptCacheWriterToken::TryParse(
				TEXT("7402-fedcba98765432100123456789abcdef")).GetValue();
		default:
			return FAngelscriptCacheWriterToken::TryParse(
				TEXT("7403-11223344556677889900aabbccddeeff")).GetValue();
		}
	}

	static FAngelscriptCacheStoreResult Publish(
		const FAngelscriptCacheStorePaths& Paths,
		const FGenerationFixture& Fixture,
		const TOptional<FAngelscriptHash256>& ObservedGeneration,
		const int32 WriterOrdinal,
		IAngelscriptCacheStoreFaultInjector* FaultInjector = nullptr)
	{
		TUniquePtr<IAngelscriptCacheAtomicFileOps> FileOps =
			CreateAngelscriptCacheAtomicFileOps();
		TUniquePtr<IAngelscriptCacheNamespaceLockOps> LockOps =
			CreateAngelscriptCacheNamespaceLockOps();
		if (!FileOps.IsValid() || !LockOps.IsValid())
		{
			return FAngelscriptCacheStoreResult::Failure(
				EAngelscriptCacheStoreError::UnsupportedPlatformAtomicity,
				EAngelscriptCacheStoreStage::RootValidation);
		}
		FAngelscriptCacheReadLimits Limits;
		FAngelscriptCacheReadBudget Budget;
		FAngelscriptUnrealZlibCacheStorageCodec Codec;
		return PublishAngelscriptCacheGeneration(
			Paths,
			EAngelscriptCachePublicationDisposition::Current,
			ObservedGeneration,
			MakeArrayView(&Fixture.Pack, 1),
			Fixture.ManifestValue,
			Fixture.Manifest,
			MakeWriterToken(WriterOrdinal),
			Limits,
			Budget,
			FPlatformTime::Seconds() + 5.0,
			[]() { return false; },
			Codec,
			*LockOps,
			*FileOps,
			FaultInjector);
	}

	class FBlockAtPointerTemps final : public IAngelscriptCacheStoreFaultInjector
	{
	public:
		FBlockAtPointerTemps(FEvent& InReached, FEvent& InRelease)
			: Reached(InReached)
			, Release(InRelease)
		{
		}

		virtual bool ShouldStopAt(
			const EAngelscriptCacheStoreFaultPoint Point) override
		{
			if (Point == EAngelscriptCacheStoreFaultPoint::AfterPointerTempsFlush)
			{
				bReached = true;
				Reached.Trigger();
				bReleased = Release.Wait(5000);
			}
			return false;
		}

		FEvent& Reached;
		FEvent& Release;
		TAtomic<bool> bReached{false};
		TAtomic<bool> bReleased{false};
	};

	static int32 CountTemps(const FAngelscriptCacheStorePaths& Paths)
	{
		TArray<FString> Temps;
		IFileManager::Get().FindFilesRecursive(
			Temps, *Paths.NamespaceRoot, TEXT("*.tmp.*"), true, false, false);
		return Temps.Num();
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheStoreConcurrencyTests,
	"Angelscript.TestModule.Cache.StoreConcurrency",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(TwoProductionPublishersSerializeToOneCommitAndOneValidatedNoOp)
	{
		using namespace AngelscriptCacheStoreConcurrencyTests_Private;
		FScopedDiskRoot Disk;
		const FGenerationFixture Fixture = MakeGenerationFixture(401);
		TUniquePtr<IAngelscriptCacheAtomicFileOps> PathFileOps =
			CreateAngelscriptCacheAtomicFileOps();
		ASSERT_THAT(IsNotNull(PathFileOps.Get()));
		FAngelscriptCacheStorePaths Paths;
		ASSERT_THAT(IsTrue(BuildAngelscriptCacheStorePaths(
			Disk.Root / TEXT("CacheV2"),
			Fixture.Compatibility,
			Fixture.Context,
			*PathFileOps,
			Paths).IsSuccess()));

		FPoolEvent FirstReady;
		FPoolEvent SecondReady;
		FPoolEvent Start(true);
		ASSERT_THAT(IsNotNull(FirstReady.Get()));
		ASSERT_THAT(IsNotNull(SecondReady.Get()));
		ASSERT_THAT(IsNotNull(Start.Get()));
		auto LaunchPublisher = [&](FEvent& Ready, const int32 WriterOrdinal)
		{
			return Async(EAsyncExecution::Thread,
				[&Paths, &Fixture, &Ready, &Start, WriterOrdinal]()
				{
					Ready.Trigger();
					if (!Start.Get()->Wait(5000))
					{
						return FAngelscriptCacheStoreResult::Failure(
							EAngelscriptCacheStoreError::LockTimeout,
							EAngelscriptCacheStoreStage::LockAcquisition);
					}
					return Publish(
						Paths,
						Fixture,
						TOptional<FAngelscriptHash256>{},
						WriterOrdinal);
				});
		};
		TFuture<FAngelscriptCacheStoreResult> First =
			LaunchPublisher(*FirstReady.Get(), 1);
		TFuture<FAngelscriptCacheStoreResult> Second =
			LaunchPublisher(*SecondReady.Get(), 2);
		ASSERT_THAT(IsTrue(FirstReady.Get()->Wait(5000)));
		ASSERT_THAT(IsTrue(SecondReady.Get()->Wait(5000)));
		Start.Get()->Trigger();
		const FAngelscriptCacheStoreResult FirstResult = First.Get();
		const FAngelscriptCacheStoreResult SecondResult = Second.Get();

		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreError::None, FirstResult.Error));
		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreError::None, SecondResult.Error));
		const int32 CommitCount =
			(FirstResult.CommitState
				== EAngelscriptCacheStoreCommitState::CurrentCommitted ? 1 : 0)
			+ (SecondResult.CommitState
				== EAngelscriptCacheStoreCommitState::CurrentCommitted ? 1 : 0);
		const int32 NoOpCount =
			(FirstResult.CommitState
				== EAngelscriptCacheStoreCommitState::NotCommitted ? 1 : 0)
			+ (SecondResult.CommitState
				== EAngelscriptCacheStoreCommitState::NotCommitted ? 1 : 0);
		ASSERT_THAT(AreEqual(1, CommitCount));
		ASSERT_THAT(AreEqual(1, NoOpCount));

		TOptional<FAngelscriptHash256> Current;
		TOptional<FAngelscriptHash256> Previous;
		ASSERT_THAT(IsTrue(ReadAngelscriptCachePointerSlot(
			Paths,
			EAngelscriptCachePointerKind::Current,
			*PathFileOps,
			Current).IsSuccess()));
		ASSERT_THAT(IsTrue(ReadAngelscriptCachePointerSlot(
			Paths,
			EAngelscriptCachePointerKind::Previous,
			*PathFileOps,
			Previous).IsSuccess()));
		ASSERT_THAT(IsTrue(Current.IsSet()));
		ASSERT_THAT(IsTrue(
			Current.GetValue() == Fixture.Manifest.ComputedGenerationId));
		ASSERT_THAT(IsFalse(Previous.IsSet(),
			TEXT("A validated no-op must not rotate the first Current")));
		ASSERT_THAT(AreEqual(0, CountTemps(Paths)));
		TestRunner->AddInfo(FString::Printf(
			TEXT("CacheV2ConcurrentPublisher firstCommit=%u secondCommit=%u current=%s temps=%d"),
			static_cast<uint32>(FirstResult.CommitState),
			static_cast<uint32>(SecondResult.CommitState),
			*Current->ToHexString(),
			CountTemps(Paths)));
	}

	TEST_METHOD(PinnedOldSessionRemainsStableWhileWriterMovesCurrent)
	{
		using namespace AngelscriptCacheStoreConcurrencyTests_Private;
		FScopedDiskRoot Disk;
		const FGenerationFixture Old = MakeGenerationFixture(501);
		const FGenerationFixture New = MakeGenerationFixture(502);
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
		const FAngelscriptCacheStoreResult OldPublish = Publish(
			Paths, Old, TOptional<FAngelscriptHash256>{}, 3);
		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreError::None, OldPublish.Error));

		FAngelscriptCacheReadSelection OldSelection;
		OldSelection.Compatibility = Old.Compatibility;
		OldSelection.Context = Old.Context;
		OldSelection.Profile = Old.ManifestValue.Profile;
		OldSelection.SourceSnapshot = Old.ManifestValue.SourceSnapshot;
		FAngelscriptCacheReadLimits Limits;
		FAngelscriptUnrealZlibCacheStorageCodec Codec;
		TUniquePtr<FAngelscriptCacheReadSession> OldSession;
		const FAngelscriptCacheStoreResult OpenOld =
			OpenBestAngelscriptCacheReadSession(
				Paths,
				OldSelection,
				Limits,
				FPlatformTime::Seconds() + 5.0,
				[]() { return false; },
				Codec,
				*LockOps,
				*FileOps,
				OldSession);
		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreError::None, OpenOld.Error));
		ASSERT_THAT(IsNotNull(OldSession.Get()));
		ASSERT_THAT(IsTrue(
			OldSession->GetGenerationId() == Old.Manifest.ComputedGenerationId));

		FPoolEvent WriterReached;
		FPoolEvent ReleaseWriter;
		ASSERT_THAT(IsNotNull(WriterReached.Get()));
		ASSERT_THAT(IsNotNull(ReleaseWriter.Get()));
		FBlockAtPointerTemps Gate(*WriterReached.Get(), *ReleaseWriter.Get());
		TFuture<FAngelscriptCacheStoreResult> Writer = Async(
			EAsyncExecution::Thread,
			[&Paths, &Old, &New, &Gate]()
			{
				return Publish(
					Paths,
					New,
					Old.Manifest.ComputedGenerationId,
					1,
					&Gate);
			});
		ASSERT_THAT(IsTrue(WriterReached.Get()->Wait(5000),
			TEXT("Writer must reach the held-lock pointer-temp boundary")));
		ASSERT_THAT(IsTrue(Gate.bReached.Load()));

		// The writer currently owns the namespace lock and has prepared the new
		// immutable generation. This session performs no path reopen or lock wait.
		ASSERT_THAT(IsTrue(
			OldSession->GetGenerationId() == Old.Manifest.ComputedGenerationId));
		ASSERT_THAT(IsTrue(
			OldSession->GetGeneration().Manifest.SourceSnapshot
				== Old.ManifestValue.SourceSnapshot));
		ASSERT_THAT(AreEqual(
			Old.ManifestValue.Records.Num(),
			OldSession->GetGeneration().Manifest.Records.Num()));
		ReleaseWriter.Get()->Trigger();
		const FAngelscriptCacheStoreResult WriterResult = Writer.Get();
		ASSERT_THAT(IsTrue(Gate.bReleased.Load()));
		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreError::None, WriterResult.Error));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreCommitState::CurrentCommitted,
			WriterResult.CommitState));

		// Current has moved, but the already-open session still exposes only Old.
		ASSERT_THAT(IsTrue(
			OldSession->GetGenerationId() == Old.Manifest.ComputedGenerationId));
		ASSERT_THAT(IsTrue(
			OldSession->GetGeneration().Manifest.SourceSnapshot
				== Old.ManifestValue.SourceSnapshot));

		FAngelscriptCacheReadSelection NewSelection;
		NewSelection.Compatibility = New.Compatibility;
		NewSelection.Context = New.Context;
		NewSelection.Profile = New.ManifestValue.Profile;
		NewSelection.SourceSnapshot = New.ManifestValue.SourceSnapshot;
		TUniquePtr<FAngelscriptCacheReadSession> NewSession;
		const FAngelscriptCacheStoreResult OpenNew =
			OpenBestAngelscriptCacheReadSession(
				Paths,
				NewSelection,
				Limits,
				FPlatformTime::Seconds() + 5.0,
				[]() { return false; },
				Codec,
				*LockOps,
				*FileOps,
				NewSession);
		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreError::None, OpenNew.Error));
		ASSERT_THAT(IsNotNull(NewSession.Get()));
		ASSERT_THAT(IsTrue(
			NewSession->GetGenerationId() == New.Manifest.ComputedGenerationId));
		ASSERT_THAT(AreEqual(0, CountTemps(Paths)));
		TestRunner->AddInfo(FString::Printf(
			TEXT("CacheV2ConcurrentReadWrite oldPinned=%s newCurrent=%s writerCommit=%u temps=%d"),
			*OldSession->GetGenerationId().ToHexString(),
			*NewSession->GetGenerationId().ToHexString(),
			static_cast<uint32>(WriterResult.CommitState),
			CountTemps(Paths)));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
