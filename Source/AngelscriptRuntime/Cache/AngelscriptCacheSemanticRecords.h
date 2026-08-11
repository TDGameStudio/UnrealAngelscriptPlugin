#pragma once

#include "Cache/AngelscriptCacheTypes.h"

enum class EAngelscriptCacheReferenceKind : uint8
{
	Invalid = 0,
	ScriptModule = 1,
	ScriptType = 2,
	ScriptFunction = 3,
	ScriptGlobal = 4,
	ScriptProperty = 5,
	ScriptImport = 6,
	EnvironmentSymbol = 7,
	CanonicalName = 8,
	StringLiteral = 9,
};

enum class EAngelscriptCachedDataTypeKind : uint8
{
	Invalid = 0,
	Primitive = 1,
	ScriptType = 2,
	EnvironmentType = 3,
	Auto = 4,
};

enum class EAngelscriptCachedPrimitiveType : uint8
{
	Invalid = 0,
	Void = 1,
	Bool = 2,
	Int8 = 3,
	Int16 = 4,
	Int32 = 5,
	Int64 = 6,
	UInt8 = 7,
	UInt16 = 8,
	UInt32 = 9,
	UInt64 = 10,
	Float32 = 11,
	Float64 = 12,
};

enum class EAngelscriptCacheDeclarationKind : uint8
{
	Invalid = 0,
	Type = 1,
	Function = 2,
	Global = 3,
	Property = 4,
};

enum class EAngelscriptCacheSchemaCoverage : uint8
{
	Invalid = 0,
	Forbidden = 1,
	Required = 2,
};

enum class EAngelscriptCacheBodyCoverage : uint8
{
	Invalid = 0,
	Forbidden = 1,
	Required = 2,
};

enum class EAngelscriptCacheDeclarationSlotKind : uint8
{
	Invalid = 0,
	Declaration = 1,
	Function = 2,
	VirtualFunction = 3,
	Import = 4,
};

enum class EAngelscriptCachedParameterPassing : uint8
{
	Invalid = 0,
	Value = 1,
	InReference = 2,
	OutReference = 3,
	InOutReference = 4,
};

enum class EAngelscriptCacheSemanticDependencyKind : uint8
{
	Invalid = 0,
	Import = 1,
	Declaration = 2,
	Signature = 3,
	Inheritance = 4,
	ValueLayout = 5,
	PropertyLayout = 6,
	GlobalStorage = 7,
	HardValue = 8,
	Initializer = 9,
	CompileOption = 10,
	EnvironmentAbi = 11,
	FunctionContent = 12,
};

enum class EAngelscriptCachePreprocessorInputKind : uint8
{
	Invalid = 0,
	IncludeFile = 1,
	Define = 2,
	ConditionalSymbol = 3,
	GeneratedSource = 4,
};

enum class EAngelscriptCachedSourceKind : uint8
{
	Invalid = 0,
	Game = 1,
	Plugin = 2,
	Memory = 3,
};

enum class EAngelscriptCachedSourceProviderKind : uint8
{
	Invalid = 0,
	BuiltInDisk = 1,
	Memory = 2,
	Generated = 3,
	External = 4,
};

enum class EAngelscriptCachedPreprocessHookPhase : uint8
{
	Invalid = 0,
	ProcessChunks = 1,
	PostProcessCode = 2,
	External = 3,
};

enum class EAngelscriptCachedSourceEdgeKind : uint8
{
	Invalid = 0,
	Include = 1,
	GeneratedSource = 2,
};

enum class EAngelscriptCachedFastPathScopeKind : uint8
{
	Invalid = 0,
	Mount = 1,
	Provider = 2,
	Hook = 3,
	SourceFile = 4,
	Module = 5,
};

enum class EAngelscriptCachedFastPathIneligibleReason : uint8
{
	Invalid = 0,
	MissingStableIdentity = 1,
	MissingVersionFingerprint = 2,
	MissingConfigurationFingerprint = 3,
	UnstableGeneratedSource = 4,
	UnknownHookBehavior = 5,
};

enum class EAngelscriptCachePreprocessorInputTargetKind : uint8
{
	None = 0,
	SourceFile = 1,
	Provider = 2,
	Hook = 3,
	Module = 4,
	GeneratedSource = 5,
};

enum class EAngelscriptCachedTypeQualifierFlags : uint32
{
	None = 0,
	Reference = 0x00000001,
	ObjectConst = 0x00000002,
	ObjectHandle = 0x00000004,
	ConstHandle = 0x00000008,
	Auto = 0x00000010,
	IfHandleThenConst = 0x00000020,
	KnownMask = 0x0000003f,
};

enum class EAngelscriptCachedSourceDiscoveryFilterFlags : uint32
{
	None = 0,
	SkipDevelopment = 0x00000001,
	SkipEditor = 0x00000002,
	KnownMask = 0x00000003,
};

enum class EAngelscriptCachedFingerprintCapabilityFlags : uint32
{
	None = 0,
	StableIdentity = 0x00000001,
	VersionFingerprint = 0x00000002,
	ConfigurationFingerprint = 0x00000004,
	ContentFingerprint = 0x00000008,
	KnownMask = 0x0000000f,
};

enum class EAngelscriptCachedDeclarationTraitFlags : uint32
{
	None = 0,
	Static = 0x00000001,
	Const = 0x00000002,
	Private = 0x00000004,
	Protected = 0x00000008,
	ThreadSafe = 0x00000010,
	Abstract = 0x00000020,
	Final = 0x00000040,
	Override = 0x00000080,
	Generated = 0x00000100,
	Shared = 0x00000200,
	External = 0x00000400,
	Property = 0x00000800,
	ImplicitConstructor = 0x00001000,
	Mixin = 0x00002000,
	Local = 0x00004000,
	NoDiscard = 0x00008000,
	Deprecated = 0x00010000,
	GenericTemplateFunction = 0x00020000,
	UsesWorldContext = 0x00040000,
	AcceptTemporaryObject = 0x00080000,
	NotCallable = 0x00100000,
	ForceConstArgumentExpressions = 0x00200000,
	ExternalImplicitThis = 0x00400000,
	AllowDiscard = 0x00800000,
	EditorOnly = 0x01000000,
	Explicit = 0x02000000,
	UnsafeDuringConstruction = 0x04000000,
	DefaultsOnly = 0x08000000,
	Constructor = 0x10000000,
	Destructor = 0x20000000,
	KnownMask = 0x3fffffff,
};

enum class EAngelscriptCachedReflectionFlags : uint32
{
	None = 0,
	BlueprintCallable = 0x00000001,
	BlueprintOverride = 0x00000002,
	BlueprintEvent = 0x00000004,
	BlueprintPure = 0x00000008,
	NetMulticast = 0x00000010,
	NetClient = 0x00000020,
	NetServer = 0x00000040,
	NetValidate = 0x00000080,
	Unreliable = 0x00000100,
	BlueprintAuthorityOnly = 0x00000200,
	Exec = 0x00000400,
	CanOverrideEvent = 0x00000800,
	BlueprintReadable = 0x00001000,
	BlueprintWritable = 0x00002000,
	KnownMask = 0x00003fff,
};

enum class EAngelscriptCachedParameterTraitFlags : uint32
{
	None = 0,
	BlueprintByValue = 0x00000001,
	BlueprintOutRef = 0x00000002,
	BlueprintInRef = 0x00000004,
	KnownMask = 0x00000007,
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCachedSourceMountKey { FAngelscriptHash256 Hash; };
struct ANGELSCRIPTRUNTIME_API FAngelscriptCachedSourceProviderKey { FAngelscriptHash256 Hash; };
struct ANGELSCRIPTRUNTIME_API FAngelscriptCachedPreprocessHookKey { FAngelscriptHash256 Hash; };
struct ANGELSCRIPTRUNTIME_API FAngelscriptCachedSourceFileKey { FAngelscriptHash256 Hash; };
struct ANGELSCRIPTRUNTIME_API FAngelscriptCachedPreprocessorInputKey { FAngelscriptHash256 Hash; };
struct ANGELSCRIPTRUNTIME_API FAngelscriptCachedSourceEdgeKey { FAngelscriptHash256 Hash; };
struct ANGELSCRIPTRUNTIME_API FAngelscriptStableImportKey { FAngelscriptHash256 Hash; };

struct ANGELSCRIPTRUNTIME_API FAngelscriptSourceMountIdentityInput
{
	EAngelscriptCachedSourceKind SourceKind = EAngelscriptCachedSourceKind::Invalid;
	FString LogicalMount;
	FAngelscriptCachedSourceProviderKey ProviderKey;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptSourceProviderIdentityInput
{
	EAngelscriptCachedSourceProviderKind ProviderKind = EAngelscriptCachedSourceProviderKind::Invalid;
	FString CanonicalImplementationIdentity;
	FAngelscriptHash256 IdentityFingerprint;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptPreprocessHookIdentityInput
{
	EAngelscriptCachedPreprocessHookPhase Phase = EAngelscriptCachedPreprocessHookPhase::Invalid;
	FString CanonicalImplementationIdentity;
	EAngelscriptCachedFastPathScopeKind AffectedScopeKind = EAngelscriptCachedFastPathScopeKind::Invalid;
	FAngelscriptHash256 AffectedScopeStableKey;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptSourceFileIdentityInput
{
	EAngelscriptCachedSourceKind SourceKind = EAngelscriptCachedSourceKind::Invalid;
	FAngelscriptCachedSourceMountKey MountKey;
	FAngelscriptCachedSourceProviderKey ProviderKey;
	FString RelativeLogicalPath;
	TOptional<FAngelscriptHash256> GeneratedSourceKey;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptPreprocessorInputIdentityInput
{
	FAngelscriptHash256 OwnerScopeStableKey;
	EAngelscriptCachePreprocessorInputKind InputKind = EAngelscriptCachePreprocessorInputKind::Invalid;
	FString CanonicalName;
	TOptional<FAngelscriptHash256> TargetStableKey;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptSourceEdgeIdentityInput
{
	EAngelscriptCachedSourceEdgeKind EdgeKind = EAngelscriptCachedSourceEdgeKind::Invalid;
	FAngelscriptCachedSourceFileKey FromSourceFileKey;
	FAngelscriptHash256 ToSourceOrGeneratedKey;
	FString CanonicalIncludeOrGeneratorIdentity;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptImportIdentityInput
{
	FAngelscriptStableModuleKey ModuleKey;
	FString CanonicalNamespace;
	FString CanonicalName;
	FString CanonicalSignature;
	FAngelscriptStableModuleKey TargetModuleKey;
	FAngelscriptStableFunctionKey TargetFunctionKey;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheStableReference
{
	EAngelscriptCacheReferenceKind Kind = EAngelscriptCacheReferenceKind::Invalid;
	FAngelscriptHash256 StableKey;
	FAngelscriptHash256 ExpectedAbi;

	friend bool operator==(
		const FAngelscriptCacheStableReference& Left,
		const FAngelscriptCacheStableReference& Right)
	{
		return Left.Kind == Right.Kind
			&& Left.StableKey == Right.StableKey
			&& Left.ExpectedAbi == Right.ExpectedAbi;
	}

	friend bool operator!=(
		const FAngelscriptCacheStableReference& Left,
		const FAngelscriptCacheStableReference& Right)
	{
		return !(Left == Right);
	}
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheSemanticDependency
{
	EAngelscriptCacheSemanticDependencyKind Kind = EAngelscriptCacheSemanticDependencyKind::Invalid;
	FAngelscriptCacheStableReference Target;
	TOptional<FAngelscriptHash256> ExpectedContentOrValue;

	friend bool operator==(
		const FAngelscriptCacheSemanticDependency& Left,
		const FAngelscriptCacheSemanticDependency& Right)
	{
		if (Left.Kind != Right.Kind || Left.Target != Right.Target
			|| Left.ExpectedContentOrValue.IsSet()
				!= Right.ExpectedContentOrValue.IsSet())
		{
			return false;
		}

		return !Left.ExpectedContentOrValue.IsSet()
			|| Left.ExpectedContentOrValue.GetValue()
				== Right.ExpectedContentOrValue.GetValue();
	}

	friend bool operator!=(
		const FAngelscriptCacheSemanticDependency& Left,
		const FAngelscriptCacheSemanticDependency& Right)
	{
		return !(Left == Right);
	}
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCachedDataType
{
	EAngelscriptCachedDataTypeKind Kind = EAngelscriptCachedDataTypeKind::Invalid;
	EAngelscriptCachedPrimitiveType Primitive = EAngelscriptCachedPrimitiveType::Invalid;
	TOptional<FAngelscriptCacheStableReference> TypeReference;
	uint32 QualifierFlags = 0;
	TArray<FAngelscriptCachedDataType> OrderedSubTypes;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCachedMetadataEntry
{
	FString CanonicalKey;
	FString CanonicalValue;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCachedCanonicalOption
{
	FString CanonicalKey;
	FAngelscriptHash256 ValueFingerprint;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCachedParameter
{
	uint32 Ordinal = 0;
	FString CanonicalName;
	FAngelscriptCachedDataType Type;
	EAngelscriptCachedParameterPassing Passing = EAngelscriptCachedParameterPassing::Invalid;
	TOptional<FString> CanonicalDefaultExpression;
	uint32 TraitFlags = 0;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCachedDeclarationSlot
{
	EAngelscriptCacheDeclarationSlotKind SlotKind = EAngelscriptCacheDeclarationSlotKind::Invalid;
	uint32 Ordinal = 0;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCachedDeclaration
{
	EAngelscriptCacheDeclarationKind DeclarationKind = EAngelscriptCacheDeclarationKind::Invalid;
	EAngelscriptArtifactEntityKind EntityKind = static_cast<EAngelscriptArtifactEntityKind>(0);
	EAngelscriptCacheSchemaCoverage SchemaCoverage = EAngelscriptCacheSchemaCoverage::Invalid;
	EAngelscriptCacheBodyCoverage BodyCoverage = EAngelscriptCacheBodyCoverage::Invalid;
	FAngelscriptHash256 StableKey;
	EAngelscriptFunctionOwnerKind OwnerKind = static_cast<EAngelscriptFunctionOwnerKind>(0);
	FAngelscriptHash256 OwnerKey;
	FAngelscriptStableModuleKey ModuleKey;
	FString CanonicalNamespace;
	FString CanonicalName;
	FString CanonicalDeclaration;
	TArray<FString> CanonicalIdentityTraits;
	TOptional<FString> CanonicalTypeSpelling;
	TOptional<FAngelscriptCachedDataType> DeclaredType;
	TArray<FAngelscriptCachedParameter> OrderedParameters;
	uint32 TraitFlags = 0;
	uint32 ReflectionFlags = 0;
	TArray<FAngelscriptCachedMetadataEntry> Metadata;
	TArray<FAngelscriptCachedDeclarationSlot> Slots;
	FAngelscriptHash256 SignatureHash;
	FAngelscriptHash256 TraitsHash;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCachedSourceDiscoveryPolicy
{
	uint32 PolicyVersion = 0;
	uint32 FilterFlags = 0;
	TArray<FAngelscriptCachedCanonicalOption> Options;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCachedSourceMount
{
	FAngelscriptCachedSourceMountKey MountKey;
	EAngelscriptCachedSourceKind SourceKind = EAngelscriptCachedSourceKind::Invalid;
	FString LogicalMount;
	FAngelscriptCachedSourceProviderKey ProviderKey;
	FAngelscriptHash256 RootConfigurationFingerprint;
	TArray<FAngelscriptCachedCanonicalOption> Options;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCachedSourceProvider
{
	FAngelscriptCachedSourceProviderKey ProviderKey;
	EAngelscriptCachedSourceProviderKind ProviderKind = EAngelscriptCachedSourceProviderKind::Invalid;
	FString CanonicalImplementationIdentity;
	FAngelscriptHash256 IdentityFingerprint;
	TOptional<FAngelscriptHash256> VersionFingerprint;
	TOptional<FAngelscriptHash256> ConfigurationFingerprint;
	TOptional<FAngelscriptHash256> ContentFingerprint;
	uint32 CapabilityFlags = 0;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCachedPreprocessHook
{
	FAngelscriptCachedPreprocessHookKey HookKey;
	EAngelscriptCachedPreprocessHookPhase Phase = EAngelscriptCachedPreprocessHookPhase::Invalid;
	FString CanonicalImplementationIdentity;
	EAngelscriptCachedFastPathScopeKind AffectedScopeKind = EAngelscriptCachedFastPathScopeKind::Invalid;
	FAngelscriptHash256 AffectedScopeStableKey;
	FAngelscriptHash256 IdentityFingerprint;
	TOptional<FAngelscriptHash256> VersionFingerprint;
	TOptional<FAngelscriptHash256> ConfigurationFingerprint;
	TOptional<FAngelscriptHash256> ContentFingerprint;
	uint32 CapabilityFlags = 0;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCachedSourceFile
{
	FAngelscriptCachedSourceFileKey SourceFileKey;
	EAngelscriptCachedSourceKind SourceKind = EAngelscriptCachedSourceKind::Invalid;
	FAngelscriptCachedSourceMountKey MountKey;
	FAngelscriptCachedSourceProviderKey ProviderKey;
	FString RelativeLogicalPath;
	FAngelscriptHash256 RawContentHash;
	TOptional<FAngelscriptHash256> GeneratedSourceKey;
	TOptional<FAngelscriptHash256> GeneratedConfigurationFingerprint;
	FAngelscriptStableModuleKey ModuleKey;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCachedPreprocessorInput
{
	FAngelscriptCachedPreprocessorInputKey InputKey;
	EAngelscriptCachedFastPathScopeKind OwnerScopeKind = EAngelscriptCachedFastPathScopeKind::Invalid;
	FAngelscriptHash256 OwnerScopeStableKey;
	EAngelscriptCachePreprocessorInputKind InputKind = EAngelscriptCachePreprocessorInputKind::Invalid;
	FString CanonicalName;
	EAngelscriptCachePreprocessorInputTargetKind TargetKind = EAngelscriptCachePreprocessorInputTargetKind::None;
	TOptional<FAngelscriptHash256> TargetStableKey;
	FAngelscriptHash256 EffectiveValueOrContentHash;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCachedSourceEdge
{
	FAngelscriptCachedSourceEdgeKey EdgeKey;
	EAngelscriptCachedSourceEdgeKind EdgeKind = EAngelscriptCachedSourceEdgeKind::Invalid;
	FAngelscriptCachedSourceFileKey FromSourceFileKey;
	FAngelscriptHash256 ToSourceOrGeneratedKey;
	FString CanonicalIncludeOrGeneratorIdentity;
	TOptional<uint32> SemanticOrdinal;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCachedFastPathIneligibleScope
{
	EAngelscriptCachedFastPathScopeKind ScopeKind = EAngelscriptCachedFastPathScopeKind::Invalid;
	FAngelscriptHash256 ScopeStableKey;
	EAngelscriptCachedFastPathIneligibleReason Reason =
		EAngelscriptCachedFastPathIneligibleReason::Invalid;
	FString CanonicalDiagnosticIdentity;
	TOptional<FAngelscriptHash256> ObservedFingerprint;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCachedSourceIndex
{
	FAngelscriptCachedSourceIndex() = default;
	FAngelscriptCachedSourceIndex(const FAngelscriptCachedSourceIndex&) = default;
	FAngelscriptCachedSourceIndex& operator=(const FAngelscriptCachedSourceIndex&) = default;
	FAngelscriptCachedSourceIndex(FAngelscriptCachedSourceIndex&&) noexcept = default;
	FAngelscriptCachedSourceIndex& operator=(FAngelscriptCachedSourceIndex&&) noexcept = default;

	uint32 PayloadSchemaVersion = 0;
	FAngelscriptHash256 SourceSnapshot;
	FAngelscriptCachedSourceDiscoveryPolicy DiscoveryPolicy;
	TArray<FAngelscriptCachedSourceMount> Mounts;
	TArray<FAngelscriptCachedSourceProvider> Providers;
	TArray<FAngelscriptCachedPreprocessHook> PreprocessHooks;
	TArray<FAngelscriptCachedSourceFile> Files;
	TArray<FAngelscriptCachedPreprocessorInput> PreprocessorInputs;
	TArray<FAngelscriptCachedSourceEdge> Edges;
	TArray<FAngelscriptCachedFastPathIneligibleScope> IneligibleScopes;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheExactFastPathEligibility
{
	bool bExactFastPathEligible = false;
	TArray<FAngelscriptCachedFastPathIneligibleScope> MatchingScopes;

	void Reset()
	{
		bExactFastPathEligible = false;
		MatchingScopes.Empty();
	}
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheExactFastPathEligibilityBatchEntry
{
	FAngelscriptStableModuleKey ModuleKey;
	FAngelscriptCacheExactFastPathEligibility Eligibility;
};

/**
 * Atomic producer-side eligibility result for one canonical SourceIndex.
 * Diagnostic counters describe published work only and therefore remain zero
 * when validation, preparation, any module query, or budget admission fails.
 */
struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheExactFastPathEligibilityBatch
{
	TArray<FAngelscriptCacheExactFastPathEligibilityBatchEntry> Entries;
	uint32 SourceValidationPasses = 0;
	uint32 PreparedIndexBuilds = 0;
	uint32 ModuleQueries = 0;

	void Reset()
	{
		Entries.Empty();
		SourceValidationPasses = 0;
		PreparedIndexBuilds = 0;
		ModuleQueries = 0;
	}
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCachedImportDeclaration
{
	FAngelscriptStableImportKey ImportKey;
	FString CanonicalNamespace;
	FString CanonicalName;
	FString CanonicalSignature;
	FAngelscriptStableModuleKey TargetModuleKey;
	FAngelscriptCacheStableReference TargetDeclaration;
	TArray<FAngelscriptCachedDeclarationSlot> Slots;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCachedModuleInterface
{
	uint32 PayloadSchemaVersion = 0;
	FAngelscriptStableModuleKey ModuleKey;
	FString CanonicalModuleName;
	FAngelscriptHash256 InterfaceAbi;
	TArray<FString> CanonicalNamespaces;
	TArray<FAngelscriptCachedDeclaration> Declarations;
	TArray<FAngelscriptCachedImportDeclaration> Imports;
	TArray<FAngelscriptCacheSemanticDependency> Dependencies;
};

class FAngelscriptDecodedCacheRecord;

#if WITH_ANGELSCRIPT_UNITTESTS
namespace AngelscriptCacheCanonicalCodecTestHooks
{
	enum class EAllocationSite : uint8
	{
		StringCharacters = 1,
		TypedArrayElements = 2,
	};

	enum class EAllocationEventPhase : uint8
	{
		BudgetAttempt = 1,
		BudgetAccepted = 2,
		AllocationAttempt = 3,
		AllocationSucceeded = 4,
	};

	struct FAllocationEvent
	{
		EAllocationSite Site = EAllocationSite::StringCharacters;
		EAllocationEventPhase Phase = EAllocationEventPhase::BudgetAttempt;
		uint64 FieldOffset = 0;
		int32 RequestedCapacity = 0;
		int32 ReservedCapacity = 0;
		uint64 ReservedBytes = 0;
		uint64 ActualAllocatedBytes = 0;
		uint64 ElementSize = 0;
		uint64 ElementAlignment = 0;
	};

	struct FAllocationEventCaptureView
	{
		// This non-owning view is passed explicitly to one decode. Observation
		// cannot allocate, route through ambient state, or outlive that call.
		FAllocationEventCaptureView(
			TArrayView<FAllocationEvent> InEventStorage,
			int32& OutEventCount,
			bool& bOutOverflowed)
			: EventStorage(InEventStorage)
			, EventCount(OutEventCount)
			, bOverflowed(bOutOverflowed)
		{
		}

		TArrayView<FAllocationEvent> EventStorage;
		int32& EventCount;
		bool& bOverflowed;
	};
}

namespace AngelscriptCacheEligibilityTestHooks
{
	enum class EAllocationPhase : uint8
	{
		PrimaryScratchArrays = 1,
		AuxiliaryScratchArrays = 2,
	};

	struct FAllocationEvent
	{
		EAllocationPhase Phase = EAllocationPhase::PrimaryScratchArrays;
		int32 RequestedCapacity = 0;
		int32 FirstCapacityBeforePopulation = 0;
		int32 SecondCapacityBeforePopulation = 0;
		int32 ThirdCapacityBeforePopulation = 0;
		int32 FourthCapacityBeforePopulation = 0;
		int32 FirstCapacityAfterPopulation = 0;
		int32 SecondCapacityAfterPopulation = 0;
		int32 ThirdCapacityAfterPopulation = 0;
		int32 FourthCapacityAfterPopulation = 0;
		uint64 AllocatedBytesBeforePopulation = 0;
		uint64 AllocatedBytesAfterPopulation = 0;
	};

	struct FAllocationEventCaptureView
	{
		FAllocationEventCaptureView(
			TArrayView<FAllocationEvent> InEventStorage,
			int32& OutEventCount,
			bool& bOutOverflowed)
			: EventStorage(InEventStorage)
			, EventCount(OutEventCount)
			, bOverflowed(bOutOverflowed)
		{
		}

		TArrayView<FAllocationEvent> EventStorage;
		int32& EventCount;
		bool& bOverflowed;
	};
}
#endif

class ANGELSCRIPTRUNTIME_API FAngelscriptCacheSemanticArchive
{
public:
	static constexpr uint32 SourceIndexPayloadSchemaVersion = 1;
	static constexpr uint32 ModuleInterfacePayloadSchemaVersion = 1;

	static FAngelscriptCacheValidationResult SerializeCanonicalString(
		FStringView Value, TArray<uint8>& OutBytes);
	static FAngelscriptCacheValidationResult DeserializeCanonicalString(
		TConstArrayView<uint8> Bytes, const FAngelscriptCacheReadLimits& Limits,
		FAngelscriptCacheReadBudget& Budget, FString& OutValue);
#if WITH_ANGELSCRIPT_UNITTESTS
	static FAngelscriptCacheValidationResult
		DeserializeCanonicalStringWithAllocationCaptureForTests(
			TConstArrayView<uint8> Bytes,
			const FAngelscriptCacheReadLimits& Limits,
			FAngelscriptCacheReadBudget& Budget,
			AngelscriptCacheCanonicalCodecTestHooks::FAllocationEventCaptureView Capture,
			FString& OutValue);
#endif

	static FAngelscriptCacheValidationResult SerializeCanonicalDataType(
		const FAngelscriptCachedDataType& Value, TArray<uint8>& OutBytes);
	static FAngelscriptCacheValidationResult DeserializeCanonicalDataType(
		TConstArrayView<uint8> Bytes, const FAngelscriptCacheReadLimits& Limits,
		FAngelscriptCacheReadBudget& Budget, FAngelscriptCachedDataType& OutValue);
#if WITH_ANGELSCRIPT_UNITTESTS
	static FAngelscriptCacheValidationResult
		DeserializeCanonicalDataTypeWithAllocationCaptureForTests(
			TConstArrayView<uint8> Bytes,
			const FAngelscriptCacheReadLimits& Limits,
			FAngelscriptCacheReadBudget& Budget,
			AngelscriptCacheCanonicalCodecTestHooks::FAllocationEventCaptureView Capture,
			FAngelscriptCachedDataType& OutValue);
#endif
	static FAngelscriptCacheValidationResult SerializeStableReference(
		const FAngelscriptCacheStableReference& Value, TArray<uint8>& OutBytes);
	static FAngelscriptCacheValidationResult SerializeSemanticDependency(
		const FAngelscriptCacheSemanticDependency& Value, TArray<uint8>& OutBytes);
	static FAngelscriptCacheValidationResult SerializeMetadataEntry(
		const FAngelscriptCachedMetadataEntry& Value, TArray<uint8>& OutBytes);
	static FAngelscriptCacheValidationResult SerializeParameter(
		const FAngelscriptCachedParameter& Value, TArray<uint8>& OutBytes);
	static FAngelscriptCacheValidationResult SerializeDeclarationSlot(
		const FAngelscriptCachedDeclarationSlot& Value, TArray<uint8>& OutBytes);

	static FAngelscriptCacheValidationResult ComputeDeclarationHashes(
		const FAngelscriptCachedDeclaration& Value,
		FAngelscriptHash256& OutSignatureHash,
		FAngelscriptHash256& OutTraitsHash);
	static FAngelscriptCacheValidationResult TryBuildImportKey(
		const FAngelscriptImportIdentityInput& Input,
		FAngelscriptStableImportKey& OutKey);
	static FAngelscriptCacheValidationResult TryBuildSourceProviderKey(
		const FAngelscriptSourceProviderIdentityInput& Input,
		FAngelscriptCachedSourceProviderKey& OutKey);
	static FAngelscriptCacheValidationResult TryBuildSourceMountKey(
		const FAngelscriptSourceMountIdentityInput& Input,
		FAngelscriptCachedSourceMountKey& OutKey);
	static FAngelscriptCacheValidationResult TryBuildPreprocessHookKey(
		const FAngelscriptPreprocessHookIdentityInput& Input,
		FAngelscriptCachedPreprocessHookKey& OutKey);
	static FAngelscriptCacheValidationResult TryBuildSourceFileKey(
		const FAngelscriptSourceFileIdentityInput& Input,
		FAngelscriptCachedSourceFileKey& OutKey);
	static FAngelscriptCacheValidationResult TryBuildPreprocessorInputKey(
		const FAngelscriptPreprocessorInputIdentityInput& Input,
		FAngelscriptCachedPreprocessorInputKey& OutKey);
	static FAngelscriptCacheValidationResult TryBuildSourceEdgeKey(
		const FAngelscriptSourceEdgeIdentityInput& Input,
		FAngelscriptCachedSourceEdgeKey& OutKey);
	static FAngelscriptCacheValidationResult ComputeSourceSnapshot(
		const FAngelscriptCachedSourceIndex& Value, FAngelscriptHash256& OutHash);
	static FAngelscriptCacheValidationResult CanonicalizeSourceIndex(
		FAngelscriptCachedSourceIndex& InOutValue);
	static FAngelscriptCacheValidationResult ComputeDirectSourceInputDigest(
		const FAngelscriptCachedSourceIndex& Value,
		const FAngelscriptArtifactProfileKey& Profile,
		FAngelscriptHash256& OutHash);
	static FAngelscriptCacheValidationResult QueryExactFastPathEligibility(
		const FAngelscriptDecodedCacheRecord& SourceIndexRecord,
		const FAngelscriptStableModuleKey& ModuleKey,
		const FAngelscriptCacheReadLimits& Limits,
		FAngelscriptCacheReadBudget& Budget,
		FAngelscriptCacheExactFastPathEligibility& OutResult);
	/**
	 * Producer-side counterpart for a current canonical SourceIndex. This is not
	 * a persisted decode path: it first validates the current snapshot and then
	 * invokes the same eligibility-closure implementation as decoded records.
	 */
	static FAngelscriptCacheValidationResult QueryCurrentExactFastPathEligibility(
		const FAngelscriptCachedSourceIndex& CurrentSourceIndex,
		const FAngelscriptStableModuleKey& ModuleKey,
		const FAngelscriptCacheReadLimits& Limits,
		FAngelscriptCacheReadBudget& Budget,
		FAngelscriptCacheExactFastPathEligibility& OutResult);
	/**
	 * Validates and prepares one current SourceIndex, then evaluates all requested
	 * modules in stable-key order under one cumulative budget. Publication is
	 * all-or-nothing; OutResult remains reset on every failure.
	 */
	static FAngelscriptCacheValidationResult
		QueryCurrentExactFastPathEligibilityBatch(
			const FAngelscriptCachedSourceIndex& CurrentSourceIndex,
			TConstArrayView<FAngelscriptStableModuleKey> ModuleKeys,
			const FAngelscriptCacheReadLimits& Limits,
			FAngelscriptCacheReadBudget& Budget,
			FAngelscriptCacheExactFastPathEligibilityBatch& OutResult);
#if WITH_ANGELSCRIPT_UNITTESTS
	static FAngelscriptCacheValidationResult
		QueryExactFastPathEligibilityWithAllocationCaptureForTests(
			const FAngelscriptDecodedCacheRecord& SourceIndexRecord,
			const FAngelscriptStableModuleKey& ModuleKey,
			const FAngelscriptCacheReadLimits& Limits,
			FAngelscriptCacheReadBudget& Budget,
			AngelscriptCacheEligibilityTestHooks::FAllocationEventCaptureView Capture,
			FAngelscriptCacheExactFastPathEligibility& OutResult);
#endif
	static FAngelscriptCacheValidationResult ComputeModuleInterfaceAbi(
		const FAngelscriptCachedModuleInterface& Value, FAngelscriptHash256& OutHash);

	static FAngelscriptCacheValidationResult SerializeSourceIndex(
		const FAngelscriptCachedSourceIndex& Value, TArray<uint8>& OutPayload);
	static FAngelscriptCacheValidationResult SerializeModuleInterface(
		const FAngelscriptCachedModuleInterface& Value, TArray<uint8>& OutPayload);

#if WITH_ANGELSCRIPT_UNITTESTS
	static FAngelscriptCacheValidationResult SerializeSourceIndexPreservingOrderForTests(
		const FAngelscriptCachedSourceIndex& Value, TArray<uint8>& OutPayload);
	static FAngelscriptCacheValidationResult SerializeModuleInterfacePreservingOrderForTests(
		const FAngelscriptCachedModuleInterface& Value, TArray<uint8>& OutPayload);
	static void SerializeSourceIndexPhysicalForTests(
		const FAngelscriptCachedSourceIndex& Value, TArray<uint8>& OutPayload);
	static void SerializeModuleInterfacePhysicalForTests(
		const FAngelscriptCachedModuleInterface& Value, TArray<uint8>& OutPayload);
#endif
};
