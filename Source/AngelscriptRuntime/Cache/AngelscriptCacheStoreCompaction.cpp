#include "Cache/AngelscriptCacheStore.h"

#include "Cache/Private/AngelscriptCacheManifestPackValidation.h"
#include "Cache/Private/AngelscriptCacheStorePointerInternal.h"

namespace AngelscriptCacheStoreCompaction_Private
{
	struct FLoadedGeneration
	{
		FAngelscriptHash256 GenerationId;
		TOptional<FAngelscriptValidatedGeneration> Generation;
	};

	struct FRoot
	{
		EAngelscriptCachePointerKind Kind = EAngelscriptCachePointerKind::Invalid;
		TOptional<FAngelscriptHash256> OldGenerationId;
		int32 LoadedGenerationIndex = INDEX_NONE;
		FAngelscriptEncodedCacheGenerationManifest RewrittenManifest;
	};

	struct FRecordLocation
	{
		FAngelscriptCacheRecordId RecordId;
		FAngelscriptCachePackLocation Location;
	};

	static EAngelscriptCacheStorePathCategory PointerCategory(
		const EAngelscriptCachePointerKind Kind)
	{
		switch (Kind)
		{
		case EAngelscriptCachePointerKind::Current:
			return EAngelscriptCacheStorePathCategory::CurrentPointer;
		case EAngelscriptCachePointerKind::Previous:
			return EAngelscriptCacheStorePathCategory::PreviousPointer;
		case EAngelscriptCachePointerKind::PendingColdStart:
			return EAngelscriptCacheStorePathCategory::PendingColdStartPointer;
		default:
			return EAngelscriptCacheStorePathCategory::None;
		}
	}

	static FAngelscriptCacheStoreResult WithContext(
		FAngelscriptCacheStoreResult Result,
		const EAngelscriptCacheStoreStage Stage,
		const EAngelscriptCacheStorePathCategory Category =
			EAngelscriptCacheStorePathCategory::None)
	{
		Result.Stage = Stage;
		if (Result.PathCategory == EAngelscriptCacheStorePathCategory::None)
		{
			Result.PathCategory = Category;
		}
		return Result;
	}

	static FAngelscriptCacheStoreResult BeforeCommit(
		FAngelscriptCacheStoreResult Result,
		const EAngelscriptCacheStoreStage Stage,
		const EAngelscriptCacheStorePathCategory Category =
			EAngelscriptCacheStorePathCategory::None)
	{
		Result = WithContext(MoveTemp(Result), Stage, Category);
		Result.CommitState = EAngelscriptCacheStoreCommitState::NotCommitted;
		Result.GenerationAfter.Reset();
		return Result;
	}

	static FAngelscriptCacheStoreResult ValidationFailure(
		const FAngelscriptCacheValidationResult& Validation)
	{
		FAngelscriptCacheStoreResult Result = FAngelscriptCacheStoreResult::Failure(
			EAngelscriptCacheStoreError::ContentValidationFailed,
			EAngelscriptCacheStoreStage::CompactionRewrite);
		Result.ContentValidation = Validation;
		Result.CommitState = EAngelscriptCacheStoreCommitState::NotCommitted;
		return Result;
	}

	static bool CheckedAdd(uint64& Total, const uint64 Value)
	{
		if (Value > MAX_uint64 - Total)
		{
			return false;
		}
		Total += Value;
		return true;
	}

	static bool IsStrictFinalFileName(
		const FString& FileName,
		const FStringView Extension)
	{
		if (FileName.Len() != 64 + Extension.Len()
			|| !FileName.EndsWith(Extension, ESearchCase::CaseSensitive)
			|| FileName.Contains(TEXT("/"))
			|| FileName.Contains(TEXT("\\")))
		{
			return false;
		}
		for (int32 Index = 0; Index < 64; ++Index)
		{
			const TCHAR Character = FileName[Index];
			if (!((Character >= TEXT('0') && Character <= TEXT('9'))
				|| (Character >= TEXT('a') && Character <= TEXT('f'))))
			{
				return false;
			}
		}
		return true;
	}

	static FAngelscriptCacheStoreResult Cancelled(
		const EAngelscriptCacheStoreStage Stage,
		const EAngelscriptCacheStoreCommitState CommitState,
		const TOptional<FAngelscriptHash256>& GenerationBefore,
		const TOptional<FAngelscriptHash256>& GenerationAfter)
	{
		FAngelscriptCacheStoreResult Result = FAngelscriptCacheStoreResult::Failure(
			EAngelscriptCacheStoreError::Cancelled, Stage);
		Result.CommitState = CommitState;
		Result.GenerationBefore = GenerationBefore;
		Result.GenerationAfter = GenerationAfter;
		return Result;
	}
}

FAngelscriptCacheStoreResult CompactAngelscriptCacheStore(
	const FAngelscriptCacheStorePaths& Paths,
	const FAngelscriptCacheCompactionAuthority& Authority,
	const FAngelscriptCacheWriterToken& WriterToken,
	const FAngelscriptCachePackPolicy& PackPolicy,
	const FAngelscriptCacheReadLimits& Limits,
	FAngelscriptCacheReadBudget& Budget,
	const double LockDeadlineSeconds,
	TFunctionRef<bool()> IsCancellationRequested,
	IAngelscriptCacheStorageCodec& Codec,
	IAngelscriptCacheNamespaceLockOps& LockOps,
	IAngelscriptCacheAtomicFileOps& FileOps)
{
	using namespace AngelscriptCacheStoreCompaction_Private;
	if (Authority.Profile.Hash.IsZero() || Authority.SourceSnapshot.IsZero())
	{
		FAngelscriptCacheStoreResult Result = FAngelscriptCacheStoreResult::Failure(
			EAngelscriptCacheStoreError::NeedsSourceRevalidation,
			EAngelscriptCacheStoreStage::CompactionRewrite);
		Result.CommitState = EAngelscriptCacheStoreCommitState::NotCommitted;
		return Result;
	}
	if (IsCancellationRequested())
	{
		return Cancelled(
			EAngelscriptCacheStoreStage::CompactionRewrite,
			EAngelscriptCacheStoreCommitState::NotCommitted,
			{}, {});
	}

	TUniquePtr<IAngelscriptCacheNamespaceLockHandle> NamespaceLock;
	FAngelscriptCacheStoreResult Result = AcquireAngelscriptCacheNamespaceLock(
		Paths,
		LockDeadlineSeconds,
		IsCancellationRequested,
		LockOps,
		NamespaceLock);
	if (!Result.IsSuccess())
	{
		return BeforeCommit(
			MoveTemp(Result), EAngelscriptCacheStoreStage::CompactionRewrite);
	}
	check(NamespaceLock.IsValid());

	Result = CleanupAngelscriptCacheStaleTempsUnderLock(
		Paths, *NamespaceLock, FileOps);
	if (!Result.IsSuccess())
	{
		return BeforeCommit(
			MoveTemp(Result), EAngelscriptCacheStoreStage::CompactionRewrite);
	}

	FRoot Roots[] = {
		{EAngelscriptCachePointerKind::Current},
		{EAngelscriptCachePointerKind::Previous},
		{EAngelscriptCachePointerKind::PendingColdStart},
	};
	TArray<FLoadedGeneration> LoadedGenerations;
	for (FRoot& Root : Roots)
	{
		Result = ReadAngelscriptCachePointerSlot(
			Paths, Root.Kind, FileOps, Root.OldGenerationId);
		if (!Result.IsSuccess())
		{
			return BeforeCommit(
				MoveTemp(Result),
				EAngelscriptCacheStoreStage::CompactionRewrite,
				PointerCategory(Root.Kind));
		}
		if (!Root.OldGenerationId.IsSet())
		{
			continue;
		}

		Root.LoadedGenerationIndex = LoadedGenerations.IndexOfByPredicate(
			[&Root](const FLoadedGeneration& Loaded)
			{
				return Loaded.GenerationId == Root.OldGenerationId.GetValue();
			});
		if (Root.LoadedGenerationIndex != INDEX_NONE)
		{
			continue;
		}

		FLoadedGeneration& Loaded = LoadedGenerations.AddDefaulted_GetRef();
		Loaded.GenerationId = Root.OldGenerationId.GetValue();
		Root.LoadedGenerationIndex = LoadedGenerations.Num() - 1;
		Result = ReadAndValidateAngelscriptCacheGenerationUnderLock(
			Paths,
			Loaded.GenerationId,
			Limits,
			Budget,
			Codec,
			*NamespaceLock,
			FileOps,
			Loaded.Generation);
		if (!Result.IsSuccess())
		{
			return BeforeCommit(
				MoveTemp(Result),
				EAngelscriptCacheStoreStage::CompactionRewrite,
				EAngelscriptCacheStorePathCategory::Manifest);
		}
		check(Loaded.Generation.IsSet());
	}

	FRoot& PendingRoot = Roots[2];
	if (PendingRoot.LoadedGenerationIndex != INDEX_NONE)
	{
		const FAngelscriptCacheGenerationManifest& PendingManifest =
			LoadedGenerations[PendingRoot.LoadedGenerationIndex]
				.Generation->Manifest;
		if (PendingManifest.Profile.Hash != Authority.Profile.Hash
			|| PendingManifest.SourceSnapshot != Authority.SourceSnapshot)
		{
			Result = FileOps.AtomicRemovePointer(Paths.PendingColdStartPointer);
			if (!Result.IsSuccess())
			{
				return BeforeCommit(
					MoveTemp(Result),
					EAngelscriptCacheStoreStage::CompactionRewrite,
					EAngelscriptCacheStorePathCategory::PendingColdStartPointer);
			}
			Result = FileOps.SyncDirectory(Paths.NamespaceRoot);
			if (!Result.IsSuccess())
			{
				return BeforeCommit(
					MoveTemp(Result),
					EAngelscriptCacheStoreStage::CompactionRewrite,
					EAngelscriptCacheStorePathCategory::PendingColdStartPointer);
			}
			PendingRoot.OldGenerationId.Reset();
			PendingRoot.LoadedGenerationIndex = INDEX_NONE;
		}
	}

	if (IsCancellationRequested())
	{
		return Cancelled(
			EAngelscriptCacheStoreStage::CompactionRewrite,
			EAngelscriptCacheStoreCommitState::NotCommitted,
			Roots[0].OldGenerationId, {});
	}

	uint64 RecordReferenceCount = 0;
	uint64 RecordPayloadBytes = 0;
	for (const FRoot& Root : Roots)
	{
		if (Root.LoadedGenerationIndex == INDEX_NONE)
		{
			continue;
		}
		const TArray<FAngelscriptDecodedCacheRecordHandle>& Records =
			LoadedGenerations[Root.LoadedGenerationIndex]
				.Generation->ReachableRecords;
		if (!CheckedAdd(RecordReferenceCount, static_cast<uint64>(Records.Num())))
		{
			return ValidationFailure(FAngelscriptCacheValidationResult::AtStage(
				EAngelscriptCacheValidationError::BudgetExceeded,
				static_cast<EAngelscriptCacheRecordKind>(0),
				EAngelscriptCacheValidationStage::ManifestGraph));
		}
		for (const FAngelscriptDecodedCacheRecordHandle& Record : Records)
		{
			if (!CheckedAdd(
				RecordPayloadBytes,
				static_cast<uint64>(Record.Get().GetCanonicalPayload().Num())))
			{
				return ValidationFailure(FAngelscriptCacheValidationResult::AtStage(
					EAngelscriptCacheValidationError::BudgetExceeded,
					static_cast<EAngelscriptCacheRecordKind>(0),
					EAngelscriptCacheValidationStage::ManifestGraph));
			}
		}
	}

	uint64 WorkingBytes = 0;
	const uint64 PerReferenceBytes =
		sizeof(const FAngelscriptDecodedCacheRecord*)
		+ sizeof(FAngelscriptPreparedRecord)
		+ sizeof(FRecordLocation)
		+ sizeof(FAngelscriptCacheRecordIndexEntry);
	if (RecordReferenceCount > MAX_uint64 / PerReferenceBytes
		|| !CheckedAdd(WorkingBytes, RecordReferenceCount * PerReferenceBytes)
		|| RecordPayloadBytes > MAX_uint64 / 2
		|| !CheckedAdd(WorkingBytes, RecordPayloadBytes * 2))
	{
		return ValidationFailure(FAngelscriptCacheValidationResult::AtStage(
			EAngelscriptCacheValidationError::BudgetExceeded,
			static_cast<EAngelscriptCacheRecordKind>(0),
			EAngelscriptCacheValidationStage::ManifestGraph));
	}
	FAngelscriptCacheTemporaryResidentReservation WorkingReservation;
	if (!Budget.TryReserveTemporaryDecoded(
		WorkingBytes, Limits, WorkingReservation))
	{
		return ValidationFailure(FAngelscriptCacheValidationResult::AtStage(
			EAngelscriptCacheValidationError::BudgetExceeded,
			static_cast<EAngelscriptCacheRecordKind>(0),
			EAngelscriptCacheValidationStage::ManifestGraph));
	}

	TArray<const FAngelscriptDecodedCacheRecord*> RecordViews;
	RecordViews.Reserve(static_cast<int32>(RecordReferenceCount));
	for (const FRoot& Root : Roots)
	{
		if (Root.LoadedGenerationIndex == INDEX_NONE)
		{
			continue;
		}
		for (const FAngelscriptDecodedCacheRecordHandle& Record :
			LoadedGenerations[Root.LoadedGenerationIndex]
				.Generation->ReachableRecords)
		{
			RecordViews.Add(&Record.Get());
		}
	}
	RecordViews.Sort([](
		const FAngelscriptDecodedCacheRecord& Left,
		const FAngelscriptDecodedCacheRecord& Right)
	{
		return Left.GetRecordId() < Right.GetRecordId();
	});

	TArray<FAngelscriptPreparedRecord> PreparedRecords;
	PreparedRecords.Reserve(RecordViews.Num());
	const FAngelscriptDecodedCacheRecord* PreviousRecord = nullptr;
	for (const FAngelscriptDecodedCacheRecord* Record : RecordViews)
	{
		if (PreviousRecord != nullptr
			&& PreviousRecord->GetRecordId() == Record->GetRecordId())
		{
			const TConstArrayView<uint8> PreviousPayload =
				PreviousRecord->GetCanonicalPayload();
			const TConstArrayView<uint8> Payload = Record->GetCanonicalPayload();
			if (PreviousPayload.Num() != Payload.Num()
				|| FMemory::Memcmp(
					PreviousPayload.GetData(), Payload.GetData(), Payload.Num()) != 0)
			{
				return BeforeCommit(
					FAngelscriptCacheStoreResult::Failure(
						EAngelscriptCacheStoreError::ImmutableObjectCollisionOrCorruption,
						EAngelscriptCacheStoreStage::CompactionRewrite),
					EAngelscriptCacheStoreStage::CompactionRewrite);
			}
			continue;
		}

		FAngelscriptPreparedRecord& Prepared = PreparedRecords.AddDefaulted_GetRef();
		Prepared.RecordId = Record->GetRecordId();
		Prepared.CanonicalPayload.Append(Record->GetCanonicalPayload());
		PreviousRecord = Record;
	}

	TArray<FAngelscriptEncodedPack> RewrittenPacks;
	const FAngelscriptCacheValidationResult PackBuild = BuildAngelscriptCachePacks(
		PreparedRecords, PackPolicy, Codec, RewrittenPacks);
	if (!PackBuild.IsSuccess())
	{
		return ValidationFailure(PackBuild);
	}

	TArray<FRecordLocation> Locations;
	for (const FAngelscriptEncodedPack& Pack : RewrittenPacks)
	{
		for (const FAngelscriptCachePackIndexEntry& Index : Pack.Index)
		{
			FRecordLocation& Location = Locations.AddDefaulted_GetRef();
			Location.RecordId = Index.RecordId;
			Location.Location.PackId = Pack.PackId;
			Location.Location.PackOffset = Index.PackOffset;
			Location.Location.StoredSize = Index.StoredSize;
			Location.Location.RawSize = Index.RawSize;
			Location.Location.Codec = Index.Codec;
			Location.Location.RawChecksum = Index.RawChecksum;
		}
	}
	Locations.Sort([](const FRecordLocation& Left, const FRecordLocation& Right)
	{
		return Left.RecordId < Right.RecordId;
	});

	for (FRoot& Root : Roots)
	{
		if (Root.LoadedGenerationIndex == INDEX_NONE)
		{
			continue;
		}
		const FAngelscriptValidatedGeneration& Loaded =
			LoadedGenerations[Root.LoadedGenerationIndex].Generation.GetValue();
		FAngelscriptCacheGenerationManifest Manifest = Loaded.Manifest;
		Manifest.Records.Reset();
		for (const FAngelscriptDecodedCacheRecordHandle& Record :
			Loaded.ReachableRecords)
		{
			const FAngelscriptCacheRecordId& RecordId = Record.Get().GetRecordId();
			const int32 LocationIndex = Algo::LowerBoundBy(
				Locations, RecordId,
				[](const FRecordLocation& Location)
				{
					return Location.RecordId;
				});
			if (!Locations.IsValidIndex(LocationIndex)
				|| Locations[LocationIndex].RecordId != RecordId)
			{
				return BeforeCommit(
					FAngelscriptCacheStoreResult::Failure(
						EAngelscriptCacheStoreError::ContentValidationFailed,
						EAngelscriptCacheStoreStage::CompactionRewrite),
					EAngelscriptCacheStoreStage::CompactionRewrite);
			}
			FAngelscriptCacheRecordIndexEntry& Entry =
				Manifest.Records.AddDefaulted_GetRef();
			Entry.RecordId = RecordId;
			Entry.Location = Locations[LocationIndex].Location;
		}
		Manifest.Records.Sort([](
			const FAngelscriptCacheRecordIndexEntry& Left,
			const FAngelscriptCacheRecordIndexEntry& Right)
		{
			return Left.RecordId < Right.RecordId;
		});
		const FAngelscriptCacheValidationResult EncodeResult =
			EncodeAngelscriptCacheGenerationManifest(
				Manifest, Root.RewrittenManifest);
		if (!EncodeResult.IsSuccess())
		{
			return ValidationFailure(EncodeResult);
		}
	}

	for (const FAngelscriptEncodedPack& Pack : RewrittenPacks)
	{
		Result = PutAngelscriptCachePackIfAbsent(
			Paths,
			Pack.PackId,
			Pack.Bytes,
			WriterToken,
			Limits,
			FileOps);
		if (!Result.IsSuccess())
		{
			return BeforeCommit(
				MoveTemp(Result), EAngelscriptCacheStoreStage::CompactionRewrite,
				EAngelscriptCacheStorePathCategory::Pack);
		}
	}
	TArray<FAngelscriptHash256> InstalledManifests;
	for (const FRoot& Root : Roots)
	{
		if (Root.LoadedGenerationIndex == INDEX_NONE
			|| InstalledManifests.Contains(
				Root.RewrittenManifest.ComputedGenerationId))
		{
			continue;
		}
		Result = PutAngelscriptCacheManifestIfAbsent(
			Paths,
			Root.RewrittenManifest.ComputedGenerationId,
			Root.RewrittenManifest.CompleteBytes,
			WriterToken,
			Limits,
			Codec,
			FileOps);
		if (!Result.IsSuccess())
		{
			return BeforeCommit(
				MoveTemp(Result), EAngelscriptCacheStoreStage::CompactionRewrite,
				EAngelscriptCacheStorePathCategory::Manifest);
		}
		InstalledManifests.Add(Root.RewrittenManifest.ComputedGenerationId);
	}

	FRoot* SwitchOrder[] = {&Roots[1], &Roots[2], &Roots[0]};
	int32 FinalSwitchIndex = INDEX_NONE;
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(SwitchOrder); ++Index)
	{
		if (SwitchOrder[Index]->LoadedGenerationIndex != INDEX_NONE)
		{
			FinalSwitchIndex = Index;
		}
	}
	TOptional<FAngelscriptHash256> GenerationBefore = Roots[0].OldGenerationId;
	TOptional<FAngelscriptHash256> GenerationAfter;
	if (Roots[0].LoadedGenerationIndex != INDEX_NONE)
	{
		GenerationAfter = Roots[0].RewrittenManifest.ComputedGenerationId;
	}
	else if (FinalSwitchIndex != INDEX_NONE)
	{
		GenerationBefore = SwitchOrder[FinalSwitchIndex]->OldGenerationId;
		GenerationAfter =
			SwitchOrder[FinalSwitchIndex]->RewrittenManifest.ComputedGenerationId;
	}

	for (int32 Index = 0; Index < UE_ARRAY_COUNT(SwitchOrder); ++Index)
	{
		FRoot& Root = *SwitchOrder[Index];
		if (Root.LoadedGenerationIndex == INDEX_NONE)
		{
			continue;
		}
		bool bSelected = false;
		Result = RewriteAngelscriptCachePointerForCompactionUnderLock(
			Paths,
			Root.Kind,
			Root.RewrittenManifest.ComputedGenerationId,
			WriterToken,
			*NamespaceLock,
			FileOps,
			bSelected);
		if (!Result.IsSuccess())
		{
			Result.CommitState = Index == FinalSwitchIndex && bSelected
				? EAngelscriptCacheStoreCommitState::CompactionCommitted
				: EAngelscriptCacheStoreCommitState::NotCommitted;
			Result.GenerationBefore = GenerationBefore;
			Result.GenerationAfter = bSelected ? GenerationAfter : TOptional<FAngelscriptHash256>{};
			return Result;
		}
	}

	NamespaceLock.Reset();
	Result = AcquireAngelscriptCacheNamespaceLock(
		Paths,
		LockDeadlineSeconds,
		IsCancellationRequested,
		LockOps,
		NamespaceLock);
	if (!Result.IsSuccess())
	{
		Result = WithContext(
			MoveTemp(Result), EAngelscriptCacheStoreStage::CompactionSweep);
		Result.CommitState = EAngelscriptCacheStoreCommitState::CompactionCommitted;
		Result.GenerationBefore = GenerationBefore;
		Result.GenerationAfter = GenerationAfter;
		return Result;
	}
	if (IsCancellationRequested())
	{
		NamespaceLock.Reset();
		return Cancelled(
			EAngelscriptCacheStoreStage::CompactionSweep,
			EAngelscriptCacheStoreCommitState::CompactionCommitted,
			GenerationBefore,
			GenerationAfter);
	}

	auto AfterCommit = [GenerationBefore, GenerationAfter](
		FAngelscriptCacheStoreResult Result,
		const EAngelscriptCacheStorePathCategory Category =
			EAngelscriptCacheStorePathCategory::None)
	{
		Result = WithContext(
			MoveTemp(Result), EAngelscriptCacheStoreStage::CompactionSweep, Category);
		Result.CommitState = EAngelscriptCacheStoreCommitState::CompactionCommitted;
		Result.GenerationBefore = GenerationBefore;
		Result.GenerationAfter = GenerationAfter;
		return Result;
	};
	auto AfterCommitValidationFailure = [&AfterCommit](
		const FAngelscriptCacheValidationResult& Validation)
	{
		FAngelscriptCacheStoreResult Failure = FAngelscriptCacheStoreResult::Failure(
			EAngelscriptCacheStoreError::ContentValidationFailed,
			EAngelscriptCacheStoreStage::CompactionSweep,
			EAngelscriptCacheStorePathCategory::Manifest);
		Failure.ContentValidation = Validation;
		return AfterCommit(MoveTemp(Failure));
	};

	TSet<FString> MarkedManifestNames;
	TSet<FString> MarkedPackNames;
	TArray<FAngelscriptHash256> MarkedGenerationIds;
	const EAngelscriptCachePointerKind PhysicalRootKinds[] = {
		EAngelscriptCachePointerKind::Current,
		EAngelscriptCachePointerKind::Previous,
		EAngelscriptCachePointerKind::PendingColdStart,
	};
	MarkedGenerationIds.Reserve(UE_ARRAY_COUNT(PhysicalRootKinds));
	for (const EAngelscriptCachePointerKind Kind : PhysicalRootKinds)
	{
		TOptional<FAngelscriptHash256> PhysicalGenerationId;
		Result = ReadAngelscriptCachePointerSlot(
			Paths, Kind, FileOps, PhysicalGenerationId);
		if (!Result.IsSuccess())
		{
			// A malformed pointer is not a physical root. I/O failures remain
			// fail-safe: abort the sweep rather than guessing its reachability.
			if (Result.Error == EAngelscriptCacheStoreError::PointerInvalid)
			{
				continue;
			}
			return AfterCommit(MoveTemp(Result), PointerCategory(Kind));
		}
		if (!PhysicalGenerationId.IsSet())
		{
			continue;
		}

		const FAngelscriptHash256& PhysicalId = PhysicalGenerationId.GetValue();
		if (MarkedGenerationIds.Contains(PhysicalId))
		{
			continue;
		}
		TArray<uint8> ManifestBytes;
		Result = FileOps.ReopenReadAll(
			Paths.BuildManifestPath(PhysicalId),
			Limits.MaxManifestBytes,
			ManifestBytes);
		if (!Result.IsSuccess())
		{
			return AfterCommit(
				MoveTemp(Result), EAngelscriptCacheStorePathCategory::Manifest);
		}

		AngelscriptCacheManifestPack_Private::FPreparedGenerationValidation Prepared;
		const FAngelscriptCacheValidationResult PrepareResult =
			AngelscriptCacheManifestPack_Private::PrepareGenerationValidation(
				ManifestBytes, PhysicalId, Limits, Budget, Prepared);
		if (!PrepareResult.IsSuccess())
		{
			return AfterCommitValidationFailure(PrepareResult);
		}

		MarkedGenerationIds.Add(PhysicalId);
		MarkedManifestNames.Add(
			PhysicalId.ToHexString() + TEXT(".asmanifest"));
		for (const FAngelscriptHash256& PackId : Prepared.GetDistinctPackIds())
		{
			MarkedPackNames.Add(PackId.ToHexString() + TEXT(".aspack"));
		}
	}

	TOptional<FAngelscriptCacheStoreResult> FirstDeferredDelete;
	auto SweepDirectory = [&](const FString& Directory,
		const FStringView Extension,
		const TSet<FString>& MarkedNames,
		const EAngelscriptCacheStorePathCategory Category)
		-> FAngelscriptCacheStoreResult
	{
		TArray<FString> FileNames;
		FAngelscriptCacheStoreResult SweepResult =
			FileOps.EnumerateDirectFileNames(Directory, FileNames);
		if (!SweepResult.IsSuccess())
		{
			return AfterCommit(MoveTemp(SweepResult), Category);
		}
		if (static_cast<uint64>(FileNames.Num()) > Limits.MaxArrayElements)
		{
			return AfterCommitValidationFailure(
				FAngelscriptCacheValidationResult::AtStage(
					EAngelscriptCacheValidationError::BudgetExceeded,
					static_cast<EAngelscriptCacheRecordKind>(0),
					EAngelscriptCacheValidationStage::ManifestGraph));
		}
		FileNames.Sort();
		for (const FString& FileName : FileNames)
		{
			if (!IsStrictFinalFileName(FileName, Extension)
				|| MarkedNames.Contains(FileName))
			{
				continue;
			}
			if (IsCancellationRequested())
			{
				return Cancelled(
					EAngelscriptCacheStoreStage::CompactionSweep,
					EAngelscriptCacheStoreCommitState::CompactionCommitted,
					GenerationBefore,
					GenerationAfter);
			}

			const FString FinalPath = Directory / FileName;
			SweepResult = FileOps.RemoveFinalImmutable(FinalPath);
			if (SweepResult.Error == EAngelscriptCacheStoreError::DeleteDeferred)
			{
				if (!FirstDeferredDelete.IsSet())
				{
					FirstDeferredDelete =
						AfterCommit(MoveTemp(SweepResult), Category);
				}
				continue;
			}
			if (!SweepResult.IsSuccess())
			{
				return AfterCommit(MoveTemp(SweepResult), Category);
			}

			// Durably close each destructive step before cancellation can be
			// observed for the next file.
			SweepResult = FileOps.SyncDirectory(Directory);
			if (!SweepResult.IsSuccess())
			{
				return AfterCommit(MoveTemp(SweepResult), Category);
			}
		}
		return FAngelscriptCacheStoreResult::Success();
	};

	Result = SweepDirectory(
		Paths.GenerationsDirectory,
		TEXTVIEW(".asmanifest"),
		MarkedManifestNames,
		EAngelscriptCacheStorePathCategory::Manifest);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	Result = SweepDirectory(
		Paths.PacksDirectory,
		TEXTVIEW(".aspack"),
		MarkedPackNames,
		EAngelscriptCacheStorePathCategory::Pack);
	if (!Result.IsSuccess())
	{
		return Result;
	}

	Result = CleanupAngelscriptCacheStaleTempsUnderLock(
		Paths, *NamespaceLock, FileOps);
	if (!Result.IsSuccess())
	{
		return AfterCommit(MoveTemp(Result));
	}
	NamespaceLock.Reset();
	if (FirstDeferredDelete.IsSet())
	{
		return FirstDeferredDelete.GetValue();
	}

	Result = FAngelscriptCacheStoreResult::Success();
	Result.Stage = EAngelscriptCacheStoreStage::CompactionSweep;
	Result.CommitState = EAngelscriptCacheStoreCommitState::CompactionCommitted;
	Result.GenerationBefore = GenerationBefore;
	Result.GenerationAfter = GenerationAfter;
	return Result;
}
