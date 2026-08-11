#include "Cache/AngelscriptCacheArchive.h"

#include "Private/AngelscriptCacheMemoryView.h"

#include "Hash/Blake3.h"

namespace AngelscriptCacheArchive_Private
{
	using FAddressRange = AngelscriptCacheMemoryView_Private::FAddressRange;
	using AngelscriptCacheMemoryView_Private::TryGetViewRange;
	using AngelscriptCacheMemoryView_Private::TryIsViewAliasedWithAllocation;

	static constexpr uint8 EnvelopeMagic[] = {'U', 'E', 'A', 'S', 'C', 'V', '2', 'R'};
	static constexpr uint8 SemanticRecordPrefix[] = {
		'U', 'E', 'A', 'S', '-', 'C', 'A', 'C', 'H', 'E', '-', 'R', 'E', 'C', 'O', 'R', 'D'};
	static constexpr uint64 SchemaOffset = 8;
	static constexpr uint64 KindOffset = 12;
	static constexpr uint64 ReservedOffset = 13;
	static constexpr uint64 PayloadLengthOffset = 16;
	static constexpr uint64 ContentHashOffset = 24;

	static bool IsKnownRecordKind(const EAngelscriptCacheRecordKind Kind)
	{
		switch (Kind)
		{
		case EAngelscriptCacheRecordKind::SourceIndex:
		case EAngelscriptCacheRecordKind::ModuleInterface:
		case EAngelscriptCacheRecordKind::TypeSchema:
		case EAngelscriptCacheRecordKind::ModuleState:
		case EAngelscriptCacheRecordKind::FunctionBody:
		case EAngelscriptCacheRecordKind::DebugSidecar:
		case EAngelscriptCacheRecordKind::ModuleSnapshot:
			return true;
		default:
			return false;
		}
	}

	static FAngelscriptCacheValidationResult Failure(const EAngelscriptCacheValidationError Error)
	{
		return FAngelscriptCacheValidationResult{Error};
	}

	static FAngelscriptCacheValidationResult EnvelopeFailure(
		const EAngelscriptCacheValidationError Error,
		const EAngelscriptCacheRecordKind Kind = static_cast<EAngelscriptCacheRecordKind>(0),
		const uint64 ByteOffset = 0)
	{
		return FAngelscriptCacheValidationResult::AtStage(
			Error,
			Kind,
			EAngelscriptCacheValidationStage::EnvelopeDecode,
			ByteOffset);
	}

	static bool TryCalculatePayloadReserveBytes(
		const int32 RequestedCount,
		uint64& OutBytes)
	{
		OutBytes = 0;
		if (RequestedCount <= 0)
		{
			return RequestedCount == 0;
		}

		using FArrayType = TArray<uint8>;
		FArrayType::ElementAllocatorType Allocator;
		int32 ReservedCapacity = 0;
		if constexpr (TAllocatorTraits<FArrayType::AllocatorType>::SupportsElementAlignment)
		{
			ReservedCapacity = Allocator.CalculateSlackReserve(
				RequestedCount, sizeof(uint8), alignof(uint8));
		}
		else
		{
			ReservedCapacity = Allocator.CalculateSlackReserve(RequestedCount, sizeof(uint8));
		}
		if (ReservedCapacity < RequestedCount)
		{
			return false;
		}

		OutBytes = static_cast<uint64>(ReservedCapacity) * sizeof(uint8);
		return true;
	}

	static void WriteUInt32LittleEndian(TArray<uint8>& Bytes, const uint32 Value)
	{
		Bytes.Add(static_cast<uint8>(Value));
		Bytes.Add(static_cast<uint8>(Value >> 8));
		Bytes.Add(static_cast<uint8>(Value >> 16));
		Bytes.Add(static_cast<uint8>(Value >> 24));
	}

	static void WriteUInt32LittleEndian(uint8* Bytes, const uint32 Value)
	{
		Bytes[0] = static_cast<uint8>(Value);
		Bytes[1] = static_cast<uint8>(Value >> 8);
		Bytes[2] = static_cast<uint8>(Value >> 16);
		Bytes[3] = static_cast<uint8>(Value >> 24);
	}

	static void WriteUInt64LittleEndian(TArray<uint8>& Bytes, const uint64 Value)
	{
		for (uint32 Shift = 0; Shift < 64; Shift += 8)
		{
			Bytes.Add(static_cast<uint8>(Value >> Shift));
		}
	}

	static void WriteUInt64LittleEndian(uint8* Bytes, const uint64 Value)
	{
		for (uint32 Shift = 0; Shift < 64; Shift += 8)
		{
			Bytes[Shift / 8] = static_cast<uint8>(Value >> Shift);
		}
	}

	static uint32 ReadUInt32LittleEndian(const uint8* Bytes)
	{
		return static_cast<uint32>(Bytes[0])
			| (static_cast<uint32>(Bytes[1]) << 8)
			| (static_cast<uint32>(Bytes[2]) << 16)
			| (static_cast<uint32>(Bytes[3]) << 24);
	}

	static uint64 ReadUInt64LittleEndian(const uint8* Bytes)
	{
		uint64 Value = 0;
		for (uint32 Shift = 0; Shift < 64; Shift += 8)
		{
			Value |= static_cast<uint64>(Bytes[Shift / 8]) << Shift;
		}
		return Value;
	}

	static FAngelscriptCacheRecordId BuildRecordIdUnchecked(
		const EAngelscriptCacheRecordKind Kind,
		const TConstArrayView<uint8> CanonicalPayload)
	{
		static constexpr int32 SemanticHeaderSize =
			UE_ARRAY_COUNT(SemanticRecordPrefix)
			+ 1 + sizeof(uint32) + sizeof(uint8) + sizeof(uint64);
		uint8 SemanticHeader[SemanticHeaderSize]{};
		int32 Offset = 0;
		FMemory::Memcpy(
			SemanticHeader + Offset,
			SemanticRecordPrefix,
			UE_ARRAY_COUNT(SemanticRecordPrefix));
		Offset += UE_ARRAY_COUNT(SemanticRecordPrefix);
		SemanticHeader[Offset++] = 0;
		WriteUInt32LittleEndian(
			SemanticHeader + Offset,
			FAngelscriptCacheRecordArchive::ArchiveSchemaVersion);
		Offset += static_cast<int32>(sizeof(uint32));
		SemanticHeader[Offset++] = static_cast<uint8>(Kind);
		WriteUInt64LittleEndian(
			SemanticHeader + Offset,
			static_cast<uint64>(CanonicalPayload.Num()));
		Offset += static_cast<int32>(sizeof(uint64));
		check(Offset == SemanticHeaderSize);

		FBlake3 Hasher;
		Hasher.Update(SemanticHeader, SemanticHeaderSize);
		if (CanonicalPayload.Num() != 0)
		{
			Hasher.Update(CanonicalPayload.GetData(), static_cast<uint64>(CanonicalPayload.Num()));
		}

		return FAngelscriptCacheRecordId{
			Kind,
			FAngelscriptHash256{Hasher.Finalize()}};
	}
}

FAngelscriptCacheValidationResult FAngelscriptCacheRecordArchive::TryBuildRecordId(
	const EAngelscriptCacheRecordKind Kind,
	const TConstArrayView<uint8> CanonicalPayload,
	FAngelscriptCacheRecordId& OutRecordId)
{
	OutRecordId = {};
	if (CanonicalPayload.Num() < 0)
	{
		return AngelscriptCacheArchive_Private::Failure(EAngelscriptCacheValidationError::InvalidArrayView);
	}

	AngelscriptCacheArchive_Private::FAddressRange PayloadRange;
	if (!AngelscriptCacheArchive_Private::TryGetViewRange(CanonicalPayload, PayloadRange))
	{
		return AngelscriptCacheArchive_Private::Failure(EAngelscriptCacheValidationError::InvalidArrayView);
	}
	if (!AngelscriptCacheArchive_Private::IsKnownRecordKind(Kind))
	{
		return AngelscriptCacheArchive_Private::Failure(EAngelscriptCacheValidationError::UnknownRecordKind);
	}

	OutRecordId = AngelscriptCacheArchive_Private::BuildRecordIdUnchecked(Kind, CanonicalPayload);
	return {};
}

FAngelscriptCacheValidationResult FAngelscriptCacheRecordArchive::SerializeRecordEnvelope(
	const EAngelscriptCacheRecordKind Kind,
	const TConstArrayView<uint8> CanonicalPayload,
	TArray<uint8>& OutBytes)
{
	if (CanonicalPayload.Num() < 0)
	{
		OutBytes.Reset();
		return AngelscriptCacheArchive_Private::Failure(EAngelscriptCacheValidationError::InvalidArrayView);
	}

	bool bAliased = false;
	if (!AngelscriptCacheArchive_Private::TryIsViewAliasedWithAllocation(
		CanonicalPayload,
		OutBytes,
		bAliased))
	{
		OutBytes.Reset();
		return AngelscriptCacheArchive_Private::Failure(EAngelscriptCacheValidationError::InvalidArrayView);
	}
	if (bAliased)
	{
		OutBytes.Reset();
		return AngelscriptCacheArchive_Private::Failure(EAngelscriptCacheValidationError::AliasedInputOutput);
	}

	OutBytes.Reset();
	if (!AngelscriptCacheArchive_Private::IsKnownRecordKind(Kind))
	{
		return AngelscriptCacheArchive_Private::Failure(EAngelscriptCacheValidationError::UnknownRecordKind);
	}

	const int64 TotalSize = static_cast<int64>(EnvelopeHeaderSize) + CanonicalPayload.Num();
	if (TotalSize > MAX_int32)
	{
		return AngelscriptCacheArchive_Private::Failure(EAngelscriptCacheValidationError::Overflow);
	}

	FAngelscriptCacheRecordId RecordId;
	const FAngelscriptCacheValidationResult RecordIdResult = TryBuildRecordId(Kind, CanonicalPayload, RecordId);
	if (!RecordIdResult.IsSuccess())
	{
		return RecordIdResult;
	}
	OutBytes.Reserve(static_cast<int32>(TotalSize));
	OutBytes.Append(
		AngelscriptCacheArchive_Private::EnvelopeMagic,
		UE_ARRAY_COUNT(AngelscriptCacheArchive_Private::EnvelopeMagic));
	AngelscriptCacheArchive_Private::WriteUInt32LittleEndian(OutBytes, ArchiveSchemaVersion);
	OutBytes.Add(static_cast<uint8>(Kind));
	OutBytes.AddZeroed(3);
	AngelscriptCacheArchive_Private::WriteUInt64LittleEndian(
		OutBytes,
		static_cast<uint64>(CanonicalPayload.Num()));
	OutBytes.Append(RecordId.ContentHash.Value.GetBytes(), sizeof(FBlake3Hash::ByteArray));
	OutBytes.Append(CanonicalPayload.GetData(), CanonicalPayload.Num());
	return {};
}

FAngelscriptCacheValidationResult FAngelscriptCacheRecordArchive::DeserializeRecordEnvelope(
	const TConstArrayView<uint8> Bytes,
	const FAngelscriptCacheReadLimits& Limits,
	FAngelscriptCacheReadBudget& Budget,
	FAngelscriptCacheRecordEnvelope& OutEnvelope)
{
	if (Bytes.Num() < 0)
	{
		OutEnvelope.Reset();
		return AngelscriptCacheArchive_Private::EnvelopeFailure(
			EAngelscriptCacheValidationError::InvalidArrayView);
	}

	bool bAliased = false;
	if (!AngelscriptCacheArchive_Private::TryIsViewAliasedWithAllocation(
		Bytes,
		OutEnvelope.CanonicalPayload,
		bAliased))
	{
		OutEnvelope.Reset();
		return AngelscriptCacheArchive_Private::EnvelopeFailure(
			EAngelscriptCacheValidationError::InvalidArrayView);
	}
	if (bAliased)
	{
		OutEnvelope.Reset();
		return AngelscriptCacheArchive_Private::EnvelopeFailure(
			EAngelscriptCacheValidationError::AliasedInputOutput);
	}

	OutEnvelope.Reset();
	if (Bytes.Num() < static_cast<int32>(EnvelopeHeaderSize))
	{
		return AngelscriptCacheArchive_Private::EnvelopeFailure(
			EAngelscriptCacheValidationError::OutOfBounds,
			static_cast<EAngelscriptCacheRecordKind>(0),
			static_cast<uint64>(FMath::Max(Bytes.Num(), 0)));
	}

	const uint8* Data = Bytes.GetData();
	if (FMemory::Memcmp(
		Data,
		AngelscriptCacheArchive_Private::EnvelopeMagic,
		UE_ARRAY_COUNT(AngelscriptCacheArchive_Private::EnvelopeMagic)) != 0)
	{
		return AngelscriptCacheArchive_Private::EnvelopeFailure(
			EAngelscriptCacheValidationError::BadMagic);
	}

	if (AngelscriptCacheArchive_Private::ReadUInt32LittleEndian(Data + 8) != ArchiveSchemaVersion)
	{
		return AngelscriptCacheArchive_Private::EnvelopeFailure(
			EAngelscriptCacheValidationError::UnsupportedSchema,
			static_cast<EAngelscriptCacheRecordKind>(0),
			AngelscriptCacheArchive_Private::SchemaOffset);
	}

	const EAngelscriptCacheRecordKind Kind = static_cast<EAngelscriptCacheRecordKind>(Data[12]);
	if (!AngelscriptCacheArchive_Private::IsKnownRecordKind(Kind))
	{
		return AngelscriptCacheArchive_Private::EnvelopeFailure(
			EAngelscriptCacheValidationError::UnknownRecordKind,
			Kind,
			AngelscriptCacheArchive_Private::KindOffset);
	}

	if (Data[13] != 0 || Data[14] != 0 || Data[15] != 0)
	{
		return AngelscriptCacheArchive_Private::EnvelopeFailure(
			EAngelscriptCacheValidationError::NonZeroReserved,
			Kind,
			AngelscriptCacheArchive_Private::ReservedOffset);
	}

	const uint64 PayloadSize = AngelscriptCacheArchive_Private::ReadUInt64LittleEndian(Data + 16);
	if (PayloadSize > MAX_uint64 - static_cast<uint64>(EnvelopeHeaderSize))
	{
		return AngelscriptCacheArchive_Private::EnvelopeFailure(
			EAngelscriptCacheValidationError::Overflow,
			Kind,
			AngelscriptCacheArchive_Private::PayloadLengthOffset);
	}

	if (PayloadSize > Limits.MaxCanonicalRecordPayloadBytes || PayloadSize > static_cast<uint64>(MAX_int32))
	{
		return AngelscriptCacheArchive_Private::EnvelopeFailure(
			EAngelscriptCacheValidationError::BudgetExceeded,
			Kind,
			AngelscriptCacheArchive_Private::PayloadLengthOffset);
	}

	const uint64 ExpectedSize = static_cast<uint64>(EnvelopeHeaderSize) + PayloadSize;
	const uint64 ActualSize = static_cast<uint64>(Bytes.Num());
	if (ExpectedSize > ActualSize)
	{
		return AngelscriptCacheArchive_Private::EnvelopeFailure(
			EAngelscriptCacheValidationError::OutOfBounds,
			Kind,
			AngelscriptCacheArchive_Private::PayloadLengthOffset);
	}
	if (ExpectedSize < ActualSize)
	{
		return AngelscriptCacheArchive_Private::EnvelopeFailure(
			EAngelscriptCacheValidationError::TrailingData,
			Kind,
			ExpectedSize);
	}

	FBlake3Hash::ByteArray DeclaredHashBytes{};
	FMemory::Memcpy(DeclaredHashBytes, Data + 24, sizeof(DeclaredHashBytes));
	const FAngelscriptHash256 DeclaredHash{FBlake3Hash(DeclaredHashBytes)};
	const TConstArrayView<uint8> CanonicalPayload(
		Data + EnvelopeHeaderSize,
		static_cast<int32>(PayloadSize));
	FAngelscriptCacheRecordId ComputedRecordId;
	const FAngelscriptCacheValidationResult RecordIdResult =
		TryBuildRecordId(Kind, CanonicalPayload, ComputedRecordId);
	if (!RecordIdResult.IsSuccess())
	{
		return RecordIdResult;
	}
	if (!(DeclaredHash == ComputedRecordId.ContentHash))
	{
		return AngelscriptCacheArchive_Private::EnvelopeFailure(
			EAngelscriptCacheValidationError::ChecksumMismatch,
			Kind,
			AngelscriptCacheArchive_Private::ContentHashOffset);
	}

	uint64 PayloadReserveBytes = 0;
	if (!AngelscriptCacheArchive_Private::TryCalculatePayloadReserveBytes(
		CanonicalPayload.Num(), PayloadReserveBytes))
	{
		return AngelscriptCacheArchive_Private::EnvelopeFailure(
			EAngelscriptCacheValidationError::Overflow,
			Kind,
			AngelscriptCacheArchive_Private::PayloadLengthOffset);
	}
	if (!Budget.TryConsumeRetainedDecoded(PayloadReserveBytes, Limits))
	{
		return AngelscriptCacheArchive_Private::EnvelopeFailure(
			EAngelscriptCacheValidationError::BudgetExceeded,
			Kind,
			AngelscriptCacheArchive_Private::PayloadLengthOffset);
	}

	FAngelscriptCacheRecordEnvelope Candidate;
	Candidate.RecordId = ComputedRecordId;
	if (PayloadSize != 0)
	{
		Candidate.CanonicalPayload.Reserve(CanonicalPayload.Num());
		Candidate.CanonicalPayload.Append(CanonicalPayload.GetData(), CanonicalPayload.Num());
	}
	OutEnvelope = MoveTemp(Candidate);
	return {};
}

FAngelscriptCacheValidationResult FAngelscriptCacheRecordArchive::DeserializeRecordEnvelope(
	const TConstArrayView<uint8> Bytes,
	const FAngelscriptCacheReadLimits& Limits,
	FAngelscriptCacheRecordEnvelope& OutEnvelope)
{
	FAngelscriptCacheReadBudget Budget;
	return DeserializeRecordEnvelope(Bytes, Limits, Budget, OutEnvelope);
}
