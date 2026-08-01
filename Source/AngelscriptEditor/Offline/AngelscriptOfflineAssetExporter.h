#pragma once

#include "CoreMinimal.h"
#include "Dump/AngelscriptOfflineContractTypes.h"

class IAssetRegistry;
struct FAssetData;

namespace AngelscriptEditor::Offline
{
	struct FOfflineAssetSourceRecord
	{
		FString PackagePath;
		FString ObjectPath;
		FString GeneratedClassPath;
		FString AssetClassPath;
		FString BaseClassPath;
		FString OriginModule;
		FString OriginPlugin;
		FString RedirectSource;
		FString RedirectTarget;
		AngelscriptOfflineContract::EAvailability Availability =
			AngelscriptOfflineContract::EAvailability::Available;
		TMap<FString, FString> TypeCheckTags;
	};

	struct FOfflineAssetExportRequest
	{
		TArray<FString> Roots = {TEXT("/Game")};
		TArray<FString> ExcludedRoots;
		bool bRegistryComplete = true;
	};

	struct FOfflineAssetExportResult
	{
		bool bSuccess = false;
		FString Error;
		AngelscriptOfflineContract::FScopeRecord Scope;
		TArray<AngelscriptOfflineContract::FAssetRecord> Assets;
	};

	ANGELSCRIPTEDITOR_API FString NormalizeAssetObjectPath(
		FStringView Value);

	ANGELSCRIPTEDITOR_API FOfflineAssetExportResult ExportAssetRecords(
		const TArray<FOfflineAssetSourceRecord>& Source,
		const FOfflineAssetExportRequest& Request);

	ANGELSCRIPTEDITOR_API FOfflineAssetExportResult ExportAssetRegistry(
		IAssetRegistry& AssetRegistry,
		const FOfflineAssetExportRequest& Request);
}
