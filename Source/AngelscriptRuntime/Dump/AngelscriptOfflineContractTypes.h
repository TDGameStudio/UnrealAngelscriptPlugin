#pragma once

#include "CoreMinimal.h"

namespace AngelscriptOfflineContract
{
	inline constexpr int32 SchemaMajorVersion = 1;
	inline constexpr int32 SchemaMinorVersion = 0;
	inline constexpr TCHAR SchemaVersion[] = TEXT("1.0");
	inline constexpr TCHAR SymbolIdentityVersion[] = TEXT("symbol-id-v1");
	inline constexpr TCHAR ModuleIdentityVersion[] = TEXT("module-id-v1");
	inline constexpr TCHAR AdapterIdentityVersion[] = TEXT("adapter-id-v1");
	inline constexpr TCHAR AssetIdentityVersion[] = TEXT("asset-id-v1");

	enum class EBundleKind : uint8
	{
		DefaultEngine,
		Project,
	};

	enum class ESymbolKind : uint8
	{
		Type,
		Callable,
		Property,
		EnumValue,
		Typedef,
		Funcdef,
		Delegate,
		Global,
	};

	enum class ETypeKind : uint8
	{
		Primitive,
		Value,
		Reference,
		Template,
		Enum,
		Typedef,
		Funcdef,
		Delegate,
	};

	enum class ECallableKind : uint8
	{
		GlobalFunction,
		Method,
		Behavior,
		Factory,
		Constructor,
		Destructor,
		Event,
	};

	enum class EParameterDirection : uint8
	{
		Value,
		In,
		Out,
		InOut,
	};

	enum class EOriginLayer : uint8
	{
		HostSurface,
		ScriptBaseline,
	};

	enum class EOriginKind : uint8
	{
		Manual,
		Generated,
		NativeModule,
		Reflective,
		Blueprint,
		Script,
		OptionalPlugin,
		Project,
		Unknown,
	};

	enum class EAvailability : uint8
	{
		Available,
		EditorOnly,
		Deprecated,
		Internal,
		Unavailable,
		Unknown,
	};

	struct FContractVersion
	{
		int32 Major = SchemaMajorVersion;
		int32 Minor = SchemaMinorVersion;
	};

	struct FOriginRecord
	{
		EOriginLayer Layer = EOriginLayer::HostSurface;
		EOriginKind Kind = EOriginKind::Unknown;
		FString Module;
		FString Plugin;
		FString StableModuleId;
	};

	struct FParameterRecord
	{
		FString Name;
		FString TypeDeclaration;
		EParameterDirection Direction = EParameterDirection::Value;
		FString DefaultExpression;
		// Optional compile-time resource semantics observed from the final
		// registration/reflection surface. Empty means this parameter is not a
		// resource path context. Consumers must resolve the owning callable by
		// StableId before using this marker.
		FString ResourceKind;
		FString ResourceTypeStableId;
		bool bHasDefault = false;
		bool bConst = false;
		bool bReference = false;
		bool bHandle = false;
	};

	struct FPropertyRecord
	{
		FString StableId;
		FString OwnerStableId;
		FString Namespace;
		FString Name;
		FString TypeDeclaration;
		FString CompleteDeclaration;
		FString UEPropertyPath;
		FString Access;
		FString AdapterStableId;
		EAvailability Availability = EAvailability::Unknown;
		FOriginRecord Origin;
		bool bConst = false;
		bool bStatic = false;
		bool bReadable = true;
		bool bWritable = true;
	};

	struct FCallableRecord
	{
		FString StableId;
		FString OwnerStableId;
		FString Namespace;
		FString Name;
		FString Declaration;
		FString ReturnType;
		FString UEFunctionPath;
		FString Access;
		FString Behavior;
		FString AdapterStableId;
		TArray<FParameterRecord> Parameters;
		ECallableKind Kind = ECallableKind::GlobalFunction;
		EAvailability Availability = EAvailability::Unknown;
		FOriginRecord Origin;
		bool bConst = false;
		bool bStatic = false;
		bool bPropertyAccessor = false;
		bool bFinal = false;
		bool bOverride = false;
	};

	struct FTypeTraitsRecord
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

	struct FEnumValueRecord
	{
		FString StableId;
		FString Name;
		int64 Value = 0;
	};

	struct FTypeRecord
	{
		FString StableId;
		FString Namespace;
		FString Name;
		FString CompleteDeclaration;
		FString UETypePath;
		FString BaseStableId;
		TArray<FString> InterfaceStableIds;
		TArray<FString> TemplateSubtypeDeclarations;
		TArray<FString> MemberStableIds;
		TArray<FString> Flags;
		TArray<FEnumValueRecord> EnumValues;
		FString AdapterStableId;
		ETypeKind Kind = ETypeKind::Reference;
		EAvailability Availability = EAvailability::Unknown;
		FOriginRecord Origin;
		FTypeTraitsRecord Traits;
		int64 CompileSize = -1;
		int64 CompileAlignment = -1;
		bool bHandle = false;
		bool bTemplateDefinition = false;
	};

	struct FAdapterRecord
	{
		FString StableId;
		FString Name;
		FString Version;
		FString SurfaceHash;
		TArray<FString> RequiredEngineProperties;
		TArray<FString> RequiredTraits;
		bool bDeclarativeOnly = true;
	};

	struct FScopeRecord
	{
		bool bComplete = false;
		FString State;
		TArray<FString> Included;
		TArray<FString> Excluded;
		TArray<FString> Skipped;
		TArray<FString> Diagnostics;
	};

	struct FAssetRecord
	{
		FString StableId;
		FString PackagePath;
		FString ObjectPath;
		FString GeneratedClassPath;
		FString AssetClassPath;
		FString BaseClassPath;
		FString MountPoint;
		FString OriginModule;
		FString OriginPlugin;
		FString RedirectSource;
		FString RedirectTarget;
		EAvailability Availability = EAvailability::Unknown;
		TMap<FString, FString> TypeCheckTags;
	};

	struct FFileRecord
	{
		FString Name;
		FString Sha256;
		int64 RecordCount = 0;
		int64 ByteCount = 0;
	};

	struct FManifestRecord
	{
		FContractVersion Schema;
		EBundleKind BundleKind = EBundleKind::Project;
		FString ProducerName;
		FString ProducerVersion;
		FString UnrealVersion;
		FString PluginVersion;
		FString ForkVersion;
		FString CompilerContractVersion;
		FString Platform;
		FString Configuration;
		FString BundleIdentity;
		FScopeRecord SymbolScope;
		FScopeRecord AssetScope;
		TMap<FString, FString> EngineProperties;
		TMap<FString, bool> FeatureFlags;
		TArray<FString> LoadedModules;
		TArray<FString> LoadedPlugins;
		TArray<FString> RequiredFields;
		TArray<FAdapterRecord> Adapters;
		TArray<FFileRecord> Files;
	};

	struct FSymbolRecord
	{
		ESymbolKind Kind = ESymbolKind::Type;
		FString StableId;
		FString CanonicalIdentity;
		FTypeRecord Type;
		FCallableRecord Callable;
		FPropertyRecord Property;
		FOriginRecord Origin;
	};

	ANGELSCRIPTRUNTIME_API const TCHAR* LexToString(EBundleKind Value);
	ANGELSCRIPTRUNTIME_API const TCHAR* LexToString(ESymbolKind Value);
	ANGELSCRIPTRUNTIME_API const TCHAR* LexToString(ETypeKind Value);
	ANGELSCRIPTRUNTIME_API const TCHAR* LexToString(ECallableKind Value);
	ANGELSCRIPTRUNTIME_API const TCHAR* LexToString(EParameterDirection Value);
	ANGELSCRIPTRUNTIME_API const TCHAR* LexToString(EOriginLayer Value);
	ANGELSCRIPTRUNTIME_API const TCHAR* LexToString(EOriginKind Value);
	ANGELSCRIPTRUNTIME_API const TCHAR* LexToString(EAvailability Value);
}
