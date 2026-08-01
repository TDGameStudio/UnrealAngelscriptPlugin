#pragma once

#include "AngelscriptOfflineBundleWriter.h"

struct FAngelscriptEngine;

namespace AngelscriptOfflineContract
{
	struct FOfflineExportBuildRequest
	{
		EBundleKind BundleKind = EBundleKind::Project;
		FString OutputDirectory;
		FScopeRecord AssetScope;
		TArray<FAssetRecord> Assets;
	};

	struct FOfflineExportBuildResult
	{
		bool bSuccess = false;
		FString Error;
		FBundleWriteRequest Bundle;
	};

	class ANGELSCRIPTRUNTIME_API FAngelscriptOfflineExportService
	{
	public:
		static FOfflineExportBuildResult Build(
			FAngelscriptEngine& Engine,
			const FOfflineExportBuildRequest& Request);

		static FBundleWriteResult Export(
			FAngelscriptEngine& Engine,
			const FOfflineExportBuildRequest& Request);
	};
}
