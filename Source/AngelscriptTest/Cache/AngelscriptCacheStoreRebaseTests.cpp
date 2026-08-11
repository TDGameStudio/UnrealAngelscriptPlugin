#include "Cache/AngelscriptCacheStore.h"

#include "Cache/AngelscriptCacheManifestPack.h"
#include "CQTest.h"
#include "Hash/Blake3.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheStoreRebaseTests,
	"Angelscript.TestModule.Cache.StoreRebase",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static FAngelscriptHash256 RepeatedByteHash(const uint8 Byte)
	{
		FBlake3Hash::ByteArray Bytes{};
		FMemory::Memset(Bytes, Byte, sizeof(Bytes));
		return FAngelscriptHash256{FBlake3Hash(Bytes)};
	}

	static FAngelscriptCacheRecordId RecordId(
		const EAngelscriptCacheRecordKind Kind,
		const uint8 Byte)
	{
		return {Kind, RepeatedByteHash(Byte)};
	}

	static FAngelscriptCacheGenerationManifest MakeManifest(
		const uint8 PhysicalLocationByte = 0x70)
	{
		FAngelscriptCacheGenerationManifest Manifest;
		Manifest.ManifestSchemaVersion =
			FAngelscriptCacheManifestPackArchive::ManifestSchemaVersion;
		Manifest.Compatibility.Hash = RepeatedByteHash(0x10);
		Manifest.Context.Hash = RepeatedByteHash(0x20);
		Manifest.Profile.Hash = RepeatedByteHash(0x30);
		Manifest.SourceSnapshot = RepeatedByteHash(0x40);
		Manifest.SourceIndexRecordId =
			RecordId(EAngelscriptCacheRecordKind::SourceIndex, 0x41);
		FAngelscriptCacheModuleSnapshotLink& Module =
			Manifest.ModuleSnapshots.AddDefaulted_GetRef();
		Module.ModuleKey.Hash = RepeatedByteHash(0x50);
		Module.RecordId = RecordId(
			EAngelscriptCacheRecordKind::ModuleSnapshot, 0x51);

		FAngelscriptCacheRecordIndexEntry& SourceEntry =
			Manifest.Records.AddDefaulted_GetRef();
		SourceEntry.RecordId = Manifest.SourceIndexRecordId;
		SourceEntry.Location.PackId = RepeatedByteHash(PhysicalLocationByte);
		SourceEntry.Location.PackOffset = 10;
		FAngelscriptCacheRecordIndexEntry& ModuleEntry =
			Manifest.Records.AddDefaulted_GetRef();
		ModuleEntry.RecordId = Module.RecordId;
		ModuleEntry.Location.PackId = RepeatedByteHash(PhysicalLocationByte);
		ModuleEntry.Location.PackOffset = 20;
		return Manifest;
	}

	void AssertRebaseFailure(
		const FAngelscriptCacheStoreResult& Result,
		const EAngelscriptCacheStoreError ExpectedError,
		const FAngelscriptHash256& ActualGeneration)
	{
		ASSERT_THAT(AreEqual(ExpectedError, Result.Error));
		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreStage::Rebase, Result.Stage));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreCommitState::NotCommitted, Result.CommitState));
		ASSERT_THAT(IsTrue(Result.GenerationBefore.IsSet()));
		ASSERT_THAT(IsTrue(Result.GenerationBefore.GetValue() == ActualGeneration));
		ASSERT_THAT(IsFalse(Result.GenerationAfter.IsSet()));
	}

public:
	TEST_METHOD(UnchangedObservedBaseProceedsWithoutReclassifyingTheManifest)
	{
		const FAngelscriptHash256 BaseGeneration = RepeatedByteHash(0x60);
		const FAngelscriptCacheGenerationManifest Prepared = MakeManifest();
		FAngelscriptCacheRebasePlan Plan;

		const FAngelscriptCacheStoreResult Result = EvaluateAngelscriptCacheRebase(
			BaseGeneration,
			BaseGeneration,
			&Prepared,
			Prepared,
			Plan);

		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreError::None, Result.Error));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheRebaseAction::ProceedFromObservedBase, Plan.Action));
		ASSERT_THAT(IsTrue(Plan.SelectedGenerationId.IsSet()));
		ASSERT_THAT(IsTrue(Plan.SelectedGenerationId.GetValue() == BaseGeneration));
	}

	TEST_METHOD(SameSemanticGenerationWithDifferentPackLocationsIsAlreadySelected)
	{
		const FAngelscriptHash256 ObservedGeneration = RepeatedByteHash(0x61);
		const FAngelscriptHash256 ConcurrentGeneration = RepeatedByteHash(0x62);
		const FAngelscriptCacheGenerationManifest Prepared = MakeManifest(0x70);
		const FAngelscriptCacheGenerationManifest Concurrent = MakeManifest(0x71);
		FAngelscriptCacheRebasePlan Plan;

		const FAngelscriptCacheStoreResult Result = EvaluateAngelscriptCacheRebase(
			ObservedGeneration,
			ConcurrentGeneration,
			&Concurrent,
			Prepared,
			Plan);

		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreError::None, Result.Error));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheRebaseAction::AlreadySelected, Plan.Action));
		ASSERT_THAT(IsTrue(
			Plan.SelectedGenerationId.GetValue() == ConcurrentGeneration));
	}

	TEST_METHOD(DifferentSourceSnapshotRequiresCallerRevalidation)
	{
		const FAngelscriptHash256 ObservedGeneration = RepeatedByteHash(0x63);
		const FAngelscriptHash256 ConcurrentGeneration = RepeatedByteHash(0x64);
		const FAngelscriptCacheGenerationManifest Prepared = MakeManifest();
		FAngelscriptCacheGenerationManifest Concurrent = MakeManifest();
		Concurrent.SourceSnapshot = RepeatedByteHash(0x99);
		FAngelscriptCacheRebasePlan Plan;

		const FAngelscriptCacheStoreResult Result = EvaluateAngelscriptCacheRebase(
			ObservedGeneration,
			ConcurrentGeneration,
			&Concurrent,
			Prepared,
			Plan);

		AssertRebaseFailure(Result,
			EAngelscriptCacheStoreError::NeedsSourceRevalidation,
			ConcurrentGeneration);
		ASSERT_THAT(AreEqual(EAngelscriptCacheRebaseAction::Invalid, Plan.Action));
	}

	TEST_METHOD(SameSourceWithDifferentSemanticRootsIsAConflict)
	{
		const FAngelscriptHash256 ObservedGeneration = RepeatedByteHash(0x65);
		const FAngelscriptHash256 ConcurrentGeneration = RepeatedByteHash(0x66);
		const FAngelscriptCacheGenerationManifest Prepared = MakeManifest();
		FAngelscriptCacheGenerationManifest Concurrent = MakeManifest();
		Concurrent.ModuleSnapshots[0].RecordId =
			RecordId(EAngelscriptCacheRecordKind::ModuleSnapshot, 0xa0);
		Concurrent.Records[1].RecordId = Concurrent.ModuleSnapshots[0].RecordId;
		FAngelscriptCacheRebasePlan Plan;

		const FAngelscriptCacheStoreResult Result = EvaluateAngelscriptCacheRebase(
			ObservedGeneration,
			ConcurrentGeneration,
			&Concurrent,
			Prepared,
			Plan);

		AssertRebaseFailure(Result,
			EAngelscriptCacheStoreError::RebaseSemanticConflict,
			ConcurrentGeneration);
		ASSERT_THAT(AreEqual(EAngelscriptCacheRebaseAction::Invalid, Plan.Action));
	}

	TEST_METHOD(ChangedBaseThatDisappearedRequiresCallerRevalidation)
	{
		const FAngelscriptHash256 ObservedGeneration = RepeatedByteHash(0x67);
		const FAngelscriptCacheGenerationManifest Prepared = MakeManifest();
		FAngelscriptCacheRebasePlan Plan;

		const FAngelscriptCacheStoreResult Result = EvaluateAngelscriptCacheRebase(
			ObservedGeneration,
			TOptional<FAngelscriptHash256>{},
			nullptr,
			Prepared,
			Plan);

		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreError::NeedsSourceRevalidation, Result.Error));
		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreStage::Rebase, Result.Stage));
		ASSERT_THAT(IsFalse(Result.GenerationBefore.IsSet()));
		ASSERT_THAT(AreEqual(EAngelscriptCacheRebaseAction::Invalid, Plan.Action));
	}

	TEST_METHOD(AbsentObservedAndActualSlotProceedsAsFirstPublication)
	{
		const FAngelscriptCacheGenerationManifest Prepared = MakeManifest();
		FAngelscriptCacheRebasePlan Plan;

		const FAngelscriptCacheStoreResult Result = EvaluateAngelscriptCacheRebase(
			TOptional<FAngelscriptHash256>{},
			TOptional<FAngelscriptHash256>{},
			nullptr,
			Prepared,
			Plan);

		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreError::None, Result.Error));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheRebaseAction::ProceedFromObservedBase, Plan.Action));
		ASSERT_THAT(IsFalse(Plan.SelectedGenerationId.IsSet()));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
