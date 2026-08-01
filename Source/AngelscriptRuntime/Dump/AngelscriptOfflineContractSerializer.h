#pragma once

#include "AngelscriptOfflineContractJson.h"
#include "AngelscriptOfflineContractTypes.h"

namespace AngelscriptOfflineContract
{
	ANGELSCRIPTRUNTIME_API FCanonicalJsonValue ToCanonicalJson(
		const FSymbolRecord& Record);
	ANGELSCRIPTRUNTIME_API FCanonicalJsonValue ToCanonicalJson(
		const FAssetRecord& Record);
	ANGELSCRIPTRUNTIME_API FCanonicalJsonValue ToCanonicalJson(
		const FManifestRecord& Record);

	ANGELSCRIPTRUNTIME_API TArray<uint8> SerializeSymbolRecords(
		const TArray<FSymbolRecord>& Records);
	ANGELSCRIPTRUNTIME_API TArray<uint8> SerializeAssetRecords(
		const TArray<FAssetRecord>& Records);
}
