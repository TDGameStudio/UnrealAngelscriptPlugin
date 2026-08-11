#include "Cache/AngelscriptCacheStore.h"
#include "Cache/Private/AngelscriptCacheStorePointerInternal.h"

#include "Hash/Blake3.h"

namespace AngelscriptCacheStorePointer_Private
{
	static constexpr uint8 Magic[8] = {'U', 'E', 'A', 'S', 'C', 'V', '2', 'C'};
	static constexpr uint32 SchemaVersion = 1;
	static constexpr int32 CompleteSize = 80;
	static constexpr int32 ChecksumInputSize = 48;

	static bool IsKnownKind(const EAngelscriptCachePointerKind Kind)
	{
		return Kind == EAngelscriptCachePointerKind::Current
			|| Kind == EAngelscriptCachePointerKind::Previous
			|| Kind == EAngelscriptCachePointerKind::PendingColdStart;
	}

	static EAngelscriptCacheStoreStage StageForKind(
		const EAngelscriptCachePointerKind Kind)
	{
		switch (Kind)
		{
		case EAngelscriptCachePointerKind::Current:
			return EAngelscriptCacheStoreStage::CurrentPointer;
		case EAngelscriptCachePointerKind::Previous:
			return EAngelscriptCacheStoreStage::PreviousPointer;
		case EAngelscriptCachePointerKind::PendingColdStart:
			return EAngelscriptCacheStoreStage::PendingPointer;
		default:
			return EAngelscriptCacheStoreStage::None;
		}
	}

	static EAngelscriptCacheStorePathCategory CategoryForKind(
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

	static FAngelscriptCacheStoreResult PointerFailure(
		const EAngelscriptCachePointerKind Kind)
	{
		return FAngelscriptCacheStoreResult::Failure(
			EAngelscriptCacheStoreError::PointerInvalid,
			StageForKind(Kind),
			CategoryForKind(Kind));
	}

	static void WriteUInt32LittleEndian(uint8* Bytes, const uint32 Value)
	{
		Bytes[0] = static_cast<uint8>(Value);
		Bytes[1] = static_cast<uint8>(Value >> 8);
		Bytes[2] = static_cast<uint8>(Value >> 16);
		Bytes[3] = static_cast<uint8>(Value >> 24);
	}

	static uint32 ReadUInt32LittleEndian(const uint8* Bytes)
	{
		return static_cast<uint32>(Bytes[0])
			| (static_cast<uint32>(Bytes[1]) << 8)
			| (static_cast<uint32>(Bytes[2]) << 16)
			| (static_cast<uint32>(Bytes[3]) << 24);
	}

	static FAngelscriptCacheStoreResult SetPointerContext(
		FAngelscriptCacheStoreResult Result,
		const EAngelscriptCachePointerKind Kind)
	{
		if (Result.Stage == EAngelscriptCacheStoreStage::None)
		{
			Result.Stage = StageForKind(Kind);
		}
		if (Result.PathCategory == EAngelscriptCacheStorePathCategory::None)
		{
			Result.PathCategory = CategoryForKind(Kind);
		}
		return Result;
	}

	static FAngelscriptCacheStoreResult PreparePointerTemp(
		const FString& TempPath,
		const EAngelscriptCachePointerKind Kind,
		const FAngelscriptHash256& GenerationId,
		IAngelscriptCacheAtomicFileOps& FileOps)
	{
		TArray<uint8> ExpectedBytes;
		FAngelscriptCacheStoreResult Result = EncodeAngelscriptCachePointer(
			{Kind, GenerationId}, ExpectedBytes);
		if (!Result.IsSuccess())
		{
			return SetPointerContext(MoveTemp(Result), Kind);
		}

		Result = FileOps.WriteFlushClose(TempPath, ExpectedBytes);
		if (!Result.IsSuccess())
		{
			return SetPointerContext(MoveTemp(Result), Kind);
		}

		TArray<uint8> ReopenedBytes;
		Result = FileOps.ReopenReadAll(TempPath, CompleteSize + 1, ReopenedBytes);
		if (!Result.IsSuccess())
		{
			if (Result.Error == EAngelscriptCacheStoreError::ReadFailed)
			{
				Result = PointerFailure(Kind);
			}
			return SetPointerContext(MoveTemp(Result), Kind);
		}

		FAngelscriptCachePointerValue ReopenedValue;
		Result = DecodeAngelscriptCachePointer(ReopenedBytes, Kind, ReopenedValue);
		const bool bExactBytes = ReopenedBytes.Num() == ExpectedBytes.Num()
			&& FMemory::Memcmp(
				ReopenedBytes.GetData(), ExpectedBytes.GetData(), ExpectedBytes.Num()) == 0;
		if (!Result.IsSuccess() || !bExactBytes
			|| ReopenedValue.GenerationId != GenerationId)
		{
			return PointerFailure(Kind);
		}
		return FAngelscriptCacheStoreResult::Success();
	}

	static bool RereadPointerSelectsGeneration(
		const FString& PointerPath,
		const EAngelscriptCachePointerKind Kind,
		const FAngelscriptHash256& GenerationId,
		IAngelscriptCacheAtomicFileOps& FileOps)
	{
		TArray<uint8> Bytes;
		if (!FileOps.ReopenReadAll(PointerPath, CompleteSize + 1, Bytes).IsSuccess())
		{
			return false;
		}
		FAngelscriptCachePointerValue Value;
		return DecodeAngelscriptCachePointer(Bytes, Kind, Value).IsSuccess()
			&& Value.GenerationId == GenerationId;
	}

	static const FString* PathForKind(
		const FAngelscriptCacheStorePaths& Paths,
		const EAngelscriptCachePointerKind Kind)
	{
		switch (Kind)
		{
		case EAngelscriptCachePointerKind::Current:
			return &Paths.CurrentPointer;
		case EAngelscriptCachePointerKind::Previous:
			return &Paths.PreviousPointer;
		case EAngelscriptCachePointerKind::PendingColdStart:
			return &Paths.PendingColdStartPointer;
		default:
			return nullptr;
		}
	}

	static void SetGenerationBefore(
		FAngelscriptCacheStoreResult& Result,
		const TOptional<FAngelscriptHash256>& GenerationBefore)
	{
		Result.GenerationBefore = GenerationBefore;
	}

	static FAngelscriptCacheStoreResult CancelledResult(
		const EAngelscriptCachePointerKind Kind,
		const TOptional<FAngelscriptHash256>& GenerationBefore)
	{
		FAngelscriptCacheStoreResult Result = FAngelscriptCacheStoreResult::Failure(
			EAngelscriptCacheStoreError::Cancelled,
			StageForKind(Kind),
			CategoryForKind(Kind));
		SetGenerationBefore(Result, GenerationBefore);
		return Result;
	}

	static FAngelscriptCacheStoreResult CommittedResult(
		FAngelscriptCacheStoreResult Result,
		const EAngelscriptCacheStoreCommitState CommitState,
		const TOptional<FAngelscriptHash256>& GenerationBefore,
		const FAngelscriptHash256& GenerationAfter)
	{
		Result.CommitState = CommitState;
		Result.GenerationBefore = GenerationBefore;
		Result.GenerationAfter = GenerationAfter;
		return Result;
	}
}

FAngelscriptCacheStoreResult EncodeAngelscriptCachePointer(
	const FAngelscriptCachePointerValue& Value,
	TArray<uint8>& OutBytes)
{
	using namespace AngelscriptCacheStorePointer_Private;
	OutBytes.Reset();
	if (!IsKnownKind(Value.Kind) || Value.GenerationId.IsZero())
	{
		return PointerFailure(Value.Kind);
	}

	OutBytes.SetNumZeroed(CompleteSize);
	FMemory::Memcpy(OutBytes.GetData(), Magic, UE_ARRAY_COUNT(Magic));
	WriteUInt32LittleEndian(OutBytes.GetData() + 8, SchemaVersion);
	OutBytes[12] = static_cast<uint8>(Value.Kind);
	FMemory::Memcpy(
		OutBytes.GetData() + 16,
		Value.GenerationId.Value.GetBytes(),
		32);
	const FBlake3Hash Checksum = FBlake3::HashBuffer(
		OutBytes.GetData(), ChecksumInputSize);
	FMemory::Memcpy(OutBytes.GetData() + 48, Checksum.GetBytes(), 32);
	return FAngelscriptCacheStoreResult::Success();
}

FAngelscriptCacheStoreResult DecodeAngelscriptCachePointer(
	const TConstArrayView<uint8> Bytes,
	const EAngelscriptCachePointerKind ExpectedKind,
	FAngelscriptCachePointerValue& OutValue)
{
	using namespace AngelscriptCacheStorePointer_Private;
	OutValue = FAngelscriptCachePointerValue{};
	if (!IsKnownKind(ExpectedKind) || Bytes.Num() != CompleteSize
		|| FMemory::Memcmp(Bytes.GetData(), Magic, UE_ARRAY_COUNT(Magic)) != 0
		|| ReadUInt32LittleEndian(Bytes.GetData() + 8) != SchemaVersion
		|| Bytes[12] != static_cast<uint8>(ExpectedKind)
		|| Bytes[13] != 0 || Bytes[14] != 0 || Bytes[15] != 0)
	{
		return PointerFailure(ExpectedKind);
	}

	FBlake3Hash::ByteArray GenerationBytes{};
	FMemory::Memcpy(GenerationBytes, Bytes.GetData() + 16, sizeof(GenerationBytes));
	const FAngelscriptHash256 GenerationId{FBlake3Hash(GenerationBytes)};
	if (GenerationId.IsZero())
	{
		return PointerFailure(ExpectedKind);
	}

	const FBlake3Hash ExpectedChecksum = FBlake3::HashBuffer(
		Bytes.GetData(), ChecksumInputSize);
	if (FMemory::Memcmp(
		Bytes.GetData() + 48, ExpectedChecksum.GetBytes(), 32) != 0)
	{
		return PointerFailure(ExpectedKind);
	}

	OutValue.Kind = ExpectedKind;
	OutValue.GenerationId = GenerationId;
	return FAngelscriptCacheStoreResult::Success();
}

FAngelscriptCacheStoreResult ReadAngelscriptCachePointerSlot(
	const FAngelscriptCacheStorePaths& Paths,
	const EAngelscriptCachePointerKind Kind,
	IAngelscriptCacheAtomicFileOps& FileOps,
	TOptional<FAngelscriptHash256>& OutGenerationId)
{
	using namespace AngelscriptCacheStorePointer_Private;
	OutGenerationId.Reset();
	const FString* PointerPath = PathForKind(Paths, Kind);
	if (PointerPath == nullptr)
	{
		return PointerFailure(Kind);
	}

	TArray<uint8> Bytes;
	FAngelscriptCacheStoreResult Result = FileOps.ReopenReadAll(
		*PointerPath, CompleteSize + 1, Bytes);
	if (!Result.IsSuccess())
	{
		// The atomic-file seam uses PointerInvalid for a not-found pointer because
		// pointer absence has no Store error number of its own. A present corrupt
		// file reaches the decoder below and remains PointerInvalid.
		if (Result.Error == EAngelscriptCacheStoreError::PointerInvalid)
		{
			return FAngelscriptCacheStoreResult::Success();
		}
		if (Result.Error == EAngelscriptCacheStoreError::ReadFailed)
		{
			return PointerFailure(Kind);
		}
		return SetPointerContext(MoveTemp(Result), Kind);
	}

	FAngelscriptCachePointerValue Value;
	Result = DecodeAngelscriptCachePointer(Bytes, Kind, Value);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	OutGenerationId = Value.GenerationId;
	return FAngelscriptCacheStoreResult::Success();
}

FAngelscriptCacheStoreResult RewriteAngelscriptCachePointerForCompactionUnderLock(
	const FAngelscriptCacheStorePaths& Paths,
	const EAngelscriptCachePointerKind Kind,
	const FAngelscriptHash256& NewGenerationId,
	const FAngelscriptCacheWriterToken& WriterToken,
	IAngelscriptCacheNamespaceLockHandle& NamespaceLock,
	IAngelscriptCacheAtomicFileOps& FileOps,
	bool& OutSelected)
{
	using namespace AngelscriptCacheStorePointer_Private;
	(void)NamespaceLock;
	OutSelected = false;
	const FString* PointerPath = PathForKind(Paths, Kind);
	if (PointerPath == nullptr || NewGenerationId.IsZero())
	{
		FAngelscriptCacheStoreResult Result = PointerFailure(Kind);
		Result.Stage = EAngelscriptCacheStoreStage::CompactionSwitch;
		return Result;
	}

	const FString TempPath = Kind == EAngelscriptCachePointerKind::Current
		? Paths.BuildCurrentPointerTempPath(WriterToken)
		: Kind == EAngelscriptCachePointerKind::Previous
			? Paths.BuildPreviousPointerTempPath(WriterToken)
			: Paths.BuildPendingColdStartPointerTempPath(WriterToken);
	auto WithContext = [Kind](FAngelscriptCacheStoreResult Result)
	{
		Result.Stage = EAngelscriptCacheStoreStage::CompactionSwitch;
		if (Result.PathCategory == EAngelscriptCacheStorePathCategory::None)
		{
			Result.PathCategory = CategoryForKind(Kind);
		}
		return Result;
	};

	FAngelscriptCacheStoreResult Result = PreparePointerTemp(
		TempPath, Kind, NewGenerationId, FileOps);
	if (!Result.IsSuccess())
	{
		FileOps.RemoveOwnTemp(TempPath);
		return WithContext(MoveTemp(Result));
	}

	Result = FileOps.AtomicInstallOrReplacePointer(TempPath, *PointerPath);
	if (!Result.IsSuccess())
	{
		OutSelected = RereadPointerSelectsGeneration(
			*PointerPath, Kind, NewGenerationId, FileOps);
		FileOps.RemoveOwnTemp(TempPath);
		return WithContext(MoveTemp(Result));
	}

	OutSelected = true;
	Result = FileOps.SyncDirectory(Paths.NamespaceRoot);
	return Result.IsSuccess()
		? Result
		: WithContext(MoveTemp(Result));
}

FAngelscriptCacheStoreResult PublishAngelscriptCachePointers(
	const FAngelscriptCacheStorePaths& Paths,
	const EAngelscriptCachePublicationDisposition Disposition,
	const FAngelscriptHash256& NewGenerationId,
	const TOptional<FAngelscriptHash256>& ValidatedOldCurrent,
	const FAngelscriptCacheWriterToken& WriterToken,
	TFunctionRef<bool()> IsCancellationRequested,
	IAngelscriptCacheNamespaceLockHandle& NamespaceLock,
	IAngelscriptCacheAtomicFileOps& FileOps,
	IAngelscriptCacheStoreFaultInjector* FaultInjector)
{
	using namespace AngelscriptCacheStorePointer_Private;
	(void)NamespaceLock;
	const bool bPublishCurrent =
		Disposition == EAngelscriptCachePublicationDisposition::Current;
	const bool bPublishPending =
		Disposition == EAngelscriptCachePublicationDisposition::PendingColdStart;
	const EAngelscriptCachePointerKind DestinationKind = bPublishCurrent
		? EAngelscriptCachePointerKind::Current
		: EAngelscriptCachePointerKind::PendingColdStart;
	if ((!bPublishCurrent && !bPublishPending) || NewGenerationId.IsZero()
		|| (ValidatedOldCurrent.IsSet()
			&& ValidatedOldCurrent.GetValue().IsZero()))
	{
		return PointerFailure(DestinationKind);
	}

	const FString& DestinationPath = bPublishCurrent
		? Paths.CurrentPointer
		: Paths.PendingColdStartPointer;
	const FString DestinationTempPath = bPublishCurrent
		? Paths.BuildCurrentPointerTempPath(WriterToken)
		: Paths.BuildPendingColdStartPointerTempPath(WriterToken);
	const FString PreviousTempPath =
		Paths.BuildPreviousPointerTempPath(WriterToken);
	TArray<FString> OwnTemps;
	OwnTemps.Add(DestinationTempPath);
	auto CleanupTemps = [&]()
	{
		for (const FString& TempPath : OwnTemps)
		{
			FileOps.RemoveOwnTemp(TempPath);
		}
	};
	auto ReturnUncommitted = [&](FAngelscriptCacheStoreResult Result,
		const EAngelscriptCachePointerKind Kind)
	{
		CleanupTemps();
		Result = SetPointerContext(MoveTemp(Result), Kind);
		Result.CommitState = EAngelscriptCacheStoreCommitState::NotCommitted;
		SetGenerationBefore(Result, ValidatedOldCurrent);
		Result.GenerationAfter.Reset();
		return Result;
	};
	auto ReturnInjectedUncommitted = [ValidatedOldCurrent](
		const EAngelscriptCachePointerKind Kind)
	{
		FAngelscriptCacheStoreResult Result = FAngelscriptCacheStoreResult::Failure(
			EAngelscriptCacheStoreError::FaultInjected,
			StageForKind(Kind),
			CategoryForKind(Kind));
		Result.CommitState = EAngelscriptCacheStoreCommitState::NotCommitted;
		SetGenerationBefore(Result, ValidatedOldCurrent);
		Result.GenerationAfter.Reset();
		return Result;
	};
	auto ShouldStopAt = [FaultInjector](
		const EAngelscriptCacheStoreFaultPoint Point)
	{
		return FaultInjector != nullptr && FaultInjector->ShouldStopAt(Point);
	};

	if (IsCancellationRequested())
	{
		return CancelledResult(DestinationKind, ValidatedOldCurrent);
	}
	FAngelscriptCacheStoreResult Result = PreparePointerTemp(
		DestinationTempPath, DestinationKind, NewGenerationId, FileOps);
	if (!Result.IsSuccess())
	{
		return ReturnUncommitted(MoveTemp(Result), DestinationKind);
	}
	if (IsCancellationRequested())
	{
		return ReturnUncommitted(
			CancelledResult(DestinationKind, ValidatedOldCurrent), DestinationKind);
	}

	if (bPublishCurrent && ValidatedOldCurrent.IsSet())
	{
		OwnTemps.Add(PreviousTempPath);
		Result = PreparePointerTemp(
			PreviousTempPath,
			EAngelscriptCachePointerKind::Previous,
			ValidatedOldCurrent.GetValue(),
			FileOps);
		if (!Result.IsSuccess())
		{
			return ReturnUncommitted(
				MoveTemp(Result), EAngelscriptCachePointerKind::Previous);
		}
		if (IsCancellationRequested())
		{
			return ReturnUncommitted(
				CancelledResult(
					EAngelscriptCachePointerKind::Previous, ValidatedOldCurrent),
				EAngelscriptCachePointerKind::Previous);
		}
	}

	if (ShouldStopAt(EAngelscriptCacheStoreFaultPoint::AfterPointerTempsFlush))
	{
		return ReturnInjectedUncommitted(DestinationKind);
	}

	if (bPublishCurrent && ValidatedOldCurrent.IsSet())
	{
		if (ShouldStopAt(EAngelscriptCacheStoreFaultPoint::BeforePreviousReplace))
		{
			return ReturnInjectedUncommitted(
				EAngelscriptCachePointerKind::Previous);
		}
		Result = FileOps.AtomicInstallOrReplacePointer(
			PreviousTempPath, Paths.PreviousPointer);
		if (!Result.IsSuccess())
		{
			return ReturnUncommitted(
				MoveTemp(Result), EAngelscriptCachePointerKind::Previous);
		}
		Result = FileOps.SyncDirectory(Paths.NamespaceRoot);
		if (!Result.IsSuccess())
		{
			return ReturnUncommitted(
				MoveTemp(Result), EAngelscriptCachePointerKind::Previous);
		}
		if (ShouldStopAt(EAngelscriptCacheStoreFaultPoint::AfterPreviousReplace))
		{
			return ReturnInjectedUncommitted(
				EAngelscriptCachePointerKind::Previous);
		}
		if (IsCancellationRequested())
		{
			return ReturnUncommitted(
				CancelledResult(DestinationKind, ValidatedOldCurrent), DestinationKind);
		}
	}
	const EAngelscriptCacheStoreFaultPoint BeforeDestinationPoint = bPublishCurrent
		? EAngelscriptCacheStoreFaultPoint::BeforeCurrentReplace
		: EAngelscriptCacheStoreFaultPoint::BeforePendingReplace;
	if (ShouldStopAt(BeforeDestinationPoint))
	{
		return ReturnInjectedUncommitted(DestinationKind);
	}

	Result = FileOps.AtomicInstallOrReplacePointer(
		DestinationTempPath, DestinationPath);
	if (!Result.IsSuccess())
	{
		Result = SetPointerContext(MoveTemp(Result), DestinationKind);
		const bool bCommitted = RereadPointerSelectsGeneration(
			DestinationPath, DestinationKind, NewGenerationId, FileOps);
		CleanupTemps();
		if (bCommitted)
		{
			return CommittedResult(
				MoveTemp(Result),
				bPublishCurrent
					? EAngelscriptCacheStoreCommitState::CurrentCommitted
					: EAngelscriptCacheStoreCommitState::PendingCommitted,
				ValidatedOldCurrent,
				NewGenerationId);
		}
		Result.CommitState = EAngelscriptCacheStoreCommitState::NotCommitted;
		SetGenerationBefore(Result, ValidatedOldCurrent);
		Result.GenerationAfter.Reset();
		return Result;
	}

	const EAngelscriptCacheStoreCommitState CommitState = bPublishCurrent
		? EAngelscriptCacheStoreCommitState::CurrentCommitted
		: EAngelscriptCacheStoreCommitState::PendingCommitted;
	Result = FileOps.SyncDirectory(Paths.NamespaceRoot);
	if (!Result.IsSuccess())
	{
		Result = SetPointerContext(MoveTemp(Result), DestinationKind);
		return CommittedResult(
			MoveTemp(Result), CommitState,
			ValidatedOldCurrent, NewGenerationId);
	}

	const EAngelscriptCacheStoreFaultPoint AfterDestinationPoint = bPublishCurrent
		? EAngelscriptCacheStoreFaultPoint::AfterCurrentReplace
		: EAngelscriptCacheStoreFaultPoint::AfterPendingReplace;
	if (ShouldStopAt(AfterDestinationPoint))
	{
		return CommittedResult(
			FAngelscriptCacheStoreResult::Failure(
				EAngelscriptCacheStoreError::FaultInjected,
				StageForKind(DestinationKind),
				CategoryForKind(DestinationKind)),
			CommitState,
			ValidatedOldCurrent,
			NewGenerationId);
	}

	if (bPublishCurrent)
	{
		TOptional<FAngelscriptHash256> PendingGenerationId;
		Result = ReadAngelscriptCachePointerSlot(
			Paths,
			EAngelscriptCachePointerKind::PendingColdStart,
			FileOps,
			PendingGenerationId);
		if (!Result.IsSuccess())
		{
			return CommittedResult(
				MoveTemp(Result), CommitState,
				ValidatedOldCurrent, NewGenerationId);
		}

		if (PendingGenerationId.IsSet()
			&& PendingGenerationId.GetValue() == NewGenerationId)
		{
			Result = FileOps.AtomicRemovePointer(Paths.PendingColdStartPointer);
			if (!Result.IsSuccess())
			{
				Result = SetPointerContext(
					MoveTemp(Result), EAngelscriptCachePointerKind::PendingColdStart);
				return CommittedResult(
					MoveTemp(Result), CommitState,
					ValidatedOldCurrent, NewGenerationId);
			}

			Result = FileOps.SyncDirectory(Paths.NamespaceRoot);
			if (!Result.IsSuccess())
			{
				Result = SetPointerContext(
					MoveTemp(Result), EAngelscriptCachePointerKind::PendingColdStart);
				return CommittedResult(
					MoveTemp(Result), CommitState,
					ValidatedOldCurrent, NewGenerationId);
			}
		}
	}
	return CommittedResult(
		FAngelscriptCacheStoreResult::Success(), CommitState,
		ValidatedOldCurrent, NewGenerationId);
}
