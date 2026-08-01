#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace AngelscriptStandalone
{
	enum class ECommand
	{
		None,
		Compile,
		Run,
		Help,
		Version,
	};

	enum class EDialect
	{
		Native,
		Unreal,
	};

	enum class EDiagnosticFormat
	{
		Text,
		Json,
	};

	struct FCommandLineOptions
	{
		ECommand Command = ECommand::None;
		EDialect Dialect = EDialect::Native;
		std::vector<std::filesystem::path> ScriptRoots;
		std::filesystem::path Entry;
		std::filesystem::path Bundle;
		std::filesystem::path OutputDirectory = "as-standalone-output";
		EDiagnosticFormat DiagnosticFormat = EDiagnosticFormat::Text;
		bool bEmitByteCode = false;
		bool bStrictResources = false;
		bool bAllowUERequired = false;
		int TimeoutMilliseconds = 5000;
		int MemoryLimitMegabytes = 256;
		std::vector<std::string> ScriptArguments;
	};

	struct FArgumentParseResult
	{
		bool bSuccess = false;
		int ExitCode = 2;
		std::string Error;
		FCommandLineOptions Options;
	};

	FArgumentParseResult ParseArguments(int ArgumentCount, char** Arguments);
	std::string GetHelpText();
}
