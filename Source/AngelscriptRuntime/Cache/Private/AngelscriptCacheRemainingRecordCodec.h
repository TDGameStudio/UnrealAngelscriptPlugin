#pragma once

#include "Cache/AngelscriptCacheDecodedRecord.h"
#include "Cache/Private/AngelscriptCacheCanonicalCodec.h"

namespace AngelscriptCacheRemainingRecords_Private
{
	struct FDecodedRecordCodecBridge final
	{
		static bool ReadModuleStateDataType(
			AngelscriptCacheCanonicalCodec_Private::FReader& Reader,
			FAngelscriptCachedDataType& OutValue,
			FAngelscriptDecodedCacheRecord::FModuleStateCapturedOffsetStorage::
				FDataTypeOffsets& OutOffsets,
			uint32 PrimaryIndex,
			bool bHardValue,
			uint32& InOutNodeOrdinal,
			uint64 Depth);

		static bool ReadModuleStateDependency(
			AngelscriptCacheCanonicalCodec_Private::FReader& Reader,
			FAngelscriptCacheSemanticDependency& OutValue,
			FAngelscriptDecodedCacheRecord::FModuleStateCapturedOffsetStorage::
				FDependencyOffsets& OutOffsets,
			bool bActionDependency,
			uint32 PrimaryIndex,
			uint32 SecondaryIndex);

		static FAngelscriptCacheValidationResult TryDecodeModuleState(
			TConstArrayView<uint8> Payload,
			const FAngelscriptCacheReadLimits& Limits,
			FAngelscriptCacheReadBudget& Budget,
			const AngelscriptCacheCanonicalCodec_Private::FDecodedChargeSink& ChargeSink,
			FAngelscriptCachedModuleState& OutValue,
			FAngelscriptDecodedCacheRecord::FModuleStateCapturedOffsetStorage& OutOffsets);

		static FAngelscriptCacheValidationResult TryDecodeDebugSidecar(
			TConstArrayView<uint8> Payload,
			const FAngelscriptCacheReadLimits& Limits,
			FAngelscriptCacheReadBudget& Budget,
			const AngelscriptCacheCanonicalCodec_Private::FDecodedChargeSink& ChargeSink,
			FAngelscriptCachedDebugSidecar& OutValue,
			FAngelscriptDecodedCacheRecord::TSingleCapturedOffsetStorage<
				FAngelscriptDebugSidecarFieldCoordinate>& OutOffsets);

		static FAngelscriptCacheValidationResult TryDecodeFunctionBody(
			TConstArrayView<uint8> Payload,
			const FAngelscriptCacheReadLimits& Limits,
			FAngelscriptCacheReadBudget& Budget,
			const AngelscriptCacheCanonicalCodec_Private::FDecodedChargeSink& ChargeSink,
			FAngelscriptCachedFunctionBody& OutValue,
			FAngelscriptDecodedCacheRecord::FFunctionBodyCapturedOffsetStorage& OutOffsets);

		static FAngelscriptCacheValidationResult TryDecodeModuleSnapshot(
			TConstArrayView<uint8> Payload,
			const FAngelscriptCacheReadLimits& Limits,
			FAngelscriptCacheReadBudget& Budget,
			const AngelscriptCacheCanonicalCodec_Private::FDecodedChargeSink& ChargeSink,
			FAngelscriptCachedModuleSnapshot& OutValue,
			FAngelscriptDecodedCacheRecord::FModuleSnapshotCapturedOffsetStorage& OutOffsets);
	};
}
