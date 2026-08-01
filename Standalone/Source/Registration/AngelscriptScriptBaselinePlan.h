#pragma once

#include "Contract/AngelscriptOfflineIndices.h"
#include "Compiler/Frontend/AngelscriptStandaloneFrontendSession.h"

#include <string>
#include <vector>

namespace AngelscriptStandalone
{
	struct FScriptBaselinePlan
	{
		bool bSuccess = false;
		std::string Error;
		std::vector<const FOfflineSymbolRecord*> IncludedSymbols;
		std::vector<const FOfflineSymbolRecord*> SuppressedBaselineSymbols;
		std::vector<std::string> ReplacedModuleIds;
	};

	FScriptBaselinePlan BuildScriptBaselinePlan(
		const FOfflineBundleIndices& Indices,
		const std::vector<AngelscriptStandalone::Frontend::FLanguageModule>&
			SourceClosure);
}
