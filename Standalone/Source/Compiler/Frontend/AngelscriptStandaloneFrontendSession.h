#pragma once

#include "Compiler/Frontend/AngelscriptStandaloneDeclarations.h"
#include "Compiler/Frontend/AngelscriptStandaloneRewrite.h"

#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace AngelscriptStandalone::Frontend
{
	enum class EDiagnosticSeverity
	{
		Info,
		Warning,
		Error,
	};

	struct FDiagnostic
	{
		std::string Code;
		std::string Message;
		std::string LogicalPath;
		FSourceSpan Span;
		EDiagnosticSeverity Severity = EDiagnosticSeverity::Error;
	};

	struct FImport
	{
		std::string ModuleName;
		FSourceSpan Span;
	};

	struct FPreprocessConfig
	{
		std::unordered_map<std::string, bool> Flags;
	};

	struct FPreprocessResult
	{
		bool bSuccess = false;
		std::string LogicalPath;
		std::string ModuleName;
		std::string ModuleId;
		std::string ProcessedSource;
		FSourceMap SourceMap;
		std::vector<FImport> Imports;
		std::vector<FDeclaration> Declarations;
		std::vector<FDiagnostic> Diagnostics;
	};

	enum class EFrontendStage
	{
		Empty,
		SourcesAdded,
		ChunksProcessed,
		CodePostProcessed,
		Completed,
	};

	struct FLanguageModule
	{
		std::string LogicalPath;
		std::string ModuleName;
		std::string ModuleId;
		std::string ProcessedSource;
		FSourceMap SourceMap;
		std::vector<std::string> ImportedModules;
		std::vector<FDeclaration> Declarations;
	};

	struct FFrontendSessionResult
	{
		bool bSuccess = false;
		EFrontendStage Stage = EFrontendStage::Empty;
		std::vector<FLanguageModule> Modules;
		std::vector<FDiagnostic> Diagnostics;
	};

	FPreprocessResult PreprocessSource(
		const FSourceInput& Source,
		const FPreprocessConfig& Config);

	class FFrontendSession
	{
	public:
		explicit FFrontendSession(FPreprocessConfig Config = {});

		FPathResult AddSource(FSourceInput Source);
		FFrontendSessionResult Process();
		EFrontendStage GetStage() const;

	private:
		FPreprocessConfig Config;
		std::map<std::string, FSourceInput> SourcesByModule;
		EFrontendStage Stage = EFrontendStage::Empty;
	};
}
