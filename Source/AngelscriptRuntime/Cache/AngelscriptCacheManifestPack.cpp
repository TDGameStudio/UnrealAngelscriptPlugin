#include "Cache/AngelscriptCacheManifestPack.h"

#include "Cache/AngelscriptCacheArchive.h"
#include "Cache/Private/AngelscriptCacheManifestPackValidation.h"

#include "Async/Async.h"
#include "Hash/Blake3.h"
#include "Misc/Compression.h"

namespace AngelscriptCacheManifestPack_Private
{
	static constexpr ANSICHAR PackMagic[] = "UEASCV2P";
	static constexpr ANSICHAR ManifestMagic[] = "UEASCV2M";
	static constexpr uint64 ManifestModuleCountOffset = 177;

	static FAngelscriptCacheValidationResult Failure(
		const EAngelscriptCacheValidationError Error,
		const EAngelscriptCacheValidationStage Stage,
		const uint64 Offset = 0,
		const EAngelscriptCacheRecordKind Kind =
			static_cast<EAngelscriptCacheRecordKind>(0))
	{
		return FAngelscriptCacheValidationResult::AtStage(
			Error, Kind, Stage, Offset);
	}

	static bool IsRecordKindValid(const EAngelscriptCacheRecordKind Kind)
	{
		const uint8 Raw = static_cast<uint8>(Kind);
		return Raw >= static_cast<uint8>(EAngelscriptCacheRecordKind::SourceIndex)
			&& Raw <= static_cast<uint8>(
				EAngelscriptCacheRecordKind::ModuleSnapshot);
	}

	static bool IsCodecValid(const EAngelscriptCacheCodec Codec)
	{
		return Codec == EAngelscriptCacheCodec::None
			|| Codec == EAngelscriptCacheCodec::Zlib;
	}

	static bool LocationsEqual(
		const FAngelscriptCachePackLocation& A,
		const FAngelscriptCachePackLocation& B)
	{
		return A.PackId == B.PackId
			&& A.PackOffset == B.PackOffset
			&& A.StoredSize == B.StoredSize
			&& A.RawSize == B.RawSize
			&& A.Codec == B.Codec
			&& A.RawChecksum == B.RawChecksum;
	}

	static void AppendUInt16(TArray<uint8>& Bytes, const uint16 Value)
	{
		Bytes.Add(static_cast<uint8>(Value));
		Bytes.Add(static_cast<uint8>(Value >> 8));
	}

	static void AppendUInt32(TArray<uint8>& Bytes, const uint32 Value)
	{
		for (uint32 Shift = 0; Shift < 32; Shift += 8)
		{
			Bytes.Add(static_cast<uint8>(Value >> Shift));
		}
	}

	static void AppendUInt64(TArray<uint8>& Bytes, const uint64 Value)
	{
		for (uint32 Shift = 0; Shift < 64; Shift += 8)
		{
			Bytes.Add(static_cast<uint8>(Value >> Shift));
		}
	}

	static void AppendHash(
		TArray<uint8>& Bytes, const FAngelscriptHash256& Hash)
	{
		Bytes.Append(Hash.Value.GetBytes(), sizeof(FBlake3Hash::ByteArray));
	}

	static void AppendRecordId(
		TArray<uint8>& Bytes, const FAngelscriptCacheRecordId& RecordId)
	{
		Bytes.Add(static_cast<uint8>(RecordId.Kind));
		AppendHash(Bytes, RecordId.ContentHash);
	}

	static uint16 ReadUInt16(const uint8* Bytes)
	{
		return static_cast<uint16>(Bytes[0])
			| static_cast<uint16>(static_cast<uint16>(Bytes[1]) << 8);
	}

	static uint32 ReadUInt32(const uint8* Bytes)
	{
		return static_cast<uint32>(Bytes[0])
			| (static_cast<uint32>(Bytes[1]) << 8)
			| (static_cast<uint32>(Bytes[2]) << 16)
			| (static_cast<uint32>(Bytes[3]) << 24);
	}

	static uint64 ReadUInt64(const uint8* Bytes)
	{
		uint64 Value = 0;
		for (uint32 Shift = 0; Shift < 64; Shift += 8)
		{
			Value |= static_cast<uint64>(Bytes[Shift / 8]) << Shift;
		}
		return Value;
	}

	static FAngelscriptHash256 ReadHash(const uint8* Bytes)
	{
		FBlake3Hash::ByteArray HashBytes{};
		FMemory::Memcpy(HashBytes, Bytes, sizeof(HashBytes));
		return FAngelscriptHash256{FBlake3Hash(HashBytes)};
	}

	static FAngelscriptCacheRecordId ReadRecordId(const uint8* Bytes)
	{
		FAngelscriptCacheRecordId RecordId;
		RecordId.Kind = static_cast<EAngelscriptCacheRecordKind>(Bytes[0]);
		RecordId.ContentHash = ReadHash(Bytes + 1);
		return RecordId;
	}

	static FAngelscriptHash256 DirectHash(const TConstArrayView<uint8> Bytes)
	{
		return FAngelscriptHash256{FBlake3::HashBuffer(
			Bytes.GetData(), static_cast<uint64>(Bytes.Num()))};
	}

	static bool TryAdd(const uint64 A, const uint64 B, uint64& Out)
	{
		if (A > MAX_uint64 - B)
		{
			return false;
		}
		Out = A + B;
		return true;
	}

	static bool TryMultiply(const uint64 A, const uint64 B, uint64& Out)
	{
		if (A != 0 && B > MAX_uint64 / A)
		{
			return false;
		}
		Out = A * B;
		return true;
	}

	struct FStoredPreparedRecord
	{
		FAngelscriptPreparedRecord Record;
		EAngelscriptCacheCodec Codec = EAngelscriptCacheCodec::None;
		FAngelscriptHash256 RawChecksum;
		TArray<uint8> StoredBytes;
	};

	static FAngelscriptCacheValidationResult BuildOnePack(
		const TConstArrayView<FStoredPreparedRecord> Records,
		FAngelscriptEncodedPack& OutPack)
	{
		OutPack = {};
		if (Records.IsEmpty())
		{
			return Failure(EAngelscriptCacheValidationError::ImpossibleCount,
				EAngelscriptCacheValidationStage::PackDecode, 20);
		}

		uint64 IndexBytes = 0;
		uint64 DataOffset = 0;
		uint64 FinalSize = 0;
		if (!TryMultiply(static_cast<uint64>(Records.Num()),
				FAngelscriptCacheManifestPackArchive::PackIndexEntryWireSize,
				IndexBytes)
			|| !TryAdd(
				FAngelscriptCacheManifestPackArchive::PackHeaderWireSize,
				IndexBytes, DataOffset))
		{
			return Failure(EAngelscriptCacheValidationError::Overflow,
				EAngelscriptCacheValidationStage::PackDecode, 20);
		}
		FinalSize = DataOffset;
		for (const FStoredPreparedRecord& Record : Records)
		{
			if (!TryAdd(FinalSize,
				static_cast<uint64>(Record.StoredBytes.Num()), FinalSize))
			{
				return Failure(EAngelscriptCacheValidationError::Overflow,
					EAngelscriptCacheValidationStage::PackDecode, 24);
			}
		}
		if (FinalSize > FAngelscriptCacheReadLimits::DefaultMaxPackBytes
			|| FinalSize > static_cast<uint64>(MAX_int32))
		{
			return Failure(EAngelscriptCacheValidationError::BudgetExceeded,
				EAngelscriptCacheValidationStage::PackDecode, 24);
		}

		OutPack.Bytes.Reserve(static_cast<int32>(FinalSize));
		OutPack.Bytes.Append(
			reinterpret_cast<const uint8*>(PackMagic), 8);
		AppendUInt32(OutPack.Bytes,
			FAngelscriptCacheManifestPackArchive::PackSchemaVersion);
		AppendUInt32(OutPack.Bytes,
			FAngelscriptCacheManifestPackArchive::PackHeaderWireSize);
		AppendUInt32(OutPack.Bytes,
			FAngelscriptCacheManifestPackArchive::PackIndexEntryWireSize);
		AppendUInt32(OutPack.Bytes, static_cast<uint32>(Records.Num()));
		AppendUInt64(OutPack.Bytes, DataOffset);

		uint64 PackOffset = DataOffset;
		OutPack.Index.Reserve(Records.Num());
		for (const FStoredPreparedRecord& Stored : Records)
		{
			FAngelscriptCachePackIndexEntry& Entry =
				OutPack.Index.AddDefaulted_GetRef();
			Entry.RecordId = Stored.Record.RecordId;
			Entry.Codec = Stored.Codec;
			Entry.PackOffset = PackOffset;
			Entry.StoredSize = static_cast<uint64>(Stored.StoredBytes.Num());
			Entry.RawSize = static_cast<uint64>(
				Stored.Record.CanonicalPayload.Num());
			Entry.RawChecksum = Stored.RawChecksum;

			OutPack.Bytes.Add(static_cast<uint8>(Entry.RecordId.Kind));
			OutPack.Bytes.Add(static_cast<uint8>(Entry.Codec));
			AppendUInt16(OutPack.Bytes, 0);
			AppendUInt32(OutPack.Bytes, 0);
			AppendHash(OutPack.Bytes, Entry.RecordId.ContentHash);
			AppendUInt64(OutPack.Bytes, Entry.PackOffset);
			AppendUInt64(OutPack.Bytes, Entry.StoredSize);
			AppendUInt64(OutPack.Bytes, Entry.RawSize);
			AppendHash(OutPack.Bytes, Entry.RawChecksum);
			PackOffset += Entry.StoredSize;
		}
		for (const FStoredPreparedRecord& Stored : Records)
		{
			OutPack.Bytes.Append(Stored.StoredBytes);
		}
		check(static_cast<uint64>(OutPack.Bytes.Num()) == FinalSize);
		OutPack.PackId = DirectHash(OutPack.Bytes);
		return {};
	}

	template <typename WorkType>
	static void RunBoundedCachePreparationWork(
		const int32 WorkItemCount,
		const uint32 RequestedWorkerCount,
		WorkType&& Work)
	{
		check(WorkItemCount > 0);
		const int32 WorkerCount = FMath::Clamp<int32>(
			static_cast<int32>(RequestedWorkerCount), 1, WorkItemCount);
		TAtomic<int32> NextWorkItem{0};
		TArray<TFuture<void>> Workers;
		Workers.Reserve(WorkerCount);
		for (int32 WorkerIndex = 0; WorkerIndex < WorkerCount; ++WorkerIndex)
		{
			Workers.Add(Async(EAsyncExecution::ThreadPool,
				[&NextWorkItem, WorkItemCount, &Work]()
				{
					for (;;)
					{
						const int32 WorkItem = NextWorkItem++;
						if (WorkItem >= WorkItemCount)
						{
							return;
						}
						Work(WorkItem);
					}
				}));
		}
		for (TFuture<void>& Worker : Workers)
		{
			Worker.Wait();
		}
	}

	static FAngelscriptCacheValidationResult ValidateManifestPrefix(
		const FAngelscriptCacheGenerationManifest& Value)
	{
		const EAngelscriptCacheValidationStage Stage =
			EAngelscriptCacheValidationStage::ManifestDecode;
		if (Value.ManifestSchemaVersion
			!= FAngelscriptCacheManifestPackArchive::ManifestSchemaVersion)
		{
			return Failure(EAngelscriptCacheValidationError::UnsupportedSchema,
				Stage, 8);
		}
		if (Value.ManifestFlags != 0)
		{
			return Failure(EAngelscriptCacheValidationError::UnknownFlags,
				Stage, 12);
		}
		if (Value.Compatibility.Hash.IsZero())
		{
			return Failure(EAngelscriptCacheValidationError::ZeroStableKey,
				Stage, 16);
		}
		if (Value.Context.Hash.IsZero())
		{
			return Failure(EAngelscriptCacheValidationError::ZeroStableKey,
				Stage, 48);
		}
		if (Value.Profile.Hash.IsZero())
		{
			return Failure(EAngelscriptCacheValidationError::ZeroStableKey,
				Stage, 80);
		}
		if (!(FAngelscriptArtifactIdentityBuilder::BuildArtifactProfileKey(
			Value.Compatibility, Value.Context).Hash == Value.Profile.Hash))
		{
			return Failure(EAngelscriptCacheValidationError::DerivedHashMismatch,
				Stage, 80);
		}
		if (Value.SourceSnapshot.IsZero())
		{
			return Failure(EAngelscriptCacheValidationError::ZeroStableKey,
				Stage, 112);
		}
		if (Value.SourceIndexRecordId.Kind
			!= EAngelscriptCacheRecordKind::SourceIndex)
		{
			return Failure(EAngelscriptCacheValidationError::WrongRecordKind,
				Stage, 144, Value.SourceIndexRecordId.Kind);
		}
		if (Value.SourceIndexRecordId.ContentHash.IsZero())
		{
			return Failure(EAngelscriptCacheValidationError::ZeroStableKey,
				Stage, 145, Value.SourceIndexRecordId.Kind);
		}
		return {};
	}

	static FAngelscriptCacheValidationResult ValidateManifestValue(
		const FAngelscriptCacheGenerationManifest& Value,
		const FAngelscriptCacheReadLimits& Limits,
		const TConstArrayView<FAngelscriptHash256> PreSortedPackIds = {})
	{
		const EAngelscriptCacheValidationStage Stage =
			EAngelscriptCacheValidationStage::ManifestDecode;
		const FAngelscriptCacheValidationResult PrefixResult =
			ValidateManifestPrefix(Value);
		if (!PrefixResult.IsSuccess())
		{
			return PrefixResult;
		}
		if (Value.ModuleSnapshots.Num() < 0
			|| static_cast<uint64>(Value.ModuleSnapshots.Num())
				> Limits.MaxModuleSnapshots)
		{
			return Failure(EAngelscriptCacheValidationError::BudgetExceeded,
				Stage, ManifestModuleCountOffset);
		}

		uint64 RootOffset = ManifestModuleCountOffset + 4;
		for (int32 Index = 0; Index < Value.ModuleSnapshots.Num(); ++Index)
		{
			const FAngelscriptCacheModuleSnapshotLink& Link =
				Value.ModuleSnapshots[Index];
			if (Link.ModuleKey.Hash.IsZero())
			{
				return Failure(EAngelscriptCacheValidationError::ZeroStableKey,
					Stage, RootOffset);
			}
			if (Link.RecordId.Kind
				!= EAngelscriptCacheRecordKind::ModuleSnapshot)
			{
				return Failure(EAngelscriptCacheValidationError::WrongRecordKind,
					Stage, RootOffset + 32, Link.RecordId.Kind);
			}
			if (Link.RecordId.ContentHash.IsZero())
			{
				return Failure(EAngelscriptCacheValidationError::ZeroStableKey,
					Stage, RootOffset + 33, Link.RecordId.Kind);
			}
			if (Index > 0)
			{
				const FAngelscriptCacheModuleSnapshotLink& Previous =
					Value.ModuleSnapshots[Index - 1];
				if (Link.ModuleKey.Hash < Previous.ModuleKey.Hash)
				{
					return Failure(
						EAngelscriptCacheValidationError::NonCanonicalOrder,
						Stage, RootOffset);
				}
				if (Link.ModuleKey == Previous.ModuleKey)
				{
					return Failure(Link.RecordId == Previous.RecordId
							? EAngelscriptCacheValidationError::DuplicateKey
							: EAngelscriptCacheValidationError::ConflictingKey,
						Stage, RootOffset);
				}
			}
			RootOffset +=
				FAngelscriptCacheManifestPackArchive::ManifestRootWireSize;
		}

		const uint64 RecordCountOffset = RootOffset;
		if (Value.Records.IsEmpty())
		{
			return Failure(EAngelscriptCacheValidationError::ImpossibleCount,
				Stage, RecordCountOffset);
		}
		if (static_cast<uint64>(Value.Records.Num())
			> Limits.MaxGenerationRecords)
		{
			return Failure(EAngelscriptCacheValidationError::BudgetExceeded,
				Stage, RecordCountOffset);
		}

		TArray<FAngelscriptHash256> OwnedPackIds;
		if (PreSortedPackIds.IsEmpty())
		{
			OwnedPackIds.Reserve(Value.Records.Num());
		}
		uint64 RecordOffset = RecordCountOffset + 4;
		for (int32 Index = 0; Index < Value.Records.Num(); ++Index)
		{
			const FAngelscriptCacheRecordIndexEntry& Entry = Value.Records[Index];
			if (!IsRecordKindValid(Entry.RecordId.Kind))
			{
				return Failure(EAngelscriptCacheValidationError::UnknownRecordKind,
					Stage, RecordOffset, Entry.RecordId.Kind);
			}
			if (Entry.RecordId.ContentHash.IsZero())
			{
				return Failure(EAngelscriptCacheValidationError::ZeroStableKey,
					Stage, RecordOffset + 1, Entry.RecordId.Kind);
			}
			if (Entry.Location.PackId.IsZero())
			{
				return Failure(EAngelscriptCacheValidationError::ZeroStableKey,
					Stage, RecordOffset + 33, Entry.RecordId.Kind);
			}
			if (!IsCodecValid(Entry.Location.Codec))
			{
				return Failure(
					EAngelscriptCacheValidationError::UnsupportedStorageCodec,
					Stage, RecordOffset + 89, Entry.RecordId.Kind);
			}
			if (Entry.Location.RawChecksum.IsZero())
			{
				return Failure(EAngelscriptCacheValidationError::ZeroStableKey,
					Stage, RecordOffset + 90, Entry.RecordId.Kind);
			}
			if (Index > 0)
			{
				const FAngelscriptCacheRecordIndexEntry& Previous =
					Value.Records[Index - 1];
				if (Entry.RecordId < Previous.RecordId)
				{
					return Failure(
						EAngelscriptCacheValidationError::NonCanonicalOrder,
						Stage, RecordOffset, Entry.RecordId.Kind);
				}
				if (Entry.RecordId == Previous.RecordId)
				{
					return Failure(LocationsEqual(
							Entry.Location, Previous.Location)
							? EAngelscriptCacheValidationError::DuplicateKey
							: EAngelscriptCacheValidationError::ConflictingKey,
						Stage, RecordOffset, Entry.RecordId.Kind);
				}
			}
			if (PreSortedPackIds.IsEmpty())
			{
				OwnedPackIds.Add(Entry.Location.PackId);
			}
			RecordOffset +=
				FAngelscriptCacheManifestPackArchive::ManifestLocationWireSize;
		}

		if (!PreSortedPackIds.IsEmpty()
			&& PreSortedPackIds.Num() != Value.Records.Num())
		{
			return Failure(EAngelscriptCacheValidationError::ImpossibleCount,
				Stage, RecordCountOffset);
		}
		OwnedPackIds.Sort();
		const TConstArrayView<FAngelscriptHash256> PackIds =
			PreSortedPackIds.IsEmpty()
				? TConstArrayView<FAngelscriptHash256>(OwnedPackIds)
				: PreSortedPackIds;
		int32 DistinctPackCount = 0;
		for (int32 Index = 0; Index < PackIds.Num(); ++Index)
		{
			if (Index == 0 || !(PackIds[Index] == PackIds[Index - 1]))
			{
				++DistinctPackCount;
			}
		}
		if (static_cast<uint64>(DistinctPackCount)
			> Limits.MaxGenerationPacks)
		{
			return Failure(EAngelscriptCacheValidationError::BudgetExceeded,
				Stage, RecordCountOffset);
		}

		const auto FindRecord = [&Value](
			const FAngelscriptCacheRecordId& RecordId) -> bool
		{
			int32 First = 0;
			int32 Last = Value.Records.Num();
			while (First < Last)
			{
				const int32 Middle = First + (Last - First) / 2;
				if (Value.Records[Middle].RecordId < RecordId)
				{
					First = Middle + 1;
				}
				else
				{
					Last = Middle;
				}
			}
			return Value.Records.IsValidIndex(First)
				&& Value.Records[First].RecordId == RecordId;
		};
		if (!FindRecord(Value.SourceIndexRecordId))
		{
			return Failure(EAngelscriptCacheValidationError::MissingRecord,
				Stage, 144, Value.SourceIndexRecordId.Kind);
		}
		for (int32 Index = 0; Index < Value.ModuleSnapshots.Num(); ++Index)
		{
			if (!FindRecord(Value.ModuleSnapshots[Index].RecordId))
			{
				return Failure(EAngelscriptCacheValidationError::MissingRecord,
					Stage,
					ManifestModuleCountOffset + 4
						+ static_cast<uint64>(Index)
							* FAngelscriptCacheManifestPackArchive::ManifestRootWireSize
						+ 32,
					Value.ModuleSnapshots[Index].RecordId.Kind);
			}
		}
		return {};
	}

	struct FDecodedManifestCandidate
	{
		FAngelscriptCacheGenerationManifest Manifest;
		uint64 RecordCountOffset = 0;
		TArray<FAngelscriptHash256> DistinctPackIds;
	};

	static FAngelscriptCacheValidationResult DecodeManifest(
		const TConstArrayView<uint8> CompleteManifestBytes,
		const FAngelscriptHash256& ExpectedGenerationId,
		const FAngelscriptCacheReadLimits& Limits,
		FAngelscriptCacheReadBudget& Budget,
		FAngelscriptCacheTemporaryResidentReservation& RetainedReservation,
		FAngelscriptCacheTemporaryResidentReservation& DistinctPackReservation,
		FDecodedManifestCandidate& OutCandidate)
	{
		OutCandidate = {};
		const EAngelscriptCacheValidationStage Stage =
			EAngelscriptCacheValidationStage::ManifestDecode;
		if (CompleteManifestBytes.Num() < 0
			|| (CompleteManifestBytes.Num() != 0
				&& CompleteManifestBytes.GetData() == nullptr))
		{
			return Failure(EAngelscriptCacheValidationError::InvalidArrayView, Stage);
		}
		if (static_cast<uint64>(CompleteManifestBytes.Num())
			> Limits.MaxManifestBytes)
		{
			return Failure(EAngelscriptCacheValidationError::BudgetExceeded, Stage);
		}
		constexpr uint64 FixedBytesThroughModuleCount =
			ManifestModuleCountOffset + sizeof(uint32);
		if (static_cast<uint64>(CompleteManifestBytes.Num())
			< FixedBytesThroughModuleCount)
		{
			return Failure(EAngelscriptCacheValidationError::OutOfBounds,
				Stage, static_cast<uint64>(CompleteManifestBytes.Num()));
		}

		const uint8* Bytes = CompleteManifestBytes.GetData();
		if (FMemory::Memcmp(Bytes, ManifestMagic, 8) != 0)
		{
			return Failure(EAngelscriptCacheValidationError::BadMagic, Stage);
		}
		FAngelscriptCacheGenerationManifest& Manifest = OutCandidate.Manifest;
		Manifest.ManifestSchemaVersion = ReadUInt32(Bytes + 8);
		Manifest.ManifestFlags = ReadUInt32(Bytes + 12);
		Manifest.Compatibility.Hash = ReadHash(Bytes + 16);
		Manifest.Context.Hash = ReadHash(Bytes + 48);
		Manifest.Profile.Hash = ReadHash(Bytes + 80);
		Manifest.SourceSnapshot = ReadHash(Bytes + 112);
		Manifest.SourceIndexRecordId = ReadRecordId(Bytes + 144);
		const FAngelscriptCacheValidationResult PrefixResult =
			ValidateManifestPrefix(Manifest);
		if (!PrefixResult.IsSuccess())
		{
			return PrefixResult;
		}

		const uint32 ModuleCount = ReadUInt32(Bytes + ManifestModuleCountOffset);
		if (static_cast<uint64>(ModuleCount) > Limits.MaxModuleSnapshots)
		{
			return Failure(EAngelscriptCacheValidationError::BudgetExceeded,
				Stage, ManifestModuleCountOffset);
		}
		uint64 RootBytes = 0;
		uint64 RecordCountOffset = 0;
		uint64 RecordEntriesOffset = 0;
		if (!TryMultiply(static_cast<uint64>(ModuleCount),
				FAngelscriptCacheManifestPackArchive::ManifestRootWireSize,
				RootBytes)
			|| !TryAdd(FixedBytesThroughModuleCount, RootBytes,
				RecordCountOffset)
			|| !TryAdd(RecordCountOffset, sizeof(uint32), RecordEntriesOffset))
		{
			return Failure(EAngelscriptCacheValidationError::Overflow,
				Stage, ManifestModuleCountOffset);
		}
		if (RecordEntriesOffset
			> static_cast<uint64>(CompleteManifestBytes.Num()))
		{
			return Failure(EAngelscriptCacheValidationError::ImpossibleCount,
				Stage, ManifestModuleCountOffset);
		}

		OutCandidate.RecordCountOffset = RecordCountOffset;
		const uint32 RecordCount = ReadUInt32(Bytes + RecordCountOffset);
		if (RecordCount == 0)
		{
			return Failure(EAngelscriptCacheValidationError::ImpossibleCount,
				Stage, RecordCountOffset);
		}
		if (static_cast<uint64>(RecordCount) > Limits.MaxGenerationRecords)
		{
			return Failure(EAngelscriptCacheValidationError::BudgetExceeded,
				Stage, RecordCountOffset);
		}
		uint64 RecordBytes = 0;
		uint64 ExpectedEof = 0;
		if (!TryMultiply(static_cast<uint64>(RecordCount),
				FAngelscriptCacheManifestPackArchive::ManifestLocationWireSize,
				RecordBytes)
			|| !TryAdd(RecordEntriesOffset, RecordBytes, ExpectedEof))
		{
			return Failure(EAngelscriptCacheValidationError::Overflow,
				Stage, RecordCountOffset);
		}
		if (ExpectedEof > static_cast<uint64>(CompleteManifestBytes.Num()))
		{
			return Failure(EAngelscriptCacheValidationError::ImpossibleCount,
				Stage, RecordCountOffset);
		}

		uint64 RetainedBytes = sizeof(FAngelscriptValidatedGeneration);
		uint64 CandidatePartBytes = 0;
		if (!TryMultiply(static_cast<uint64>(ModuleCount),
				static_cast<uint64>(
					sizeof(FAngelscriptCacheModuleSnapshotLink)),
				CandidatePartBytes)
			|| !TryAdd(RetainedBytes, CandidatePartBytes, RetainedBytes)
			|| !TryMultiply(static_cast<uint64>(RecordCount),
				static_cast<uint64>(sizeof(FAngelscriptCacheRecordIndexEntry)),
				CandidatePartBytes)
			|| !TryAdd(RetainedBytes, CandidatePartBytes, RetainedBytes)
			|| !TryMultiply(static_cast<uint64>(RecordCount),
				static_cast<uint64>(
					sizeof(FAngelscriptDecodedCacheRecordHandle)),
				CandidatePartBytes)
			|| !TryAdd(RetainedBytes, CandidatePartBytes, RetainedBytes))
		{
			return Failure(EAngelscriptCacheValidationError::Overflow,
				Stage, RecordCountOffset);
		}
		if (!Budget.TryReserveTemporaryDecoded(
			RetainedBytes, Limits, RetainedReservation))
		{
			return Failure(EAngelscriptCacheValidationError::BudgetExceeded,
				Stage, RecordCountOffset);
		}

		uint64 SortedPackBytes = 0;
		if (!TryMultiply(static_cast<uint64>(RecordCount),
				static_cast<uint64>(sizeof(FAngelscriptHash256)),
				SortedPackBytes))
		{
			return Failure(EAngelscriptCacheValidationError::Overflow,
				Stage, RecordCountOffset);
		}
		FAngelscriptCacheTemporaryResidentReservation SortedPackReservation;
		if (!Budget.TryReserveTemporaryDecoded(
			SortedPackBytes, Limits, SortedPackReservation))
		{
			return Failure(EAngelscriptCacheValidationError::BudgetExceeded,
				Stage, RecordCountOffset);
		}

		Manifest.ModuleSnapshots.Reserve(static_cast<int32>(ModuleCount));
		uint64 RootOffset = FixedBytesThroughModuleCount;
		for (uint32 Index = 0; Index < ModuleCount; ++Index)
		{
			FAngelscriptCacheModuleSnapshotLink& Link =
				Manifest.ModuleSnapshots.AddDefaulted_GetRef();
			Link.ModuleKey.Hash = ReadHash(Bytes + RootOffset);
			Link.RecordId = ReadRecordId(Bytes + RootOffset + 32);
			RootOffset +=
				FAngelscriptCacheManifestPackArchive::ManifestRootWireSize;
		}

		Manifest.Records.Reserve(static_cast<int32>(RecordCount));
		TArray<FAngelscriptHash256> SortedPackIds;
		SortedPackIds.Reserve(static_cast<int32>(RecordCount));
		uint64 RecordOffset = RecordEntriesOffset;
		for (uint32 Index = 0; Index < RecordCount; ++Index)
		{
			const uint8* EntryBytes = Bytes + RecordOffset;
			FAngelscriptCacheRecordIndexEntry& Entry =
				Manifest.Records.AddDefaulted_GetRef();
			Entry.RecordId = ReadRecordId(EntryBytes);
			Entry.Location.PackId = ReadHash(EntryBytes + 33);
			Entry.Location.PackOffset = ReadUInt64(EntryBytes + 65);
			Entry.Location.StoredSize = ReadUInt64(EntryBytes + 73);
			Entry.Location.RawSize = ReadUInt64(EntryBytes + 81);
			Entry.Location.Codec =
				static_cast<EAngelscriptCacheCodec>(EntryBytes[89]);
			Entry.Location.RawChecksum = ReadHash(EntryBytes + 90);
			SortedPackIds.Add(Entry.Location.PackId);
			RecordOffset +=
				FAngelscriptCacheManifestPackArchive::ManifestLocationWireSize;
		}

		SortedPackIds.Sort();
		const FAngelscriptCacheValidationResult ValueResult =
			ValidateManifestValue(Manifest, Limits, SortedPackIds);
		if (!ValueResult.IsSuccess())
		{
			return ValueResult;
		}
		if (ExpectedEof < static_cast<uint64>(CompleteManifestBytes.Num()))
		{
			return Failure(EAngelscriptCacheValidationError::TrailingData,
				Stage, ExpectedEof);
		}

		int32 DistinctPackCount = 0;
		for (int32 Index = 0; Index < SortedPackIds.Num(); ++Index)
		{
			if (Index == 0 || !(SortedPackIds[Index] == SortedPackIds[Index - 1]))
			{
				++DistinctPackCount;
			}
		}
		uint64 DistinctPackBytes = 0;
		if (!TryMultiply(static_cast<uint64>(DistinctPackCount),
				static_cast<uint64>(sizeof(FAngelscriptHash256)),
				DistinctPackBytes))
		{
			return Failure(EAngelscriptCacheValidationError::Overflow,
				Stage, RecordCountOffset);
		}
		if (!Budget.TryReserveTemporaryDecoded(
			DistinctPackBytes, Limits, DistinctPackReservation))
		{
			return Failure(EAngelscriptCacheValidationError::BudgetExceeded,
				Stage, RecordCountOffset);
		}
		OutCandidate.DistinctPackIds.Reserve(DistinctPackCount);
		for (int32 Index = 0; Index < SortedPackIds.Num(); ++Index)
		{
			if (Index == 0 || !(SortedPackIds[Index] == SortedPackIds[Index - 1]))
			{
				OutCandidate.DistinctPackIds.Add(SortedPackIds[Index]);
			}
		}
		if (!(DirectHash(CompleteManifestBytes) == ExpectedGenerationId))
		{
			return Failure(EAngelscriptCacheValidationError::GenerationIdMismatch,
				Stage);
		}
		return {};
	}

	static FAngelscriptCacheValidationResult ValidateDecodedGenerationReachability(
		const FDecodedManifestCandidate& Candidate,
		const TConstArrayView<FAngelscriptDecodedCacheRecordHandle> Records)
	{
		const EAngelscriptCacheValidationStage Stage =
			EAngelscriptCacheValidationStage::ManifestGraph;
		const FAngelscriptCacheGenerationManifest& Manifest = Candidate.Manifest;
		if (Records.Num() != Manifest.Records.Num())
		{
			return Failure(EAngelscriptCacheValidationError::MissingRecord,
				Stage, Candidate.RecordCountOffset);
		}
		const auto RecordOffset = [&Candidate](const int32 Ordinal)
		{
			return Candidate.RecordCountOffset + sizeof(uint32)
				+ static_cast<uint64>(Ordinal)
					* FAngelscriptCacheManifestPackArchive::ManifestLocationWireSize;
		};
		const auto FindRecordOrdinal = [&Manifest](
			const FAngelscriptCacheRecordId& RecordId) -> int32
		{
			return Algo::LowerBoundBy(
				Manifest.Records, RecordId,
				[](const FAngelscriptCacheRecordIndexEntry& Entry)
				{
					return Entry.RecordId;
				});
		};
		TArray<bool> Visited;
		Visited.Init(false, Manifest.Records.Num());
		TArray<int32> Pending;
		Pending.Reserve(Manifest.Records.Num());
		const auto RequireTarget = [&](
			const FAngelscriptCacheRecordId& Target,
			const EAngelscriptCacheRecordKind ExpectedKind,
			const uint64 OwnerOffset) -> FAngelscriptCacheValidationResult
		{
			if (Target.Kind != ExpectedKind)
			{
				return Failure(EAngelscriptCacheValidationError::WrongRecordKind,
					Stage, OwnerOffset, ExpectedKind);
			}
			const int32 Ordinal = FindRecordOrdinal(Target);
			if (!Manifest.Records.IsValidIndex(Ordinal)
				|| !(Manifest.Records[Ordinal].RecordId == Target))
			{
				return Failure(EAngelscriptCacheValidationError::MissingRecord,
					Stage, OwnerOffset, ExpectedKind);
			}
			if (!Visited[Ordinal])
			{
				Visited[Ordinal] = true;
				Pending.Add(Ordinal);
			}
			return {};
		};

		FAngelscriptCacheValidationResult Result = RequireTarget(
			Manifest.SourceIndexRecordId,
			EAngelscriptCacheRecordKind::SourceIndex, 144);
		if (!Result.IsSuccess())
		{
			return Result;
		}
		for (int32 RootOrdinal = 0;
			RootOrdinal < Manifest.ModuleSnapshots.Num(); ++RootOrdinal)
		{
			Result = RequireTarget(
				Manifest.ModuleSnapshots[RootOrdinal].RecordId,
				EAngelscriptCacheRecordKind::ModuleSnapshot,
				ManifestModuleCountOffset + sizeof(uint32)
					+ static_cast<uint64>(RootOrdinal)
						* FAngelscriptCacheManifestPackArchive::ManifestRootWireSize
					+ 32);
			if (!Result.IsSuccess())
			{
				return Result;
			}
		}

		while (!Pending.IsEmpty())
		{
			const int32 Ordinal = Pending.Pop(EAllowShrinking::No);
			const FAngelscriptDecodedCacheRecord& Record = Records[Ordinal].Get();
			const FAngelscriptCacheRecordId& RecordId =
				Manifest.Records[Ordinal].RecordId;
			if (!(Record.GetRecordId() == RecordId))
			{
				return Failure(EAngelscriptCacheValidationError::RecordIdMismatch,
					Stage, RecordOffset(Ordinal), RecordId.Kind);
			}

			switch (RecordId.Kind)
			{
			case EAngelscriptCacheRecordKind::ModuleSnapshot:
			{
				const FAngelscriptCachedModuleSnapshot* Snapshot =
					Record.TryGetModuleSnapshot();
				if (Snapshot == nullptr)
				{
					return Failure(EAngelscriptCacheValidationError::WrongRecordKind,
						Stage, RecordOffset(Ordinal), RecordId.Kind);
				}
				Result = RequireTarget(Snapshot->ModuleInterface.RecordId,
					EAngelscriptCacheRecordKind::ModuleInterface,
					RecordOffset(Ordinal));
				if (!Result.IsSuccess())
				{
					return Result;
				}
				for (const FAngelscriptCachedTypeSchemaLink& Link :
					Snapshot->TypeSchemas)
				{
					Result = RequireTarget(Link.RecordId,
						EAngelscriptCacheRecordKind::TypeSchema,
						RecordOffset(Ordinal));
					if (!Result.IsSuccess())
					{
						return Result;
					}
				}
				Result = RequireTarget(Snapshot->ModuleState.RecordId,
					EAngelscriptCacheRecordKind::ModuleState,
					RecordOffset(Ordinal));
				if (!Result.IsSuccess())
				{
					return Result;
				}
				for (const FAngelscriptCachedFunctionBodyLink& Link :
					Snapshot->FunctionBodies)
				{
					Result = RequireTarget(Link.RecordId,
						EAngelscriptCacheRecordKind::FunctionBody,
						RecordOffset(Ordinal));
					if (!Result.IsSuccess())
					{
						return Result;
					}
				}
				break;
			}
			case EAngelscriptCacheRecordKind::FunctionBody:
			{
				const FAngelscriptCachedFunctionBody* Body =
					Record.TryGetFunctionBody();
				if (Body == nullptr)
				{
					return Failure(EAngelscriptCacheValidationError::WrongRecordKind,
						Stage, RecordOffset(Ordinal), RecordId.Kind);
				}
				if (Body->DebugSidecar.IsSet())
				{
					Result = RequireTarget(Body->DebugSidecar.GetValue(),
						EAngelscriptCacheRecordKind::DebugSidecar,
						RecordOffset(Ordinal));
					if (!Result.IsSuccess())
					{
						return Result;
					}
				}
				break;
			}
			case EAngelscriptCacheRecordKind::SourceIndex:
			case EAngelscriptCacheRecordKind::ModuleInterface:
			case EAngelscriptCacheRecordKind::TypeSchema:
			case EAngelscriptCacheRecordKind::ModuleState:
			case EAngelscriptCacheRecordKind::DebugSidecar:
				break;
			default:
				return Failure(EAngelscriptCacheValidationError::UnknownRecordKind,
					Stage, RecordOffset(Ordinal), RecordId.Kind);
			}
		}

		for (int32 Ordinal = 0; Ordinal < Visited.Num(); ++Ordinal)
		{
			if (!Visited[Ordinal])
			{
				return Failure(EAngelscriptCacheValidationError::UnexpectedRecord,
					Stage, RecordOffset(Ordinal),
					Manifest.Records[Ordinal].RecordId.Kind);
			}
		}

		const int32 SourceOrdinal = FindRecordOrdinal(
			Manifest.SourceIndexRecordId);
		const FAngelscriptCachedSourceIndex* Source =
			Records[SourceOrdinal]->TryGetSourceIndex();
		if (Source == nullptr || !(Source->SourceSnapshot == Manifest.SourceSnapshot))
		{
			return Failure(EAngelscriptCacheValidationError::SourceSnapshotMismatch,
				Stage, RecordOffset(SourceOrdinal),
				EAngelscriptCacheRecordKind::SourceIndex);
		}
		for (const FAngelscriptCacheModuleSnapshotLink& Root :
			Manifest.ModuleSnapshots)
		{
			const int32 RootOrdinal = FindRecordOrdinal(Root.RecordId);
			const FAngelscriptCachedModuleSnapshot* Snapshot =
				Records[RootOrdinal]->TryGetModuleSnapshot();
			if (Snapshot == nullptr || !(Snapshot->ModuleKey == Root.ModuleKey))
			{
				return Failure(EAngelscriptCacheValidationError::CrossModuleOwner,
					Stage, RecordOffset(RootOrdinal),
					EAngelscriptCacheRecordKind::ModuleSnapshot);
			}
		}
		return {};
	}
}

bool FAngelscriptUnrealZlibCacheStorageCodec::TryCompressCanonicalZlib(
	const TConstArrayView<uint8> RawBytes,
	TArray<uint8>& OutStoredBytes)
{
	OutStoredBytes.Reset();
	if (RawBytes.IsEmpty())
	{
		return true;
	}
	int64 Bound = 0;
	if (!FCompression::CompressMemoryBound(
		NAME_Zlib, Bound, static_cast<int64>(RawBytes.Num()),
		DEFAULT_ZLIB_BIT_WINDOW)
		|| Bound <= 0 || Bound > MAX_int32)
	{
		return false;
	}
	OutStoredBytes.SetNumUninitialized(static_cast<int32>(Bound));
	int64 StoredSize = Bound;
	if (!FCompression::CompressMemory(
		NAME_Zlib, OutStoredBytes.GetData(), StoredSize,
		RawBytes.GetData(), static_cast<int64>(RawBytes.Num()),
		COMPRESS_BiasMemory, DEFAULT_ZLIB_BIT_WINDOW)
		|| StoredSize < 0 || StoredSize > Bound)
	{
		OutStoredBytes.Reset();
		return false;
	}
	OutStoredBytes.SetNum(static_cast<int32>(StoredSize), EAllowShrinking::No);
	return true;
}

bool FAngelscriptUnrealZlibCacheStorageCodec::TryCompressCanonicalZlibInto(
	const TConstArrayView<uint8> RawBytes,
	const TArrayView<uint8> StoredOutput,
	uint64& OutProducedBytes)
{
	OutProducedBytes = 0;
	if (RawBytes.IsEmpty())
	{
		return StoredOutput.IsEmpty();
	}
	if (StoredOutput.IsEmpty())
	{
		return false;
	}
	int64 StoredSize = static_cast<int64>(StoredOutput.Num());
	if (!FCompression::CompressMemory(
		NAME_Zlib, StoredOutput.GetData(), StoredSize,
		RawBytes.GetData(), static_cast<int64>(RawBytes.Num()),
		COMPRESS_BiasMemory, DEFAULT_ZLIB_BIT_WINDOW)
		|| StoredSize < 0 || StoredSize > StoredOutput.Num())
	{
		return false;
	}
	OutProducedBytes = static_cast<uint64>(StoredSize);
	return true;
}

bool FAngelscriptUnrealZlibCacheStorageCodec::TryDecompressCanonicalZlib(
	const TConstArrayView<uint8> StoredBytes,
	const TArrayView<uint8> RawOutput,
	uint64& OutProducedBytes)
{
	OutProducedBytes = 0;
	if (!FCompression::UncompressMemory(
		NAME_Zlib, RawOutput.GetData(), static_cast<int64>(RawOutput.Num()),
		StoredBytes.GetData(), static_cast<int64>(StoredBytes.Num()),
		COMPRESS_NoFlags, DEFAULT_ZLIB_BIT_WINDOW))
	{
		return false;
	}
	OutProducedBytes = static_cast<uint64>(RawOutput.Num());
	return true;
}

FAngelscriptCacheValidationResult BuildAngelscriptCachePacks(
	const TConstArrayView<FAngelscriptPreparedRecord> NewRecords,
	const FAngelscriptCachePackPolicy& Policy,
	IAngelscriptCacheStorageCodec& Codec,
	TArray<FAngelscriptEncodedPack>& OutPacks)
{
	using namespace AngelscriptCacheManifestPack_Private;
	OutPacks.Reset();
	if (NewRecords.Num() < 0
		|| (NewRecords.Num() != 0 && NewRecords.GetData() == nullptr))
	{
		return Failure(EAngelscriptCacheValidationError::InvalidArrayView,
			EAngelscriptCacheValidationStage::PackDecode);
	}
	if (NewRecords.IsEmpty())
	{
		return {};
	}
	if (Policy.CompressionPolicy != EAngelscriptCachePackCompressionPolicy::Auto
		&& Policy.CompressionPolicy
			!= EAngelscriptCachePackCompressionPolicy::ForceNoneForTest
		&& Policy.CompressionPolicy
			!= EAngelscriptCachePackCompressionPolicy::ForceZlibForTest)
	{
		return Failure(EAngelscriptCacheValidationError::UnknownEnumValue,
			EAngelscriptCacheValidationStage::PackDecode);
	}
	if (Policy.ExecutionMode
			!= EAngelscriptCachePreparationExecutionMode::ForcedSerial
		&& Policy.ExecutionMode
			!= EAngelscriptCachePreparationExecutionMode::BoundedParallel)
	{
		return Failure(EAngelscriptCacheValidationError::UnknownEnumValue,
			EAngelscriptCacheValidationStage::PackDecode);
	}
	if (Policy.MaxWorkerCount == 0)
	{
		return Failure(EAngelscriptCacheValidationError::ImpossibleCount,
			EAngelscriptCacheValidationStage::PackDecode);
	}

	TArray<FAngelscriptPreparedRecord> Canonical;
	Canonical.Append(NewRecords.GetData(), NewRecords.Num());
	Canonical.Sort([](
		const FAngelscriptPreparedRecord& A,
		const FAngelscriptPreparedRecord& B)
	{
		return A.RecordId < B.RecordId;
	});

	TArray<FAngelscriptPreparedRecord> UniqueRecords;
	UniqueRecords.Reserve(Canonical.Num());
	for (int32 Index = 0; Index < Canonical.Num(); ++Index)
	{
		const FAngelscriptPreparedRecord& Record = Canonical[Index];
		if (static_cast<uint64>(Record.CanonicalPayload.Num())
			> FAngelscriptCacheReadLimits::DefaultMaxCanonicalRecordPayloadBytes)
		{
			return Failure(EAngelscriptCacheValidationError::BudgetExceeded,
				EAngelscriptCacheValidationStage::PackDecode, 0,
				Record.RecordId.Kind);
		}
		FAngelscriptCacheRecordId Computed;
		const FAngelscriptCacheValidationResult IdResult =
			FAngelscriptCacheRecordArchive::TryBuildRecordId(
				Record.RecordId.Kind, Record.CanonicalPayload, Computed);
		if (!IdResult.IsSuccess() || !(Computed == Record.RecordId))
		{
			return Failure(EAngelscriptCacheValidationError::RecordIdMismatch,
				EAngelscriptCacheValidationStage::PackDecode, 0,
				Record.RecordId.Kind);
		}
		if (Index > 0 && Record.RecordId == Canonical[Index - 1].RecordId)
		{
			if (Record.CanonicalPayload == Canonical[Index - 1].CanonicalPayload)
			{
				continue;
			}
			return Failure(EAngelscriptCacheValidationError::ConflictingKey,
				EAngelscriptCacheValidationStage::PackDecode, 0,
				Record.RecordId.Kind);
		}
		UniqueRecords.Add(Record);
	}

	TArray<FStoredPreparedRecord> Prepared;
	Prepared.SetNum(UniqueRecords.Num());
	TArray<FAngelscriptCacheValidationResult> PreparationResults;
	PreparationResults.SetNum(UniqueRecords.Num());
	auto PrepareRecord = [
		&UniqueRecords,
		&Prepared,
		&PreparationResults,
		&Policy,
		&Codec](const int32 Index)
	{
		const FAngelscriptPreparedRecord& Record = UniqueRecords[Index];
		FStoredPreparedRecord& Stored = Prepared[Index];
		Stored.Record = Record;
		Stored.RawChecksum = DirectHash(Record.CanonicalPayload);
		const bool bMayCompress = !Record.CanonicalPayload.IsEmpty()
			&& Policy.CompressionPolicy
				!= EAngelscriptCachePackCompressionPolicy::ForceNoneForTest;
		TArray<uint8> Compressed;
		const bool bCompressed = bMayCompress
			&& Codec.TryCompressCanonicalZlib(
				Record.CanonicalPayload, Compressed)
			&& Compressed.Num() < Record.CanonicalPayload.Num();
		if (Policy.CompressionPolicy
			== EAngelscriptCachePackCompressionPolicy::ForceZlibForTest
			&& !bCompressed)
		{
			PreparationResults[Index] = Failure(
				EAngelscriptCacheValidationError::UnsupportedStorageCodec,
				EAngelscriptCacheValidationStage::PackDecode, 0,
				Record.RecordId.Kind);
			return;
		}
		if (bCompressed)
		{
			Stored.Codec = EAngelscriptCacheCodec::Zlib;
			Stored.StoredBytes = MoveTemp(Compressed);
		}
		else
		{
			Stored.Codec = EAngelscriptCacheCodec::None;
			Stored.StoredBytes = Record.CanonicalPayload;
		}
	};
	if (Policy.ExecutionMode
		== EAngelscriptCachePreparationExecutionMode::BoundedParallel)
	{
		RunBoundedCachePreparationWork(
			Prepared.Num(), Policy.MaxWorkerCount, PrepareRecord);
	}
	else
	{
		for (int32 Index = 0; Index < Prepared.Num(); ++Index)
		{
			PrepareRecord(Index);
		}
	}
	for (const FAngelscriptCacheValidationResult& Result : PreparationResults)
	{
		if (!Result.IsSuccess())
		{
			return Result;
		}
	}

	struct FPackGroup
	{
		int32 Start = 0;
		int32 Count = 0;
	};
	TArray<FPackGroup> PackGroups;
	int32 GroupStart = 0;
	uint64 GroupRawBytes = 0;
	uint64 GroupStoredBytes = 0;
	for (int32 Index = 0; Index < Prepared.Num(); ++Index)
	{
		const FStoredPreparedRecord& Record = Prepared[Index];
		const uint64 RawBytes = static_cast<uint64>(
			Record.Record.CanonicalPayload.Num());
		const uint64 StoredBytes = static_cast<uint64>(Record.StoredBytes.Num());
		const int32 ExistingCount = Index - GroupStart;
		uint64 ProspectiveRaw = 0;
		uint64 ProspectiveStored = 0;
		uint64 ProspectiveIndexBytes = 0;
		uint64 ProspectiveFinalBytes = 0;
		const bool bArithmeticOk = TryAdd(GroupRawBytes, RawBytes, ProspectiveRaw)
			&& TryAdd(GroupStoredBytes, StoredBytes, ProspectiveStored)
			&& TryMultiply(static_cast<uint64>(ExistingCount + 1),
				FAngelscriptCacheManifestPackArchive::PackIndexEntryWireSize,
				ProspectiveIndexBytes)
			&& TryAdd(
				FAngelscriptCacheManifestPackArchive::PackHeaderWireSize,
				ProspectiveIndexBytes, ProspectiveFinalBytes)
			&& TryAdd(ProspectiveFinalBytes, ProspectiveStored,
				ProspectiveFinalBytes);
		if (!bArithmeticOk)
		{
			return Failure(EAngelscriptCacheValidationError::Overflow,
				EAngelscriptCacheValidationStage::PackDecode);
		}
		const bool bCloseExisting = ExistingCount > 0
			&& (ProspectiveRaw > Policy.TargetRawBytesPerPack
				|| static_cast<uint64>(ExistingCount + 1)
					> FAngelscriptCacheReadLimits::DefaultMaxPackIndexEntries
				|| ProspectiveFinalBytes
					> FAngelscriptCacheReadLimits::DefaultMaxPackBytes);
		if (bCloseExisting)
		{
			PackGroups.Add({GroupStart, ExistingCount});
			GroupStart = Index;
			GroupRawBytes = 0;
			GroupStoredBytes = 0;
		}
		if (!TryAdd(GroupRawBytes, RawBytes, GroupRawBytes)
			|| !TryAdd(GroupStoredBytes, StoredBytes, GroupStoredBytes))
		{
			return Failure(EAngelscriptCacheValidationError::Overflow,
				EAngelscriptCacheValidationStage::PackDecode);
		}
	}
	if (GroupStart < Prepared.Num())
	{
		PackGroups.Add({GroupStart, Prepared.Num() - GroupStart});
	}

	TArray<FAngelscriptEncodedPack> CandidatePacks;
	CandidatePacks.SetNum(PackGroups.Num());
	TArray<FAngelscriptCacheValidationResult> PackResults;
	PackResults.SetNum(PackGroups.Num());
	auto BuildPack = [&Prepared, &PackGroups, &CandidatePacks, &PackResults](
		const int32 GroupIndex)
	{
		const FPackGroup& Group = PackGroups[GroupIndex];
		PackResults[GroupIndex] = BuildOnePack(
			MakeArrayView(Prepared.GetData() + Group.Start, Group.Count),
			CandidatePacks[GroupIndex]);
	};
	if (Policy.ExecutionMode
			== EAngelscriptCachePreparationExecutionMode::BoundedParallel
		&& PackGroups.Num() > 1)
	{
		RunBoundedCachePreparationWork(
			PackGroups.Num(), Policy.MaxWorkerCount, BuildPack);
	}
	else
	{
		for (int32 GroupIndex = 0;
			GroupIndex < PackGroups.Num(); ++GroupIndex)
		{
			BuildPack(GroupIndex);
		}
	}
	for (const FAngelscriptCacheValidationResult& Result : PackResults)
	{
		if (!Result.IsSuccess())
		{
			return Result;
		}
	}
	OutPacks = MoveTemp(CandidatePacks);
	return {};
}

FAngelscriptCacheValidationResult AggregateAngelscriptCachePreparedRecordCompletions(
	const TConstArrayView<FAngelscriptPreparedRecordCompletion> Completions,
	const EAngelscriptCachePreparationExecutionMode ExecutionMode,
	const FAngelscriptCachePackPolicy& Policy,
	IAngelscriptCacheStorageCodec& Codec,
	TArray<FAngelscriptEncodedPack>& OutPacks)
{
	using namespace AngelscriptCacheManifestPack_Private;
	OutPacks.Reset();
	if (ExecutionMode != EAngelscriptCachePreparationExecutionMode::ForcedSerial
		&& ExecutionMode
			!= EAngelscriptCachePreparationExecutionMode::BoundedParallel)
	{
		return Failure(EAngelscriptCacheValidationError::UnknownEnumValue,
			EAngelscriptCacheValidationStage::PackDecode);
	}
	TArray<const FAngelscriptPreparedRecordCompletion*> ByPreparation;
	TArray<bool> SeenCompletion;
	ByPreparation.Init(nullptr, Completions.Num());
	SeenCompletion.Init(false, Completions.Num());
	for (const FAngelscriptPreparedRecordCompletion& Completion : Completions)
	{
		if (!ByPreparation.IsValidIndex(
				static_cast<int32>(Completion.PreparationOrdinal))
			|| !SeenCompletion.IsValidIndex(
				static_cast<int32>(Completion.CompletionOrdinal)))
		{
			return Failure(EAngelscriptCacheValidationError::OrdinalGap,
				EAngelscriptCacheValidationStage::PackDecode);
		}
		if (ByPreparation[Completion.PreparationOrdinal] != nullptr
			|| SeenCompletion[Completion.CompletionOrdinal])
		{
			return Failure(EAngelscriptCacheValidationError::DuplicateOrdinal,
				EAngelscriptCacheValidationStage::PackDecode);
		}
		if (ExecutionMode
				== EAngelscriptCachePreparationExecutionMode::ForcedSerial
			&& Completion.CompletionOrdinal != Completion.PreparationOrdinal)
		{
			return Failure(EAngelscriptCacheValidationError::NonCanonicalOrder,
				EAngelscriptCacheValidationStage::PackDecode);
		}
		ByPreparation[Completion.PreparationOrdinal] = &Completion;
		SeenCompletion[Completion.CompletionOrdinal] = true;
	}
	TArray<FAngelscriptPreparedRecord> Records;
	Records.Reserve(Completions.Num());
	for (const FAngelscriptPreparedRecordCompletion* Completion : ByPreparation)
	{
		if (Completion == nullptr)
		{
			return Failure(EAngelscriptCacheValidationError::OrdinalGap,
				EAngelscriptCacheValidationStage::PackDecode);
		}
		Records.Add(Completion->Record);
	}
	return BuildAngelscriptCachePacks(Records, Policy, Codec, OutPacks);
}

namespace AngelscriptCacheManifestPack_Private
{
static FAngelscriptCacheValidationResult ValidatePackWithIndexReservation(
	const TConstArrayView<uint8> CompletePackBytes,
	const FAngelscriptHash256& ExpectedPackId,
	const FAngelscriptCacheReadLimits& Limits,
	FAngelscriptCacheReadBudget& Budget,
	FAngelscriptCacheTemporaryResidentReservation* IndexReservation,
	TArray<FAngelscriptCachePackIndexEntry>& OutIndex)
{
	OutIndex.Reset();
	const EAngelscriptCacheValidationStage Stage =
		EAngelscriptCacheValidationStage::PackDecode;
	if (CompletePackBytes.Num() < 0
		|| (CompletePackBytes.Num() != 0
			&& CompletePackBytes.GetData() == nullptr))
	{
		return Failure(EAngelscriptCacheValidationError::InvalidArrayView, Stage);
	}
	if (static_cast<uint64>(CompletePackBytes.Num()) > Limits.MaxPackBytes)
	{
		return Failure(EAngelscriptCacheValidationError::BudgetExceeded, Stage);
	}
	if (CompletePackBytes.Num()
		< static_cast<int32>(
			FAngelscriptCacheManifestPackArchive::PackHeaderWireSize))
	{
		return Failure(EAngelscriptCacheValidationError::OutOfBounds,
			Stage, static_cast<uint64>(CompletePackBytes.Num()));
	}
	const uint8* Bytes = CompletePackBytes.GetData();
	if (FMemory::Memcmp(Bytes, PackMagic, 8) != 0)
	{
		return Failure(EAngelscriptCacheValidationError::BadMagic, Stage);
	}
	if (ReadUInt32(Bytes + 8)
		!= FAngelscriptCacheManifestPackArchive::PackSchemaVersion)
	{
		return Failure(EAngelscriptCacheValidationError::UnsupportedSchema,
			Stage, 8);
	}
	if (ReadUInt32(Bytes + 12)
		!= FAngelscriptCacheManifestPackArchive::PackHeaderWireSize)
	{
		return Failure(EAngelscriptCacheValidationError::UnsupportedSchema,
			Stage, 12);
	}
	if (ReadUInt32(Bytes + 16)
		!= FAngelscriptCacheManifestPackArchive::PackIndexEntryWireSize)
	{
		return Failure(EAngelscriptCacheValidationError::UnsupportedSchema,
			Stage, 16);
	}
	const uint32 Count = ReadUInt32(Bytes + 20);
	if (Count == 0)
	{
		return Failure(EAngelscriptCacheValidationError::ImpossibleCount,
			Stage, 20);
	}
	if (static_cast<uint64>(Count) > Limits.MaxPackIndexEntries)
	{
		return Failure(EAngelscriptCacheValidationError::BudgetExceeded,
			Stage, 20);
	}
	uint64 IndexBytes = 0;
	uint64 ExpectedDataOffset = 0;
	if (!TryMultiply(Count,
			FAngelscriptCacheManifestPackArchive::PackIndexEntryWireSize,
			IndexBytes)
		|| !TryAdd(
			FAngelscriptCacheManifestPackArchive::PackHeaderWireSize,
			IndexBytes, ExpectedDataOffset))
	{
		return Failure(EAngelscriptCacheValidationError::Overflow, Stage, 20);
	}
	if (ReadUInt64(Bytes + 24) != ExpectedDataOffset
		|| ExpectedDataOffset > static_cast<uint64>(CompletePackBytes.Num()))
	{
		return Failure(EAngelscriptCacheValidationError::OutOfBounds, Stage, 24);
	}
	if (IndexReservation != nullptr)
	{
		uint64 CandidateBytes = 0;
		if (!TryMultiply(static_cast<uint64>(Count),
				static_cast<uint64>(sizeof(FAngelscriptCachePackIndexEntry)),
				CandidateBytes))
		{
			return Failure(EAngelscriptCacheValidationError::Overflow, Stage, 20);
		}
		if (!Budget.TryReserveTemporaryDecoded(
			CandidateBytes, Limits, *IndexReservation))
		{
			return Failure(
				EAngelscriptCacheValidationError::BudgetExceeded, Stage, 20);
		}
	}

	TArray<FAngelscriptCachePackIndexEntry> Candidate;
	Candidate.Reserve(static_cast<int32>(Count));
	uint64 ExpectedOffset = ExpectedDataOffset;
	for (uint32 Index = 0; Index < Count; ++Index)
	{
		const uint64 EntryOffset =
			FAngelscriptCacheManifestPackArchive::PackHeaderWireSize
			+ static_cast<uint64>(Index)
				* FAngelscriptCacheManifestPackArchive::PackIndexEntryWireSize;
		const uint8* EntryBytes = Bytes + EntryOffset;
		FAngelscriptCachePackIndexEntry& Entry = Candidate.AddDefaulted_GetRef();
		Entry.RecordId.Kind =
			static_cast<EAngelscriptCacheRecordKind>(EntryBytes[0]);
		if (!IsRecordKindValid(Entry.RecordId.Kind))
		{
			return Failure(EAngelscriptCacheValidationError::UnknownRecordKind,
				Stage, EntryOffset, Entry.RecordId.Kind);
		}
		Entry.Codec = static_cast<EAngelscriptCacheCodec>(EntryBytes[1]);
		if (!IsCodecValid(Entry.Codec))
		{
			return Failure(
				EAngelscriptCacheValidationError::UnsupportedStorageCodec,
				Stage, EntryOffset + 1, Entry.RecordId.Kind);
		}
		if (ReadUInt16(EntryBytes + 2) != 0)
		{
			return Failure(EAngelscriptCacheValidationError::NonZeroReserved,
				Stage, EntryOffset + 2, Entry.RecordId.Kind);
		}
		if (ReadUInt32(EntryBytes + 4) != 0)
		{
			return Failure(EAngelscriptCacheValidationError::NonZeroReserved,
				Stage, EntryOffset + 4, Entry.RecordId.Kind);
		}
		Entry.RecordId.ContentHash = ReadHash(EntryBytes + 8);
		Entry.PackOffset = ReadUInt64(EntryBytes + 40);
		Entry.StoredSize = ReadUInt64(EntryBytes + 48);
		Entry.RawSize = ReadUInt64(EntryBytes + 56);
		Entry.RawChecksum = ReadHash(EntryBytes + 64);
		if (Entry.StoredSize > Limits.MaxStoredRecordBytes)
		{
			return Failure(EAngelscriptCacheValidationError::BudgetExceeded,
				Stage, EntryOffset + 48, Entry.RecordId.Kind);
		}
		if (Entry.RawSize > Limits.MaxCanonicalRecordPayloadBytes)
		{
			return Failure(EAngelscriptCacheValidationError::BudgetExceeded,
				Stage, EntryOffset + 56, Entry.RecordId.Kind);
		}
		if (Entry.Codec == EAngelscriptCacheCodec::None
			&& Entry.StoredSize != Entry.RawSize)
		{
			return Failure(EAngelscriptCacheValidationError::OutOfBounds,
				Stage, EntryOffset + 56, Entry.RecordId.Kind);
		}
		if (Entry.Codec == EAngelscriptCacheCodec::Zlib)
		{
			if (Entry.StoredSize == 0)
			{
				return Failure(EAngelscriptCacheValidationError::OutOfBounds,
					Stage, EntryOffset + 48, Entry.RecordId.Kind);
			}
			if (Entry.RawSize == 0 || Entry.StoredSize >= Entry.RawSize)
			{
				return Failure(EAngelscriptCacheValidationError::OutOfBounds,
					Stage, EntryOffset + 56, Entry.RecordId.Kind);
			}
		}
		uint64 EndOffset = 0;
		if (!TryAdd(Entry.PackOffset, Entry.StoredSize, EndOffset))
		{
			return Failure(EAngelscriptCacheValidationError::Overflow,
				Stage, EntryOffset + 40, Entry.RecordId.Kind);
		}
		if (Entry.PackOffset < ExpectedOffset)
		{
			return Failure(Index > 0 && Entry.StoredSize > 0
					? EAngelscriptCacheValidationError::OverlappingRange
					: EAngelscriptCacheValidationError::OutOfBounds,
				Stage, EntryOffset + 40, Entry.RecordId.Kind);
		}
		if (Entry.PackOffset > ExpectedOffset)
		{
			return Failure(EAngelscriptCacheValidationError::OutOfBounds,
				Stage, EntryOffset + 40, Entry.RecordId.Kind);
		}
		if (EndOffset > static_cast<uint64>(CompletePackBytes.Num()))
		{
			return Failure(EAngelscriptCacheValidationError::OutOfBounds,
				Stage, EntryOffset + 48, Entry.RecordId.Kind);
		}
		ExpectedOffset = EndOffset;
		if (Index > 0)
		{
			const FAngelscriptCachePackIndexEntry& Previous = Candidate[Index - 1];
			if (Entry.RecordId < Previous.RecordId)
			{
				return Failure(
					EAngelscriptCacheValidationError::NonCanonicalOrder,
					Stage, EntryOffset, Entry.RecordId.Kind);
			}
			if (Entry.RecordId == Previous.RecordId)
			{
				return Failure(EAngelscriptCacheValidationError::DuplicateKey,
					Stage, EntryOffset, Entry.RecordId.Kind);
			}
		}
	}
	if (ExpectedOffset < static_cast<uint64>(CompletePackBytes.Num()))
	{
		return Failure(EAngelscriptCacheValidationError::TrailingData,
			Stage, ExpectedOffset);
	}
	if (!(DirectHash(CompletePackBytes) == ExpectedPackId))
	{
		return Failure(EAngelscriptCacheValidationError::PackIdMismatch, Stage);
	}
	OutIndex = MoveTemp(Candidate);
	return {};
}
}

FAngelscriptCacheValidationResult ValidateAngelscriptCachePack(
	const TConstArrayView<uint8> CompletePackBytes,
	const FAngelscriptHash256& ExpectedPackId,
	const FAngelscriptCacheReadLimits& Limits,
	FAngelscriptCacheReadBudget& Budget,
	TArray<FAngelscriptCachePackIndexEntry>& OutIndex)
{
	FAngelscriptCacheTemporaryResidentReservation IndexReservation;
	const FAngelscriptCacheValidationResult Result =
		AngelscriptCacheManifestPack_Private::ValidatePackWithIndexReservation(
			CompletePackBytes, ExpectedPackId, Limits, Budget,
			&IndexReservation, OutIndex);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	if (!IndexReservation.PromoteToRetained())
	{
		OutIndex.Reset();
		return AngelscriptCacheManifestPack_Private::Failure(
			EAngelscriptCacheValidationError::Overflow,
			EAngelscriptCacheValidationStage::PackDecode, 20);
	}
	return {};
}

namespace AngelscriptCacheManifestPack_Private
{
	static FAngelscriptCacheValidationResult ReadRecordFromValidatedPack(
	const TConstArrayView<uint8> CompletePackBytes,
	const FAngelscriptHash256& ExpectedPackId,
	const FAngelscriptCacheRecordIndexEntry& ManifestEntry,
	const TConstArrayView<FAngelscriptCachePackIndexEntry> Index,
	const FAngelscriptCacheReadLimits& Limits,
	FAngelscriptCacheReadBudget& Budget,
	IAngelscriptCacheStorageCodec& Codec,
	FAngelscriptDecodedCacheRecordBatch* Batch,
	TOptional<FAngelscriptDecodedCacheRecordHandle>& OutRecord)
{
	OutRecord.Reset();
	const int32 EntryOrdinal = Algo::LowerBoundBy(
		Index, ManifestEntry.RecordId,
		[](const FAngelscriptCachePackIndexEntry& Entry)
		{
			return Entry.RecordId;
		});
	if (!Index.IsValidIndex(EntryOrdinal)
		|| !(Index[EntryOrdinal].RecordId == ManifestEntry.RecordId))
	{
		return Failure(EAngelscriptCacheValidationError::MissingRecord,
			EAngelscriptCacheValidationStage::PackDecode, 0,
			ManifestEntry.RecordId.Kind);
	}
	const FAngelscriptCachePackIndexEntry& Entry = Index[EntryOrdinal];
	const uint64 EntryOffset =
		FAngelscriptCacheManifestPackArchive::PackHeaderWireSize
		+ static_cast<uint64>(EntryOrdinal)
			* FAngelscriptCacheManifestPackArchive::PackIndexEntryWireSize;
	if (!(ManifestEntry.Location.PackId == ExpectedPackId))
	{
		return Failure(EAngelscriptCacheValidationError::PackIndexMismatch,
			EAngelscriptCacheValidationStage::PackDecode, 0, Entry.RecordId.Kind);
	}
	if (ManifestEntry.Location.PackOffset != Entry.PackOffset)
	{
		return Failure(EAngelscriptCacheValidationError::PackIndexMismatch,
			EAngelscriptCacheValidationStage::PackDecode, EntryOffset + 40,
			Entry.RecordId.Kind);
	}
	if (ManifestEntry.Location.StoredSize != Entry.StoredSize)
	{
		return Failure(EAngelscriptCacheValidationError::PackIndexMismatch,
			EAngelscriptCacheValidationStage::PackDecode, EntryOffset + 48,
			Entry.RecordId.Kind);
	}
	if (ManifestEntry.Location.RawSize != Entry.RawSize)
	{
		return Failure(EAngelscriptCacheValidationError::PackIndexMismatch,
			EAngelscriptCacheValidationStage::PackDecode, EntryOffset + 56,
			Entry.RecordId.Kind);
	}
	if (ManifestEntry.Location.Codec != Entry.Codec)
	{
		return Failure(EAngelscriptCacheValidationError::PackIndexMismatch,
			EAngelscriptCacheValidationStage::PackDecode, EntryOffset + 1,
			Entry.RecordId.Kind);
	}
	if (!(ManifestEntry.Location.RawChecksum == Entry.RawChecksum))
	{
		return Failure(EAngelscriptCacheValidationError::PackIndexMismatch,
			EAngelscriptCacheValidationStage::PackDecode, EntryOffset + 64,
			Entry.RecordId.Kind);
	}
	if (!Budget.TryConsumeStored(Entry.StoredSize, Limits))
	{
		return Failure(EAngelscriptCacheValidationError::BudgetExceeded,
			EAngelscriptCacheValidationStage::PackDecode, EntryOffset + 48,
			Entry.RecordId.Kind);
	}
	if (!Budget.TryConsumeDecompressed(Entry.RawSize, Limits))
	{
		return Failure(EAngelscriptCacheValidationError::BudgetExceeded,
			EAngelscriptCacheValidationStage::PackDecode, EntryOffset + 56,
			Entry.RecordId.Kind);
	}
	FAngelscriptCacheTemporaryResidentReservation RawReservation;
	if (!Budget.TryReserveTemporaryDecoded(
		Entry.RawSize, Limits, RawReservation))
	{
		return Failure(EAngelscriptCacheValidationError::BudgetExceeded,
			EAngelscriptCacheValidationStage::PackDecode, EntryOffset + 56,
			Entry.RecordId.Kind);
	}
	TArray<uint8> Raw;
	Raw.SetNumUninitialized(static_cast<int32>(Entry.RawSize));
	const TConstArrayView<uint8> Stored(
		CompletePackBytes.GetData() + Entry.PackOffset,
		static_cast<int32>(Entry.StoredSize));
	if (Entry.Codec == EAngelscriptCacheCodec::None)
	{
		if (Entry.RawSize != 0)
		{
			FMemory::Memcpy(Raw.GetData(), Stored.GetData(), Raw.Num());
		}
	}
	else
	{
		uint64 ProducedBytes = 0;
		if (!Codec.TryDecompressCanonicalZlib(Stored, Raw, ProducedBytes))
		{
			return Failure(EAngelscriptCacheValidationError::DecompressionFailed,
				EAngelscriptCacheValidationStage::PackDecode, Entry.PackOffset,
				Entry.RecordId.Kind);
		}
		if (ProducedBytes != Entry.RawSize)
		{
			return Failure(
				EAngelscriptCacheValidationError::DecompressedSizeMismatch,
				EAngelscriptCacheValidationStage::PackDecode, Entry.PackOffset,
				Entry.RecordId.Kind);
		}
		FAngelscriptCacheTemporaryResidentReservation CanonicalReservation;
		if (!Budget.TryReserveTemporaryDecoded(
			Entry.StoredSize, Limits, CanonicalReservation))
		{
			return Failure(EAngelscriptCacheValidationError::BudgetExceeded,
				EAngelscriptCacheValidationStage::PackDecode, EntryOffset + 48,
				Entry.RecordId.Kind);
		}
		TArray<uint8> Recompressed;
		Recompressed.SetNumUninitialized(static_cast<int32>(Entry.StoredSize));
		uint64 RecompressedBytes = 0;
		if (!Codec.TryCompressCanonicalZlibInto(
				Raw, Recompressed, RecompressedBytes)
			|| RecompressedBytes != Entry.StoredSize
			|| Recompressed.Num() != Stored.Num()
			|| FMemory::Memcmp(Recompressed.GetData(), Stored.GetData(),
				Recompressed.Num()) != 0)
		{
			return Failure(EAngelscriptCacheValidationError::DecompressionFailed,
				EAngelscriptCacheValidationStage::PackDecode, Entry.PackOffset,
				Entry.RecordId.Kind);
		}
		Recompressed.Empty();
		CanonicalReservation.Reset();
	}
	if (!(DirectHash(Raw) == Entry.RawChecksum))
	{
		return Failure(EAngelscriptCacheValidationError::ChecksumMismatch,
			EAngelscriptCacheValidationStage::PackDecode, EntryOffset + 64,
			Entry.RecordId.Kind);
	}
	FAngelscriptCacheRecordId ComputedRecordId;
	const FAngelscriptCacheValidationResult IdResult =
		FAngelscriptCacheRecordArchive::TryBuildRecordId(
			Entry.RecordId.Kind, Raw, ComputedRecordId);
	if (!IdResult.IsSuccess() || !(ComputedRecordId == Entry.RecordId))
	{
		return Failure(EAngelscriptCacheValidationError::RecordIdMismatch,
			EAngelscriptCacheValidationStage::PackDecode, EntryOffset + 8,
			Entry.RecordId.Kind);
	}
	return Batch != nullptr
		? Batch->TryDecode(Entry.RecordId, Raw, OutRecord)
		: FAngelscriptDecodedCacheRecord::TryDecode(
			Entry.RecordId, Raw, Limits, Budget, OutRecord);
}
}

FAngelscriptCacheValidationResult ReadAngelscriptCacheRecordFromPack(
	const TConstArrayView<uint8> CompletePackBytes,
	const FAngelscriptHash256& ExpectedPackId,
	const FAngelscriptCacheRecordIndexEntry& ManifestEntry,
	const FAngelscriptCacheReadLimits& Limits,
	FAngelscriptCacheReadBudget& Budget,
	IAngelscriptCacheStorageCodec& Codec,
	TOptional<FAngelscriptDecodedCacheRecordHandle>& OutRecord)
{
	using namespace AngelscriptCacheManifestPack_Private;
	OutRecord.Reset();
	FAngelscriptCacheTemporaryResidentReservation IndexReservation;
	TArray<FAngelscriptCachePackIndexEntry> Index;
	const FAngelscriptCacheValidationResult PackResult =
		ValidatePackWithIndexReservation(CompletePackBytes, ExpectedPackId,
			Limits, Budget, &IndexReservation, Index);
	if (!PackResult.IsSuccess())
	{
		return PackResult;
	}
	return ReadRecordFromValidatedPack(
		CompletePackBytes, ExpectedPackId, ManifestEntry, Index,
		Limits, Budget, Codec, nullptr, OutRecord);
}

FAngelscriptCacheValidationResult
ValidateAngelscriptCacheGenerationManifestValue(
	const FAngelscriptCacheGenerationManifest& Value,
	const FAngelscriptCacheReadLimits& Limits)
{
	using namespace AngelscriptCacheManifestPack_Private;
	return ValidateManifestValue(Value, Limits);
}

FAngelscriptCacheValidationResult EncodeAngelscriptCacheGenerationManifest(
	const FAngelscriptCacheGenerationManifest& Value,
	FAngelscriptEncodedCacheGenerationManifest& OutManifest)
{
	using namespace AngelscriptCacheManifestPack_Private;
	OutManifest = {};
	const FAngelscriptCacheValidationResult Validation =
		ValidateAngelscriptCacheGenerationManifestValue(
			Value, FAngelscriptCacheReadLimits{});
	if (!Validation.IsSuccess())
	{
		return Validation;
	}
	uint64 Size = 181;
	uint64 RootBytes = 0;
	uint64 RecordBytes = 0;
	if (!TryMultiply(static_cast<uint64>(Value.ModuleSnapshots.Num()),
			FAngelscriptCacheManifestPackArchive::ManifestRootWireSize,
			RootBytes)
		|| !TryMultiply(static_cast<uint64>(Value.Records.Num()),
			FAngelscriptCacheManifestPackArchive::ManifestLocationWireSize,
			RecordBytes)
		|| !TryAdd(Size, RootBytes, Size)
		|| !TryAdd(Size, 4, Size)
		|| !TryAdd(Size, RecordBytes, Size))
	{
		return Failure(EAngelscriptCacheValidationError::Overflow,
			EAngelscriptCacheValidationStage::ManifestDecode);
	}
	if (Size > FAngelscriptCacheReadLimits::DefaultMaxManifestBytes
		|| Size > static_cast<uint64>(MAX_int32))
	{
		return Failure(EAngelscriptCacheValidationError::BudgetExceeded,
			EAngelscriptCacheValidationStage::ManifestDecode);
	}
	OutManifest.CompleteBytes.Reserve(static_cast<int32>(Size));
	OutManifest.CompleteBytes.Append(
		reinterpret_cast<const uint8*>(ManifestMagic), 8);
	AppendUInt32(OutManifest.CompleteBytes, Value.ManifestSchemaVersion);
	AppendUInt32(OutManifest.CompleteBytes, Value.ManifestFlags);
	AppendHash(OutManifest.CompleteBytes, Value.Compatibility.Hash);
	AppendHash(OutManifest.CompleteBytes, Value.Context.Hash);
	AppendHash(OutManifest.CompleteBytes, Value.Profile.Hash);
	AppendHash(OutManifest.CompleteBytes, Value.SourceSnapshot);
	AppendRecordId(OutManifest.CompleteBytes, Value.SourceIndexRecordId);
	AppendUInt32(OutManifest.CompleteBytes,
		static_cast<uint32>(Value.ModuleSnapshots.Num()));
	for (const FAngelscriptCacheModuleSnapshotLink& Link : Value.ModuleSnapshots)
	{
		AppendHash(OutManifest.CompleteBytes, Link.ModuleKey.Hash);
		AppendRecordId(OutManifest.CompleteBytes, Link.RecordId);
	}
	AppendUInt32(OutManifest.CompleteBytes,
		static_cast<uint32>(Value.Records.Num()));
	for (const FAngelscriptCacheRecordIndexEntry& Entry : Value.Records)
	{
		AppendRecordId(OutManifest.CompleteBytes, Entry.RecordId);
		AppendHash(OutManifest.CompleteBytes, Entry.Location.PackId);
		AppendUInt64(OutManifest.CompleteBytes, Entry.Location.PackOffset);
		AppendUInt64(OutManifest.CompleteBytes, Entry.Location.StoredSize);
		AppendUInt64(OutManifest.CompleteBytes, Entry.Location.RawSize);
		OutManifest.CompleteBytes.Add(static_cast<uint8>(Entry.Location.Codec));
		AppendHash(OutManifest.CompleteBytes, Entry.Location.RawChecksum);
	}
	check(static_cast<uint64>(OutManifest.CompleteBytes.Num()) == Size);
	OutManifest.ComputedGenerationId = DirectHash(OutManifest.CompleteBytes);
	return {};
}

struct AngelscriptCacheManifestPack_Private::FPreparedGenerationValidation::FState
{
	FAngelscriptCacheReadLimits Limits;
	FAngelscriptCacheReadBudget* Budget = nullptr;
	FAngelscriptCacheTemporaryResidentReservation RetainedReservation;
	FAngelscriptCacheTemporaryResidentReservation DistinctPackReservation;
	FDecodedManifestCandidate DecodedManifest;
};

AngelscriptCacheManifestPack_Private::FPreparedGenerationValidation::
	FPreparedGenerationValidation() = default;

AngelscriptCacheManifestPack_Private::FPreparedGenerationValidation::
	~FPreparedGenerationValidation() = default;

AngelscriptCacheManifestPack_Private::FPreparedGenerationValidation::
	FPreparedGenerationValidation(FPreparedGenerationValidation&& Other) noexcept = default;

AngelscriptCacheManifestPack_Private::FPreparedGenerationValidation&
AngelscriptCacheManifestPack_Private::FPreparedGenerationValidation::operator=(
	FPreparedGenerationValidation&& Other) noexcept = default;

bool AngelscriptCacheManifestPack_Private::FPreparedGenerationValidation::
	IsPrepared() const
{
	return State.IsValid();
}

const FAngelscriptCacheGenerationManifest&
AngelscriptCacheManifestPack_Private::FPreparedGenerationValidation::
	GetManifest() const
{
	check(State.IsValid());
	return State->DecodedManifest.Manifest;
}

TConstArrayView<FAngelscriptHash256>
AngelscriptCacheManifestPack_Private::FPreparedGenerationValidation::
	GetDistinctPackIds() const
{
	check(State.IsValid());
	return State->DecodedManifest.DistinctPackIds;
}

FAngelscriptCacheValidationResult
AngelscriptCacheManifestPack_Private::PrepareGenerationValidation(
	const TConstArrayView<uint8> CompleteManifestBytes,
	const FAngelscriptHash256& ExpectedGenerationId,
	const FAngelscriptCacheReadLimits& Limits,
	FAngelscriptCacheReadBudget& Budget,
	FPreparedGenerationValidation& OutPrepared)
{
	OutPrepared = FPreparedGenerationValidation{};
	TUniquePtr<FPreparedGenerationValidation::FState> State =
		MakeUnique<FPreparedGenerationValidation::FState>();
	State->Limits = Limits;
	State->Budget = &Budget;
	const FAngelscriptCacheValidationResult Result = DecodeManifest(
		CompleteManifestBytes,
		ExpectedGenerationId,
		Limits,
		Budget,
		State->RetainedReservation,
		State->DistinctPackReservation,
		State->DecodedManifest);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	OutPrepared.State = MoveTemp(State);
	return {};
}

FAngelscriptCacheValidationResult
AngelscriptCacheManifestPack_Private::CompleteGenerationValidation(
	FPreparedGenerationValidation&& Prepared,
	IAngelscriptCachePackSource& Packs,
	IAngelscriptCacheStorageCodec& Codec,
	TOptional<FAngelscriptValidatedGeneration>& OutGeneration)
{
	OutGeneration.Reset();
	TUniquePtr<FPreparedGenerationValidation::FState> State =
		MoveTemp(Prepared.State);
	if (!State.IsValid() || State->Budget == nullptr)
	{
		return Failure(
			EAngelscriptCacheValidationError::Overflow,
			EAngelscriptCacheValidationStage::ManifestDecode);
	}
	const FAngelscriptCacheReadLimits& Limits = State->Limits;
	FAngelscriptCacheReadBudget& Budget = *State->Budget;
	FAngelscriptCacheTemporaryResidentReservation& RetainedReservation =
		State->RetainedReservation;
	FAngelscriptCacheTemporaryResidentReservation& DistinctPackReservation =
		State->DistinctPackReservation;
	FDecodedManifestCandidate& DecodedManifest = State->DecodedManifest;

	struct FLoadedPack
	{
		FAngelscriptHash256 PackId;
		TConstArrayView<uint8> CompleteBytes;
		FAngelscriptCacheTemporaryResidentReservation IndexReservation;
		TArray<FAngelscriptCachePackIndexEntry> Index;
	};
	const uint64 RecordCount = static_cast<uint64>(
		DecodedManifest.Manifest.Records.Num());
	const uint64 RootCount = static_cast<uint64>(
		DecodedManifest.Manifest.ModuleSnapshots.Num());
	const uint64 PackCount = static_cast<uint64>(
		DecodedManifest.DistinctPackIds.Num());

	uint64 ReferenceCharge = 0;
	if (!TryAdd(RecordCount, RootCount, ReferenceCharge)
		|| !TryAdd(ReferenceCharge, PackCount, ReferenceCharge))
	{
		return Failure(EAngelscriptCacheValidationError::Overflow,
			EAngelscriptCacheValidationStage::ManifestDecode,
			DecodedManifest.RecordCountOffset);
	}
	if (!Budget.TryConsumeReferencesAndRelocations(ReferenceCharge, Limits))
	{
		return Failure(EAngelscriptCacheValidationError::BudgetExceeded,
			EAngelscriptCacheValidationStage::ManifestDecode,
			DecodedManifest.RecordCountOffset);
	}

	uint64 ScratchBytes = 0;
	uint64 PartBytes = 0;
	if (!TryMultiply(PackCount,
			static_cast<uint64>(sizeof(FLoadedPack)), ScratchBytes)
		|| !TryMultiply(RecordCount,
			static_cast<uint64>(sizeof(int32) + sizeof(bool)), PartBytes)
		|| !TryAdd(ScratchBytes, PartBytes, ScratchBytes))
	{
		return Failure(EAngelscriptCacheValidationError::Overflow,
			EAngelscriptCacheValidationStage::ManifestDecode,
			DecodedManifest.RecordCountOffset);
	}
	FAngelscriptCacheTemporaryResidentReservation ScratchReservation;
	if (!Budget.TryReserveTemporaryDecoded(
		ScratchBytes, Limits, ScratchReservation))
	{
		return Failure(EAngelscriptCacheValidationError::BudgetExceeded,
			EAngelscriptCacheValidationStage::ManifestDecode,
			DecodedManifest.RecordCountOffset);
	}

	TArray<FLoadedPack> LoadedPacks;
	LoadedPacks.Reserve(DecodedManifest.DistinctPackIds.Num());
	for (const FAngelscriptHash256& PackId : DecodedManifest.DistinctPackIds)
	{
		FLoadedPack& Loaded = LoadedPacks.AddDefaulted_GetRef();
		Loaded.PackId = PackId;
		if (!Packs.TryGetCompletePack(PackId, Loaded.CompleteBytes))
		{
			return Failure(EAngelscriptCacheValidationError::MissingRecord,
				EAngelscriptCacheValidationStage::PackDecode);
		}
		const FAngelscriptCacheValidationResult PackResult =
			ValidatePackWithIndexReservation(
				Loaded.CompleteBytes, PackId, Limits, Budget,
				&Loaded.IndexReservation, Loaded.Index);
		if (!PackResult.IsSuccess())
		{
			return PackResult;
		}
	}
	DecodedManifest.DistinctPackIds.Empty();
	DistinctPackReservation.Reset();

	FAngelscriptDecodedCacheRecordBatch DecodedBatch(Budget, Limits);
	TArray<FAngelscriptDecodedCacheRecordHandle> DecodedRecords;
	DecodedRecords.Reserve(DecodedManifest.Manifest.Records.Num());
	for (const FAngelscriptCacheRecordIndexEntry& Entry :
		DecodedManifest.Manifest.Records)
	{
		const int32 PackOrdinal = Algo::LowerBoundBy(
			LoadedPacks, Entry.Location.PackId,
			[](const FLoadedPack& Loaded)
			{
				return Loaded.PackId;
			});
		if (!LoadedPacks.IsValidIndex(PackOrdinal)
			|| !(LoadedPacks[PackOrdinal].PackId == Entry.Location.PackId))
		{
			return Failure(EAngelscriptCacheValidationError::MissingRecord,
				EAngelscriptCacheValidationStage::PackDecode, 0,
				Entry.RecordId.Kind);
		}
		TOptional<FAngelscriptDecodedCacheRecordHandle> Record;
		const FAngelscriptCacheValidationResult RecordResult =
			ReadRecordFromValidatedPack(
				LoadedPacks[PackOrdinal].CompleteBytes,
				LoadedPacks[PackOrdinal].PackId,
				Entry, LoadedPacks[PackOrdinal].Index,
				Limits, Budget, Codec, &DecodedBatch, Record);
		if (!RecordResult.IsSuccess())
		{
			return RecordResult;
		}
		check(Record.IsSet());
		DecodedRecords.Add(Record.GetValue());
	}

	const FAngelscriptCacheValidationResult ReachabilityResult =
		ValidateDecodedGenerationReachability(DecodedManifest, DecodedRecords);
	if (!ReachabilityResult.IsSuccess())
	{
		return ReachabilityResult;
	}

	if (!DecodedBatch.PromoteToRetained())
	{
		return Failure(EAngelscriptCacheValidationError::Overflow,
			EAngelscriptCacheValidationStage::ManifestGraph,
			DecodedManifest.RecordCountOffset);
	}
	LoadedPacks.Empty();
	ScratchReservation.Reset();
	if (!RetainedReservation.PromoteToRetained())
	{
		return Failure(EAngelscriptCacheValidationError::Overflow,
			EAngelscriptCacheValidationStage::ManifestGraph,
			DecodedManifest.RecordCountOffset);
	}
	FAngelscriptValidatedGeneration Validated;
	Validated.Manifest = MoveTemp(DecodedManifest.Manifest);
	Validated.ReachableRecords = MoveTemp(DecodedRecords);
	OutGeneration = MoveTemp(Validated);
	return {};
}

FAngelscriptCacheValidationResult ValidateAngelscriptCacheGeneration(
	const TConstArrayView<uint8> CompleteManifestBytes,
	const FAngelscriptHash256& ExpectedGenerationId,
	IAngelscriptCachePackSource& Packs,
	const FAngelscriptCacheReadLimits& Limits,
	FAngelscriptCacheReadBudget& Budget,
	IAngelscriptCacheStorageCodec& Codec,
	TOptional<FAngelscriptValidatedGeneration>& OutGeneration)
{
	AngelscriptCacheManifestPack_Private::FPreparedGenerationValidation Prepared;
	const FAngelscriptCacheValidationResult PrepareResult =
		AngelscriptCacheManifestPack_Private::PrepareGenerationValidation(
			CompleteManifestBytes,
			ExpectedGenerationId,
			Limits,
			Budget,
			Prepared);
	if (!PrepareResult.IsSuccess())
	{
		OutGeneration.Reset();
		return PrepareResult;
	}
	return AngelscriptCacheManifestPack_Private::CompleteGenerationValidation(
		MoveTemp(Prepared), Packs, Codec, OutGeneration);
}

#if WITH_ANGELSCRIPT_UNITTESTS
FAngelscriptCacheValidationResult ValidateAngelscriptCacheGenerationReachabilityForTests(
	const FAngelscriptHash256& ManifestSourceSnapshot,
	const FAngelscriptCacheRecordId& SourceIndexRecordId,
	const uint64 SourceIndexManifestByteOffset,
	const TConstArrayView<FAngelscriptCacheReachabilityRootForTests> Roots,
	const TConstArrayView<FAngelscriptCacheReachabilityManifestEntryForTests> ManifestIndex,
	const TConstArrayView<FAngelscriptCacheReachabilityNodeForTests> Nodes,
	const FAngelscriptCacheReadLimits& Limits,
	FAngelscriptCacheReadBudget& Budget,
	TArray<FAngelscriptCacheRecordId>& OutVisited,
	FAngelscriptCacheModuleGraphValidationProbeForTests* Probe)
{
	using namespace AngelscriptCacheManifestPack_Private;
	OutVisited.Reset();
	if (Probe != nullptr)
	{
		Probe->CallCount = 0;
		Probe->bOverflow = false;
	}
	const EAngelscriptCacheValidationStage Stage =
		EAngelscriptCacheValidationStage::ManifestGraph;
	if (Roots.Num() < 0 || ManifestIndex.Num() < 0 || Nodes.Num() < 0
		|| (Roots.Num() != 0 && Roots.GetData() == nullptr)
		|| (ManifestIndex.Num() != 0 && ManifestIndex.GetData() == nullptr)
		|| (Nodes.Num() != 0 && Nodes.GetData() == nullptr))
	{
		return Failure(EAngelscriptCacheValidationError::InvalidArrayView, Stage);
	}
	if (static_cast<uint64>(Roots.Num()) > Limits.MaxModuleSnapshots
		|| static_cast<uint64>(ManifestIndex.Num()) > Limits.MaxGenerationRecords)
	{
		return Failure(EAngelscriptCacheValidationError::BudgetExceeded, Stage);
	}
	if (!Budget.TryConsumeReferencesAndRelocations(
		static_cast<uint64>(ManifestIndex.Num())
			+ static_cast<uint64>(Roots.Num()), Limits))
	{
		return Failure(EAngelscriptCacheValidationError::BudgetExceeded, Stage);
	}

	const auto FindManifestEntry = [ManifestIndex](
		const FAngelscriptCacheRecordId& RecordId) ->
		const FAngelscriptCacheReachabilityManifestEntryForTests*
	{
		int32 First = 0;
		int32 Last = ManifestIndex.Num();
		while (First < Last)
		{
			const int32 Middle = First + (Last - First) / 2;
			if (ManifestIndex[Middle].RecordId < RecordId)
			{
				First = Middle + 1;
			}
			else
			{
				Last = Middle;
			}
		}
		return ManifestIndex.IsValidIndex(First)
			&& ManifestIndex[First].RecordId == RecordId
			? &ManifestIndex[First] : nullptr;
	};
	const auto FindNode = [Nodes](const FAngelscriptCacheRecordId& RecordId) ->
		const FAngelscriptCacheReachabilityNodeForTests*
	{
		for (const FAngelscriptCacheReachabilityNodeForTests& Node : Nodes)
		{
			if (Node.RecordId == RecordId)
			{
				return &Node;
			}
		}
		return nullptr;
	};
	const auto RequireTarget = [&](
		const FAngelscriptCacheRecordId& Target,
		const EAngelscriptCacheRecordKind ExpectedKind,
		const uint64 OwnerOffset) -> FAngelscriptCacheValidationResult
	{
		if (Target.Kind != ExpectedKind)
		{
			return Failure(EAngelscriptCacheValidationError::WrongRecordKind,
				Stage, OwnerOffset, ExpectedKind);
		}
		if (FindManifestEntry(Target) == nullptr || FindNode(Target) == nullptr)
		{
			return Failure(EAngelscriptCacheValidationError::MissingRecord,
				Stage, OwnerOffset, ExpectedKind);
		}
		return {};
	};

	FAngelscriptCacheValidationResult Result = RequireTarget(
		SourceIndexRecordId, EAngelscriptCacheRecordKind::SourceIndex,
		SourceIndexManifestByteOffset);
	if (!Result.IsSuccess())
	{
		return Result;
	}
	for (const FAngelscriptCacheReachabilityRootForTests& Root : Roots)
	{
		Result = RequireTarget(Root.Link.RecordId,
			EAngelscriptCacheRecordKind::ModuleSnapshot,
			Root.ManifestByteOffset);
		if (!Result.IsSuccess())
		{
			return Result;
		}
	}

	TArray<FAngelscriptCacheRecordId> Pending;
	TArray<FAngelscriptCacheRecordId> CandidateVisited;
	Pending.Reserve(1 + Roots.Num());
	CandidateVisited.Reserve(ManifestIndex.Num());
	Pending.Add(SourceIndexRecordId);
	for (const FAngelscriptCacheReachabilityRootForTests& Root : Roots)
	{
		Pending.Add(Root.Link.RecordId);
	}
	while (!Pending.IsEmpty())
	{
		const FAngelscriptCacheRecordId Current = Pending.Pop(EAllowShrinking::No);
		if (CandidateVisited.Contains(Current))
		{
			continue;
		}
		const FAngelscriptCacheReachabilityManifestEntryForTests* CurrentEntry =
			FindManifestEntry(Current);
		const FAngelscriptCacheReachabilityNodeForTests* CurrentNode = FindNode(Current);
		if (CurrentEntry == nullptr || CurrentNode == nullptr)
		{
			return Failure(EAngelscriptCacheValidationError::MissingRecord,
				Stage, 0, Current.Kind);
		}
		CandidateVisited.Add(Current);

		if (Current.Kind == EAngelscriptCacheRecordKind::ModuleSnapshot)
		{
			Result = RequireTarget(CurrentNode->ModuleInterface,
				EAngelscriptCacheRecordKind::ModuleInterface,
				CurrentEntry->ManifestByteOffset);
			if (!Result.IsSuccess()) return Result;
			Result = RequireTarget(CurrentNode->ModuleState,
				EAngelscriptCacheRecordKind::ModuleState,
				CurrentEntry->ManifestByteOffset);
			if (!Result.IsSuccess()) return Result;
			for (const FAngelscriptCacheRecordId& Type : CurrentNode->TypeSchemas)
			{
				Result = RequireTarget(Type,
					EAngelscriptCacheRecordKind::TypeSchema,
					CurrentEntry->ManifestByteOffset);
				if (!Result.IsSuccess()) return Result;
			}
			for (const FAngelscriptCacheRecordId& Function :
				CurrentNode->FunctionBodies)
			{
				Result = RequireTarget(Function,
					EAngelscriptCacheRecordKind::FunctionBody,
					CurrentEntry->ManifestByteOffset);
				if (!Result.IsSuccess()) return Result;
			}
			Pending.Add(CurrentNode->ModuleInterface);
			Pending.Add(CurrentNode->ModuleState);
			Pending.Append(CurrentNode->TypeSchemas);
			Pending.Append(CurrentNode->FunctionBodies);
		}
		else if (Current.Kind == EAngelscriptCacheRecordKind::FunctionBody
			&& !CurrentNode->DebugSidecar.ContentHash.IsZero())
		{
			Result = RequireTarget(CurrentNode->DebugSidecar,
				EAngelscriptCacheRecordKind::DebugSidecar,
				CurrentEntry->ManifestByteOffset);
			if (!Result.IsSuccess()) return Result;
			Pending.Add(CurrentNode->DebugSidecar);
		}
	}

	CandidateVisited.Sort();
	for (const FAngelscriptCacheReachabilityManifestEntryForTests& Entry :
		ManifestIndex)
	{
		if (!CandidateVisited.Contains(Entry.RecordId))
		{
			return Failure(EAngelscriptCacheValidationError::UnexpectedRecord,
				Stage, Entry.ManifestByteOffset, Entry.RecordId.Kind);
		}
	}
	const FAngelscriptCacheReachabilityNodeForTests* SourceNode =
		FindNode(SourceIndexRecordId);
	const FAngelscriptCacheReachabilityManifestEntryForTests* SourceEntry =
		FindManifestEntry(SourceIndexRecordId);
	check(SourceNode != nullptr && SourceEntry != nullptr);
	if (!SourceNode->EmbeddedSourceSnapshot.IsSet()
		|| !(SourceNode->EmbeddedSourceSnapshot.GetValue()
			== ManifestSourceSnapshot))
	{
		return Failure(EAngelscriptCacheValidationError::SourceSnapshotMismatch,
			Stage, SourceEntry->ManifestByteOffset,
			EAngelscriptCacheRecordKind::SourceIndex);
	}
	for (const FAngelscriptCacheReachabilityRootForTests& Root : Roots)
	{
		const FAngelscriptCacheReachabilityNodeForTests* RootNode =
			FindNode(Root.Link.RecordId);
		const FAngelscriptCacheReachabilityManifestEntryForTests* RootEntry =
			FindManifestEntry(Root.Link.RecordId);
		check(RootNode != nullptr && RootEntry != nullptr);
		if (!RootNode->EmbeddedModuleKey.IsSet()
			|| !(RootNode->EmbeddedModuleKey.GetValue() == Root.Link.ModuleKey))
		{
			return Failure(EAngelscriptCacheValidationError::CrossModuleOwner,
				Stage, RootEntry->ManifestByteOffset,
				EAngelscriptCacheRecordKind::ModuleSnapshot);
		}
	}
	if (Probe != nullptr)
	{
		for (const FAngelscriptCacheReachabilityRootForTests& Root : Roots)
		{
			if (Probe->CallCount >= static_cast<uint64>(Probe->ModuleKeys.Num()))
			{
				Probe->bOverflow = true;
			}
			else
			{
				Probe->ModuleKeys[Probe->CallCount] = Root.Link.ModuleKey;
			}
			++Probe->CallCount;
		}
	}
	const uint64 RetainedBytes =
		static_cast<uint64>(CandidateVisited.GetAllocatedSize());
	if (!Budget.TryConsumeRetainedDecoded(RetainedBytes, Limits))
	{
		return Failure(EAngelscriptCacheValidationError::BudgetExceeded, Stage);
	}
	OutVisited = MoveTemp(CandidateVisited);
	return {};
}
#endif
