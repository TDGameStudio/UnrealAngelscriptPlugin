#include "AngelscriptOfflineExportCommandlet.h"

#include "AngelscriptOfflineAssetExporter.h"
#include "AngelscriptOfflineExportCommand.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Core/AngelscriptEngine.h"
#include "Dump/AngelscriptOfflineExportService.h"
#include "Modules/ModuleManager.h"

int32 UAngelscriptOfflineExportCommandlet::Main(const FString& Params)
{
	using namespace AngelscriptEditor::Offline;
	using namespace AngelscriptOfflineContract;

	FOfflineExportCommandOptions Options;
	FString Error;
	if (!TryParseOfflineExportCommandOptions(
		Params,
		Options,
		Error))
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Angelscript offline export arguments are invalid: %s"),
			*Error);
		return static_cast<int32>(
			EOfflineExportCommandletExitCode::InvalidArguments);
	}

	FAngelscriptEngine& Engine = FAngelscriptEngine::Get();
	const bool bEngineReady =
		Engine.bDidInitialCompileSucceed
		&& Engine.GetScriptEngine() != nullptr;
	if (!bEngineReady)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT(
				"Angelscript offline export requires a successfully initialized final AngelScript engine."));
		return static_cast<int32>(
			EOfflineExportCommandletExitCode::EngineNotReady);
	}

	FOfflineAssetExportResult AssetExport;
	if (Options.AssetRoots.IsEmpty())
	{
		AssetExport.bSuccess = true;
		AssetExport.Scope.bComplete = false;
		AssetExport.Scope.State =
			TEXT("asset-scope-not-requested");
		AssetExport.Scope.Diagnostics.Add(
			TEXT("No asset roots were requested."));
	}
	else
	{
		FAssetRegistryModule& AssetRegistryModule =
			FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
				TEXT("AssetRegistry"));
		IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
		AssetRegistry.SearchAllAssets(true);

		FOfflineAssetExportRequest AssetRequest;
		AssetRequest.Roots = Options.AssetRoots;
		AssetRequest.ExcludedRoots =
			Options.ExcludedAssetRoots;
		AssetRequest.bRegistryComplete =
			!AssetRegistry.IsLoadingAssets();
		AssetExport =
			ExportAssetRegistry(AssetRegistry, AssetRequest);
	}
	if (!AssetExport.bSuccess)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Angelscript offline asset export failed: %s"),
			*AssetExport.Error);
		return static_cast<int32>(
			EOfflineExportCommandletExitCode::AssetScanFailure);
	}

	FOfflineExportBuildRequest BuildRequest;
	BuildRequest.BundleKind = Options.BundleKind;
	BuildRequest.OutputDirectory = Options.OutputDirectory;
	BuildRequest.AssetScope = MoveTemp(AssetExport.Scope);
	BuildRequest.Assets = MoveTemp(AssetExport.Assets);
	FOfflineExportBuildResult Built =
		FAngelscriptOfflineExportService::Build(
			Engine,
			BuildRequest);
	if (!Built.bSuccess)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Angelscript offline symbol export failed: %s"),
			*Built.Error);
		return static_cast<int32>(
			EOfflineExportCommandletExitCode::PublicationFailure);
	}

	const FOfflineExportPublicationResult Published =
		PublishPreparedOfflineBundle(
			MoveTemp(Built.Bundle),
			Options,
			true);
	if (!Published.IsSuccess())
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Angelscript offline bundle was not published: %s"),
			*Published.Error);
		return static_cast<int32>(Published.ExitCode);
	}

	UE_LOG(
		LogTemp,
		Display,
		TEXT(
			"Angelscript offline bundle published: Output=\"%s\" Kind=%s Symbols=%lld Assets=%lld Identity=%s"),
		*Published.WriteResult.PublishedDirectory,
		LexToString(Options.BundleKind),
		Published.WriteResult.SymbolFile.RecordCount,
		Published.WriteResult.AssetFile.RecordCount,
		*Published.WriteResult.BundleIdentity);
	return static_cast<int32>(
		EOfflineExportCommandletExitCode::Success);
}
