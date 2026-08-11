#pragma once

#include "Cache/AngelscriptCacheDecodedRecord.h"
#include "Cache/Private/AngelscriptCacheCanonicalCodec.h"

namespace AngelscriptCacheSemanticRecords_Private
{
	struct FDecodedRecordCodecBridge final
	{
		static FAngelscriptCacheValidationResult TryDecodeSourceIndex(
			TConstArrayView<uint8> Payload,
			const FAngelscriptCacheReadLimits& Limits,
			FAngelscriptCacheReadBudget& Budget,
			const AngelscriptCacheCanonicalCodec_Private::FDecodedChargeSink& ChargeSink,
			FAngelscriptCachedSourceIndex& OutValue,
			FAngelscriptDecodedCacheRecord::FSourceIndexCapturedOffsetStorage& OutOffsets);

		static FAngelscriptCacheValidationResult TryDecodeModuleInterface(
			TConstArrayView<uint8> Payload,
			const FAngelscriptCacheReadLimits& Limits,
			FAngelscriptCacheReadBudget& Budget,
			const AngelscriptCacheCanonicalCodec_Private::FDecodedChargeSink& ChargeSink,
			FAngelscriptCachedModuleInterface& OutValue,
			FAngelscriptDecodedCacheRecord::FModuleInterfaceCapturedOffsetStorage& OutOffsets);
	};
}
