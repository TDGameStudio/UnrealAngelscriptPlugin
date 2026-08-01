#pragma once

#include "AngelscriptOfflineContractTypes.h"

namespace AngelscriptOfflineContract
{
	struct FBundleWriteRequest
	{
		FString OutputDirectory;
		FManifestRecord Manifest;
		TArray<FSymbolRecord> Symbols;
		TArray<FAssetRecord> Assets;
	};

	struct FBundleWriteResult
	{
		bool bSuccess = false;
		FString Error;
		FString PublishedDirectory;
		FFileRecord ManifestFile;
		FFileRecord SymbolFile;
		FFileRecord AssetFile;
		FString BundleIdentity;
	};

	class ANGELSCRIPTRUNTIME_API FAngelscriptOfflineBundleWriter
	{
	public:
		static FBundleWriteResult Write(
			const FBundleWriteRequest& Request);
	};
}
