#pragma once

#include "Cache/AngelscriptCacheSemanticRecords.h"

enum class EAngelscriptCachedGlobalInitializationKind : uint8
{
	Invalid = 0,
	Default = 1,
	PureConstant = 2,
	VmInitializer = 3,
};

enum class EAngelscriptCachedGlobalCleanupPolicy : uint8
{
	Invalid = 0,
	None = 1,
	DestroyValue = 2,
	ReleaseHandle = 3,
};

enum class EAngelscriptCachedHardValueKind : uint8
{
	Invalid = 0,
	GlobalConstant = 1,
	EnumAuthority = 2,
};

enum class EAngelscriptCachedInitializerKind : uint8
{
	Invalid = 0,
	Global = 1,
	Module = 2,
};

enum class EAngelscriptCachedInitializationActionKind : uint8
{
	Invalid = 0,
	DefaultConstructGlobal = 1,
	ExecuteInitializer = 2,
};

enum class EAngelscriptCachedCanonicalValueKind : uint8
{
	Invalid = 0,
	Bool = 1,
	SignedInteger = 2,
	UnsignedInteger = 3,
	Float32 = 4,
	Float64 = 5,
	EnumInt32 = 6,
};

enum class EAngelscriptCachedFunctionInvocationKind : uint8
{
	Invalid = 0,
	GlobalFunction = 1,
	Method = 2,
	Constructor = 3,
	Destructor = 4,
	Factory = 5,
	GeneratedDefaultConstructor = 6,
	GeneratedDefaultDestructor = 7,
	InitDefaults = 8,
	PublicSingleFunction = 9,
	Lambda = 10,
};

enum class EAngelscriptCacheValueStorageKind : uint8
{
	Invalid = 0,
	Trivial = 1,
	OwningValue = 2,
	ReferenceCounted = 3,
};

enum class EAngelscriptCacheOpaquePayloadKind : uint8
{
	Invalid = 0,
	FunctionExecution = 1,
	InitializerExecution = 2,
	Debug = 3,
};

enum class EAngelscriptModuleStateCapturedField : uint16
{
	Invalid = 0,

	PayloadSchemaVersion = 1,
	ModuleKey = 2,
	Profile = 3,
	StateInputHash = 4,

	OrderedGlobals = 5,
	Global = 6,
	GlobalStorageOrdinal = 7,
	GlobalKey = 8,
	GlobalCanonicalNamespace = 9,
	GlobalCanonicalName = 10,
	GlobalTypeNode = 11,
	GlobalTypeKind = 12,
	GlobalTypePrimitive = 13,
	GlobalTypeReferencePresence = 14,
	GlobalTypeReference = 15,
	GlobalTypeReferenceKind = 16,
	GlobalTypeReferenceStableKey = 17,
	GlobalTypeReferenceExpectedAbi = 18,
	GlobalTypeQualifierFlags = 19,
	GlobalTypeOrderedSubTypes = 20,
	GlobalTraitFlags = 21,
	GlobalInitializationKind = 22,
	GlobalCleanupPolicy = 23,
	GlobalStorageLayoutFingerprint = 24,

	HardValues = 25,
	HardValue = 26,
	HardValueKind = 27,
	HardValueOwner = 28,
	HardValueOwnerReferenceKind = 29,
	HardValueOwnerStableKey = 30,
	HardValueOwnerExpectedAbi = 31,
	HardValueTypeNode = 32,
	HardValueTypeKind = 33,
	HardValueTypePrimitive = 34,
	HardValueTypeReferencePresence = 35,
	HardValueTypeReference = 36,
	HardValueTypeReferenceKind = 37,
	HardValueTypeReferenceStableKey = 38,
	HardValueTypeReferenceExpectedAbi = 39,
	HardValueTypeQualifierFlags = 40,
	HardValueTypeOrderedSubTypes = 41,
	HardValueCanonicalValuePresence = 42,
	HardValueCanonicalValue = 43,
	HardValueCanonicalValueKind = 44,
	HardValueCanonicalValueFixedWidthValueBytes = 45,
	HardValueHash = 46,

	Initializers = 47,
	Initializer = 48,
	InitializerKind = 49,
	InitializerKey = 50,
	InitializerOwnerGlobalPresence = 51,
	InitializerOwnerGlobal = 52,
	InitializerVmInitializerCodecVersion = 53,
	InitializerExecutionHash = 54,
	InitializerCanonicalExecutionPayload = 55,

	OrderedInitializationActions = 56,
	InitializationAction = 57,
	InitializationActionOrdinal = 58,
	InitializationActionKind = 59,
	InitializationActionTarget = 60,
	InitializationActionTargetReferenceKind = 61,
	InitializationActionTargetStableKey = 62,
	InitializationActionTargetExpectedAbi = 63,
	InitializationActionDependencies = 64,
	InitializationActionDependency = 65,
	InitializationActionDependencyKind = 66,
	InitializationActionDependencyTarget = 67,
	InitializationActionDependencyTargetReferenceKind = 68,
	InitializationActionDependencyTargetStableKey = 69,
	InitializationActionDependencyTargetExpectedAbi = 70,
	InitializationActionDependencyExpectedContentOrValuePresence = 71,
	InitializationActionDependencyExpectedContentOrValue = 72,

	OrderedPostInitFunctions = 73,
	PostInitFunction = 74,
	PostInitOrdinal = 75,
	PostInitFunctionReference = 76,
	PostInitFunctionReferenceKind = 77,
	PostInitFunctionStableKey = 78,
	PostInitFunctionExpectedAbi = 79,

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

enum class EAngelscriptFunctionBodyCapturedField : uint16
{
	Invalid = 0,

	PayloadSchemaVersion = 1,
	ModuleKey = 2,
	Identity = 3,
	IdentityFunctionKey = 4,
	IdentityContent = 5,
	IdentityContentExecution = 6,
	IdentityContentDebug = 7,
	IdentityProfile = 8,
	ExpectedDeclarationAbi = 9,
	FunctionSourceDigest = 10,
	FunctionInputDigest = 11,
	InvocationKind = 12,
	VmExecutionCodecVersion = 13,
	CanonicalExecutionPayload = 14,
	ActualDependencies = 15,
	ActualDependency = 16,
	ActualDependencyKind = 17,
	ActualDependencyTarget = 18,
	ActualDependencyTargetReferenceKind = 19,
	ActualDependencyTargetStableKey = 20,
	ActualDependencyTargetExpectedAbi = 21,
	ActualDependencyExpectedContentOrValuePresence = 22,
	ActualDependencyExpectedContentOrValue = 23,
	DebugSidecarPresence = 24,
	DebugSidecar = 25,
	DebugSidecarKind = 26,
	DebugSidecarContentHash = 27,
};

enum class EAngelscriptDebugSidecarCapturedField : uint16
{
	Invalid = 0,

	PayloadSchemaVersion = 1,
	FunctionKey = 2,
	Profile = 3,
	DebugHash = 4,
	VmDebugCodecVersion = 5,
	Sources = 6,
	Source = 7,
	SourceFileKey = 8,
	SourceLogicalSectionKey = 9,
	SourceCanonicalLogicalSection = 10,
	CanonicalDebugPayload = 11,
};

enum class EAngelscriptModuleSnapshotCapturedField : uint16
{
	Invalid = 0,

	PayloadSchemaVersion = 1,
	ModuleKey = 2,
	ModuleInterface = 3,
	ModuleInterfaceModuleKey = 4,
	ModuleInterfaceRecordId = 5,
	ModuleInterfaceRecordIdKind = 6,
	ModuleInterfaceRecordIdContentHash = 7,
	TypeSchemas = 8,
	TypeSchemaLink = 9,
	TypeSchemaLinkTypeKey = 10,
	TypeSchemaLinkRecordId = 11,
	TypeSchemaLinkRecordIdKind = 12,
	TypeSchemaLinkRecordIdContentHash = 13,
	ModuleState = 14,
	ModuleStateModuleKey = 15,
	ModuleStateRecordId = 16,
	ModuleStateRecordIdKind = 17,
	ModuleStateRecordIdContentHash = 18,
	FunctionBodies = 19,
	FunctionBodyLink = 20,
	FunctionBodyLinkFunctionKey = 21,
	FunctionBodyLinkRecordId = 22,
	FunctionBodyLinkRecordIdKind = 23,
	FunctionBodyLinkRecordIdContentHash = 24,
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptModuleStateFieldCoordinate
{
	EAngelscriptModuleStateCapturedField Field =
		EAngelscriptModuleStateCapturedField::Invalid;
	uint32 PrimaryIndex = MAX_uint32;
	uint32 SecondaryIndex = MAX_uint32;
	uint32 TertiaryIndex = MAX_uint32;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptFunctionBodyFieldCoordinate
{
	EAngelscriptFunctionBodyCapturedField Field =
		EAngelscriptFunctionBodyCapturedField::Invalid;
	uint32 PrimaryIndex = MAX_uint32;
	uint32 SecondaryIndex = MAX_uint32;
	uint32 TertiaryIndex = MAX_uint32;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptDebugSidecarFieldCoordinate
{
	EAngelscriptDebugSidecarCapturedField Field =
		EAngelscriptDebugSidecarCapturedField::Invalid;
	uint32 PrimaryIndex = MAX_uint32;
	uint32 SecondaryIndex = MAX_uint32;
	uint32 TertiaryIndex = MAX_uint32;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptModuleSnapshotFieldCoordinate
{
	EAngelscriptModuleSnapshotCapturedField Field =
		EAngelscriptModuleSnapshotCapturedField::Invalid;
	uint32 PrimaryIndex = MAX_uint32;
	uint32 SecondaryIndex = MAX_uint32;
	uint32 TertiaryIndex = MAX_uint32;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCachedGlobalSchema
{
	uint32 StorageOrdinal = 0;
	FAngelscriptStableGlobalKey GlobalKey;
	FString CanonicalNamespace;
	FString CanonicalName;
	FAngelscriptCachedDataType Type;
	uint32 GlobalTraitFlags = 0;
	EAngelscriptCachedGlobalInitializationKind InitializationKind =
		EAngelscriptCachedGlobalInitializationKind::Invalid;
	EAngelscriptCachedGlobalCleanupPolicy CleanupPolicy =
		EAngelscriptCachedGlobalCleanupPolicy::Invalid;
	FAngelscriptHash256 StorageLayoutFingerprint;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCachedCanonicalValue
{
	EAngelscriptCachedCanonicalValueKind ValueKind =
		EAngelscriptCachedCanonicalValueKind::Invalid;
	TArray<uint8> FixedWidthValueBytes;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCachedHardValue
{
	EAngelscriptCachedHardValueKind HardValueKind =
		EAngelscriptCachedHardValueKind::Invalid;
	FAngelscriptCacheStableReference Owner;
	FAngelscriptCachedDataType Type;
	TOptional<FAngelscriptCachedCanonicalValue> CanonicalValue;
	FAngelscriptHash256 HardValueHash;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCachedInitializerUnit
{
	EAngelscriptCachedInitializerKind InitializerKind =
		EAngelscriptCachedInitializerKind::Invalid;
	FAngelscriptStableFunctionKey InitializerKey;
	TOptional<FAngelscriptStableGlobalKey> OwnerGlobal;
	uint32 VmInitializerCodecVersion = 0;
	FAngelscriptHash256 InitializerExecutionHash;
	TArray<uint8> CanonicalExecutionPayload;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCachedInitializationAction
{
	uint32 ActionOrdinal = 0;
	EAngelscriptCachedInitializationActionKind ActionKind =
		EAngelscriptCachedInitializationActionKind::Invalid;
	FAngelscriptCacheStableReference Target;
	TArray<FAngelscriptCacheSemanticDependency> Dependencies;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCachedPostInitFunction
{
	uint32 PostInitOrdinal = 0;
	FAngelscriptCacheStableReference Function;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCachedModuleState
{
	uint32 PayloadSchemaVersion = 0;
	FAngelscriptStableModuleKey ModuleKey;
	FAngelscriptArtifactProfileKey Profile;
	FAngelscriptHash256 StateInputHash;
	TArray<FAngelscriptCachedGlobalSchema> OrderedGlobals;
	TArray<FAngelscriptCachedHardValue> HardValues;
	TArray<FAngelscriptCachedInitializerUnit> Initializers;
	TArray<FAngelscriptCachedInitializationAction> OrderedInitializationActions;
	TArray<FAngelscriptCachedPostInitFunction> OrderedPostInitFunctions;
	TArray<FAngelscriptCacheSemanticDependency> Dependencies;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCachedFunctionBody
{
	uint32 PayloadSchemaVersion = 0;
	FAngelscriptStableModuleKey ModuleKey;
	FAngelscriptFunctionArtifactIdentity Identity;
	FAngelscriptHash256 ExpectedDeclarationAbi;
	FAngelscriptFunctionSourceDigest FunctionSourceDigest;
	FAngelscriptFunctionInputDigest FunctionInputDigest;
	EAngelscriptCachedFunctionInvocationKind InvocationKind =
		EAngelscriptCachedFunctionInvocationKind::Invalid;
	uint32 VmExecutionCodecVersion = 0;
	TArray<uint8> CanonicalExecutionPayload;
	TArray<FAngelscriptCacheSemanticDependency> ActualDependencies;
	TOptional<FAngelscriptCacheRecordId> DebugSidecar;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCachedLogicalSectionKey
{
	FAngelscriptHash256 Hash;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCachedDebugSourceReference
{
	FAngelscriptCachedSourceFileKey SourceFileKey;
	FAngelscriptCachedLogicalSectionKey LogicalSectionKey;
	FString CanonicalLogicalSection;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCachedDebugSidecar
{
	uint32 PayloadSchemaVersion = 0;
	FAngelscriptStableFunctionKey FunctionKey;
	FAngelscriptArtifactProfileKey Profile;
	FAngelscriptHash256 DebugHash;
	uint32 VmDebugCodecVersion = 0;
	TArray<FAngelscriptCachedDebugSourceReference> Sources;
	TArray<uint8> CanonicalDebugPayload;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCachedModuleRecordLink
{
	FAngelscriptStableModuleKey ModuleKey;
	FAngelscriptCacheRecordId RecordId;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCachedTypeSchemaLink
{
	FAngelscriptStableTypeKey TypeKey;
	FAngelscriptCacheRecordId RecordId;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCachedFunctionBodyLink
{
	FAngelscriptStableFunctionKey FunctionKey;
	FAngelscriptCacheRecordId RecordId;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCachedModuleSnapshot
{
	uint32 PayloadSchemaVersion = 0;
	FAngelscriptStableModuleKey ModuleKey;
	FAngelscriptCachedModuleRecordLink ModuleInterface;
	TArray<FAngelscriptCachedTypeSchemaLink> TypeSchemas;
	FAngelscriptCachedModuleRecordLink ModuleState;
	TArray<FAngelscriptCachedFunctionBodyLink> FunctionBodies;
};

class ANGELSCRIPTRUNTIME_API FAngelscriptCacheRemainingRecordArchive
{
public:
	static constexpr uint32 ModuleStatePayloadSchemaVersion = 1;
	static constexpr uint32 FunctionBodyPayloadSchemaVersion = 1;
	static constexpr uint32 DebugSidecarPayloadSchemaVersion = 1;
	static constexpr uint32 ModuleSnapshotPayloadSchemaVersion = 1;

	static FAngelscriptCacheValidationResult TryBuildLogicalSectionKey(
		const FAngelscriptCachedSourceFileKey& SourceFileKey,
		FStringView CanonicalLogicalSection,
		FAngelscriptCachedLogicalSectionKey& OutKey);

	static FAngelscriptCacheValidationResult SerializeDebugSidecar(
		const FAngelscriptCachedDebugSidecar& Value,
		TArray<uint8>& OutPayload);

	static FAngelscriptCacheValidationResult ComputeModuleStateInputHash(
		const FAngelscriptCachedModuleState& Value,
		FAngelscriptHash256& OutHash);

	static FAngelscriptCacheValidationResult ComputeGlobalStorageLayoutFingerprint(
		const FAngelscriptStableModuleKey& ModuleKey,
		const FAngelscriptCachedGlobalSchema& Global,
		FAngelscriptHash256& OutHash);

	static FAngelscriptCacheValidationResult ComputeGlobalConstantHardValueHash(
		const FAngelscriptCachedHardValue& HardValue,
		FAngelscriptHash256& OutHash);

	static FAngelscriptCacheValidationResult ComputeInitializerExecutionHash(
		const FAngelscriptStableModuleKey& ModuleKey,
		const FAngelscriptArtifactProfileKey& Profile,
		const FAngelscriptCachedInitializerUnit& Initializer,
		FAngelscriptHash256& OutHash);

	static FAngelscriptCacheValidationResult SerializeModuleState(
		const FAngelscriptCachedModuleState& Value,
		TArray<uint8>& OutPayload);

	static FAngelscriptCacheValidationResult SerializeFunctionBody(
		const FAngelscriptCachedFunctionBody& Value,
		TArray<uint8>& OutPayload);

	static FAngelscriptCacheValidationResult SerializeModuleSnapshot(
		const FAngelscriptCachedModuleSnapshot& Value,
		TArray<uint8>& OutPayload);
};
