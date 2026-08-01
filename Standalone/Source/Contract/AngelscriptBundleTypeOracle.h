#pragma once

#include "Contract/AngelscriptOfflineIndices.h"
#include "Compiler/Frontend/AngelscriptStandaloneDeclarations.h"

#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace AngelscriptStandalone
{
	class FBundleTypeOracle final
		: public AngelscriptStandalone::Frontend::ITypeOracle
	{
	public:
		explicit FBundleTypeOracle(const FOfflineBundleIndices& Indices);

		bool ResolveType(
			std::string_view TypeSpelling,
			std::string& OutStableTypeId) const override;

	private:
		std::map<std::string, std::vector<std::string>> TypeIdsByName_;
	};
}
