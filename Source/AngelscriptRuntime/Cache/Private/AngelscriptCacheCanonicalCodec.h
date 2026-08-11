#pragma once

#include "Cache/AngelscriptCacheSemanticRecords.h"
#include "Containers/StringConv.h"

#if WITH_ANGELSCRIPT_UNITTESTS
#include <type_traits>
#endif

// Private pointer-free canonical byte cursors shared by Cache V2 record codecs.
// This header deliberately exposes no Runtime module public API.
namespace AngelscriptCacheCanonicalCodec_Private
{
	static FAngelscriptCacheValidationResult Failure(
		const EAngelscriptCacheValidationError Error,
		const EAngelscriptCacheRecordKind Kind = static_cast<EAngelscriptCacheRecordKind>(0),
		const uint64 Offset = 0)
	{
		return FAngelscriptCacheValidationResult::AtStage(
			Error,
			Kind,
			EAngelscriptCacheValidationStage::PayloadDecode,
			Offset);
	}

	static bool ValidateUtf8(
		const uint8* Bytes,
		const uint32 Length,
		bool& bOutEmbeddedNul,
		uint64& OutTCharCount)
	{
		bOutEmbeddedNul = false;
		OutTCharCount = 0;
		uint32 Index = 0;
		while (Index < Length)
		{
			const uint8 Lead = Bytes[Index++];
			if (Lead == 0)
			{
				bOutEmbeddedNul = true;
				return false;
			}
			if (Lead <= 0x7f)
			{
				++OutTCharCount;
				continue;
			}

			uint32 CodePoint = 0;
			uint32 Continuations = 0;
			uint32 Minimum = 0;
			if (Lead >= 0xc2 && Lead <= 0xdf)
			{
				CodePoint = Lead & 0x1f;
				Continuations = 1;
				Minimum = 0x80;
			}
			else if (Lead >= 0xe0 && Lead <= 0xef)
			{
				CodePoint = Lead & 0x0f;
				Continuations = 2;
				Minimum = 0x800;
			}
			else if (Lead >= 0xf0 && Lead <= 0xf4)
			{
				CodePoint = Lead & 0x07;
				Continuations = 3;
				Minimum = 0x10000;
			}
			else
			{
				return false;
			}

			if (Continuations > Length - Index)
			{
				return false;
			}
			for (uint32 Continuation = 0; Continuation < Continuations; ++Continuation)
			{
				const uint8 Byte = Bytes[Index++];
				if ((Byte & 0xc0) != 0x80)
				{
					return false;
				}
				CodePoint = (CodePoint << 6) | (Byte & 0x3f);
			}
			if (CodePoint < Minimum || CodePoint > 0x10ffff
				|| (CodePoint >= 0xd800 && CodePoint <= 0xdfff))
			{
				return false;
			}
			OutTCharCount += sizeof(TCHAR) == 2 && CodePoint > 0xffff ? 2 : 1;
		}
		return true;
	}

	template <typename ElementType>
	static bool TryCalculateArrayReserveBytes(
		const int32 RequestedCapacity,
		int32& OutReservedCapacity,
		uint64& OutBytes)
	{
		OutReservedCapacity = 0;
		OutBytes = 0;
		if (RequestedCapacity <= 0)
		{
			return RequestedCapacity == 0;
		}

		using FArrayType = TArray<ElementType>;
		typename FArrayType::ElementAllocatorType Allocator;
		if constexpr (TAllocatorTraits<typename FArrayType::AllocatorType>::SupportsElementAlignment)
		{
			OutReservedCapacity = Allocator.CalculateSlackReserve(
				RequestedCapacity, sizeof(ElementType), alignof(ElementType));
		}
		else
		{
			OutReservedCapacity = Allocator.CalculateSlackReserve(
				RequestedCapacity, sizeof(ElementType));
		}
		if (OutReservedCapacity < RequestedCapacity
			|| static_cast<uint64>(OutReservedCapacity) > MAX_uint64 / sizeof(ElementType))
		{
			OutReservedCapacity = 0;
			return false;
		}

		OutBytes = static_cast<uint64>(OutReservedCapacity) * sizeof(ElementType);
		return true;
	}

	enum class EDecodedChargeResult : uint8
	{
		Accepted,
		BudgetExceeded,
		Overflow,
	};

#if WITH_ANGELSCRIPT_UNITTESTS
	struct FDecodedAllocationObserverForTests
	{
		using FObserveFunction = void (*)(
			void* Context,
			const AngelscriptCacheCanonicalCodecTestHooks::FAllocationEvent& Event);
		using FShouldInjectFailureFunction = bool (*)(
			void* Context,
			const AngelscriptCacheCanonicalCodecTestHooks::FAllocationEvent& Event);

		void Observe(
			const AngelscriptCacheCanonicalCodecTestHooks::FAllocationEvent& Event) const
		{
			if (ObserveFunction != nullptr)
			{
				ObserveFunction(Context, Event);
			}
		}

		bool ShouldInjectFailureAfterAllocation(
			const AngelscriptCacheCanonicalCodecTestHooks::FAllocationEvent& Event) const
		{
			return ShouldInjectFailureFunction != nullptr
				&& ShouldInjectFailureFunction(Context, Event);
		}

		void* Context = nullptr;
		FObserveFunction ObserveFunction = nullptr;
		FShouldInjectFailureFunction ShouldInjectFailureFunction = nullptr;
	};
#endif

	class FDecodedChargeSink final
	{
	public:
		using FTryChargeFunction = EDecodedChargeResult (*)(void* Context, uint64 Bytes);

		FDecodedChargeSink(
			void* InContext,
			const FTryChargeFunction InTryChargeFunction
#if WITH_ANGELSCRIPT_UNITTESTS
			, const FDecodedAllocationObserverForTests InObserver = {}
#endif
			)
			: Context(InContext)
			, TryChargeFunction(InTryChargeFunction)
#if WITH_ANGELSCRIPT_UNITTESTS
			, Observer(InObserver)
#endif
		{
			check(Context != nullptr);
			check(TryChargeFunction != nullptr);
		}

		EDecodedChargeResult TryCharge(const uint64 Bytes) const
		{
			return TryChargeFunction(Context, Bytes);
		}

#if WITH_ANGELSCRIPT_UNITTESTS
		void Observe(
			const AngelscriptCacheCanonicalCodecTestHooks::FAllocationEvent& Event) const
		{
			Observer.Observe(Event);
		}

		bool ShouldInjectFailureAfterAllocation(
			const AngelscriptCacheCanonicalCodecTestHooks::FAllocationEvent& Event) const
		{
			return Observer.ShouldInjectFailureAfterAllocation(Event);
		}
#endif

	private:
		void* Context = nullptr;
		FTryChargeFunction TryChargeFunction = nullptr;
#if WITH_ANGELSCRIPT_UNITTESTS
		FDecodedAllocationObserverForTests Observer;
#endif
	};

	class FWriter
	{
	public:
		void WriteUInt8(const uint8 Value) { Bytes.Add(Value); }
		void WriteUInt32(const uint32 Value)
		{
			Bytes.Add(static_cast<uint8>(Value));
			Bytes.Add(static_cast<uint8>(Value >> 8));
			Bytes.Add(static_cast<uint8>(Value >> 16));
			Bytes.Add(static_cast<uint8>(Value >> 24));
		}
		void WriteUInt64(const uint64 Value)
		{
			for (uint32 Shift = 0; Shift < 64; Shift += 8)
			{
				Bytes.Add(static_cast<uint8>(Value >> Shift));
			}
		}
		void WriteHash(const FAngelscriptHash256& Value)
		{
			Bytes.Append(Value.Value.GetBytes(), sizeof(FBlake3Hash::ByteArray));
		}
		void WriteString(const FString& Value)
		{
			const FTCHARToUTF8 Utf8(*Value, Value.Len());
			WriteUInt32(static_cast<uint32>(Utf8.Length()));
			Bytes.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
		}
		void WriteByteArray(const TConstArrayView<uint8> Value)
		{
			check(Value.Num() >= 0);
			WriteUInt64(static_cast<uint64>(Value.Num()));
			Bytes.Append(Value.GetData(), Value.Num());
		}
		void WriteOptionalHash(const TOptional<FAngelscriptHash256>& Value)
		{
			WriteUInt8(Value.IsSet() ? 1 : 0);
			if (Value.IsSet())
			{
				WriteHash(Value.GetValue());
			}
		}
		void WriteOptionalString(const TOptional<FString>& Value)
		{
			WriteUInt8(Value.IsSet() ? 1 : 0);
			if (Value.IsSet())
			{
				WriteString(Value.GetValue());
			}
		}

		TArray<uint8> Bytes;
	};

	class FReader
	{
	public:
		FReader(
			const TConstArrayView<uint8> InBytes,
			const FAngelscriptCacheReadLimits& InLimits,
			FAngelscriptCacheReadBudget& InBudget,
			const EAngelscriptCacheRecordKind InKind,
			const FDecodedChargeSink InDecodedChargeSink)
			: Bytes(InBytes)
			, Limits(InLimits)
			, Budget(InBudget)
			, Kind(InKind)
			, DecodedChargeSink(InDecodedChargeSink)
		{
		}

		bool HasFailed() const { return !Result.IsSuccess(); }
		const FAngelscriptCacheValidationResult& GetResult() const { return Result; }
		uint64 GetOffset() const { return Offset; }
		bool IsAtEnd() const { return Offset == static_cast<uint64>(Bytes.Num()); }

		void Fail(const EAngelscriptCacheValidationError Error, const uint64 AtOffset = MAX_uint64)
		{
			if (Result.IsSuccess())
			{
				Result = Failure(Error, Kind, AtOffset == MAX_uint64 ? Offset : AtOffset);
			}
		}

		void SetSemanticEnclosingFieldOffset(const uint64 AtOffset)
		{
			SemanticEnclosingFieldOffset = AtOffset;
		}

		void FailSemantic(const EAngelscriptCacheValidationError Error)
		{
			if (Result.IsSuccess())
			{
				Result = FAngelscriptCacheValidationResult::AtStage(
					Error,
					Kind,
					EAngelscriptCacheValidationStage::LocalSemantic,
					SemanticEnclosingFieldOffset);
			}
		}

		bool ReadUInt8(uint8& OutValue)
		{
			const uint8* Data = ReadFixed(1);
			if (Data == nullptr)
			{
				return false;
			}
			OutValue = Data[0];
			return true;
		}

		bool ReadUInt32(uint32& OutValue)
		{
			const uint8* Data = ReadFixed(4);
			if (Data == nullptr)
			{
				return false;
			}
			OutValue = static_cast<uint32>(Data[0])
				| (static_cast<uint32>(Data[1]) << 8)
				| (static_cast<uint32>(Data[2]) << 16)
				| (static_cast<uint32>(Data[3]) << 24);
			return true;
		}

		bool ReadUInt64(uint64& OutValue)
		{
			const uint8* Data = ReadFixed(8);
			if (Data == nullptr)
			{
				return false;
			}
			OutValue = 0;
			for (uint32 Shift = 0; Shift < 64; Shift += 8)
			{
				OutValue |= static_cast<uint64>(Data[Shift / 8]) << Shift;
			}
			return true;
		}

		bool ReadHash(FAngelscriptHash256& OutValue)
		{
			const uint8* Data = ReadFixed(sizeof(FBlake3Hash::ByteArray));
			if (Data == nullptr)
			{
				return false;
			}
			FBlake3Hash::ByteArray HashBytes{};
			FMemory::Memcpy(HashBytes, Data, sizeof(HashBytes));
			OutValue = FAngelscriptHash256{FBlake3Hash(HashBytes)};
			return true;
		}

		bool ReadString(FString& OutValue)
		{
			OutValue.Empty();
			const uint64 FieldOffset = Offset;
			uint32 Length = 0;
			if (!ReadUInt32(Length))
			{
				return false;
			}
			if (Length > Limits.MaxStringBytes)
			{
				Fail(EAngelscriptCacheValidationError::BudgetExceeded, FieldOffset);
				return false;
			}
			const uint8* Data = ReadFixed(Length);
			if (Data == nullptr)
			{
				return false;
			}
			bool bEmbeddedNul = false;
			uint64 TCharCount = 0;
			if (!ValidateUtf8(Data, Length, bEmbeddedNul, TCharCount))
			{
				Fail(bEmbeddedNul
					? EAngelscriptCacheValidationError::EmbeddedNul
					: EAngelscriptCacheValidationError::InvalidUtf8, FieldOffset);
				return false;
			}
			if (Length == 0)
			{
				return true;
			}
			const uint64 CharacterCountWithTerminator = TCharCount + 1;
			if (CharacterCountWithTerminator > static_cast<uint64>(MAX_int32))
			{
				Fail(EAngelscriptCacheValidationError::ImpossibleCount, FieldOffset);
				return false;
			}
			const int32 CharacterCountWithTerminator32 =
				static_cast<int32>(CharacterCountWithTerminator);
			int32 ReservedCapacity = 0;
			uint64 ReservedBytes = 0;
			if (!TryCalculateArrayReserveBytes<TCHAR>(
				CharacterCountWithTerminator32, ReservedCapacity, ReservedBytes))
			{
				Fail(EAngelscriptCacheValidationError::Overflow, FieldOffset);
				return false;
			}
			const int32 TCharCount32 = static_cast<int32>(TCharCount);
			TArray<TCHAR>& CharacterArray = OutValue.GetCharArray();
			if (!TryConsumeAndReserveDecodedArray(
				FieldOffset,
				CharacterCountWithTerminator32,
				ReservedCapacity,
				ReservedBytes,
				CharacterArray))
			{
				return false;
			}
			CharacterArray.SetNumUninitialized(TCharCount32 + 1);
			FUTF8ToTCHAR_Convert::Convert(
				OutValue.GetCharArray().GetData(), TCharCount32,
				reinterpret_cast<const ANSICHAR*>(Data), static_cast<int32>(Length));
			OutValue.GetCharArray()[TCharCount32] = TEXT('\0');
			return true;
		}

		bool ReadOptionalHash(TOptional<FAngelscriptHash256>& OutValue)
		{
			OutValue.Reset();
			uint8 Tag = 0;
			const uint64 TagOffset = Offset;
			if (!ReadUInt8(Tag))
			{
				return false;
			}
			if (Tag > 1)
			{
				Fail(EAngelscriptCacheValidationError::InvalidOptionalTag, TagOffset);
				return false;
			}
			if (Tag == 1)
			{
				FAngelscriptHash256 Hash;
				if (!ReadHash(Hash))
				{
					return false;
				}
				OutValue = Hash;
			}
			return true;
		}

		bool ReadOptionalString(TOptional<FString>& OutValue)
		{
			OutValue.Reset();
			uint8 Tag = 0;
			const uint64 TagOffset = Offset;
			if (!ReadUInt8(Tag))
			{
				return false;
			}
			if (Tag > 1)
			{
				Fail(EAngelscriptCacheValidationError::InvalidOptionalTag, TagOffset);
				return false;
			}
			if (Tag == 1)
			{
				FString Value;
				if (!ReadString(Value))
				{
					return false;
				}
				OutValue = MoveTemp(Value);
			}
			return true;
		}

		template <typename ElementType>
		bool ReadArrayCountAndReserve(
			const uint64 MinimumElementBytes,
			TArray<ElementType>& OutValues,
			uint32& OutCount)
		{
			OutValues.Empty();
			const uint64 FieldOffset = Offset;
			if (!ReadUInt32(OutCount))
			{
				return false;
			}
			if (OutCount > Limits.MaxArrayElements)
			{
				Fail(EAngelscriptCacheValidationError::BudgetExceeded, FieldOffset);
				return false;
			}
			if (OutCount > static_cast<uint32>(MAX_int32))
			{
				Fail(EAngelscriptCacheValidationError::ImpossibleCount, FieldOffset);
				return false;
			}
			if (MinimumElementBytes != 0
				&& static_cast<uint64>(OutCount) > MAX_uint64 / MinimumElementBytes)
			{
				Fail(EAngelscriptCacheValidationError::Overflow, FieldOffset);
				return false;
			}
			const uint64 MinimumBytes = static_cast<uint64>(OutCount) * MinimumElementBytes;
			const uint64 AvailableBytes = static_cast<uint64>(Bytes.Num());
			if (Offset > AvailableBytes || MinimumBytes > AvailableBytes - Offset)
			{
				// The count is representable and within the configured element Budget;
				// an undersized backing payload is a physical truncation, not an
				// intrinsically impossible declaration.
				Fail(EAngelscriptCacheValidationError::OutOfBounds, AvailableBytes);
				return false;
			}
			int32 ReservedCapacity = 0;
			uint64 ReservedBytes = 0;
			if (!TryCalculateArrayReserveBytes<ElementType>(
				static_cast<int32>(OutCount), ReservedCapacity, ReservedBytes))
			{
				Fail(EAngelscriptCacheValidationError::Overflow, FieldOffset);
				return false;
			}
			if (OutCount == 0)
			{
				return true;
			}
			return TryConsumeAndReserveDecodedArray(
				FieldOffset,
				static_cast<int32>(OutCount),
				ReservedCapacity,
				ReservedBytes,
				OutValues);
		}

		bool ReadByteArrayCountAndReserve(
			TArray<uint8>& OutValues,
			uint64& OutCount)
		{
			OutValues.Empty();
			const uint64 FieldOffset = Offset;
			if (!ReadUInt64(OutCount))
			{
				return false;
			}
			if (OutCount > Limits.MaxArrayElements)
			{
				Fail(EAngelscriptCacheValidationError::BudgetExceeded, FieldOffset);
				return false;
			}
			if (OutCount > static_cast<uint64>(MAX_int32))
			{
				Fail(EAngelscriptCacheValidationError::ImpossibleCount, FieldOffset);
				return false;
			}
			const uint64 AvailableBytes = static_cast<uint64>(Bytes.Num());
			if (Offset > AvailableBytes || OutCount > AvailableBytes - Offset)
			{
				Fail(EAngelscriptCacheValidationError::OutOfBounds, AvailableBytes);
				return false;
			}
			return ReserveDecodedArrayAtOffset(FieldOffset, OutCount, OutValues);
		}

		bool ConsumeReference()
		{
			if (!Budget.TryConsumeReferencesAndRelocations(1, Limits))
			{
				Fail(EAngelscriptCacheValidationError::BudgetExceeded);
				return false;
			}
			return true;
		}

		template <typename ElementType>
		bool ReserveDecodedArrayAtOffset(
			const uint64 FieldOffset,
			const uint64 RequestedCount,
			TArray<ElementType>& OutValues)
		{
			OutValues.Empty();
			if (RequestedCount > static_cast<uint64>(MAX_int32))
			{
				Fail(EAngelscriptCacheValidationError::ImpossibleCount, FieldOffset);
				return false;
			}
			const int32 RequestedCapacity = static_cast<int32>(RequestedCount);
			int32 ReservedCapacity = 0;
			uint64 ReservedBytes = 0;
			if (!TryCalculateArrayReserveBytes<ElementType>(
				RequestedCapacity, ReservedCapacity, ReservedBytes))
			{
				Fail(EAngelscriptCacheValidationError::Overflow, FieldOffset);
				return false;
			}
			if (RequestedCapacity == 0)
			{
				return true;
			}
			return TryConsumeAndReserveDecodedArray(
				FieldOffset,
				RequestedCapacity,
				ReservedCapacity,
				ReservedBytes,
				OutValues);
		}

		const FAngelscriptCacheReadLimits& GetLimits() const { return Limits; }

	private:
		template <typename ElementType>
		bool TryConsumeAndReserveDecodedArray(
			const uint64 FieldOffset,
			const int32 RequestedCapacity,
			const int32 ReservedCapacity,
			const uint64 ReservedBytes,
			TArray<ElementType>& OutValues)
		{
#if WITH_ANGELSCRIPT_UNITTESTS
			constexpr AngelscriptCacheCanonicalCodecTestHooks::EAllocationSite Site =
				std::is_same_v<ElementType, TCHAR>
					? AngelscriptCacheCanonicalCodecTestHooks::EAllocationSite::StringCharacters
					: AngelscriptCacheCanonicalCodecTestHooks::EAllocationSite::TypedArrayElements;
			DecodedChargeSink.Observe({
				Site,
				AngelscriptCacheCanonicalCodecTestHooks::EAllocationEventPhase::BudgetAttempt,
				FieldOffset,
				RequestedCapacity,
				ReservedCapacity,
				ReservedBytes,
				0,
				sizeof(ElementType),
				alignof(ElementType)});
#endif
			const EDecodedChargeResult ChargeResult =
				DecodedChargeSink.TryCharge(ReservedBytes);
			if (ChargeResult != EDecodedChargeResult::Accepted)
			{
				Fail(
					ChargeResult == EDecodedChargeResult::BudgetExceeded
						? EAngelscriptCacheValidationError::BudgetExceeded
						: EAngelscriptCacheValidationError::Overflow,
					FieldOffset);
				return false;
			}
#if WITH_ANGELSCRIPT_UNITTESTS
			DecodedChargeSink.Observe({
				Site,
				AngelscriptCacheCanonicalCodecTestHooks::EAllocationEventPhase::BudgetAccepted,
				FieldOffset,
				RequestedCapacity,
				ReservedCapacity,
				ReservedBytes,
				0,
				sizeof(ElementType),
				alignof(ElementType)});
			DecodedChargeSink.Observe({
				Site,
				AngelscriptCacheCanonicalCodecTestHooks::EAllocationEventPhase::AllocationAttempt,
				FieldOffset,
				RequestedCapacity,
				ReservedCapacity,
				ReservedBytes,
				0,
				sizeof(ElementType),
				alignof(ElementType)});
#endif
			// This is the only physical Reserve entry used by the canonical reader.
			OutValues.Reserve(RequestedCapacity);
			const uint64 ActualAllocatedBytes =
				static_cast<uint64>(OutValues.GetAllocatedSize());
#if WITH_ANGELSCRIPT_UNITTESTS
			const AngelscriptCacheCanonicalCodecTestHooks::FAllocationEvent
				AllocationSucceededEvent{
				Site,
				AngelscriptCacheCanonicalCodecTestHooks::EAllocationEventPhase::AllocationSucceeded,
				FieldOffset,
				RequestedCapacity,
				ReservedCapacity,
				ReservedBytes,
				ActualAllocatedBytes,
				sizeof(ElementType),
				alignof(ElementType)};
			DecodedChargeSink.Observe(AllocationSucceededEvent);
#endif
			if (ActualAllocatedBytes != ReservedBytes)
			{
				// The allocator prediction is a pre-allocation Budget authority. It may
				// never be corrected after allocation; unsupported combinations fail closed.
				OutValues.Empty();
				Fail(EAngelscriptCacheValidationError::Overflow, FieldOffset);
				return false;
			}
#if WITH_ANGELSCRIPT_UNITTESTS
			if (DecodedChargeSink.ShouldInjectFailureAfterAllocation(
				AllocationSucceededEvent))
			{
				OutValues.Empty();
				Fail(EAngelscriptCacheValidationError::Overflow, FieldOffset);
				return false;
			}
#endif
			return true;
		}

		const uint8* ReadFixed(const uint64 Count)
		{
			const uint64 AvailableBytes = static_cast<uint64>(Bytes.Num());
			if (Offset > AvailableBytes || Count > AvailableBytes - Offset)
			{
				// A truncated physical field reports the first unavailable byte,
				// rather than the beginning of the field that spans the cut.
				Fail(EAngelscriptCacheValidationError::OutOfBounds, AvailableBytes);
				return nullptr;
			}
			const uint8* Data = Bytes.GetData() + Offset;
			Offset += Count;
			return Data;
		}

		TConstArrayView<uint8> Bytes;
		const FAngelscriptCacheReadLimits& Limits;
		FAngelscriptCacheReadBudget& Budget;
		EAngelscriptCacheRecordKind Kind;
		FDecodedChargeSink DecodedChargeSink;
		uint64 Offset = 0;
		uint64 SemanticEnclosingFieldOffset = 0;
		FAngelscriptCacheValidationResult Result;
	};

	static FAngelscriptCacheValidationResult BeginRead(
		const TConstArrayView<uint8> Bytes,
		const FAngelscriptCacheReadLimits& Limits,
		FAngelscriptCacheReadBudget& Budget,
		const EAngelscriptCacheRecordKind Kind)
	{
		if (Bytes.Num() < 0)
		{
			return Failure(EAngelscriptCacheValidationError::InvalidArrayView, Kind);
		}
		if (Bytes.Num() > 0 && Bytes.GetData() == nullptr)
		{
			return Failure(EAngelscriptCacheValidationError::InvalidArrayView, Kind);
		}
		const uint64 Size = static_cast<uint64>(Bytes.Num());
		if (Size > Limits.MaxCanonicalRecordPayloadBytes)
		{
			return Failure(EAngelscriptCacheValidationError::BudgetExceeded, Kind);
		}
		return {};
	}

	template <typename EnumType>
	static bool ReadEnum(FReader& Reader, EnumType& OutValue, const uint8 Minimum, const uint8 Maximum)
	{
		uint8 Raw = 0;
		const uint64 Offset = Reader.GetOffset();
		if (!Reader.ReadUInt8(Raw))
		{
			return false;
		}
		if (Raw < Minimum || Raw > Maximum)
		{
			Reader.Fail(EAngelscriptCacheValidationError::UnknownEnumValue, Offset);
			return false;
		}
		OutValue = static_cast<EnumType>(Raw);
		return true;
	}
}
