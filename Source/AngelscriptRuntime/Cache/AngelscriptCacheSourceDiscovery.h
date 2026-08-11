#pragma once

#include "Cache/AngelscriptCacheSourcePlanner.h"
#include "Core/AngelscriptSourceProvider.h"

enum class EAngelscriptCacheSourceDiscoveryError : uint8
{
	None = 0,
	InvalidRequest = 1,
	SourceReadFailed = 2,
	InvalidSourceEncoding = 3,
	InvalidSourceDescriptor = 4,
	DirectPlanRejected = 5,
	EligibilityPlanningFailed = 6,
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheSourceDiscoveryStatus
{
	EAngelscriptCacheSourceDiscoveryError Error =
		EAngelscriptCacheSourceDiscoveryError::None;
	FAngelscriptCacheValidationResult Validation;
	FString Detail;

	bool IsSuccess() const
	{
		return Error == EAngelscriptCacheSourceDiscoveryError::None
			&& Validation.IsSuccess();
	}
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheProductionSourceDiscoveryConfig
{
	FAngelscriptArtifactProfileKey Profile;
	uint32 DiscoveryPolicyVersion = 0;
	TArray<FAngelscriptCacheDirectOptionInput> Options;
	TArray<FAngelscriptCachedPreprocessHook> PreprocessHooks;
	bool bObserveLegacyGlobalPreprocessHooks = true;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheModuleSourcePlan
{
	FAngelscriptStableModuleKey ModuleKey;
	FString CanonicalModuleName;
	int32 SourceFileCount = 0;
	bool bExactFastPathEligible = false;
	TArray<FAngelscriptCachedFastPathIneligibleScope> MatchingIneligibleScopes;
};

/**
 * Current-attempt source material retained only long enough to project a
 * validated Cache V2 module into this machine's FAngelscriptModuleDesc.
 * Absolute paths and raw bytes are deliberately not persisted in SourceIndex.
 */
struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheCurrentSourceProjection
{
	FAngelscriptCachedSourceFileKey SourceFileKey;
	FAngelscriptStableModuleKey ModuleKey;
	FString VirtualPath;
	FString RelativeFilename;
	FString AbsoluteFilename;
	TArray<uint8> RawSourceBytes;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheProductionSourceDiscoveryResult
{
	FAngelscriptCacheDirectSourcePlan DirectPlan;
	TArray<FAngelscriptCacheModuleSourcePlan> Modules;
	TArray<FAngelscriptCacheCurrentSourceProjection> CurrentSources;
	int32 DiscoveredSourceCount = 0;
	int32 LoadedSourceCount = 0;

	void Reset()
	{
		DirectPlan.Reset();
		Modules.Reset();
		CurrentSources.Reset();
		DiscoveredSourceCount = 0;
		LoadedSourceCount = 0;
	}
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheDependencyObservationPlan
{
	TArray<FAngelscriptCacheObservedDependencyInput> Observations;
	TArray<FAngelscriptCachedPreprocessorInputKey> UnavailableInputKeys;

	void Reset()
	{
		Observations.Reset();
		UnavailableInputKeys.Reset();
	}
};

class ANGELSCRIPTRUNTIME_API FAngelscriptCacheSourceDiscovery
{
public:
	/**
	 * Discover the same typed source inventory consumed by normal compile, load
	 * its current byte authority and build an absolute-path-free direct plan.
	 */
	static FAngelscriptCacheSourceDiscoveryStatus DiscoverProductionSources(
		IAngelscriptSourceProvider& SourceProvider,
		TConstArrayView<FAngelscriptSourceRoot> ScriptRoots,
		bool bSkipDevelopmentScripts,
		bool bSkipEditorScripts,
		const FAngelscriptCacheProductionSourceDiscoveryConfig& Config,
		const FAngelscriptCacheDirectSourceLimits& Limits,
		FAngelscriptCacheProductionSourceDiscoveryResult& OutResult);

	/**
	 * Resolve the current observations for one persisted dependency candidate.
	 * Missing current authorities are reported in UnavailableInputKeys and remain
	 * an ordinary V3.2 cache miss rather than a malformed-data failure.
	 */
	static FAngelscriptCacheValidationResult BuildCurrentDependencyObservations(
		const FAngelscriptCacheDirectSourcePlan& CurrentDirectPlan,
		const FAngelscriptCachedSourceIndex& PersistedCandidate,
		FAngelscriptCacheDependencyObservationPlan& OutPlan);
};
