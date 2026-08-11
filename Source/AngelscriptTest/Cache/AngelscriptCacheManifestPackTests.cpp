#include "Cache/AngelscriptCacheManifestPack.h"
#include "Cache/AngelscriptCacheArchive.h"
#include "Cache/AngelscriptCacheSemanticRecords.h"

#include "CQTest.h"

#include <type_traits>

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheManifestPackTests,
	"Angelscript.TestModule.Cache.PackFormat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static constexpr int32 ManifestSchemaOffset = 8;
	static constexpr int32 ManifestFlagsOffset = 12;
	static constexpr int32 ManifestModuleCountOffset = 177;
	static constexpr int32 MinimumManifestRecordCountOffset = 181;
	static constexpr int32 MinimumManifestRecordEntryOffset = 185;
	static constexpr int32 PackSchemaOffset = 8;
	static constexpr int32 PackHeaderSizeOffset = 12;
	static constexpr int32 PackIndexEntrySizeOffset = 16;
	static constexpr int32 PackIndexCountOffset = 20;
	static constexpr int32 PackDataOffsetOffset = 24;
	static constexpr int32 FirstPackIndexOffset = 32;
	static constexpr int32 PackIndexCodecOffset = FirstPackIndexOffset + 1;
	static constexpr int32 PackIndexPackOffsetOffset = FirstPackIndexOffset + 40;
	static constexpr int32 PackIndexStoredSizeOffset = FirstPackIndexOffset + 48;
	static constexpr int32 PackIndexRawSizeOffset = FirstPackIndexOffset + 56;
	static constexpr int32 PackIndexRawChecksumOffset = FirstPackIndexOffset + 64;

	static uint8 HexDigit(const TCHAR Character)
	{
		if (Character >= TEXT('0') && Character <= TEXT('9'))
		{
			return static_cast<uint8>(Character - TEXT('0'));
		}
		if (Character >= TEXT('a') && Character <= TEXT('f'))
		{
			return static_cast<uint8>(Character - TEXT('a') + 10);
		}
		check(Character >= TEXT('A') && Character <= TEXT('F'));
		return static_cast<uint8>(Character - TEXT('A') + 10);
	}

	static TArray<uint8> BytesFromHex(const FStringView Hex)
	{
		check((Hex.Len() % 2) == 0);
		TArray<uint8> Bytes;
		Bytes.Reserve(Hex.Len() / 2);
		for (int32 Index = 0; Index < Hex.Len(); Index += 2)
		{
			Bytes.Add(static_cast<uint8>((HexDigit(Hex[Index]) << 4) | HexDigit(Hex[Index + 1])));
		}
		return Bytes;
	}

	static FAngelscriptHash256 HashFromHex(const FStringView Hex)
	{
		const TArray<uint8> Bytes = BytesFromHex(Hex);
		check(Bytes.Num() == 32);
		FBlake3Hash::ByteArray HashBytes{};
		FMemory::Memcpy(HashBytes, Bytes.GetData(), sizeof(HashBytes));
		return FAngelscriptHash256{FBlake3Hash(HashBytes)};
	}

	static FAngelscriptCacheRecordId RecordIdFromHex(const FStringView Hex)
	{
		const TArray<uint8> Bytes = BytesFromHex(Hex);
		check(Bytes.Num() == 33);
		FBlake3Hash::ByteArray HashBytes{};
		FMemory::Memcpy(HashBytes, Bytes.GetData() + 1, sizeof(HashBytes));
		return FAngelscriptCacheRecordId{
			static_cast<EAngelscriptCacheRecordKind>(Bytes[0]),
			FAngelscriptHash256{FBlake3Hash(HashBytes)}};
	}

	static FString Hex(const TConstArrayView<uint8> Bytes)
	{
		return BytesToHexLower(Bytes.GetData(), Bytes.Num());
	}

	static FAngelscriptHash256 DirectHash(const TConstArrayView<uint8> Bytes)
	{
		return FAngelscriptHash256{FBlake3::HashBuffer(
			Bytes.GetData(), static_cast<uint64>(Bytes.Num()))};
	}

	static uint32 ReadUInt32LittleEndian(const TConstArrayView<uint8> Bytes, const int32 Offset)
	{
		check(Bytes.IsValidIndex(Offset + 3));
		return static_cast<uint32>(Bytes[Offset])
			| (static_cast<uint32>(Bytes[Offset + 1]) << 8)
			| (static_cast<uint32>(Bytes[Offset + 2]) << 16)
			| (static_cast<uint32>(Bytes[Offset + 3]) << 24);
	}

	static uint64 ReadUInt64LittleEndian(const TConstArrayView<uint8> Bytes, const int32 Offset)
	{
		check(Bytes.IsValidIndex(Offset + 7));
		uint64 Value = 0;
		for (uint32 Shift = 0; Shift < 64; Shift += 8)
		{
			Value |= static_cast<uint64>(Bytes[Offset + static_cast<int32>(Shift / 8)]) << Shift;
		}
		return Value;
	}

	static void WriteUInt32LittleEndian(TArray<uint8>& Bytes, const int32 Offset, const uint32 Value)
	{
		check(Bytes.IsValidIndex(Offset + 3));
		Bytes[Offset] = static_cast<uint8>(Value);
		Bytes[Offset + 1] = static_cast<uint8>(Value >> 8);
		Bytes[Offset + 2] = static_cast<uint8>(Value >> 16);
		Bytes[Offset + 3] = static_cast<uint8>(Value >> 24);
	}

	static void WriteUInt64LittleEndian(TArray<uint8>& Bytes, const int32 Offset, const uint64 Value)
	{
		check(Bytes.IsValidIndex(Offset + 7));
		for (uint32 Shift = 0; Shift < 64; Shift += 8)
		{
			Bytes[Offset + static_cast<int32>(Shift / 8)] = static_cast<uint8>(Value >> Shift);
		}
	}

	static void AppendUInt32LittleEndian(TArray<uint8>& Bytes, const uint32 Value)
	{
		const int32 Offset = Bytes.AddUninitialized(sizeof(uint32));
		WriteUInt32LittleEndian(Bytes, Offset, Value);
	}

	static void AppendUInt64LittleEndian(TArray<uint8>& Bytes, const uint64 Value)
	{
		const int32 Offset = Bytes.AddUninitialized(sizeof(uint64));
		WriteUInt64LittleEndian(Bytes, Offset, Value);
	}

	static void AppendHash(TArray<uint8>& Bytes, const FAngelscriptHash256& Hash)
	{
		Bytes.Append(Hash.Value.GetBytes(), 32);
	}

	static void AppendRecordId(TArray<uint8>& Bytes, const FAngelscriptCacheRecordId& RecordId)
	{
		Bytes.Add(static_cast<uint8>(RecordId.Kind));
		AppendHash(Bytes, RecordId.ContentHash);
	}

	static FAngelscriptHash256 OrdinalHash(const uint32 Ordinal, const uint8 Prefix)
	{
		FBlake3Hash::ByteArray Bytes{};
		Bytes[0] = Prefix;
		Bytes[28] = static_cast<uint8>(Ordinal >> 24);
		Bytes[29] = static_cast<uint8>(Ordinal >> 16);
		Bytes[30] = static_cast<uint8>(Ordinal >> 8);
		Bytes[31] = static_cast<uint8>(Ordinal);
		return FAngelscriptHash256{FBlake3Hash(Bytes)};
	}

	struct FReadBudgetSnapshot
	{
		uint64 StoredBytes = 0;
		uint64 DecompressedBytes = 0;
		uint64 DecodedBytes = 0;
		uint64 ResidentDecodedBytes = 0;
		uint64 TemporaryResidentDecodedBytes = 0;
		uint64 PeakLiveResidentDecodedBytes = 0;
		uint64 ReferencesAndRelocations = 0;
	};

	static FReadBudgetSnapshot CaptureBudget(const FAngelscriptCacheReadBudget& Budget)
	{
		return FReadBudgetSnapshot{
			Budget.GetStoredBytes(),
			Budget.GetDecompressedBytes(),
			Budget.GetDecodedBytes(),
			Budget.GetResidentDecodedBytes(),
			Budget.GetTemporaryResidentDecodedBytes(),
			Budget.GetPeakLiveResidentDecodedBytes(),
			Budget.GetReferencesAndRelocations()};
	}

	static bool ExpectBudgetUnchanged(
		FAutomationTestBase& Test,
		const FReadBudgetSnapshot& Expected,
		const FAngelscriptCacheReadBudget& Actual,
		const TCHAR* Message)
	{
		FNoDiscardAsserter LocalAssert(Test);
		const FReadBudgetSnapshot ActualSnapshot = CaptureBudget(Actual);
		bool bPassed = LocalAssert.AreEqual(
			Expected.StoredBytes, ActualSnapshot.StoredBytes, Message);
		bPassed &= LocalAssert.AreEqual(
			Expected.DecompressedBytes, ActualSnapshot.DecompressedBytes, Message);
		bPassed &= LocalAssert.AreEqual(
			Expected.DecodedBytes, ActualSnapshot.DecodedBytes, Message);
		bPassed &= LocalAssert.AreEqual(
			Expected.ResidentDecodedBytes, ActualSnapshot.ResidentDecodedBytes, Message);
		bPassed &= LocalAssert.AreEqual(
			Expected.TemporaryResidentDecodedBytes,
			ActualSnapshot.TemporaryResidentDecodedBytes, Message);
		bPassed &= LocalAssert.AreEqual(
			Expected.PeakLiveResidentDecodedBytes,
			ActualSnapshot.PeakLiveResidentDecodedBytes, Message);
		bPassed &= LocalAssert.AreEqual(
			Expected.ReferencesAndRelocations,
			ActualSnapshot.ReferencesAndRelocations, Message);
		return bPassed;
	}

	class FReportedDecompressedSizeCodec final : public IAngelscriptCacheStorageCodec
	{
	public:
		explicit FReportedDecompressedSizeCodec(const int64 InProducedSizeDelta)
			: ProducedSizeDelta(InProducedSizeDelta)
		{
		}

		virtual bool TryCompressCanonicalZlib(
			const TConstArrayView<uint8>, TArray<uint8>& OutStoredBytes) override
		{
			OutStoredBytes.Reset();
			return false;
		}

		virtual bool TryCompressCanonicalZlibInto(
			const TConstArrayView<uint8>, const TArrayView<uint8>,
			uint64& OutProducedBytes) override
		{
			OutProducedBytes = 0;
			return false;
		}

		virtual bool TryDecompressCanonicalZlib(
			const TConstArrayView<uint8>,
			const TArrayView<uint8> RawOutput,
			uint64& OutProducedBytes) override
		{
			if (!RawOutput.IsEmpty())
			{
				FMemory::Memset(RawOutput.GetData(), static_cast<uint8>('A'), RawOutput.Num());
			}
			const int64 ReportedSize = static_cast<int64>(RawOutput.Num()) + ProducedSizeDelta;
			check(ReportedSize >= 0);
			OutProducedBytes = static_cast<uint64>(ReportedSize);
			return true;
		}

	private:
		int64 ProducedSizeDelta = 0;
	};

	static TArray<uint8> FrozenEmptyPayloadPackBytes()
	{
		return BytesFromHex(TEXT(
			"5545415343563250010000002000000060000000010000008000000000000000"
			"01000000000000003dc136fb31bf03a8d55c17197191abbc50c708e405ebedf"
			"10354926365771b54800000000000000000000000000000000000000000000000"
			"af1349b9f5f9a1a6a0404dea36dcc9499bcb25c9adc112b7cc9a93cae41f3262"));
	}

	static TArray<uint8> FrozenMinimumManifestBytes()
	{
		return BytesFromHex(TEXT(
			"554541534356324d0100000000000000c33a5dbefd943e446a91cb9e4f923a2d"
			"75ecf615c3f3f8c88ad89d9f8aaa33af38aadabf648ee783d14a3bc32d76b786"
			"f2647ea2adccb62b3581ff9463768e6a85c25f64159a3ff0d393e11c8bf2f425"
			"79494faa0e8c8a7ab3250e0c5ce1f26b2122232425262728292a2b2c2d2e2f30"
			"3132333435363738393a3b3c3d3e3f40013dc136fb31bf03a8d55c17197191ab"
			"bc50c708e405ebedf10354926365771b540000000001000000013dc136fb31bf"
			"03a8d55c17197191abbc50c708e405ebedf10354926365771b54cab3a361e103"
			"4a5790e3f2d7cfa6b50803f4c7e026a83b95a657e777f48d4e8e800000000000"
			"00000000000000000000000000000000000000af1349b9f5f9a1a6a0404dea36"
			"dcc9499bcb25c9adc112b7cc9a93cae41f3262"));
	}

	static TArray<uint8> FrozenMixedPackBytes()
	{
		return BytesFromHex(TEXT(
			"554541534356325001000000200000006000000002000000e000000000000000"
			"01000000000000003dc136fb31bf03a8d55c17197191abbc50c708e405ebedf"
			"10354926365771b54e00000000000000000000000000000000000000000000000"
			"af1349b9f5f9a1a6a0404dea36dcc9499bcb25c9adc112b7cc9a93cae41f3262"
			"05010000000000001fdf4b6b71329edf74ab7b78039c44e418033b324ddfddcb"
			"c3e47142a3e63280e0000000000000000c000000000000004000000000000000"
			"e028424e46205e56b2ed1ce1bf7087054072e6c4e41f843bed1e749db635792c"
			"78da7374a40c0000107e1041"));
	}

	static TArray<uint8> FrozenZlibFunctionPackBytes()
	{
		return BytesFromHex(TEXT(
			"5545415343563250010000002000000060000000010000008000000000000000"
			"05010000000000001fdf4b6b71329edf74ab7b78039c44e418033b324ddfddcb"
			"c3e47142a3e6328080000000000000000c000000000000004000000000000000"
			"e028424e46205e56b2ed1ce1bf7087054072e6c4e41f843bed1e749db635792c"
			"78da7374a40c0000107e1041"));
	}

	static TArray<uint8> FrozenMultiModuleManifestBytes()
	{
		return BytesFromHex(TEXT(
			"554541534356324d0100000000000000c33a5dbefd943e446a91cb9e4f923a2d"
			"75ecf615c3f3f8c88ad89d9f8aaa33af38aadabf648ee783d14a3bc32d76b786"
			"f2647ea2adccb62b3581ff9463768e6a85c25f64159a3ff0d393e11c8bf2f425"
			"79494faa0e8c8a7ab3250e0c5ce1f26b2122232425262728292a2b2c2d2e2f30"
			"3132333435363738393a3b3c3d3e3f40013dc136fb31bf03a8d55c17197191ab"
			"bc50c708e405ebedf10354926365771b5402000000101112131415161718191a"
			"1b1c1d1e1f202122232425262728292a2b2c2d2e2f075152535455565758595a"
			"5b5c5d5e5f606162636465666768696a6b6c6d6e6f7030313233343536373839"
			"3a3b3c3d3e3f404142434445464748494a4b4c4d4e4f07717273747576777879"
			"7a7b7c7d7e7f808182838485868788898a8b8c8d8e8f9004000000013dc136fb"
			"31bf03a8d55c17197191abbc50c708e405ebedf10354926365771b54a1a2a3a4"
			"a5a6a7a8a9aaabacadaeafb0b1b2b3b4b5b6b7b8b9babbbcbdbebfc040010000"
			"000000000000000000000000000000000000000000af1349b9f5f9a1a6a0404d"
			"ea36dcc9499bcb25c9adc112b7cc9a93cae41f3262051fdf4b6b71329edf74ab"
			"7b78039c44e418033b324ddfddcbc3e47142a3e63280a1a2a3a4a5a6a7a8a9aa"
			"abacadaeafb0b1b2b3b4b5b6b7b8b9babbbcbdbebfc040010000000000000c00"
			"000000000000400000000000000001e028424e46205e56b2ed1ce1bf70870540"
			"72e6c4e41f843bed1e749db635792c075152535455565758595a5b5c5d5e5f60"
			"6162636465666768696a6b6c6d6e6f70c1c2c3c4c5c6c7c8c9cacbcccdcecfd0"
			"d1d2d3d4d5d6d7d8d9dadbdcdddedfe0e0000000000000000700000000000000"
			"07000000000000000002d10c739239061dddf1fdf8c93d2c4611b7c0f032fff7"
			"0e2c3f77f8e017f5bb077172737475767778797a7b7c7d7e7f80818283848586"
			"8788898a8b8c8d8e8f90c1c2c3c4c5c6c7c8c9cacbcccdcecfd0d1d2d3d4d5d6"
			"d7d8d9dadbdcdddedfe0e7000000000000000700000000000000070000000000"
			"000000469e0a6862d04edb5fe7bcf8362e933687c226e54b03ae86f44ce241c1"
			"8a22f3"));
	}

	static FAngelscriptCacheGenerationManifest MakeMinimumManifestValue()
	{
		FAngelscriptCacheGenerationManifest Value;
		Value.ManifestSchemaVersion = 1;
		Value.ManifestFlags = 0;
		Value.Compatibility.Hash = HashFromHex(
			TEXT("c33a5dbefd943e446a91cb9e4f923a2d75ecf615c3f3f8c88ad89d9f8aaa33af"));
		Value.Context.Hash = HashFromHex(
			TEXT("38aadabf648ee783d14a3bc32d76b786f2647ea2adccb62b3581ff9463768e6a"));
		Value.Profile.Hash = HashFromHex(
			TEXT("85c25f64159a3ff0d393e11c8bf2f42579494faa0e8c8a7ab3250e0c5ce1f26b"));
		Value.SourceSnapshot = HashFromHex(
			TEXT("2122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f40"));
		Value.SourceIndexRecordId = RecordIdFromHex(
			TEXT("013dc136fb31bf03a8d55c17197191abbc50c708e405ebedf10354926365771b54"));

		FAngelscriptCacheRecordIndexEntry SourceEntry;
		SourceEntry.RecordId = Value.SourceIndexRecordId;
		SourceEntry.Location.PackId = HashFromHex(
			TEXT("cab3a361e1034a5790e3f2d7cfa6b50803f4c7e026a83b95a657e777f48d4e8e"));
		SourceEntry.Location.PackOffset = 128;
		SourceEntry.Location.StoredSize = 0;
		SourceEntry.Location.RawSize = 0;
		SourceEntry.Location.Codec = EAngelscriptCacheCodec::None;
		SourceEntry.Location.RawChecksum = HashFromHex(
			TEXT("af1349b9f5f9a1a6a0404dea36dcc9499bcb25c9adc112b7cc9a93cae41f3262"));
		Value.Records.Add(MoveTemp(SourceEntry));
		return Value;
	}

	static FAngelscriptCachePackLocation MakeLocation(
		const FStringView PackId,
		const uint64 Offset,
		const uint64 StoredSize,
		const uint64 RawSize,
		const EAngelscriptCacheCodec Codec,
		const FStringView RawChecksum)
	{
		FAngelscriptCachePackLocation Location;
		Location.PackId = HashFromHex(PackId);
		Location.PackOffset = Offset;
		Location.StoredSize = StoredSize;
		Location.RawSize = RawSize;
		Location.Codec = Codec;
		Location.RawChecksum = HashFromHex(RawChecksum);
		return Location;
	}

	static FAngelscriptCacheRecordIndexEntry MakeZlibFunctionManifestEntry()
	{
		return FAngelscriptCacheRecordIndexEntry{
			RecordIdFromHex(TEXT(
				"051fdf4b6b71329edf74ab7b78039c44e418033b324ddfddcbc3e47142a3e63280")),
			MakeLocation(
				TEXT("fe843139ae082b72468153bc5bd5837c4a891978c9e4a234a9f94518983f121b"),
				128, 12, 64, EAngelscriptCacheCodec::Zlib,
				TEXT("e028424e46205e56b2ed1ce1bf7087054072e6c4e41f843bed1e749db635792c"))};
	}

	static FAngelscriptPreparedRecord MakeMinimalSourceRecord(
		const uint32 PolicyVersion,
		FAngelscriptHash256& OutSourceSnapshot)
	{
		FAngelscriptCachedSourceIndex Source;
		Source.PayloadSchemaVersion =
			FAngelscriptCacheSemanticArchive::SourceIndexPayloadSchemaVersion;
		Source.DiscoveryPolicy.PolicyVersion = PolicyVersion;
		check(FAngelscriptCacheSemanticArchive::ComputeSourceSnapshot(
			Source, Source.SourceSnapshot).IsSuccess());
		FAngelscriptPreparedRecord Record;
		check(FAngelscriptCacheSemanticArchive::SerializeSourceIndex(
			Source, Record.CanonicalPayload).IsSuccess());
		check(FAngelscriptCacheRecordArchive::TryBuildRecordId(
			EAngelscriptCacheRecordKind::SourceIndex,
			Record.CanonicalPayload, Record.RecordId).IsSuccess());
		OutSourceSnapshot = Source.SourceSnapshot;
		return Record;
	}

	static FAngelscriptCachePackLocation LocationFromPack(
		const FAngelscriptEncodedPack& Pack,
		const FAngelscriptCachePackIndexEntry& IndexEntry)
	{
		FAngelscriptCachePackLocation Location;
		Location.PackId = Pack.PackId;
		Location.PackOffset = IndexEntry.PackOffset;
		Location.StoredSize = IndexEntry.StoredSize;
		Location.RawSize = IndexEntry.RawSize;
		Location.Codec = IndexEntry.Codec;
		Location.RawChecksum = IndexEntry.RawChecksum;
		return Location;
	}

	static FAngelscriptCacheValidationResult ReadZlibFunctionPack(
		const TConstArrayView<uint8> Bytes,
		const FAngelscriptHash256& ExpectedPackId,
		const FAngelscriptCacheRecordIndexEntry& ManifestEntry,
		const FAngelscriptCacheReadLimits& Limits,
		FAngelscriptCacheReadBudget& Budget,
		TOptional<FAngelscriptDecodedCacheRecordHandle>& OutRecord)
	{
		FAngelscriptUnrealZlibCacheStorageCodec Codec;
		return ReadAngelscriptCacheRecordFromPack(
			Bytes, ExpectedPackId, ManifestEntry, Limits, Budget, Codec, OutRecord);
	}

	static bool ExpectPackFailure(
		FAutomationTestBase& Test,
		const TConstArrayView<uint8> Bytes,
		const FAngelscriptHash256& ExpectedPackId,
		const FAngelscriptCacheRecordIndexEntry& ManifestEntry,
		const FAngelscriptCacheReadLimits& Limits,
		const EAngelscriptCacheValidationError ExpectedError,
		const uint64 ExpectedOffset,
		const TCHAR* Message)
	{
		FNoDiscardAsserter LocalAssert(Test);
		FAngelscriptCacheReadBudget Budget;
		TOptional<FAngelscriptDecodedCacheRecordHandle> OutRecord;
		const FAngelscriptCacheValidationResult Result = ReadZlibFunctionPack(
			Bytes, ExpectedPackId, ManifestEntry, Limits, Budget, OutRecord);
		bool bPassed = LocalAssert.AreEqual(ExpectedError, Result.Error, Message);
		bPassed &= LocalAssert.AreEqual(
			FAngelscriptCacheValidationResult::Classify(ExpectedError), Result.Class,
			TEXT("Pack failures derive their class from the one Classify authority"));
		bPassed &= LocalAssert.AreEqual(EAngelscriptCacheValidationStage::PackDecode,
			Result.Stage, TEXT("Physical pack failures are always PackDecode"));
		bPassed &= LocalAssert.AreEqual(ExpectedOffset, Result.ByteOffset,
			TEXT("Pack failure offsets are absolute in the complete pack"));
		bPassed &= LocalAssert.IsFalse(OutRecord.IsSet(),
			TEXT("No decoded handle is published after a pack failure"));
		return bPassed;
	}

	static bool ExpectPackIndexFailure(
		FAutomationTestBase& Test,
		const TConstArrayView<uint8> Bytes,
		const FAngelscriptHash256& ExpectedPackId,
		const FAngelscriptCacheReadLimits& Limits,
		const EAngelscriptCacheValidationError ExpectedError,
		const uint64 ExpectedOffset,
		const TCHAR* Message)
	{
		FNoDiscardAsserter LocalAssert(Test);
		FAngelscriptCacheReadBudget Budget;
		TArray<FAngelscriptCachePackIndexEntry> OutIndex = {
			FAngelscriptCachePackIndexEntry{}};
		const FAngelscriptCacheValidationResult Result = ValidateAngelscriptCachePack(
			Bytes, ExpectedPackId, Limits, Budget, OutIndex);
		bool bPassed = LocalAssert.AreEqual(ExpectedError, Result.Error, Message);
		bPassed &= LocalAssert.AreEqual(
			FAngelscriptCacheValidationResult::Classify(ExpectedError), Result.Class,
			TEXT("Pack-index failures derive their class from Classify"));
		bPassed &= LocalAssert.AreEqual(EAngelscriptCacheValidationStage::PackDecode,
			Result.Stage, TEXT("Pack-index failures are PackDecode"));
		bPassed &= LocalAssert.AreEqual(ExpectedOffset, Result.ByteOffset,
			TEXT("Pack-index failure offsets are absolute"));
		bPassed &= LocalAssert.IsTrue(OutIndex.IsEmpty(),
			TEXT("A failed physical pack validation clears its index output"));
		return bPassed;
	}

	static FAngelscriptCacheGenerationManifest MakeMultiModuleManifestValue()
	{
		FAngelscriptCacheGenerationManifest Value = MakeMinimumManifestValue();
		Value.ModuleSnapshots = {
			{FAngelscriptStableModuleKey{HashFromHex(
				TEXT("101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f"))},
				RecordIdFromHex(TEXT(
					"075152535455565758595a5b5c5d5e5f606162636465666768696a6b6c6d6e6f70"))},
			{FAngelscriptStableModuleKey{HashFromHex(
				TEXT("303132333435363738393a3b3c3d3e3f404142434445464748494a4b4c4d4e4f"))},
				RecordIdFromHex(TEXT(
					"077172737475767778797a7b7c7d7e7f808182838485868788898a8b8c8d8e8f90"))},
		};
		Value.Records.Reset();
		const FString PackOne =
			TEXT("a1a2a3a4a5a6a7a8a9aaabacadaeafb0b1b2b3b4b5b6b7b8b9babbbcbdbebfc0");
		const FString PackTwo =
			TEXT("c1c2c3c4c5c6c7c8c9cacbcccdcecfd0d1d2d3d4d5d6d7d8d9dadbdcdddedfe0");
		Value.Records.Add({Value.SourceIndexRecordId,
			MakeLocation(PackOne, 320, 0, 0, EAngelscriptCacheCodec::None,
				TEXT("af1349b9f5f9a1a6a0404dea36dcc9499bcb25c9adc112b7cc9a93cae41f3262"))});
		Value.Records.Add({RecordIdFromHex(TEXT(
			"051fdf4b6b71329edf74ab7b78039c44e418033b324ddfddcbc3e47142a3e63280")),
			MakeLocation(PackOne, 320, 12, 64, EAngelscriptCacheCodec::Zlib,
				TEXT("e028424e46205e56b2ed1ce1bf7087054072e6c4e41f843bed1e749db635792c"))});
		Value.Records.Add({Value.ModuleSnapshots[0].RecordId,
			MakeLocation(PackTwo, 224, 7, 7, EAngelscriptCacheCodec::None,
				TEXT("02d10c739239061dddf1fdf8c93d2c4611b7c0f032fff70e2c3f77f8e017f5bb"))});
		Value.Records.Add({Value.ModuleSnapshots[1].RecordId,
			MakeLocation(PackTwo, 231, 7, 7, EAngelscriptCacheCodec::None,
				TEXT("469e0a6862d04edb5fe7bcf8362e933687c226e54b03ae86f44ce241c18a22f3"))});
		return Value;
	}

	static FAngelscriptCacheGenerationManifest MakePhysicalManifestFromPacks(
		const FAngelscriptCacheRecordId& SourceIndexRecordId,
		TConstArrayView<FAngelscriptEncodedPack> Packs)
	{
		FAngelscriptCacheGenerationManifest Value = MakeMinimumManifestValue();
		Value.SourceIndexRecordId = SourceIndexRecordId;
		Value.Records.Reset();
		for (const FAngelscriptEncodedPack& Pack : Packs)
		{
			for (const FAngelscriptCachePackIndexEntry& Entry : Pack.Index)
			{
				Value.Records.Add({Entry.RecordId, LocationFromPack(Pack, Entry)});
			}
		}
		Value.Records.Sort([](
			const FAngelscriptCacheRecordIndexEntry& A,
			const FAngelscriptCacheRecordIndexEntry& B)
		{
			return A.RecordId < B.RecordId;
		});
		return Value;
	}

	static TArray<uint8> MakeManifestWithDefaultMaxGenerationPacksPlusOne()
	{
		const FAngelscriptCacheGenerationManifest Minimum = MakeMinimumManifestValue();
		TArray<uint8> Bytes;
		Bytes.Append(reinterpret_cast<const uint8*>("UEASCV2M"), 8);
		AppendUInt32LittleEndian(Bytes, 1);
		AppendUInt32LittleEndian(Bytes, 0);
		AppendHash(Bytes, Minimum.Compatibility.Hash);
		AppendHash(Bytes, Minimum.Context.Hash);
		AppendHash(Bytes, Minimum.Profile.Hash);
		AppendHash(Bytes, Minimum.SourceSnapshot);
		AppendRecordId(Bytes, Minimum.SourceIndexRecordId);
		AppendUInt32LittleEndian(Bytes, 0);
		AppendUInt32LittleEndian(Bytes, 4097);

		auto AppendLocation = [&](const FAngelscriptCacheRecordId& RecordId,
			const FAngelscriptHash256& PackId)
		{
			AppendRecordId(Bytes, RecordId);
			AppendHash(Bytes, PackId);
			AppendUInt64LittleEndian(Bytes, 128);
			AppendUInt64LittleEndian(Bytes, 0);
			AppendUInt64LittleEndian(Bytes, 0);
			Bytes.Add(static_cast<uint8>(EAngelscriptCacheCodec::None));
			AppendHash(Bytes, HashFromHex(TEXT(
				"af1349b9f5f9a1a6a0404dea36dcc9499bcb25c9adc112b7cc9a93cae41f3262")));
		};
		AppendLocation(Minimum.SourceIndexRecordId,
			Minimum.Records[0].Location.PackId);
		for (uint32 Ordinal = 1; Ordinal <= 4096; ++Ordinal)
		{
			AppendLocation(
				FAngelscriptCacheRecordId{
					EAngelscriptCacheRecordKind::FunctionBody,
					OrdinalHash(Ordinal, 0x10)},
				OrdinalHash(Ordinal, 0x20));
		}
		return Bytes;
	}

	class FCountingPackSource final : public IAngelscriptCachePackSource
	{
	public:
		void Add(const FAngelscriptHash256& PackId, TArray<uint8> Bytes)
		{
			FStoredPack Pack;
			Pack.PackId = PackId;
			Pack.Bytes = MoveTemp(Bytes);
			Packs.Add(MoveTemp(Pack));
		}

		virtual bool TryGetCompletePack(
			const FAngelscriptHash256& PackId,
			TConstArrayView<uint8>& OutBytes) override
		{
			++LookupOrOpenCount;
			RequestedPackIds.Add(PackId);
			for (const FStoredPack& Pack : Packs)
			{
				if (Pack.PackId == PackId)
				{
					OutBytes = Pack.Bytes;
					return true;
				}
			}
			OutBytes = {};
			return false;
		}

		int32 GetLookupOrOpenCount() const { return LookupOrOpenCount; }
		TConstArrayView<FAngelscriptHash256> GetRequestedPackIds() const
		{
			return RequestedPackIds;
		}

	private:
		struct FStoredPack
		{
			FAngelscriptHash256 PackId;
			TArray<uint8> Bytes;
		};

		TArray<FStoredPack> Packs;
		TArray<FAngelscriptHash256> RequestedPackIds;
		int32 LookupOrOpenCount = 0;
	};

	static FAngelscriptCacheValidationResult ValidateGeneration(
		const TConstArrayView<uint8> ManifestBytes,
		const FAngelscriptHash256& ExpectedGenerationId,
		FCountingPackSource& Packs,
		const FAngelscriptCacheReadLimits& Limits,
		FAngelscriptCacheReadBudget& Budget,
		TOptional<FAngelscriptValidatedGeneration>& OutGeneration)
	{
		FAngelscriptUnrealZlibCacheStorageCodec Codec;
		return ValidateAngelscriptCacheGeneration(
			ManifestBytes, ExpectedGenerationId, Packs, Limits, Budget, Codec, OutGeneration);
	}

	static bool ExpectManifestFailure(
		FAutomationTestBase& Test,
		const TConstArrayView<uint8> Bytes,
		const FAngelscriptHash256& ExpectedGenerationId,
		const FAngelscriptCacheReadLimits& Limits,
		const EAngelscriptCacheValidationError ExpectedError,
		const EAngelscriptCacheValidationStage ExpectedStage,
		const uint64 ExpectedOffset,
		const TCHAR* Message)
	{
		FNoDiscardAsserter LocalAssert(Test);
		FCountingPackSource Packs;
		FAngelscriptCacheReadBudget Budget;
		TOptional<FAngelscriptValidatedGeneration> OutGeneration;
		const FAngelscriptCacheValidationResult Result = ValidateGeneration(
			Bytes, ExpectedGenerationId, Packs, Limits, Budget, OutGeneration);
		bool bPassed = LocalAssert.AreEqual(ExpectedError, Result.Error, Message);
		bPassed &= LocalAssert.AreEqual(
			FAngelscriptCacheValidationResult::Classify(ExpectedError), Result.Class,
			TEXT("Manifest failures derive class from the sole Classify authority"));
		bPassed &= LocalAssert.AreEqual(ExpectedStage, Result.Stage,
			TEXT("Manifest diagnostics preserve local-versus-graph stage"));
		bPassed &= LocalAssert.AreEqual(ExpectedOffset, Result.ByteOffset,
			TEXT("Manifest error offsets are absolute"));
		bPassed &= LocalAssert.IsFalse(OutGeneration.IsSet(),
			TEXT("A failed generation never publishes a partial validated value"));
		bPassed &= LocalAssert.AreEqual(0, Packs.GetLookupOrOpenCount(),
			TEXT("Manifest-local failure must precede pack lookup/open"));
		return bPassed;
	}

	struct FZeroModuleGenerationFixture
	{
		FAngelscriptEncodedPack Pack;
		FAngelscriptEncodedCacheGenerationManifest Manifest;
	};

	static FZeroModuleGenerationFixture MakeZeroModuleGenerationFixture()
	{
		FAngelscriptHash256 SourceSnapshot;
		const FAngelscriptPreparedRecord Source = MakeMinimalSourceRecord(1, SourceSnapshot);
		FAngelscriptCachePackPolicy Policy;
		Policy.CompressionPolicy = EAngelscriptCachePackCompressionPolicy::ForceNoneForTest;
		FAngelscriptUnrealZlibCacheStorageCodec Codec;
		TArray<FAngelscriptEncodedPack> Packs;
		check(BuildAngelscriptCachePacks(
			MakeArrayView(&Source, 1), Policy, Codec, Packs).IsSuccess());
		check(Packs.Num() == 1 && Packs[0].Index.Num() == 1);

		FAngelscriptCacheGenerationManifest Value = MakeMinimumManifestValue();
		Value.SourceSnapshot = SourceSnapshot;
		Value.SourceIndexRecordId = Source.RecordId;
		Value.Records = {{Source.RecordId,
			LocationFromPack(Packs[0], Packs[0].Index[0])}};
		FZeroModuleGenerationFixture Fixture;
		Fixture.Pack = MoveTemp(Packs[0]);
		check(EncodeAngelscriptCacheGenerationManifest(
			Value, Fixture.Manifest).IsSuccess());
		return Fixture;
	}

	struct FReachabilityFixture
	{
		static constexpr uint64 SourceRootManifestByteOffset = 144;
		FAngelscriptHash256 ManifestSourceSnapshot = OrdinalHash(1, 0x01);
		FAngelscriptStableModuleKey FirstModuleKey{OrdinalHash(1, 0x10)};
		FAngelscriptStableModuleKey SecondModuleKey{OrdinalHash(1, 0x11)};
		FAngelscriptCacheRecordId Source{
			EAngelscriptCacheRecordKind::SourceIndex, OrdinalHash(1, 0x10)};
		FAngelscriptCacheRecordId FirstInterface{
			EAngelscriptCacheRecordKind::ModuleInterface, OrdinalHash(1, 0x20)};
		FAngelscriptCacheRecordId SecondInterface{
			EAngelscriptCacheRecordKind::ModuleInterface, OrdinalHash(1, 0x21)};
		FAngelscriptCacheRecordId TypeSchema{
			EAngelscriptCacheRecordKind::TypeSchema, OrdinalHash(1, 0x30)};
		FAngelscriptCacheRecordId FirstState{
			EAngelscriptCacheRecordKind::ModuleState, OrdinalHash(1, 0x40)};
		FAngelscriptCacheRecordId SecondState{
			EAngelscriptCacheRecordKind::ModuleState, OrdinalHash(1, 0x41)};
		FAngelscriptCacheRecordId Function{
			EAngelscriptCacheRecordKind::FunctionBody, OrdinalHash(1, 0x50)};
		FAngelscriptCacheRecordId Debug{
			EAngelscriptCacheRecordKind::DebugSidecar, OrdinalHash(1, 0x60)};
		FAngelscriptCacheRecordId FirstSnapshot{
			EAngelscriptCacheRecordKind::ModuleSnapshot, OrdinalHash(1, 0x70)};
		FAngelscriptCacheRecordId SecondSnapshot{
			EAngelscriptCacheRecordKind::ModuleSnapshot, OrdinalHash(1, 0x71)};
		TArray<FAngelscriptCacheReachabilityRootForTests> Roots;
		TArray<FAngelscriptCacheReachabilityManifestEntryForTests> Index;
		TArray<FAngelscriptCacheReachabilityNodeForTests> Nodes;

		FReachabilityFixture()
		{
			Roots.SetNum(2);
			Roots[0].Link = {FirstModuleKey, FirstSnapshot};
			Roots[0].ManifestByteOffset = 181;
			Roots[1].Link = {SecondModuleKey, SecondSnapshot};
			Roots[1].ManifestByteOffset = 246;
			Index = {
				{Source, 185},
				{FirstInterface, 307},
				{SecondInterface, 429},
				{TypeSchema, 551},
				{FirstState, 673},
				{SecondState, 795},
				{Function, 917},
				{Debug, 1039},
				{FirstSnapshot, 1161},
				{SecondSnapshot, 1283},
			};
			Nodes.SetNum(Index.Num());
			for (int32 IndexPosition = 0; IndexPosition < Index.Num(); ++IndexPosition)
			{
				Nodes[IndexPosition].RecordId = Index[IndexPosition].RecordId;
			}
			Nodes[0].EmbeddedSourceSnapshot = ManifestSourceSnapshot;
			Nodes[6].DebugSidecar = Debug;
			Nodes[8].EmbeddedModuleKey = FirstModuleKey;
			Nodes[8].ModuleInterface = FirstInterface;
			Nodes[8].TypeSchemas = {TypeSchema};
			Nodes[8].ModuleState = FirstState;
			Nodes[8].FunctionBodies = {Function};
			Nodes[9].EmbeddedModuleKey = SecondModuleKey;
			Nodes[9].ModuleInterface = SecondInterface;
			Nodes[9].ModuleState = SecondState;
		}

		int32 FindIndexPosition(const FAngelscriptCacheRecordId& RecordId) const
		{
			return Index.IndexOfByPredicate([&](
				const FAngelscriptCacheReachabilityManifestEntryForTests& Entry)
			{
				return Entry.RecordId == RecordId;
			});
		}

		int32 FindNodePosition(const FAngelscriptCacheRecordId& RecordId) const
		{
			return Nodes.IndexOfByPredicate([&](
				const FAngelscriptCacheReachabilityNodeForTests& Node)
			{
				return Node.RecordId == RecordId;
			});
		}
	};

	static FAngelscriptCacheValidationResult ValidateReachability(
		const FReachabilityFixture& Fixture,
		const FAngelscriptCacheReadLimits& Limits,
		FAngelscriptCacheReadBudget& Budget,
		TArray<FAngelscriptCacheRecordId>& OutVisited,
		FAngelscriptCacheModuleGraphValidationProbeForTests* Probe = nullptr)
	{
		return ValidateAngelscriptCacheGenerationReachabilityForTests(
			Fixture.ManifestSourceSnapshot, Fixture.Source,
			FReachabilityFixture::SourceRootManifestByteOffset, Fixture.Roots,
			Fixture.Index, Fixture.Nodes, Limits, Budget, OutVisited, Probe);
	}

	static bool ExpectEncodedPackSetsEqual(
		FAutomationTestBase& Test,
		const TConstArrayView<FAngelscriptEncodedPack> Expected,
		const TConstArrayView<FAngelscriptEncodedPack> Actual,
		const TCHAR* Message)
	{
		FNoDiscardAsserter LocalAssert(Test);
		bool bPassed = LocalAssert.AreEqual(Expected.Num(), Actual.Num(), Message);
		if (Expected.Num() != Actual.Num())
		{
			return false;
		}
		for (int32 PackIndex = 0; PackIndex < Expected.Num(); ++PackIndex)
		{
			bPassed &= LocalAssert.IsTrue(
				Expected[PackIndex].PackId == Actual[PackIndex].PackId, Message);
			bPassed &= LocalAssert.AreEqual(
				Hex(Expected[PackIndex].Bytes), Hex(Actual[PackIndex].Bytes), Message);
			bPassed &= LocalAssert.AreEqual(
				Expected[PackIndex].Index.Num(), Actual[PackIndex].Index.Num(), Message);
			if (Expected[PackIndex].Index.Num() != Actual[PackIndex].Index.Num())
			{
				continue;
			}
			for (int32 EntryIndex = 0;
				EntryIndex < Expected[PackIndex].Index.Num(); ++EntryIndex)
			{
				const FAngelscriptCachePackIndexEntry& ExpectedEntry =
					Expected[PackIndex].Index[EntryIndex];
				const FAngelscriptCachePackIndexEntry& ActualEntry =
					Actual[PackIndex].Index[EntryIndex];
				bPassed &= LocalAssert.IsTrue(
					ExpectedEntry.RecordId == ActualEntry.RecordId, Message);
				bPassed &= LocalAssert.AreEqual(
					static_cast<uint8>(ExpectedEntry.Codec),
					static_cast<uint8>(ActualEntry.Codec), Message);
				bPassed &= LocalAssert.AreEqual(
					ExpectedEntry.PackOffset, ActualEntry.PackOffset, Message);
				bPassed &= LocalAssert.AreEqual(
					ExpectedEntry.StoredSize, ActualEntry.StoredSize, Message);
				bPassed &= LocalAssert.AreEqual(
					ExpectedEntry.RawSize, ActualEntry.RawSize, Message);
				bPassed &= LocalAssert.IsTrue(
					ExpectedEntry.RawChecksum == ActualEntry.RawChecksum, Message);
			}
		}
		return bPassed;
	}

public:
	TEST_METHOD(PureInterfaceShapeOwnsCompleteBytesIdsBudgetAndSoleDecodedFactoryBoundary)
	{
		using FBuildPacks = FAngelscriptCacheValidationResult(*)(
			TConstArrayView<FAngelscriptPreparedRecord>, const FAngelscriptCachePackPolicy&,
			IAngelscriptCacheStorageCodec&, TArray<FAngelscriptEncodedPack>&);
		using FReadRecord = FAngelscriptCacheValidationResult(*)(
			TConstArrayView<uint8>, const FAngelscriptHash256&,
			const FAngelscriptCacheRecordIndexEntry&, const FAngelscriptCacheReadLimits&,
			FAngelscriptCacheReadBudget&, IAngelscriptCacheStorageCodec&,
			TOptional<FAngelscriptDecodedCacheRecordHandle>&);
		using FValidatePack = FAngelscriptCacheValidationResult(*)(
			TConstArrayView<uint8>, const FAngelscriptHash256&,
			const FAngelscriptCacheReadLimits&, FAngelscriptCacheReadBudget&,
			TArray<FAngelscriptCachePackIndexEntry>&);
		using FEncodeManifest = FAngelscriptCacheValidationResult(*)(
			const FAngelscriptCacheGenerationManifest&,
			FAngelscriptEncodedCacheGenerationManifest&);
		using FValidateGeneration = FAngelscriptCacheValidationResult(*)(
			TConstArrayView<uint8>, const FAngelscriptHash256&,
			IAngelscriptCachePackSource&, const FAngelscriptCacheReadLimits&,
			FAngelscriptCacheReadBudget&, IAngelscriptCacheStorageCodec&,
			TOptional<FAngelscriptValidatedGeneration>&);
		using FValidateReachabilityForTests = FAngelscriptCacheValidationResult(*)(
			const FAngelscriptHash256&,
			const FAngelscriptCacheRecordId&,
			uint64,
			TConstArrayView<FAngelscriptCacheReachabilityRootForTests>,
			TConstArrayView<FAngelscriptCacheReachabilityManifestEntryForTests>,
			TConstArrayView<FAngelscriptCacheReachabilityNodeForTests>,
			const FAngelscriptCacheReadLimits&, FAngelscriptCacheReadBudget&,
			TArray<FAngelscriptCacheRecordId>&,
			FAngelscriptCacheModuleGraphValidationProbeForTests*);
		using FPackLookup = bool (IAngelscriptCachePackSource::*)(
			const FAngelscriptHash256&, TConstArrayView<uint8>&);
		using FCompressCanonicalZlib = bool (IAngelscriptCacheStorageCodec::*)(
			TConstArrayView<uint8>, TArray<uint8>&);
		using FCompressCanonicalZlibInto = bool (IAngelscriptCacheStorageCodec::*)(
			TConstArrayView<uint8>, TArrayView<uint8>, uint64&);
		using FDecompressCanonicalZlib = bool (IAngelscriptCacheStorageCodec::*)(
			TConstArrayView<uint8>, TArrayView<uint8>, uint64&);
		using FAggregatePreparedCompletions = FAngelscriptCacheValidationResult(*)(
			TConstArrayView<FAngelscriptPreparedRecordCompletion>,
			EAngelscriptCachePreparationExecutionMode,
			const FAngelscriptCachePackPolicy&, IAngelscriptCacheStorageCodec&,
			TArray<FAngelscriptEncodedPack>&);

		static_assert(std::is_same_v<decltype(&BuildAngelscriptCachePacks), FBuildPacks>);
		static_assert(std::is_same_v<decltype(&ValidateAngelscriptCachePack), FValidatePack>);
		static_assert(std::is_same_v<decltype(&ReadAngelscriptCacheRecordFromPack), FReadRecord>);
		static_assert(std::is_same_v<decltype(&EncodeAngelscriptCacheGenerationManifest),
			FEncodeManifest>);
		static_assert(std::is_same_v<decltype(&ValidateAngelscriptCacheGeneration),
			FValidateGeneration>);
		static_assert(std::is_same_v<decltype(
			&ValidateAngelscriptCacheGenerationReachabilityForTests),
			FValidateReachabilityForTests>);
		static_assert(std::is_same_v<decltype(
			&IAngelscriptCachePackSource::TryGetCompletePack), FPackLookup>);
		static_assert(std::is_same_v<decltype(
			&IAngelscriptCacheStorageCodec::TryCompressCanonicalZlib),
			FCompressCanonicalZlib>);
		static_assert(std::is_same_v<decltype(
			&IAngelscriptCacheStorageCodec::TryCompressCanonicalZlibInto),
			FCompressCanonicalZlibInto>);
		static_assert(std::is_same_v<decltype(
			&IAngelscriptCacheStorageCodec::TryDecompressCanonicalZlib),
			FDecompressCanonicalZlib>);
		static_assert(std::is_same_v<decltype(
			&AggregateAngelscriptCachePreparedRecordCompletions),
			FAggregatePreparedCompletions>);
		static_assert(std::is_abstract_v<IAngelscriptCacheStorageCodec>);
		static_assert(std::is_abstract_v<IAngelscriptCachePackSource>);

		ASSERT_THAT(IsFalse(std::is_copy_constructible_v<FAngelscriptCacheReadBudget>));
		ASSERT_THAT(IsFalse(std::is_move_constructible_v<FAngelscriptCacheReadBudget>));
	}

	TEST_METHOD(WireConstantsStagesErrorsClassificationAndCompleteLimitsAreFrozen)
	{
		ASSERT_THAT(AreEqual(1u, FAngelscriptCacheManifestPackArchive::PackSchemaVersion));
		ASSERT_THAT(AreEqual(1u, FAngelscriptCacheManifestPackArchive::ManifestSchemaVersion));
		ASSERT_THAT(AreEqual(33u, FAngelscriptCacheManifestPackArchive::RecordIdWireSize));
		ASSERT_THAT(AreEqual(65u, FAngelscriptCacheManifestPackArchive::ManifestRootWireSize));
		ASSERT_THAT(AreEqual(122u, FAngelscriptCacheManifestPackArchive::ManifestLocationWireSize));
		ASSERT_THAT(AreEqual(32u, FAngelscriptCacheManifestPackArchive::PackHeaderWireSize));
		ASSERT_THAT(AreEqual(96u, FAngelscriptCacheManifestPackArchive::PackIndexEntryWireSize));
		ASSERT_THAT(AreEqual(64ull * 1024 * 1024,
			FAngelscriptCachePackPolicy::DefaultTargetRawBytesPerPack));

		ASSERT_THAT(AreEqual(0, static_cast<int32>(EAngelscriptCacheCodec::None)));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(EAngelscriptCacheCodec::Zlib)));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(EAngelscriptCachePackCompressionPolicy::Auto)));
		ASSERT_THAT(AreEqual(2, static_cast<int32>(
			EAngelscriptCachePackCompressionPolicy::ForceNoneForTest)));
		ASSERT_THAT(AreEqual(3, static_cast<int32>(
			EAngelscriptCachePackCompressionPolicy::ForceZlibForTest)));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(
			EAngelscriptCachePreparationExecutionMode::ForcedSerial)));
		ASSERT_THAT(AreEqual(2, static_cast<int32>(
			EAngelscriptCachePreparationExecutionMode::BoundedParallel)));

		ASSERT_THAT(AreEqual(7, static_cast<int32>(EAngelscriptCacheValidationStage::PackDecode)));
		ASSERT_THAT(AreEqual(8, static_cast<int32>(EAngelscriptCacheValidationStage::ManifestDecode)));
		ASSERT_THAT(AreEqual(9, static_cast<int32>(EAngelscriptCacheValidationStage::ManifestGraph)));
		ASSERT_THAT(AreEqual(65, static_cast<int32>(
			EAngelscriptCacheValidationError::UnsupportedStorageCodec)));
		ASSERT_THAT(AreEqual(66, static_cast<int32>(
			EAngelscriptCacheValidationError::DecompressionFailed)));
		ASSERT_THAT(AreEqual(67, static_cast<int32>(
			EAngelscriptCacheValidationError::DecompressedSizeMismatch)));
		ASSERT_THAT(AreEqual(68, static_cast<int32>(
			EAngelscriptCacheValidationError::PackIdMismatch)));
		ASSERT_THAT(AreEqual(69, static_cast<int32>(
			EAngelscriptCacheValidationError::GenerationIdMismatch)));
		ASSERT_THAT(AreEqual(70, static_cast<int32>(
			EAngelscriptCacheValidationError::OverlappingRange)));
		ASSERT_THAT(AreEqual(71, static_cast<int32>(
			EAngelscriptCacheValidationError::PackIndexMismatch)));

		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationClass::CodecOrIntegrity,
			FAngelscriptCacheValidationResult::Classify(
				EAngelscriptCacheValidationError::UnsupportedStorageCodec)));
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationClass::CodecOrIntegrity,
			FAngelscriptCacheValidationResult::Classify(
				EAngelscriptCacheValidationError::DecompressionFailed)));
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationClass::CodecOrIntegrity,
			FAngelscriptCacheValidationResult::Classify(
				EAngelscriptCacheValidationError::DecompressedSizeMismatch)));
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationClass::CodecOrIntegrity,
			FAngelscriptCacheValidationResult::Classify(
				EAngelscriptCacheValidationError::PackIdMismatch)));
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationClass::CodecOrIntegrity,
			FAngelscriptCacheValidationResult::Classify(
				EAngelscriptCacheValidationError::GenerationIdMismatch)));
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationClass::ArithmeticOrBudget,
			FAngelscriptCacheValidationResult::Classify(
				EAngelscriptCacheValidationError::OverlappingRange)));
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationClass::CodecOrIntegrity,
			FAngelscriptCacheValidationResult::Classify(
				EAngelscriptCacheValidationError::PackIndexMismatch)));
		const TPair<EAngelscriptCacheValidationError, EAngelscriptCacheValidationClass>
			AppendedClassifications[] = {
				{EAngelscriptCacheValidationError::UnsupportedStorageCodec,
					EAngelscriptCacheValidationClass::CodecOrIntegrity},
				{EAngelscriptCacheValidationError::DecompressionFailed,
					EAngelscriptCacheValidationClass::CodecOrIntegrity},
				{EAngelscriptCacheValidationError::DecompressedSizeMismatch,
					EAngelscriptCacheValidationClass::CodecOrIntegrity},
				{EAngelscriptCacheValidationError::PackIdMismatch,
					EAngelscriptCacheValidationClass::CodecOrIntegrity},
				{EAngelscriptCacheValidationError::GenerationIdMismatch,
					EAngelscriptCacheValidationClass::CodecOrIntegrity},
				{EAngelscriptCacheValidationError::OverlappingRange,
					EAngelscriptCacheValidationClass::ArithmeticOrBudget},
				{EAngelscriptCacheValidationError::PackIndexMismatch,
					EAngelscriptCacheValidationClass::CodecOrIntegrity},
			};
		static_assert(UE_ARRAY_COUNT(AppendedClassifications) == 7);
		for (const TPair<EAngelscriptCacheValidationError,
			EAngelscriptCacheValidationClass>& Entry : AppendedClassifications)
		{
			ASSERT_THAT(AreEqual(Entry.Value,
				FAngelscriptCacheValidationResult::Classify(Entry.Key)));
		}

		const FAngelscriptCacheReadLimits Limits;
		ASSERT_THAT(AreEqual(UINT64_C(64) * 1024 * 1024, Limits.MaxCanonicalRecordPayloadBytes));
		ASSERT_THAT(AreEqual(UINT64_C(64) * 1024 * 1024, Limits.MaxStoredRecordBytes));
		ASSERT_THAT(AreEqual(UINT64_C(64) * 1024 * 1024, Limits.MaxManifestBytes));
		ASSERT_THAT(AreEqual(UINT64_C(128) * 1024 * 1024, Limits.MaxPackBytes));
		ASSERT_THAT(AreEqual(UINT64_C(262144), Limits.MaxPackIndexEntries));
		ASSERT_THAT(AreEqual(UINT64_C(262144), Limits.MaxGenerationRecords));
		ASSERT_THAT(AreEqual(UINT64_C(262144), Limits.MaxModuleSnapshots));
		ASSERT_THAT(AreEqual(UINT64_C(4096), Limits.MaxGenerationPacks));
		ASSERT_THAT(AreEqual(UINT64_C(1) * 1024 * 1024, Limits.MaxStringBytes));
		ASSERT_THAT(AreEqual(UINT64_C(1) * 1024 * 1024, Limits.MaxArrayElements));
		ASSERT_THAT(AreEqual(UINT64_C(64), Limits.MaxNestingDepth));
		ASSERT_THAT(AreEqual(UINT64_C(1) * 1024 * 1024,
			Limits.MaxReferencesAndRelocations));
		ASSERT_THAT(AreEqual(UINT64_C(512) * 1024 * 1024, Limits.MaxTotalStoredBytes));
		ASSERT_THAT(AreEqual(UINT64_C(512) * 1024 * 1024,
			Limits.MaxTotalDecompressedBytes));
		ASSERT_THAT(AreEqual(UINT64_C(512) * 1024 * 1024, Limits.MaxTotalDecodedBytes));
		ASSERT_THAT(AreEqual(UINT64_C(512) * 1024 * 1024,
			Limits.MaxResidentDecodedBytes));
	}

	TEST_METHOD(MinimumManifestIsExact307ByteGoldenAndGenerationIdIsExternal)
	{
		FAngelscriptEncodedCacheGenerationManifest Encoded;
		const FAngelscriptCacheValidationResult Result =
			EncodeAngelscriptCacheGenerationManifest(MakeMinimumManifestValue(), Encoded);
		ASSERT_THAT(IsTrue(Result.IsSuccess()));
		ASSERT_THAT(AreEqual(307, Encoded.CompleteBytes.Num()));
		ASSERT_THAT(AreEqual(Hex(FrozenMinimumManifestBytes()), Hex(Encoded.CompleteBytes)));
		ASSERT_THAT(AreEqual(
			TEXT("e4536599616fa82b552de4283e858da39c31d245ac33b829f0b3b9d175269fc4"),
			Encoded.ComputedGenerationId.ToHexString()));
		ASSERT_THAT(IsFalse(Hex(Encoded.CompleteBytes).Contains(
			Encoded.ComputedGenerationId.ToHexString()),
			TEXT("GenerationId is whole-final-file metadata and is not self-serialized")));
		ASSERT_THAT(AreEqual(1u, ReadUInt32LittleEndian(
			Encoded.CompleteBytes, ManifestSchemaOffset)));
		ASSERT_THAT(AreEqual(0u, ReadUInt32LittleEndian(
			Encoded.CompleteBytes, ManifestFlagsOffset)));
		ASSERT_THAT(AreEqual(0u, ReadUInt32LittleEndian(
			Encoded.CompleteBytes, ManifestModuleCountOffset)));
		ASSERT_THAT(AreEqual(1u, ReadUInt32LittleEndian(
			Encoded.CompleteBytes, MinimumManifestRecordCountOffset)));
	}

	TEST_METHOD(EmptyNonePackIsExact128ByteGoldenAndPackIdHashesTheWholeFile)
	{
		FAngelscriptPreparedRecord Record;
		Record.RecordId = RecordIdFromHex(
			TEXT("013dc136fb31bf03a8d55c17197191abbc50c708e405ebedf10354926365771b54"));
		FAngelscriptCachePackPolicy Policy;
		Policy.CompressionPolicy = EAngelscriptCachePackCompressionPolicy::ForceNoneForTest;
		FAngelscriptUnrealZlibCacheStorageCodec Codec;
		TArray<FAngelscriptEncodedPack> Packs;
		const FAngelscriptCacheValidationResult Result = BuildAngelscriptCachePacks(
			MakeArrayView(&Record, 1), Policy, Codec, Packs);

		ASSERT_THAT(IsTrue(Result.IsSuccess()));
		ASSERT_THAT(AreEqual(1, Packs.Num()));
		ASSERT_THAT(AreEqual(Hex(FrozenEmptyPayloadPackBytes()), Hex(Packs[0].Bytes)));
		ASSERT_THAT(AreEqual(
			TEXT("cab3a361e1034a5790e3f2d7cfa6b50803f4c7e026a83b95a657e777f48d4e8e"),
			Packs[0].PackId.ToHexString()));
		ASSERT_THAT(IsFalse(Hex(Packs[0].Bytes).Contains(Packs[0].PackId.ToHexString()),
			TEXT("PackId is whole-final-file metadata and is not self-serialized")));
		ASSERT_THAT(AreEqual(32u, ReadUInt32LittleEndian(Packs[0].Bytes, PackHeaderSizeOffset)));
		ASSERT_THAT(AreEqual(96u, ReadUInt32LittleEndian(Packs[0].Bytes, PackIndexEntrySizeOffset)));
		ASSERT_THAT(AreEqual(1u, ReadUInt32LittleEndian(Packs[0].Bytes, PackIndexCountOffset)));
		ASSERT_THAT(AreEqual(UINT64_C(128),
			ReadUInt64LittleEndian(Packs[0].Bytes, PackDataOffsetOffset)));
	}

	TEST_METHOD(MultiModuleMultiPackManifestFreezesKeyed65And122ByteEntries)
	{
		FAngelscriptEncodedCacheGenerationManifest Encoded;
		const FAngelscriptCacheValidationResult Result =
			EncodeAngelscriptCacheGenerationManifest(MakeMultiModuleManifestValue(), Encoded);
		ASSERT_THAT(IsTrue(Result.IsSuccess()));
		ASSERT_THAT(AreEqual(803, Encoded.CompleteBytes.Num()));
		ASSERT_THAT(AreEqual(Hex(FrozenMultiModuleManifestBytes()), Hex(Encoded.CompleteBytes)));
		ASSERT_THAT(AreEqual(
			TEXT("fb429c7cf8288d81489069702c9251c469b3cdd5085ec20cfc4eed23351437cd"),
			Encoded.ComputedGenerationId.ToHexString()));
		ASSERT_THAT(AreEqual(2u, ReadUInt32LittleEndian(
			Encoded.CompleteBytes, ManifestModuleCountOffset)));
		ASSERT_THAT(AreEqual(4u, ReadUInt32LittleEndian(
			Encoded.CompleteBytes,
			ManifestModuleCountOffset + sizeof(uint32) + 2 * 65)));
		ASSERT_THAT(AreEqual(static_cast<uint8>(EAngelscriptCacheRecordKind::ModuleSnapshot),
			Encoded.CompleteBytes[ManifestModuleCountOffset + sizeof(uint32) + 32]));
		ASSERT_THAT(AreEqual(static_cast<uint8>(EAngelscriptCacheRecordKind::SourceIndex),
			Encoded.CompleteBytes[ManifestModuleCountOffset + sizeof(uint32) + 2 * 65
				+ sizeof(uint32)]));
	}

	TEST_METHOD(MixedNoneZlibPackFreezesHeaderIndexesPayloadChecksumAndWholeFileId)
	{
		FAngelscriptPreparedRecord EmptySource;
		EmptySource.RecordId = RecordIdFromHex(
			TEXT("013dc136fb31bf03a8d55c17197191abbc50c708e405ebedf10354926365771b54"));
		FAngelscriptPreparedRecord CompressibleFunction;
		CompressibleFunction.RecordId = RecordIdFromHex(
			TEXT("051fdf4b6b71329edf74ab7b78039c44e418033b324ddfddcbc3e47142a3e63280"));
		CompressibleFunction.CanonicalPayload.Init(static_cast<uint8>('A'), 64);
		const FAngelscriptPreparedRecord Records[] = {EmptySource, CompressibleFunction};

		FAngelscriptCachePackPolicy Policy;
		Policy.CompressionPolicy = EAngelscriptCachePackCompressionPolicy::Auto;
		FAngelscriptUnrealZlibCacheStorageCodec Codec;
		TArray<FAngelscriptEncodedPack> Packs;
		const FAngelscriptCacheValidationResult Result = BuildAngelscriptCachePacks(
			Records, Policy, Codec, Packs);
		ASSERT_THAT(IsTrue(Result.IsSuccess()));
		ASSERT_THAT(AreEqual(1, Packs.Num()));
		ASSERT_THAT(AreEqual(236, Packs[0].Bytes.Num()));
		ASSERT_THAT(AreEqual(Hex(FrozenMixedPackBytes()), Hex(Packs[0].Bytes)));
		ASSERT_THAT(AreEqual(
			TEXT("3def100ec98858d1a8601261f5363445c5c5b8905b7659cbcfce8c2570f5c98e"),
			Packs[0].PackId.ToHexString()));
		ASSERT_THAT(AreEqual(2, Packs[0].Index.Num()));

		const int32 FirstIndex = FAngelscriptCacheManifestPackArchive::PackHeaderWireSize;
		const int32 SecondIndex = FirstIndex
			+ FAngelscriptCacheManifestPackArchive::PackIndexEntryWireSize;
		ASSERT_THAT(AreEqual(static_cast<uint8>(EAngelscriptCacheRecordKind::SourceIndex),
			Packs[0].Bytes[FirstIndex]));
		ASSERT_THAT(AreEqual(static_cast<uint8>(EAngelscriptCacheCodec::None),
			Packs[0].Bytes[FirstIndex + 1]));
		ASSERT_THAT(AreEqual(UINT64_C(224), ReadUInt64LittleEndian(
			Packs[0].Bytes, FirstIndex + 40)));
		ASSERT_THAT(AreEqual(UINT64_C(0), ReadUInt64LittleEndian(
			Packs[0].Bytes, FirstIndex + 48)));
		ASSERT_THAT(AreEqual(static_cast<uint8>(EAngelscriptCacheRecordKind::FunctionBody),
			Packs[0].Bytes[SecondIndex]));
		ASSERT_THAT(AreEqual(static_cast<uint8>(EAngelscriptCacheCodec::Zlib),
			Packs[0].Bytes[SecondIndex + 1]));
		ASSERT_THAT(AreEqual(UINT64_C(224), ReadUInt64LittleEndian(
			Packs[0].Bytes, SecondIndex + 40)));
		ASSERT_THAT(AreEqual(UINT64_C(12), ReadUInt64LittleEndian(
			Packs[0].Bytes, SecondIndex + 48)));
		ASSERT_THAT(AreEqual(UINT64_C(64), ReadUInt64LittleEndian(
			Packs[0].Bytes, SecondIndex + 56)));
		ASSERT_THAT(AreEqual(
			TEXT("78da7374a40c0000107e1041"),
			Hex(MakeArrayView(Packs[0].Bytes.GetData() + 224, 12))));
		ASSERT_THAT(AreEqual(
			TEXT("e028424e46205e56b2ed1ce1bf7087054072e6c4e41f843bed1e749db635792c"),
			Packs[0].Index[1].RawChecksum.ToHexString()));
	}

	TEST_METHOD(PacksStoreSemanticPayloadRatherThanNestedRecordEnvelopes)
	{
		FAngelscriptPreparedRecord Function;
		Function.RecordId = RecordIdFromHex(
			TEXT("051fdf4b6b71329edf74ab7b78039c44e418033b324ddfddcbc3e47142a3e63280"));
		Function.CanonicalPayload.Init(static_cast<uint8>('A'), 64);
		FAngelscriptCachePackPolicy Policy;
		Policy.CompressionPolicy = EAngelscriptCachePackCompressionPolicy::ForceNoneForTest;
		FAngelscriptUnrealZlibCacheStorageCodec Codec;
		TArray<FAngelscriptEncodedPack> Packs;
		ASSERT_THAT(IsTrue(BuildAngelscriptCachePacks(
			MakeArrayView(&Function, 1), Policy, Codec, Packs).IsSuccess()));
		ASSERT_THAT(AreEqual(1, Packs.Num()));
		ASSERT_THAT(AreEqual(192, Packs[0].Bytes.Num(),
			TEXT("32-byte header + one 96-byte index + exactly 64 semantic payload bytes")));
		ASSERT_THAT(AreEqual(UINT64_C(128), Packs[0].Index[0].PackOffset));
		ASSERT_THAT(AreEqual(UINT64_C(64), Packs[0].Index[0].StoredSize));
		ASSERT_THAT(AreEqual(UINT64_C(64), Packs[0].Index[0].RawSize));
		for (int32 Index = 128; Index < Packs[0].Bytes.Num(); ++Index)
		{
			ASSERT_THAT(AreEqual(static_cast<uint8>('A'), Packs[0].Bytes[Index]));
		}
		ASSERT_THAT(IsFalse(Hex(MakeArrayView(Packs[0].Bytes.GetData() + 128, 8))
			== TEXT("5545415343563241"),
			TEXT("A pack blob is never a nested UEASCV2A record envelope")));
	}

	TEST_METHOD(RawChecksumIsDirectPayloadHashWhileRecordIdRemainsDomainSeparated)
	{
		TArray<uint8> Payload;
		Payload.Init(static_cast<uint8>('A'), 64);
		const FAngelscriptHash256 DirectRawHash{
			FBlake3::HashBuffer(Payload.GetData(), static_cast<uint64>(Payload.Num()))};
		ASSERT_THAT(AreEqual(
			TEXT("e028424e46205e56b2ed1ce1bf7087054072e6c4e41f843bed1e749db635792c"),
			DirectRawHash.ToHexString()));

		FAngelscriptCacheRecordId FunctionId;
		FAngelscriptCacheRecordId DebugId;
		ASSERT_THAT(IsTrue(FAngelscriptCacheRecordArchive::TryBuildRecordId(
			EAngelscriptCacheRecordKind::FunctionBody, Payload, FunctionId).IsSuccess()));
		ASSERT_THAT(IsTrue(FAngelscriptCacheRecordArchive::TryBuildRecordId(
			EAngelscriptCacheRecordKind::DebugSidecar, Payload, DebugId).IsSuccess()));
		ASSERT_THAT(AreEqual(
			TEXT("1fdf4b6b71329edf74ab7b78039c44e418033b324ddfddcbc3e47142a3e63280"),
			FunctionId.ContentHash.ToHexString()));
		ASSERT_THAT(IsFalse(FunctionId.ContentHash == DirectRawHash));
		ASSERT_THAT(IsFalse(FunctionId == DebugId,
			TEXT("Equal payloads have one RawChecksum but kind-separated semantic RecordIds")));
	}

	TEST_METHOD(NoneAndZlibPreserveRecordIdButChangePackAndGenerationIds)
	{
		FAngelscriptPreparedRecord Function;
		Function.RecordId = RecordIdFromHex(
			TEXT("051fdf4b6b71329edf74ab7b78039c44e418033b324ddfddcbc3e47142a3e63280"));
		Function.CanonicalPayload.Init(static_cast<uint8>('A'), 64);
		FAngelscriptUnrealZlibCacheStorageCodec Codec;
		FAngelscriptCachePackPolicy NonePolicy;
		NonePolicy.CompressionPolicy = EAngelscriptCachePackCompressionPolicy::ForceNoneForTest;
		FAngelscriptCachePackPolicy AutoPolicy;
		AutoPolicy.CompressionPolicy = EAngelscriptCachePackCompressionPolicy::Auto;
		TArray<FAngelscriptEncodedPack> NonePacks;
		TArray<FAngelscriptEncodedPack> ZlibPacks;
		ASSERT_THAT(IsTrue(BuildAngelscriptCachePacks(
			MakeArrayView(&Function, 1), NonePolicy, Codec, NonePacks).IsSuccess()));
		ASSERT_THAT(IsTrue(BuildAngelscriptCachePacks(
			MakeArrayView(&Function, 1), AutoPolicy, Codec, ZlibPacks).IsSuccess()));
		ASSERT_THAT(AreEqual(1, NonePacks.Num()));
		ASSERT_THAT(AreEqual(1, ZlibPacks.Num()));
		ASSERT_THAT(IsTrue(NonePacks[0].Index[0].RecordId == ZlibPacks[0].Index[0].RecordId));
		ASSERT_THAT(IsFalse(NonePacks[0].PackId == ZlibPacks[0].PackId));
		ASSERT_THAT(AreEqual(
			TEXT("31eb4dd4802a35f8742c46f45dc781f091037841f982b9225d1d8cc0d8a22390"),
			NonePacks[0].PackId.ToHexString()));
		ASSERT_THAT(AreEqual(
			TEXT("fe843139ae082b72468153bc5bd5837c4a891978c9e4a234a9f94518983f121b"),
			ZlibPacks[0].PackId.ToHexString()));

		FAngelscriptCacheGenerationManifest NoneManifest = MakeMinimumManifestValue();
		NoneManifest.Records.Add({Function.RecordId,
			MakeLocation(NonePacks[0].PackId.ToHexString(), 128, 64, 64,
				EAngelscriptCacheCodec::None,
				TEXT("e028424e46205e56b2ed1ce1bf7087054072e6c4e41f843bed1e749db635792c"))});
		FAngelscriptCacheGenerationManifest ZlibManifest = MakeMinimumManifestValue();
		ZlibManifest.Records.Add({Function.RecordId,
			MakeLocation(ZlibPacks[0].PackId.ToHexString(), 128, 12, 64,
				EAngelscriptCacheCodec::Zlib,
				TEXT("e028424e46205e56b2ed1ce1bf7087054072e6c4e41f843bed1e749db635792c"))});
		FAngelscriptEncodedCacheGenerationManifest NoneGeneration;
		FAngelscriptEncodedCacheGenerationManifest ZlibGeneration;
		ASSERT_THAT(IsTrue(EncodeAngelscriptCacheGenerationManifest(
			NoneManifest, NoneGeneration).IsSuccess()));
		ASSERT_THAT(IsTrue(EncodeAngelscriptCacheGenerationManifest(
			ZlibManifest, ZlibGeneration).IsSuccess()));
		ASSERT_THAT(AreEqual(
			TEXT("d05690f3b10dd3f862f1181a8c47f9fa5e22533fd2526f3dff1056d142b252f7"),
			NoneGeneration.ComputedGenerationId.ToHexString()));
		ASSERT_THAT(AreEqual(
			TEXT("0d65eec00e6a474f1ad17eb26ae4cbb3c243d0be2027c3aa63bc060c41aa799a"),
			ZlibGeneration.ComputedGenerationId.ToHexString()));
		ASSERT_THAT(IsFalse(NoneGeneration.ComputedGenerationId
			== ZlibGeneration.ComputedGenerationId));
	}

	TEST_METHOD(ForcedSerialForwardReverseAndSeededRandomEmitIdenticalPacksManifestsAndIds)
	{
		FAngelscriptPreparedRecord Source;
		Source.RecordId = RecordIdFromHex(
			TEXT("013dc136fb31bf03a8d55c17197191abbc50c708e405ebedf10354926365771b54"));
		FAngelscriptPreparedRecord Function;
		Function.CanonicalPayload.Init(static_cast<uint8>('A'), 64);
		ASSERT_THAT(IsTrue(FAngelscriptCacheRecordArchive::TryBuildRecordId(
			EAngelscriptCacheRecordKind::FunctionBody,
			Function.CanonicalPayload, Function.RecordId).IsSuccess()));
		FAngelscriptPreparedRecord Debug;
		Debug.CanonicalPayload.Init(static_cast<uint8>('B'), 17);
		ASSERT_THAT(IsTrue(FAngelscriptCacheRecordArchive::TryBuildRecordId(
			EAngelscriptCacheRecordKind::DebugSidecar,
			Debug.CanonicalPayload, Debug.RecordId).IsSuccess()));

		auto MakeCompletion = [](
			const uint32 PreparationOrdinal,
			const uint32 CompletionOrdinal,
			const FAngelscriptPreparedRecord& Record)
		{
			FAngelscriptPreparedRecordCompletion Completion;
			Completion.PreparationOrdinal = PreparationOrdinal;
			Completion.CompletionOrdinal = CompletionOrdinal;
			Completion.Record = Record;
			return Completion;
		};
		const TArray<FAngelscriptPreparedRecordCompletion> ForcedSerial = {
			MakeCompletion(0, 0, Source),
			MakeCompletion(1, 1, Function),
			MakeCompletion(2, 2, Debug),
		};
		const TArray<FAngelscriptPreparedRecordCompletion> Forward = {
			MakeCompletion(2, 2, Debug),
			MakeCompletion(0, 0, Source),
			MakeCompletion(1, 1, Function),
		};
		const TArray<FAngelscriptPreparedRecordCompletion> Reverse = {
			MakeCompletion(0, 2, Source),
			MakeCompletion(2, 0, Debug),
			MakeCompletion(1, 1, Function),
		};
		TArray<uint32> SeededCompletionOrdinals = {0, 1, 2};
		FRandomStream RandomStream(0x5eed1234);
		for (int32 Index = SeededCompletionOrdinals.Num() - 1; Index > 0; --Index)
		{
			const int32 SwapIndex = RandomStream.RandRange(0, Index);
			SeededCompletionOrdinals.Swap(Index, SwapIndex);
		}
		if (SeededCompletionOrdinals == TArray<uint32>{0, 1, 2}
			|| SeededCompletionOrdinals == TArray<uint32>{2, 1, 0})
		{
			SeededCompletionOrdinals.Swap(0, 1);
		}
		const TArray<FAngelscriptPreparedRecordCompletion> SeededRandom = {
			MakeCompletion(0, SeededCompletionOrdinals[0], Source),
			MakeCompletion(1, SeededCompletionOrdinals[1], Function),
			MakeCompletion(2, SeededCompletionOrdinals[2], Debug),
		};

		FAngelscriptCachePackPolicy Policy;
		Policy.TargetRawBytesPerPack = 32;
		Policy.CompressionPolicy = EAngelscriptCachePackCompressionPolicy::ForceNoneForTest;
		FAngelscriptUnrealZlibCacheStorageCodec Codec;
		TArray<FAngelscriptEncodedPack> SerialPacks;
		TArray<FAngelscriptEncodedPack> ForwardPacks;
		TArray<FAngelscriptEncodedPack> ReversePacks;
		TArray<FAngelscriptEncodedPack> RandomPacks;
		ASSERT_THAT(IsTrue(AggregateAngelscriptCachePreparedRecordCompletions(
			ForcedSerial, EAngelscriptCachePreparationExecutionMode::ForcedSerial,
			Policy, Codec, SerialPacks).IsSuccess()));
		ASSERT_THAT(IsTrue(AggregateAngelscriptCachePreparedRecordCompletions(
			Forward, EAngelscriptCachePreparationExecutionMode::BoundedParallel,
			Policy, Codec, ForwardPacks).IsSuccess()));
		ASSERT_THAT(IsTrue(AggregateAngelscriptCachePreparedRecordCompletions(
			Reverse, EAngelscriptCachePreparationExecutionMode::BoundedParallel,
			Policy, Codec, ReversePacks).IsSuccess()));
		ASSERT_THAT(IsTrue(AggregateAngelscriptCachePreparedRecordCompletions(
			SeededRandom, EAngelscriptCachePreparationExecutionMode::BoundedParallel,
			Policy, Codec, RandomPacks).IsSuccess()));
		ASSERT_THAT(IsTrue(ExpectEncodedPackSetsEqual(
			*TestRunner, SerialPacks, ForwardPacks,
			TEXT("Forward completion produces identical canonical pack groups/indexes/bytes/IDs"))));
		ASSERT_THAT(IsTrue(ExpectEncodedPackSetsEqual(
			*TestRunner, SerialPacks, ReversePacks,
			TEXT("Reverse completion produces identical canonical pack groups/indexes/bytes/IDs"))));
		ASSERT_THAT(IsTrue(ExpectEncodedPackSetsEqual(
			*TestRunner, SerialPacks, RandomPacks,
			TEXT("Seeded-random completion produces identical canonical pack groups/indexes/bytes/IDs"))));

		FAngelscriptEncodedCacheGenerationManifest SerialManifest;
		FAngelscriptEncodedCacheGenerationManifest ForwardManifest;
		FAngelscriptEncodedCacheGenerationManifest ReverseManifest;
		FAngelscriptEncodedCacheGenerationManifest RandomManifest;
		ASSERT_THAT(IsTrue(EncodeAngelscriptCacheGenerationManifest(
			MakePhysicalManifestFromPacks(Source.RecordId, SerialPacks),
			SerialManifest).IsSuccess()));
		ASSERT_THAT(IsTrue(EncodeAngelscriptCacheGenerationManifest(
			MakePhysicalManifestFromPacks(Source.RecordId, ForwardPacks),
			ForwardManifest).IsSuccess()));
		ASSERT_THAT(IsTrue(EncodeAngelscriptCacheGenerationManifest(
			MakePhysicalManifestFromPacks(Source.RecordId, ReversePacks),
			ReverseManifest).IsSuccess()));
		ASSERT_THAT(IsTrue(EncodeAngelscriptCacheGenerationManifest(
			MakePhysicalManifestFromPacks(Source.RecordId, RandomPacks),
			RandomManifest).IsSuccess()));
		ASSERT_THAT(AreEqual(Hex(SerialManifest.CompleteBytes),
			Hex(ForwardManifest.CompleteBytes)));
		ASSERT_THAT(AreEqual(Hex(SerialManifest.CompleteBytes),
			Hex(ReverseManifest.CompleteBytes)));
		ASSERT_THAT(AreEqual(Hex(SerialManifest.CompleteBytes),
			Hex(RandomManifest.CompleteBytes)));
		ASSERT_THAT(IsTrue(SerialManifest.ComputedGenerationId
			== ForwardManifest.ComputedGenerationId));
		ASSERT_THAT(IsTrue(SerialManifest.ComputedGenerationId
			== ReverseManifest.ComputedGenerationId));
		ASSERT_THAT(IsTrue(SerialManifest.ComputedGenerationId
			== RandomManifest.ComputedGenerationId));
	}

	TEST_METHOD(PackHeaderSchemaCountReservedCodecAndPhysicalLimitFailuresAreExact)
	{
		const FAngelscriptCacheRecordIndexEntry ManifestEntry = MakeZlibFunctionManifestEntry();
		const TArray<uint8> Golden = FrozenZlibFunctionPackBytes();
		const FAngelscriptCacheReadLimits DefaultLimits;
		auto ExpectMutation = [&](const int32 Offset, const uint8 Value,
			const EAngelscriptCacheValidationError Error, const uint64 ErrorOffset,
			const TCHAR* Message)
		{
			TArray<uint8> Bytes = Golden;
			Bytes[Offset] = Value;
			ASSERT_THAT(IsTrue(ExpectPackIndexFailure(*TestRunner, Bytes, DirectHash(Bytes),
				DefaultLimits, Error, ErrorOffset, Message)));
		};

		ExpectMutation(0, static_cast<uint8>('X'), EAngelscriptCacheValidationError::BadMagic,
			0, TEXT("Pack magic is exact"));
		ExpectMutation(PackSchemaOffset, 2,
			EAngelscriptCacheValidationError::UnsupportedSchema,
			PackSchemaOffset, TEXT("Pack schema is exactly one"));
		ExpectMutation(PackHeaderSizeOffset, 31,
			EAngelscriptCacheValidationError::UnsupportedSchema,
			PackHeaderSizeOffset, TEXT("Pack header size is exactly 32"));
		ExpectMutation(PackIndexEntrySizeOffset, 95,
			EAngelscriptCacheValidationError::UnsupportedSchema,
			PackIndexEntrySizeOffset, TEXT("Pack index entry size is exactly 96"));
		ExpectMutation(PackIndexCountOffset, 0,
			EAngelscriptCacheValidationError::ImpossibleCount,
			PackIndexCountOffset, TEXT("A pack never contains zero entries"));
		ExpectMutation(PackDataOffsetOffset, 129,
			EAngelscriptCacheValidationError::OutOfBounds,
			PackDataOffsetOffset, TEXT("DataOffset is exactly header plus checked index bytes"));
		ExpectMutation(32 + 2, 1,
			EAngelscriptCacheValidationError::NonZeroReserved,
			34, TEXT("Reserved16 is zero"));
		ExpectMutation(32 + 4, 1,
			EAngelscriptCacheValidationError::NonZeroReserved,
			36, TEXT("Reserved32 is zero"));
		ExpectMutation(32 + 1, 2,
			EAngelscriptCacheValidationError::UnsupportedStorageCodec,
			33, TEXT("Only None and Zlib storage codecs exist"));

		FAngelscriptCacheReadLimits PackBytesShort = DefaultLimits;
		PackBytesShort.MaxPackBytes = static_cast<uint64>(Golden.Num() - 1);
		ASSERT_THAT(IsTrue(ExpectPackIndexFailure(*TestRunner, Golden, DirectHash(Golden),
			PackBytesShort, EAngelscriptCacheValidationError::BudgetExceeded, 0,
			TEXT("MaxPackBytes is checked before whole-pack allocation or scanning"))));

		FAngelscriptCacheReadLimits IndexCountShort = DefaultLimits;
		IndexCountShort.MaxPackIndexEntries = 0;
		ASSERT_THAT(IsTrue(ExpectPackIndexFailure(*TestRunner, Golden, DirectHash(Golden),
			IndexCountShort, EAngelscriptCacheValidationError::BudgetExceeded,
			PackIndexCountOffset, TEXT("MaxPackIndexEntries is a hard pre-allocation limit"))));

		FAngelscriptCacheReadLimits StoredRecordShort = DefaultLimits;
		StoredRecordShort.MaxStoredRecordBytes = 11;
		ASSERT_THAT(IsTrue(ExpectPackIndexFailure(*TestRunner, Golden, DirectHash(Golden),
			StoredRecordShort, EAngelscriptCacheValidationError::BudgetExceeded,
			32 + 48, TEXT("Each StoredSize is bounded before selecting a record"))));
	}

	TEST_METHOD(CodecSizeRulesMaxCanonicalAndDecompressedSizeMismatchAreBehavioral)
	{
		const FAngelscriptCacheReadLimits DefaultLimits;
		const TArray<uint8> EmptyNone = FrozenEmptyPayloadPackBytes();
		TArray<uint8> NoneSizeMismatch = EmptyNone;
		WriteUInt64LittleEndian(NoneSizeMismatch, PackIndexRawSizeOffset, 1);
		ASSERT_THAT(IsTrue(ExpectPackIndexFailure(*TestRunner,
			NoneSizeMismatch, DirectHash(NoneSizeMismatch), DefaultLimits,
			EAngelscriptCacheValidationError::OutOfBounds,
			PackIndexRawSizeOffset,
			TEXT("None requires StoredSize equal RawSize, including the zero-size case"))));

		const TArray<uint8> Zlib = FrozenZlibFunctionPackBytes();
		auto ExpectZlibSizeMutation = [&](const int32 FieldOffset, const uint64 Value,
			const TCHAR* Message)
		{
			TArray<uint8> Bytes = Zlib;
			WriteUInt64LittleEndian(Bytes, FieldOffset, Value);
			ASSERT_THAT(IsTrue(ExpectPackIndexFailure(*TestRunner,
				Bytes, DirectHash(Bytes), DefaultLimits,
				EAngelscriptCacheValidationError::OutOfBounds,
				FieldOffset, Message)));
		};
		ExpectZlibSizeMutation(PackIndexStoredSizeOffset, 0,
			TEXT("Zlib StoredSize is nonzero"));
		ExpectZlibSizeMutation(PackIndexRawSizeOffset, 0,
			TEXT("Zlib RawSize is nonzero"));
		ExpectZlibSizeMutation(PackIndexRawSizeOffset, 12,
			TEXT("Zlib StoredSize is strictly smaller than RawSize"));

		FAngelscriptCacheReadLimits ExactCanonicalLimits = DefaultLimits;
		ExactCanonicalLimits.MaxCanonicalRecordPayloadBytes = 64;
		ExactCanonicalLimits.MaxStoredRecordBytes = 12;
		FAngelscriptCacheReadBudget ExactBudget;
		TArray<FAngelscriptCachePackIndexEntry> ExactIndex;
		ASSERT_THAT(IsTrue(ValidateAngelscriptCachePack(
			Zlib, DirectHash(Zlib), ExactCanonicalLimits, ExactBudget, ExactIndex).IsSuccess(),
			TEXT("Exact MaxCanonicalRecordPayloadBytes and MaxStoredRecordBytes succeed")));
		ASSERT_THAT(AreEqual(1, ExactIndex.Num()));

		FAngelscriptCacheReadLimits CanonicalOneShort = ExactCanonicalLimits;
		CanonicalOneShort.MaxCanonicalRecordPayloadBytes = 63;
		ASSERT_THAT(IsTrue(ExpectPackIndexFailure(*TestRunner,
			Zlib, DirectHash(Zlib), CanonicalOneShort,
			EAngelscriptCacheValidationError::BudgetExceeded,
			PackIndexRawSizeOffset,
			TEXT("MaxCanonicalRecordPayloadBytes one-short fails before allocation"))));

		const FAngelscriptCacheRecordIndexEntry Entry = MakeZlibFunctionManifestEntry();
		const uint64 PackIndexBytes =
			static_cast<uint64>(sizeof(FAngelscriptCachePackIndexEntry));
		for (const int64 ProducedSizeDelta : {-INT64_C(1), INT64_C(1)})
		{
			FReportedDecompressedSizeCodec Codec(ProducedSizeDelta);
			FAngelscriptCacheReadBudget Budget;
			TOptional<FAngelscriptDecodedCacheRecordHandle> OutRecord;
			const FAngelscriptCacheValidationResult Result =
				ReadAngelscriptCacheRecordFromPack(
					Zlib, DirectHash(Zlib), Entry, DefaultLimits,
					Budget, Codec, OutRecord);
			ASSERT_THAT(AreEqual(
				EAngelscriptCacheValidationError::DecompressedSizeMismatch,
				Result.Error,
				ProducedSizeDelta < 0
					? TEXT("Successful decompression reporting short output is rejected")
					: TEXT("Successful decompression reporting excess output is rejected")));
			ASSERT_THAT(AreEqual(EAngelscriptCacheValidationStage::PackDecode,
				Result.Stage));
			ASSERT_THAT(AreEqual(UINT64_C(128), Result.ByteOffset));
			ASSERT_THAT(IsFalse(OutRecord.IsSet()));
			ASSERT_THAT(AreEqual(UINT64_C(12), Budget.GetStoredBytes()));
			ASSERT_THAT(AreEqual(UINT64_C(64), Budget.GetDecompressedBytes()));
			ASSERT_THAT(AreEqual(PackIndexBytes + UINT64_C(64),
				Budget.GetDecodedBytes(),
				TEXT("The temporary Pack index and raw output share one decoded budget")));
			ASSERT_THAT(AreEqual(UINT64_C(0),
				Budget.GetTemporaryResidentDecodedBytes(),
				TEXT("Failed decompression releases the raw temporary buffer")));
			ASSERT_THAT(AreEqual(PackIndexBytes + UINT64_C(64),
				Budget.GetPeakLiveResidentDecodedBytes()));
		}
	}

	TEST_METHOD(PackRangeGapOverflowTrailingEmptyAndOverlapRulesAreFrozen)
	{
		const TArray<uint8> Golden = FrozenZlibFunctionPackBytes();
		const FAngelscriptCacheReadLimits Limits;
		auto ExpectOffsetMutation = [&](const uint64 Offset,
			const EAngelscriptCacheValidationError Error, const uint64 ErrorOffset,
			const TCHAR* Message)
		{
			TArray<uint8> Bytes = Golden;
			WriteUInt64LittleEndian(Bytes, 32 + 40, Offset);
			ASSERT_THAT(IsTrue(ExpectPackIndexFailure(*TestRunner, Bytes, DirectHash(Bytes),
				Limits, Error, ErrorOffset, Message)));
		};
		ExpectOffsetMutation(129, EAngelscriptCacheValidationError::OutOfBounds,
			72, TEXT("A gap before the first blob is invalid"));
		ExpectOffsetMutation(127, EAngelscriptCacheValidationError::OutOfBounds,
			72, TEXT("A blob cannot point into the index"));
		ExpectOffsetMutation(MAX_uint64 - 6, EAngelscriptCacheValidationError::Overflow,
			72, TEXT("PackOffset plus StoredSize uses checked arithmetic"));

		TArray<uint8> Trailing = Golden;
		Trailing.Add(0xee);
		ASSERT_THAT(IsTrue(ExpectPackIndexFailure(*TestRunner, Trailing, DirectHash(Trailing),
			Limits, EAngelscriptCacheValidationError::TrailingData,
			static_cast<uint64>(Golden.Num()), TEXT("Bytes after the final exact blob are trailing"))));

		FAngelscriptPreparedRecord EmptySource;
		ASSERT_THAT(IsTrue(FAngelscriptCacheRecordArchive::TryBuildRecordId(
			EAngelscriptCacheRecordKind::SourceIndex, {}, EmptySource.RecordId).IsSuccess()));
		FAngelscriptPreparedRecord EmptyFunction;
		ASSERT_THAT(IsTrue(FAngelscriptCacheRecordArchive::TryBuildRecordId(
			EAngelscriptCacheRecordKind::FunctionBody, {}, EmptyFunction.RecordId).IsSuccess()));
		const FAngelscriptPreparedRecord EmptyRecords[] = {EmptySource, EmptyFunction};
		FAngelscriptCachePackPolicy EmptyPolicy;
		EmptyPolicy.CompressionPolicy = EAngelscriptCachePackCompressionPolicy::ForceNoneForTest;
		FAngelscriptUnrealZlibCacheStorageCodec Codec;
		TArray<FAngelscriptEncodedPack> EmptyPacks;
		ASSERT_THAT(IsTrue(BuildAngelscriptCachePacks(
			EmptyRecords, EmptyPolicy, Codec, EmptyPacks).IsSuccess()));
		ASSERT_THAT(AreEqual(1, EmptyPacks.Num()));
		ASSERT_THAT(AreEqual(2, EmptyPacks[0].Index.Num()));
		ASSERT_THAT(AreEqual(EmptyPacks[0].Index[0].PackOffset,
			EmptyPacks[0].Index[1].PackOffset));
		FAngelscriptCacheReadBudget EmptyBudget;
		TArray<FAngelscriptCachePackIndexEntry> EmptyIndex;
		ASSERT_THAT(IsTrue(ValidateAngelscriptCachePack(EmptyPacks[0].Bytes,
			EmptyPacks[0].PackId, Limits, EmptyBudget, EmptyIndex).IsSuccess(),
			TEXT("Two empty half-open ranges at one offset do not overlap")));

		FAngelscriptPreparedRecord First;
		First.CanonicalPayload = {0x11, 0x12, 0x13};
		ASSERT_THAT(IsTrue(FAngelscriptCacheRecordArchive::TryBuildRecordId(
			EAngelscriptCacheRecordKind::FunctionBody, First.CanonicalPayload,
			First.RecordId).IsSuccess()));
		FAngelscriptPreparedRecord Second;
		Second.CanonicalPayload = {0x21, 0x22, 0x23};
		ASSERT_THAT(IsTrue(FAngelscriptCacheRecordArchive::TryBuildRecordId(
			EAngelscriptCacheRecordKind::DebugSidecar, Second.CanonicalPayload,
			Second.RecordId).IsSuccess()));
		const FAngelscriptPreparedRecord NonEmptyRecords[] = {First, Second};
		TArray<FAngelscriptEncodedPack> NonEmptyPacks;
		ASSERT_THAT(IsTrue(BuildAngelscriptCachePacks(
			NonEmptyRecords, EmptyPolicy, Codec, NonEmptyPacks).IsSuccess()));
		ASSERT_THAT(AreEqual(1, NonEmptyPacks.Num()));
		TArray<uint8> Overlap = NonEmptyPacks[0].Bytes;
		const int32 SecondOffsetField = 32 + 96 + 40;
		const uint64 OverlappingOffset = NonEmptyPacks[0].Index[0].PackOffset
			+ NonEmptyPacks[0].Index[0].StoredSize - 1;
		WriteUInt64LittleEndian(Overlap, SecondOffsetField, OverlappingOffset);
		ASSERT_THAT(IsTrue(ExpectPackIndexFailure(*TestRunner, Overlap, DirectHash(Overlap),
			Limits, EAngelscriptCacheValidationError::OverlappingRange,
			static_cast<uint64>(SecondOffsetField),
			TEXT("A later backwards non-empty range is classified as overlap"))));

		TArray<uint8> LaterGap = NonEmptyPacks[0].Bytes;
		WriteUInt64LittleEndian(LaterGap, SecondOffsetField,
			NonEmptyPacks[0].Index[1].PackOffset + 1);
		ASSERT_THAT(IsTrue(ExpectPackIndexFailure(*TestRunner,
			LaterGap, DirectHash(LaterGap), Limits,
			EAngelscriptCacheValidationError::OutOfBounds,
			static_cast<uint64>(SecondOffsetField),
			TEXT("A non-empty gap before a later blob is invalid"))));

		const int32 SecondStoredSizeField = 32 + 96 + 48;
		const int32 SecondRawSizeField = 32 + 96 + 56;
		TArray<uint8> LaterRangePastEof = NonEmptyPacks[0].Bytes;
		const uint64 PastEofSize = NonEmptyPacks[0].Index[1].StoredSize + 1;
		WriteUInt64LittleEndian(
			LaterRangePastEof, SecondStoredSizeField, PastEofSize);
		WriteUInt64LittleEndian(
			LaterRangePastEof, SecondRawSizeField, PastEofSize);
		ASSERT_THAT(IsTrue(ExpectPackIndexFailure(*TestRunner,
			LaterRangePastEof, DirectHash(LaterRangePastEof), Limits,
			EAngelscriptCacheValidationError::OutOfBounds,
			static_cast<uint64>(SecondStoredSizeField),
			TEXT("A later non-empty range cannot extend beyond EOF"))));

		TArray<uint8> LaterRangeOverflow = NonEmptyPacks[0].Bytes;
		WriteUInt64LittleEndian(LaterRangeOverflow, SecondOffsetField, MAX_uint64 - 1);
		ASSERT_THAT(IsTrue(ExpectPackIndexFailure(*TestRunner,
			LaterRangeOverflow, DirectHash(LaterRangeOverflow), Limits,
			EAngelscriptCacheValidationError::Overflow,
			static_cast<uint64>(SecondOffsetField),
			TEXT("Every later PackOffset plus StoredSize uses checked arithmetic"))));
	}

	TEST_METHOD(PackOrderDuplicatePackIdLocationCodecChecksumAndRecordIdFailuresPrecedeDecode)
	{
		FAngelscriptCachePackPolicy Policy;
		Policy.CompressionPolicy = EAngelscriptCachePackCompressionPolicy::ForceNoneForTest;
		FAngelscriptUnrealZlibCacheStorageCodec Codec;
		FAngelscriptPreparedRecord Function;
		Function.CanonicalPayload = {0x31, 0x32, 0x33};
		ASSERT_THAT(IsTrue(FAngelscriptCacheRecordArchive::TryBuildRecordId(
			EAngelscriptCacheRecordKind::FunctionBody, Function.CanonicalPayload,
			Function.RecordId).IsSuccess()));
		FAngelscriptPreparedRecord Debug;
		Debug.CanonicalPayload = {0x41, 0x42, 0x43};
		ASSERT_THAT(IsTrue(FAngelscriptCacheRecordArchive::TryBuildRecordId(
			EAngelscriptCacheRecordKind::DebugSidecar, Debug.CanonicalPayload,
			Debug.RecordId).IsSuccess()));
		const FAngelscriptPreparedRecord Records[] = {Function, Debug};
		TArray<FAngelscriptEncodedPack> Packs;
		ASSERT_THAT(IsTrue(BuildAngelscriptCachePacks(Records, Policy, Codec, Packs).IsSuccess()));
		ASSERT_THAT(AreEqual(1, Packs.Num()));
		const FAngelscriptCacheReadLimits Limits;

		TArray<uint8> Reordered = Packs[0].Bytes;
		Reordered[32] = static_cast<uint8>(EAngelscriptCacheRecordKind::DebugSidecar);
		Reordered[32 + 96] = static_cast<uint8>(EAngelscriptCacheRecordKind::FunctionBody);
		ASSERT_THAT(IsTrue(ExpectPackIndexFailure(*TestRunner, Reordered, DirectHash(Reordered),
			Limits, EAngelscriptCacheValidationError::NonCanonicalOrder,
			32 + 96, TEXT("Pack indexes sort by complete RecordId"))));

		TArray<uint8> Duplicate = Packs[0].Bytes;
		FMemory::Memcpy(Duplicate.GetData() + 32 + 96,
			Duplicate.GetData() + 32, 1);
		FMemory::Memcpy(Duplicate.GetData() + 32 + 96 + 8,
			Duplicate.GetData() + 32 + 8, 32);
		ASSERT_THAT(IsTrue(ExpectPackIndexFailure(*TestRunner, Duplicate, DirectHash(Duplicate),
			Limits, EAngelscriptCacheValidationError::DuplicateKey,
			32 + 96, TEXT("Duplicate pack RecordIds fail even when locations differ"))));

		const TArray<uint8> ZlibPack = FrozenZlibFunctionPackBytes();
		const FAngelscriptCacheRecordIndexEntry ZlibEntry = MakeZlibFunctionManifestEntry();
		ASSERT_THAT(IsTrue(ExpectPackFailure(*TestRunner, ZlibPack, HashFromHex(
			TEXT("0101010101010101010101010101010101010101010101010101010101010101")),
			ZlibEntry, Limits, EAngelscriptCacheValidationError::PackIdMismatch, 0,
			TEXT("Expected PackId covers every final file byte"))));

		auto ExpectLocationMismatch = [&](FAngelscriptCacheRecordIndexEntry WrongLocation,
			const uint64 ErrorOffset, const TCHAR* Message)
		{
			ASSERT_THAT(IsTrue(ExpectPackFailure(*TestRunner,
				ZlibPack, DirectHash(ZlibPack), WrongLocation, Limits,
				EAngelscriptCacheValidationError::PackIndexMismatch,
				ErrorOffset, Message)));
		};
		FAngelscriptCacheRecordIndexEntry WrongPackAssociation = ZlibEntry;
		WrongPackAssociation.Location.PackId = OrdinalHash(1, 0xe1);
		ExpectLocationMismatch(WrongPackAssociation, 0,
			TEXT("Manifest PackId association equals the complete containing pack"));
		FAngelscriptCacheRecordIndexEntry WrongOffset = ZlibEntry;
		WrongOffset.Location.PackOffset = 129;
		ExpectLocationMismatch(WrongOffset, PackIndexPackOffsetOffset,
			TEXT("Manifest PackOffset equals the internal pack index"));
		FAngelscriptCacheRecordIndexEntry WrongStoredSize = ZlibEntry;
		WrongStoredSize.Location.StoredSize = 11;
		ExpectLocationMismatch(WrongStoredSize, PackIndexStoredSizeOffset,
			TEXT("Manifest StoredSize equals the internal pack index"));
		FAngelscriptCacheRecordIndexEntry WrongRawSize = ZlibEntry;
		WrongRawSize.Location.RawSize = 63;
		ExpectLocationMismatch(WrongRawSize, PackIndexRawSizeOffset,
			TEXT("Manifest RawSize equals the internal pack index"));
		FAngelscriptCacheRecordIndexEntry WrongCodec = ZlibEntry;
		WrongCodec.Location.Codec = EAngelscriptCacheCodec::None;
		ExpectLocationMismatch(WrongCodec, PackIndexCodecOffset,
			TEXT("Manifest Codec equals the internal pack index"));
		FAngelscriptCacheRecordIndexEntry WrongRawChecksumLocation = ZlibEntry;
		WrongRawChecksumLocation.Location.RawChecksum = OrdinalHash(1, 0xe2);
		ExpectLocationMismatch(WrongRawChecksumLocation, PackIndexRawChecksumOffset,
			TEXT("Manifest RawChecksum equals the internal pack index"));

		TArray<uint8> TrailingZlib = ZlibPack;
		TrailingZlib.Add(0);
		WriteUInt64LittleEndian(TrailingZlib, 32 + 48, 13);
		FAngelscriptCacheRecordIndexEntry TrailingEntry = ZlibEntry;
		TrailingEntry.Location.PackId = DirectHash(TrailingZlib);
		TrailingEntry.Location.StoredSize = 13;
		ASSERT_THAT(IsTrue(ExpectPackFailure(*TestRunner, TrailingZlib,
			DirectHash(TrailingZlib), TrailingEntry, Limits,
			EAngelscriptCacheValidationError::DecompressionFailed, 128,
			TEXT("Canonical recompression rejects trailing compressed input"))));
		const uint64 PackIndexBytes =
			static_cast<uint64>(sizeof(FAngelscriptCachePackIndexEntry));
		const uint64 CanonicalVerificationBytes =
			PackIndexBytes + TrailingEntry.Location.RawSize
			+ TrailingEntry.Location.StoredSize;
		{
			FAngelscriptCacheReadBudget Budget;
			TOptional<FAngelscriptDecodedCacheRecordHandle> Out;
			const FAngelscriptCacheValidationResult Result = ReadZlibFunctionPack(
				TrailingZlib, DirectHash(TrailingZlib), TrailingEntry,
				Limits, Budget, Out);
			ASSERT_THAT(AreEqual(
				EAngelscriptCacheValidationError::DecompressionFailed, Result.Error));
			ASSERT_THAT(AreEqual(CanonicalVerificationBytes,
				Budget.GetDecodedBytes(),
				TEXT("Canonical verification charges index, raw and fixed recompression scratch")));
			ASSERT_THAT(AreEqual(CanonicalVerificationBytes,
				Budget.GetPeakLiveResidentDecodedBytes()));
			ASSERT_THAT(AreEqual(UINT64_C(0),
				Budget.GetTemporaryResidentDecodedBytes()));
			ASSERT_THAT(IsFalse(Out.IsSet()));
		}
		{
			FAngelscriptCacheReadLimits OneShort = Limits;
			OneShort.MaxTotalDecodedBytes = CanonicalVerificationBytes - 1;
			OneShort.MaxResidentDecodedBytes = CanonicalVerificationBytes - 1;
			FAngelscriptCacheReadBudget Budget;
			TOptional<FAngelscriptDecodedCacheRecordHandle> Out;
			const FAngelscriptCacheValidationResult Result = ReadZlibFunctionPack(
				TrailingZlib, DirectHash(TrailingZlib), TrailingEntry,
				OneShort, Budget, Out);
			ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::BudgetExceeded,
				Result.Error,
				TEXT("One byte short rejects canonical recompression before allocation")));
			ASSERT_THAT(AreEqual(EAngelscriptCacheValidationStage::PackDecode,
				Result.Stage));
			ASSERT_THAT(AreEqual(UINT64_C(80), Result.ByteOffset));
			ASSERT_THAT(AreEqual(
				PackIndexBytes + TrailingEntry.Location.RawSize,
				Budget.GetDecodedBytes()));
			ASSERT_THAT(AreEqual(UINT64_C(0),
				Budget.GetTemporaryResidentDecodedBytes()));
			ASSERT_THAT(IsFalse(Out.IsSet()));
		}

		TArray<uint8> WrongChecksum = ZlibPack;
		WrongChecksum[32 + 64] ^= 0x01;
		FAngelscriptCacheRecordIndexEntry WrongChecksumEntry = ZlibEntry;
		FBlake3Hash::ByteArray WrongChecksumBytes{};
		FMemory::Memcpy(WrongChecksumBytes, WrongChecksum.GetData() + 32 + 64, 32);
		WrongChecksumEntry.Location.PackId = DirectHash(WrongChecksum);
		WrongChecksumEntry.Location.RawChecksum =
			FAngelscriptHash256{FBlake3Hash(WrongChecksumBytes)};
		ASSERT_THAT(IsTrue(ExpectPackFailure(*TestRunner, WrongChecksum,
			DirectHash(WrongChecksum), WrongChecksumEntry, Limits,
			EAngelscriptCacheValidationError::ChecksumMismatch, 32 + 64,
			TEXT("RawChecksum is checked after decompression"))));

		TArray<uint8> WrongRecordId = ZlibPack;
		WrongRecordId[32 + 8] ^= 0x01;
		FAngelscriptCacheRecordIndexEntry WrongRecordIdEntry = ZlibEntry;
		FBlake3Hash::ByteArray WrongRecordHash{};
		FMemory::Memcpy(WrongRecordHash, WrongRecordId.GetData() + 32 + 8, 32);
		WrongRecordIdEntry.Location.PackId = DirectHash(WrongRecordId);
		WrongRecordIdEntry.RecordId.ContentHash =
			FAngelscriptHash256{FBlake3Hash(WrongRecordHash)};
		ASSERT_THAT(IsTrue(ExpectPackFailure(*TestRunner, WrongRecordId,
			DirectHash(WrongRecordId), WrongRecordIdEntry, Limits,
			EAngelscriptCacheValidationError::RecordIdMismatch, 32 + 8,
			TEXT("Semantic RecordId is recomputed from kind and raw payload"))));
	}

	TEST_METHOD(PackReadChargesStoredDecompressedAndRawResidentBudgetsInOrder)
	{
		const TArray<uint8> Pack = FrozenZlibFunctionPackBytes();
		const FAngelscriptCacheRecordIndexEntry Entry = MakeZlibFunctionManifestEntry();
		const uint64 PackIndexBytes =
			static_cast<uint64>(sizeof(FAngelscriptCachePackIndexEntry));
		{
			FAngelscriptCacheReadLimits Limits;
			Limits.MaxTotalStoredBytes = 11;
			FAngelscriptCacheReadBudget Budget;
			TOptional<FAngelscriptDecodedCacheRecordHandle> Out;
			const FAngelscriptCacheValidationResult Result = ReadZlibFunctionPack(
				Pack, DirectHash(Pack), Entry, Limits, Budget, Out);
			ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::BudgetExceeded, Result.Error));
			ASSERT_THAT(AreEqual(EAngelscriptCacheValidationStage::PackDecode, Result.Stage));
			ASSERT_THAT(AreEqual(UINT64_C(0), Budget.GetStoredBytes()));
			ASSERT_THAT(AreEqual(UINT64_C(0), Budget.GetDecompressedBytes()));
			ASSERT_THAT(AreEqual(PackIndexBytes, Budget.GetDecodedBytes(),
				TEXT("Pack validation charges its temporary index before record I/O")));
			ASSERT_THAT(AreEqual(UINT64_C(0),
				Budget.GetTemporaryResidentDecodedBytes()));
			ASSERT_THAT(IsFalse(Out.IsSet()));
		}
		{
			FAngelscriptCacheReadLimits Limits;
			Limits.MaxTotalDecompressedBytes = 63;
			FAngelscriptCacheReadBudget Budget;
			TOptional<FAngelscriptDecodedCacheRecordHandle> Out;
			const FAngelscriptCacheValidationResult Result = ReadZlibFunctionPack(
				Pack, DirectHash(Pack), Entry, Limits, Budget, Out);
			ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::BudgetExceeded, Result.Error));
			ASSERT_THAT(AreEqual(UINT64_C(12), Budget.GetStoredBytes()));
			ASSERT_THAT(AreEqual(UINT64_C(0), Budget.GetDecompressedBytes()));
			ASSERT_THAT(AreEqual(PackIndexBytes, Budget.GetDecodedBytes()));
			ASSERT_THAT(AreEqual(UINT64_C(0),
				Budget.GetTemporaryResidentDecodedBytes()));
			ASSERT_THAT(IsFalse(Out.IsSet()));
		}
		{
			FAngelscriptCacheReadLimits Limits;
			Limits.MaxTotalDecodedBytes = PackIndexBytes + 63;
			Limits.MaxResidentDecodedBytes = PackIndexBytes + 63;
			FAngelscriptCacheReadBudget Budget;
			TOptional<FAngelscriptDecodedCacheRecordHandle> Out;
			const FAngelscriptCacheValidationResult Result = ReadZlibFunctionPack(
				Pack, DirectHash(Pack), Entry, Limits, Budget, Out);
			ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::BudgetExceeded, Result.Error));
			ASSERT_THAT(AreEqual(UINT64_C(12), Budget.GetStoredBytes()));
			ASSERT_THAT(AreEqual(UINT64_C(64), Budget.GetDecompressedBytes()));
			ASSERT_THAT(AreEqual(PackIndexBytes, Budget.GetDecodedBytes(),
				TEXT("The one-short raw reservation leaves the charged Pack index monotonic")));
			ASSERT_THAT(AreEqual(UINT64_C(0), Budget.GetResidentDecodedBytes()));
			ASSERT_THAT(AreEqual(UINT64_C(0),
				Budget.GetTemporaryResidentDecodedBytes()));
			ASSERT_THAT(IsFalse(Out.IsSet()));
		}
	}

	TEST_METHOD(ManifestHeaderKeysProfileCountsKindsCodecEofAndGenerationIdAreExact)
	{
		const TArray<uint8> Golden = FrozenMinimumManifestBytes();
		const FAngelscriptCacheReadLimits Limits;
		auto ExpectMutation = [&](const int32 Offset, const uint8 Value,
			const EAngelscriptCacheValidationError Error, const uint64 ErrorOffset,
			const TCHAR* Message)
		{
			TArray<uint8> Bytes = Golden;
			Bytes[Offset] = Value;
			ASSERT_THAT(IsTrue(ExpectManifestFailure(*TestRunner, Bytes, DirectHash(Bytes),
				Limits, Error, EAngelscriptCacheValidationStage::ManifestDecode,
				ErrorOffset, Message)));
		};
		ExpectMutation(0, static_cast<uint8>('X'),
			EAngelscriptCacheValidationError::BadMagic, 0,
			TEXT("Manifest magic is exactly UEASCV2M"));
		ExpectMutation(ManifestSchemaOffset, 2,
			EAngelscriptCacheValidationError::UnsupportedSchema,
			ManifestSchemaOffset, TEXT("Manifest schema is exactly one"));
		ExpectMutation(ManifestFlagsOffset, 1,
			EAngelscriptCacheValidationError::UnknownFlags,
			ManifestFlagsOffset, TEXT("Manifest V1 flags are exactly zero"));
		TArray<uint8> ZeroCompatibility = Golden;
		FMemory::Memzero(ZeroCompatibility.GetData() + 16, 32);
		ASSERT_THAT(IsTrue(ExpectManifestFailure(*TestRunner, ZeroCompatibility,
			DirectHash(ZeroCompatibility), Limits,
			EAngelscriptCacheValidationError::ZeroStableKey,
			EAngelscriptCacheValidationStage::ManifestDecode, 16,
			TEXT("CompatibilityKey is a nonzero full hash"))));
		TArray<uint8> ZeroContext = Golden;
		FMemory::Memzero(ZeroContext.GetData() + 48, 32);
		ASSERT_THAT(IsTrue(ExpectManifestFailure(*TestRunner, ZeroContext,
			DirectHash(ZeroContext), Limits,
			EAngelscriptCacheValidationError::ZeroStableKey,
			EAngelscriptCacheValidationStage::ManifestDecode, 48,
			TEXT("ContextKey is a nonzero full hash"))));

		TArray<uint8> ZeroProfile = Golden;
		FMemory::Memzero(ZeroProfile.GetData() + 80, 32);
		ASSERT_THAT(IsTrue(ExpectManifestFailure(*TestRunner, ZeroProfile,
			DirectHash(ZeroProfile), Limits, EAngelscriptCacheValidationError::ZeroStableKey,
			EAngelscriptCacheValidationStage::ManifestDecode, 80,
			TEXT("ArtifactProfileKey is nonzero"))));
		TArray<uint8> WrongProfile = Golden;
		WrongProfile[80] ^= 1;
		ASSERT_THAT(IsTrue(ExpectManifestFailure(*TestRunner, WrongProfile,
			DirectHash(WrongProfile), Limits,
			EAngelscriptCacheValidationError::DerivedHashMismatch,
			EAngelscriptCacheValidationStage::ManifestDecode, 80,
			TEXT("Profile recomputes exactly from Compatibility and Context"))));
		TArray<uint8> ZeroSourceSnapshot = Golden;
		FMemory::Memzero(ZeroSourceSnapshot.GetData() + 112, 32);
		ASSERT_THAT(IsTrue(ExpectManifestFailure(*TestRunner, ZeroSourceSnapshot,
			DirectHash(ZeroSourceSnapshot), Limits,
			EAngelscriptCacheValidationError::ZeroStableKey,
			EAngelscriptCacheValidationStage::ManifestDecode, 112,
			TEXT("SourceSnapshot is a nonzero full hash"))));

		ExpectMutation(144, static_cast<uint8>(EAngelscriptCacheRecordKind::FunctionBody),
			EAngelscriptCacheValidationError::WrongRecordKind, 144,
			TEXT("The manifest root is exactly one SourceIndex"));
		ExpectMutation(MinimumManifestRecordCountOffset, 0,
			EAngelscriptCacheValidationError::ImpossibleCount,
			MinimumManifestRecordCountOffset,
			TEXT("RecordCount is at least one"));
		ExpectMutation(MinimumManifestRecordCountOffset, 2,
			EAngelscriptCacheValidationError::ImpossibleCount,
			MinimumManifestRecordCountOffset,
			TEXT("Count times minimum wire bytes is proven against remaining bytes"));
		ExpectMutation(MinimumManifestRecordEntryOffset + 33 + 32 + 8 + 8 + 8, 2,
			EAngelscriptCacheValidationError::UnsupportedStorageCodec,
			MinimumManifestRecordEntryOffset + 33 + 32 + 8 + 8 + 8,
			TEXT("Manifest locations admit only None or Zlib"));

		TArray<uint8> Trailing = Golden;
		Trailing.Add(0xee);
		ASSERT_THAT(IsTrue(ExpectManifestFailure(*TestRunner, Trailing, DirectHash(Trailing),
			Limits, EAngelscriptCacheValidationError::TrailingData,
			EAngelscriptCacheValidationStage::ManifestDecode,
			static_cast<uint64>(Golden.Num()), TEXT("Manifest EOF is exact"))));

		ASSERT_THAT(IsTrue(ExpectManifestFailure(*TestRunner, Golden,
			HashFromHex(TEXT(
				"0101010101010101010101010101010101010101010101010101010101010101")),
			Limits, EAngelscriptCacheValidationError::GenerationIdMismatch,
			EAngelscriptCacheValidationStage::ManifestDecode, 0,
			TEXT("Outer GenerationId covers the whole final manifest"))));
	}

	TEST_METHOD(ManifestRejectsZeroRootAndLocationKeysWrongModuleRootAndMissingIndexes)
	{
		FAngelscriptEncodedCacheGenerationManifest Encoded;
		ASSERT_THAT(IsTrue(EncodeAngelscriptCacheGenerationManifest(
			MakeMultiModuleManifestValue(), Encoded).IsSuccess()));
		const TArray<uint8> Golden = Encoded.CompleteBytes;
		const FAngelscriptCacheReadLimits Limits;
		constexpr int32 FirstRoot = 181;
		constexpr int32 FirstRootRecordKind = FirstRoot + 32;
		constexpr int32 FirstRootRecordHash = FirstRootRecordKind + 1;
		constexpr int32 FirstRecord = FirstRoot + 2 * 65 + sizeof(uint32);
		constexpr int32 FirstRecordHash = FirstRecord + 1;
		constexpr int32 FirstRecordPackId = FirstRecord + 33;

		auto ExpectZeroHash = [&](const int32 Offset, const uint64 ErrorOffset,
			const TCHAR* Message)
		{
			TArray<uint8> Bytes = Golden;
			FMemory::Memzero(Bytes.GetData() + Offset, 32);
			ASSERT_THAT(IsTrue(ExpectManifestFailure(*TestRunner,
				Bytes, DirectHash(Bytes), Limits,
				EAngelscriptCacheValidationError::ZeroStableKey,
				EAngelscriptCacheValidationStage::ManifestDecode,
				ErrorOffset, Message)));
		};
		ExpectZeroHash(FirstRoot, FirstRoot,
			TEXT("Every keyed ModuleSnapshot root has a nonzero ModuleKey"));
		ExpectZeroHash(145, 145,
			TEXT("The SourceIndex root RecordId has a nonzero content hash"));
		ExpectZeroHash(FirstRootRecordHash, FirstRootRecordHash,
			TEXT("Every ModuleSnapshot root RecordId has a nonzero content hash"));
		ExpectZeroHash(FirstRecordHash, FirstRecordHash,
			TEXT("Every manifest-index RecordId has a nonzero content hash"));
		ExpectZeroHash(FirstRecordPackId, FirstRecordPackId,
			TEXT("Every manifest location has a nonzero PackId"));

		TArray<uint8> WrongModuleRootKind = Golden;
		WrongModuleRootKind[FirstRootRecordKind] =
			static_cast<uint8>(EAngelscriptCacheRecordKind::FunctionBody);
		ASSERT_THAT(IsTrue(ExpectManifestFailure(*TestRunner,
			WrongModuleRootKind, DirectHash(WrongModuleRootKind), Limits,
			EAngelscriptCacheValidationError::WrongRecordKind,
			EAngelscriptCacheValidationStage::ManifestDecode,
			FirstRootRecordKind,
			TEXT("Every keyed module root is exactly ModuleSnapshot kind"))));

		TArray<uint8> MissingSourceIndex = Golden;
		MissingSourceIndex[145] ^= 0x80;
		ASSERT_THAT(IsTrue(ExpectManifestFailure(*TestRunner,
			MissingSourceIndex, DirectHash(MissingSourceIndex), Limits,
			EAngelscriptCacheValidationError::MissingRecord,
			EAngelscriptCacheValidationStage::ManifestDecode, 144,
			TEXT("SourceIndexRecordId occurs exactly once in the manifest index"))));

		TArray<uint8> MissingModuleRoot = Golden;
		MissingModuleRoot[FirstRootRecordHash] ^= 0x80;
		ASSERT_THAT(IsTrue(ExpectManifestFailure(*TestRunner,
			MissingModuleRoot, DirectHash(MissingModuleRoot), Limits,
			EAngelscriptCacheValidationError::MissingRecord,
			EAngelscriptCacheValidationStage::ManifestDecode,
			FirstRootRecordKind,
			TEXT("Every keyed ModuleSnapshot root occurs exactly once in the manifest index"))));
	}

	TEST_METHOD(ManifestRootAndRecordOrderDuplicateConflictAndBudgetFailuresPrecedePackLookup)
	{
		FAngelscriptEncodedCacheGenerationManifest Encoded;
		ASSERT_THAT(IsTrue(EncodeAngelscriptCacheGenerationManifest(
			MakeMultiModuleManifestValue(), Encoded).IsSuccess()));
		const TArray<uint8> Golden = Encoded.CompleteBytes;
		const FAngelscriptCacheReadLimits Limits;
		constexpr int32 FirstRoot = 181;
		constexpr int32 SecondRoot = FirstRoot + 65;
		constexpr int32 RecordCount = FirstRoot + 2 * 65;
		constexpr int32 FirstRecord = RecordCount + 4;
		constexpr int32 ThirdRecord = FirstRecord + 2 * 122;
		constexpr int32 FourthRecord = FirstRecord + 3 * 122;

		TArray<uint8> RootOrder = Golden;
		uint8 FirstModuleKey[32]{};
		FMemory::Memcpy(FirstModuleKey, RootOrder.GetData() + FirstRoot, 32);
		FMemory::Memcpy(RootOrder.GetData() + FirstRoot,
			RootOrder.GetData() + SecondRoot, 32);
		FMemory::Memcpy(RootOrder.GetData() + SecondRoot, FirstModuleKey, 32);
		ASSERT_THAT(IsTrue(ExpectManifestFailure(*TestRunner, RootOrder,
			DirectHash(RootOrder), Limits,
			EAngelscriptCacheValidationError::NonCanonicalOrder,
			EAngelscriptCacheValidationStage::ManifestDecode, SecondRoot,
			TEXT("ModuleSnapshot roots sort by full ModuleKey"))));

		TArray<uint8> DuplicateRoot = Golden;
		FMemory::Memcpy(DuplicateRoot.GetData() + SecondRoot,
			DuplicateRoot.GetData() + FirstRoot, 65);
		ASSERT_THAT(IsTrue(ExpectManifestFailure(*TestRunner, DuplicateRoot,
			DirectHash(DuplicateRoot), Limits,
			EAngelscriptCacheValidationError::DuplicateKey,
			EAngelscriptCacheValidationStage::ManifestDecode, SecondRoot,
			TEXT("An identical keyed root is DuplicateKey"))));

		TArray<uint8> ConflictingRoot = Golden;
		FMemory::Memcpy(ConflictingRoot.GetData() + SecondRoot,
			ConflictingRoot.GetData() + FirstRoot, 32);
		ASSERT_THAT(IsTrue(ExpectManifestFailure(*TestRunner, ConflictingRoot,
			DirectHash(ConflictingRoot), Limits,
			EAngelscriptCacheValidationError::ConflictingKey,
			EAngelscriptCacheValidationStage::ManifestDecode, SecondRoot,
			TEXT("One ModuleKey mapped to another root RecordId is ConflictingKey"))));

		TArray<uint8> RecordOrder = Golden;
		uint8 ThirdRecordId[33]{};
		FMemory::Memcpy(ThirdRecordId, RecordOrder.GetData() + ThirdRecord, 33);
		FMemory::Memcpy(RecordOrder.GetData() + ThirdRecord,
			RecordOrder.GetData() + FourthRecord, 33);
		FMemory::Memcpy(RecordOrder.GetData() + FourthRecord, ThirdRecordId, 33);
		ASSERT_THAT(IsTrue(ExpectManifestFailure(*TestRunner, RecordOrder,
			DirectHash(RecordOrder), Limits,
			EAngelscriptCacheValidationError::NonCanonicalOrder,
			EAngelscriptCacheValidationStage::ManifestDecode, FourthRecord,
			TEXT("Manifest locations sort by complete RecordId"))));

		TArray<uint8> DuplicateRecord = Golden;
		FMemory::Memcpy(DuplicateRecord.GetData() + FourthRecord,
			DuplicateRecord.GetData() + ThirdRecord, 122);
		ASSERT_THAT(IsTrue(ExpectManifestFailure(*TestRunner, DuplicateRecord,
			DirectHash(DuplicateRecord), Limits,
			EAngelscriptCacheValidationError::DuplicateKey,
			EAngelscriptCacheValidationStage::ManifestDecode, FourthRecord,
			TEXT("An identical manifest location is DuplicateKey"))));

		TArray<uint8> ConflictingRecord = Golden;
		FMemory::Memcpy(ConflictingRecord.GetData() + FourthRecord,
			ConflictingRecord.GetData() + ThirdRecord, 33);
		ASSERT_THAT(IsTrue(ExpectManifestFailure(*TestRunner, ConflictingRecord,
			DirectHash(ConflictingRecord), Limits,
			EAngelscriptCacheValidationError::ConflictingKey,
			EAngelscriptCacheValidationStage::ManifestDecode, FourthRecord,
			TEXT("One RecordId with another location is ConflictingKey"))));

		FAngelscriptCacheReadLimits ManifestBytesShort = Limits;
		ManifestBytesShort.MaxManifestBytes = static_cast<uint64>(Golden.Num() - 1);
		ASSERT_THAT(IsTrue(ExpectManifestFailure(*TestRunner, Golden, DirectHash(Golden),
			ManifestBytesShort, EAngelscriptCacheValidationError::BudgetExceeded,
			EAngelscriptCacheValidationStage::ManifestDecode, 0,
			TEXT("MaxManifestBytes is checked before whole-file allocation"))));
		FAngelscriptCacheReadLimits RootsShort = Limits;
		RootsShort.MaxModuleSnapshots = 1;
		ASSERT_THAT(IsTrue(ExpectManifestFailure(*TestRunner, Golden, DirectHash(Golden),
			RootsShort, EAngelscriptCacheValidationError::BudgetExceeded,
			EAngelscriptCacheValidationStage::ManifestDecode, ManifestModuleCountOffset,
			TEXT("MaxModuleSnapshots is enforced before root allocation"))));
		FAngelscriptCacheReadLimits RecordsShort = Limits;
		RecordsShort.MaxGenerationRecords = 3;
		ASSERT_THAT(IsTrue(ExpectManifestFailure(*TestRunner, Golden, DirectHash(Golden),
			RecordsShort, EAngelscriptCacheValidationError::BudgetExceeded,
			EAngelscriptCacheValidationStage::ManifestDecode, RecordCount,
			TEXT("MaxGenerationRecords is enforced before record-index allocation"))));
	}

	TEST_METHOD(MaxGenerationPacksPlusOneFailsBeforeAnyPackLookupOrOpenForWriterAndReader)
	{
		const TArray<uint8> Bytes = MakeManifestWithDefaultMaxGenerationPacksPlusOne();
		ASSERT_THAT(AreEqual(181 + 4 + 4097 * 122, Bytes.Num()));
		FAngelscriptCacheReadLimits Limits;
		ASSERT_THAT(AreEqual(UINT64_C(4096), Limits.MaxGenerationPacks));
		FCountingPackSource Packs;
		FAngelscriptCacheReadBudget Budget;
		TOptional<FAngelscriptValidatedGeneration> OutGeneration;
		const FAngelscriptCacheValidationResult ReadResult = ValidateGeneration(
			Bytes, DirectHash(Bytes), Packs, Limits, Budget, OutGeneration);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::BudgetExceeded,
			ReadResult.Error));
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationClass::ArithmeticOrBudget,
			ReadResult.Class));
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationStage::ManifestDecode,
			ReadResult.Stage));
		ASSERT_THAT(AreEqual(UINT64_C(181), ReadResult.ByteOffset));
		ASSERT_THAT(AreEqual(0, Packs.GetLookupOrOpenCount(),
			TEXT("4097 distinct PackIds are rejected before any pack-source side effect")));
		ASSERT_THAT(IsFalse(OutGeneration.IsSet()));

		FAngelscriptCacheGenerationManifest WriterValue = MakeMinimumManifestValue();
		WriterValue.Records.Reset();
		for (uint32 Ordinal = 0; Ordinal < 4097; ++Ordinal)
		{
			const FAngelscriptCacheRecordId RecordId = Ordinal == 0
				? WriterValue.SourceIndexRecordId
				: FAngelscriptCacheRecordId{
					EAngelscriptCacheRecordKind::FunctionBody,
					OrdinalHash(Ordinal, 0x10)};
			WriterValue.Records.Add({RecordId,
				MakeLocation(OrdinalHash(Ordinal + 1, 0x20).ToHexString(),
					128, 0, 0, EAngelscriptCacheCodec::None,
					TEXT("af1349b9f5f9a1a6a0404dea36dcc9499bcb25c9adc112b7cc9a93cae41f3262"))});
		}
		FAngelscriptEncodedCacheGenerationManifest WriterOutput;
		WriterOutput.CompleteBytes = {0xde, 0xad};
		const FAngelscriptCacheValidationResult WriteResult =
			EncodeAngelscriptCacheGenerationManifest(WriterValue, WriterOutput);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::BudgetExceeded,
			WriteResult.Error));
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationStage::ManifestDecode,
			WriteResult.Stage));
		ASSERT_THAT(IsTrue(WriterOutput.CompleteBytes.IsEmpty(),
			TEXT("The writer emits no partial manifest beyond MaxGenerationPacks")));
		ASSERT_THAT(IsTrue(WriterOutput.ComputedGenerationId.IsZero()));
	}

	TEST_METHOD(PackHistoricalExtrasAreAcceptedButManifestIndexedExtrasAreUnexpected)
	{
		FAngelscriptHash256 SelectedSnapshot;
		FAngelscriptHash256 HistoricalSnapshot;
		const FAngelscriptPreparedRecord Selected =
			MakeMinimalSourceRecord(1, SelectedSnapshot);
		const FAngelscriptPreparedRecord Historical =
			MakeMinimalSourceRecord(2, HistoricalSnapshot);
		ASSERT_THAT(IsFalse(Selected.RecordId == Historical.RecordId));
		const FAngelscriptPreparedRecord Prepared[] = {Historical, Selected};
		FAngelscriptCachePackPolicy Policy;
		Policy.CompressionPolicy = EAngelscriptCachePackCompressionPolicy::ForceNoneForTest;
		FAngelscriptUnrealZlibCacheStorageCodec Codec;
		TArray<FAngelscriptEncodedPack> Packs;
		ASSERT_THAT(IsTrue(BuildAngelscriptCachePacks(
			Prepared, Policy, Codec, Packs).IsSuccess()));
		ASSERT_THAT(AreEqual(1, Packs.Num()));
		ASSERT_THAT(AreEqual(2, Packs[0].Index.Num()));

		const FAngelscriptCachePackIndexEntry* SelectedPackEntry = nullptr;
		const FAngelscriptCachePackIndexEntry* HistoricalPackEntry = nullptr;
		for (const FAngelscriptCachePackIndexEntry& Entry : Packs[0].Index)
		{
			if (Entry.RecordId == Selected.RecordId)
			{
				SelectedPackEntry = &Entry;
			}
			if (Entry.RecordId == Historical.RecordId)
			{
				HistoricalPackEntry = &Entry;
			}
		}
		ASSERT_THAT(IsNotNull(SelectedPackEntry));
		ASSERT_THAT(IsNotNull(HistoricalPackEntry));
		if (SelectedPackEntry == nullptr || HistoricalPackEntry == nullptr)
		{
			return;
		}

		FAngelscriptCacheGenerationManifest SelectedOnly = MakeMinimumManifestValue();
		SelectedOnly.SourceSnapshot = SelectedSnapshot;
		SelectedOnly.SourceIndexRecordId = Selected.RecordId;
		SelectedOnly.Records = {{Selected.RecordId,
			LocationFromPack(Packs[0], *SelectedPackEntry)}};
		FAngelscriptEncodedCacheGenerationManifest SelectedManifest;
		ASSERT_THAT(IsTrue(EncodeAngelscriptCacheGenerationManifest(
			SelectedOnly, SelectedManifest).IsSuccess()));
		FCountingPackSource SelectedSource;
		SelectedSource.Add(Packs[0].PackId, Packs[0].Bytes);
		FAngelscriptCacheReadBudget SelectedBudget;
		TOptional<FAngelscriptValidatedGeneration> SelectedGeneration;
		const FAngelscriptCacheValidationResult SelectedResult = ValidateGeneration(
			SelectedManifest.CompleteBytes, SelectedManifest.ComputedGenerationId,
			SelectedSource, FAngelscriptCacheReadLimits{}, SelectedBudget,
			SelectedGeneration);
		ASSERT_THAT(IsTrue(SelectedResult.IsSuccess(),
			TEXT("A selected SourceIndex may share a pack with historical unselected records")));
		ASSERT_THAT(IsTrue(SelectedGeneration.IsSet()));
		ASSERT_THAT(AreEqual(1, SelectedSource.GetLookupOrOpenCount()));

		FAngelscriptCacheGenerationManifest WithManifestExtra = SelectedOnly;
		WithManifestExtra.Records.Add({Historical.RecordId,
			LocationFromPack(Packs[0], *HistoricalPackEntry)});
		WithManifestExtra.Records.Sort([](
			const FAngelscriptCacheRecordIndexEntry& A,
			const FAngelscriptCacheRecordIndexEntry& B)
		{
			return A.RecordId < B.RecordId;
		});
		FAngelscriptEncodedCacheGenerationManifest ExtraManifest;
		ASSERT_THAT(IsTrue(EncodeAngelscriptCacheGenerationManifest(
			WithManifestExtra, ExtraManifest).IsSuccess()));
		FCountingPackSource ExtraSource;
		ExtraSource.Add(Packs[0].PackId, Packs[0].Bytes);
		FAngelscriptCacheReadBudget ExtraBudget;
		TOptional<FAngelscriptValidatedGeneration> ExtraGeneration;
		const FAngelscriptCacheValidationResult ExtraResult = ValidateGeneration(
			ExtraManifest.CompleteBytes, ExtraManifest.ComputedGenerationId,
			ExtraSource, FAngelscriptCacheReadLimits{}, ExtraBudget, ExtraGeneration);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::UnexpectedRecord,
			ExtraResult.Error));
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationClass::GraphOrOwnership,
			ExtraResult.Class));
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationStage::ManifestGraph,
			ExtraResult.Stage));
		ASSERT_THAT(IsFalse(ExtraGeneration.IsSet()));
		ASSERT_THAT(AreEqual(1, ExtraSource.GetLookupOrOpenCount(),
			TEXT("One distinct PackId is opened once even when two records are selected")));
		ASSERT_THAT(AreEqual(UINT64_C(0),
			ExtraBudget.GetResidentDecodedBytes(),
			TEXT("A late exact-reachability failure never promotes decoded record ownership")));
		ASSERT_THAT(AreEqual(UINT64_C(0),
			ExtraBudget.GetTemporaryResidentDecodedBytes(),
			TEXT("A late exact-reachability failure releases the complete generation candidate")));
	}

	TEST_METHOD(ExactReachabilityCoversRootsChildrenDebugOwnersWrongKindsMissingAndExtras)
	{
		const FAngelscriptStableModuleKey ModuleKey{OrdinalHash(1, 0x31)};
		const FAngelscriptCacheRecordId Source{
			EAngelscriptCacheRecordKind::SourceIndex, OrdinalHash(1, 0x41)};
		const FAngelscriptCacheRecordId Interface{
			EAngelscriptCacheRecordKind::ModuleInterface, OrdinalHash(1, 0x42)};
		const FAngelscriptCacheRecordId State{
			EAngelscriptCacheRecordKind::ModuleState, OrdinalHash(1, 0x43)};
		const FAngelscriptCacheRecordId Function{
			EAngelscriptCacheRecordKind::FunctionBody, OrdinalHash(1, 0x44)};
		const FAngelscriptCacheRecordId Debug{
			EAngelscriptCacheRecordKind::DebugSidecar, OrdinalHash(1, 0x45)};
		const FAngelscriptCacheRecordId Snapshot{
			EAngelscriptCacheRecordKind::ModuleSnapshot, OrdinalHash(1, 0x46)};

		const FAngelscriptCacheReachabilityRootForTests Root{
			{ModuleKey, Snapshot}, 181};
		TArray<FAngelscriptCacheReachabilityNodeForTests> Nodes;
		Nodes.SetNum(6);
		Nodes[0].RecordId = Source;
		Nodes[0].EmbeddedSourceSnapshot = OrdinalHash(1, 0x01);
		Nodes[1].RecordId = Interface;
		Nodes[2].RecordId = State;
		Nodes[3].RecordId = Function;
		Nodes[3].DebugSidecar = Debug;
		Nodes[4].RecordId = Debug;
		Nodes[5].RecordId = Snapshot;
		Nodes[5].EmbeddedModuleKey = ModuleKey;
		Nodes[5].ModuleInterface = Interface;
		Nodes[5].ModuleState = State;
		Nodes[5].FunctionBodies = {Function};
		const FAngelscriptCacheReachabilityManifestEntryForTests ExactIndex[] = {
			{Source, 185},
			{Interface, 307},
			{State, 429},
			{Function, 551},
			{Debug, 673},
			{Snapshot, 795},
		};
		FAngelscriptCacheReadLimits Limits;
		FAngelscriptCacheReadBudget Budget;
		TArray<FAngelscriptCacheRecordId> Visited;
		const FAngelscriptCacheValidationResult ExactResult =
			ValidateAngelscriptCacheGenerationReachabilityForTests(
				OrdinalHash(1, 0x01), Source,
				FReachabilityFixture::SourceRootManifestByteOffset,
				MakeArrayView(&Root, 1), ExactIndex, Nodes,
				Limits, Budget, Visited, nullptr);
		ASSERT_THAT(IsTrue(ExactResult.IsSuccess()));
		ASSERT_THAT(AreEqual(6, Visited.Num()));

		TArray<FAngelscriptCacheReachabilityManifestEntryForTests> MissingDebug(
			MakeArrayView(ExactIndex));
		MissingDebug.RemoveAt(4);
		Visited = {Source};
		FAngelscriptCacheReadBudget MissingBudget;
		const FAngelscriptCacheValidationResult MissingResult =
			ValidateAngelscriptCacheGenerationReachabilityForTests(
				OrdinalHash(1, 0x01), Source,
				FReachabilityFixture::SourceRootManifestByteOffset,
				MakeArrayView(&Root, 1), MissingDebug, Nodes,
				Limits, MissingBudget, Visited, nullptr);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::MissingRecord,
			MissingResult.Error));
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationStage::ManifestGraph,
			MissingResult.Stage));
		ASSERT_THAT(AreEqual(static_cast<uint8>(EAngelscriptCacheRecordKind::DebugSidecar),
			static_cast<uint8>(MissingResult.RecordKind)));
		ASSERT_THAT(IsTrue(Visited.IsEmpty()));

		TArray<FAngelscriptCacheReachabilityNodeForTests> WrongDebugNodes = Nodes;
		WrongDebugNodes[3].DebugSidecar = FAngelscriptCacheRecordId{
			EAngelscriptCacheRecordKind::TypeSchema, Debug.ContentHash};
		Visited = {Source};
		FAngelscriptCacheReadBudget WrongKindBudget;
		const FAngelscriptCacheValidationResult WrongKindResult =
			ValidateAngelscriptCacheGenerationReachabilityForTests(
				OrdinalHash(1, 0x01), Source,
				FReachabilityFixture::SourceRootManifestByteOffset,
				MakeArrayView(&Root, 1), ExactIndex, WrongDebugNodes,
				Limits, WrongKindBudget, Visited, nullptr);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::WrongRecordKind,
			WrongKindResult.Error));
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationStage::ManifestGraph,
			WrongKindResult.Stage));
		ASSERT_THAT(AreEqual(UINT64_C(551), WrongKindResult.ByteOffset));
		ASSERT_THAT(IsTrue(Visited.IsEmpty()));

		TArray<FAngelscriptCacheReachabilityManifestEntryForTests> WithExtra(
			MakeArrayView(ExactIndex));
		WithExtra.Add({FAngelscriptCacheRecordId{
			EAngelscriptCacheRecordKind::TypeSchema, OrdinalHash(9, 0x55)}, 917});
		WithExtra.Sort([](
			const FAngelscriptCacheReachabilityManifestEntryForTests& A,
			const FAngelscriptCacheReachabilityManifestEntryForTests& B)
		{
			return A.RecordId < B.RecordId;
		});
		Visited = {Source};
		FAngelscriptCacheReadBudget ExtraBudget;
		const FAngelscriptCacheValidationResult ExtraResult =
			ValidateAngelscriptCacheGenerationReachabilityForTests(
				OrdinalHash(1, 0x01), Source,
				FReachabilityFixture::SourceRootManifestByteOffset,
				MakeArrayView(&Root, 1), WithExtra, Nodes,
				Limits, ExtraBudget, Visited, nullptr);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::UnexpectedRecord,
			ExtraResult.Error));
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationStage::ManifestGraph,
			ExtraResult.Stage));
		ASSERT_THAT(AreEqual(UINT64_C(917), ExtraResult.ByteOffset));
		ASSERT_THAT(IsTrue(Visited.IsEmpty()));

		TArray<FAngelscriptCacheReachabilityNodeForTests> WrongRootNodes = Nodes;
		WrongRootNodes[5].EmbeddedModuleKey =
			FAngelscriptStableModuleKey{OrdinalHash(2, 0x31)};
		Visited = {Source};
		FAngelscriptCacheReadBudget WrongRootBudget;
		const FAngelscriptCacheValidationResult WrongRootResult =
			ValidateAngelscriptCacheGenerationReachabilityForTests(
				OrdinalHash(1, 0x01), Source,
				FReachabilityFixture::SourceRootManifestByteOffset,
				MakeArrayView(&Root, 1), ExactIndex, WrongRootNodes,
				Limits, WrongRootBudget, Visited, nullptr);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::CrossModuleOwner,
			WrongRootResult.Error));
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationStage::ManifestGraph,
			WrongRootResult.Stage));
		ASSERT_THAT(AreEqual(UINT64_C(795), WrongRootResult.ByteOffset));
		ASSERT_THAT(IsTrue(Visited.IsEmpty()));
	}

	TEST_METHOD(ExactReachabilityVisitsEveryTargetAndCallsEachRootModuleGraphExactlyOnce)
	{
		const FReachabilityFixture Fixture;
		FAngelscriptStableModuleKey GraphCallStorage[2];
		FAngelscriptCacheModuleGraphValidationProbeForTests Probe;
		Probe.ModuleKeys = MakeArrayView(GraphCallStorage);
		FAngelscriptCacheReadBudget Budget;
		TArray<FAngelscriptCacheRecordId> Visited;
		const FAngelscriptCacheValidationResult Result = ValidateReachability(
			Fixture, FAngelscriptCacheReadLimits{}, Budget, Visited, &Probe);
		ASSERT_THAT(IsTrue(Result.IsSuccess()));
		ASSERT_THAT(AreEqual(Fixture.Index.Num(), Visited.Num()));
		for (int32 Index = 0; Index < Fixture.Index.Num(); ++Index)
		{
			ASSERT_THAT(IsTrue(Visited[Index] == Fixture.Index[Index].RecordId,
				TEXT("The final canonical visited set equals the complete manifest index")));
		}
		const EAngelscriptCacheRecordKind ExpectedKinds[] = {
			EAngelscriptCacheRecordKind::SourceIndex,
			EAngelscriptCacheRecordKind::ModuleInterface,
			EAngelscriptCacheRecordKind::TypeSchema,
			EAngelscriptCacheRecordKind::ModuleState,
			EAngelscriptCacheRecordKind::FunctionBody,
			EAngelscriptCacheRecordKind::DebugSidecar,
			EAngelscriptCacheRecordKind::ModuleSnapshot,
		};
		for (const EAngelscriptCacheRecordKind Kind : ExpectedKinds)
		{
			ASSERT_THAT(IsTrue(Visited.ContainsByPredicate([Kind](
				const FAngelscriptCacheRecordId& RecordId)
			{
				return RecordId.Kind == Kind;
			}), TEXT("The exact fixture traverses every V1 target kind")));
		}
		ASSERT_THAT(AreEqual(UINT64_C(2), Probe.CallCount));
		ASSERT_THAT(IsFalse(Probe.bOverflow));
		ASSERT_THAT(IsTrue(GraphCallStorage[0].Hash == Fixture.FirstModuleKey.Hash));
		ASSERT_THAT(IsTrue(GraphCallStorage[1].Hash == Fixture.SecondModuleKey.Hash));
	}

	TEST_METHOD(ReachabilityRejectsEveryMissingAndWrongKindRootChildAndDebugTarget)
	{
		enum class ETarget : uint8
		{
			SourceIndex,
			Root,
			ModuleInterface,
			TypeSchema,
			ModuleState,
			FunctionBody,
			DebugSidecar,
		};
		struct FTargetCase
		{
			ETarget Target;
			FAngelscriptCacheRecordId FReachabilityFixture::* Record;
			EAngelscriptCacheRecordKind ExpectedKind;
			uint64 OwnerOffset;
			const TCHAR* Name;
		};
		const FTargetCase Cases[] = {
			{ETarget::SourceIndex, &FReachabilityFixture::Source,
				EAngelscriptCacheRecordKind::SourceIndex, 144, TEXT("source root")},
			{ETarget::Root, &FReachabilityFixture::FirstSnapshot,
				EAngelscriptCacheRecordKind::ModuleSnapshot, 181, TEXT("root")},
			{ETarget::ModuleInterface, &FReachabilityFixture::FirstInterface,
				EAngelscriptCacheRecordKind::ModuleInterface, 1161, TEXT("module interface child")},
			{ETarget::TypeSchema, &FReachabilityFixture::TypeSchema,
				EAngelscriptCacheRecordKind::TypeSchema, 1161, TEXT("type schema child")},
			{ETarget::ModuleState, &FReachabilityFixture::FirstState,
				EAngelscriptCacheRecordKind::ModuleState, 1161, TEXT("module state child")},
			{ETarget::FunctionBody, &FReachabilityFixture::Function,
				EAngelscriptCacheRecordKind::FunctionBody, 1161, TEXT("function child")},
			{ETarget::DebugSidecar, &FReachabilityFixture::Debug,
				EAngelscriptCacheRecordKind::DebugSidecar, 917, TEXT("debug child")},
		};
		auto ReplaceTarget = [](FReachabilityFixture& Fixture, const ETarget Target,
			const FAngelscriptCacheRecordId& Replacement)
		{
			switch (Target)
			{
			case ETarget::Root:
				Fixture.Roots[0].Link.RecordId = Replacement;
				break;
			case ETarget::SourceIndex:
				Fixture.Source = Replacement;
				break;
			case ETarget::ModuleInterface:
				Fixture.Nodes[8].ModuleInterface = Replacement;
				break;
			case ETarget::TypeSchema:
				Fixture.Nodes[8].TypeSchemas[0] = Replacement;
				break;
			case ETarget::ModuleState:
				Fixture.Nodes[8].ModuleState = Replacement;
				break;
			case ETarget::FunctionBody:
				Fixture.Nodes[8].FunctionBodies[0] = Replacement;
				break;
			case ETarget::DebugSidecar:
				Fixture.Nodes[6].DebugSidecar = Replacement;
				break;
			}
		};

		for (const FTargetCase& Case : Cases)
		{
			FReachabilityFixture MissingFixture;
			const FAngelscriptCacheRecordId MissingTarget = MissingFixture.*(Case.Record);
			const int32 MissingIndex = MissingFixture.FindIndexPosition(MissingTarget);
			check(MissingIndex != INDEX_NONE);
			MissingFixture.Index.RemoveAt(MissingIndex);
			FAngelscriptCacheReadBudget MissingBudget;
			TArray<FAngelscriptCacheRecordId> MissingVisited = {MissingFixture.Source};
			const FAngelscriptCacheValidationResult MissingResult = ValidateReachability(
				MissingFixture, FAngelscriptCacheReadLimits{}, MissingBudget, MissingVisited);
			ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::MissingRecord,
				MissingResult.Error, Case.Name));
			ASSERT_THAT(AreEqual(EAngelscriptCacheValidationStage::ManifestGraph,
				MissingResult.Stage));
			ASSERT_THAT(AreEqual(Case.OwnerOffset, MissingResult.ByteOffset));
			ASSERT_THAT(AreEqual(static_cast<uint8>(Case.ExpectedKind),
				static_cast<uint8>(MissingResult.RecordKind)));
			ASSERT_THAT(IsTrue(MissingVisited.IsEmpty()));

			FReachabilityFixture WrongKindFixture;
			const FAngelscriptCacheRecordId Original = WrongKindFixture.*(Case.Record);
			const EAngelscriptCacheRecordKind WrongKind = Case.ExpectedKind
				== EAngelscriptCacheRecordKind::SourceIndex
				? EAngelscriptCacheRecordKind::FunctionBody
				: EAngelscriptCacheRecordKind::SourceIndex;
			ReplaceTarget(WrongKindFixture, Case.Target,
				FAngelscriptCacheRecordId{WrongKind, Original.ContentHash});
			FAngelscriptCacheReadBudget WrongKindBudget;
			TArray<FAngelscriptCacheRecordId> WrongKindVisited = {WrongKindFixture.Source};
			const FAngelscriptCacheValidationResult WrongKindResult = ValidateReachability(
				WrongKindFixture, FAngelscriptCacheReadLimits{},
				WrongKindBudget, WrongKindVisited);
			ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::WrongRecordKind,
				WrongKindResult.Error, Case.Name));
			ASSERT_THAT(AreEqual(EAngelscriptCacheValidationStage::ManifestGraph,
				WrongKindResult.Stage));
			ASSERT_THAT(AreEqual(Case.OwnerOffset, WrongKindResult.ByteOffset));
			ASSERT_THAT(AreEqual(static_cast<uint8>(Case.ExpectedKind),
				static_cast<uint8>(WrongKindResult.RecordKind)));
			ASSERT_THAT(IsTrue(WrongKindVisited.IsEmpty()));
		}
	}

	TEST_METHOD(ReachabilityRejectsSourceSnapshotMismatchAndEveryUnreachableTargetKind)
	{
		{
			FReachabilityFixture Fixture;
			Fixture.ManifestSourceSnapshot = OrdinalHash(2, 0x01);
			ASSERT_THAT(IsFalse(Fixture.ManifestSourceSnapshot
				== Fixture.Nodes[0].EmbeddedSourceSnapshot,
				TEXT("Manifest and decoded SourceIndex snapshots are independent coordinates")));
			FAngelscriptCacheReadBudget Budget;
			TArray<FAngelscriptCacheRecordId> Visited = {Fixture.Source};
			const FAngelscriptCacheValidationResult Result = ValidateReachability(
				Fixture, FAngelscriptCacheReadLimits{}, Budget, Visited);
			ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::SourceSnapshotMismatch,
				Result.Error));
			ASSERT_THAT(AreEqual(EAngelscriptCacheValidationStage::ManifestGraph,
				Result.Stage));
			ASSERT_THAT(AreEqual(UINT64_C(185), Result.ByteOffset));
			ASSERT_THAT(IsTrue(Visited.IsEmpty()));
		}

		enum class EUnreachable : uint8
		{
			SourceRoot,
			ModuleRoot,
			ModuleInterface,
			TypeSchema,
			ModuleState,
			FunctionBody,
			DebugSidecar,
		};
		const EUnreachable Cases[] = {
			EUnreachable::SourceRoot,
			EUnreachable::ModuleRoot,
			EUnreachable::ModuleInterface,
			EUnreachable::TypeSchema,
			EUnreachable::ModuleState,
			EUnreachable::FunctionBody,
			EUnreachable::DebugSidecar,
		};
		for (const EUnreachable Case : Cases)
		{
			FReachabilityFixture Fixture;
			EAngelscriptCacheRecordKind UnexpectedKind =
				EAngelscriptCacheRecordKind::SourceIndex;
			switch (Case)
			{
			case EUnreachable::SourceRoot:
				UnexpectedKind = EAngelscriptCacheRecordKind::SourceIndex;
				break;
			case EUnreachable::ModuleRoot:
				UnexpectedKind = EAngelscriptCacheRecordKind::ModuleSnapshot;
				break;
			case EUnreachable::ModuleInterface:
				UnexpectedKind = EAngelscriptCacheRecordKind::ModuleInterface;
				break;
			case EUnreachable::TypeSchema:
				UnexpectedKind = EAngelscriptCacheRecordKind::TypeSchema;
				break;
			case EUnreachable::ModuleState:
				UnexpectedKind = EAngelscriptCacheRecordKind::ModuleState;
				break;
			case EUnreachable::FunctionBody:
				UnexpectedKind = EAngelscriptCacheRecordKind::FunctionBody;
				break;
			case EUnreachable::DebugSidecar:
				UnexpectedKind = EAngelscriptCacheRecordKind::DebugSidecar;
				break;
			}
			const uint64 UnexpectedOffset = 1405 + static_cast<uint64>(Case) * 122;
			const FAngelscriptCacheRecordId Unexpected{
				UnexpectedKind,
				OrdinalHash(90 + static_cast<uint32>(Case), 0x72)};
			Fixture.Index.Add({Unexpected, UnexpectedOffset});
			Fixture.Index.Sort([](
				const FAngelscriptCacheReachabilityManifestEntryForTests& A,
				const FAngelscriptCacheReachabilityManifestEntryForTests& B)
			{
				return A.RecordId < B.RecordId;
			});
			Fixture.Nodes.AddDefaulted();
			Fixture.Nodes.Last().RecordId = Unexpected;
			if (UnexpectedKind == EAngelscriptCacheRecordKind::SourceIndex)
			{
				Fixture.Nodes.Last().EmbeddedSourceSnapshot = Fixture.ManifestSourceSnapshot;
			}
			else if (UnexpectedKind == EAngelscriptCacheRecordKind::ModuleSnapshot)
			{
				Fixture.Nodes.Last().EmbeddedModuleKey =
					FAngelscriptStableModuleKey{OrdinalHash(99, 0x19)};
			}
			FAngelscriptCacheReadBudget Budget;
			TArray<FAngelscriptCacheRecordId> Visited = {Fixture.Source};
			const FAngelscriptCacheValidationResult Result = ValidateReachability(
				Fixture, FAngelscriptCacheReadLimits{}, Budget, Visited);
			ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::UnexpectedRecord,
				Result.Error));
			ASSERT_THAT(AreEqual(EAngelscriptCacheValidationStage::ManifestGraph,
				Result.Stage));
			ASSERT_THAT(AreEqual(UnexpectedOffset, Result.ByteOffset));
			ASSERT_THAT(AreEqual(static_cast<uint8>(Unexpected.Kind),
				static_cast<uint8>(Result.RecordKind)));
			ASSERT_THAT(IsTrue(Visited.IsEmpty()));
		}
	}

	TEST_METHOD(ManifestAndPackIndexCandidatesReserveBeforeAllocation)
	{
		const TArray<uint8> PackBytes = FrozenEmptyPayloadPackBytes();
		const uint64 PackIndexBytes =
			static_cast<uint64>(sizeof(FAngelscriptCachePackIndexEntry));
		FAngelscriptCacheReadLimits ExactPackLimits;
		ExactPackLimits.MaxTotalDecodedBytes = PackIndexBytes;
		ExactPackLimits.MaxResidentDecodedBytes = PackIndexBytes;
		{
			FAngelscriptCacheReadBudget Budget;
			TArray<FAngelscriptCachePackIndexEntry> Index;
			const FAngelscriptCacheValidationResult Result =
				ValidateAngelscriptCachePack(PackBytes, DirectHash(PackBytes),
					ExactPackLimits, Budget, Index);
			ASSERT_THAT(IsTrue(Result.IsSuccess(),
				TEXT("An exact standalone Pack-index allocation budget succeeds")));
			ASSERT_THAT(AreEqual(1, Index.Num()));
			ASSERT_THAT(AreEqual(PackIndexBytes, Budget.GetDecodedBytes(),
				TEXT("The returned Pack index is charged exactly once")));
			ASSERT_THAT(AreEqual(PackIndexBytes, Budget.GetResidentDecodedBytes(),
				TEXT("A successful standalone Pack index is retained output")));
			ASSERT_THAT(AreEqual(UINT64_C(0),
				Budget.GetTemporaryResidentDecodedBytes()));
			ASSERT_THAT(AreEqual(PackIndexBytes,
				Budget.GetPeakLiveResidentDecodedBytes()));
		}
		{
			FAngelscriptCacheReadLimits OneShort = ExactPackLimits;
			OneShort.MaxTotalDecodedBytes = PackIndexBytes - 1;
			OneShort.MaxResidentDecodedBytes = PackIndexBytes - 1;
			FAngelscriptCacheReadBudget Budget;
			TArray<FAngelscriptCachePackIndexEntry> Index = {
				FAngelscriptCachePackIndexEntry{}};
			const FAngelscriptCacheValidationResult Result =
				ValidateAngelscriptCachePack(PackBytes, DirectHash(PackBytes),
					OneShort, Budget, Index);
			ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::BudgetExceeded,
				Result.Error,
				TEXT("One byte short rejects the Pack index before allocation")));
			ASSERT_THAT(AreEqual(EAngelscriptCacheValidationStage::PackDecode,
				Result.Stage));
			ASSERT_THAT(AreEqual(UINT64_C(20), Result.ByteOffset));
			ASSERT_THAT(IsTrue(Index.IsEmpty()));
			ASSERT_THAT(AreEqual(UINT64_C(0), Budget.GetDecodedBytes(),
				TEXT("A failed reservation is atomic")));
			ASSERT_THAT(AreEqual(UINT64_C(0),
				Budget.GetTemporaryResidentDecodedBytes()));
		}

		FAngelscriptEncodedCacheGenerationManifest Manifest;
		ASSERT_THAT(IsTrue(EncodeAngelscriptCacheGenerationManifest(
			MakeMinimumManifestValue(), Manifest).IsSuccess()));
		const uint64 ManifestRetainedBytes =
			static_cast<uint64>(sizeof(FAngelscriptValidatedGeneration))
			+ static_cast<uint64>(sizeof(FAngelscriptCacheRecordIndexEntry))
			+ static_cast<uint64>(sizeof(FAngelscriptDecodedCacheRecordHandle));
		const uint64 SortedPackIdBytes =
			static_cast<uint64>(sizeof(FAngelscriptHash256));
		const uint64 DistinctPackIdBytes =
			static_cast<uint64>(sizeof(FAngelscriptHash256));
		const uint64 ExactManifestCandidateBytes =
			ManifestRetainedBytes + SortedPackIdBytes + DistinctPackIdBytes;
		FAngelscriptCacheReadLimits ExactManifestLimits;
		ExactManifestLimits.MaxTotalDecodedBytes = ExactManifestCandidateBytes;
		ExactManifestLimits.MaxResidentDecodedBytes = ExactManifestCandidateBytes;
		{
			FCountingPackSource Packs;
			FAngelscriptCacheReadBudget Budget;
			TOptional<FAngelscriptValidatedGeneration> Out;
			const FAngelscriptCacheValidationResult Result = ValidateGeneration(
				Manifest.CompleteBytes, FAngelscriptHash256{}, Packs,
				ExactManifestLimits, Budget, Out);
			ASSERT_THAT(AreEqual(
				EAngelscriptCacheValidationError::GenerationIdMismatch, Result.Error,
				TEXT("The exact manifest candidate budget reaches identity validation")));
			ASSERT_THAT(AreEqual(EAngelscriptCacheValidationStage::ManifestDecode,
				Result.Stage));
			ASSERT_THAT(IsFalse(Out.IsSet()));
			ASSERT_THAT(AreEqual(0, Packs.GetLookupOrOpenCount()));
			ASSERT_THAT(AreEqual(ExactManifestCandidateBytes,
				Budget.GetDecodedBytes(),
				TEXT("Manifest retained and sorted/distinct PackId arrays are all charged")));
			ASSERT_THAT(AreEqual(UINT64_C(0), Budget.GetResidentDecodedBytes()));
			ASSERT_THAT(AreEqual(UINT64_C(0),
				Budget.GetTemporaryResidentDecodedBytes(),
				TEXT("Identity failure releases every manifest candidate reservation")));
			ASSERT_THAT(AreEqual(ExactManifestCandidateBytes,
				Budget.GetPeakLiveResidentDecodedBytes()));
		}
		{
			FAngelscriptCacheReadLimits OneShort = ExactManifestLimits;
			OneShort.MaxTotalDecodedBytes = ExactManifestCandidateBytes - 1;
			OneShort.MaxResidentDecodedBytes = ExactManifestCandidateBytes - 1;
			FCountingPackSource Packs;
			FAngelscriptCacheReadBudget Budget;
			TOptional<FAngelscriptValidatedGeneration> Out;
			const FAngelscriptCacheValidationResult Result = ValidateGeneration(
				Manifest.CompleteBytes, Manifest.ComputedGenerationId, Packs,
				OneShort, Budget, Out);
			ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::BudgetExceeded,
				Result.Error,
				TEXT("One byte short rejects the distinct PackId array before allocation")));
			ASSERT_THAT(AreEqual(EAngelscriptCacheValidationStage::ManifestDecode,
				Result.Stage));
			ASSERT_THAT(IsFalse(Out.IsSet()));
			ASSERT_THAT(AreEqual(0, Packs.GetLookupOrOpenCount(),
				TEXT("Manifest allocation failure precedes Pack source access")));
			ASSERT_THAT(AreEqual(
				ManifestRetainedBytes + SortedPackIdBytes,
				Budget.GetDecodedBytes(),
				TEXT("The failed distinct-ID reservation does not consume decoded bytes")));
			ASSERT_THAT(AreEqual(UINT64_C(0), Budget.GetResidentDecodedBytes()));
			ASSERT_THAT(AreEqual(UINT64_C(0),
				Budget.GetTemporaryResidentDecodedBytes()));
		}
	}

	TEST_METHOD(GenerationUsesOneCumulativeBudgetAcrossManifestPackRecordAndReachability)
	{
		const FZeroModuleGenerationFixture Fixture = MakeZeroModuleGenerationFixture();
		uint64 ExactStored = 0;
		uint64 ExactDecompressed = 0;
		uint64 ExactDecoded = 0;
		uint64 ExactResident = 0;
		uint64 ExactPeakLive = 0;
		uint64 ExactReferences = 0;
		{
			FCountingPackSource Packs;
			Packs.Add(Fixture.Pack.PackId, Fixture.Pack.Bytes);
			FAngelscriptCacheReadBudget Budget;
			TOptional<FAngelscriptValidatedGeneration> Out;
			const FAngelscriptCacheValidationResult Result = ValidateGeneration(
				Fixture.Manifest.CompleteBytes, Fixture.Manifest.ComputedGenerationId,
				Packs, FAngelscriptCacheReadLimits{}, Budget, Out);
			ASSERT_THAT(IsTrue(Result.IsSuccess()));
			ASSERT_THAT(IsTrue(Out.IsSet()));
			ASSERT_THAT(AreEqual(1, Packs.GetLookupOrOpenCount()));
			ExactStored = Budget.GetStoredBytes();
			ExactDecompressed = Budget.GetDecompressedBytes();
			ExactDecoded = Budget.GetDecodedBytes();
			ExactResident = Budget.GetResidentDecodedBytes();
			ExactPeakLive = Budget.GetPeakLiveResidentDecodedBytes();
			ExactReferences = Budget.GetReferencesAndRelocations();
			ASSERT_THAT(IsTrue(ExactStored > 0));
			ASSERT_THAT(AreEqual(ExactStored, ExactDecompressed,
				TEXT("ForceNone charges the same selected raw byte count to both counters")));
			ASSERT_THAT(IsTrue(ExactDecoded >= ExactDecompressed));
			ASSERT_THAT(IsTrue(ExactResident > 0));
			ASSERT_THAT(AreEqual(UINT64_C(0),
				Budget.GetTemporaryResidentDecodedBytes(),
				TEXT("A successful generation promotes or releases every temporary byte")));
			ASSERT_THAT(IsTrue(ExactPeakLive >= ExactResident));
			ASSERT_THAT(IsTrue(ExactReferences > 0));
		}

		FAngelscriptCacheReadLimits ExactLimits;
		ExactLimits.MaxTotalStoredBytes = ExactStored;
		ExactLimits.MaxTotalDecompressedBytes = ExactDecompressed;
		ExactLimits.MaxTotalDecodedBytes = ExactDecoded;
		ExactLimits.MaxResidentDecodedBytes = ExactPeakLive;
		ExactLimits.MaxReferencesAndRelocations = ExactReferences;
		{
			FCountingPackSource Packs;
			Packs.Add(Fixture.Pack.PackId, Fixture.Pack.Bytes);
			FAngelscriptCacheReadBudget Budget;
			TOptional<FAngelscriptValidatedGeneration> Out;
			ASSERT_THAT(IsTrue(ValidateGeneration(Fixture.Manifest.CompleteBytes,
				Fixture.Manifest.ComputedGenerationId, Packs, ExactLimits, Budget, Out).IsSuccess(),
				TEXT("The exact cumulative five-counter boundary succeeds")));
			ASSERT_THAT(IsTrue(Out.IsSet()));
			ASSERT_THAT(AreEqual(ExactStored, Budget.GetStoredBytes()));
			ASSERT_THAT(AreEqual(ExactDecompressed, Budget.GetDecompressedBytes()));
			ASSERT_THAT(AreEqual(ExactDecoded, Budget.GetDecodedBytes()));
			ASSERT_THAT(AreEqual(ExactResident, Budget.GetResidentDecodedBytes()));
			ASSERT_THAT(AreEqual(UINT64_C(0),
				Budget.GetTemporaryResidentDecodedBytes()));
			ASSERT_THAT(AreEqual(ExactPeakLive, Budget.GetPeakLiveResidentDecodedBytes()));
			ASSERT_THAT(AreEqual(ExactReferences, Budget.GetReferencesAndRelocations()));
		}

		auto ExpectOneShort = [&](const FAngelscriptCacheReadLimits& Limits,
			const TCHAR* Message)
		{
			FCountingPackSource Packs;
			Packs.Add(Fixture.Pack.PackId, Fixture.Pack.Bytes);
			FAngelscriptCacheReadBudget Budget;
			TOptional<FAngelscriptValidatedGeneration> Out;
			const FAngelscriptCacheValidationResult Result = ValidateGeneration(
				Fixture.Manifest.CompleteBytes, Fixture.Manifest.ComputedGenerationId,
				Packs, Limits, Budget, Out);
			ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::BudgetExceeded,
				Result.Error, Message));
			ASSERT_THAT(AreEqual(EAngelscriptCacheValidationClass::ArithmeticOrBudget,
				Result.Class));
			ASSERT_THAT(IsFalse(Out.IsSet(),
				TEXT("Every cumulative-budget failure remains publication-atomic")));
			ASSERT_THAT(AreEqual(UINT64_C(0),
				Budget.GetTemporaryResidentDecodedBytes(),
				TEXT("Every failed generation releases temporary candidate ownership")));
			ASSERT_THAT(IsTrue(Packs.GetLookupOrOpenCount() <= 1,
				TEXT("A failed candidate never reopens its one referenced pack")));
		};
		FAngelscriptCacheReadLimits StoredShort = ExactLimits;
		StoredShort.MaxTotalStoredBytes = ExactStored - 1;
		ExpectOneShort(StoredShort, TEXT("StoredBytes one-short fails"));
		FAngelscriptCacheReadLimits DecompressedShort = ExactLimits;
		DecompressedShort.MaxTotalDecompressedBytes = ExactDecompressed - 1;
		ExpectOneShort(DecompressedShort, TEXT("DecompressedBytes one-short fails"));
		FAngelscriptCacheReadLimits DecodedShort = ExactLimits;
		DecodedShort.MaxTotalDecodedBytes = ExactDecoded - 1;
		ExpectOneShort(DecodedShort, TEXT("DecodedBytes one-short fails"));
		FAngelscriptCacheReadLimits ResidentShort = ExactLimits;
		ResidentShort.MaxResidentDecodedBytes = ExactPeakLive - 1;
		ExpectOneShort(ResidentShort, TEXT("ResidentDecodedBytes one-short fails"));
		FAngelscriptCacheReadLimits ReferencesShort = ExactLimits;
		ReferencesShort.MaxReferencesAndRelocations = ExactReferences - 1;
		ExpectOneShort(ReferencesShort, TEXT("ReferencesAndRelocations one-short fails"));

		constexpr uint64 PreexistingRetainedBytes = 37;
		FAngelscriptCacheReadLimits CombinedExact = ExactLimits;
		CombinedExact.MaxTotalDecodedBytes = ExactDecoded + PreexistingRetainedBytes;
		CombinedExact.MaxResidentDecodedBytes = ExactPeakLive + PreexistingRetainedBytes;
		{
			FCountingPackSource Packs;
			Packs.Add(Fixture.Pack.PackId, Fixture.Pack.Bytes);
			FAngelscriptCacheReadBudget Budget;
			ASSERT_THAT(IsTrue(Budget.TryConsumeRetainedDecoded(
				PreexistingRetainedBytes, CombinedExact)));
			TOptional<FAngelscriptValidatedGeneration> Out;
			ASSERT_THAT(IsTrue(ValidateGeneration(Fixture.Manifest.CompleteBytes,
				Fixture.Manifest.ComputedGenerationId, Packs,
				CombinedExact, Budget, Out).IsSuccess(),
				TEXT("Exact retained-plus-temporary combined-live boundary succeeds")));
			ASSERT_THAT(IsTrue(Out.IsSet()));
			ASSERT_THAT(AreEqual(
				PreexistingRetainedBytes + ExactDecoded, Budget.GetDecodedBytes()));
			ASSERT_THAT(AreEqual(
				PreexistingRetainedBytes + ExactResident,
				Budget.GetResidentDecodedBytes()));
			ASSERT_THAT(AreEqual(UINT64_C(0),
				Budget.GetTemporaryResidentDecodedBytes()));
			ASSERT_THAT(AreEqual(
				PreexistingRetainedBytes + ExactPeakLive,
				Budget.GetPeakLiveResidentDecodedBytes(),
				TEXT("Peak is one unified retained-plus-temporary live value")));
		}

		FAngelscriptCacheReadLimits CombinedOneShort = CombinedExact;
		CombinedOneShort.MaxResidentDecodedBytes =
			PreexistingRetainedBytes + ExactPeakLive - 1;
		{
			FCountingPackSource Packs;
			Packs.Add(Fixture.Pack.PackId, Fixture.Pack.Bytes);
			FAngelscriptCacheReadBudget Budget;
			ASSERT_THAT(IsTrue(Budget.TryConsumeRetainedDecoded(
				PreexistingRetainedBytes, CombinedOneShort)));
			TOptional<FAngelscriptValidatedGeneration> Out;
			const FAngelscriptCacheValidationResult Result = ValidateGeneration(
				Fixture.Manifest.CompleteBytes, Fixture.Manifest.ComputedGenerationId,
				Packs, CombinedOneShort, Budget, Out);
			ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::BudgetExceeded,
				Result.Error));
			ASSERT_THAT(IsFalse(Out.IsSet()));
			ASSERT_THAT(AreEqual(PreexistingRetainedBytes,
				Budget.GetResidentDecodedBytes(),
				TEXT("Failed candidate never promotes temporary ownership")));
			ASSERT_THAT(AreEqual(UINT64_C(0),
				Budget.GetTemporaryResidentDecodedBytes(),
				TEXT("Failed candidate releases temporary ownership")));
			ASSERT_THAT(IsTrue(Budget.GetDecodedBytes() >= PreexistingRetainedBytes,
				TEXT("Failure never refunds monotonic TotalDecoded")));
			ASSERT_THAT(IsTrue(Budget.GetPeakLiveResidentDecodedBytes()
				<= CombinedOneShort.MaxResidentDecodedBytes));
		}
	}

	TEST_METHOD(PackSourceCallsAreOncePerAttemptAndEveryFailureClearsOwnedOutputs)
	{
		const FZeroModuleGenerationFixture Fixture = MakeZeroModuleGenerationFixture();
		FCountingPackSource Packs;
		Packs.Add(Fixture.Pack.PackId, Fixture.Pack.Bytes);
		for (int32 Attempt = 1; Attempt <= 2; ++Attempt)
		{
			FAngelscriptCacheReadBudget Budget;
			TOptional<FAngelscriptValidatedGeneration> Out;
			ASSERT_THAT(IsTrue(ValidateGeneration(Fixture.Manifest.CompleteBytes,
				Fixture.Manifest.ComputedGenerationId, Packs,
				FAngelscriptCacheReadLimits{}, Budget, Out).IsSuccess()));
			ASSERT_THAT(IsTrue(Out.IsSet()));
			ASSERT_THAT(AreEqual(Attempt, Packs.GetLookupOrOpenCount()));
			ASSERT_THAT(AreEqual(Attempt, Packs.GetRequestedPackIds().Num()));
			ASSERT_THAT(IsTrue(Packs.GetRequestedPackIds()[Attempt - 1]
				== Fixture.Pack.PackId));
		}

		{
			const FAngelscriptCachePackIndexEntry& PackIndex = Fixture.Pack.Index[0];
			const FAngelscriptCacheRecordIndexEntry ManifestEntry{
				PackIndex.RecordId, LocationFromPack(Fixture.Pack, PackIndex)};
			FAngelscriptUnrealZlibCacheStorageCodec Codec;
			FAngelscriptCacheReadBudget Budget;
			TOptional<FAngelscriptDecodedCacheRecordHandle> OutRecord;
			ASSERT_THAT(IsTrue(ReadAngelscriptCacheRecordFromPack(
				Fixture.Pack.Bytes, Fixture.Pack.PackId, ManifestEntry,
				FAngelscriptCacheReadLimits{}, Budget, Codec, OutRecord).IsSuccess()));
			ASSERT_THAT(IsTrue(OutRecord.IsSet(),
				TEXT("The record failure fixture begins with a successful owned handle")));
			const FReadBudgetSnapshot SuccessfulBudget = CaptureBudget(Budget);
			TArray<uint8> BadMagicPack = Fixture.Pack.Bytes;
			BadMagicPack[0] = static_cast<uint8>('X');
			const FAngelscriptCacheValidationResult Result =
				ReadAngelscriptCacheRecordFromPack(
					BadMagicPack, DirectHash(BadMagicPack), ManifestEntry,
					FAngelscriptCacheReadLimits{}, Budget, Codec, OutRecord);
			ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::BadMagic,
				Result.Error));
			ASSERT_THAT(IsFalse(OutRecord.IsSet(),
				TEXT("Record failure clears a previously successful owned handle")));
			ASSERT_THAT(IsTrue(ExpectBudgetUnchanged(
				*TestRunner, SuccessfulBudget, Budget,
				TEXT("Header failure does not mutate the caller's cumulative Budget"))));
		}

		{
			FCountingPackSource GenerationPacks;
			GenerationPacks.Add(Fixture.Pack.PackId, Fixture.Pack.Bytes);
			FAngelscriptCacheReadBudget Budget;
			TOptional<FAngelscriptValidatedGeneration> OutGeneration;
			ASSERT_THAT(IsTrue(ValidateGeneration(
				Fixture.Manifest.CompleteBytes, Fixture.Manifest.ComputedGenerationId,
				GenerationPacks, FAngelscriptCacheReadLimits{},
				Budget, OutGeneration).IsSuccess()));
			ASSERT_THAT(IsTrue(OutGeneration.IsSet(),
				TEXT("The generation failure fixture begins with a successful owned value")));
			ASSERT_THAT(AreEqual(1, GenerationPacks.GetLookupOrOpenCount()));
			const FReadBudgetSnapshot SuccessfulBudget = CaptureBudget(Budget);
			TArray<uint8> BadMagicManifest = Fixture.Manifest.CompleteBytes;
			BadMagicManifest[0] = static_cast<uint8>('X');
			const FAngelscriptCacheValidationResult Result = ValidateGeneration(
				BadMagicManifest, DirectHash(BadMagicManifest), GenerationPacks,
				FAngelscriptCacheReadLimits{}, Budget, OutGeneration);
			ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::BadMagic,
				Result.Error));
			ASSERT_THAT(IsFalse(OutGeneration.IsSet(),
				TEXT("Generation failure clears a previously successful owned value")));
			ASSERT_THAT(IsTrue(ExpectBudgetUnchanged(
				*TestRunner, SuccessfulBudget, Budget,
				TEXT("Manifest-header failure does not mutate the cumulative Budget"))));
			ASSERT_THAT(AreEqual(1, GenerationPacks.GetLookupOrOpenCount(),
				TEXT("Manifest-header failure performs no new pack lookup/open")));
			ASSERT_THAT(AreEqual(1, GenerationPacks.GetRequestedPackIds().Num()));
			ASSERT_THAT(IsTrue(GenerationPacks.GetRequestedPackIds()[0]
				== Fixture.Pack.PackId));
		}

		FAngelscriptPreparedRecord InvalidRecord;
		InvalidRecord.RecordId = FAngelscriptCacheRecordId{
			EAngelscriptCacheRecordKind::FunctionBody, OrdinalHash(77, 0x77)};
		InvalidRecord.CanonicalPayload = {0x01, 0x02};
		FAngelscriptCachePackPolicy Policy;
		Policy.CompressionPolicy = EAngelscriptCachePackCompressionPolicy::ForceNoneForTest;
		FAngelscriptUnrealZlibCacheStorageCodec Codec;
		TArray<FAngelscriptEncodedPack> PackOutput;
		PackOutput.Add(Fixture.Pack);
		const FAngelscriptCacheValidationResult PackResult = BuildAngelscriptCachePacks(
			MakeArrayView(&InvalidRecord, 1), Policy, Codec, PackOutput);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::RecordIdMismatch,
			PackResult.Error));
		ASSERT_THAT(IsTrue(PackOutput.IsEmpty(),
			TEXT("A failed pack build clears every prior output pack")));

		FAngelscriptCacheGenerationManifest InvalidManifest = MakeMinimumManifestValue();
		InvalidManifest.Profile.Hash = OrdinalHash(88, 0x78);
		FAngelscriptEncodedCacheGenerationManifest ManifestOutput;
		ManifestOutput.CompleteBytes = {0xde, 0xad};
		ManifestOutput.ComputedGenerationId = OrdinalHash(89, 0x79);
		const FAngelscriptCacheValidationResult ManifestResult =
			EncodeAngelscriptCacheGenerationManifest(InvalidManifest, ManifestOutput);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::DerivedHashMismatch,
			ManifestResult.Error));
		ASSERT_THAT(IsTrue(ManifestOutput.CompleteBytes.IsEmpty()));
		ASSERT_THAT(IsTrue(ManifestOutput.ComputedGenerationId.IsZero()));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
