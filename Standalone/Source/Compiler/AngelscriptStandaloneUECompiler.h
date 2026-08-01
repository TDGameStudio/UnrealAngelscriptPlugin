#pragma once

#include "Contract/AngelscriptOfflineBundleLoader.h"
#include "Compiler/AngelscriptStandaloneSemanticObserver.h"
#include "Host/AngelscriptStandaloneDiagnosticSink.h"
#include "Compiler/Frontend/AngelscriptStandaloneDeclarations.h"
#include "Registration/AngelscriptCapabilityClassification.h"
#include "Resources/AngelscriptResourceValidator.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace AngelscriptStandalone
{
	struct FUECompileRequest
	{
		std::vector<std::filesystem::path> ScriptRoots;
		std::filesystem::path Entry;
		std::optional<std::filesystem::path> ExplicitBundle;
		std::filesystem::path PackagedDefaultBundle;
		bool bEmitByteCode = false;
		bool bStrictResources = false;
		bool bAllowUERequired = false;
	};

	struct FUECompileResult
	{
		bool bSuccess = false;
		bool bInfrastructureFailure = false;
		std::string Error;
		std::string ModuleId;
		std::string LogicalEntryPath;
		std::string InputHash;
		std::string ProfileHash;
		std::string BundleIdentity;
		std::filesystem::path BundleDirectory;
		EOfflineBundleKind BundleKind = EOfflineBundleKind::Project;
		FOfflineScope SymbolScope;
		FOfflineScope AssetScope;
		bool bStrictResources = false;
		FCapabilitySummary CapabilitySummary;
		std::vector<FCapabilityObservation> Capabilities;
		std::vector<std::string> SourceModuleIds;
		std::vector<std::string> ReplacedBaselineModuleIds;
		std::vector<AngelscriptStandalone::Frontend::FDeclaration> Declarations;
		std::vector<FSemanticObservation> SemanticObservations;
		std::vector<FResourceValidation> Resources;
		std::vector<std::uint8_t> ByteCode;
		std::vector<FDiagnostic> Diagnostics;
	};

	class FUECompiler
	{
	public:
		FUECompileResult Compile(const FUECompileRequest& Request) const;
		static std::string GetProfileHash();
	};
}
