#include "Cache/AngelscriptCacheStore.h"

#include "CQTest.h"
#include "Hash/Blake3.h"

#if WITH_ANGELSCRIPT_UNITTESTS

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheStorePointerTests,
	"Angelscript.TestModule.Cache.StorePointer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
private:
	static FAngelscriptHash256 RepeatedByteHash(const uint8 Byte)
	{
		FBlake3Hash::ByteArray Bytes{};
		FMemory::Memset(Bytes, Byte, sizeof(Bytes));
		return FAngelscriptHash256{FBlake3Hash(Bytes)};
	}

	static FString BytesToHexString(const TConstArrayView<uint8> Bytes)
	{
		return BytesToHexLower(Bytes.GetData(), Bytes.Num());
	}

	void AssertPointerInvalid(
		const FAngelscriptCacheStoreResult& Result,
		const EAngelscriptCacheStorePathCategory ExpectedCategory)
	{
		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreError::PointerInvalid, Result.Error));
		ASSERT_THAT(AreEqual(ExpectedCategory, Result.PathCategory));
	}

public:
	TEST_METHOD(EncodesAndDecodesTheExactFixedPointerWire)
	{
		const FAngelscriptCachePointerValue Value{
			EAngelscriptCachePointerKind::Current,
			RepeatedByteHash(0x2a)};
		TArray<uint8> Bytes;

		const FAngelscriptCacheStoreResult EncodeResult =
			EncodeAngelscriptCachePointer(Value, Bytes);

		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreError::None, EncodeResult.Error));
		ASSERT_THAT(AreEqual(80, Bytes.Num()));
		const uint8 ExpectedMagic[8] = {'U', 'E', 'A', 'S', 'C', 'V', '2', 'C'};
		ASSERT_THAT(IsTrue(FMemory::Memcmp(Bytes.GetData(), ExpectedMagic, 8) == 0));
		ASSERT_THAT(AreEqual(static_cast<uint8>(1), Bytes[8]));
		ASSERT_THAT(AreEqual(static_cast<uint8>(0), Bytes[9]));
		ASSERT_THAT(AreEqual(static_cast<uint8>(0), Bytes[10]));
		ASSERT_THAT(AreEqual(static_cast<uint8>(0), Bytes[11]));
		ASSERT_THAT(AreEqual(static_cast<uint8>(1), Bytes[12]));
		ASSERT_THAT(AreEqual(static_cast<uint8>(0), Bytes[13]));
		ASSERT_THAT(AreEqual(static_cast<uint8>(0), Bytes[14]));
		ASSERT_THAT(AreEqual(static_cast<uint8>(0), Bytes[15]));
		for (int32 Index = 16; Index < 48; ++Index)
		{
			ASSERT_THAT(AreEqual(static_cast<uint8>(0x2a), Bytes[Index]));
		}
		const FBlake3Hash ExpectedChecksum = FBlake3::HashBuffer(Bytes.GetData(), 48);
		ASSERT_THAT(IsTrue(FMemory::Memcmp(
			Bytes.GetData() + 48,
			ExpectedChecksum.GetBytes(),
			32) == 0));
		const FString ActualPointerHex = BytesToHexString(Bytes);
		const FString ExpectedPointerHex = TEXT(
			"55454153435632430100000001000000"
			"2a2a2a2a2a2a2a2a2a2a2a2a2a2a2a2a"
			"2a2a2a2a2a2a2a2a2a2a2a2a2a2a2a2a"
			"77c0a448f50546ad80dc25343f89e870"
			"152dc18ae990db9ac9c5fdb1cebaf5b9");
		ASSERT_THAT(AreEqual(
			ExpectedPointerHex,
			ActualPointerHex,
			*FString::Printf(
				TEXT("The complete Current pointer must match its 80-byte golden; actual=%s"),
				*ActualPointerHex)));

		FAngelscriptCachePointerValue Decoded;
		const FAngelscriptCacheStoreResult DecodeResult = DecodeAngelscriptCachePointer(
			Bytes,
			EAngelscriptCachePointerKind::Current,
			Decoded);
		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreError::None, DecodeResult.Error));
		ASSERT_THAT(AreEqual(EAngelscriptCachePointerKind::Current, Decoded.Kind));
		ASSERT_THAT(AreEqual(
			Value.GenerationId.ToHexString(),
			Decoded.GenerationId.ToHexString(),
			TEXT("Pointer round-trip must preserve the full GenerationId")));
	}

	TEST_METHOD(EachFilenameKindHasDistinctValidatedBytes)
	{
		const FAngelscriptHash256 GenerationId = RepeatedByteHash(0x4c);
		TArray<uint8> Current;
		TArray<uint8> Previous;
		TArray<uint8> Pending;
		ASSERT_THAT(IsTrue(EncodeAngelscriptCachePointer(
			{EAngelscriptCachePointerKind::Current, GenerationId}, Current).IsSuccess()));
		ASSERT_THAT(IsTrue(EncodeAngelscriptCachePointer(
			{EAngelscriptCachePointerKind::Previous, GenerationId}, Previous).IsSuccess()));
		ASSERT_THAT(IsTrue(EncodeAngelscriptCachePointer(
			{EAngelscriptCachePointerKind::PendingColdStart, GenerationId}, Pending).IsSuccess()));
		ASSERT_THAT(IsFalse(Current == Previous));
		ASSERT_THAT(IsFalse(Current == Pending));
		ASSERT_THAT(IsFalse(Previous == Pending));
		const FString PreviousHex = BytesToHexString(Previous);
		const FString PendingHex = BytesToHexString(Pending);
		ASSERT_THAT(AreEqual(
			FString(TEXT(
				"55454153435632430100000002000000"
				"4c4c4c4c4c4c4c4c4c4c4c4c4c4c4c4c"
				"4c4c4c4c4c4c4c4c4c4c4c4c4c4c4c4c"
				"d33ed6367f59821d2d38d28ce0b7edec"
				"c5e062ed03e59c0461d8f2ef97e9cdce")),
			PreviousHex,
			*FString::Printf(TEXT("Previous pointer golden actual=%s"), *PreviousHex)));
		ASSERT_THAT(AreEqual(
			FString(TEXT(
				"55454153435632430100000003000000"
				"4c4c4c4c4c4c4c4c4c4c4c4c4c4c4c4c"
				"4c4c4c4c4c4c4c4c4c4c4c4c4c4c4c4c"
				"682937b439bbcd2bae1d66fe6318209e"
				"cea389b26bba2de1cfb8da98eca4716c")),
			PendingHex,
			*FString::Printf(TEXT("Pending pointer golden actual=%s"), *PendingHex)));

		FAngelscriptCachePointerValue Decoded;
		AssertPointerInvalid(
			DecodeAngelscriptCachePointer(
				Current, EAngelscriptCachePointerKind::Previous, Decoded),
			EAngelscriptCacheStorePathCategory::PreviousPointer);
		AssertPointerInvalid(
			DecodeAngelscriptCachePointer(
				Previous, EAngelscriptCachePointerKind::Current, Decoded),
			EAngelscriptCacheStorePathCategory::CurrentPointer);
		AssertPointerInvalid(
			DecodeAngelscriptCachePointer(
				Pending, EAngelscriptCachePointerKind::Current, Decoded),
			EAngelscriptCacheStorePathCategory::CurrentPointer);
	}

	TEST_METHOD(RejectsEveryMalformedPointerFieldAndExactEofMismatch)
	{
		TArray<uint8> Valid;
		ASSERT_THAT(IsTrue(EncodeAngelscriptCachePointer(
			{EAngelscriptCachePointerKind::Current, RepeatedByteHash(0x6e)},
			Valid).IsSuccess()));
		FAngelscriptCachePointerValue Decoded;
		auto Reject = [&](TArray<uint8> Candidate)
		{
			AssertPointerInvalid(
				DecodeAngelscriptCachePointer(
					Candidate, EAngelscriptCachePointerKind::Current, Decoded),
				EAngelscriptCacheStorePathCategory::CurrentPointer);
			ASSERT_THAT(AreEqual(EAngelscriptCachePointerKind::Invalid, Decoded.Kind));
			ASSERT_THAT(IsTrue(Decoded.GenerationId.IsZero()));
		};

		TArray<uint8> Candidate = Valid;
		Candidate[0] ^= 1;
		Reject(MoveTemp(Candidate));
		Candidate = Valid;
		Candidate[8] = 2;
		Reject(MoveTemp(Candidate));
		Candidate = Valid;
		Candidate[12] = 0;
		Reject(MoveTemp(Candidate));
		Candidate = Valid;
		Candidate[13] = 1;
		Reject(MoveTemp(Candidate));
		Candidate = Valid;
		FMemory::Memzero(Candidate.GetData() + 16, 32);
		Reject(MoveTemp(Candidate));
		Candidate = Valid;
		Candidate[48] ^= 1;
		Reject(MoveTemp(Candidate));
		Candidate = Valid;
		Candidate.Pop();
		Reject(MoveTemp(Candidate));
		Candidate = Valid;
		Candidate.Add(0);
		Reject(MoveTemp(Candidate));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
