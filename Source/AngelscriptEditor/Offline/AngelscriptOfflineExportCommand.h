#pragma once

#include "CoreMinimal.h"
#include "Dump/AngelscriptOfflineBundleWriter.h"

namespace AngelscriptEditor::Offline
{
	enum class EOfflineExportCommandletExitCode : int32
	{
		Success = 0,
		InvalidArguments = 2,
		EngineNotReady = 3,
		IncompleteSymbolScope = 4,
		IncompleteAssetScope = 5,
		PublicationFailure = 7,
		AssetScanFailure = 8,
	};

	struct FOfflineExportCommandOptions
	{
		FString OutputDirectory;
		AngelscriptOfflineContract::EBundleKind BundleKind =
			AngelscriptOfflineContract::EBundleKind::Project;
		TArray<FString> AssetRoots;
		TArray<FString> ExcludedAssetRoots;
		bool bAllowIncompleteAssets = false;
		bool bOutputWasExplicit = false;
		bool bAssetRootsWereExplicit = false;
	};

	struct FOfflineExportPublicationResult
	{
		EOfflineExportCommandletExitCode ExitCode =
			EOfflineExportCommandletExitCode::PublicationFailure;
		FString Error;
		AngelscriptOfflineContract::FBundleWriteResult WriteResult;

		bool IsSuccess() const
		{
			return ExitCode
				== EOfflineExportCommandletExitCode::Success;
		}
	};

	ANGELSCRIPTEDITOR_API FString GetDefaultOfflineExportDirectory(
		AngelscriptOfflineContract::EBundleKind BundleKind);

	ANGELSCRIPTEDITOR_API bool TryParseOfflineExportCommandOptions(
		const FString& Params,
		FOfflineExportCommandOptions& OutOptions,
		FString& OutError);

	/**
	 * Final commandlet gate and publication seam. Tests may supply a prepared
	 * bundle so option, completeness, and atomic-publication
	 * behavior can be exercised without starting a second editor process.
	 */
	ANGELSCRIPTEDITOR_API FOfflineExportPublicationResult
		PublishPreparedOfflineBundle(
			AngelscriptOfflineContract::FBundleWriteRequest Bundle,
			const FOfflineExportCommandOptions& Options,
			bool bEngineReady);
}
