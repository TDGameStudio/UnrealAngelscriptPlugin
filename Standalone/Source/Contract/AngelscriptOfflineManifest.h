#pragma once

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace AngelscriptStandalone
{
	enum class EOfflineBundleKind
	{
		DefaultEngine,
		Project,
	};

	struct FOfflineScope
	{
		bool bComplete = false;
		std::string State;
		std::vector<std::string> Included;
		std::vector<std::string> Excluded;
		std::vector<std::string> Skipped;
		std::vector<std::string> Diagnostics;
	};

	struct FOfflineFileDescriptor
	{
		std::string Name;
		std::string Sha256;
		std::uint64_t RecordCount = 0;
		std::uint64_t ByteCount = 0;
	};

	struct FOfflineAdapterDescriptor
	{
		std::string StableId;
		std::string Name;
		std::string Version;
		std::string SurfaceHash;
		bool bDeclarativeOnly = true;
		std::vector<std::string> RequiredTraits;
		std::vector<std::string> RequiredEngineProperties;
	};

	struct FOfflineManifest
	{
		int SchemaMajor = 0;
		int SchemaMinor = 0;
		EOfflineBundleKind BundleKind = EOfflineBundleKind::Project;
		std::string BundleIdentity;
		std::string ProducerName;
		std::string ProducerVersion;
		std::string UnrealVersion;
		std::string PluginVersion;
		std::string ForkVersion;
		std::string CompilerContractVersion;
		std::string Platform;
		std::string Configuration;
		FOfflineScope SymbolScope;
		FOfflineScope AssetScope;
		std::map<std::string, std::string> EngineProperties;
		std::map<std::string, bool> FeatureFlags;
		std::vector<std::string> LoadedModules;
		std::vector<std::string> LoadedPlugins;
		std::vector<std::string> RequiredFields;
		std::vector<FOfflineAdapterDescriptor> Adapters;
		std::vector<FOfflineFileDescriptor> Files;
	};

	struct FOfflineManifestCompatibility
	{
		int SupportedSchemaMajor = 1;
		int SupportedSchemaMinor = 0;
		std::set<std::string> SupportedRequiredFields = {
			"manifest.schema",
			"manifest.symbolScope",
			"records.stableId",
		};
		std::string ExpectedForkVersion;
		std::string ExpectedCompilerContractVersion;
		std::map<std::string, std::string> RequiredEngineProperties;
	};

	struct FOfflineManifestParseResult
	{
		bool bSuccess = false;
		std::string Error;
		FOfflineManifest Manifest;
	};

	FOfflineManifestParseResult ParseOfflineManifest(
		std::string_view Utf8Json,
		const FOfflineManifestCompatibility& Compatibility = {});
}
