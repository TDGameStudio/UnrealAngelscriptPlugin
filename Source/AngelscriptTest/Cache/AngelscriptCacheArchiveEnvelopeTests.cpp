#include "Cache/AngelscriptCacheArchive.h"

#include "CQTest.h"

#include <type_traits>

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheArchiveEnvelopeTests,
	"Angelscript.TestModule.Cache.Archive.Envelope",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static constexpr int32 SchemaOffset = 8;
	static constexpr int32 KindOffset = 12;
	static constexpr int32 ReservedOffset = 13;
	static constexpr int32 PayloadLengthOffset = 16;
	static constexpr int32 ContentHashOffset = 24;
	static constexpr int32 PayloadOffset = 56;

	static TArray<uint8> MakeFunctionBodyPayload()
	{
		return {0x10, 0x00, 0xff, 0x7e};
	}

	static FString BytesToHexString(const TConstArrayView<uint8> Bytes)
	{
		return BytesToHexLower(Bytes.GetData(), Bytes.Num());
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

	static TArray<uint8> MakeValidEnvelope()
	{
		TArray<uint8> Bytes;
		const FAngelscriptCacheValidationResult Result =
			FAngelscriptCacheRecordArchive::SerializeRecordEnvelope(
				EAngelscriptCacheRecordKind::FunctionBody,
				MakeFunctionBodyPayload(),
				Bytes);
		check(Result.IsSuccess());
		return Bytes;
	}

	static uint64 CalculatePayloadReserveBytes(const int32 RequestedCount)
	{
		if (RequestedCount <= 0)
		{
			return 0;
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
		return static_cast<uint64>(ReservedCapacity) * sizeof(uint8);
	}

	static FAngelscriptCacheRecordEnvelope MakeSentinelEnvelope()
	{
		FAngelscriptCacheRecordEnvelope Envelope;
		Envelope.RecordId.Kind = EAngelscriptCacheRecordKind::FunctionBody;
		FBlake3Hash::ByteArray HashBytes{};
		HashBytes[31] = 0x7f;
		Envelope.RecordId.ContentHash.Value = FBlake3Hash(HashBytes);
		Envelope.CanonicalPayload = {0xde, 0xad};
		return Envelope;
	}

	static FAngelscriptCacheRecordId MakeSentinelRecordId()
	{
		FBlake3Hash::ByteArray HashBytes{};
		HashBytes[31] = 0x7f;
		return FAngelscriptCacheRecordId{
			EAngelscriptCacheRecordKind::FunctionBody,
			FAngelscriptHash256{FBlake3Hash(HashBytes)}};
	}

	static bool IsResetRecordId(const FAngelscriptCacheRecordId& RecordId)
	{
		return static_cast<uint8>(RecordId.Kind) == 0 && RecordId.ContentHash.IsZero();
	}

	static TConstArrayView<uint8> MakeIntrusiveUnsetArrayView()
	{
		using FView = TConstArrayView<uint8>;
		using FOptionalView = TOptional<FView>;
		static_assert(sizeof(FOptionalView) == sizeof(FView));
		static_assert(alignof(FOptionalView) == alignof(FView));
		static_assert(std::is_trivially_copyable_v<FOptionalView>);
		static_assert(std::is_trivially_copyable_v<FView>);

		const FOptionalView UnsetOptional;
		FView UnsetView;
		FMemory::Memcpy(&UnsetView, &UnsetOptional, sizeof(UnsetView));
		return UnsetView;
	}

	static bool IsResetEnvelope(const FAngelscriptCacheRecordEnvelope& Envelope)
	{
		return static_cast<uint8>(Envelope.RecordId.Kind) == 0
			&& Envelope.RecordId.ContentHash.IsZero()
			&& Envelope.CanonicalPayload.IsEmpty();
	}

	static FAngelscriptCacheValidationResult Deserialize(
		const TConstArrayView<uint8> Bytes,
		FAngelscriptCacheRecordEnvelope& OutEnvelope,
		const uint64 MaxPayloadBytes = FAngelscriptCacheReadLimits::DefaultMaxCanonicalRecordPayloadBytes)
	{
		FAngelscriptCacheReadLimits Limits;
		Limits.MaxCanonicalRecordPayloadBytes = MaxPayloadBytes;
		return FAngelscriptCacheRecordArchive::DeserializeRecordEnvelope(Bytes, Limits, OutEnvelope);
	}

	static FAngelscriptCacheValidationResult DeserializeWithBudget(
		const TConstArrayView<uint8> Bytes,
		const FAngelscriptCacheReadLimits& Limits,
		FAngelscriptCacheReadBudget& Budget,
		FAngelscriptCacheRecordEnvelope& OutEnvelope)
	{
		return FAngelscriptCacheRecordArchive::DeserializeRecordEnvelope(
			Bytes, Limits, Budget, OutEnvelope);
	}

	static bool ExpectFailureAndReset(
		FAutomationTestBase& Test,
		const TConstArrayView<uint8> Bytes,
		const EAngelscriptCacheValidationError ExpectedError,
		const TCHAR* Message,
		const uint64 MaxPayloadBytes = FAngelscriptCacheReadLimits::DefaultMaxCanonicalRecordPayloadBytes)
	{
		FNoDiscardAsserter LocalAssert(Test);
		FAngelscriptCacheRecordEnvelope Envelope = MakeSentinelEnvelope();
		const FAngelscriptCacheValidationResult Result = Deserialize(Bytes, Envelope, MaxPayloadBytes);
		bool bPassed = LocalAssert.AreEqual(ExpectedError, Result.Error, Message);
		bPassed &= LocalAssert.IsTrue(IsResetEnvelope(Envelope),
			TEXT("A failed envelope read must clear every output field"));
		return bPassed;
	}

public:
	TEST_METHOD(StableFormatConstantsAndEnumsAreExplicit)
	{
		ASSERT_THAT(AreEqual(2u, FAngelscriptCacheRecordArchive::ArchiveSchemaVersion,
			TEXT("The minimal record archive schema is frozen at version 2")));
		ASSERT_THAT(AreEqual(56u, FAngelscriptCacheRecordArchive::EnvelopeHeaderSize,
			TEXT("The fixed envelope header must remain 56 bytes")));
		ASSERT_THAT(AreEqual(64u * 1024u * 1024u,
			FAngelscriptCacheReadLimits::DefaultMaxCanonicalRecordPayloadBytes,
			TEXT("The default single-record payload budget must be explicit")));
		ASSERT_THAT(AreEqual(0, static_cast<int32>(EAngelscriptCacheCodec::None),
			TEXT("None codec wire value is stable")));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(EAngelscriptCacheCodec::Zlib),
			TEXT("Zlib codec wire value is stable")));
		ASSERT_THAT(AreEqual(1, static_cast<int32>(EAngelscriptCacheRecordKind::SourceIndex),
			TEXT("SourceIndex wire value is stable")));
		ASSERT_THAT(AreEqual(2, static_cast<int32>(EAngelscriptCacheRecordKind::ModuleInterface),
			TEXT("ModuleInterface wire value is stable")));
		ASSERT_THAT(AreEqual(3, static_cast<int32>(EAngelscriptCacheRecordKind::TypeSchema),
			TEXT("TypeSchema wire value is stable")));
		ASSERT_THAT(AreEqual(4, static_cast<int32>(EAngelscriptCacheRecordKind::ModuleState),
			TEXT("ModuleState wire value is stable")));
		ASSERT_THAT(AreEqual(5, static_cast<int32>(EAngelscriptCacheRecordKind::FunctionBody),
			TEXT("FunctionBody wire value is stable")));
		ASSERT_THAT(AreEqual(6, static_cast<int32>(EAngelscriptCacheRecordKind::DebugSidecar),
			TEXT("DebugSidecar wire value is stable")));
		ASSERT_THAT(AreEqual(7, static_cast<int32>(EAngelscriptCacheRecordKind::ModuleSnapshot),
			TEXT("ModuleSnapshot wire value is stable")));
	}

	TEST_METHOD(BudgetOverloadOwnsAndChargesExactPayloadCapacity)
	{
		using FBudgetDeserializer = FAngelscriptCacheValidationResult(*)(
			TConstArrayView<uint8>, const FAngelscriptCacheReadLimits&,
			FAngelscriptCacheReadBudget&, FAngelscriptCacheRecordEnvelope&);
		static_assert(std::is_same_v<decltype(static_cast<FBudgetDeserializer>(
			&FAngelscriptCacheRecordArchive::DeserializeRecordEnvelope)), FBudgetDeserializer>);

		const TArray<uint8> Bytes = MakeValidEnvelope();
		const TArray<uint8> Payload = MakeFunctionBodyPayload();
		const uint64 ExpectedCapacity = CalculatePayloadReserveBytes(Payload.Num());
		ASSERT_THAT(IsTrue(ExpectedCapacity > 0));

		FAngelscriptCacheReadLimits Limits;
		Limits.MaxTotalDecodedBytes = ExpectedCapacity;
		Limits.MaxResidentDecodedBytes = ExpectedCapacity;
		FAngelscriptCacheReadBudget Budget;
		FAngelscriptCacheRecordEnvelope Envelope;
		const FAngelscriptCacheValidationResult Result =
			DeserializeWithBudget(Bytes, Limits, Budget, Envelope);

		ASSERT_THAT(IsTrue(Result.IsSuccess()));
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationStage::None, Result.Stage));
		ASSERT_THAT(AreEqual(ExpectedCapacity, Budget.GetDecodedBytes()));
		ASSERT_THAT(AreEqual(ExpectedCapacity, Budget.GetResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(0), Budget.GetTemporaryResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(ExpectedCapacity, Budget.GetPeakLiveResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(ExpectedCapacity,
			static_cast<uint64>(Envelope.CanonicalPayload.GetAllocatedSize()),
			TEXT("The Budget charge must equal the output array's actual allocator capacity")));
		ASSERT_THAT(AreEqual(BytesToHexString(Payload),
			BytesToHexString(Envelope.CanonicalPayload)));
		ASSERT_THAT(IsTrue(Envelope.CanonicalPayload.GetData() != Bytes.GetData() + PayloadOffset,
			TEXT("The decoded envelope owns its canonical payload rather than aliasing input")));

		TArray<uint8> EmptyBytes;
		ASSERT_THAT(IsTrue(FAngelscriptCacheRecordArchive::SerializeRecordEnvelope(
			EAngelscriptCacheRecordKind::FunctionBody,
			TConstArrayView<uint8>(), EmptyBytes).IsSuccess()));
		FAngelscriptCacheReadLimits EmptyLimits;
		EmptyLimits.MaxTotalDecodedBytes = 0;
		EmptyLimits.MaxResidentDecodedBytes = 0;
		FAngelscriptCacheReadBudget EmptyBudget;
		FAngelscriptCacheRecordEnvelope EmptyEnvelope = MakeSentinelEnvelope();
		ASSERT_THAT(IsTrue(DeserializeWithBudget(
			EmptyBytes, EmptyLimits, EmptyBudget, EmptyEnvelope).IsSuccess()));
		ASSERT_THAT(IsTrue(EmptyEnvelope.CanonicalPayload.IsEmpty()));
		ASSERT_THAT(AreEqual(static_cast<SIZE_T>(0),
			EmptyEnvelope.CanonicalPayload.GetAllocatedSize()));
		ASSERT_THAT(AreEqual(UINT64_C(0), EmptyBudget.GetDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(0), EmptyBudget.GetResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(0), EmptyBudget.GetPeakLiveResidentDecodedBytes()));
	}

	TEST_METHOD(BudgetOverloadRejectsBothLimitsAtomicallyAndAccumulatesAcrossRecords)
	{
		const TArray<uint8> Bytes = MakeValidEnvelope();
		const uint64 ExactCapacity = CalculatePayloadReserveBytes(MakeFunctionBodyPayload().Num());
		ASSERT_THAT(IsTrue(ExactCapacity > 0));

		for (int32 ShortDimension = 0; ShortDimension < 2; ++ShortDimension)
		{
			FAngelscriptCacheReadLimits Limits;
			Limits.MaxTotalDecodedBytes = ShortDimension == 0
				? ExactCapacity - 1 : MAX_uint64;
			Limits.MaxResidentDecodedBytes = ShortDimension == 1
				? ExactCapacity - 1 : MAX_uint64;
			FAngelscriptCacheReadBudget Budget;
			ASSERT_THAT(IsTrue(Budget.TryConsumeStored(3, Limits)));
			ASSERT_THAT(IsTrue(Budget.TryConsumeDecompressed(5, Limits)));
			ASSERT_THAT(IsTrue(Budget.TryConsumeReferencesAndRelocations(7, Limits)));
			FAngelscriptCacheRecordEnvelope Envelope;
			Envelope.RecordId = MakeSentinelRecordId();

			const FAngelscriptCacheValidationResult Result =
				DeserializeWithBudget(Bytes, Limits, Budget, Envelope);
			ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::BudgetExceeded, Result.Error));
			ASSERT_THAT(AreEqual(EAngelscriptCacheRecordKind::FunctionBody, Result.RecordKind));
			ASSERT_THAT(AreEqual(EAngelscriptCacheValidationStage::EnvelopeDecode, Result.Stage));
			ASSERT_THAT(AreEqual(static_cast<uint64>(PayloadLengthOffset), Result.ByteOffset));
			ASSERT_THAT(IsTrue(IsResetEnvelope(Envelope)));
			ASSERT_THAT(AreEqual(static_cast<SIZE_T>(0),
				Envelope.CanonicalPayload.GetAllocatedSize(),
				TEXT("Budget rejection must happen before the owned payload allocation")));
			ASSERT_THAT(AreEqual(UINT64_C(3), Budget.GetStoredBytes()));
			ASSERT_THAT(AreEqual(UINT64_C(5), Budget.GetDecompressedBytes()));
			ASSERT_THAT(AreEqual(UINT64_C(0), Budget.GetDecodedBytes()));
			ASSERT_THAT(AreEqual(UINT64_C(0), Budget.GetResidentDecodedBytes()));
			ASSERT_THAT(AreEqual(UINT64_C(0), Budget.GetTemporaryResidentDecodedBytes()));
			ASSERT_THAT(AreEqual(UINT64_C(0), Budget.GetPeakLiveResidentDecodedBytes()));
			ASSERT_THAT(AreEqual(UINT64_C(7), Budget.GetReferencesAndRelocations()));
		}

		FAngelscriptCacheReadLimits CumulativeLimits;
		CumulativeLimits.MaxTotalDecodedBytes = ExactCapacity * 2;
		CumulativeLimits.MaxResidentDecodedBytes = ExactCapacity * 2;
		FAngelscriptCacheReadBudget CumulativeBudget;
		FAngelscriptCacheRecordEnvelope First;
		FAngelscriptCacheRecordEnvelope Second;
		FAngelscriptCacheRecordEnvelope Rejected = MakeSentinelEnvelope();
		ASSERT_THAT(IsTrue(DeserializeWithBudget(
			Bytes, CumulativeLimits, CumulativeBudget, First).IsSuccess()));
		ASSERT_THAT(IsTrue(DeserializeWithBudget(
			Bytes, CumulativeLimits, CumulativeBudget, Second).IsSuccess()));
		const FAngelscriptCacheValidationResult ThirdResult = DeserializeWithBudget(
			Bytes, CumulativeLimits, CumulativeBudget, Rejected);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::BudgetExceeded, ThirdResult.Error));
		ASSERT_THAT(IsTrue(IsResetEnvelope(Rejected)));
		ASSERT_THAT(AreEqual(ExactCapacity * 2, CumulativeBudget.GetDecodedBytes()));
		ASSERT_THAT(AreEqual(ExactCapacity * 2, CumulativeBudget.GetResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(ExactCapacity * 2,
			CumulativeBudget.GetPeakLiveResidentDecodedBytes()));
		ASSERT_THAT(IsTrue(First.CanonicalPayload.GetData() != Second.CanonicalPayload.GetData(),
			TEXT("Each successful envelope decode owns one separately charged payload allocation")));
	}

	TEST_METHOD(FunctionBodyRecordIdHasFrozenSemanticGolden)
	{
		FAngelscriptCacheRecordId RecordId;
		const FAngelscriptCacheValidationResult Result =
			FAngelscriptCacheRecordArchive::TryBuildRecordId(
				EAngelscriptCacheRecordKind::FunctionBody,
				MakeFunctionBodyPayload(),
				RecordId);
		ASSERT_THAT(IsTrue(Result.IsSuccess(), TEXT("A valid FunctionBody must produce a RecordId")));
		const FString ExpectedHash = TEXT("0dd0eb1839134871fdb07ec1276b07b386114d1a8d5967b4ff44df131dde3501");
		ASSERT_THAT(AreEqual(ExpectedHash, RecordId.ContentHash.ToHexString(),
			*FString::Printf(TEXT("The semantic RecordId must match its frozen full BLAKE3-256 golden; actual=%s"),
				*RecordId.ContentHash.ToHexString())));
	}

	TEST_METHOD(RecordIdDomainsSeparateEveryKnownKindAndOrderingUsesKindFirst)
	{
		const EAngelscriptCacheRecordKind Kinds[] = {
			EAngelscriptCacheRecordKind::SourceIndex,
			EAngelscriptCacheRecordKind::ModuleInterface,
			EAngelscriptCacheRecordKind::TypeSchema,
			EAngelscriptCacheRecordKind::ModuleState,
			EAngelscriptCacheRecordKind::FunctionBody,
			EAngelscriptCacheRecordKind::DebugSidecar,
			EAngelscriptCacheRecordKind::ModuleSnapshot,
		};
		const TArray<uint8> EmptyPayload;
		const TArray<uint8> NonEmptyPayload = {0x00, 0x7f, 0x80, 0xff};
		const TConstArrayView<uint8> Payloads[] = {
			TConstArrayView<uint8>(EmptyPayload),
			TConstArrayView<uint8>(NonEmptyPayload),
		};

		for (const TConstArrayView<uint8> Payload : Payloads)
		{
			FAngelscriptCacheRecordId RecordIds[UE_ARRAY_COUNT(Kinds)];
			for (int32 KindIndex = 0;
				KindIndex < static_cast<int32>(UE_ARRAY_COUNT(Kinds));
				++KindIndex)
			{
				ASSERT_THAT(IsTrue(FAngelscriptCacheRecordArchive::TryBuildRecordId(
					Kinds[KindIndex], Payload, RecordIds[KindIndex]).IsSuccess()));
				ASSERT_THAT(AreEqual(Kinds[KindIndex], RecordIds[KindIndex].Kind));
			}
			for (int32 LeftIndex = 0;
				LeftIndex < static_cast<int32>(UE_ARRAY_COUNT(Kinds));
				++LeftIndex)
			{
				for (int32 RightIndex = LeftIndex + 1;
					RightIndex < static_cast<int32>(UE_ARRAY_COUNT(Kinds));
					++RightIndex)
				{
					ASSERT_THAT(IsFalse(
						RecordIds[LeftIndex] == RecordIds[RightIndex]));
					ASSERT_THAT(IsFalse(RecordIds[LeftIndex].ContentHash
						== RecordIds[RightIndex].ContentHash));
					ASSERT_THAT(IsTrue(
						RecordIds[LeftIndex] < RecordIds[RightIndex]));
					ASSERT_THAT(IsFalse(
						RecordIds[RightIndex] < RecordIds[LeftIndex]));
				}
			}
		}

		FAngelscriptCacheRecordId SameKindA;
		FAngelscriptCacheRecordId SameKindB;
		ASSERT_THAT(IsTrue(FAngelscriptCacheRecordArchive::TryBuildRecordId(
			EAngelscriptCacheRecordKind::FunctionBody,
			TConstArrayView<uint8>(EmptyPayload), SameKindA).IsSuccess()));
		ASSERT_THAT(IsTrue(FAngelscriptCacheRecordArchive::TryBuildRecordId(
			EAngelscriptCacheRecordKind::FunctionBody,
			TConstArrayView<uint8>(NonEmptyPayload), SameKindB).IsSuccess()));
		ASSERT_THAT(IsFalse(SameKindA == SameKindB));
		ASSERT_THAT(AreEqual(SameKindA.ContentHash < SameKindB.ContentHash,
			SameKindA < SameKindB));
		ASSERT_THAT(AreEqual(SameKindB.ContentHash < SameKindA.ContentHash,
			SameKindB < SameKindA));
	}

	TEST_METHOD(RecordIdBuilderRejectsInvalidKindsAndUnsetViews)
	{
		const TArray<uint8> Payload = MakeFunctionBodyPayload();
		FAngelscriptCacheRecordId RecordId = MakeSentinelRecordId();
		FAngelscriptCacheValidationResult Result =
			FAngelscriptCacheRecordArchive::TryBuildRecordId(
				static_cast<EAngelscriptCacheRecordKind>(0),
				Payload,
				RecordId);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::UnknownRecordKind, Result.Error,
			TEXT("Record kind zero must not produce a public RecordId")));
		ASSERT_THAT(IsTrue(IsResetRecordId(RecordId),
			TEXT("A failed zero-kind RecordId build must clear the output")));

		RecordId = MakeSentinelRecordId();
		Result = FAngelscriptCacheRecordArchive::TryBuildRecordId(
			static_cast<EAngelscriptCacheRecordKind>(0xff),
			Payload,
			RecordId);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::UnknownRecordKind, Result.Error,
			TEXT("An unknown record kind must not produce a public RecordId")));
		ASSERT_THAT(IsTrue(IsResetRecordId(RecordId),
			TEXT("A failed unknown-kind RecordId build must clear the output")));

		const TConstArrayView<uint8> UnsetPayload = MakeIntrusiveUnsetArrayView();
		ASSERT_THAT(AreEqual(-1, UnsetPayload.Num(),
			TEXT("The negative-size fixture must exercise the public intrusive-unset state")));
		RecordId = MakeSentinelRecordId();
		Result = FAngelscriptCacheRecordArchive::TryBuildRecordId(
			EAngelscriptCacheRecordKind::FunctionBody,
			UnsetPayload,
			RecordId);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::InvalidArrayView, Result.Error,
			TEXT("An intrusive-unset payload view must be rejected before hashing")));
		ASSERT_THAT(IsTrue(IsResetRecordId(RecordId),
			TEXT("An invalid-view RecordId build must clear the output")));
	}

	TEST_METHOD(EmptyPayloadHasFrozenRecordIdAndEnvelope)
	{
		const TConstArrayView<uint8> EmptyPayload;
		FAngelscriptCacheRecordId RecordId = MakeSentinelRecordId();
		const FAngelscriptCacheValidationResult RecordIdResult =
			FAngelscriptCacheRecordArchive::TryBuildRecordId(
				EAngelscriptCacheRecordKind::FunctionBody,
				EmptyPayload,
				RecordId);
		ASSERT_THAT(IsTrue(RecordIdResult.IsSuccess(), TEXT("An empty canonical payload is valid")));
		const FString ExpectedEmptyRecordId = TEXT("7717b3b344c513d829e689476b5005824b5f5c5b92447bd970d1491b490b446d");
		FNoDiscardAsserter GoldenAssert(*TestRunner);
		bool bGoldensMatch = GoldenAssert.AreEqual(ExpectedEmptyRecordId, RecordId.ContentHash.ToHexString(),
			*FString::Printf(TEXT("The empty semantic RecordId must match its full golden; actual=%s"),
				*RecordId.ContentHash.ToHexString()));

		TArray<uint8> Bytes;
		const FAngelscriptCacheValidationResult WriteResult =
			FAngelscriptCacheRecordArchive::SerializeRecordEnvelope(
				EAngelscriptCacheRecordKind::FunctionBody,
				EmptyPayload,
				Bytes);
		ASSERT_THAT(IsTrue(WriteResult.IsSuccess(), TEXT("An empty canonical payload must serialize")));
		const FString ActualEnvelopeHex = BytesToHexString(Bytes);
		const FString ExpectedEmptyEnvelopeHex = TEXT(
			"554541534356325202000000050000000000000000000000"
			"7717b3b344c513d829e689476b5005824b5f5c5b92447bd970d1491b490b446d");
		bGoldensMatch &= GoldenAssert.AreEqual(ExpectedEmptyEnvelopeHex, ActualEnvelopeHex,
			*FString::Printf(TEXT("The complete empty envelope must match its 56-byte golden; actual=%s"),
				*ActualEnvelopeHex));

		FAngelscriptCacheRecordEnvelope Envelope = MakeSentinelEnvelope();
		const FAngelscriptCacheValidationResult ReadResult = Deserialize(Bytes, Envelope);
		ASSERT_THAT(IsTrue(ReadResult.IsSuccess(), TEXT("The frozen empty envelope must deserialize")));
		ASSERT_THAT(IsTrue(Envelope.RecordId == RecordId,
			TEXT("The empty RecordId must survive its round trip")));
		ASSERT_THAT(IsTrue(Envelope.CanonicalPayload.IsEmpty(),
			TEXT("The empty canonical payload must remain empty after decoding")));

		TArray<uint8> ReserializedBytes;
		const FAngelscriptCacheValidationResult RewriteResult =
			FAngelscriptCacheRecordArchive::SerializeRecordEnvelope(
				Envelope.RecordId.Kind,
				Envelope.CanonicalPayload,
				ReserializedBytes);
		ASSERT_THAT(IsTrue(RewriteResult.IsSuccess(), TEXT("The decoded empty envelope must reserialize")));
		ASSERT_THAT(AreEqual(ActualEnvelopeHex, BytesToHexString(ReserializedBytes),
			TEXT("The empty envelope round trip must preserve all 56 physical bytes")));
		ASSERT_THAT(IsTrue(bGoldensMatch, TEXT("Both empty-payload goldens must be frozen")));
	}

	TEST_METHOD(UnsetViewsAreRejectedBySerializationAndDeserialization)
	{
		const TConstArrayView<uint8> UnsetBytes = MakeIntrusiveUnsetArrayView();
		TArray<uint8> SerializedBytes = {0xde, 0xad};
		const FAngelscriptCacheValidationResult WriteResult =
			FAngelscriptCacheRecordArchive::SerializeRecordEnvelope(
				EAngelscriptCacheRecordKind::FunctionBody,
				UnsetBytes,
				SerializedBytes);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::InvalidArrayView, WriteResult.Error,
			TEXT("Serialization must reject an intrusive-unset input before hashing")));
		ASSERT_THAT(IsTrue(SerializedBytes.IsEmpty(),
			TEXT("Invalid serialization input must clear the output byte array")));

		FAngelscriptCacheRecordEnvelope Envelope = MakeSentinelEnvelope();
		const FAngelscriptCacheValidationResult ReadResult = Deserialize(UnsetBytes, Envelope);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::InvalidArrayView, ReadResult.Error,
			TEXT("Deserialization must reject an intrusive-unset input before inspection")));
		ASSERT_THAT(IsTrue(IsResetEnvelope(Envelope),
			TEXT("Invalid deserialization input must clear the output envelope")));
	}

	TEST_METHOD(OverflowedAddressRangesAreRejectedBeforePointerInspection)
	{
		const UPTRINT NearAddressLimit = TNumericLimits<UPTRINT>::Max() - 1;
		const TConstArrayView<uint8> OverflowedView(
			reinterpret_cast<const uint8*>(NearAddressLimit), 4);

		TArray<uint8> SerializedBytes = {0xde, 0xad};
		const FAngelscriptCacheValidationResult WriteResult =
			FAngelscriptCacheRecordArchive::SerializeRecordEnvelope(
				EAngelscriptCacheRecordKind::FunctionBody,
				OverflowedView,
				SerializedBytes);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::InvalidArrayView,
			WriteResult.Error,
			TEXT("A pointer-plus-size address wrap must fail before hashing or Reset invalidation")));
		ASSERT_THAT(IsTrue(SerializedBytes.IsEmpty()));

		FAngelscriptCacheRecordEnvelope Envelope = MakeSentinelEnvelope();
		FAngelscriptCacheReadLimits Limits;
		FAngelscriptCacheReadBudget Budget;
		const FAngelscriptCacheValidationResult ReadResult =
			FAngelscriptCacheRecordArchive::DeserializeRecordEnvelope(
				OverflowedView, Limits, Budget, Envelope);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::InvalidArrayView,
			ReadResult.Error,
			TEXT("A wrapped input range must fail before any header byte is inspected")));
		ASSERT_THAT(IsTrue(IsResetEnvelope(Envelope)));
		ASSERT_THAT(AreEqual(UINT64_C(0), Budget.GetDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(0), Budget.GetPeakLiveResidentDecodedBytes()));
	}

	TEST_METHOD(AliasedInputOutputIsRejectedBeforeMutation)
	{
		TArray<uint8> SerializedBytes = MakeFunctionBodyPayload();
		const FAngelscriptCacheValidationResult WriteResult =
			FAngelscriptCacheRecordArchive::SerializeRecordEnvelope(
				EAngelscriptCacheRecordKind::FunctionBody,
				TConstArrayView<uint8>(SerializedBytes),
				SerializedBytes);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::AliasedInputOutput, WriteResult.Error,
			TEXT("Serialization must reject a payload view into its output allocation")));
		ASSERT_THAT(IsTrue(SerializedBytes.IsEmpty(),
			TEXT("Aliased serialization must clear its output without reading stale memory")));

		FAngelscriptCacheRecordEnvelope Envelope = MakeSentinelEnvelope();
		Envelope.CanonicalPayload = MakeValidEnvelope();
		FAngelscriptCacheReadLimits Limits;
		const FAngelscriptCacheValidationResult ReadResult =
			FAngelscriptCacheRecordArchive::DeserializeRecordEnvelope(
				TConstArrayView<uint8>(Envelope.CanonicalPayload),
				Limits,
				Envelope);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::AliasedInputOutput, ReadResult.Error,
			TEXT("Deserialization must reject bytes from its output payload allocation")));
		ASSERT_THAT(IsTrue(IsResetEnvelope(Envelope),
			TEXT("Aliased deserialization must clear its output after detecting overlap")));

		FAngelscriptCacheRecordEnvelope BudgetedEnvelope = MakeSentinelEnvelope();
		BudgetedEnvelope.CanonicalPayload = MakeValidEnvelope();
		FAngelscriptCacheReadBudget Budget;
		ASSERT_THAT(IsTrue(Budget.TryConsumeStored(3, Limits)));
		const FAngelscriptCacheValidationResult BudgetedReadResult =
			FAngelscriptCacheRecordArchive::DeserializeRecordEnvelope(
				TConstArrayView<uint8>(BudgetedEnvelope.CanonicalPayload),
				Limits,
				Budget,
				BudgetedEnvelope);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::AliasedInputOutput,
			BudgetedReadResult.Error));
		ASSERT_THAT(IsTrue(IsResetEnvelope(BudgetedEnvelope)));
		ASSERT_THAT(AreEqual(UINT64_C(3), Budget.GetStoredBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(0), Budget.GetDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(0), Budget.GetResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(0), Budget.GetPeakLiveResidentDecodedBytes(),
			TEXT("Alias rejection must precede every payload capacity charge")));
	}

	TEST_METHOD(AliasingCoversUnusedOutputCapacityNotOnlyLogicalElements)
	{
		TArray<uint8> SerializeOutput;
		SerializeOutput.Reserve(64);
		SerializeOutput.SetNumUninitialized(1);
		check(SerializeOutput.Max() >= 8);
		uint8* CapacityPayload = SerializeOutput.GetData() + 4;
		CapacityPayload[0] = 0x10;
		CapacityPayload[1] = 0x20;
		CapacityPayload[2] = 0x30;
		CapacityPayload[3] = 0x40;
		const FAngelscriptCacheValidationResult WriteResult =
			FAngelscriptCacheRecordArchive::SerializeRecordEnvelope(
				EAngelscriptCacheRecordKind::FunctionBody,
				TConstArrayView<uint8>(CapacityPayload, 4),
				SerializeOutput);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::AliasedInputOutput,
			WriteResult.Error,
			TEXT("An input inside unused output capacity must be rejected before Reset frees it")));
		ASSERT_THAT(IsTrue(SerializeOutput.IsEmpty()));

		const TArray<uint8> ValidEnvelope = MakeValidEnvelope();
		FAngelscriptCacheRecordEnvelope ReadOutput = MakeSentinelEnvelope();
		ReadOutput.CanonicalPayload.Reset();
		ReadOutput.CanonicalPayload.Reserve(ValidEnvelope.Num() + 8);
		ReadOutput.CanonicalPayload.SetNumUninitialized(1);
		check(ReadOutput.CanonicalPayload.Max() >= ValidEnvelope.Num() + 1);
		uint8* CapacityEnvelope = ReadOutput.CanonicalPayload.GetData() + 1;
		FMemory::Memcpy(CapacityEnvelope, ValidEnvelope.GetData(), ValidEnvelope.Num());

		FAngelscriptCacheReadLimits Limits;
		FAngelscriptCacheReadBudget Budget;
		const FAngelscriptCacheValidationResult ReadResult =
			FAngelscriptCacheRecordArchive::DeserializeRecordEnvelope(
				TConstArrayView<uint8>(CapacityEnvelope, ValidEnvelope.Num()),
				Limits,
				Budget,
				ReadOutput);
		ASSERT_THAT(AreEqual(EAngelscriptCacheValidationError::AliasedInputOutput,
			ReadResult.Error,
			TEXT("A record inside unused payload capacity must be rejected before Reset frees it")));
		ASSERT_THAT(IsTrue(IsResetEnvelope(ReadOutput)));
		ASSERT_THAT(AreEqual(UINT64_C(0), Budget.GetDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(0), Budget.GetResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(0), Budget.GetTemporaryResidentDecodedBytes()));
		ASSERT_THAT(AreEqual(UINT64_C(0), Budget.GetPeakLiveResidentDecodedBytes()));
	}

	TEST_METHOD(FunctionBodyEnvelopeRoundTripsByteExactly)
	{
		const TArray<uint8> Payload = MakeFunctionBodyPayload();
		TArray<uint8> Bytes;
		const FAngelscriptCacheValidationResult WriteResult =
			FAngelscriptCacheRecordArchive::SerializeRecordEnvelope(
				EAngelscriptCacheRecordKind::FunctionBody,
				Payload,
				Bytes);
		ASSERT_THAT(IsTrue(WriteResult.IsSuccess(), TEXT("A valid FunctionBody envelope must serialize")));

		const FString ActualEnvelopeHex = BytesToHexString(Bytes);
		const FString ExpectedEnvelopeHex = TEXT(
			"554541534356325202000000050000000400000000000000"
			"0dd0eb1839134871fdb07ec1276b07b386114d1a8d5967b4ff44df131dde3501"
			"1000ff7e");
		ASSERT_THAT(AreEqual(ExpectedEnvelopeHex, ActualEnvelopeHex,
			*FString::Printf(TEXT("The complete physical envelope must match its byte-exact golden; actual=%s"),
				*ActualEnvelopeHex)));

		FAngelscriptCacheRecordEnvelope Envelope;
		const FAngelscriptCacheValidationResult ReadResult = Deserialize(Bytes, Envelope);
		ASSERT_THAT(IsTrue(ReadResult.IsSuccess(), TEXT("The frozen FunctionBody envelope must deserialize")));
		ASSERT_THAT(AreEqual(static_cast<int32>(EAngelscriptCacheRecordKind::FunctionBody),
			static_cast<int32>(Envelope.RecordId.Kind),
			TEXT("The record kind must survive the round trip")));
		ASSERT_THAT(AreEqual(BytesToHexString(Payload), BytesToHexString(Envelope.CanonicalPayload),
			TEXT("The canonical payload must survive the round trip")));

		TArray<uint8> ReserializedBytes;
		const FAngelscriptCacheValidationResult RewriteResult =
			FAngelscriptCacheRecordArchive::SerializeRecordEnvelope(
				Envelope.RecordId.Kind,
				Envelope.CanonicalPayload,
				ReserializedBytes);
		ASSERT_THAT(IsTrue(RewriteResult.IsSuccess(), TEXT("The decoded envelope must reserialize")));
		ASSERT_THAT(AreEqual(ActualEnvelopeHex, BytesToHexString(ReserializedBytes),
			TEXT("Deserialize and reserialize must preserve every physical byte")));
	}

	TEST_METHOD(TruncatedHeaderAndPayloadAreRejected)
	{
		TArray<uint8> TruncatedHeader = MakeValidEnvelope();
		TruncatedHeader.SetNum(PayloadOffset - 1);
		ASSERT_THAT(IsTrue(ExpectFailureAndReset(*TestRunner, TruncatedHeader,
			EAngelscriptCacheValidationError::OutOfBounds,
			TEXT("A truncated fixed header must be rejected")),
			TEXT("A truncated fixed header must fail without exposing partial output")));

		TArray<uint8> TruncatedPayload = MakeValidEnvelope();
		TruncatedPayload.SetNum(TruncatedPayload.Num() - 1);
		ASSERT_THAT(IsTrue(ExpectFailureAndReset(*TestRunner, TruncatedPayload,
			EAngelscriptCacheValidationError::OutOfBounds,
			TEXT("A truncated declared payload must be rejected")),
			TEXT("A truncated declared payload must fail without exposing partial output")));
	}

	TEST_METHOD(OverflowAndBudgetAreRejectedBeforePayloadCopy)
	{
		TArray<uint8> OverflowedLength = MakeValidEnvelope();
		WriteUInt64LittleEndian(OverflowedLength, PayloadLengthOffset, UINT64_MAX);
		ASSERT_THAT(IsTrue(ExpectFailureAndReset(*TestRunner, OverflowedLength,
			EAngelscriptCacheValidationError::Overflow,
			TEXT("Header plus declared payload length overflow must be rejected before copy")),
			TEXT("An overflowing declared length must fail without exposing partial output")));

		const TArray<uint8> OverBudget = MakeValidEnvelope();
		ASSERT_THAT(IsTrue(ExpectFailureAndReset(*TestRunner, OverBudget,
			EAngelscriptCacheValidationError::BudgetExceeded,
			TEXT("A payload over the configured budget must be rejected before copy"), 3),
			TEXT("An over-budget payload must fail without exposing partial output")));
	}

	TEST_METHOD(ChecksumMismatchIsRejected)
	{
		TArray<uint8> CorruptHash = MakeValidEnvelope();
		CorruptHash[ContentHashOffset + 31] ^= 0x80;
		ASSERT_THAT(IsTrue(ExpectFailureAndReset(*TestRunner, CorruptHash,
			EAngelscriptCacheValidationError::ChecksumMismatch,
			TEXT("A declared semantic content hash mismatch must be rejected")),
			TEXT("A corrupt declared hash must fail without exposing partial output")));

		TArray<uint8> CorruptPayload = MakeValidEnvelope();
		CorruptPayload[PayloadOffset] ^= 0x01;
		ASSERT_THAT(IsTrue(ExpectFailureAndReset(*TestRunner, CorruptPayload,
			EAngelscriptCacheValidationError::ChecksumMismatch,
			TEXT("A payload mutation must be rejected by the semantic content hash")),
			TEXT("A corrupt payload must fail without exposing partial output")));
	}

	TEST_METHOD(UnknownAndZeroRecordKindsAreRejected)
	{
		TArray<uint8> ZeroKind = MakeValidEnvelope();
		ZeroKind[KindOffset] = 0;
		ASSERT_THAT(IsTrue(ExpectFailureAndReset(*TestRunner, ZeroKind,
			EAngelscriptCacheValidationError::UnknownRecordKind,
			TEXT("Record kind zero is reserved and invalid")),
			TEXT("Record kind zero must fail without exposing partial output")));

		TArray<uint8> UnknownKind = MakeValidEnvelope();
		UnknownKind[KindOffset] = 0xff;
		ASSERT_THAT(IsTrue(ExpectFailureAndReset(*TestRunner, UnknownKind,
			EAngelscriptCacheValidationError::UnknownRecordKind,
			TEXT("An unknown nonzero record kind must be rejected")),
			TEXT("An unknown record kind must fail without exposing partial output")));
	}

	TEST_METHOD(HeaderCorruptionAndTrailingDataHaveTypedErrors)
	{
		TArray<uint8> WrongMagic = MakeValidEnvelope();
		WrongMagic[0] ^= 0x20;
		ASSERT_THAT(IsTrue(ExpectFailureAndReset(*TestRunner, WrongMagic,
			EAngelscriptCacheValidationError::BadMagic,
			TEXT("A wrong record magic must have a typed error")),
			TEXT("A wrong magic must fail without exposing partial output")));

		TArray<uint8> WrongSchema = MakeValidEnvelope();
		WriteUInt32LittleEndian(WrongSchema, SchemaOffset, 3);
		ASSERT_THAT(IsTrue(ExpectFailureAndReset(*TestRunner, WrongSchema,
			EAngelscriptCacheValidationError::UnsupportedSchema,
			TEXT("An unsupported record archive schema must have a typed error")),
			TEXT("A wrong schema must fail without exposing partial output")));

		TArray<uint8> NonZeroReserved = MakeValidEnvelope();
		NonZeroReserved[ReservedOffset + 1] = 0x01;
		ASSERT_THAT(IsTrue(ExpectFailureAndReset(*TestRunner, NonZeroReserved,
			EAngelscriptCacheValidationError::NonZeroReserved,
			TEXT("Reserved envelope bytes must remain zero")),
			TEXT("Nonzero reserved bytes must fail without exposing partial output")));

		TArray<uint8> TrailingData = MakeValidEnvelope();
		TrailingData.Add(0x42);
		ASSERT_THAT(IsTrue(ExpectFailureAndReset(*TestRunner, TrailingData,
			EAngelscriptCacheValidationError::TrailingData,
			TEXT("The reader must accept exactly one envelope and reject trailing bytes")),
			TEXT("Trailing data must fail without exposing partial output")));
	}

	TEST_METHOD(RecordIdEqualityUsesTheCompleteHash)
	{
		FBlake3Hash::ByteArray FirstBytes{};
		FBlake3Hash::ByteArray SecondBytes{};
		FirstBytes[31] = 0x01;
		SecondBytes[31] = 0x02;
		const FAngelscriptCacheRecordId First{
			EAngelscriptCacheRecordKind::FunctionBody,
			FAngelscriptHash256{FBlake3Hash(FirstBytes)}};
		const FAngelscriptCacheRecordId Second{
			EAngelscriptCacheRecordKind::FunctionBody,
			FAngelscriptHash256{FBlake3Hash(SecondBytes)}};

		ASSERT_THAT(AreEqual(First.ContentHash.ToDisplayGuid(), Second.ContentHash.ToDisplayGuid(),
			TEXT("The fixture must collide in the display-only GUID")));
		ASSERT_THAT(IsFalse(First == Second,
			TEXT("RecordId equality must compare all 256 content-hash bits")));
		ASSERT_THAT(IsTrue((First < Second) != (Second < First),
			TEXT("RecordId ordering must compare the complete hash after its kind")));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
