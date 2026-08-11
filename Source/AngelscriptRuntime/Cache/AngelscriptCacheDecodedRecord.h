#pragma once

#include "Cache/AngelscriptCacheRemainingRecordTypes.h"
#include "Cache/AngelscriptCacheTypeSchema.h"
#include "Misc/TVariant.h"

enum class EAngelscriptSourceIndexCapturedField : uint16
{
	Invalid = 0,
	PayloadSchemaVersion = 1,
	SourceSnapshot = 2,
	DiscoveryPolicy = 3,
	DiscoveryPolicyVersion = 4,
	DiscoveryPolicyFilterFlags = 5,
	DiscoveryPolicyOptions = 6,
	DiscoveryPolicyOption = 7,
	DiscoveryPolicyOptionCanonicalKey = 8,
	DiscoveryPolicyOptionValueFingerprint = 9,
	Mounts = 10,
	Mount = 11,
	MountKey = 12,
	MountSourceKind = 13,
	MountLogicalMount = 14,
	MountProviderKey = 15,
	MountRootConfigurationFingerprint = 16,
	MountOptions = 17,
	MountOption = 18,
	MountOptionCanonicalKey = 19,
	MountOptionValueFingerprint = 20,
	Providers = 21,
	Provider = 22,
	ProviderKey = 23,
	ProviderKind = 24,
	ProviderCanonicalImplementationIdentity = 25,
	ProviderIdentityFingerprint = 26,
	ProviderVersionFingerprintPresence = 27,
	ProviderVersionFingerprint = 28,
	ProviderConfigurationFingerprintPresence = 29,
	ProviderConfigurationFingerprint = 30,
	ProviderContentFingerprintPresence = 31,
	ProviderContentFingerprint = 32,
	ProviderCapabilityFlags = 33,
	PreprocessHooks = 34,
	PreprocessHook = 35,
	HookKey = 36,
	HookPhase = 37,
	HookCanonicalImplementationIdentity = 38,
	HookAffectedScopeKind = 39,
	HookAffectedScopeStableKey = 40,
	HookIdentityFingerprint = 41,
	HookVersionFingerprintPresence = 42,
	HookVersionFingerprint = 43,
	HookConfigurationFingerprintPresence = 44,
	HookConfigurationFingerprint = 45,
	HookContentFingerprintPresence = 46,
	HookContentFingerprint = 47,
	HookCapabilityFlags = 48,
	Files = 49,
	File = 50,
	FileSourceFileKey = 51,
	FileSourceKind = 52,
	FileMountKey = 53,
	FileProviderKey = 54,
	FileRelativeLogicalPath = 55,
	FileRawContentHash = 56,
	FileGeneratedSourceKeyPresence = 57,
	FileGeneratedSourceKey = 58,
	FileGeneratedConfigurationFingerprintPresence = 59,
	FileGeneratedConfigurationFingerprint = 60,
	FileModuleKey = 61,
	PreprocessorInputs = 62,
	PreprocessorInput = 63,
	InputKey = 64,
	InputOwnerScopeKind = 65,
	InputOwnerScopeStableKey = 66,
	InputKind = 67,
	InputCanonicalName = 68,
	InputTargetKind = 69,
	InputTargetStableKeyPresence = 70,
	InputTargetStableKey = 71,
	InputEffectiveValueOrContentHash = 72,
	Edges = 73,
	Edge = 74,
	EdgeKey = 75,
	EdgeKind = 76,
	EdgeFromSourceFileKey = 77,
	EdgeToSourceOrGeneratedKey = 78,
	EdgeCanonicalIncludeOrGeneratorIdentity = 79,
	EdgeSemanticOrdinalPresence = 80,
	EdgeSemanticOrdinal = 81,
	IneligibleScopes = 82,
	IneligibleScope = 83,
	IneligibleScopeKind = 84,
	IneligibleScopeStableKey = 85,
	IneligibleScopeReason = 86,
	IneligibleScopeCanonicalDiagnosticIdentity = 87,
	IneligibleScopeObservedFingerprintPresence = 88,
	IneligibleScopeObservedFingerprint = 89,
};

enum class EAngelscriptModuleInterfaceCapturedField : uint16
{
	Invalid = 0,
	PayloadSchemaVersion = 1,
	ModuleKey = 2,
	CanonicalModuleName = 3,
	InterfaceAbi = 4,
	CanonicalNamespaces = 5,
	CanonicalNamespace = 6,
	Declarations = 7,
	Declaration = 8,
	DeclarationKind = 9,
	DeclarationEntityKind = 10,
	DeclarationSchemaCoverage = 11,
	DeclarationBodyCoverage = 12,
	DeclarationStableKey = 13,
	DeclarationOwnerKind = 14,
	DeclarationOwnerKey = 15,
	DeclarationModuleKey = 16,
	DeclarationCanonicalNamespace = 17,
	DeclarationCanonicalName = 18,
	DeclarationCanonicalDeclaration = 19,
	DeclarationCanonicalIdentityTraits = 20,
	DeclarationCanonicalIdentityTrait = 21,
	DeclarationCanonicalTypeSpellingPresence = 22,
	DeclarationCanonicalTypeSpelling = 23,
	DeclarationDeclaredTypePresence = 24,
	DeclarationDeclaredTypeNode = 25,
	DeclarationDeclaredTypeKind = 26,
	DeclarationDeclaredTypePrimitive = 27,
	DeclarationDeclaredTypeReferencePresence = 28,
	DeclarationDeclaredTypeReference = 29,
	DeclarationDeclaredTypeReferenceKind = 30,
	DeclarationDeclaredTypeReferenceStableKey = 31,
	DeclarationDeclaredTypeReferenceExpectedAbi = 32,
	DeclarationDeclaredTypeQualifierFlags = 33,
	DeclarationDeclaredTypeOrderedSubTypes = 34,
	DeclarationOrderedParameters = 35,
	DeclarationParameter = 36,
	DeclarationParameterOrdinal = 37,
	DeclarationParameterCanonicalName = 38,
	DeclarationParameterTypeNode = 39,
	DeclarationParameterTypeKind = 40,
	DeclarationParameterTypePrimitive = 41,
	DeclarationParameterTypeReferencePresence = 42,
	DeclarationParameterTypeReference = 43,
	DeclarationParameterTypeReferenceKind = 44,
	DeclarationParameterTypeReferenceStableKey = 45,
	DeclarationParameterTypeReferenceExpectedAbi = 46,
	DeclarationParameterTypeQualifierFlags = 47,
	DeclarationParameterTypeOrderedSubTypes = 48,
	DeclarationParameterPassing = 49,
	DeclarationParameterDefaultExpressionPresence = 50,
	DeclarationParameterDefaultExpression = 51,
	DeclarationParameterTraitFlags = 52,
	DeclarationTraitFlags = 53,
	DeclarationReflectionFlags = 54,
	DeclarationMetadata = 55,
	DeclarationMetadataEntry = 56,
	DeclarationMetadataCanonicalKey = 57,
	DeclarationMetadataCanonicalValue = 58,
	DeclarationSlots = 59,
	DeclarationSlot = 60,
	DeclarationSlotKind = 61,
	DeclarationSlotOrdinal = 62,
	DeclarationSignatureHash = 63,
	DeclarationTraitsHash = 64,
	Imports = 65,
	Import = 66,
	ImportKey = 67,
	ImportCanonicalNamespace = 68,
	ImportCanonicalName = 69,
	ImportCanonicalSignature = 70,
	ImportTargetModuleKey = 71,
	ImportTargetDeclaration = 72,
	ImportTargetDeclarationReferenceKind = 73,
	ImportTargetDeclarationStableKey = 74,
	ImportTargetDeclarationExpectedAbi = 75,
	ImportSlots = 76,
	ImportSlot = 77,
	ImportSlotKind = 78,
	ImportSlotOrdinal = 79,
	Dependencies = 80,
	Dependency = 81,
	DependencyKind = 82,
	DependencyTarget = 83,
	DependencyTargetReferenceKind = 84,
	DependencyTargetStableKey = 85,
	DependencyTargetExpectedAbi = 86,
	DependencyExpectedContentOrValuePresence = 87,
	DependencyExpectedContentOrValue = 88,
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptSourceIndexFieldCoordinate
{
	EAngelscriptSourceIndexCapturedField Field = EAngelscriptSourceIndexCapturedField::Invalid;
	uint32 PrimaryIndex = MAX_uint32;
	uint32 SecondaryIndex = MAX_uint32;
	uint32 TertiaryIndex = MAX_uint32;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptModuleInterfaceFieldCoordinate
{
	EAngelscriptModuleInterfaceCapturedField Field =
		EAngelscriptModuleInterfaceCapturedField::Invalid;
	uint32 PrimaryIndex = MAX_uint32;
	uint32 SecondaryIndex = MAX_uint32;
	uint32 TertiaryIndex = MAX_uint32;
};

namespace AngelscriptCacheTypeSchema_Private
{
	struct FDecodedRecordCodecBridge;
}

namespace AngelscriptCacheSemanticRecords_Private
{
	struct FDecodedRecordCodecBridge;
}

namespace AngelscriptCacheRemainingRecords_Private
{
	struct FDecodedRecordCodecBridge;
}

class ANGELSCRIPTRUNTIME_API FAngelscriptDecodedCacheRecord final
{
private:
	template <typename CoordinateType>
	struct TCapturedOffset final
	{
		CoordinateType Coordinate;
		uint64 Offset = 0;
	};

	template <typename CoordinateType>
	struct TSingleCapturedOffsetStorage final
	{
		TArray<TCapturedOffset<CoordinateType>> Entries;
	};

	struct FTypeSchemaCapturedOffsetStorage final
	{
		using FEntry = TCapturedOffset<FAngelscriptTypeSchemaFieldCoordinate>;

		TArray<FEntry> FlatHeaderOffsets;
		TArray<FEntry> ParallelMetadataOffsets;
		TArray<FEntry> ParallelRelationOffsets;
		TArray<FEntry> ParallelLayoutInputOffsets;
		TArray<FEntry> ParallelPropertyOffsets;
		TArray<FEntry> ParallelNestedPropertyOffsets;
		TArray<FEntry> ParallelMethodOffsets;
		TArray<FEntry> ParallelVftOffsets;
		TArray<FEntry> ParallelBehaviorOffsets;
		TArray<FEntry> FlatSelectedArmOffsets;
		TOptional<FEntry> ReflectionOffset;
		TOptional<FEntry> ReflectionKindOffset;
		TOptional<FEntry> ClassReflectionFlagsOffset;
		TArray<FEntry> ParallelReflectionOffsets;
		TArray<FEntry> ParallelDependencyOffsets;
	};

	struct FModuleSnapshotCapturedOffsetStorage final
	{
		using FEntry = TCapturedOffset<FAngelscriptModuleSnapshotFieldCoordinate>;

		TStaticArray<FEntry, 14> HeaderOffsets;
		TArray<FEntry> TypeSchemaLinkOffsets;
		TArray<FEntry> FunctionBodyLinkOffsets;
	};

	struct FFunctionBodyCapturedOffsetStorage final
	{
		using FEntry = TCapturedOffset<FAngelscriptFunctionBodyFieldCoordinate>;

		// These are exactly the always-present fields: 1..15 plus field 24.
		TStaticArray<FEntry, 16> HeaderOffsets;
		// Seven entries are always present for each dependency. The optional
		// value occurrence has one exact slot per dependency instead of an
		// over-reserved worst-case offsets array.
		TArray<FEntry> DependencyOffsets;
		TArray<TOptional<FEntry>> DependencyExpectedValueOffsets;
		TOptional<FEntry> DebugSidecarOffset;
		TOptional<FEntry> DebugSidecarKindOffset;
		TOptional<FEntry> DebugSidecarContentHashOffset;
	};

	struct FModuleStateCapturedOffsetStorage final
	{
		using FEntry = TCapturedOffset<FAngelscriptModuleStateFieldCoordinate>;

		struct FDataTypeOffsets final
		{
			// Node, kind, primitive, reference-presence, qualifiers and subtype count.
			TStaticArray<FEntry, 6> Fields;
			TOptional<FEntry> Reference;
			TOptional<FEntry> ReferenceKind;
			TOptional<FEntry> ReferenceStableKey;
			TOptional<FEntry> ReferenceExpectedAbi;
			TArray<FDataTypeOffsets> SubTypes;
		};

		struct FDependencyOffsets final
		{
			// Row, kind, target, target kind/key/ABI and optional-value presence.
			TStaticArray<FEntry, 7> Fields;
			TOptional<FEntry> ExpectedContentOrValue;
		};

		struct FGlobalOffsets final
		{
			// Row, ordinal, key, namespace, name, traits, init, cleanup, fingerprint.
			TStaticArray<FEntry, 9> Fields;
			FDataTypeOffsets Type;
		};

		struct FHardValueOffsets final
		{
			// Row, kind, owner, owner kind/key/ABI, value presence and stored hash.
			TStaticArray<FEntry, 8> Fields;
			FDataTypeOffsets Type;
			TOptional<FEntry> CanonicalValue;
			TOptional<FEntry> CanonicalValueKind;
			TOptional<FEntry> CanonicalValueBytes;
		};

		struct FInitializerOffsets final
		{
			// Row, kind, key, owner presence, codec, execution hash and payload.
			TStaticArray<FEntry, 7> Fields;
			TOptional<FEntry> OwnerGlobal;
		};

		struct FInitializationActionOffsets final
		{
			// Row, ordinal, kind, target, target kind/key/ABI and dependencies count.
			TStaticArray<FEntry, 8> Fields;
			TArray<FDependencyOffsets> Dependencies;
		};

		struct FPostInitOffsets final
		{
			// Row, ordinal, reference and reference kind/key/ABI.
			TStaticArray<FEntry, 6> Fields;
		};

		// The ten top-level scalar/count fields are always present. Every nested
		// owner mirrors the decoded DTO and reserves only its exact row/subrow count;
		// no flat worst-case offset table or unbudgeted scratch allocation exists.
		TStaticArray<FEntry, 10> HeaderOffsets;
		TArray<FGlobalOffsets> Globals;
		TArray<FHardValueOffsets> HardValues;
		TArray<FInitializerOffsets> Initializers;
		TArray<FInitializationActionOffsets> InitializationActions;
		TArray<FPostInitOffsets> PostInitFunctions;
		TArray<FDependencyOffsets> Dependencies;
	};

	struct FSourceIndexCapturedOffsetStorage final
	{
		using FEntry = TCapturedOffset<FAngelscriptSourceIndexFieldCoordinate>;

		struct FOptionOffsets final
		{
			TStaticArray<FEntry, 3> Fields;
		};

		struct FDiscoveryOffsets final
		{
			// DiscoveryPolicy, version, filter flags and options count.
			TStaticArray<FEntry, 4> Fields;
			TArray<FOptionOffsets> Options;
		};

		struct FMountOffsets final
		{
			// Row, key, kind, mount string, provider, root fingerprint and options count.
			TStaticArray<FEntry, 7> Fields;
			TArray<FOptionOffsets> Options;
		};

		struct FProviderOffsets final
		{
			// Row/key/kind/identity, three presence tags and capability flags.
			TStaticArray<FEntry, 9> Fields;
			TOptional<FEntry> VersionFingerprint;
			TOptional<FEntry> ConfigurationFingerprint;
			TOptional<FEntry> ContentFingerprint;
		};

		struct FHookOffsets final
		{
			// Row/key/phase/identity/scope/identity hash, presence tags and flags.
			TStaticArray<FEntry, 11> Fields;
			TOptional<FEntry> VersionFingerprint;
			TOptional<FEntry> ConfigurationFingerprint;
			TOptional<FEntry> ContentFingerprint;
		};

		struct FFileOffsets final
		{
			// Row/key/kind/mount/provider/path/raw hash, presence tags and module.
			TStaticArray<FEntry, 10> Fields;
			TOptional<FEntry> GeneratedSourceKey;
			TOptional<FEntry> GeneratedConfigurationFingerprint;
		};

		struct FInputOffsets final
		{
			// Row/key/owner/input/name/target/presence and effective value.
			TStaticArray<FEntry, 9> Fields;
			TOptional<FEntry> TargetStableKey;
		};

		struct FEdgeOffsets final
		{
			// Row/key/kind/from/to/identity and semantic-ordinal presence.
			TStaticArray<FEntry, 7> Fields;
			TOptional<FEntry> SemanticOrdinal;
		};

		struct FIneligibleOffsets final
		{
			// Row/kind/key/reason/diagnostic and observed-fingerprint presence.
			TStaticArray<FEntry, 6> Fields;
			TOptional<FEntry> ObservedFingerprint;
		};

		// Schema, snapshot and the seven top-level collection counts.
		TStaticArray<FEntry, 9> HeaderOffsets;
		FDiscoveryOffsets Discovery;
		TArray<FMountOffsets> Mounts;
		TArray<FProviderOffsets> Providers;
		TArray<FHookOffsets> Hooks;
		TArray<FFileOffsets> Files;
		TArray<FInputOffsets> Inputs;
		TArray<FEdgeOffsets> Edges;
		TArray<FIneligibleOffsets> IneligibleScopes;
	};

	struct FModuleInterfaceCapturedOffsetStorage final
	{
		using FEntry = TCapturedOffset<FAngelscriptModuleInterfaceFieldCoordinate>;

		struct FDataTypeOffsets final
		{
			// Node, kind, primitive, reference-presence, qualifiers and subtype count.
			TStaticArray<FEntry, 6> Fields;
			TOptional<FEntry> Reference;
			TOptional<FEntry> ReferenceKind;
			TOptional<FEntry> ReferenceStableKey;
			TOptional<FEntry> ReferenceExpectedAbi;
			TArray<FDataTypeOffsets> SubTypes;
		};

		struct FParameterOffsets final
		{
			// Row, ordinal, name, passing, default-presence and trait flags.
			TStaticArray<FEntry, 6> Fields;
			FDataTypeOffsets Type;
			TOptional<FEntry> DefaultExpression;
		};

		struct FMetadataOffsets final
		{
			TStaticArray<FEntry, 3> Fields;
		};

		struct FSlotOffsets final
		{
			TStaticArray<FEntry, 3> Fields;
		};

		struct FDeclarationOffsets final
		{
			// All always-present direct fields/counts/presence tags/hashes.
			TStaticArray<FEntry, 22> Fields;
			TArray<FEntry> IdentityTraits;
			TOptional<FEntry> CanonicalTypeSpelling;
			TOptional<FDataTypeOffsets> DeclaredType;
			TArray<FParameterOffsets> Parameters;
			TArray<FMetadataOffsets> Metadata;
			TArray<FSlotOffsets> Slots;
		};

		struct FImportOffsets final
		{
			// Row/key/names/signature/module/reference and slots count.
			TStaticArray<FEntry, 11> Fields;
			TArray<FSlotOffsets> Slots;
		};

		struct FDependencyOffsets final
		{
			// Row/kind/target/reference fields and content-presence tag.
			TStaticArray<FEntry, 7> Fields;
			TOptional<FEntry> ExpectedContentOrValue;
		};

		// Schema/module/name/ABI/namespaces plus declarations/imports/dependencies counts.
		TStaticArray<FEntry, 8> HeaderOffsets;
		TArray<FEntry> CanonicalNamespaces;
		TArray<FDeclarationOffsets> Declarations;
		TArray<FImportOffsets> Imports;
		TArray<FDependencyOffsets> Dependencies;
	};

	template <typename ValueType, typename CapturedOffsetStorageType>
	struct TDecodedRecordAlternative final
	{
		ValueType Value;
		CapturedOffsetStorageType Offsets;
	};

	using FSourceIndexRecord = TDecodedRecordAlternative<
		FAngelscriptCachedSourceIndex,
		FSourceIndexCapturedOffsetStorage>;
	using FModuleInterfaceRecord = TDecodedRecordAlternative<
		FAngelscriptCachedModuleInterface,
		FModuleInterfaceCapturedOffsetStorage>;
	using FTypeSchemaRecord = TDecodedRecordAlternative<
		FAngelscriptCachedTypeSchema,
		FTypeSchemaCapturedOffsetStorage>;
	using FModuleStateRecord = TDecodedRecordAlternative<
		FAngelscriptCachedModuleState,
		FModuleStateCapturedOffsetStorage>;
	using FFunctionBodyRecord = TDecodedRecordAlternative<
		FAngelscriptCachedFunctionBody,
		FFunctionBodyCapturedOffsetStorage>;
	using FDebugSidecarRecord = TDecodedRecordAlternative<
		FAngelscriptCachedDebugSidecar,
		TSingleCapturedOffsetStorage<FAngelscriptDebugSidecarFieldCoordinate>>;
	using FModuleSnapshotRecord = TDecodedRecordAlternative<
		FAngelscriptCachedModuleSnapshot,
		FModuleSnapshotCapturedOffsetStorage>;

	using FRecordVariant = TVariant<
		FSourceIndexRecord,
		FModuleInterfaceRecord,
		FTypeSchemaRecord,
		FModuleStateRecord,
		FFunctionBodyRecord,
		FDebugSidecarRecord,
		FModuleSnapshotRecord>;

	struct FPrivateConstructionToken final
	{
	};

public:
	FAngelscriptDecodedCacheRecord(
		FPrivateConstructionToken,
		const FAngelscriptCacheRecordId& InRecordId,
		TArray<uint8>&& InCanonicalPayload,
		FRecordVariant&& InRecord);
	~FAngelscriptDecodedCacheRecord();

	FAngelscriptDecodedCacheRecord() = delete;
	FAngelscriptDecodedCacheRecord(const FAngelscriptDecodedCacheRecord&) = delete;
	FAngelscriptDecodedCacheRecord& operator=(const FAngelscriptDecodedCacheRecord&) = delete;
	FAngelscriptDecodedCacheRecord(FAngelscriptDecodedCacheRecord&&) = delete;
	FAngelscriptDecodedCacheRecord& operator=(FAngelscriptDecodedCacheRecord&&) = delete;

	static FAngelscriptCacheValidationResult TryDecode(
		const FAngelscriptCacheRecordId& DeclaredRecordId,
		TConstArrayView<uint8> CanonicalPayload,
		const FAngelscriptCacheReadLimits& Limits,
		FAngelscriptCacheReadBudget& Budget,
		TOptional<TSharedRef<const FAngelscriptDecodedCacheRecord, ESPMode::ThreadSafe>>& OutRecord);

	const FAngelscriptCacheRecordId& GetRecordId() const { return RecordId; }
	TConstArrayView<uint8> GetCanonicalPayload() const { return CanonicalPayload; }

	const FAngelscriptCachedSourceIndex* TryGetSourceIndex() const;
	const FAngelscriptCachedModuleInterface* TryGetModuleInterface() const;
	const FAngelscriptCachedTypeSchema* TryGetTypeSchema() const;
	const FAngelscriptCachedModuleState* TryGetModuleState() const;
	const FAngelscriptCachedFunctionBody* TryGetFunctionBody() const;
	const FAngelscriptCachedDebugSidecar* TryGetDebugSidecar() const;
	const FAngelscriptCachedModuleSnapshot* TryGetModuleSnapshot() const;

	TOptional<uint64> FindCapturedOffset(
		const FAngelscriptSourceIndexFieldCoordinate& Coordinate) const;
	TOptional<uint64> FindCapturedOffset(
		const FAngelscriptModuleInterfaceFieldCoordinate& Coordinate) const;
	TOptional<uint64> FindCapturedOffset(
		const FAngelscriptTypeSchemaFieldCoordinate& Coordinate) const;
	TOptional<uint64> FindCapturedOffset(
		const FAngelscriptModuleStateFieldCoordinate& Coordinate) const;
	TOptional<uint64> FindCapturedOffset(
		const FAngelscriptFunctionBodyFieldCoordinate& Coordinate) const;
	TOptional<uint64> FindCapturedOffset(
		const FAngelscriptDebugSidecarFieldCoordinate& Coordinate) const;
	TOptional<uint64> FindCapturedOffset(
		const FAngelscriptModuleSnapshotFieldCoordinate& Coordinate) const;

#if WITH_ANGELSCRIPT_UNITTESTS
	static uint64 MeasureExactControllerBaseAllocationForTests();
#endif

private:
	static TOptional<uint64> FindSourceIndexOffsetInStorage(
		const FSourceIndexCapturedOffsetStorage& Offsets,
		const FAngelscriptSourceIndexFieldCoordinate& Coordinate);
	static TOptional<uint64> FindModuleInterfaceOffsetInStorage(
		const FModuleInterfaceCapturedOffsetStorage& Offsets,
		const FAngelscriptModuleInterfaceFieldCoordinate& Coordinate);

	static FAngelscriptCacheValidationResult TryDecodeInternal(
		const FAngelscriptCacheRecordId& DeclaredRecordId,
		TConstArrayView<uint8> CanonicalPayload,
		const FAngelscriptCacheReadLimits& Limits,
		FAngelscriptCacheReadBudget& Budget,
		FAngelscriptCacheReadBudget::FDecodedCandidateTransaction& Candidate,
		bool bPromoteCandidate,
#if WITH_ANGELSCRIPT_UNITTESTS
		FAngelscriptCacheTypeSchemaAllocationProbeForTests* Probe,
#endif
		TOptional<TSharedRef<const FAngelscriptDecodedCacheRecord, ESPMode::ThreadSafe>>& OutRecord);

	FAngelscriptCacheRecordId RecordId;
	TArray<uint8> CanonicalPayload;
	FRecordVariant Record;

	friend struct AngelscriptCacheTypeSchema_Private::FDecodedRecordCodecBridge;
	friend struct AngelscriptCacheSemanticRecords_Private::FDecodedRecordCodecBridge;
	friend struct AngelscriptCacheRemainingRecords_Private::FDecodedRecordCodecBridge;
	friend class FAngelscriptDecodedCacheRecordBatch;

#if WITH_ANGELSCRIPT_UNITTESTS
	friend struct FAngelscriptDecodedCacheRecordTestAccess;
#endif
};

using FAngelscriptDecodedCacheRecordHandle =
	TSharedRef<const FAngelscriptDecodedCacheRecord, ESPMode::ThreadSafe>;

class ANGELSCRIPTRUNTIME_API FAngelscriptDecodedCacheRecordBatch final
{
public:
	FAngelscriptDecodedCacheRecordBatch(
		FAngelscriptCacheReadBudget& Budget,
		const FAngelscriptCacheReadLimits& Limits);
	~FAngelscriptDecodedCacheRecordBatch() = default;

	FAngelscriptDecodedCacheRecordBatch(
		const FAngelscriptDecodedCacheRecordBatch&) = delete;
	FAngelscriptDecodedCacheRecordBatch& operator=(
		const FAngelscriptDecodedCacheRecordBatch&) = delete;
	FAngelscriptDecodedCacheRecordBatch(
		FAngelscriptDecodedCacheRecordBatch&&) = delete;
	FAngelscriptDecodedCacheRecordBatch& operator=(
		FAngelscriptDecodedCacheRecordBatch&&) = delete;

	FAngelscriptCacheValidationResult TryDecode(
		const FAngelscriptCacheRecordId& DeclaredRecordId,
		TConstArrayView<uint8> CanonicalPayload,
		TOptional<FAngelscriptDecodedCacheRecordHandle>& OutRecord);
	bool PromoteToRetained();
	bool IsOpen() const;

private:
	FAngelscriptCacheReadLimits Limits;
	FAngelscriptCacheReadBudget& Budget;
	FAngelscriptCacheReadBudget::FDecodedCandidateTransaction Candidate;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheCurrentSymbol
{
	FAngelscriptHash256 CurrentAbi;
	TOptional<FAngelscriptHash256> CurrentContentOrValue;
	TOptional<EAngelscriptCacheValueStorageKind> CurrentValueStorageKind;
};

class ANGELSCRIPTRUNTIME_API IAngelscriptCacheCurrentSymbolResolver
{
public:
	virtual ~IAngelscriptCacheCurrentSymbolResolver() = default;
	virtual TOptional<FAngelscriptCacheCurrentSymbol> Resolve(
		EAngelscriptCacheReferenceKind ReferenceKind,
		const FAngelscriptHash256& StableKey) const = 0;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheOpaquePayloadValidationRequest
{
	EAngelscriptCacheOpaquePayloadKind Kind =
		EAngelscriptCacheOpaquePayloadKind::Invalid;
	uint32 CodecVersion = 0;
	FAngelscriptStableModuleKey ModuleKey;
	FAngelscriptHash256 OwnerKey;
	FAngelscriptArtifactProfileKey Profile;
	TConstArrayView<uint8> CanonicalPayload;
	TConstArrayView<FAngelscriptCacheSemanticDependency> DeclaredDependencies;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheRelocationUse
{
	uint32 InstructionOrdinal = 0;
	uint16 OperandSlot = 0;
	EAngelscriptCacheSemanticDependencyKind DependencyKind =
		EAngelscriptCacheSemanticDependencyKind::Invalid;
	EAngelscriptCacheReferenceKind ReferenceKind =
		static_cast<EAngelscriptCacheReferenceKind>(0);
	FAngelscriptHash256 StableKey;
	FAngelscriptHash256 ExpectedAbi;
	TOptional<FAngelscriptHash256> ExpectedContentOrValue;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheOwnedOpaqueBytes
{
	EAngelscriptCacheReferenceKind ReferenceKind =
		static_cast<EAngelscriptCacheReferenceKind>(0);
	FAngelscriptHash256 StableKey;
	TArray<uint8> CanonicalUtf8Bytes;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheOpaquePayloadSummary
{
	FAngelscriptHash256 ValidatedPayloadHash;
	TArray<FAngelscriptCacheRelocationUse> OrderedRelocations;
	TArray<FAngelscriptCachedDebugSourceReference> ExactDebugSources;
	TArray<FAngelscriptCacheOwnedOpaqueBytes> OwnedCanonicalBytes;
};

enum class EAngelscriptCacheCandidateChargeResult : uint8
{
	Success = 0,
	BudgetExceeded = 1,
	Overflow = 2,
	InvalidState = 3,
};

class ANGELSCRIPTRUNTIME_API IAngelscriptCacheCandidateChargeSink
{
public:
	virtual ~IAngelscriptCacheCandidateChargeSink() = default;
	virtual EAngelscriptCacheCandidateChargeResult TryExtend(
		uint64 RetainedCapacityBytes) = 0;
};

class ANGELSCRIPTRUNTIME_API IAngelscriptCacheOpaquePayloadValidator
{
public:
	virtual ~IAngelscriptCacheOpaquePayloadValidator() = default;
	virtual FAngelscriptCacheValidationResult Validate(
		const FAngelscriptCacheOpaquePayloadValidationRequest& Request,
		const FAngelscriptCacheReadLimits& Limits,
		FAngelscriptCacheReadBudget& Budget,
		IAngelscriptCacheCandidateChargeSink& GraphCandidate,
		FAngelscriptCacheOpaquePayloadSummary& OutSummary) const = 0;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheModuleGraphValidationContext
{
	FAngelscriptArtifactProfileKey SelectedProfile;
	FAngelscriptHash256 SelectedSourceSnapshot;
	const FAngelscriptDecodedCacheRecord* SourceIndex = nullptr;
	const IAngelscriptCacheCurrentSymbolResolver* CurrentSymbols = nullptr;
	const IAngelscriptCacheCurrentLayoutResolver* CurrentLayouts = nullptr;
	const IAngelscriptCacheOpaquePayloadValidator* OpaquePayloads = nullptr;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheValidatedRecordOrdinal
{
	FAngelscriptCacheRecordId RecordId;
	uint32 RecordOrdinal = 0;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheValidatedTypeOrdinal
{
	FAngelscriptStableTypeKey TypeKey;
	uint32 TypeSchemaRecordOrdinal = 0;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheValidatedGlobalOrdinal
{
	FAngelscriptStableGlobalKey GlobalKey;
	uint32 ModuleStateGlobalOrdinal = 0;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheValidatedFunctionOrdinal
{
	FAngelscriptStableFunctionKey FunctionKey;
	uint32 DeclarationOrdinal = 0;
	TOptional<uint32> BodyRecordOrdinal;
	TOptional<uint32> DebugRecordOrdinal;
	TOptional<uint32> BodySummaryOrdinal;
	TOptional<uint32> DebugSummaryOrdinal;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheValidatedInitializerOrdinal
{
	FAngelscriptStableFunctionKey InitializerKey;
	uint32 UnitOrdinal = 0;
	uint32 ExecuteActionOrdinal = 0;
	uint32 SummaryOrdinal = 0;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheOpaqueOwnerCoordinate
{
	EAngelscriptCacheOpaquePayloadKind Kind =
		EAngelscriptCacheOpaquePayloadKind::Invalid;
	FAngelscriptHash256 OwnerKey;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheValidatedOpaqueOwnerOrdinal
{
	FAngelscriptCacheOpaqueOwnerCoordinate Owner;
	uint32 SummaryOrdinal = 0;
};

class FAngelscriptValidatedModuleGraph;

ANGELSCRIPTRUNTIME_API FAngelscriptCacheValidationResult ValidateModuleSnapshotGraph(
	const FAngelscriptCacheRecordId& ModuleSnapshotRecordId,
	TConstArrayView<FAngelscriptDecodedCacheRecordHandle> LocallyValidatedRecords,
	const FAngelscriptCacheModuleGraphValidationContext& Context,
	const FAngelscriptCacheReadLimits& Limits,
	FAngelscriptCacheReadBudget& Budget,
	FAngelscriptValidatedModuleGraph& OutGraph);

class ANGELSCRIPTRUNTIME_API FAngelscriptValidatedModuleGraph
{
public:
	FAngelscriptValidatedModuleGraph() = default;
	FAngelscriptValidatedModuleGraph(const FAngelscriptValidatedModuleGraph&) = delete;
	FAngelscriptValidatedModuleGraph& operator=(
		const FAngelscriptValidatedModuleGraph&) = delete;
	FAngelscriptValidatedModuleGraph(FAngelscriptValidatedModuleGraph&&) noexcept = default;
	FAngelscriptValidatedModuleGraph& operator=(
		FAngelscriptValidatedModuleGraph&&) noexcept = default;

	void Reset();
	bool IsEmpty() const { return ReachableRecords.IsEmpty(); }
	const FAngelscriptStableModuleKey& GetModuleKey() const { return ModuleKey; }
	TConstArrayView<FAngelscriptDecodedCacheRecordHandle> GetReachableRecords() const
	{
		return ReachableRecords;
	}
	TConstArrayView<FAngelscriptCacheValidatedRecordOrdinal> GetRecordOrdinals() const
	{
		return RecordOrdinals;
	}
	TConstArrayView<FAngelscriptCacheValidatedTypeOrdinal> GetTypeOrdinals() const
	{
		return TypeOrdinals;
	}
	TConstArrayView<FAngelscriptCacheValidatedGlobalOrdinal> GetGlobalOrdinals() const
	{
		return GlobalOrdinals;
	}
	TConstArrayView<FAngelscriptCacheValidatedFunctionOrdinal> GetFunctionOrdinals() const
	{
		return FunctionOrdinals;
	}
	TConstArrayView<FAngelscriptCacheValidatedInitializerOrdinal> GetInitializerOrdinals() const
	{
		return InitializerOrdinals;
	}
	TConstArrayView<FAngelscriptCacheOpaquePayloadSummary> GetOpaqueSummaries() const
	{
		return OpaqueSummaries;
	}
	TConstArrayView<FAngelscriptCacheValidatedOpaqueOwnerOrdinal>
	GetOpaqueOwnerOrdinals() const
	{
		return OpaqueOwnerOrdinals;
	}

	TOptional<uint32> FindRecordOrdinal(
		const FAngelscriptCacheRecordId& RecordId) const;

private:
	FAngelscriptStableModuleKey ModuleKey;
	TArray<FAngelscriptDecodedCacheRecordHandle> ReachableRecords;
	TArray<FAngelscriptCacheValidatedRecordOrdinal> RecordOrdinals;
	TArray<FAngelscriptCacheValidatedTypeOrdinal> TypeOrdinals;
	TArray<FAngelscriptCacheValidatedGlobalOrdinal> GlobalOrdinals;
	TArray<FAngelscriptCacheValidatedFunctionOrdinal> FunctionOrdinals;
	TArray<FAngelscriptCacheValidatedInitializerOrdinal> InitializerOrdinals;
	TArray<FAngelscriptCacheOpaquePayloadSummary> OpaqueSummaries;
	TArray<FAngelscriptCacheValidatedOpaqueOwnerOrdinal> OpaqueOwnerOrdinals;

	friend ANGELSCRIPTRUNTIME_API FAngelscriptCacheValidationResult
	ValidateModuleSnapshotGraph(
		const FAngelscriptCacheRecordId& ModuleSnapshotRecordId,
		TConstArrayView<FAngelscriptDecodedCacheRecordHandle> LocallyValidatedRecords,
		const FAngelscriptCacheModuleGraphValidationContext& Context,
		const FAngelscriptCacheReadLimits& Limits,
		FAngelscriptCacheReadBudget& Budget,
		FAngelscriptValidatedModuleGraph& OutGraph);
};

#if WITH_ANGELSCRIPT_UNITTESTS
struct ANGELSCRIPTRUNTIME_API FAngelscriptDecodedCacheRecordTestAccess
{
	static FAngelscriptCacheValidationResult TryDecodeWithProbe(
		const FAngelscriptCacheRecordId& DeclaredRecordId,
		TConstArrayView<uint8> CanonicalPayload,
		const FAngelscriptCacheReadLimits& Limits,
		FAngelscriptCacheReadBudget& Budget,
		FAngelscriptCacheTypeSchemaAllocationProbeForTests& Probe,
		TOptional<FAngelscriptDecodedCacheRecordHandle>& OutRecord);
};
#endif
