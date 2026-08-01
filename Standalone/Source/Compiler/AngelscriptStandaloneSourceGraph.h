#pragma once

#include "Compiler/Frontend/AngelscriptStandaloneFrontendSession.h"

#include <filesystem>
#include <string>
#include <vector>

namespace AngelscriptStandalone
{
	struct FStandaloneSourceCollectionResult
	{
		bool bSuccess = false;
		std::string Error;
		std::vector<AngelscriptStandalone::Frontend::FSourceInput> Sources;
	};

	struct FStandaloneSourceGraphRequest
	{
		std::vector<AngelscriptStandalone::Frontend::FSourceInput> Sources;
		std::string Entry;
		AngelscriptStandalone::Frontend::FPreprocessConfig Config;
	};

	struct FStandaloneSourceGraphResult
	{
		bool bSuccess = false;
		std::string Error;
		std::string EntryModule;
		std::vector<AngelscriptStandalone::Frontend::FLanguageModule> Modules;
		std::vector<AngelscriptStandalone::Frontend::FDiagnostic> Diagnostics;
	};

	FStandaloneSourceCollectionResult CollectStandaloneSources(
		const std::vector<std::filesystem::path>& ScriptRoots);

	FStandaloneSourceGraphResult BuildStandaloneSourceGraph(
		const FStandaloneSourceGraphRequest& Request);
}
