#pragma once

#include "Cache/AngelscriptCacheDecodedRecord.h"
#include "as_buildartifact.h"

enum class EAngelscriptCacheFunctionInputStatus : uint8
{
	Invalid = 0,
	SourceChanged = 1,
	DependencyMissing = 2,
	ResolvedMismatch = 3,
	ResolvedMatch = 4,
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheFunctionInputAuthorities
{
	const FAngelscriptCachedModuleInterface* ModuleInterface = nullptr;
	TConstArrayView<FAngelscriptCachedTypeSchema> TypeSchemas;
	const FAngelscriptCachedModuleState* ModuleState = nullptr;
	TConstArrayView<FAngelscriptCachedFunctionBody> FunctionBodies;
	const IAngelscriptCacheCurrentSymbolResolver* ExternalSymbols = nullptr;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheFunctionInputResolution
{
	EAngelscriptCacheFunctionInputStatus Status =
		EAngelscriptCacheFunctionInputStatus::Invalid;
	FAngelscriptFunctionInputDigest CurrentInputDigest;
	TOptional<uint32> MissingDependencyOrdinal;
};

enum class EAngelscriptCacheFunctionCandidateLookupStatus : uint8
{
	Invalid = 0,
	Restored = 1,
	FunctionKeyMiss = 2,
	ProfileMismatch = 3,
	SourceChanged = 4,
	DependencyMissing = 5,
	InputChanged = 6,
	RejectedCorrupt = 7,
	NotCacheable = 8,
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheFunctionCandidateLookupResult
{
	EAngelscriptCacheFunctionCandidateLookupStatus Status =
		EAngelscriptCacheFunctionCandidateLookupStatus::Invalid;
	asEBuildArtifactRestoreResult RestoreResult =
		asBUILD_ARTIFACT_RESTORE_MISS;
	FAngelscriptFunctionSourceDigest CurrentSourceDigest;
	FAngelscriptFunctionInputDigest CurrentInputDigest;
	TOptional<uint32> MissingDependencyOrdinal;
	// Populated only after the selected graph body has matched current source and
	// input authority and its complete VM artifact was committed successfully.
	// These are stable pointer-free coordinates, never raw compiler observations.
	TArray<FAngelscriptCacheSemanticDependency> RestoredActualDependencies;
	FString Detail;
};

// Read-only compile-transaction seam used by post-compile Clean Capture. A
// provider may return dependencies only for functions that were actually
// restored after graph validation and exact current-input matching.
class ANGELSCRIPTRUNTIME_API
	IAngelscriptCacheRestoredFunctionDependencySource
{
public:
	virtual ~IAngelscriptCacheRestoredFunctionDependencySource() = default;

	virtual bool TryCopyActualDependencies(
		const FAngelscriptStableModuleKey& ModuleKey,
		const FAngelscriptStableFunctionKey& FunctionKey,
		TArray<FAngelscriptCacheSemanticDependency>& OutDependencies) const = 0;
};

class ANGELSCRIPTRUNTIME_API IAngelscriptCacheBuildDependencyResolver
{
public:
	virtual ~IAngelscriptCacheBuildDependencyResolver() = default;
	virtual bool Resolve(
		const asSBuildArtifactDependency& RawDependency,
		FAngelscriptCacheSemanticDependency& OutDependency) const = 0;
};

class ANGELSCRIPTRUNTIME_API FAngelscriptCacheCompilerBridge final
{
public:
	static bool TryBuildFunctionSourceDigest(
		const asSBuildArtifactInvocation& Invocation,
		const TArray<FString>& CanonicalCompileOptions,
		FAngelscriptFunctionSourceDigest& OutDigest,
		FString& OutFailure);

	static FAngelscriptCacheValidationResult CanonicalizeActualDependencies(
		TConstArrayView<FAngelscriptCacheSemanticDependency> ObservedDependencies,
		TArray<FAngelscriptCacheSemanticDependency>& OutCanonicalDependencies);

	static FAngelscriptCacheValidationResult CaptureSuccessfulActualDependencies(
		const asCScriptFunction& Function,
		const IAngelscriptCacheBuildDependencyResolver& Resolver,
		TArray<FAngelscriptCacheSemanticDependency>& OutCanonicalDependencies);

	static FAngelscriptCacheFunctionInputResolution ResolveCurrentFunctionInput(
		const FAngelscriptCachedFunctionBody& CachedBody,
		const FAngelscriptFunctionSourceDigest& CurrentSourceDigest,
		const FAngelscriptCacheFunctionInputAuthorities& Authorities);

	// Selects only from a graph-validated immutable candidate set. The caller
	// supplies the current semantic function key and current declaration/type/
	// state authorities produced before function compilation. A source or input
	// mismatch is a typed Miss and never reaches VM artifact attachment.
	static FAngelscriptCacheFunctionCandidateLookupResult
	TryRestoreFunctionFromValidatedGraph(
		const FAngelscriptValidatedModuleGraph& CandidateGraph,
		const asSBuildArtifactInvocation& Invocation,
		asCScriptFunction& TargetFunction,
		const FAngelscriptStableFunctionKey& CurrentFunctionKey,
		const FAngelscriptArtifactProfileKey& CurrentProfile,
		const TArray<FString>& CanonicalCompileOptions,
		const FAngelscriptCacheFunctionInputAuthorities& CurrentAuthorities,
		const FAngelscriptCacheReadLimits& Limits,
		FAngelscriptCacheReadBudget& Budget);

	// Candidate selection and current-input matching happen before this call.
	// This boundary rechecks immutable execution/debug coordinates and delegates
	// complete private VM reconstruction/commit to the maintained-fork codec.
	static asEBuildArtifactRestoreResult TryRestoreFunctionArtifact(
		const asSBuildArtifactInvocation& Invocation,
		asCScriptFunction& TargetFunction,
		const FAngelscriptCachedFunctionBody& CachedBody,
		const FAngelscriptCachedDebugSidecar& CachedDebug,
		const FAngelscriptCacheReadLimits& Limits,
		FAngelscriptCacheReadBudget& Budget,
		FString& OutDetail);
};
