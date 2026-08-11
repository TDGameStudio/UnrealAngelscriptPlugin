#include "Cache/AngelscriptCacheDecodedRecord.h"

#include "Cache/AngelscriptCacheArchive.h"
#include "Cache/Private/AngelscriptCacheCanonicalCodec.h"
#include "Cache/Private/AngelscriptCacheRemainingRecordCodec.h"
#include "Cache/Private/AngelscriptCacheSemanticRecordCodec.h"
#include "Cache/Private/AngelscriptCacheTypeSchemaCodec.h"

#include "HAL/UnrealMemory.h"
#include "Misc/ScopeExit.h"
#include "Templates/SharedPointer.h"

namespace
{
	template <typename CoordinateType>
	bool CoordinatesMatch(
		const CoordinateType& Left,
		const CoordinateType& Right)
	{
		return Left.Field == Right.Field
			&& Left.PrimaryIndex == Right.PrimaryIndex
			&& Left.SecondaryIndex == Right.SecondaryIndex
			&& Left.TertiaryIndex == Right.TertiaryIndex;
	}

	template <typename EntryRangeType, typename CoordinateType>
	TOptional<uint64> FindOffsetInEntries(
		const EntryRangeType& Entries,
		const CoordinateType& Coordinate)
	{
		for (const auto& Entry : Entries)
		{
			if (CoordinatesMatch(Entry.Coordinate, Coordinate))
			{
				return Entry.Offset;
			}
		}
		return {};
	}

	FAngelscriptCacheValidationResult FactoryFailure(
		const EAngelscriptCacheValidationError Error,
		const EAngelscriptCacheRecordKind Kind,
		const EAngelscriptCacheValidationStage Stage,
		const uint64 Offset = 0)
	{
		return FAngelscriptCacheValidationResult::AtStage(Error, Kind, Stage, Offset);
	}

	using FDecodedController = SharedPointerInternals::TIntrusiveReferenceController<
		FAngelscriptDecodedCacheRecord,
		ESPMode::ThreadSafe>;

	constexpr SIZE_T GetDecodedControllerNewAlignment()
	{
		return alignof(FDecodedController) > __STDCPP_DEFAULT_NEW_ALIGNMENT__
			? alignof(FDecodedController)
			: (sizeof(FDecodedController) <= 8
				? SIZE_T(8)
				: SIZE_T(__STDCPP_DEFAULT_NEW_ALIGNMENT__));
	}

	uint64 GetDecodedControllerCharge()
	{
		// QuantizeSize is a container-slack hint, not a promise that it equals
		// GetAllocSize for a concrete allocation.  In particular, UE's active
		// allocator may return a larger usable block for the same operator-new
		// request.  Calibrate that fixed request once so the hostile-input Budget
		// is charged before the real controller allocation and never undercounts
		// allocator-owned bytes.
		static const uint64 ExactCharge = []
		{
			const uint64 QuantizedCharge = static_cast<uint64>(FMemory::QuantizeSize(
				sizeof(FDecodedController),
				GetDecodedControllerNewAlignment()));
			void* Allocation = FMemory::Malloc(
				sizeof(FDecodedController),
				static_cast<uint32>(GetDecodedControllerNewAlignment()));
			check(Allocation != nullptr);
			const uint64 ActualCharge = static_cast<uint64>(
				FMemory::GetAllocSize(Allocation));
			FMemory::Free(Allocation);
			return ActualCharge >= sizeof(FDecodedController)
				? ActualCharge
				: QuantizedCharge;
		}();
		return ExactCharge;
	}
}

FAngelscriptDecodedCacheRecord::FAngelscriptDecodedCacheRecord(
	FPrivateConstructionToken,
	const FAngelscriptCacheRecordId& InRecordId,
	TArray<uint8>&& InCanonicalPayload,
	FRecordVariant&& InRecord)
	: RecordId(InRecordId)
	, CanonicalPayload(MoveTemp(InCanonicalPayload))
	, Record(MoveTemp(InRecord))
{
}

FAngelscriptDecodedCacheRecord::~FAngelscriptDecodedCacheRecord() = default;

FAngelscriptCacheValidationResult FAngelscriptDecodedCacheRecord::TryDecode(
	const FAngelscriptCacheRecordId& DeclaredRecordId,
	const TConstArrayView<uint8> CanonicalPayload,
	const FAngelscriptCacheReadLimits& Limits,
	FAngelscriptCacheReadBudget& Budget,
	TOptional<TSharedRef<const FAngelscriptDecodedCacheRecord, ESPMode::ThreadSafe>>& OutRecord)
{
	FAngelscriptCacheReadBudget::FDecodedCandidateTransaction Candidate =
		Budget.BeginDecodedCandidateTransaction(Limits);
	return TryDecodeInternal(
		DeclaredRecordId,
		CanonicalPayload,
		Limits,
		Budget,
		Candidate,
		true,
#if WITH_ANGELSCRIPT_UNITTESTS
		nullptr,
#endif
		OutRecord);
}

FAngelscriptCacheValidationResult FAngelscriptDecodedCacheRecord::TryDecodeInternal(
	const FAngelscriptCacheRecordId& DeclaredRecordId,
	const TConstArrayView<uint8> CanonicalPayload,
	const FAngelscriptCacheReadLimits& Limits,
	FAngelscriptCacheReadBudget& Budget,
	FAngelscriptCacheReadBudget::FDecodedCandidateTransaction& Candidate,
	const bool bPromoteCandidate,
#if WITH_ANGELSCRIPT_UNITTESTS
	FAngelscriptCacheTypeSchemaAllocationProbeForTests* Probe,
#endif
	TOptional<TSharedRef<const FAngelscriptDecodedCacheRecord, ESPMode::ThreadSafe>>& OutRecord)
{
	// The input view is allowed to alias the old output handle. Keep that handle
	// alive until the view has been checked and copied into the new candidate.
	const TOptional<TSharedRef<const FAngelscriptDecodedCacheRecord, ESPMode::ThreadSafe>>
		InputLifetimeGuard = OutRecord;
	OutRecord.Reset();

#if WITH_ANGELSCRIPT_UNITTESTS
	if (Probe != nullptr)
	{
		Probe->BeginDecodeForTests();
	}
	ON_SCOPE_EXIT
	{
		if (Probe != nullptr)
		{
			Probe->CloseLiveObservationForTests();
		}
	};
#endif

	FAngelscriptCacheRecordId ComputedRecordId;
	const FAngelscriptCacheValidationResult RecordIdResult =
		FAngelscriptCacheRecordArchive::TryBuildRecordId(
			DeclaredRecordId.Kind,
			CanonicalPayload,
			ComputedRecordId);
	if (!RecordIdResult.IsSuccess())
	{
		return FactoryFailure(
			RecordIdResult.Error,
			DeclaredRecordId.Kind,
			EAngelscriptCacheValidationStage::PayloadDecode);
	}
	if (!(ComputedRecordId == DeclaredRecordId))
	{
		return FactoryFailure(
			EAngelscriptCacheValidationError::RecordIdMismatch,
			DeclaredRecordId.Kind,
			EAngelscriptCacheValidationStage::PayloadDecode);
	}
	if (static_cast<uint64>(CanonicalPayload.Num())
		> Limits.MaxCanonicalRecordPayloadBytes)
	{
		return FactoryFailure(
			EAngelscriptCacheValidationError::BudgetExceeded,
			DeclaredRecordId.Kind,
			EAngelscriptCacheValidationStage::PayloadDecode);
	}

	const auto ExtendCandidate = [&](const uint64 Bytes, const uint64 Offset)
		-> FAngelscriptCacheValidationResult
	{
		using EExtend = FAngelscriptCacheReadBudget::EDecodedCandidateExtendResult;
		switch (Candidate.TryExtend(Bytes))
		{
		case EExtend::Success:
			return {};
		case EExtend::BudgetExceeded:
#if WITH_ANGELSCRIPT_UNITTESTS
			if (Probe != nullptr)
			{
				Probe->RecordRejectedReservationForTests();
			}
#endif
			return FactoryFailure(
				EAngelscriptCacheValidationError::BudgetExceeded,
				DeclaredRecordId.Kind,
				EAngelscriptCacheValidationStage::PayloadDecode,
				Offset);
		case EExtend::Overflow:
		case EExtend::InvalidState:
		default:
			return FactoryFailure(
				EAngelscriptCacheValidationError::Overflow,
				DeclaredRecordId.Kind,
				EAngelscriptCacheValidationStage::PayloadDecode,
				Offset);
		}
	};

#if WITH_ANGELSCRIPT_UNITTESTS
	const auto RecordAcceptedAllocation = [Probe](
		const int32 RequestedElementCount,
		const uint64 ElementSize,
		const uint64 ElementAlignment,
		const uint64 ReservedCapacity,
		const uint64 AllocatedBytes,
		const uint64 FieldOffset) -> bool
	{
		if (Probe == nullptr)
		{
			return false;
		}
		using namespace AngelscriptCacheCanonicalCodecTestHooks;
		return Probe->RecordAcceptedAllocationForTests({
			EAllocationSite::TypedArrayElements,
			EAllocationEventPhase::AllocationSucceeded,
			FieldOffset,
			RequestedElementCount,
			static_cast<int32>(ReservedCapacity),
			AllocatedBytes,
			AllocatedBytes,
			ElementSize,
			ElementAlignment});
	};
#endif

	const uint64 ControllerCharge = GetDecodedControllerCharge();
	const FAngelscriptCacheValidationResult ControllerChargeResult =
		ExtendCandidate(ControllerCharge, 0);
	if (!ControllerChargeResult.IsSuccess())
	{
		return ControllerChargeResult;
	}

	FRecordVariant RecordVariant = [&]() -> FRecordVariant
	{
		switch (DeclaredRecordId.Kind)
		{
		case EAngelscriptCacheRecordKind::SourceIndex:
			return FRecordVariant(TInPlaceType<FSourceIndexRecord>{});
		case EAngelscriptCacheRecordKind::ModuleInterface:
			return FRecordVariant(TInPlaceType<FModuleInterfaceRecord>{});
		case EAngelscriptCacheRecordKind::TypeSchema:
			return FRecordVariant(TInPlaceType<FTypeSchemaRecord>{});
		case EAngelscriptCacheRecordKind::ModuleState:
			return FRecordVariant(TInPlaceType<FModuleStateRecord>{});
		case EAngelscriptCacheRecordKind::FunctionBody:
			return FRecordVariant(TInPlaceType<FFunctionBodyRecord>{});
		case EAngelscriptCacheRecordKind::DebugSidecar:
			return FRecordVariant(TInPlaceType<FDebugSidecarRecord>{});
		case EAngelscriptCacheRecordKind::ModuleSnapshot:
			return FRecordVariant(TInPlaceType<FModuleSnapshotRecord>{});
		default:
			checkNoEntry();
			return FRecordVariant(TInPlaceType<FSourceIndexRecord>{});
		}
	}();

	TSharedRef<FAngelscriptDecodedCacheRecord, ESPMode::ThreadSafe> CandidateRecord =
		MakeShared<FAngelscriptDecodedCacheRecord, ESPMode::ThreadSafe>(
			FPrivateConstructionToken{},
			ComputedRecordId,
			TArray<uint8>{},
			MoveTemp(RecordVariant));

#if WITH_ANGELSCRIPT_UNITTESTS
	if (Probe != nullptr)
	{
		++Probe->DecodedRecordControllerAllocationCount;
		Probe->DecodedRecordControllerAllocatedBytes += ControllerCharge;
	}
	if (RecordAcceptedAllocation(
		1,
		sizeof(FDecodedController),
		alignof(FDecodedController),
		1,
		ControllerCharge,
		0))
	{
		return FactoryFailure(
			EAngelscriptCacheValidationError::Overflow,
			DeclaredRecordId.Kind,
			EAngelscriptCacheValidationStage::PayloadDecode);
	}
#endif

	int32 PayloadReservedCapacity = 0;
	uint64 PayloadAllocatedBytes = 0;
	if (!AngelscriptCacheCanonicalCodec_Private::TryCalculateArrayReserveBytes<uint8>(
		CanonicalPayload.Num(),
		PayloadReservedCapacity,
		PayloadAllocatedBytes))
	{
		return FactoryFailure(
			EAngelscriptCacheValidationError::Overflow,
			DeclaredRecordId.Kind,
			EAngelscriptCacheValidationStage::PayloadDecode);
	}
	if (PayloadAllocatedBytes != 0)
	{
		const FAngelscriptCacheValidationResult PayloadChargeResult =
			ExtendCandidate(PayloadAllocatedBytes, 0);
		if (!PayloadChargeResult.IsSuccess())
		{
			return PayloadChargeResult;
		}
		CandidateRecord->CanonicalPayload.Reserve(CanonicalPayload.Num());
		const uint64 ActualPayloadBytes = static_cast<uint64>(
			CandidateRecord->CanonicalPayload.GetAllocatedSize());
		if (ActualPayloadBytes != PayloadAllocatedBytes)
		{
			CandidateRecord->CanonicalPayload.Empty();
			return FactoryFailure(
				EAngelscriptCacheValidationError::Overflow,
				DeclaredRecordId.Kind,
				EAngelscriptCacheValidationStage::PayloadDecode);
		}
#if WITH_ANGELSCRIPT_UNITTESTS
		if (RecordAcceptedAllocation(
			CanonicalPayload.Num(),
			sizeof(uint8),
			alignof(uint8),
			static_cast<uint64>(PayloadReservedCapacity),
			ActualPayloadBytes,
			0))
		{
			CandidateRecord->CanonicalPayload.Empty();
			return FactoryFailure(
				EAngelscriptCacheValidationError::Overflow,
				DeclaredRecordId.Kind,
				EAngelscriptCacheValidationStage::PayloadDecode);
		}
#endif
		CandidateRecord->CanonicalPayload.Append(
			CanonicalPayload.GetData(),
			CanonicalPayload.Num());
	}

	struct FFactoryChargeContext final
	{
		FAngelscriptCacheReadBudget::FDecodedCandidateTransaction* Transaction = nullptr;
#if WITH_ANGELSCRIPT_UNITTESTS
		FAngelscriptCacheTypeSchemaAllocationProbeForTests* Probe = nullptr;
#endif
	};
	FFactoryChargeContext ChargeContext{
		&Candidate,
#if WITH_ANGELSCRIPT_UNITTESTS
		Probe,
#endif
	};
	const AngelscriptCacheCanonicalCodec_Private::FDecodedChargeSink ChargeSink(
		&ChargeContext,
		[](void* Context, const uint64 Bytes)
		{
			using ECharge =
				AngelscriptCacheCanonicalCodec_Private::EDecodedChargeResult;
			using EExtend = FAngelscriptCacheReadBudget::EDecodedCandidateExtendResult;
			auto& FactoryContext = *static_cast<FFactoryChargeContext*>(Context);
			check(FactoryContext.Transaction != nullptr);
			switch (FactoryContext.Transaction->TryExtend(Bytes))
			{
			case EExtend::Success:
				return ECharge::Accepted;
			case EExtend::BudgetExceeded:
#if WITH_ANGELSCRIPT_UNITTESTS
				if (FactoryContext.Probe != nullptr)
				{
					FactoryContext.Probe->RecordRejectedReservationForTests();
				}
#endif
				return ECharge::BudgetExceeded;
			case EExtend::Overflow:
			case EExtend::InvalidState:
			default:
				return ECharge::Overflow;
			}
		}
#if WITH_ANGELSCRIPT_UNITTESTS
		, {
			&ChargeContext,
			nullptr,
			[](void* Context,
				const AngelscriptCacheCanonicalCodecTestHooks::FAllocationEvent& Event)
			{
				auto& FactoryContext = *static_cast<FFactoryChargeContext*>(Context);
				return FactoryContext.Probe != nullptr
					&& FactoryContext.Probe->RecordAcceptedAllocationForTests(Event);
			}}
#endif
	);

	FAngelscriptCacheValidationResult DecodeResult;
	switch (DeclaredRecordId.Kind)
	{
	case EAngelscriptCacheRecordKind::SourceIndex:
	{
		FSourceIndexRecord* SourceIndex = CandidateRecord->Record.TryGet<FSourceIndexRecord>();
		check(SourceIndex != nullptr);
		DecodeResult =
			AngelscriptCacheSemanticRecords_Private::FDecodedRecordCodecBridge::
				TryDecodeSourceIndex(
					CandidateRecord->CanonicalPayload,
					Limits,
					Budget,
					ChargeSink,
					SourceIndex->Value,
					SourceIndex->Offsets);
		break;
	}
	case EAngelscriptCacheRecordKind::ModuleInterface:
	{
		FModuleInterfaceRecord* ModuleInterface =
			CandidateRecord->Record.TryGet<FModuleInterfaceRecord>();
		check(ModuleInterface != nullptr);
		DecodeResult =
			AngelscriptCacheSemanticRecords_Private::FDecodedRecordCodecBridge::
				TryDecodeModuleInterface(
					CandidateRecord->CanonicalPayload,
					Limits,
					Budget,
					ChargeSink,
					ModuleInterface->Value,
					ModuleInterface->Offsets);
		break;
	}
	case EAngelscriptCacheRecordKind::ModuleState:
	{
		FModuleStateRecord* ModuleState =
			CandidateRecord->Record.TryGet<FModuleStateRecord>();
		check(ModuleState != nullptr);
		DecodeResult =
			AngelscriptCacheRemainingRecords_Private::FDecodedRecordCodecBridge::
				TryDecodeModuleState(
					CandidateRecord->CanonicalPayload,
					Limits,
					Budget,
					ChargeSink,
					ModuleState->Value,
					ModuleState->Offsets);
		break;
	}
	case EAngelscriptCacheRecordKind::TypeSchema:
	{
		FTypeSchemaRecord* TypeSchema = CandidateRecord->Record.TryGet<FTypeSchemaRecord>();
		check(TypeSchema != nullptr);
		DecodeResult =
			AngelscriptCacheTypeSchema_Private::FDecodedRecordCodecBridge::TryDecodeTypeSchema(
				CandidateRecord->CanonicalPayload,
				Limits,
				Budget,
				ChargeSink,
#if WITH_ANGELSCRIPT_UNITTESTS
				Probe,
#endif
				TypeSchema->Value,
				TypeSchema->Offsets);
		break;
	}
	case EAngelscriptCacheRecordKind::DebugSidecar:
	{
		FDebugSidecarRecord* DebugSidecar =
			CandidateRecord->Record.TryGet<FDebugSidecarRecord>();
		check(DebugSidecar != nullptr);
		DecodeResult =
			AngelscriptCacheRemainingRecords_Private::FDecodedRecordCodecBridge::
				TryDecodeDebugSidecar(
					CandidateRecord->CanonicalPayload,
					Limits,
					Budget,
					ChargeSink,
					DebugSidecar->Value,
					DebugSidecar->Offsets);
		break;
	}
	case EAngelscriptCacheRecordKind::FunctionBody:
	{
		FFunctionBodyRecord* FunctionBody =
			CandidateRecord->Record.TryGet<FFunctionBodyRecord>();
		check(FunctionBody != nullptr);
		DecodeResult =
			AngelscriptCacheRemainingRecords_Private::FDecodedRecordCodecBridge::
				TryDecodeFunctionBody(
					CandidateRecord->CanonicalPayload,
					Limits,
					Budget,
					ChargeSink,
					FunctionBody->Value,
					FunctionBody->Offsets);
		break;
	}
	case EAngelscriptCacheRecordKind::ModuleSnapshot:
	{
		FModuleSnapshotRecord* ModuleSnapshot =
			CandidateRecord->Record.TryGet<FModuleSnapshotRecord>();
		check(ModuleSnapshot != nullptr);
		DecodeResult =
			AngelscriptCacheRemainingRecords_Private::FDecodedRecordCodecBridge::
				TryDecodeModuleSnapshot(
					CandidateRecord->CanonicalPayload,
					Limits,
					Budget,
					ChargeSink,
					ModuleSnapshot->Value,
					ModuleSnapshot->Offsets);
		break;
	}
	default:
		DecodeResult = FactoryFailure(
			EAngelscriptCacheValidationError::UnexpectedRecord,
			DeclaredRecordId.Kind,
			EAngelscriptCacheValidationStage::PayloadDecode);
		break;
	}
	if (!DecodeResult.IsSuccess())
	{
		return DecodeResult;
	}

	if (bPromoteCandidate)
	{
#if WITH_ANGELSCRIPT_UNITTESTS
		if (Probe != nullptr)
		{
			FAngelscriptCacheTypeSchemaProbeEventForTests Promotion;
			Promotion.Kind =
				EAngelscriptCacheTypeSchemaProbeEventKindForTests::CandidatePromotion;
			Promotion.SequenceOrdinal = Probe->TotalAllocationAttempts;
			Promotion.TemporaryBytesBefore = Budget.GetTemporaryResidentDecodedBytes();
			Promotion.TotalDecodedBytes = Budget.GetDecodedBytes();
			Promotion.AcceptedAllocationEventCount =
				static_cast<int32>(Probe->TotalAllocationAttempts);
			Promotion.AllocationAttemptCount = Probe->TotalAllocationAttempts;
			Promotion.AllocatedBytes = Probe->TotalAllocatedBytes;
			Promotion.bHandleWasObservable = false;
			if (!Candidate.PromoteToRetained())
			{
				return FactoryFailure(
					EAngelscriptCacheValidationError::Overflow,
					DeclaredRecordId.Kind,
					EAngelscriptCacheValidationStage::LocalSemantic);
			}
			Promotion.ResidentBytesAfter = Budget.GetResidentDecodedBytes();
			Probe->Record(Promotion);
		}
		else
#endif
		if (!Candidate.PromoteToRetained())
		{
			return FactoryFailure(
				EAngelscriptCacheValidationError::Overflow,
				DeclaredRecordId.Kind,
				EAngelscriptCacheValidationStage::LocalSemantic);
		}
	}

	const TSharedRef<const FAngelscriptDecodedCacheRecord, ESPMode::ThreadSafe>
		PublishedRecord = CandidateRecord;
	OutRecord = PublishedRecord;
	return {};
}

FAngelscriptDecodedCacheRecordBatch::FAngelscriptDecodedCacheRecordBatch(
	FAngelscriptCacheReadBudget& InBudget,
	const FAngelscriptCacheReadLimits& InLimits)
	: Limits(InLimits)
	, Budget(InBudget)
	, Candidate(InBudget.BeginDecodedCandidateTransaction(InLimits))
{
}

FAngelscriptCacheValidationResult FAngelscriptDecodedCacheRecordBatch::TryDecode(
	const FAngelscriptCacheRecordId& DeclaredRecordId,
	const TConstArrayView<uint8> CanonicalPayload,
	TOptional<FAngelscriptDecodedCacheRecordHandle>& OutRecord)
{
	if (!Candidate.IsOpen())
	{
		OutRecord.Reset();
		return FactoryFailure(EAngelscriptCacheValidationError::Overflow,
			DeclaredRecordId.Kind,
			EAngelscriptCacheValidationStage::PayloadDecode);
	}
	return FAngelscriptDecodedCacheRecord::TryDecodeInternal(
		DeclaredRecordId, CanonicalPayload, Limits, Budget, Candidate, false,
#if WITH_ANGELSCRIPT_UNITTESTS
		nullptr,
#endif
		OutRecord);
}

bool FAngelscriptDecodedCacheRecordBatch::PromoteToRetained()
{
	return Candidate.PromoteToRetained();
}

bool FAngelscriptDecodedCacheRecordBatch::IsOpen() const
{
	return Candidate.IsOpen();
}

const FAngelscriptCachedSourceIndex* FAngelscriptDecodedCacheRecord::TryGetSourceIndex() const
{
	const FSourceIndexRecord* Alternative = Record.TryGet<FSourceIndexRecord>();
	return Alternative != nullptr ? &Alternative->Value : nullptr;
}

const FAngelscriptCachedModuleInterface*
FAngelscriptDecodedCacheRecord::TryGetModuleInterface() const
{
	const FModuleInterfaceRecord* Alternative = Record.TryGet<FModuleInterfaceRecord>();
	return Alternative != nullptr ? &Alternative->Value : nullptr;
}

const FAngelscriptCachedTypeSchema* FAngelscriptDecodedCacheRecord::TryGetTypeSchema() const
{
	const FTypeSchemaRecord* Alternative = Record.TryGet<FTypeSchemaRecord>();
	return Alternative != nullptr ? &Alternative->Value : nullptr;
}

const FAngelscriptCachedModuleState* FAngelscriptDecodedCacheRecord::TryGetModuleState() const
{
	const FModuleStateRecord* Alternative = Record.TryGet<FModuleStateRecord>();
	return Alternative != nullptr ? &Alternative->Value : nullptr;
}

const FAngelscriptCachedFunctionBody* FAngelscriptDecodedCacheRecord::TryGetFunctionBody() const
{
	const FFunctionBodyRecord* Alternative = Record.TryGet<FFunctionBodyRecord>();
	return Alternative != nullptr ? &Alternative->Value : nullptr;
}

const FAngelscriptCachedDebugSidecar* FAngelscriptDecodedCacheRecord::TryGetDebugSidecar() const
{
	const FDebugSidecarRecord* Alternative = Record.TryGet<FDebugSidecarRecord>();
	return Alternative != nullptr ? &Alternative->Value : nullptr;
}

const FAngelscriptCachedModuleSnapshot*
FAngelscriptDecodedCacheRecord::TryGetModuleSnapshot() const
{
	const FModuleSnapshotRecord* Alternative = Record.TryGet<FModuleSnapshotRecord>();
	return Alternative != nullptr ? &Alternative->Value : nullptr;
}

TOptional<uint64> FAngelscriptDecodedCacheRecord::FindSourceIndexOffsetInStorage(
	const FSourceIndexCapturedOffsetStorage& Offsets,
	const FAngelscriptSourceIndexFieldCoordinate& Coordinate)
{
	const auto FindOptional = [&Coordinate](const auto& Entry) -> TOptional<uint64>
	{
		return Entry.IsSet() && CoordinatesMatch(Entry->Coordinate, Coordinate)
			? TOptional<uint64>{Entry->Offset}
			: TOptional<uint64>{};
	};
	if (const TOptional<uint64> Found =
		FindOffsetInEntries(Offsets.HeaderOffsets, Coordinate); Found.IsSet())
	{
		return Found;
	}
	if (const TOptional<uint64> Found =
		FindOffsetInEntries(Offsets.Discovery.Fields, Coordinate); Found.IsSet())
	{
		return Found;
	}
	for (const auto& Option : Offsets.Discovery.Options)
	{
		if (const TOptional<uint64> Found =
			FindOffsetInEntries(Option.Fields, Coordinate); Found.IsSet())
		{
			return Found;
		}
	}
	for (const auto& Mount : Offsets.Mounts)
	{
		if (const TOptional<uint64> Found =
			FindOffsetInEntries(Mount.Fields, Coordinate); Found.IsSet())
		{
			return Found;
		}
		for (const auto& Option : Mount.Options)
		{
			if (const TOptional<uint64> Found =
				FindOffsetInEntries(Option.Fields, Coordinate); Found.IsSet())
			{
				return Found;
			}
		}
	}
	for (const auto& Provider : Offsets.Providers)
	{
		if (const TOptional<uint64> Found =
			FindOffsetInEntries(Provider.Fields, Coordinate); Found.IsSet())
		{
			return Found;
		}
		for (const auto& Entry : {
			&Provider.VersionFingerprint,
			&Provider.ConfigurationFingerprint,
			&Provider.ContentFingerprint})
		{
			if (const TOptional<uint64> Found = FindOptional(*Entry); Found.IsSet())
			{
				return Found;
			}
		}
	}
	for (const auto& Hook : Offsets.Hooks)
	{
		if (const TOptional<uint64> Found =
			FindOffsetInEntries(Hook.Fields, Coordinate); Found.IsSet())
		{
			return Found;
		}
		for (const auto& Entry : {
			&Hook.VersionFingerprint,
			&Hook.ConfigurationFingerprint,
			&Hook.ContentFingerprint})
		{
			if (const TOptional<uint64> Found = FindOptional(*Entry); Found.IsSet())
			{
				return Found;
			}
		}
	}
	for (const auto& File : Offsets.Files)
	{
		if (const TOptional<uint64> Found =
			FindOffsetInEntries(File.Fields, Coordinate); Found.IsSet())
		{
			return Found;
		}
		if (const TOptional<uint64> Found =
			FindOptional(File.GeneratedSourceKey); Found.IsSet())
		{
			return Found;
		}
		if (const TOptional<uint64> Found =
			FindOptional(File.GeneratedConfigurationFingerprint); Found.IsSet())
		{
			return Found;
		}
	}
	for (const auto& Input : Offsets.Inputs)
	{
		if (const TOptional<uint64> Found =
			FindOffsetInEntries(Input.Fields, Coordinate); Found.IsSet())
		{
			return Found;
		}
		if (const TOptional<uint64> Found =
			FindOptional(Input.TargetStableKey); Found.IsSet())
		{
			return Found;
		}
	}
	for (const auto& Edge : Offsets.Edges)
	{
		if (const TOptional<uint64> Found =
			FindOffsetInEntries(Edge.Fields, Coordinate); Found.IsSet())
		{
			return Found;
		}
		if (const TOptional<uint64> Found =
			FindOptional(Edge.SemanticOrdinal); Found.IsSet())
		{
			return Found;
		}
	}
	for (const auto& Scope : Offsets.IneligibleScopes)
	{
		if (const TOptional<uint64> Found =
			FindOffsetInEntries(Scope.Fields, Coordinate); Found.IsSet())
		{
			return Found;
		}
		if (const TOptional<uint64> Found =
			FindOptional(Scope.ObservedFingerprint); Found.IsSet())
		{
			return Found;
		}
	}
	return {};
}

TOptional<uint64> FAngelscriptDecodedCacheRecord::FindCapturedOffset(
	const FAngelscriptSourceIndexFieldCoordinate& Coordinate) const
{
	const FSourceIndexRecord* Alternative = Record.TryGet<FSourceIndexRecord>();
	if (Alternative == nullptr)
	{
		return {};
	}
	return FindSourceIndexOffsetInStorage(Alternative->Offsets, Coordinate);
}

TOptional<uint64> FAngelscriptDecodedCacheRecord::FindModuleInterfaceOffsetInStorage(
	const FModuleInterfaceCapturedOffsetStorage& Offsets,
	const FAngelscriptModuleInterfaceFieldCoordinate& Coordinate)
{
	const auto FindOptional = [&Coordinate](const auto& Entry) -> TOptional<uint64>
	{
		return Entry.IsSet() && CoordinatesMatch(Entry->Coordinate, Coordinate)
			? TOptional<uint64>{Entry->Offset}
			: TOptional<uint64>{};
	};
	const auto FindDataType = [&Coordinate, &FindOptional](
		const auto& Self,
		const FModuleInterfaceCapturedOffsetStorage::FDataTypeOffsets& Type)
		-> TOptional<uint64>
	{
		if (const TOptional<uint64> Found =
			FindOffsetInEntries(Type.Fields, Coordinate); Found.IsSet())
		{
			return Found;
		}
		for (const auto& Entry : {
			&Type.Reference,
			&Type.ReferenceKind,
			&Type.ReferenceStableKey,
			&Type.ReferenceExpectedAbi})
		{
			if (const TOptional<uint64> Found = FindOptional(*Entry); Found.IsSet())
			{
				return Found;
			}
		}
		for (const auto& SubType : Type.SubTypes)
		{
			if (const TOptional<uint64> Found = Self(Self, SubType); Found.IsSet())
			{
				return Found;
			}
		}
		return {};
	};
	if (const TOptional<uint64> Found =
		FindOffsetInEntries(Offsets.HeaderOffsets, Coordinate); Found.IsSet())
	{
		return Found;
	}
	if (const TOptional<uint64> Found =
		FindOffsetInEntries(Offsets.CanonicalNamespaces, Coordinate); Found.IsSet())
	{
		return Found;
	}
	for (const auto& Declaration : Offsets.Declarations)
	{
		if (const TOptional<uint64> Found =
			FindOffsetInEntries(Declaration.Fields, Coordinate); Found.IsSet())
		{
			return Found;
		}
		if (const TOptional<uint64> Found =
			FindOffsetInEntries(Declaration.IdentityTraits, Coordinate); Found.IsSet())
		{
			return Found;
		}
		if (const TOptional<uint64> Found =
			FindOptional(Declaration.CanonicalTypeSpelling); Found.IsSet())
		{
			return Found;
		}
		if (Declaration.DeclaredType.IsSet())
		{
			if (const TOptional<uint64> Found =
				FindDataType(FindDataType, Declaration.DeclaredType.GetValue());
				Found.IsSet())
			{
				return Found;
			}
		}
		for (const auto& Parameter : Declaration.Parameters)
		{
			if (const TOptional<uint64> Found =
				FindOffsetInEntries(Parameter.Fields, Coordinate); Found.IsSet())
			{
				return Found;
			}
			if (const TOptional<uint64> Found =
				FindDataType(FindDataType, Parameter.Type); Found.IsSet())
			{
				return Found;
			}
			if (const TOptional<uint64> Found =
				FindOptional(Parameter.DefaultExpression); Found.IsSet())
			{
				return Found;
			}
		}
		for (const auto& Metadata : Declaration.Metadata)
		{
			if (const TOptional<uint64> Found =
				FindOffsetInEntries(Metadata.Fields, Coordinate); Found.IsSet())
			{
				return Found;
			}
		}
		for (const auto& Slot : Declaration.Slots)
		{
			if (const TOptional<uint64> Found =
				FindOffsetInEntries(Slot.Fields, Coordinate); Found.IsSet())
			{
				return Found;
			}
		}
	}
	for (const auto& Import : Offsets.Imports)
	{
		if (const TOptional<uint64> Found =
			FindOffsetInEntries(Import.Fields, Coordinate); Found.IsSet())
		{
			return Found;
		}
		for (const auto& Slot : Import.Slots)
		{
			if (const TOptional<uint64> Found =
				FindOffsetInEntries(Slot.Fields, Coordinate); Found.IsSet())
			{
				return Found;
			}
		}
	}
	for (const auto& Dependency : Offsets.Dependencies)
	{
		if (const TOptional<uint64> Found =
			FindOffsetInEntries(Dependency.Fields, Coordinate); Found.IsSet())
		{
			return Found;
		}
		if (const TOptional<uint64> Found =
			FindOptional(Dependency.ExpectedContentOrValue); Found.IsSet())
		{
			return Found;
		}
	}
	return {};
}

TOptional<uint64> FAngelscriptDecodedCacheRecord::FindCapturedOffset(
	const FAngelscriptModuleInterfaceFieldCoordinate& Coordinate) const
{
	const FModuleInterfaceRecord* Alternative = Record.TryGet<FModuleInterfaceRecord>();
	if (Alternative == nullptr)
	{
		return {};
	}
	return FindModuleInterfaceOffsetInStorage(Alternative->Offsets, Coordinate);
}

TOptional<uint64> FAngelscriptDecodedCacheRecord::FindCapturedOffset(
	const FAngelscriptTypeSchemaFieldCoordinate& Coordinate) const
{
	const FTypeSchemaRecord* Alternative = Record.TryGet<FTypeSchemaRecord>();
	if (Alternative == nullptr)
	{
		return {};
	}

	const FTypeSchemaCapturedOffsetStorage& Offsets = Alternative->Offsets;
	const TConstArrayView<FTypeSchemaCapturedOffsetStorage::FEntry> Groups[] = {
		Offsets.FlatHeaderOffsets,
		Offsets.ParallelMetadataOffsets,
		Offsets.ParallelRelationOffsets,
		Offsets.ParallelLayoutInputOffsets,
		Offsets.ParallelPropertyOffsets,
		Offsets.ParallelNestedPropertyOffsets,
		Offsets.ParallelMethodOffsets,
		Offsets.ParallelVftOffsets,
		Offsets.ParallelBehaviorOffsets,
		Offsets.FlatSelectedArmOffsets,
		Offsets.ParallelReflectionOffsets,
		Offsets.ParallelDependencyOffsets,
	};
	if (Offsets.ReflectionOffset.IsSet()
		&& CoordinatesMatch(Offsets.ReflectionOffset->Coordinate, Coordinate))
	{
		return Offsets.ReflectionOffset->Offset;
	}
	if (Offsets.ReflectionKindOffset.IsSet()
		&& CoordinatesMatch(Offsets.ReflectionKindOffset->Coordinate, Coordinate))
	{
		return Offsets.ReflectionKindOffset->Offset;
	}
	if (Offsets.ClassReflectionFlagsOffset.IsSet()
		&& CoordinatesMatch(
			Offsets.ClassReflectionFlagsOffset->Coordinate, Coordinate))
	{
		return Offsets.ClassReflectionFlagsOffset->Offset;
	}
	for (const TConstArrayView<FTypeSchemaCapturedOffsetStorage::FEntry> Group : Groups)
	{
		const TOptional<uint64> Found = FindOffsetInEntries(Group, Coordinate);
		if (Found.IsSet())
		{
			return Found;
		}
	}
	return {};
}

TOptional<uint64> FAngelscriptDecodedCacheRecord::FindCapturedOffset(
	const FAngelscriptModuleStateFieldCoordinate& Coordinate) const
{
	const FModuleStateRecord* Alternative = Record.TryGet<FModuleStateRecord>();
	if (Alternative == nullptr)
	{
		return {};
	}
	const FModuleStateCapturedOffsetStorage& Offsets = Alternative->Offsets;
	if (const TOptional<uint64> Header =
		FindOffsetInEntries(Offsets.HeaderOffsets, Coordinate); Header.IsSet())
	{
		return Header;
	}
	const auto FindOptional = [&Coordinate](const auto& Entry) -> TOptional<uint64>
	{
		return Entry.IsSet() && CoordinatesMatch(Entry->Coordinate, Coordinate)
			? TOptional<uint64>{Entry->Offset}
			: TOptional<uint64>{};
	};
	const auto FindDataType = [&Coordinate, &FindOptional](
		const auto& Self,
		const FModuleStateCapturedOffsetStorage::FDataTypeOffsets& Type)
		-> TOptional<uint64>
	{
		if (const TOptional<uint64> Base =
			FindOffsetInEntries(Type.Fields, Coordinate); Base.IsSet())
		{
			return Base;
		}
		const TOptional<FModuleStateCapturedOffsetStorage::FEntry> OptionalEntries[] = {
			Type.Reference,
			Type.ReferenceKind,
			Type.ReferenceStableKey,
			Type.ReferenceExpectedAbi,
		};
		for (const auto& Entry : OptionalEntries)
		{
			if (const TOptional<uint64> Found = FindOptional(Entry); Found.IsSet())
			{
				return Found;
			}
		}
		for (const auto& SubType : Type.SubTypes)
		{
			if (const TOptional<uint64> Found = Self(Self, SubType); Found.IsSet())
			{
				return Found;
			}
		}
		return {};
	};
	const auto FindDependency = [&Coordinate, &FindOptional](
		const FModuleStateCapturedOffsetStorage::FDependencyOffsets& Dependency)
		-> TOptional<uint64>
	{
		if (const TOptional<uint64> Base =
			FindOffsetInEntries(Dependency.Fields, Coordinate); Base.IsSet())
		{
			return Base;
		}
		return FindOptional(Dependency.ExpectedContentOrValue);
	};
	for (const auto& Global : Offsets.Globals)
	{
		if (const TOptional<uint64> Found =
			FindOffsetInEntries(Global.Fields, Coordinate); Found.IsSet())
		{
			return Found;
		}
		if (const TOptional<uint64> Found = FindDataType(FindDataType, Global.Type);
			Found.IsSet())
		{
			return Found;
		}
	}
	for (const auto& HardValue : Offsets.HardValues)
	{
		if (const TOptional<uint64> Found =
			FindOffsetInEntries(HardValue.Fields, Coordinate); Found.IsSet())
		{
			return Found;
		}
		if (const TOptional<uint64> Found =
			FindDataType(FindDataType, HardValue.Type); Found.IsSet())
		{
			return Found;
		}
		const TOptional<FModuleStateCapturedOffsetStorage::FEntry> OptionalEntries[] = {
			HardValue.CanonicalValue,
			HardValue.CanonicalValueKind,
			HardValue.CanonicalValueBytes,
		};
		for (const auto& Entry : OptionalEntries)
		{
			if (const TOptional<uint64> Found = FindOptional(Entry); Found.IsSet())
			{
				return Found;
			}
		}
	}
	for (const auto& Initializer : Offsets.Initializers)
	{
		if (const TOptional<uint64> Found =
			FindOffsetInEntries(Initializer.Fields, Coordinate); Found.IsSet())
		{
			return Found;
		}
		if (const TOptional<uint64> Found = FindOptional(Initializer.OwnerGlobal);
			Found.IsSet())
		{
			return Found;
		}
	}
	for (const auto& Action : Offsets.InitializationActions)
	{
		if (const TOptional<uint64> Found =
			FindOffsetInEntries(Action.Fields, Coordinate); Found.IsSet())
		{
			return Found;
		}
		for (const auto& Dependency : Action.Dependencies)
		{
			if (const TOptional<uint64> Found = FindDependency(Dependency); Found.IsSet())
			{
				return Found;
			}
		}
	}
	for (const auto& PostInit : Offsets.PostInitFunctions)
	{
		if (const TOptional<uint64> Found =
			FindOffsetInEntries(PostInit.Fields, Coordinate); Found.IsSet())
		{
			return Found;
		}
	}
	for (const auto& Dependency : Offsets.Dependencies)
	{
		if (const TOptional<uint64> Found = FindDependency(Dependency); Found.IsSet())
		{
			return Found;
		}
	}
	return {};
}

TOptional<uint64> FAngelscriptDecodedCacheRecord::FindCapturedOffset(
	const FAngelscriptFunctionBodyFieldCoordinate& Coordinate) const
{
	const FFunctionBodyRecord* Alternative = Record.TryGet<FFunctionBodyRecord>();
	if (Alternative == nullptr)
	{
		return {};
	}
	const FFunctionBodyCapturedOffsetStorage& Offsets = Alternative->Offsets;
	if (const TOptional<uint64> Header =
		FindOffsetInEntries(Offsets.HeaderOffsets, Coordinate); Header.IsSet())
	{
		return Header;
	}
	if (const TOptional<uint64> Dependency =
		FindOffsetInEntries(Offsets.DependencyOffsets, Coordinate); Dependency.IsSet())
	{
		return Dependency;
	}
	for (const TOptional<FFunctionBodyCapturedOffsetStorage::FEntry>& Entry
		: Offsets.DependencyExpectedValueOffsets)
	{
		if (Entry.IsSet() && CoordinatesMatch(Entry->Coordinate, Coordinate))
		{
			return Entry->Offset;
		}
	}
	const TOptional<FFunctionBodyCapturedOffsetStorage::FEntry> OptionalEntries[] = {
		Offsets.DebugSidecarOffset,
		Offsets.DebugSidecarKindOffset,
		Offsets.DebugSidecarContentHashOffset,
	};
	for (const TOptional<FFunctionBodyCapturedOffsetStorage::FEntry>& Entry
		: OptionalEntries)
	{
		if (Entry.IsSet() && CoordinatesMatch(Entry->Coordinate, Coordinate))
		{
			return Entry->Offset;
		}
	}
	return {};
}

TOptional<uint64> FAngelscriptDecodedCacheRecord::FindCapturedOffset(
	const FAngelscriptDebugSidecarFieldCoordinate& Coordinate) const
{
	const FDebugSidecarRecord* Alternative = Record.TryGet<FDebugSidecarRecord>();
	return Alternative != nullptr
		? FindOffsetInEntries(Alternative->Offsets.Entries, Coordinate)
		: TOptional<uint64>{};
}

TOptional<uint64> FAngelscriptDecodedCacheRecord::FindCapturedOffset(
	const FAngelscriptModuleSnapshotFieldCoordinate& Coordinate) const
{
	const FModuleSnapshotRecord* Alternative = Record.TryGet<FModuleSnapshotRecord>();
	if (Alternative == nullptr)
	{
		return {};
	}
	const TConstArrayView<FModuleSnapshotCapturedOffsetStorage::FEntry> Groups[] = {
		Alternative->Offsets.HeaderOffsets,
		Alternative->Offsets.TypeSchemaLinkOffsets,
		Alternative->Offsets.FunctionBodyLinkOffsets,
	};
	for (const TConstArrayView<FModuleSnapshotCapturedOffsetStorage::FEntry> Group : Groups)
	{
		if (const TOptional<uint64> Found = FindOffsetInEntries(Group, Coordinate);
			Found.IsSet())
		{
			return Found;
		}
	}
	return {};
}

#if WITH_ANGELSCRIPT_UNITTESTS
FAngelscriptCacheTypeSchemaAllocationProbeForTests::
	FAngelscriptCacheTypeSchemaAllocationProbeForTests(
		TArrayView<FAngelscriptCacheTypeSchemaProbeEventForTests> InEventStorage,
		int32& InOutEventCount,
		bool& bInOutOverflowed)
	: EventStorage(InEventStorage)
	, EventCount(InOutEventCount)
	, bOverflowed(bInOutOverflowed)
{
	EventCount = 0;
	bOverflowed = false;
}

void FAngelscriptCacheTypeSchemaAllocationProbeForTests::
	InjectOverflowAfterValidationCheckpointForTests(const uint64 CheckpointOrdinal)
{
	InjectAfterValidationCheckpoint = CheckpointOrdinal;
}

void FAngelscriptCacheTypeSchemaAllocationProbeForTests::
	InjectOverflowAfterAcceptedEventForTests(
		const int32 AcceptedEventIndex,
		const EAngelscriptCacheTypeSchemaInjectedFailureForTests FailureMode)
{
	InjectAfterAcceptedEvent = AcceptedEventIndex;
	InjectedFailureMode = FailureMode;
}

void FAngelscriptCacheTypeSchemaAllocationProbeForTests::BeginDecodeForTests()
{
	EventCount = 0;
	bOverflowed = false;
	TotalAllocationAttempts = 0;
	TotalAllocatedBytes = 0;
	RejectedReservationCount = 0;
	LiveAllocatedBytes = 0;
	PeakLiveAllocatedBytes = 0;
	AllocationBalance = 0;
	DecodedRecordControllerAllocatedBytes = 0;
	DecodedRecordControllerAllocationCount = 0;
	TriggeredAcceptedEventOrdinal = MAX_uint64;
	TriggeredAcceptedEventOffset = 0;
	bInjectedOverflowRecorded = false;
}

void FAngelscriptCacheTypeSchemaAllocationProbeForTests::Record(
	const FAngelscriptCacheTypeSchemaProbeEventForTests& Event)
{
	if (EventCount < 0 || EventCount >= EventStorage.Num())
	{
		bOverflowed = true;
		return;
	}
	EventStorage[EventCount++] = Event;
}

bool FAngelscriptCacheTypeSchemaAllocationProbeForTests::
	RecordAcceptedAllocationForTests(
		const AngelscriptCacheCanonicalCodecTestHooks::FAllocationEvent& Event)
{
	check(Event.Phase
		== AngelscriptCacheCanonicalCodecTestHooks::
			EAllocationEventPhase::AllocationSucceeded);

	const uint64 AcceptedOrdinal = TotalAllocationAttempts;
	FAngelscriptCacheTypeSchemaProbeEventForTests ProbeEvent;
	ProbeEvent.Kind = EAngelscriptCacheTypeSchemaProbeEventKindForTests::Allocation;
	ProbeEvent.SequenceOrdinal = AcceptedOrdinal;
	ProbeEvent.FieldOffset = Event.FieldOffset;
	ProbeEvent.RequestedElementCount = Event.RequestedCapacity;
	ProbeEvent.ElementSize = Event.ElementSize;
	ProbeEvent.ElementAlignment = Event.ElementAlignment;
	ProbeEvent.ReservedCapacity = static_cast<uint64>(Event.ReservedCapacity);
	ProbeEvent.AllocatedBytes = Event.ActualAllocatedBytes;
	ProbeEvent.TotalChargeBytes = Event.ActualAllocatedBytes;
	ProbeEvent.TemporaryChargeBytes = Event.ActualAllocatedBytes;
	Record(ProbeEvent);

	++TotalAllocationAttempts;
	TotalAllocatedBytes += Event.ActualAllocatedBytes;
	LiveAllocatedBytes += Event.ActualAllocatedBytes;
	PeakLiveAllocatedBytes = FMath::Max(
		PeakLiveAllocatedBytes,
		LiveAllocatedBytes);
	++AllocationBalance;

	if (InjectAfterAcceptedEvent >= 0
		&& AcceptedOrdinal == static_cast<uint64>(InjectAfterAcceptedEvent))
	{
		TriggeredAcceptedEventOrdinal = AcceptedOrdinal;
		TriggeredAcceptedEventOffset = Event.FieldOffset;
		if (InjectedFailureMode
			== EAngelscriptCacheTypeSchemaInjectedFailureForTests::PhysicalAfterTarget)
		{
			FAngelscriptCacheTypeSchemaProbeEventForTests InjectionEvent;
			InjectionEvent.Kind =
				EAngelscriptCacheTypeSchemaProbeEventKindForTests::
					InjectedOverflowCheckpoint;
			InjectionEvent.SequenceOrdinal = AcceptedOrdinal;
			InjectionEvent.AllocationAttemptCount = TotalAllocationAttempts;
			InjectionEvent.AllocatedBytes = TotalAllocatedBytes;
			Record(InjectionEvent);
			bInjectedOverflowRecorded = true;
			return true;
		}
	}
	return false;
}

void FAngelscriptCacheTypeSchemaAllocationProbeForTests::
	RecordRejectedReservationForTests()
{
	++RejectedReservationCount;
}

bool FAngelscriptCacheTypeSchemaAllocationProbeForTests::
	RecordValidationCheckpointForTests(
		const uint64 CheckpointOrdinal,
		const FAngelscriptCacheReadBudget& Budget)
{
	FAngelscriptCacheTypeSchemaProbeEventForTests Checkpoint;
	Checkpoint.Kind =
		EAngelscriptCacheTypeSchemaProbeEventKindForTests::ValidationCheckpoint;
	Checkpoint.SequenceOrdinal = CheckpointOrdinal;
	Checkpoint.TotalChargeBytes = Budget.GetDecodedBytes();
	Checkpoint.ResidentChargeBytes = Budget.GetResidentDecodedBytes();
	Checkpoint.TemporaryChargeBytes = Budget.GetTemporaryResidentDecodedBytes();
	Checkpoint.TotalDecodedBytes = Budget.GetDecodedBytes();
	Checkpoint.AcceptedAllocationEventCount =
		static_cast<int32>(TotalAllocationAttempts);
	Checkpoint.AllocationAttemptCount = TotalAllocationAttempts;
	Checkpoint.AllocatedBytes = TotalAllocatedBytes;
	Record(Checkpoint);

	if (!bInjectedOverflowRecorded
		&& CheckpointOrdinal == InjectAfterValidationCheckpoint)
	{
		FAngelscriptCacheTypeSchemaProbeEventForTests InjectionEvent;
		InjectionEvent.Kind =
			EAngelscriptCacheTypeSchemaProbeEventKindForTests::
				InjectedOverflowCheckpoint;
		InjectionEvent.SequenceOrdinal = CheckpointOrdinal;
		InjectionEvent.AllocationAttemptCount = TotalAllocationAttempts;
		InjectionEvent.AllocatedBytes = TotalAllocatedBytes;
		Record(InjectionEvent);
		bInjectedOverflowRecorded = true;
		return true;
	}
	return false;
}

bool FAngelscriptCacheTypeSchemaAllocationProbeForTests::
	ConsumeDeferredAcceptedEventFailureForTests(
		const EAngelscriptCacheTypeSchemaInjectedFailureForTests FailureMode,
		uint64& OutOffset)
{
	if (bInjectedOverflowRecorded
		|| TriggeredAcceptedEventOrdinal == MAX_uint64
		|| InjectedFailureMode != FailureMode)
	{
		return false;
	}

	FAngelscriptCacheTypeSchemaProbeEventForTests InjectionEvent;
	InjectionEvent.Kind =
		EAngelscriptCacheTypeSchemaProbeEventKindForTests::InjectedOverflowCheckpoint;
	InjectionEvent.SequenceOrdinal = TriggeredAcceptedEventOrdinal;
	InjectionEvent.AllocationAttemptCount = TotalAllocationAttempts;
	InjectionEvent.AllocatedBytes = TotalAllocatedBytes;
	Record(InjectionEvent);
	bInjectedOverflowRecorded = true;
	OutOffset = TriggeredAcceptedEventOffset;
	return true;
}

void FAngelscriptCacheTypeSchemaAllocationProbeForTests::
	CloseLiveObservationForTests()
{
	// Observation is synchronous and may not escape in a published token.
	// Peak and cumulative counters remain; only call-local live state closes.
	LiveAllocatedBytes = 0;
	AllocationBalance = 0;
}

uint64 FAngelscriptDecodedCacheRecord::MeasureExactControllerBaseAllocationForTests()
{
	FRecordVariant RecordVariant(TInPlaceType<FTypeSchemaRecord>{});
	FDecodedController* Controller =
		SharedPointerInternals::NewIntrusiveReferenceController<
			ESPMode::ThreadSafe,
			FAngelscriptDecodedCacheRecord>(
				FPrivateConstructionToken{},
				FAngelscriptCacheRecordId{},
				TArray<uint8>{},
				MoveTemp(RecordVariant));
	check(Controller != nullptr);
	const uint64 AllocatedBytes = static_cast<uint64>(FMemory::GetAllocSize(Controller));
	Controller->ReleaseSharedReference();
	return AllocatedBytes;
}

FAngelscriptCacheValidationResult FAngelscriptDecodedCacheRecordTestAccess::
	TryDecodeWithProbe(
		const FAngelscriptCacheRecordId& DeclaredRecordId,
		const TConstArrayView<uint8> CanonicalPayload,
		const FAngelscriptCacheReadLimits& Limits,
		FAngelscriptCacheReadBudget& Budget,
		FAngelscriptCacheTypeSchemaAllocationProbeForTests& Probe,
		TOptional<FAngelscriptDecodedCacheRecordHandle>& OutRecord)
{
	FAngelscriptCacheReadBudget::FDecodedCandidateTransaction Candidate =
		Budget.BeginDecodedCandidateTransaction(Limits);
	return FAngelscriptDecodedCacheRecord::TryDecodeInternal(
		DeclaredRecordId,
		CanonicalPayload,
		Limits,
		Budget,
		Candidate,
		true,
		&Probe,
		OutRecord);
}
#endif
