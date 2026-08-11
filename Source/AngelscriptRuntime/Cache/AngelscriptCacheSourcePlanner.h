#pragma once

#include "Cache/AngelscriptCacheSemanticRecords.h"

enum class EAngelscriptCacheDirectOptionKind : uint8
{
	Invalid = 0,
	Preprocessor = 1,
	Compiler = 2,
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheDirectOptionInput
{
	EAngelscriptCacheDirectOptionKind Kind =
		EAngelscriptCacheDirectOptionKind::Invalid;
	FString CanonicalKey;
	FString CanonicalValue;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheDirectSourceFileInput
{
	int32 MountIndex = INDEX_NONE;
	FString RelativeLogicalPath;
	TArray<uint8> RawSourceBytes;
	TOptional<FAngelscriptHash256> GeneratedSourceKey;
	TOptional<FAngelscriptHash256> GeneratedConfigurationFingerprint;
	FAngelscriptStableModuleKey ModuleKey;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheDirectSourceInputs
{
	FAngelscriptArtifactProfileKey Profile;
	uint32 DiscoveryPolicyVersion = 0;
	uint32 DiscoveryFilterFlags = 0;
	TArray<FAngelscriptCacheDirectOptionInput> Options;
	TArray<FAngelscriptCachedSourceProvider> Providers;
	TArray<FAngelscriptCachedSourceMount> Mounts;
	TArray<FAngelscriptCachedPreprocessHook> PreprocessHooks;
	TArray<FAngelscriptCacheDirectSourceFileInput> Files;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheDirectSourceLimits
{
	static constexpr uint64 DefaultMaxOptions = UINT64_C(65536);
	static constexpr uint64 DefaultMaxProviders = UINT64_C(4096);
	static constexpr uint64 DefaultMaxMounts = UINT64_C(4096);
	static constexpr uint64 DefaultMaxPreprocessHooks = UINT64_C(65536);
	static constexpr uint64 DefaultMaxFiles = UINT64_C(1048576);
	static constexpr uint64 DefaultMaxSingleRawSourceBytes =
		UINT64_C(64) * 1024 * 1024;
	static constexpr uint64 DefaultMaxTotalRawSourceBytes =
		UINT64_C(512) * 1024 * 1024;
	static constexpr uint64 DefaultMaxCanonicalStringCharacters = UINT64_C(1048576);

	uint64 MaxOptions = DefaultMaxOptions;
	uint64 MaxProviders = DefaultMaxProviders;
	uint64 MaxMounts = DefaultMaxMounts;
	uint64 MaxPreprocessHooks = DefaultMaxPreprocessHooks;
	uint64 MaxFiles = DefaultMaxFiles;
	uint64 MaxSingleRawSourceBytes = DefaultMaxSingleRawSourceBytes;
	uint64 MaxTotalRawSourceBytes = DefaultMaxTotalRawSourceBytes;
	uint64 MaxCanonicalStringCharacters = DefaultMaxCanonicalStringCharacters;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheDirectSourcePlan
{
	FAngelscriptHash256 DirectInputDigest;
	// This is a canonical direct-only projection. PreprocessorInputs and Edges are
	// intentionally empty until the second-stage dependency capture appends them.
	FAngelscriptCachedSourceIndex DirectProjection;

	void Reset()
	{
		DirectInputDigest = {};
		DirectProjection = {};
	}
};

enum class EAngelscriptCacheDependencyCandidateMatch : uint8
{
	Invalid = 0,
	Exact = 1,
	DirectInputMismatch = 2,
	ObservationUnavailable = 3,
	DependencyMismatch = 4,
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheObservedDependencyInput
{
	FAngelscriptCachedPreprocessorInputKey InputKey;
	FAngelscriptHash256 EffectiveValueOrContentHash;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheDependencyCandidateLimits
{
	static constexpr uint64 DefaultMaxPreprocessorInputs = UINT64_C(1048576);
	static constexpr uint64 DefaultMaxEdges = UINT64_C(1048576);
	static constexpr uint64 DefaultMaxObservations = UINT64_C(1048576);
	static constexpr uint64 DefaultMaxCanonicalStringCharacters = UINT64_C(1048576);

	uint64 MaxPreprocessorInputs = DefaultMaxPreprocessorInputs;
	uint64 MaxEdges = DefaultMaxEdges;
	uint64 MaxObservations = DefaultMaxObservations;
	uint64 MaxCanonicalStringCharacters = DefaultMaxCanonicalStringCharacters;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheDependencyCandidateResult
{
	EAngelscriptCacheDependencyCandidateMatch Match =
		EAngelscriptCacheDependencyCandidateMatch::Invalid;
	int32 ComparedDependencyCount = 0;
	FAngelscriptCachedPreprocessorInputKey FirstNonMatchingInputKey;
	// Populated only for Exact. Every miss and validation failure keeps this empty.
	FAngelscriptCachedSourceIndex ExactSourceIndex;

	void Reset()
	{
		Match = EAngelscriptCacheDependencyCandidateMatch::Invalid;
		ComparedDependencyCount = 0;
		FirstNonMatchingInputKey = {};
		ExactSourceIndex = {};
	}
};

class ANGELSCRIPTRUNTIME_API FAngelscriptCacheSourcePlanner
{
public:
	static FAngelscriptCacheValidationResult BuildDirectSourcePlan(
		const FAngelscriptCacheDirectSourceInputs& Inputs,
		const FAngelscriptCacheDirectSourceLimits& Limits,
		FAngelscriptCacheDirectSourcePlan& OutPlan);

	static FAngelscriptCacheValidationResult ComputePersistedDirectInputDigest(
		const FAngelscriptCachedSourceIndex& PersistedSourceIndex,
		const FAngelscriptArtifactProfileKey& Profile,
		FAngelscriptHash256& OutDigest);

	static FAngelscriptCacheValidationResult BuildPersistedDependencyCandidate(
		const FAngelscriptCacheDirectSourcePlan& DirectPlan,
		const FAngelscriptArtifactProfileKey& Profile,
		TConstArrayView<FAngelscriptCachedPreprocessorInput> CapturedInputs,
		TConstArrayView<FAngelscriptCachedSourceEdge> CapturedEdges,
		const FAngelscriptCacheDependencyCandidateLimits& Limits,
		FAngelscriptCachedSourceIndex& OutCandidate);

	static FAngelscriptCacheValidationResult ValidatePersistedDependencyCandidate(
		const FAngelscriptCacheDirectSourcePlan& CurrentDirectPlan,
		const FAngelscriptArtifactProfileKey& Profile,
		const FAngelscriptCachedSourceIndex& PersistedCandidate,
		TConstArrayView<FAngelscriptCacheObservedDependencyInput> Observations,
		const FAngelscriptCacheDependencyCandidateLimits& Limits,
		FAngelscriptCacheDependencyCandidateResult& OutResult);
};
