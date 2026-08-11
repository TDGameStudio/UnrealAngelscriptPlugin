#pragma once

#include "Cache/AngelscriptCacheSemanticRecords.h"

namespace AngelscriptCacheTypeSchema_Private
{
	struct FDecodedRecordCodecBridge;
}

enum class EAngelscriptCachedTypeKind : uint8
{
	Invalid = 0,
	Class = 1,
	Struct = 2,
	Interface = 3,
	Enum = 4,
	Delegate = 5,
	Typedef = 6,
	Funcdef = 7,
};

enum class EAngelscriptCachedTypeSemanticFlags : uint32
{
	None = 0,
	Abstract = 0x01,
	Final = 0x02,
	Shared = 0x04,
	Generated = 0x08,
	HasDefaultConstructor = 0x10,
	HasDestructor = 0x20,
	ValueType = 0x40,
	ReferenceType = 0x80,
	KnownMask = 0xff,
};

enum class EAngelscriptCachedTypeRelationKind : uint8
{
	Invalid = 0,
	Base = 1,
	ShadowSuper = 2,
	CodeSuper = 3,
	ImplementedInterface = 4,
	Compose = 5,
};

enum class EAngelscriptCachedTypeLayoutInputKind : uint8
{
	Invalid = 0,
	BaseType = 1,
	CodeRoot = 2,
	StructHeader = 3,
};

enum class EAngelscriptCachedPropertyStorageKind : uint8
{
	Invalid = 0,
	InlineValue = 1,
	ObjectHandle = 2,
};

enum class EAngelscriptCachedMemberAccess : uint8
{
	Invalid = 0,
	Public = 1,
	Protected = 2,
	Private = 3,
};

enum class EAngelscriptCachedMethodSlotKind : uint8
{
	Invalid = 0,
	LocalMethod = 1,
	VirtualDeclaration = 2,
	VirtualOverride = 3,
	Inherited = 4,
};

enum class EAngelscriptCachedBehaviorKind : uint8
{
	Invalid = 0,
	Construct = 1,
	ListConstruct = 2,
	Destruct = 3,
	Factory = 4,
	ListFactory = 5,
	AddRef = 6,
	Release = 7,
	GetWeakRefFlag = 8,
	TemplateCallback = 9,
	GetRefCount = 10,
	SetGcFlag = 11,
	GetGcFlag = 12,
	EnumRefs = 13,
	ReleaseRefs = 14,
	Copy = 15,
	CopyConstruct = 16,
	CopyFactory = 17,
};

enum class EAngelscriptCachedReflectionKind : uint8
{
	Invalid = 0,
	None = 1,
	UClass = 2,
	UStruct = 3,
	UEnum = 4,
	UDelegate = 5,
};

enum class EAngelscriptCachedClassReflectionFlags : uint32
{
	None = 0,
	SuperIsCodeClass = 0x001,
	StaticsClass = 0x002,
	Abstract = 0x004,
	Transient = 0x008,
	HideDropdown = 0x010,
	DefaultToInstanced = 0x020,
	EditInlineNew = 0x040,
	Deprecated = 0x080,
	Placeable = 0x100,
	IsStruct = 0x200,
	KnownMask = 0x3ff,
};

enum class EAngelscriptCachedPropertySemanticFlags : uint32
{
	None = 0,
	HasUnrealProperty = 0x00001,
	BlueprintReadable = 0x00002,
	BlueprintWritable = 0x00004,
	EditableOnDefaults = 0x00008,
	EditableOnInstance = 0x00010,
	EditConst = 0x00020,
	InstancedReference = 0x00040,
	PersistentInstance = 0x00080,
	AdvancedDisplay = 0x00100,
	Transient = 0x00200,
	Replicated = 0x00400,
	SkipReplication = 0x00800,
	SkipSerialization = 0x01000,
	SaveGame = 0x02000,
	RepNotify = 0x04000,
	Config = 0x08000,
	Interp = 0x10000,
	AssetRegistrySearchable = 0x20000,
	NoClear = 0x40000,
	KnownMask = 0x7ffff,
};

enum class EAngelscriptCachedReplicationCondition : uint8
{
	None = 0,
	InitialOnly = 1,
	OwnerOnly = 2,
	SkipOwner = 3,
	SimulatedOnly = 4,
	AutonomousOnly = 5,
	SimulatedOrPhysics = 6,
	InitialOrOwner = 7,
	Custom = 8,
	ReplayOrOwner = 9,
	ReplayOnly = 10,
	SimulatedOnlyNoReplay = 11,
	SimulatedOrPhysicsNoReplay = 12,
	SkipReplay = 13,
	Dynamic = 14,
	Never = 15,
	NetGroup = 16,
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCachedTypeRelation
{
	EAngelscriptCachedTypeRelationKind RelationKind = EAngelscriptCachedTypeRelationKind::Invalid;
	TOptional<uint32> SemanticOrdinal;
	FAngelscriptCacheStableReference Target;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCachedTypeLayoutInput
{
	EAngelscriptCachedTypeLayoutInputKind InputKind = EAngelscriptCachedTypeLayoutInputKind::Invalid;
	FAngelscriptCacheStableReference Target;
	TOptional<uint32> BoundaryContribution;
	TOptional<uint32> AlignmentContribution;
	FAngelscriptHash256 LayoutInputHash;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCachedTypeLayoutExpectation
{
	uint64 SemanticSize = 0;
	uint32 SemanticAlignment = 0;
	uint32 BasePropertyBoundary = 0;
	FAngelscriptHash256 TypeLayoutHash;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCachedPropertySchema
{
	uint32 LayoutOrdinal = 0;
	uint32 SemanticByteOffset = 0;
	FAngelscriptStablePropertyKey PropertyKey;
	FString CanonicalName;
	FAngelscriptCachedDataType Type;
	EAngelscriptCachedPropertyStorageKind StorageKind = EAngelscriptCachedPropertyStorageKind::Invalid;
	uint32 SemanticStorageSize = 0;
	uint32 SemanticStorageAlignment = 0;
	FAngelscriptHash256 StorageLayoutHash;
	EAngelscriptCachedMemberAccess Access = EAngelscriptCachedMemberAccess::Invalid;
	uint32 PropertySemanticFlags = 0;
	EAngelscriptCachedReplicationCondition ReplicationCondition = EAngelscriptCachedReplicationCondition::None;
	TArray<FAngelscriptCachedMetadataEntry> Metadata;
	FAngelscriptHash256 PropertyLayoutFingerprint;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCachedMethodEntry
{
	EAngelscriptCachedMethodSlotKind EntryKind = EAngelscriptCachedMethodSlotKind::Invalid;
	uint32 MethodOrdinal = 0;
	FAngelscriptStableFunctionKey FunctionKey;
	FAngelscriptStableTypeKey DeclaringOwner;
	FAngelscriptHash256 ExpectedDeclarationAbi;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCachedVirtualFunctionSlot
{
	EAngelscriptCachedMethodSlotKind SlotKind = EAngelscriptCachedMethodSlotKind::Invalid;
	uint32 VftOrdinal = 0;
	FAngelscriptStableFunctionKey FunctionKey;
	FAngelscriptStableTypeKey DeclaringOwner;
	FAngelscriptStableTypeKey ImplementingOwner;
	FAngelscriptHash256 ExpectedDeclarationAbi;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCachedBehaviorSlot
{
	EAngelscriptCachedBehaviorKind BehaviorKind = EAngelscriptCachedBehaviorKind::Invalid;
	uint32 SlotOrdinal = 0;
	FAngelscriptCacheStableReference Target;
	TOptional<FAngelscriptStableTypeKey> DeclaringOwner;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCachedEnumEnumerator
{
	uint32 DeclarationOrdinal = 0;
	FString CanonicalName;
	int32 Value = 0;
	TArray<FAngelscriptCachedMetadataEntry> Metadata;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCachedEnumTypePayload
{
	TArray<FAngelscriptCachedEnumEnumerator> OrderedEnumerators;
	FAngelscriptHash256 EnumAuthorityHash;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCachedCallableTypePayload
{
	FAngelscriptStableFunctionKey SignatureFunctionKey;
	FAngelscriptHash256 ExpectedSignatureAbi;
	bool bMulticast = false;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCachedTypedefTypePayload
{
	FAngelscriptCachedDataType AliasedType;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCachedTypeKindPayload
{
	TOptional<FAngelscriptCachedEnumTypePayload> Enum;
	TOptional<FAngelscriptCachedCallableTypePayload> Callable;
	TOptional<FAngelscriptCachedTypedefTypePayload> Typedef;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCachedReflectedFunctionMember
{
	uint32 ReflectionOrdinal = 0;
	// Exact post-analysis Unreal/reflection-facing name. This is deliberately
	// independent from the script implementation name: BlueprintEvent and
	// BlueprintOverride commonly map Compute to Compute_Implementation.
	FString CanonicalFunctionName;
	// Pre-analysis reflection name retained by FAngelscriptFunctionDesc when a
	// parent event/display-name mapping rewrites CanonicalFunctionName. Empty
	// means the producer descriptor had no rewritten original name.
	FString CanonicalOriginalFunctionName;
	// Exact AngelScript function name resolved by Target.
	FString CanonicalScriptFunctionName;
	FAngelscriptCacheStableReference Target;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCachedTypeReflection
{
	EAngelscriptCachedReflectionKind ReflectionKind = EAngelscriptCachedReflectionKind::Invalid;
	uint32 ClassReflectionFlags = 0;
	TOptional<FString> ConfigName;
	TOptional<FString> StaticClassGlobalName;
	TArray<FAngelscriptCachedReflectedFunctionMember> OrderedUFunctionMembers;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCachedTypeSchema
{
	uint32 PayloadSchemaVersion = 0;
	FAngelscriptStableModuleKey ModuleKey;
	FAngelscriptStableTypeKey TypeKey;
	EAngelscriptCachedTypeKind TypeKind = EAngelscriptCachedTypeKind::Invalid;
	FString CanonicalNamespace;
	FString CanonicalName;
	FString CanonicalDeclaration;
	uint32 TypeSemanticFlags = 0;
	TArray<FAngelscriptCachedMetadataEntry> Metadata;
	TArray<FAngelscriptCachedTypeRelation> Relations;
	TArray<FAngelscriptCachedTypeLayoutInput> LayoutInputs;
	FAngelscriptCachedTypeLayoutExpectation Layout;
	TArray<FAngelscriptCachedPropertySchema> OrderedProperties;
	TArray<FAngelscriptCachedMethodEntry> OrderedMethods;
	TArray<FAngelscriptCachedVirtualFunctionSlot> VirtualFunctionTable;
	TArray<FAngelscriptCachedBehaviorSlot> OrderedBehaviorSlots;
	FAngelscriptCachedTypeKindPayload KindPayload;
	FAngelscriptCachedTypeReflection Reflection;
	TArray<FAngelscriptCacheSemanticDependency> Dependencies;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheV1StorageLayout
{
	uint32 SemanticStorageSize = 0;
	uint32 SemanticStorageAlignment = 0;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheV1BuildLayoutConstants
{
	uint32 AsSizeOfBool = 0;
	uint32 PointerByteWidth = 0;
	uint32 Int64Alignment = 0;
	uint32 DoubleAlignment = 0;
	uint32 ObjectHandleAlignment = 0;
	uint32 ObjectInitialAlignment = 0;
	uint32 TypeInfoInitialAlignment = 0;

	bool TryGetPrimitiveStorageLayout(
		EAngelscriptCachedPrimitiveType Primitive,
		FAngelscriptCacheV1StorageLayout& OutLayout) const;
	FAngelscriptCacheV1StorageLayout GetObjectHandleStorageLayout() const;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheResolvedDataTypeLayout
{
	EAngelscriptCachedPropertyStorageKind StorageKind = EAngelscriptCachedPropertyStorageKind::Invalid;
	uint32 SemanticStorageSize = 0;
	uint32 SemanticStorageAlignment = 0;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheResolvedTypeLayoutInput
{
	TOptional<uint32> BoundaryContribution;
	TOptional<uint32> AlignmentContribution;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheProspectiveTypeLayout
{
	EAngelscriptCachedTypeKind TypeKind = EAngelscriptCachedTypeKind::Invalid;
	uint64 SemanticSize = 0;
	uint32 SemanticAlignment = 0;
};

class ANGELSCRIPTRUNTIME_API IAngelscriptCacheProspectiveTypeLayoutView
{
public:
	virtual ~IAngelscriptCacheProspectiveTypeLayoutView() = default;
	virtual TOptional<FAngelscriptCacheProspectiveTypeLayout> FindLocalScriptTypeLayout(
		const FAngelscriptStableTypeKey& TypeKey) const = 0;
};

class ANGELSCRIPTRUNTIME_API IAngelscriptCacheCurrentLayoutResolver
{
public:
	virtual ~IAngelscriptCacheCurrentLayoutResolver() = default;
	virtual TOptional<FAngelscriptCacheResolvedDataTypeLayout> ResolveDataTypeLayout(
		const FAngelscriptCachedDataType& DataType,
		const IAngelscriptCacheProspectiveTypeLayoutView& LocalLayouts) const = 0;
	virtual TOptional<FAngelscriptCacheResolvedTypeLayoutInput> ResolveTypeLayoutInput(
		EAngelscriptCachedTypeLayoutInputKind InputKind,
		EAngelscriptCacheReferenceKind ReferenceKind,
		const FAngelscriptHash256& StableKey) const = 0;
};

enum class EAngelscriptTypeSchemaCapturedField : uint16
{
	Invalid = 0,
	PayloadSchemaVersion = 1,
	ModuleKey = 2,
	TypeKey = 3,
	TypeKind = 4,
	CanonicalNamespace = 5,
	CanonicalName = 6,
	CanonicalDeclaration = 7,
	TypeSemanticFlags = 8,
	Metadata = 9,
	MetadataEntry = 10,
	Relation = 11,
	RelationTarget = 12,
	LayoutInput = 13,
	LayoutInputTarget = 14,
	LayoutExpectation = 15,
	OrderedProperty = 16,
	PropertyKey = 17,
	PropertyType = 18,
	PropertyMetadata = 19,
	OrderedMethod = 20,
	MethodFunction = 21,
	MethodDeclaringOwner = 22,
	VirtualFunctionSlot = 23,
	VirtualFunction = 24,
	VirtualDeclaringOwner = 25,
	VirtualImplementingOwner = 26,
	BehaviorSlot = 27,
	BehaviorTarget = 28,
	BehaviorDeclaringOwner = 29,
	KindPayload = 30,
	EnumEnumerator = 31,
	EnumEnumeratorMetadata = 32,
	CallableSignature = 33,
	Reflection = 34,
	ReflectedFunctionMember = 35,
	ReflectedFunctionTarget = 36,
	Dependency = 37,
	DependencyTarget = 38,
	// Append-only reflection subfield coordinates.  The top-level Reflection
	// coordinate remains the form-closure fallback; these identify intrinsic
	// discriminator/flag failures at their exact physical fields.
	ReflectionKind = 39,
	ClassReflectionFlags = 40,
	ReflectedFunctionName = 41,
	ReflectedOriginalFunctionName = 42,
	ReflectedScriptFunctionName = 43,
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptTypeSchemaFieldCoordinate
{
	EAngelscriptTypeSchemaCapturedField Field = EAngelscriptTypeSchemaCapturedField::Invalid;
	uint32 PrimaryIndex = MAX_uint32;
	uint32 SecondaryIndex = MAX_uint32;
	uint32 TertiaryIndex = MAX_uint32;
};

#if WITH_ANGELSCRIPT_UNITTESTS
enum class EAngelscriptCacheTypeSchemaTestField : uint16
{
	PayloadSchemaVersion,
	ModuleKey,
	TypeKey,
	TypeKind,
	CanonicalNamespace,
	CanonicalNameBytes,
	CanonicalDeclaration,
	TypeSemanticFlags,
	Metadata,
	MetadataEntry,
	Relations,
	RelationKind,
	RelationSemanticOrdinalOptionalTag,
	LayoutInputs,
	LayoutInput,
	LayoutInputKind,
	LayoutInputBoundaryOptionalTag,
	LayoutInputAlignmentOptionalTag,
	Layout,
	LayoutSemanticSize,
	LayoutSemanticAlignment,
	LayoutBasePropertyBoundary,
	TypeLayoutHash,
	OrderedProperties,
	OrderedProperty,
	PropertyLayoutOrdinal,
	PropertySemanticByteOffset,
	PropertyKey,
	PropertyCanonicalName,
	PropertyType,
	PropertyStorageKind,
	PropertySemanticStorageSize,
	PropertySemanticStorageAlignment,
	PropertyStorageLayoutHash,
	PropertyMemberAccess,
	PropertySemanticFlags,
	PropertyReplicationCondition,
	PropertyMetadata,
	PropertyMetadataEntry,
	PropertyLayoutFingerprint,
	OrderedMethods,
	OrderedMethod,
	MethodSlotKind,
	MethodOrdinal,
	MethodFunctionKey,
	MethodDeclaringOwner,
	MethodExpectedDeclarationAbi,
	VirtualFunctionTable,
	BehaviorSlots,
	BehaviorSlot,
	BehaviorKind,
	BehaviorOrdinal,
	BehaviorTarget,
	BehaviorDeclaringOwnerOptionalTag,
	KindPayload,
	EnumEnumerator,
	EnumDeclarationOrdinal,
	EnumCanonicalName,
	EnumSignedValue,
	EnumEnumeratorMetadata,
	EnumEnumeratorMetadataEntry,
	EnumAuthorityHash,
	CallableSignatureFunctionKey,
	CallableExpectedSignatureAbi,
	CallableMulticastBoolean,
	DataTypeKind,
	DataTypePrimitive,
	DataTypeTypeReferenceOptionalTag,
	DataTypeQualifierFlags,
	DataTypeOrderedSubTypes,
	Reflection,
	ReflectionKind,
	ClassReflectionFlags,
	ReflectionConfigNameOptionalTag,
	ReflectionStaticClassGlobalNameOptionalTag,
	ReflectedFunctionMembers,
	ReflectedFunctionNameBytes,
	ReflectedOriginalFunctionNameBytes,
	ReflectedScriptFunctionNameBytes,
	Dependencies,
	Dependency,
	DependencyExpectedContentOrValueOptionalTag,
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheTypeSchemaTestWireSpan
{
	EAngelscriptCacheTypeSchemaTestField Field = EAngelscriptCacheTypeSchemaTestField::PayloadSchemaVersion;
	int32 PrimaryIndex = INDEX_NONE;
	int32 SecondaryIndex = INDEX_NONE;
	int32 TertiaryIndex = INDEX_NONE;
	uint64 Offset = 0;
	uint64 Size = 0;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheTypeSchemaTestWireTrace
{
	TArray<FAngelscriptCacheTypeSchemaTestWireSpan> Spans;

	TConstArrayView<FAngelscriptCacheTypeSchemaTestWireSpan> GetAllV1Spans() const
	{
		return Spans;
	}
	TOptional<FAngelscriptCacheTypeSchemaTestWireSpan> FindUnique(
		EAngelscriptCacheTypeSchemaTestField Field,
		int32 PrimaryIndex = INDEX_NONE,
		int32 SecondaryIndex = INDEX_NONE,
		int32 TertiaryIndex = INDEX_NONE) const;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptTypeSchemaCapturedOffsetEntryForTests
{
	FAngelscriptTypeSchemaFieldCoordinate Coordinate;
	uint64 Offset = 0;
};

enum class EAngelscriptCacheTypeSchemaProbeEventKindForTests : uint8
{
	Allocation = 1,
	CandidatePromotion = 2,
	ValidationCheckpoint = 3,
	InjectedOverflowCheckpoint = 4,
};

enum class EAngelscriptCacheTypeSchemaInjectedFailureForTests : uint8
{
	PhysicalAfterTarget = 1,
	LocalAfterTarget = 2,
	HashAfterTarget = 3,
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheTypeSchemaProbeEventForTests
{
	EAngelscriptCacheTypeSchemaProbeEventKindForTests Kind =
		EAngelscriptCacheTypeSchemaProbeEventKindForTests::Allocation;
	uint64 SequenceOrdinal = 0;
	uint64 FieldOffset = 0;
	int32 RequestedElementCount = 0;
	uint64 ElementSize = 0;
	uint64 ElementAlignment = 0;
	uint64 ReservedCapacity = 0;
	uint64 AllocatedBytes = 0;
	uint64 TotalChargeBytes = 0;
	uint64 ResidentChargeBytes = 0;
	uint64 TemporaryChargeBytes = 0;
	uint64 TemporaryBytesBefore = 0;
	uint64 ResidentBytesAfter = 0;
	uint64 TotalDecodedBytes = 0;
	int32 AcceptedAllocationEventCount = 0;
	uint64 AllocationAttemptCount = 0;
	bool bHandleWasObservable = false;
};

class ANGELSCRIPTRUNTIME_API FAngelscriptCacheTypeSchemaAllocationProbeForTests
{
public:
	FAngelscriptCacheTypeSchemaAllocationProbeForTests(
		TArrayView<FAngelscriptCacheTypeSchemaProbeEventForTests> InEventStorage,
		int32& InOutEventCount,
		bool& bInOutOverflowed);
	FAngelscriptCacheTypeSchemaAllocationProbeForTests() = delete;
	FAngelscriptCacheTypeSchemaAllocationProbeForTests(
		const FAngelscriptCacheTypeSchemaAllocationProbeForTests&) = delete;
	FAngelscriptCacheTypeSchemaAllocationProbeForTests& operator=(
		const FAngelscriptCacheTypeSchemaAllocationProbeForTests&) = delete;

	uint64 GetTotalAllocationAttempts() const { return TotalAllocationAttempts; }
	uint64 GetTotalAllocatedBytes() const { return TotalAllocatedBytes; }
	uint64 GetRejectedReservationCount() const { return RejectedReservationCount; }
	uint64 GetLiveAllocatedBytes() const { return LiveAllocatedBytes; }
	uint64 GetPeakLiveAllocatedBytes() const { return PeakLiveAllocatedBytes; }
	int64 GetAllocationBalance() const { return AllocationBalance; }
	uint64 GetDecodedRecordControllerAllocatedBytes() const
	{
		return DecodedRecordControllerAllocatedBytes;
	}
	uint64 GetDecodedRecordControllerAllocationCount() const
	{
		return DecodedRecordControllerAllocationCount;
	}
	bool UsedSingleMakeSharedAllocation() const
	{
		return DecodedRecordControllerAllocationCount == 1;
	}

	void InjectOverflowAfterValidationCheckpointForTests(uint64 CheckpointOrdinal);
	void InjectOverflowAfterAcceptedEventForTests(
		int32 AcceptedEventIndex,
		EAngelscriptCacheTypeSchemaInjectedFailureForTests FailureMode);

private:
	friend class FAngelscriptDecodedCacheRecord;
	friend struct FAngelscriptDecodedCacheRecordTestAccess;
	friend struct AngelscriptCacheTypeSchema_Private::FDecodedRecordCodecBridge;

	void BeginDecodeForTests();
	void Record(const FAngelscriptCacheTypeSchemaProbeEventForTests& Event);
	bool RecordAcceptedAllocationForTests(
		const AngelscriptCacheCanonicalCodecTestHooks::FAllocationEvent& Event);
	void RecordRejectedReservationForTests();
	bool RecordValidationCheckpointForTests(
		uint64 CheckpointOrdinal,
		const FAngelscriptCacheReadBudget& Budget);
	bool ConsumeDeferredAcceptedEventFailureForTests(
		EAngelscriptCacheTypeSchemaInjectedFailureForTests FailureMode,
		uint64& OutOffset);
	void CloseLiveObservationForTests();

	TArrayView<FAngelscriptCacheTypeSchemaProbeEventForTests> EventStorage;
	int32& EventCount;
	bool& bOverflowed;
	uint64 TotalAllocationAttempts = 0;
	uint64 TotalAllocatedBytes = 0;
	uint64 RejectedReservationCount = 0;
	uint64 LiveAllocatedBytes = 0;
	uint64 PeakLiveAllocatedBytes = 0;
	int64 AllocationBalance = 0;
	uint64 DecodedRecordControllerAllocatedBytes = 0;
	uint64 DecodedRecordControllerAllocationCount = 0;
	uint64 InjectAfterValidationCheckpoint = MAX_uint64;
	int32 InjectAfterAcceptedEvent = INDEX_NONE;
	EAngelscriptCacheTypeSchemaInjectedFailureForTests InjectedFailureMode =
		EAngelscriptCacheTypeSchemaInjectedFailureForTests::PhysicalAfterTarget;
	uint64 TriggeredAcceptedEventOrdinal = MAX_uint64;
	uint64 TriggeredAcceptedEventOffset = 0;
	bool bInjectedOverflowRecorded = false;
};
#endif

class ANGELSCRIPTRUNTIME_API FAngelscriptCacheTypeSchemaArchive
{
public:
	static constexpr uint32 TypeSchemaPayloadSchemaVersion = 2;

	static FAngelscriptCacheValidationResult SerializeTypeSchema(
		const FAngelscriptCachedTypeSchema& Value,
		TArray<uint8>& OutPayload);
	static FAngelscriptCacheValidationResult SerializeTypeSchemaWithDiagnostics(
		const FAngelscriptCachedTypeSchema& Value,
		TArray<uint8>& OutPayload,
		FAngelscriptTypeSchemaFieldCoordinate& OutFailureCoordinate);
#if WITH_ANGELSCRIPT_UNITTESTS
	static FAngelscriptCacheValidationResult SerializeTypeSchemaPhysicalForTests(
		const FAngelscriptCachedTypeSchema& Value,
		TArray<uint8>& OutPayload,
		FAngelscriptCacheTypeSchemaTestWireTrace& OutTrace);
#endif

	static FAngelscriptCacheValidationResult ComputeLayoutInputHash(
		const FAngelscriptCachedTypeLayoutInput& Value,
		FAngelscriptHash256& OutHash);
	static FAngelscriptCacheValidationResult ComputeStorageLayoutHash(
		const FAngelscriptCachedDataType& DataType,
		EAngelscriptCachedPropertyStorageKind StorageKind,
		uint32 SemanticStorageSize,
		uint32 SemanticStorageAlignment,
		FAngelscriptHash256& OutHash);
	static FAngelscriptCacheValidationResult ComputePropertyLayoutFingerprint(
		const FAngelscriptStableTypeKey& OwnerTypeKey,
		const FAngelscriptCachedPropertySchema& Property,
		FAngelscriptHash256& OutHash);
	static FAngelscriptCacheValidationResult ComputeEnumAuthorityHash(
		const FAngelscriptStableTypeKey& OwnerTypeKey,
		const FAngelscriptCachedEnumTypePayload& EnumPayload,
		FAngelscriptHash256& OutHash);
	static FAngelscriptCacheValidationResult ComputeTypeLayoutHash(
		const FAngelscriptCachedTypeSchema& Value,
		FAngelscriptHash256& OutHash);

	static int32 CompareDependencies(
		const FAngelscriptCacheSemanticDependency& A,
		const FAngelscriptCacheSemanticDependency& B);
	static int32 CompareMetadata(
		const FAngelscriptCachedMetadataEntry& A,
		const FAngelscriptCachedMetadataEntry& B);
	static const FAngelscriptCacheV1BuildLayoutConstants& GetV1BuildLayoutConstants();
};
