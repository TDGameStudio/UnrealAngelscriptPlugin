#include "Contract/AngelscriptOfflineManifest.h"

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>

#include <algorithm>
#include <limits>
#include <set>

namespace AngelscriptStandalone
{
	namespace
	{
		using FJsonValue = rapidjson::Value;

		bool Fail(std::string& OutError, std::string Message)
		{
			OutError = std::move(Message);
			return false;
		}

		const FJsonValue* FindMember(
			const FJsonValue& Object,
			const char* Name)
		{
			if (!Object.IsObject())
			{
				return nullptr;
			}
			const auto Iterator = Object.FindMember(Name);
			return Iterator == Object.MemberEnd()
				? nullptr
				: &Iterator->value;
		}

		bool ReadRequiredString(
			const FJsonValue& Object,
			const char* Name,
			std::string& OutValue,
			std::string& OutError)
		{
			const FJsonValue* Value = FindMember(Object, Name);
			if (Value == nullptr || !Value->IsString())
			{
				return Fail(
					OutError,
					std::string("manifest field '") + Name
						+ "' must be a string");
			}
			OutValue.assign(Value->GetString(), Value->GetStringLength());
			if (OutValue.empty())
			{
				return Fail(
					OutError,
					std::string("manifest field '") + Name
						+ "' must not be empty");
			}
			return true;
		}

		bool ReadStringArray(
			const FJsonValue& Object,
			const char* Name,
			std::vector<std::string>& OutValues,
			std::string& OutError,
			const bool bRequired = false)
		{
			const FJsonValue* Value = FindMember(Object, Name);
			if (Value == nullptr)
			{
				return !bRequired
					|| Fail(
						OutError,
						std::string("manifest field '") + Name
							+ "' is required");
			}
			if (!Value->IsArray())
			{
				return Fail(
					OutError,
					std::string("manifest field '") + Name
						+ "' must be an array");
			}
			for (const FJsonValue& Item : Value->GetArray())
			{
				if (!Item.IsString())
				{
					return Fail(
						OutError,
						std::string("manifest field '") + Name
							+ "' contains a non-string value");
				}
				OutValues.emplace_back(
					Item.GetString(),
					Item.GetStringLength());
			}
			return true;
		}

		bool ReadScope(
			const FJsonValue& Root,
			const char* Name,
			FOfflineScope& OutScope,
			std::string& OutError)
		{
			const FJsonValue* Scope = FindMember(Root, Name);
			if (Scope == nullptr || !Scope->IsObject())
			{
				return Fail(
					OutError,
					std::string("manifest field '") + Name
						+ "' must be an object");
			}
			const FJsonValue* Complete = FindMember(*Scope, "complete");
			if (Complete == nullptr || !Complete->IsBool())
			{
				return Fail(
					OutError,
					std::string("manifest field '") + Name
						+ ".complete' must be a boolean");
			}
			OutScope.bComplete = Complete->GetBool();
			const FJsonValue* State = FindMember(*Scope, "state");
			if (State != nullptr)
			{
				if (!State->IsString())
				{
					return Fail(
						OutError,
						std::string("manifest field '") + Name
							+ ".state' must be a string");
				}
				OutScope.State.assign(
					State->GetString(),
					State->GetStringLength());
			}
			return ReadStringArray(
					*Scope,
					"included",
					OutScope.Included,
					OutError)
				&& ReadStringArray(
					*Scope,
					"excluded",
					OutScope.Excluded,
					OutError)
				&& ReadStringArray(
					*Scope,
					"skipped",
					OutScope.Skipped,
					OutError)
				&& ReadStringArray(
					*Scope,
					"diagnostics",
					OutScope.Diagnostics,
					OutError);
		}

		bool ReadStringMap(
			const FJsonValue& Root,
			const char* Name,
			std::map<std::string, std::string>& OutValues,
			std::string& OutError)
		{
			const FJsonValue* Object = FindMember(Root, Name);
			if (Object == nullptr || !Object->IsObject())
			{
				return Fail(
					OutError,
					std::string("manifest field '") + Name
						+ "' must be an object");
			}
			for (auto Iterator = Object->MemberBegin();
				Iterator != Object->MemberEnd();
				++Iterator)
			{
				if (!Iterator->value.IsString())
				{
					return Fail(
						OutError,
						std::string("manifest map '") + Name
							+ "' contains a non-string value");
				}
				OutValues.emplace(
					std::string(
						Iterator->name.GetString(),
						Iterator->name.GetStringLength()),
					std::string(
						Iterator->value.GetString(),
						Iterator->value.GetStringLength()));
			}
			return true;
		}

		bool ReadBoolMap(
			const FJsonValue& Root,
			const char* Name,
			std::map<std::string, bool>& OutValues,
			std::string& OutError)
		{
			const FJsonValue* Object = FindMember(Root, Name);
			if (Object == nullptr || !Object->IsObject())
			{
				return Fail(
					OutError,
					std::string("manifest field '") + Name
						+ "' must be an object");
			}
			for (auto Iterator = Object->MemberBegin();
				Iterator != Object->MemberEnd();
				++Iterator)
			{
				if (!Iterator->value.IsBool())
				{
					return Fail(
						OutError,
						std::string("manifest map '") + Name
							+ "' contains a non-boolean value");
				}
				OutValues.emplace(
					std::string(
						Iterator->name.GetString(),
						Iterator->name.GetStringLength()),
					Iterator->value.GetBool());
			}
			return true;
		}

		bool ReadFiles(
			const FJsonValue& Root,
			std::vector<FOfflineFileDescriptor>& OutFiles,
			std::string& OutError)
		{
			const FJsonValue* Files = FindMember(Root, "files");
			if (Files == nullptr || !Files->IsArray())
			{
				return Fail(OutError, "manifest field 'files' must be an array");
			}
			std::set<std::string> Names;
			for (const FJsonValue& Value : Files->GetArray())
			{
				if (!Value.IsObject())
				{
					return Fail(OutError, "manifest file record must be an object");
				}
				FOfflineFileDescriptor File;
				if (!ReadRequiredString(
						Value,
						"name",
						File.Name,
						OutError)
					|| !ReadRequiredString(
						Value,
						"sha256",
						File.Sha256,
						OutError))
				{
					return false;
				}
				const FJsonValue* RecordCount =
					FindMember(Value, "recordCount");
				const FJsonValue* ByteCount =
					FindMember(Value, "byteCount");
				if (RecordCount == nullptr
					|| !RecordCount->IsUint64()
					|| ByteCount == nullptr
					|| !ByteCount->IsUint64())
				{
					return Fail(
						OutError,
						"manifest file counts must be unsigned integers");
				}
				File.RecordCount = RecordCount->GetUint64();
				File.ByteCount = ByteCount->GetUint64();
				if (File.Sha256.size() != 64)
				{
					return Fail(
						OutError,
						"manifest file SHA-256 must contain 64 hexadecimal characters");
				}
				if (!Names.insert(File.Name).second)
				{
					return Fail(
						OutError,
						"manifest contains a duplicate file record");
				}
				OutFiles.emplace_back(std::move(File));
			}
			if (Names != std::set<std::string>{
				"assets.jsonl",
				"symbols.jsonl"})
			{
				return Fail(
					OutError,
					"manifest must describe exactly symbols.jsonl and assets.jsonl");
			}
			return true;
		}

		bool ReadAdapters(
			const FJsonValue& Root,
			std::vector<FOfflineAdapterDescriptor>& OutAdapters,
			std::string& OutError)
		{
			const FJsonValue* Adapters = FindMember(Root, "adapters");
			if (Adapters == nullptr || !Adapters->IsArray())
			{
				return Fail(
					OutError,
					"manifest field 'adapters' must be an array");
			}
			std::set<std::string> StableIds;
			for (const FJsonValue& Value : Adapters->GetArray())
			{
				if (!Value.IsObject())
				{
					return Fail(OutError, "adapter record must be an object");
				}
				FOfflineAdapterDescriptor Adapter;
				if (!ReadRequiredString(
						Value,
						"stableId",
						Adapter.StableId,
						OutError)
					|| !ReadRequiredString(
						Value,
						"name",
						Adapter.Name,
						OutError)
					|| !ReadRequiredString(
						Value,
						"version",
						Adapter.Version,
						OutError)
					|| !ReadRequiredString(
						Value,
						"surfaceHash",
						Adapter.SurfaceHash,
						OutError)
					|| !ReadStringArray(
						Value,
						"requiredTraits",
						Adapter.RequiredTraits,
						OutError)
					|| !ReadStringArray(
						Value,
						"requiredEngineProperties",
						Adapter.RequiredEngineProperties,
						OutError))
				{
					return false;
				}
				const FJsonValue* Declarative =
					FindMember(Value, "declarativeOnly");
				if (Declarative == nullptr || !Declarative->IsBool())
				{
					return Fail(
						OutError,
						"adapter declarativeOnly must be a boolean");
				}
				Adapter.bDeclarativeOnly = Declarative->GetBool();
				if (!StableIds.insert(Adapter.StableId).second)
				{
					return Fail(
						OutError,
						"manifest contains a duplicate adapter stable ID");
				}
				OutAdapters.emplace_back(std::move(Adapter));
			}
			std::sort(
				OutAdapters.begin(),
				OutAdapters.end(),
				[](const auto& Left, const auto& Right)
				{
					return Left.StableId < Right.StableId;
				});
			return true;
		}

		bool ValidateCompatibility(
			const FOfflineManifest& Manifest,
			const FOfflineManifestCompatibility& Compatibility,
			std::string& OutError)
		{
			if (Manifest.SchemaMajor
				!= Compatibility.SupportedSchemaMajor)
			{
				return Fail(OutError, "unsupported offline contract schema major");
			}
			if (Manifest.SchemaMinor
				> Compatibility.SupportedSchemaMinor)
			{
				return Fail(OutError, "unsupported offline contract schema minor");
			}
			if (!Manifest.SymbolScope.bComplete)
			{
				return Fail(
					OutError,
					"offline bundle symbol scope is incomplete");
			}
			for (const std::string& Required : Manifest.RequiredFields)
			{
				if (!Compatibility.SupportedRequiredFields.contains(Required))
				{
					return Fail(
						OutError,
						"unsupported required field: " + Required);
				}
			}
			if (!Compatibility.ExpectedForkVersion.empty()
				&& Manifest.ForkVersion
					!= Compatibility.ExpectedForkVersion)
			{
				return Fail(OutError, "incompatible AngelScript fork version");
			}
			if (!Compatibility.ExpectedCompilerContractVersion.empty()
				&& Manifest.CompilerContractVersion
					!= Compatibility.ExpectedCompilerContractVersion)
			{
				return Fail(OutError, "incompatible compiler contract version");
			}
			for (const auto& [Name, Expected]
				: Compatibility.RequiredEngineProperties)
			{
				const auto Found = Manifest.EngineProperties.find(Name);
				if (Found == Manifest.EngineProperties.end()
					|| Found->second != Expected)
				{
					return Fail(
						OutError,
						"incompatible engine property: " + Name);
				}
			}
			for (const FOfflineAdapterDescriptor& Adapter
				: Manifest.Adapters)
			{
				for (const std::string& Property
					: Adapter.RequiredEngineProperties)
				{
					if (!Manifest.EngineProperties.contains(Property))
					{
						return Fail(
							OutError,
							"adapter requires missing engine property: "
								+ Property);
					}
				}
			}
			return true;
		}
	}

	FOfflineManifestParseResult ParseOfflineManifest(
		const std::string_view Utf8Json,
		const FOfflineManifestCompatibility& Compatibility)
	{
		FOfflineManifestParseResult Result;
		if (Utf8Json.size() >= 3
			&& static_cast<unsigned char>(Utf8Json[0]) == 0xefu
			&& static_cast<unsigned char>(Utf8Json[1]) == 0xbbu
			&& static_cast<unsigned char>(Utf8Json[2]) == 0xbfu)
		{
			Result.Error = "manifest must be UTF-8 without BOM";
			return Result;
		}
		if (Utf8Json.find('\r') != std::string_view::npos)
		{
			Result.Error = "manifest must use LF line endings";
			return Result;
		}

		rapidjson::Document Document;
		Document.Parse<rapidjson::kParseValidateEncodingFlag>(
			Utf8Json.data(),
			Utf8Json.size());
		if (Document.HasParseError())
		{
			Result.Error =
				std::string("invalid manifest JSON at byte ")
				+ std::to_string(Document.GetErrorOffset())
				+ ": "
				+ rapidjson::GetParseError_En(Document.GetParseError());
			return Result;
		}
		if (!Document.IsObject())
		{
			Result.Error = "manifest root must be an object";
			return Result;
		}

		const FJsonValue* Schema = FindMember(Document, "schema");
		if (Schema == nullptr || !Schema->IsObject())
		{
			Result.Error = "manifest field 'schema' must be an object";
			return Result;
		}
		const FJsonValue* Major = FindMember(*Schema, "major");
		const FJsonValue* Minor = FindMember(*Schema, "minor");
		if (Major == nullptr
			|| !Major->IsInt()
			|| Minor == nullptr
			|| !Minor->IsInt()
			|| Major->GetInt() < 0
			|| Minor->GetInt() < 0)
		{
			Result.Error =
				"manifest schema major/minor must be non-negative integers";
			return Result;
		}
		Result.Manifest.SchemaMajor = Major->GetInt();
		Result.Manifest.SchemaMinor = Minor->GetInt();

		std::string Kind;
		if (!ReadRequiredString(
				Document,
				"bundleKind",
				Kind,
				Result.Error))
		{
			return Result;
		}
		if (Kind == "project")
		{
			Result.Manifest.BundleKind = EOfflineBundleKind::Project;
		}
		else if (Kind == "default-engine")
		{
			Result.Manifest.BundleKind =
				EOfflineBundleKind::DefaultEngine;
		}
		else
		{
			Result.Error = "unsupported offline bundle kind: " + Kind;
			return Result;
		}

		if (!ReadRequiredString(
				Document,
				"bundleIdentity",
				Result.Manifest.BundleIdentity,
				Result.Error)
			|| !ReadRequiredString(
				Document,
				"producerName",
				Result.Manifest.ProducerName,
				Result.Error)
			|| !ReadRequiredString(
				Document,
				"producerVersion",
				Result.Manifest.ProducerVersion,
				Result.Error)
			|| !ReadRequiredString(
				Document,
				"unrealVersion",
				Result.Manifest.UnrealVersion,
				Result.Error)
			|| !ReadRequiredString(
				Document,
				"pluginVersion",
				Result.Manifest.PluginVersion,
				Result.Error)
			|| !ReadRequiredString(
				Document,
				"forkVersion",
				Result.Manifest.ForkVersion,
				Result.Error)
			|| !ReadRequiredString(
				Document,
				"compilerContractVersion",
				Result.Manifest.CompilerContractVersion,
				Result.Error)
			|| !ReadRequiredString(
				Document,
				"platform",
				Result.Manifest.Platform,
				Result.Error)
			|| !ReadRequiredString(
				Document,
				"configuration",
				Result.Manifest.Configuration,
				Result.Error)
			|| !ReadScope(
				Document,
				"symbolScope",
				Result.Manifest.SymbolScope,
				Result.Error)
			|| !ReadScope(
				Document,
				"assetScope",
				Result.Manifest.AssetScope,
				Result.Error)
			|| !ReadStringMap(
				Document,
				"engineProperties",
				Result.Manifest.EngineProperties,
				Result.Error)
			|| !ReadBoolMap(
				Document,
				"featureFlags",
				Result.Manifest.FeatureFlags,
				Result.Error)
			|| !ReadStringArray(
				Document,
				"loadedModules",
				Result.Manifest.LoadedModules,
				Result.Error,
				true)
			|| !ReadStringArray(
				Document,
				"loadedPlugins",
				Result.Manifest.LoadedPlugins,
				Result.Error,
				true)
			|| !ReadStringArray(
				Document,
				"requiredFields",
				Result.Manifest.RequiredFields,
				Result.Error,
				true)
			|| !ReadAdapters(
				Document,
				Result.Manifest.Adapters,
				Result.Error)
			|| !ReadFiles(
				Document,
				Result.Manifest.Files,
				Result.Error)
			|| !ValidateCompatibility(
				Result.Manifest,
				Compatibility,
				Result.Error))
		{
			return Result;
		}

		std::sort(
			Result.Manifest.LoadedModules.begin(),
			Result.Manifest.LoadedModules.end());
		std::sort(
			Result.Manifest.LoadedPlugins.begin(),
			Result.Manifest.LoadedPlugins.end());
		Result.bSuccess = true;
		return Result;
	}
}
