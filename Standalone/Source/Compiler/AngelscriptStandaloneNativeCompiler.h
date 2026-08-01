#pragma once

#include "Host/AngelscriptStandaloneDiagnosticSink.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace AngelscriptStandalone
{
	struct FCompileRequest
	{
		std::vector<std::filesystem::path> ScriptRoots;
		std::filesystem::path Entry;
		bool bEmitByteCode = false;
	};

	struct FCompileResult
	{
		bool bSuccess = false;
		std::string Error;
		std::string ModuleId;
		std::string LogicalEntryPath;
		std::string InputHash;
		std::string ProfileHash;
		std::vector<std::uint8_t> ByteCode;
		std::vector<FDiagnostic> Diagnostics;
	};

	class FNativeCompiler
	{
	public:
		FCompileResult Compile(const FCompileRequest& Request) const;
		static std::string GetProfileHash();
	};
}
