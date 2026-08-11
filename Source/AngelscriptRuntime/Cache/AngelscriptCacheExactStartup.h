#pragma once

#include "Cache/AngelscriptCacheRestore.h"

enum class EAngelscriptCacheExactStartupDisposition : uint8
{
	Invalid = 0,
	Restored = 1,
	Miss = 2,
	Rejected = 3,
};

enum class EAngelscriptCacheExactStartupReason : uint8
{
	None = 0,
	InvalidInput = 1,
	ProfileMismatch = 2,
	MissingSourceIndex = 3,
	DirectInputMismatch = 4,
	ObservationUnavailable = 5,
	DependencyMismatch = 6,
	SourceCandidateRejected = 7,
	SourceSnapshotMismatch = 8,
	ModuleSetMismatch = 9,
	ModuleIneligible = 10,
	CurrentSourceProjectionMismatch = 11,
	RestoreRejected = 12,
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheExactStartupCounters
{
	uint32 PreprocessCalls = 0;
	uint32 ParseCalls = 0;
	uint32 ModuleCompilerCalls = 0;
	uint32 FunctionCompilerCalls = 0;
	uint32 PublicationAttempts = 0;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheExactStartupResult
{
	EAngelscriptCacheExactStartupDisposition Disposition =
		EAngelscriptCacheExactStartupDisposition::Invalid;
	EAngelscriptCacheExactStartupReason Reason =
		EAngelscriptCacheExactStartupReason::InvalidInput;
	EAngelscriptCacheDependencyCandidateMatch CandidateMatch =
		EAngelscriptCacheDependencyCandidateMatch::Invalid;
	TOptional<FAngelscriptCacheValidationResult> Validation;
	FAngelscriptCacheExactStartupCounters Counters;
	uint32 SelectedModuleCount = 0;
	uint32 RestoredModuleCount = 0;
	uint32 RestoredTypeCount = 0;
	uint32 RestoredFunctionCount = 0;
	FString Detail;

	bool IsRestored() const
	{
		return Disposition
			== EAngelscriptCacheExactStartupDisposition::Restored;
	}
};

/**
 * Validate the current direct/dependency source authority and, only on Exact,
 * activate the supported generation in a normally initialized target Engine.
 * This coordinator never invokes the frontend/compiler and never writes the
 * Store. Its caller owns the authoritative miss fallback and later publication.
 */
ANGELSCRIPTRUNTIME_API FAngelscriptCacheExactStartupResult
RestoreAngelscriptCacheExactStartup(
	FAngelscriptEngine& TargetEngine,
	const FAngelscriptValidatedGeneration& Generation,
	const FAngelscriptArtifactProfileKey& CurrentProfile,
	const FAngelscriptCacheProductionSourceDiscoveryResult& CurrentDiscovery,
	const FAngelscriptCacheDependencyCandidateLimits& DependencyLimits,
	const FAngelscriptCacheReadLimits& ReadLimits,
	const FAngelscriptHash256* PersistedGenerationId = nullptr,
	IAngelscriptCacheRestoreFaultInjector* FaultInjector = nullptr);
