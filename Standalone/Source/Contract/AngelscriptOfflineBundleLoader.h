#pragma once

#include "Contract/AngelscriptOfflineIndices.h"
#include "Contract/AngelscriptOfflineManifest.h"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>

namespace AngelscriptStandalone
{
	enum class EOfflineBundleSelectionSource
	{
		Explicit,
		PackagedDefault,
	};

	struct FOfflineBundleSelectionResult
	{
		bool bSuccess = false;
		std::string Error;
		std::filesystem::path Directory;
		EOfflineBundleSelectionSource Source =
			EOfflineBundleSelectionSource::PackagedDefault;
	};

	struct FOfflineBundleLoadOptions
	{
		FOfflineManifestCompatibility Compatibility;
		std::uint64_t MaximumManifestBytes = 16u * 1024u * 1024u;
		std::uint64_t MaximumRecordBytes = 16u * 1024u * 1024u;
		bool bRejectUnexpectedEntries = true;
	};

	struct FOfflineBundle
	{
		std::filesystem::path Directory;
		FOfflineManifest Manifest;
		std::shared_ptr<const FOfflineBundleIndices> Indices;
	};

	struct FOfflineBundleLoadResult
	{
		bool bSuccess = false;
		std::string Error;
		FOfflineBundle Bundle;
	};

	FOfflineBundleSelectionResult SelectOfflineBundleDirectory(
		const std::optional<std::filesystem::path>& ExplicitDirectory,
		const std::filesystem::path& PackagedDefaultDirectory);

	FOfflineBundleLoadResult LoadOfflineBundle(
		const std::filesystem::path& Directory,
		const FOfflineBundleLoadOptions& Options = {});

	FOfflineBundleLoadResult LoadSelectedOfflineBundle(
		const std::optional<std::filesystem::path>& ExplicitDirectory,
		const std::filesystem::path& PackagedDefaultDirectory,
		const FOfflineBundleLoadOptions& Options = {});
}
