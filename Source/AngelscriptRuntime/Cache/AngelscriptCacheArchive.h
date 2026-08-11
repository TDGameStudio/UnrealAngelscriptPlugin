#pragma once

#include "Cache/AngelscriptCacheTypes.h"

class ANGELSCRIPTRUNTIME_API FAngelscriptCacheRecordArchive
{
public:
	static constexpr uint32 ArchiveSchemaVersion = 2;
	static constexpr uint32 EnvelopeHeaderSize = 56;

	static FAngelscriptCacheValidationResult TryBuildRecordId(
		EAngelscriptCacheRecordKind Kind,
		TConstArrayView<uint8> CanonicalPayload,
		FAngelscriptCacheRecordId& OutRecordId);

	static FAngelscriptCacheValidationResult SerializeRecordEnvelope(
		EAngelscriptCacheRecordKind Kind,
		TConstArrayView<uint8> CanonicalPayload,
		TArray<uint8>& OutBytes);

	static FAngelscriptCacheValidationResult DeserializeRecordEnvelope(
		TConstArrayView<uint8> Bytes,
		const FAngelscriptCacheReadLimits& Limits,
		FAngelscriptCacheReadBudget& Budget,
		FAngelscriptCacheRecordEnvelope& OutEnvelope);

	static FAngelscriptCacheValidationResult DeserializeRecordEnvelope(
		TConstArrayView<uint8> Bytes,
		const FAngelscriptCacheReadLimits& Limits,
		FAngelscriptCacheRecordEnvelope& OutEnvelope);
};
