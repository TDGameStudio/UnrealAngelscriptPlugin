#include "Contract/AngelscriptOfflineIndices.h"

#include <algorithm>

namespace AngelscriptStandalone
{
	namespace
	{
		template <typename Record>
		bool StableIdLess(const Record& Left, const Record& Right)
		{
			return Left.StableId < Right.StableId;
		}

		std::vector<const FOfflineSymbolRecord*> ResolveSymbolIndices(
			const std::vector<FOfflineSymbolRecord>& Symbols,
			const std::vector<std::size_t>* Indices)
		{
			std::vector<const FOfflineSymbolRecord*> Result;
			if (Indices == nullptr)
			{
				return Result;
			}
			Result.reserve(Indices->size());
			for (const std::size_t Index : *Indices)
			{
				Result.push_back(&Symbols[Index]);
			}
			return Result;
		}
	}

	std::shared_ptr<const FOfflineBundleIndices>
	FOfflineBundleIndices::Build(
		std::vector<FOfflineSymbolRecord> Symbols,
		std::vector<FOfflineAssetRecord> Assets,
		std::vector<FOfflineAdapterDescriptor> Adapters,
		std::string& OutError)
	{
		std::sort(Symbols.begin(), Symbols.end(), StableIdLess<FOfflineSymbolRecord>);
		std::sort(Assets.begin(), Assets.end(), StableIdLess<FOfflineAssetRecord>);
		std::sort(
			Adapters.begin(),
			Adapters.end(),
			[](const auto& Left, const auto& Right)
			{
				return Left.StableId < Right.StableId;
			});

		auto Result = std::shared_ptr<FOfflineBundleIndices>(
			new FOfflineBundleIndices());
		Result->Symbols_ = std::move(Symbols);
		Result->Assets_ = std::move(Assets);
		Result->Adapters_ = std::move(Adapters);

		for (std::size_t Index = 0;
			Index < Result->Symbols_.size();
			++Index)
		{
			const FOfflineSymbolRecord& Symbol = Result->Symbols_[Index];
			if (!Result->SymbolById_.emplace(Symbol.StableId, Index).second)
			{
				OutError =
					"duplicate symbol stable ID in immutable index: "
					+ Symbol.StableId;
				return {};
			}
			if (!Symbol.Type.StableId.empty())
			{
				Result->TypeById_.emplace(Symbol.StableId, Index);
			}
			if (!Symbol.Callable.StableId.empty())
			{
				Result->CallableById_.emplace(Symbol.StableId, Index);
			}
			Result->SymbolsByNamespace_[
				std::string(Symbol.GetNamespace())].push_back(Index);
			if (!Symbol.GetOwnerStableId().empty())
			{
				Result->SymbolsByOwner_[
					std::string(Symbol.GetOwnerStableId())].push_back(Index);
			}
			if (!Symbol.Origin.StableModuleId.empty())
			{
				Result->SymbolsByModule_[
					Symbol.Origin.StableModuleId].push_back(Index);
			}
		}

		for (std::size_t Index = 0;
			Index < Result->Assets_.size();
			++Index)
		{
			const FOfflineAssetRecord& Asset = Result->Assets_[Index];
			if (!Result->AssetById_.emplace(Asset.StableId, Index).second)
			{
				OutError =
					"duplicate asset stable ID in immutable index: "
					+ Asset.StableId;
				return {};
			}
			if (!Result->AssetByObjectPath_.emplace(
					Asset.ObjectPath,
					Index).second)
			{
				OutError =
					"duplicate asset object path in immutable index: "
					+ Asset.ObjectPath;
				return {};
			}
			if (!Asset.GeneratedClassPath.empty()
				&& !Result->AssetByGeneratedClassPath_.emplace(
					Asset.GeneratedClassPath,
					Index).second)
			{
				OutError =
					"duplicate generated class path in immutable index: "
					+ Asset.GeneratedClassPath;
				return {};
			}
		}

		for (std::size_t Index = 0;
			Index < Result->Adapters_.size();
			++Index)
		{
			const FOfflineAdapterDescriptor& Adapter =
				Result->Adapters_[Index];
			if (!Result->AdapterById_.emplace(
					Adapter.StableId,
					Index).second)
			{
				OutError =
					"duplicate adapter stable ID in immutable index: "
					+ Adapter.StableId;
				return {};
			}
		}
		return Result;
	}

	const std::vector<FOfflineSymbolRecord>&
	FOfflineBundleIndices::Symbols() const
	{
		return Symbols_;
	}

	const std::vector<FOfflineAssetRecord>&
	FOfflineBundleIndices::Assets() const
	{
		return Assets_;
	}

	const std::vector<FOfflineAdapterDescriptor>&
	FOfflineBundleIndices::Adapters() const
	{
		return Adapters_;
	}

	const FOfflineSymbolRecord* FOfflineBundleIndices::FindSymbol(
		const std::string_view StableId) const
	{
		const auto Found = SymbolById_.find(std::string(StableId));
		return Found == SymbolById_.end()
			? nullptr
			: &Symbols_[Found->second];
	}

	const FOfflineSymbolRecord* FOfflineBundleIndices::FindType(
		const std::string_view StableId) const
	{
		const auto Found = TypeById_.find(std::string(StableId));
		return Found == TypeById_.end()
			? nullptr
			: &Symbols_[Found->second];
	}

	const FOfflineSymbolRecord* FOfflineBundleIndices::FindCallable(
		const std::string_view StableId) const
	{
		const auto Found = CallableById_.find(std::string(StableId));
		return Found == CallableById_.end()
			? nullptr
			: &Symbols_[Found->second];
	}

	const FOfflineAssetRecord* FOfflineBundleIndices::FindAsset(
		const std::string_view StableId) const
	{
		const auto Found = AssetById_.find(std::string(StableId));
		return Found == AssetById_.end()
			? nullptr
			: &Assets_[Found->second];
	}

	const FOfflineAssetRecord*
	FOfflineBundleIndices::FindAssetByObjectPath(
		const std::string_view ObjectPath) const
	{
		const auto Found =
			AssetByObjectPath_.find(std::string(ObjectPath));
		return Found == AssetByObjectPath_.end()
			? nullptr
			: &Assets_[Found->second];
	}

	const FOfflineAssetRecord*
	FOfflineBundleIndices::FindAssetByGeneratedClassPath(
		const std::string_view GeneratedClassPath) const
	{
		const auto Found = AssetByGeneratedClassPath_.find(
			std::string(GeneratedClassPath));
		return Found == AssetByGeneratedClassPath_.end()
			? nullptr
			: &Assets_[Found->second];
	}

	const FOfflineAdapterDescriptor* FOfflineBundleIndices::FindAdapter(
		const std::string_view StableId) const
	{
		const auto Found = AdapterById_.find(std::string(StableId));
		return Found == AdapterById_.end()
			? nullptr
			: &Adapters_[Found->second];
	}

	std::vector<const FOfflineSymbolRecord*>
	FOfflineBundleIndices::FindOwnedSymbols(
		const std::string_view OwnerStableId) const
	{
		const auto Found =
			SymbolsByOwner_.find(std::string(OwnerStableId));
		return ResolveSymbolIndices(
			Symbols_,
			Found == SymbolsByOwner_.end() ? nullptr : &Found->second);
	}

	std::vector<const FOfflineSymbolRecord*>
	FOfflineBundleIndices::FindNamespaceSymbols(
		const std::string_view Namespace) const
	{
		const auto Found =
			SymbolsByNamespace_.find(std::string(Namespace));
		return ResolveSymbolIndices(
			Symbols_,
			Found == SymbolsByNamespace_.end()
				? nullptr
				: &Found->second);
	}

	std::vector<const FOfflineSymbolRecord*>
	FOfflineBundleIndices::FindModuleSymbols(
		const std::string_view StableModuleId) const
	{
		const auto Found =
			SymbolsByModule_.find(std::string(StableModuleId));
		return ResolveSymbolIndices(
			Symbols_,
			Found == SymbolsByModule_.end() ? nullptr : &Found->second);
	}
}
