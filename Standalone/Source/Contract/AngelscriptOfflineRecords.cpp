#include "Contract/AngelscriptOfflineRecords.h"

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>

#include <algorithm>
#include <cctype>

namespace AngelscriptStandalone
{
	namespace
	{
		using FJsonValue = rapidjson::Value;

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

		bool ReadString(
			const FJsonValue& Object,
			const char* Name,
			std::string& Out,
			std::string& OutError,
			const bool bAllowEmpty)
		{
			const FJsonValue* Value = FindMember(Object, Name);
			if (Value == nullptr || !Value->IsString())
			{
				OutError =
					std::string("record field '") + Name
					+ "' must be a string";
				return false;
			}
			Out.assign(Value->GetString(), Value->GetStringLength());
			if (!bAllowEmpty && Out.empty())
			{
				OutError =
					std::string("record field '") + Name
					+ "' must not be empty";
				return false;
			}
			return true;
		}

		bool ReadStringArray(
			const FJsonValue& Object,
			const char* Name,
			std::vector<std::string>& Out,
			std::string& OutError)
		{
			const FJsonValue* Array = FindMember(Object, Name);
			if (Array == nullptr || !Array->IsArray())
			{
				OutError =
					std::string("record field '") + Name
					+ "' must be an array";
				return false;
			}
			for (const FJsonValue& Value : Array->GetArray())
			{
				if (!Value.IsString())
				{
					OutError =
						std::string("record field '") + Name
						+ "' contains a non-string";
					return false;
				}
				Out.emplace_back(
					Value.GetString(),
					Value.GetStringLength());
			}
			return true;
		}

		bool ReadOptionalString(
			const FJsonValue& Object,
			const char* Name,
			std::string& Out,
			std::string& OutError)
		{
			const FJsonValue* Value = FindMember(Object, Name);
			if (Value == nullptr)
			{
				return true;
			}
			if (!Value->IsString())
			{
				OutError =
					std::string("record field '") + Name
					+ "' must be a string";
				return false;
			}
			Out.assign(Value->GetString(), Value->GetStringLength());
			return true;
		}

		bool ReadOptionalBool(
			const FJsonValue& Object,
			const char* Name,
			bool& Out,
			std::string& OutError)
		{
			const FJsonValue* Value = FindMember(Object, Name);
			if (Value == nullptr)
			{
				return true;
			}
			if (!Value->IsBool())
			{
				OutError =
					std::string("record field '") + Name
					+ "' must be a boolean";
				return false;
			}
			Out = Value->GetBool();
			return true;
		}

		bool ReadOptionalInt64(
			const FJsonValue& Object,
			const char* Name,
			std::int64_t& Out,
			std::string& OutError)
		{
			const FJsonValue* Value = FindMember(Object, Name);
			if (Value == nullptr)
			{
				return true;
			}
			if (!Value->IsInt64())
			{
				OutError =
					std::string("record field '") + Name
					+ "' must be an integer";
				return false;
			}
			Out = Value->GetInt64();
			return true;
		}

		bool ReadOptionalStringArray(
			const FJsonValue& Object,
			const char* Name,
			std::vector<std::string>& Out,
			std::string& OutError)
		{
			return FindMember(Object, Name) == nullptr
				|| ReadStringArray(Object, Name, Out, OutError);
		}

		bool ReadTypeTraits(
			const FJsonValue& Type,
			FOfflineTypeRecord::FTraits& Out,
			std::string& OutError)
		{
			const FJsonValue* Traits = FindMember(Type, "traits");
			if (Traits == nullptr)
			{
				return true;
			}
			if (!Traits->IsObject())
			{
				OutError = "record field 'traits' must be an object";
				return false;
			}
			return ReadOptionalBool(
					*Traits,
					"constructible",
					Out.bConstructible,
					OutError)
				&& ReadOptionalBool(
					*Traits,
					"destructible",
					Out.bDestructible,
					OutError)
				&& ReadOptionalBool(
					*Traits,
					"copyConstructible",
					Out.bCopyConstructible,
					OutError)
				&& ReadOptionalBool(
					*Traits,
					"copyAssignable",
					Out.bCopyAssignable,
					OutError)
				&& ReadOptionalBool(
					*Traits,
					"comparable",
					Out.bComparable,
					OutError)
				&& ReadOptionalBool(
					*Traits,
					"hashable",
					Out.bHashable,
					OutError)
				&& ReadOptionalBool(
					*Traits,
					"garbageCollected",
					Out.bGarbageCollected,
					OutError)
				&& ReadOptionalBool(
					*Traits,
					"templateEligible",
					Out.bTemplateEligible,
					OutError);
		}

		bool ReadEnumValues(
			const FJsonValue& Type,
			std::vector<FOfflineTypeRecord::FEnumValue>& Out,
			std::string& OutError)
		{
			const FJsonValue* Values = FindMember(Type, "enumValues");
			if (Values == nullptr)
			{
				return true;
			}
			if (!Values->IsArray())
			{
				OutError = "record field 'enumValues' must be an array";
				return false;
			}
			for (const FJsonValue& Value : Values->GetArray())
			{
				if (!Value.IsObject())
				{
					OutError = "enum value must be an object";
					return false;
				}
				FOfflineTypeRecord::FEnumValue& Item = Out.emplace_back();
				if (!ReadString(
						Value,
						"stableId",
						Item.StableId,
						OutError,
						true)
					|| !ReadString(
						Value,
						"name",
						Item.Name,
						OutError,
						false)
					|| !ReadOptionalInt64(
						Value,
						"value",
						Item.Value,
						OutError))
				{
					return false;
				}
			}
			return true;
		}

		bool ReadParameters(
			const FJsonValue& Callable,
			std::vector<FOfflineCallableRecord::FParameter>& Out,
			std::string& OutError)
		{
			const FJsonValue* Parameters = FindMember(Callable, "parameters");
			if (Parameters == nullptr)
			{
				return true;
			}
			if (!Parameters->IsArray())
			{
				OutError = "record field 'parameters' must be an array";
				return false;
			}
			for (const FJsonValue& Value : Parameters->GetArray())
			{
				if (!Value.IsObject())
				{
					OutError = "callable parameter must be an object";
					return false;
				}
				FOfflineCallableRecord::FParameter& Parameter =
					Out.emplace_back();
				if (!ReadString(
						Value,
						"name",
						Parameter.Name,
						OutError,
						true)
					|| !ReadString(
						Value,
						"type",
						Parameter.TypeDeclaration,
						OutError,
						false)
					|| !ReadOptionalString(
						Value,
						"direction",
						Parameter.Direction,
						OutError)
					|| !ReadOptionalString(
						Value,
						"defaultExpression",
						Parameter.DefaultExpression,
						OutError)
					|| !ReadOptionalString(
						Value,
						"resourceKind",
						Parameter.ResourceKind,
						OutError)
					|| !ReadOptionalString(
						Value,
						"resourceTypeStableId",
						Parameter.ResourceTypeStableId,
						OutError)
					|| !ReadOptionalBool(
						Value,
						"hasDefault",
						Parameter.bHasDefault,
						OutError)
					|| !ReadOptionalBool(
						Value,
						"const",
						Parameter.bConst,
						OutError)
					|| !ReadOptionalBool(
						Value,
						"reference",
						Parameter.bReference,
						OutError)
					|| !ReadOptionalBool(
						Value,
						"handle",
						Parameter.bHandle,
						OutError))
				{
					return false;
				}
			}
			return true;
		}

		bool IsStableId(const std::string& Value)
		{
			return Value.size() == 64
				&& std::all_of(
					Value.begin(),
					Value.end(),
					[](const unsigned char Character)
					{
						return std::isxdigit(Character) != 0;
					});
		}

		bool ParseDocument(
			const std::string_view Json,
			rapidjson::Document& OutDocument,
			std::string& OutError)
		{
			if (Json.empty())
			{
				OutError = "empty JSONL record";
				return false;
			}
			if (Json.find('\r') != std::string_view::npos
				|| Json.find('\n') != std::string_view::npos)
			{
				OutError = "JSONL parser expects exactly one LF-free record";
				return false;
			}
			OutDocument.Parse<rapidjson::kParseValidateEncodingFlag>(
				Json.data(),
				Json.size());
			if (OutDocument.HasParseError())
			{
				OutError =
					std::string("invalid JSONL record at byte ")
					+ std::to_string(OutDocument.GetErrorOffset())
					+ ": "
					+ rapidjson::GetParseError_En(
						OutDocument.GetParseError());
				return false;
			}
			if (!OutDocument.IsObject())
			{
				OutError = "JSONL record root must be an object";
				return false;
			}
			std::string Schema;
			if (!ReadString(
					OutDocument,
					"schema",
					Schema,
					OutError,
					false)
				|| Schema != "1.0")
			{
				if (OutError.empty())
				{
					OutError = "unsupported JSONL record schema";
				}
				return false;
			}
			return true;
		}

		bool ReadOrigin(
			const FJsonValue& Object,
			FOfflineOrigin& Out,
			std::string& OutError)
		{
			const FJsonValue* Origin = FindMember(Object, "origin");
			return Origin != nullptr
				&& Origin->IsObject()
				&& ReadString(
					*Origin,
					"layer",
					Out.Layer,
					OutError,
					false)
				&& ReadString(
					*Origin,
					"kind",
					Out.Kind,
					OutError,
					false)
				&& ReadString(
					*Origin,
					"module",
					Out.Module,
					OutError,
					true)
				&& ReadString(
					*Origin,
					"plugin",
					Out.Plugin,
					OutError,
					true)
				&& ReadString(
					*Origin,
					"stableModuleId",
					Out.StableModuleId,
					OutError,
					true);
		}

		bool ParseType(
			const FJsonValue& Root,
			FOfflineTypeRecord& Out,
			std::string& OutError)
		{
			const FJsonValue* Type = FindMember(Root, "type");
			return Type != nullptr
				&& Type->IsObject()
				&& ReadString(
					*Type,
					"stableId",
					Out.StableId,
					OutError,
					false)
				&& ReadString(
					*Type,
					"kind",
					Out.Kind,
					OutError,
					false)
				&& ReadString(
					*Type,
					"name",
					Out.Name,
					OutError,
					false)
				&& ReadString(
					*Type,
					"namespace",
					Out.Namespace,
					OutError,
					true)
				&& ReadString(
					*Type,
					"completeDeclaration",
					Out.CompleteDeclaration,
					OutError,
					false)
				&& ReadString(
					*Type,
					"baseStableId",
					Out.BaseStableId,
					OutError,
					true)
				&& ReadStringArray(
					*Type,
					"interfaces",
					Out.InterfaceStableIds,
					OutError)
				&& ReadStringArray(
					*Type,
					"members",
					Out.MemberStableIds,
					OutError)
				&& ReadOptionalStringArray(
					*Type,
					"templateSubtypes",
					Out.TemplateSubtypeDeclarations,
					OutError)
				&& ReadOptionalStringArray(
					*Type,
					"flags",
					Out.Flags,
					OutError)
				&& ReadEnumValues(
					*Type,
					Out.EnumValues,
					OutError)
				&& ReadString(
					*Type,
					"adapterStableId",
					Out.AdapterStableId,
					OutError,
					true)
				&& ReadString(
					*Type,
					"ueTypePath",
					Out.UETypePath,
					OutError,
					true)
				&& ReadOptionalString(
					*Type,
					"availability",
					Out.Availability,
					OutError)
				&& ReadTypeTraits(
					*Type,
					Out.Traits,
					OutError)
				&& ReadOptionalInt64(
					*Type,
					"compileSize",
					Out.CompileSize,
					OutError)
				&& ReadOptionalInt64(
					*Type,
					"compileAlignment",
					Out.CompileAlignment,
					OutError)
				&& ReadOptionalBool(
					*Type,
					"handle",
					Out.bHandle,
					OutError)
				&& ReadOptionalBool(
					*Type,
					"templateDefinition",
					Out.bTemplateDefinition,
					OutError);
		}

		bool ParseCallable(
			const FJsonValue& Root,
			FOfflineCallableRecord& Out,
			std::string& OutError)
		{
			const FJsonValue* Callable = FindMember(Root, "callable");
			return Callable != nullptr
				&& Callable->IsObject()
				&& ReadString(
					*Callable,
					"stableId",
					Out.StableId,
					OutError,
					false)
				&& ReadString(
					*Callable,
					"kind",
					Out.Kind,
					OutError,
					false)
				&& ReadString(
					*Callable,
					"name",
					Out.Name,
					OutError,
					false)
				&& ReadString(
					*Callable,
					"namespace",
					Out.Namespace,
					OutError,
					true)
				&& ReadString(
					*Callable,
					"ownerStableId",
					Out.OwnerStableId,
					OutError,
					true)
				&& ReadString(
					*Callable,
					"declaration",
					Out.Declaration,
					OutError,
					false)
				&& ReadString(
					*Callable,
					"returnType",
					Out.ReturnType,
					OutError,
					false)
				&& ReadOptionalString(
					*Callable,
					"access",
					Out.Access,
					OutError)
				&& ReadOptionalString(
					*Callable,
					"behavior",
					Out.Behavior,
					OutError)
				&& ReadOptionalString(
					*Callable,
					"availability",
					Out.Availability,
					OutError)
				&& ReadParameters(
					*Callable,
					Out.Parameters,
					OutError)
				&& ReadString(
					*Callable,
					"adapterStableId",
					Out.AdapterStableId,
					OutError,
					true)
				&& ReadString(
					*Callable,
					"ueFunctionPath",
					Out.UEFunctionPath,
					OutError,
					true)
				&& ReadOptionalBool(
					*Callable,
					"const",
					Out.bConst,
					OutError)
				&& ReadOptionalBool(
					*Callable,
					"static",
					Out.bStatic,
					OutError)
				&& ReadOptionalBool(
					*Callable,
					"propertyAccessor",
					Out.bPropertyAccessor,
					OutError)
				&& ReadOptionalBool(
					*Callable,
					"final",
					Out.bFinal,
					OutError)
				&& ReadOptionalBool(
					*Callable,
					"override",
					Out.bOverride,
					OutError);
		}

		bool ParseProperty(
			const FJsonValue& Root,
			FOfflinePropertyRecord& Out,
			std::string& OutError)
		{
			const FJsonValue* Property = FindMember(Root, "property");
			return Property != nullptr
				&& Property->IsObject()
				&& ReadString(
					*Property,
					"stableId",
					Out.StableId,
					OutError,
					false)
				&& ReadString(
					*Property,
					"name",
					Out.Name,
					OutError,
					false)
				&& ReadString(
					*Property,
					"namespace",
					Out.Namespace,
					OutError,
					true)
				&& ReadString(
					*Property,
					"ownerStableId",
					Out.OwnerStableId,
					OutError,
					true)
				&& ReadString(
					*Property,
					"type",
					Out.TypeDeclaration,
					OutError,
					false)
				&& ReadString(
					*Property,
					"completeDeclaration",
					Out.Declaration,
					OutError,
					false)
				&& ReadOptionalString(
					*Property,
					"access",
					Out.Access,
					OutError)
				&& ReadOptionalString(
					*Property,
					"availability",
					Out.Availability,
					OutError)
				&& ReadString(
					*Property,
					"adapterStableId",
					Out.AdapterStableId,
					OutError,
					true)
				&& ReadString(
					*Property,
					"uePropertyPath",
					Out.UEPropertyPath,
					OutError,
					true)
				&& ReadOptionalBool(
					*Property,
					"const",
					Out.bConst,
					OutError)
				&& ReadOptionalBool(
					*Property,
					"static",
					Out.bStatic,
					OutError)
				&& ReadOptionalBool(
					*Property,
					"readable",
					Out.bReadable,
					OutError)
				&& ReadOptionalBool(
					*Property,
					"writable",
					Out.bWritable,
					OutError);
		}
	}

	std::string_view FOfflineSymbolRecord::GetNamespace() const
	{
		if (!Type.StableId.empty())
		{
			return Type.Namespace;
		}
		if (!Callable.StableId.empty())
		{
			return Callable.Namespace;
		}
		return Property.Namespace;
	}

	std::string_view FOfflineSymbolRecord::GetOwnerStableId() const
	{
		if (!Callable.StableId.empty())
		{
			return Callable.OwnerStableId;
		}
		return Property.OwnerStableId;
	}

	std::string_view FOfflineSymbolRecord::GetName() const
	{
		if (!Type.StableId.empty())
		{
			return Type.Name;
		}
		if (!Callable.StableId.empty())
		{
			return Callable.Name;
		}
		return Property.Name;
	}

	std::string_view FOfflineSymbolRecord::GetAdapterStableId() const
	{
		if (!Type.StableId.empty())
		{
			return Type.AdapterStableId;
		}
		if (!Callable.StableId.empty())
		{
			return Callable.AdapterStableId;
		}
		return Property.AdapterStableId;
	}

	TOfflineRecordParseResult<FOfflineSymbolRecord>
		ParseOfflineSymbolRecord(const std::string_view Utf8Json)
	{
		TOfflineRecordParseResult<FOfflineSymbolRecord> Result;
		rapidjson::Document Document;
		if (!ParseDocument(Utf8Json, Document, Result.Error)
			|| !ReadString(
				Document,
				"stableId",
				Result.Record.StableId,
				Result.Error,
				false)
			|| !ReadString(
				Document,
				"kind",
				Result.Record.Kind,
				Result.Error,
				false)
			|| !ReadString(
				Document,
				"canonicalIdentity",
				Result.Record.CanonicalIdentity,
				Result.Error,
				false)
			|| !ReadOrigin(
				Document,
				Result.Record.Origin,
				Result.Error))
		{
			if (Result.Error.empty())
			{
				Result.Error = "symbol origin must be an object";
			}
			return Result;
		}
		if (!IsStableId(Result.Record.StableId))
		{
			Result.Error = "symbol stable ID must be a 64-character hexadecimal value";
			return Result;
		}

		const bool bType =
			Result.Record.Kind == "type"
			|| Result.Record.Kind == "typedef"
			|| Result.Record.Kind == "funcdef"
			|| Result.Record.Kind == "delegate";
		const bool bCallable = Result.Record.Kind == "callable";
		const bool bProperty =
			Result.Record.Kind == "property"
			|| Result.Record.Kind == "global";
		if ((!bType && !bCallable && !bProperty)
			|| (bType && !ParseType(
				Document,
				Result.Record.Type,
				Result.Error))
			|| (bCallable && !ParseCallable(
				Document,
				Result.Record.Callable,
				Result.Error))
			|| (bProperty && !ParseProperty(
				Document,
				Result.Record.Property,
				Result.Error)))
		{
			if (Result.Error.empty())
			{
				Result.Error =
					"unsupported or incomplete symbol record kind";
			}
			return Result;
		}
		const std::string& NestedStableId = bType
			? Result.Record.Type.StableId
			: bCallable
				? Result.Record.Callable.StableId
				: Result.Record.Property.StableId;
		if (NestedStableId != Result.Record.StableId)
		{
			Result.Error =
				"symbol payload stable ID disagrees with record stable ID";
			return Result;
		}

		Result.Record.RawCanonicalJson.assign(
			Utf8Json.data(),
			Utf8Json.size());
		Result.bSuccess = true;
		return Result;
	}

	TOfflineRecordParseResult<FOfflineAssetRecord>
		ParseOfflineAssetRecord(const std::string_view Utf8Json)
	{
		TOfflineRecordParseResult<FOfflineAssetRecord> Result;
		rapidjson::Document Document;
		if (!ParseDocument(Utf8Json, Document, Result.Error)
			|| !ReadString(
				Document,
				"stableId",
				Result.Record.StableId,
				Result.Error,
				false)
			|| !ReadString(
				Document,
				"packagePath",
				Result.Record.PackagePath,
				Result.Error,
				false)
			|| !ReadString(
				Document,
				"objectPath",
				Result.Record.ObjectPath,
				Result.Error,
				false)
			|| !ReadString(
				Document,
				"generatedClassPath",
				Result.Record.GeneratedClassPath,
				Result.Error,
				true)
			|| !ReadString(
				Document,
				"assetClassPath",
				Result.Record.AssetClassPath,
				Result.Error,
				true)
			|| !ReadString(
				Document,
				"baseClassPath",
				Result.Record.BaseClassPath,
				Result.Error,
				true)
			|| !ReadString(
				Document,
				"mountPoint",
				Result.Record.MountPoint,
				Result.Error,
				false)
			|| !ReadString(
				Document,
				"originModule",
				Result.Record.OriginModule,
				Result.Error,
				true)
			|| !ReadString(
				Document,
				"originPlugin",
				Result.Record.OriginPlugin,
				Result.Error,
				true)
			|| !ReadString(
				Document,
				"redirectSource",
				Result.Record.RedirectSource,
				Result.Error,
				true)
			|| !ReadString(
				Document,
				"redirectTarget",
				Result.Record.RedirectTarget,
				Result.Error,
				true)
			|| !ReadString(
				Document,
				"availability",
				Result.Record.Availability,
				Result.Error,
				false))
		{
			return Result;
		}
		if (!IsStableId(Result.Record.StableId))
		{
			Result.Error = "asset stable ID must be a 64-character hexadecimal value";
			return Result;
		}
		const FJsonValue* Tags = FindMember(Document, "typeCheckTags");
		if (Tags == nullptr || !Tags->IsObject())
		{
			Result.Error = "asset typeCheckTags must be an object";
			return Result;
		}
		for (auto Iterator = Tags->MemberBegin();
			Iterator != Tags->MemberEnd();
			++Iterator)
		{
			if (!Iterator->value.IsString())
			{
				Result.Error =
					"asset typeCheckTags contains a non-string value";
				return Result;
			}
			Result.Record.TypeCheckTags.emplace(
				std::string(
					Iterator->name.GetString(),
					Iterator->name.GetStringLength()),
				std::string(
					Iterator->value.GetString(),
					Iterator->value.GetStringLength()));
		}
		Result.Record.RawCanonicalJson.assign(
			Utf8Json.data(),
			Utf8Json.size());
		Result.bSuccess = true;
		return Result;
	}
}
