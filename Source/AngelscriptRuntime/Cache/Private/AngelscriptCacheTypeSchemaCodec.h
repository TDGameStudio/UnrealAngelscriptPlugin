#pragma once

#include "Cache/AngelscriptCacheDecodedRecord.h"
#include "Cache/Private/AngelscriptCacheCanonicalCodec.h"

namespace AngelscriptCacheTypeSchema_Private
{
	using FDecodedChargeSink =
		AngelscriptCacheCanonicalCodec_Private::FDecodedChargeSink;

	struct FDecodedRecordCodecBridge final
	{
		static FAngelscriptCacheValidationResult TryDecodeTypeSchema(
			TConstArrayView<uint8> Payload,
			const FAngelscriptCacheReadLimits& Limits,
			FAngelscriptCacheReadBudget& Budget,
			const FDecodedChargeSink& ChargeSink,
#if WITH_ANGELSCRIPT_UNITTESTS
			FAngelscriptCacheTypeSchemaAllocationProbeForTests* Probe,
#endif
			FAngelscriptCachedTypeSchema& OutValue,
			FAngelscriptDecodedCacheRecord::FTypeSchemaCapturedOffsetStorage&
				OutOffsets);
	};
}
