#pragma once

#include "Contract/AngelscriptOfflineManifest.h"
#include "Contract/AngelscriptOfflineRecords.h"

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace AngelscriptStandalone
{
	class FOfflineBundleIndices
	{
	public:
		static std::shared_ptr<const FOfflineBundleIndices> Build(
			std::vector<FOfflineSymbolRecord> Symbols,
			std::vector<FOfflineAssetRecord> Assets,
			std::vector<FOfflineAdapterDescriptor> Adapters,
			std::string& OutError);

		const std::vector<FOfflineSymbolRecord>& Symbols() const;
		const std::vector<FOfflineAssetRecord>& Assets() const;
		const std::vector<FOfflineAdapterDescriptor>& Adapters() const;

		const FOfflineSymbolRecord* FindSymbol(
			std::string_view StableId) const;
		const FOfflineSymbolRecord* FindType(
			std::string_view StableId) const;
		const FOfflineSymbolRecord* FindCallable(
			std::string_view StableId) const;
		const FOfflineAssetRecord* FindAsset(
			std::string_view StableId) const;
		const FOfflineAssetRecord* FindAssetByObjectPath(
			std::string_view ObjectPath) const;
		const FOfflineAssetRecord* FindAssetByGeneratedClassPath(
			std::string_view GeneratedClassPath) const;
		const FOfflineAdapterDescriptor* FindAdapter(
			std::string_view StableId) const;

		std::vector<const FOfflineSymbolRecord*> FindOwnedSymbols(
			std::string_view OwnerStableId) const;
		std::vector<const FOfflineSymbolRecord*> FindNamespaceSymbols(
			std::string_view Namespace) const;
		std::vector<const FOfflineSymbolRecord*> FindModuleSymbols(
			std::string_view StableModuleId) const;

	private:
		std::vector<FOfflineSymbolRecord> Symbols_;
		std::vector<FOfflineAssetRecord> Assets_;
		std::vector<FOfflineAdapterDescriptor> Adapters_;
		std::unordered_map<std::string, std::size_t> SymbolById_;
		std::unordered_map<std::string, std::size_t> TypeById_;
		std::unordered_map<std::string, std::size_t> CallableById_;
		std::unordered_map<std::string, std::size_t> AssetById_;
		std::unordered_map<std::string, std::size_t> AssetByObjectPath_;
		std::unordered_map<std::string, std::size_t>
			AssetByGeneratedClassPath_;
		std::unordered_map<std::string, std::size_t> AdapterById_;
		std::unordered_map<std::string, std::vector<std::size_t>>
			SymbolsByOwner_;
		std::unordered_map<std::string, std::vector<std::size_t>>
			SymbolsByNamespace_;
		std::unordered_map<std::string, std::vector<std::size_t>>
			SymbolsByModule_;
	};
}
