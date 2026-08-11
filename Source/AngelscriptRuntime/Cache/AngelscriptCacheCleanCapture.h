#pragma once

#include "Cache/AngelscriptCacheManifestPack.h"

struct FAngelscriptModuleDesc;
class IAngelscriptCacheRestoredFunctionDependencySource;

enum class EAngelscriptCacheCleanCaptureError : uint8
{
	None = 0,
	InvalidInput = 1,
	NotCacheable = 2,
	SourceReadFailed = 3,
	FunctionArtifactFailed = 4,
	RecordEncodingFailed = 5,
	PackBuildFailed = 6,
	ManifestBuildFailed = 7,
	GraphValidationFailed = 8,
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheCleanCaptureResult
{
	EAngelscriptCacheCleanCaptureError Error =
		EAngelscriptCacheCleanCaptureError::None;
	uint32 ValidatedGraphRecordCount = 0;
	uint32 GraphCarriedDependencyFunctionCount = 0;
	FString Detail;

	bool IsSuccess() const
	{
		return Error == EAngelscriptCacheCleanCaptureError::None;
	}
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheCleanCaptureOptions
{
	FAngelscriptCacheCompatibilityKey Compatibility;
	FAngelscriptCacheContextKey Context;
	FAngelscriptArtifactProfileKey Profile;
	TArray<FString> CanonicalCompileOptions;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheCleanModuleArtifacts
{
	FAngelscriptStableModuleKey ModuleKey;
	FString CanonicalModuleName;
	FAngelscriptHash256 SourceSnapshot;
	FAngelscriptCacheRecordId SourceIndexRecordId;
	FAngelscriptCacheModuleSnapshotLink ModuleSnapshot;
	TArray<FAngelscriptPreparedRecord> Records;
	// Derived only from the sole validated module graph. This pointer-free
	// handoff feeds current-Engine execution routing and diagnostics; it is not
	// another persisted identity or record authority.
	TArray<FAngelscriptFunctionArtifactIdentity>
		ValidatedFunctionArtifactIdentities;

	void Reset()
	{
		ModuleKey = {};
		CanonicalModuleName.Reset();
		SourceSnapshot = {};
		SourceIndexRecordId = {};
		ModuleSnapshot = {};
		Records.Reset();
		ValidatedFunctionArtifactIdentities.Reset();
	}
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCachePreparedColdGeneration
{
	TArray<FAngelscriptEncodedPack> Packs;
	FAngelscriptCacheGenerationManifest Manifest;
	FAngelscriptEncodedCacheGenerationManifest EncodedManifest;

	void Reset()
	{
		Packs.Reset();
		Manifest = {};
		EncodedManifest = {};
	}
};

ANGELSCRIPTRUNTIME_API FAngelscriptCacheCleanCaptureResult
CaptureAngelscriptCleanCompiledModule(
	const TSharedRef<FAngelscriptModuleDesc>& Module,
	const FAngelscriptCacheCleanCaptureOptions& Options,
	FAngelscriptCacheCleanModuleArtifacts& OutArtifacts);

// Production capture overload. The SourceIndex must be the authoritative
// candidate built from current source discovery plus captured preprocessing
// dependencies; capture validates that the compiled module is one of its exact
// source owners instead of synthesizing a parallel discovery identity.
ANGELSCRIPTRUNTIME_API FAngelscriptCacheCleanCaptureResult
CaptureAngelscriptCleanCompiledModule(
	const TSharedRef<FAngelscriptModuleDesc>& Module,
	const FAngelscriptCacheCleanCaptureOptions& Options,
	const FAngelscriptCachedSourceIndex& AuthoritativeSourceIndex,
	FAngelscriptCacheCleanModuleArtifacts& OutArtifacts);

// Production hybrid-capture overload. RestoredDependencies is scoped to the
// current compile transaction and can supply pointer-free dependencies only for
// functions that were graph-validated and restored in that transaction.
ANGELSCRIPTRUNTIME_API FAngelscriptCacheCleanCaptureResult
CaptureAngelscriptCleanCompiledModule(
	const TSharedRef<FAngelscriptModuleDesc>& Module,
	const FAngelscriptCacheCleanCaptureOptions& Options,
	const FAngelscriptCachedSourceIndex& AuthoritativeSourceIndex,
	const IAngelscriptCacheRestoredFunctionDependencySource*
		RestoredDependencies,
	FAngelscriptCacheCleanModuleArtifacts& OutArtifacts);

ANGELSCRIPTRUNTIME_API FAngelscriptCacheCleanCaptureResult
ValidateAndPromoteAngelscriptCleanCompiledModuleArtifacts(
	const TSharedRef<FAngelscriptModuleDesc>& Module,
	const FAngelscriptCacheCleanCaptureOptions& Options,
	FAngelscriptCacheCleanModuleArtifacts CandidateArtifacts,
	FAngelscriptCacheCleanModuleArtifacts& OutArtifacts);

// Reopens pointer-free clean artifacts through the same decoder, current-
// symbol/layout authorities and opaque VM validator used by capture. The
// caller-owned Budget must outlive OutGraph because decoded graph handles retain
// charged ownership. This is also the production-shaped test/debug seam used
// before the persisted Store service owns graph opening in V6.
ANGELSCRIPTRUNTIME_API FAngelscriptCacheCleanCaptureResult
OpenAngelscriptValidatedModuleGraphFromCleanArtifacts(
	const TSharedRef<FAngelscriptModuleDesc>& Module,
	const FAngelscriptCacheCleanCaptureOptions& Options,
	const FAngelscriptCacheCleanModuleArtifacts& Artifacts,
	const FAngelscriptCacheReadLimits& Limits,
	FAngelscriptCacheReadBudget& Budget,
	FAngelscriptValidatedModuleGraph& OutGraph);

ANGELSCRIPTRUNTIME_API FAngelscriptCacheCleanCaptureResult
PrepareAngelscriptCacheColdGeneration(
	const FAngelscriptCacheCleanModuleArtifacts& Artifacts,
	const FAngelscriptCacheCleanCaptureOptions& Options,
	const FAngelscriptCachePackPolicy& PackPolicy,
	IAngelscriptCacheStorageCodec& Codec,
	FAngelscriptCachePreparedColdGeneration& OutGeneration);

// Prepares one complete generation from the authoritative module set captured
// by a successful compile transaction. Modules must share the same source
// snapshot and SourceIndex. Identical content-addressed records (normally the
// shared SourceIndex) are stored once; conflicting payloads for one RecordId are
// rejected before any output is promoted.
ANGELSCRIPTRUNTIME_API FAngelscriptCacheCleanCaptureResult
PrepareAngelscriptCacheColdGeneration(
	TConstArrayView<FAngelscriptCacheCleanModuleArtifacts> Modules,
	const FAngelscriptCacheCleanCaptureOptions& Options,
	const FAngelscriptCachePackPolicy& PackPolicy,
	IAngelscriptCacheStorageCodec& Codec,
	FAngelscriptCachePreparedColdGeneration& OutGeneration);
