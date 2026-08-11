#include "Cache/AngelscriptCacheArchive.h"
#include "Cache/AngelscriptCacheManifestPack.h"

#include "CQTest.h"
#include "HAL/PlatformTLS.h"
#include "Misc/ScopeLock.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptCacheParallelPackPreparationTests_Private
{
	class FThreadObservingCodec final : public IAngelscriptCacheStorageCodec
	{
	public:
		virtual bool TryCompressCanonicalZlib(
			const TConstArrayView<uint8> RawBytes,
			TArray<uint8>& OutStoredBytes) override
		{
			ObserveThread();
			return Inner.TryCompressCanonicalZlib(RawBytes, OutStoredBytes);
		}

		virtual bool TryCompressCanonicalZlibInto(
			const TConstArrayView<uint8> RawBytes,
			const TArrayView<uint8> StoredOutput,
			uint64& OutProducedBytes) override
		{
			ObserveThread();
			return Inner.TryCompressCanonicalZlibInto(
				RawBytes, StoredOutput, OutProducedBytes);
		}

		virtual bool TryDecompressCanonicalZlib(
			const TConstArrayView<uint8> StoredBytes,
			const TArrayView<uint8> RawOutput,
			uint64& OutProducedBytes) override
		{
			ObserveThread();
			return Inner.TryDecompressCanonicalZlib(
				StoredBytes, RawOutput, OutProducedBytes);
		}

		TArray<uint32> CaptureThreadIds() const
		{
			FScopeLock Lock(&Gate);
			return ThreadIds.Array();
		}

	private:
		void ObserveThread()
		{
			FScopeLock Lock(&Gate);
			ThreadIds.Add(FPlatformTLS::GetCurrentThreadId());
		}

		FAngelscriptUnrealZlibCacheStorageCodec Inner;
		mutable FCriticalSection Gate;
		TSet<uint32> ThreadIds;
	};

	static TArray<FAngelscriptPreparedRecord> MakeRecords(
		const int32 Count,
		const int32 BytesPerRecord)
	{
		TArray<FAngelscriptPreparedRecord> Records;
		Records.Reserve(Count);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			FAngelscriptPreparedRecord& Record = Records.AddDefaulted_GetRef();
			Record.CanonicalPayload.SetNumUninitialized(BytesPerRecord);
			for (int32 ByteIndex = 0; ByteIndex < BytesPerRecord; ++ByteIndex)
			{
				Record.CanonicalPayload[ByteIndex] = static_cast<uint8>(
					(Index * 131 + ByteIndex * 17 + ByteIndex / 251) & 0xff);
			}
			check(FAngelscriptCacheRecordArchive::TryBuildRecordId(
				EAngelscriptCacheRecordKind::FunctionBody,
				Record.CanonicalPayload,
				Record.RecordId).IsSuccess());
		}
		return Records;
	}

	static bool ArePackSetsIdentical(
		const TConstArrayView<FAngelscriptEncodedPack> Left,
		const TConstArrayView<FAngelscriptEncodedPack> Right)
	{
		if (Left.Num() != Right.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < Left.Num(); ++Index)
		{
			if (!(Left[Index].PackId == Right[Index].PackId)
				|| Left[Index].Bytes != Right[Index].Bytes
				|| Left[Index].Index.Num() != Right[Index].Index.Num())
			{
				return false;
			}
			for (int32 EntryIndex = 0;
				EntryIndex < Left[Index].Index.Num(); ++EntryIndex)
			{
				const FAngelscriptCachePackIndexEntry& A =
					Left[Index].Index[EntryIndex];
				const FAngelscriptCachePackIndexEntry& B =
					Right[Index].Index[EntryIndex];
				if (!(A.RecordId == B.RecordId)
					|| A.Codec != B.Codec
					|| A.PackOffset != B.PackOffset
					|| A.StoredSize != B.StoredSize
					|| A.RawSize != B.RawSize
					|| !(A.RawChecksum == B.RawChecksum))
				{
					return false;
				}
			}
		}
		return true;
	}
}

TEST_CLASS_WITH_FLAGS(
	FAngelscriptCacheParallelPackPreparationTests,
	"Angelscript.TestModule.Cache.ParallelPackPreparation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(BoundedParallelUsesOnlyWorkersAndEmitsSerialIdenticalBytes)
	{
		using namespace AngelscriptCacheParallelPackPreparationTests_Private;
		const TArray<FAngelscriptPreparedRecord> Records =
			MakeRecords(12, 128 * 1024);
		const uint32 CallingThreadId = FPlatformTLS::GetCurrentThreadId();

		FAngelscriptCachePackPolicy SerialPolicy;
		SerialPolicy.TargetRawBytesPerPack = 256 * 1024;
		SerialPolicy.ExecutionMode =
			EAngelscriptCachePreparationExecutionMode::ForcedSerial;
		SerialPolicy.MaxWorkerCount = 1;
		FThreadObservingCodec SerialCodec;
		TArray<FAngelscriptEncodedPack> SerialPacks;
		ASSERT_THAT(IsTrue(BuildAngelscriptCachePacks(
			Records, SerialPolicy, SerialCodec, SerialPacks).IsSuccess()));
		const TArray<uint32> SerialThreads = SerialCodec.CaptureThreadIds();
		ASSERT_THAT(AreEqual(1, SerialThreads.Num()));
		ASSERT_THAT(AreEqual(CallingThreadId, SerialThreads[0]));

		FAngelscriptCachePackPolicy ParallelPolicy = SerialPolicy;
		ParallelPolicy.ExecutionMode =
			EAngelscriptCachePreparationExecutionMode::BoundedParallel;
		ParallelPolicy.MaxWorkerCount = 3;
		FThreadObservingCodec ParallelCodec;
		TArray<FAngelscriptEncodedPack> ParallelPacks;
		ASSERT_THAT(IsTrue(BuildAngelscriptCachePacks(
			Records, ParallelPolicy, ParallelCodec, ParallelPacks).IsSuccess()));
		ASSERT_THAT(IsTrue(ArePackSetsIdentical(SerialPacks, ParallelPacks),
			TEXT("Parallel compression and Pack assembly must retain canonical bytes")));

		const TArray<uint32> ParallelThreads = ParallelCodec.CaptureThreadIds();
		ASSERT_THAT(IsTrue(!ParallelThreads.IsEmpty()));
		ASSERT_THAT(IsTrue(ParallelThreads.Num() <= 3));
		ASSERT_THAT(IsFalse(ParallelThreads.Contains(CallingThreadId),
			TEXT("Bounded-parallel record preparation must execute on workers")));
	}

	TEST_METHOD(PackTargetsFourSixteenAndSixtyFourMiBProduceExpectedGroups)
	{
		using namespace AngelscriptCacheParallelPackPreparationTests_Private;
		const TArray<FAngelscriptPreparedRecord> Records =
			MakeRecords(17, 1024 * 1024);
		const TArray<uint64> Targets = {
			UINT64_C(4) * 1024 * 1024,
			UINT64_C(16) * 1024 * 1024,
			UINT64_C(64) * 1024 * 1024,
		};
		const TArray<int32> ExpectedPackCounts = {5, 2, 1};
		for (int32 Index = 0; Index < Targets.Num(); ++Index)
		{
			FAngelscriptCachePackPolicy Policy;
			Policy.TargetRawBytesPerPack = Targets[Index];
			Policy.CompressionPolicy =
				EAngelscriptCachePackCompressionPolicy::ForceNoneForTest;
			Policy.ExecutionMode =
				EAngelscriptCachePreparationExecutionMode::BoundedParallel;
			Policy.MaxWorkerCount = 4;
			FThreadObservingCodec Codec;
			TArray<FAngelscriptEncodedPack> Packs;
			ASSERT_THAT(IsTrue(BuildAngelscriptCachePacks(
				Records, Policy, Codec, Packs).IsSuccess()));
			ASSERT_THAT(AreEqual(ExpectedPackCounts[Index], Packs.Num()));
		}
	}

	TEST_METHOD(InvalidParallelInputClearsCallerOutput)
	{
		using namespace AngelscriptCacheParallelPackPreparationTests_Private;
		TArray<FAngelscriptPreparedRecord> Records = MakeRecords(4, 1024);
		Records[2].RecordId.ContentHash = {};
		FAngelscriptCachePackPolicy Policy;
		Policy.ExecutionMode =
			EAngelscriptCachePreparationExecutionMode::BoundedParallel;
		Policy.MaxWorkerCount = 2;
		FThreadObservingCodec Codec;
		TArray<FAngelscriptEncodedPack> Packs;
		Packs.AddDefaulted();
		const FAngelscriptCacheValidationResult Result =
			BuildAngelscriptCachePacks(Records, Policy, Codec, Packs);
		ASSERT_THAT(IsFalse(Result.IsSuccess()));
		ASSERT_THAT(IsTrue(Packs.IsEmpty()));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheValidationError::RecordIdMismatch,
			Result.Error));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
