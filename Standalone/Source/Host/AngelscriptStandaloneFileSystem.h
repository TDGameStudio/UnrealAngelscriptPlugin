#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace AngelscriptStandalone
{
	struct FResolvedSource
	{
		std::filesystem::path AbsolutePath;
		std::string LogicalPath;
		std::string Contents;
	};

	struct FSourceResolveResult
	{
		bool bSuccess = false;
		FResolvedSource Source;
		std::string Error;
	};

	std::string NormalizeLogicalPath(const std::filesystem::path& Path);
	bool IsValidUtf8(const std::string& Text);
	FSourceResolveResult ResolveAndReadEntry(
		const std::vector<std::filesystem::path>& ScriptRoots,
		const std::filesystem::path& Entry);
}
