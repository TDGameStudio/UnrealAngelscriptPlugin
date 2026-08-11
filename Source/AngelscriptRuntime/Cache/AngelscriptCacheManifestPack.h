#pragma once

#include "Cache/AngelscriptCacheDecodedRecord.h"

enum class EAngelscriptCachePackCompressionPolicy : uint8
{
	Auto = 1,
	ForceNoneForTest = 2,
	ForceZlibForTest = 3,
};

enum class EAngelscriptCachePreparationExecutionMode : uint8
{
	ForcedSerial = 1,
	BoundedParallel = 2,
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptPreparedRecord
{
	FAngelscriptCacheRecordId RecordId;
	TArray<uint8> CanonicalPayload;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptPreparedRecordCompletion
{
	uint32 PreparationOrdinal = 0;
	uint32 CompletionOrdinal = 0;
	FAngelscriptPreparedRecord Record;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCachePackPolicy
{
	static constexpr uint64 DefaultTargetRawBytesPerPack =
		UINT64_C(64) * 1024 * 1024;

	uint64 TargetRawBytesPerPack = DefaultTargetRawBytesPerPack;
	EAngelscriptCachePackCompressionPolicy CompressionPolicy =
		EAngelscriptCachePackCompressionPolicy::Auto;
	// ForcedSerial is the deterministic oracle used by tests/benchmarks. Runtime
	// explicitly selects BoundedParallel after Engine-owned mutable data has been
	// frozen into immutable record DTOs.
	EAngelscriptCachePreparationExecutionMode ExecutionMode =
		EAngelscriptCachePreparationExecutionMode::ForcedSerial;
	// Applies only to BoundedParallel. The implementation never schedules more
	// workers than records or independent Pack groups. A zero value is invalid.
	uint32 MaxWorkerCount = 1;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCachePackIndexEntry
{
	FAngelscriptCacheRecordId RecordId;
	EAngelscriptCacheCodec Codec = EAngelscriptCacheCodec::None;
	uint64 PackOffset = 0;
	uint64 StoredSize = 0;
	uint64 RawSize = 0;
	FAngelscriptHash256 RawChecksum;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptEncodedPack
{
	FAngelscriptHash256 PackId;
	TArray<uint8> Bytes;
	TArray<FAngelscriptCachePackIndexEntry> Index;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCachePackLocation
{
	FAngelscriptHash256 PackId;
	uint64 PackOffset = 0;
	uint64 StoredSize = 0;
	uint64 RawSize = 0;
	EAngelscriptCacheCodec Codec = EAngelscriptCacheCodec::None;
	FAngelscriptHash256 RawChecksum;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheRecordIndexEntry
{
	FAngelscriptCacheRecordId RecordId;
	FAngelscriptCachePackLocation Location;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheModuleSnapshotLink
{
	FAngelscriptStableModuleKey ModuleKey;
	FAngelscriptCacheRecordId RecordId;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheGenerationManifest
{
	uint32 ManifestSchemaVersion = 0;
	uint32 ManifestFlags = 0;
	FAngelscriptCacheCompatibilityKey Compatibility;
	FAngelscriptCacheContextKey Context;
	FAngelscriptArtifactProfileKey Profile;
	FAngelscriptHash256 SourceSnapshot;
	FAngelscriptCacheRecordId SourceIndexRecordId;
	TArray<FAngelscriptCacheModuleSnapshotLink> ModuleSnapshots;
	TArray<FAngelscriptCacheRecordIndexEntry> Records;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptEncodedCacheGenerationManifest
{
	TArray<uint8> CompleteBytes;
	FAngelscriptHash256 ComputedGenerationId;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptValidatedGeneration
{
	FAngelscriptCacheGenerationManifest Manifest;
	TArray<FAngelscriptDecodedCacheRecordHandle> ReachableRecords;
};

class ANGELSCRIPTRUNTIME_API IAngelscriptCacheStorageCodec
{
public:
	virtual ~IAngelscriptCacheStorageCodec() = default;
	virtual bool TryCompressCanonicalZlib(
		TConstArrayView<uint8> RawBytes,
		TArray<uint8>& OutStoredBytes) = 0;
	virtual bool TryCompressCanonicalZlibInto(
		TConstArrayView<uint8> RawBytes,
		TArrayView<uint8> StoredOutput,
		uint64& OutProducedBytes) = 0;
	virtual bool TryDecompressCanonicalZlib(
		TConstArrayView<uint8> StoredBytes,
		TArrayView<uint8> RawOutput,
		uint64& OutProducedBytes) = 0;
};

class ANGELSCRIPTRUNTIME_API FAngelscriptUnrealZlibCacheStorageCodec final
	: public IAngelscriptCacheStorageCodec
{
public:
	virtual bool TryCompressCanonicalZlib(
		TConstArrayView<uint8> RawBytes,
		TArray<uint8>& OutStoredBytes) override;
	virtual bool TryCompressCanonicalZlibInto(
		TConstArrayView<uint8> RawBytes,
		TArrayView<uint8> StoredOutput,
		uint64& OutProducedBytes) override;
	virtual bool TryDecompressCanonicalZlib(
		TConstArrayView<uint8> StoredBytes,
		TArrayView<uint8> RawOutput,
		uint64& OutProducedBytes) override;
};

class ANGELSCRIPTRUNTIME_API IAngelscriptCachePackSource
{
public:
	virtual ~IAngelscriptCachePackSource() = default;
	virtual bool TryGetCompletePack(
		const FAngelscriptHash256& PackId,
		TConstArrayView<uint8>& OutBytes) = 0;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheManifestPackArchive
{
	static constexpr uint32 PackSchemaVersion = 1;
	static constexpr uint32 ManifestSchemaVersion = 1;
	static constexpr uint32 RecordIdWireSize = 33;
	static constexpr uint32 ManifestRootWireSize = 65;
	static constexpr uint32 ManifestLocationWireSize = 122;
	static constexpr uint32 PackHeaderWireSize = 32;
	static constexpr uint32 PackIndexEntryWireSize = 96;
};

ANGELSCRIPTRUNTIME_API FAngelscriptCacheValidationResult BuildAngelscriptCachePacks(
	TConstArrayView<FAngelscriptPreparedRecord> NewRecords,
	const FAngelscriptCachePackPolicy& Policy,
	// In BoundedParallel mode compression calls may occur concurrently; supplied
	// codecs must therefore be stateless or internally thread-safe. The production
	// Unreal Zlib codec has no mutable instance state.
	IAngelscriptCacheStorageCodec& Codec,
	TArray<FAngelscriptEncodedPack>& OutPacks);

ANGELSCRIPTRUNTIME_API FAngelscriptCacheValidationResult
AggregateAngelscriptCachePreparedRecordCompletions(
	TConstArrayView<FAngelscriptPreparedRecordCompletion> Completions,
	EAngelscriptCachePreparationExecutionMode ExecutionMode,
	const FAngelscriptCachePackPolicy& Policy,
	IAngelscriptCacheStorageCodec& Codec,
	TArray<FAngelscriptEncodedPack>& OutPacks);

ANGELSCRIPTRUNTIME_API FAngelscriptCacheValidationResult ValidateAngelscriptCachePack(
	TConstArrayView<uint8> CompletePackBytes,
	const FAngelscriptHash256& ExpectedPackId,
	const FAngelscriptCacheReadLimits& Limits,
	FAngelscriptCacheReadBudget& Budget,
	TArray<FAngelscriptCachePackIndexEntry>& OutIndex);

ANGELSCRIPTRUNTIME_API FAngelscriptCacheValidationResult
ReadAngelscriptCacheRecordFromPack(
	TConstArrayView<uint8> CompletePackBytes,
	const FAngelscriptHash256& ExpectedPackId,
	const FAngelscriptCacheRecordIndexEntry& ManifestEntry,
	const FAngelscriptCacheReadLimits& Limits,
	FAngelscriptCacheReadBudget& Budget,
	IAngelscriptCacheStorageCodec& Codec,
	TOptional<FAngelscriptDecodedCacheRecordHandle>& OutRecord);

ANGELSCRIPTRUNTIME_API FAngelscriptCacheValidationResult
EncodeAngelscriptCacheGenerationManifest(
	const FAngelscriptCacheGenerationManifest& Value,
	FAngelscriptEncodedCacheGenerationManifest& OutManifest);

// Validates an already materialized writer-side Manifest value against the
// same canonical and bounded rules used by encode/decode, without opening Packs
// or mutating a Store. This is the publication preflight authority for caller-
// supplied limits such as MaxGenerationPacks.
ANGELSCRIPTRUNTIME_API FAngelscriptCacheValidationResult
ValidateAngelscriptCacheGenerationManifestValue(
	const FAngelscriptCacheGenerationManifest& Value,
	const FAngelscriptCacheReadLimits& Limits);

ANGELSCRIPTRUNTIME_API FAngelscriptCacheValidationResult
ValidateAngelscriptCacheGeneration(
	TConstArrayView<uint8> CompleteManifestBytes,
	const FAngelscriptHash256& ExpectedGenerationId,
	IAngelscriptCachePackSource& Packs,
	const FAngelscriptCacheReadLimits& Limits,
	FAngelscriptCacheReadBudget& Budget,
	IAngelscriptCacheStorageCodec& Codec,
	TOptional<FAngelscriptValidatedGeneration>& OutGeneration);

#if WITH_ANGELSCRIPT_UNITTESTS
struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheReachabilityRootForTests
{
	FAngelscriptCacheModuleSnapshotLink Link;
	uint64 ManifestByteOffset = 0;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheReachabilityManifestEntryForTests
{
	FAngelscriptCacheRecordId RecordId;
	uint64 ManifestByteOffset = 0;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheReachabilityNodeForTests
{
	FAngelscriptCacheRecordId RecordId;
	TOptional<FAngelscriptHash256> EmbeddedSourceSnapshot;
	TOptional<FAngelscriptStableModuleKey> EmbeddedModuleKey;
	FAngelscriptCacheRecordId ModuleInterface;
	TArray<FAngelscriptCacheRecordId> TypeSchemas;
	FAngelscriptCacheRecordId ModuleState;
	TArray<FAngelscriptCacheRecordId> FunctionBodies;
	FAngelscriptCacheRecordId DebugSidecar;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheModuleGraphValidationProbeForTests
{
	TArrayView<FAngelscriptStableModuleKey> ModuleKeys;
	uint64 CallCount = 0;
	bool bOverflow = false;
};

ANGELSCRIPTRUNTIME_API FAngelscriptCacheValidationResult
ValidateAngelscriptCacheGenerationReachabilityForTests(
	const FAngelscriptHash256& ManifestSourceSnapshot,
	const FAngelscriptCacheRecordId& SourceIndexRecordId,
	uint64 SourceIndexManifestByteOffset,
	TConstArrayView<FAngelscriptCacheReachabilityRootForTests> Roots,
	TConstArrayView<FAngelscriptCacheReachabilityManifestEntryForTests> ManifestIndex,
	TConstArrayView<FAngelscriptCacheReachabilityNodeForTests> Nodes,
	const FAngelscriptCacheReadLimits& Limits,
	FAngelscriptCacheReadBudget& Budget,
	TArray<FAngelscriptCacheRecordId>& OutVisited,
	FAngelscriptCacheModuleGraphValidationProbeForTests* Probe);
#endif
