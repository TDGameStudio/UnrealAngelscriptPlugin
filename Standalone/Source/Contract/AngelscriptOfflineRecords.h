#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace AngelscriptStandalone
{
	struct FOfflineOrigin
	{
		std::string Layer;
		std::string Kind;
		std::string Module;
		std::string Plugin;
		std::string StableModuleId;
	};

	struct FOfflineTypeRecord
	{
		struct FEnumValue
		{
			std::string StableId;
			std::string Name;
			std::int64_t Value = 0;
		};

		struct FTraits
		{
			bool bConstructible = false;
			bool bDestructible = false;
			bool bCopyConstructible = false;
			bool bCopyAssignable = false;
			bool bComparable = false;
			bool bHashable = false;
			bool bGarbageCollected = false;
			bool bTemplateEligible = false;
		};

		std::string StableId;
		std::string Kind;
		std::string Name;
		std::string Namespace;
		std::string CompleteDeclaration;
		std::string BaseStableId;
		std::vector<std::string> InterfaceStableIds;
		std::vector<std::string> MemberStableIds;
		std::vector<std::string> TemplateSubtypeDeclarations;
		std::vector<std::string> Flags;
		std::vector<FEnumValue> EnumValues;
		std::string AdapterStableId;
		std::string UETypePath;
		std::string Availability;
		FTraits Traits;
		std::int64_t CompileSize = -1;
		std::int64_t CompileAlignment = -1;
		bool bHandle = false;
		bool bTemplateDefinition = false;
	};

	struct FOfflineCallableRecord
	{
		struct FParameter
		{
			std::string Name;
			std::string TypeDeclaration;
			std::string Direction;
			std::string DefaultExpression;
			std::string ResourceKind;
			std::string ResourceTypeStableId;
			bool bHasDefault = false;
			bool bConst = false;
			bool bReference = false;
			bool bHandle = false;
		};

		std::string StableId;
		std::string Kind;
		std::string Name;
		std::string Namespace;
		std::string OwnerStableId;
		std::string Declaration;
		std::string ReturnType;
		std::string Access;
		std::string Behavior;
		std::string Availability;
		std::vector<FParameter> Parameters;
		std::string AdapterStableId;
		std::string UEFunctionPath;
		bool bConst = false;
		bool bStatic = false;
		bool bPropertyAccessor = false;
		bool bFinal = false;
		bool bOverride = false;
	};

	struct FOfflinePropertyRecord
	{
		std::string StableId;
		std::string Name;
		std::string Namespace;
		std::string OwnerStableId;
		std::string TypeDeclaration;
		std::string Declaration;
		std::string Access;
		std::string Availability;
		std::string AdapterStableId;
		std::string UEPropertyPath;
		bool bConst = false;
		bool bStatic = false;
		bool bReadable = true;
		bool bWritable = true;
	};

	struct FOfflineSymbolRecord
	{
		std::string StableId;
		std::string Kind;
		std::string CanonicalIdentity;
		FOfflineOrigin Origin;
		FOfflineTypeRecord Type;
		FOfflineCallableRecord Callable;
		FOfflinePropertyRecord Property;
		std::string RawCanonicalJson;

		std::string_view GetNamespace() const;
		std::string_view GetOwnerStableId() const;
		std::string_view GetName() const;
		std::string_view GetAdapterStableId() const;
	};

	struct FOfflineAssetRecord
	{
		std::string StableId;
		std::string PackagePath;
		std::string ObjectPath;
		std::string GeneratedClassPath;
		std::string AssetClassPath;
		std::string BaseClassPath;
		std::string MountPoint;
		std::string OriginModule;
		std::string OriginPlugin;
		std::string RedirectSource;
		std::string RedirectTarget;
		std::string Availability;
		std::map<std::string, std::string> TypeCheckTags;
		std::string RawCanonicalJson;
	};

	template <typename T>
	struct TOfflineRecordParseResult
	{
		bool bSuccess = false;
		std::string Error;
		T Record;
	};

	TOfflineRecordParseResult<FOfflineSymbolRecord>
		ParseOfflineSymbolRecord(std::string_view Utf8Json);
	TOfflineRecordParseResult<FOfflineAssetRecord>
		ParseOfflineAssetRecord(std::string_view Utf8Json);
}
