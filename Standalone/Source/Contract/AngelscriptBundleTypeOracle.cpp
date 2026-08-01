#include "Contract/AngelscriptBundleTypeOracle.h"

#include <algorithm>
#include <cctype>

namespace AngelscriptStandalone
{
	namespace
	{
		std::string NormalizeTypeName(std::string_view Name)
		{
			while (!Name.empty()
				&& std::isspace(
					static_cast<unsigned char>(Name.front())) != 0)
			{
				Name.remove_prefix(1);
			}
			while (!Name.empty()
				&& std::isspace(
					static_cast<unsigned char>(Name.back())) != 0)
			{
				Name.remove_suffix(1);
			}

			std::string Result;
			Result.reserve(Name.size());
			bool bPendingSpace = false;
			for (const char Character : Name)
			{
				if (std::isspace(
						static_cast<unsigned char>(Character)) != 0)
				{
					bPendingSpace = true;
					continue;
				}
				if (bPendingSpace
					&& !Result.empty()
					&& Result.back() != ':'
					&& Character != ':'
					&& Result.back() != '<'
					&& Character != '>'
					&& Result.back() != ','
					&& Character != ',')
				{
					Result.push_back(' ');
				}
				bPendingSpace = false;
				Result.push_back(Character);
			}
			return Result;
		}

		void AddName(
			std::map<std::string, std::vector<std::string>>& Index,
			const std::string_view Name,
			const std::string_view StableId)
		{
			if (Name.empty() || StableId.empty())
			{
				return;
			}
			std::vector<std::string>& Ids =
				Index[NormalizeTypeName(Name)];
			const std::string Id(StableId);
			if (std::find(Ids.begin(), Ids.end(), Id) == Ids.end())
			{
				Ids.push_back(Id);
				std::sort(Ids.begin(), Ids.end());
			}
		}
	}

	FBundleTypeOracle::FBundleTypeOracle(
		const FOfflineBundleIndices& Indices)
	{
		for (const FOfflineSymbolRecord& Symbol : Indices.Symbols())
		{
			if (Symbol.Type.StableId.empty()
				|| Symbol.Type.Name.empty())
			{
				continue;
			}
			AddName(
				TypeIdsByName_,
				Symbol.Type.Name,
				Symbol.Type.StableId);
			if (!Symbol.Type.Namespace.empty())
			{
				AddName(
					TypeIdsByName_,
					Symbol.Type.Namespace + "::" + Symbol.Type.Name,
					Symbol.Type.StableId);
			}
		}
	}

	bool FBundleTypeOracle::ResolveType(
		const std::string_view TypeSpelling,
		std::string& OutStableTypeId) const
	{
		const auto Found =
			TypeIdsByName_.find(NormalizeTypeName(TypeSpelling));
		if (Found == TypeIdsByName_.end()
			|| Found->second.size() != 1)
		{
			return false;
		}
		OutStableTypeId = Found->second.front();
		return true;
	}
}
