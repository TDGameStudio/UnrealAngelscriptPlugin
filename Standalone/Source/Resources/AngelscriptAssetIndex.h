#pragma once

#include "Contract/AngelscriptOfflineManifest.h"
#include "Contract/AngelscriptOfflineRecords.h"

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace AngelscriptStandalone
{
	struct FAssetLookup
	{
		const FOfflineAssetRecord* Asset = nullptr;
		std::string OriginalPath;
		std::string NormalizedPath;
		std::string FinalPath;
		bool bRedirected = false;
		bool bAuthoritativelyCovered = false;
	};

	class FAssetIndex
	{
	public:
		static std::shared_ptr<const FAssetIndex> Build(
			const std::vector<FOfflineAssetRecord>& Assets,
			FOfflineScope Scope,
			std::string& OutError);

		FAssetLookup Lookup(std::string_view Path) const;
		bool IsAuthoritativelyCovered(std::string_view Path) const;
		const FOfflineScope& Scope() const;

	private:
		std::vector<FOfflineAssetRecord> Assets_;
		FOfflineScope Scope_;
		std::map<std::string, std::size_t> AssetByPath_;
		std::map<std::string, std::string> Redirects_;
	};
}
