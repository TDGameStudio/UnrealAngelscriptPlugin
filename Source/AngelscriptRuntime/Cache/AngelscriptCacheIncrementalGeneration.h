#pragma once

#include "Cache/AngelscriptCacheSemanticDiff.h"

enum class EAngelscriptCacheIncrementalPreparationError : uint8
{
	None = 0,
	InvalidBaseManifest = 1,
	InvalidCurrentManifest = 2,
	SemanticDiffFailed = 3,
	MissingBaseLocation = 4,
	MissingCurrentRecord = 5,
	UnexpectedCurrentRecord = 6,
	PackBuildFailed = 7,
	NewPackIndexMismatch = 8,
	ManifestBuildFailed = 9,
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheIncrementalPreparationResult
{
	EAngelscriptCacheIncrementalPreparationError Error =
		EAngelscriptCacheIncrementalPreparationError::None;
	EAngelscriptCacheSemanticDiffError SemanticDiffError =
		EAngelscriptCacheSemanticDiffError::None;
	TOptional<FAngelscriptCacheValidationResult> Validation;
	FString Detail;

	bool IsSuccess() const
	{
		return Error == EAngelscriptCacheIncrementalPreparationError::None;
	}
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCachePreparedIncrementalGeneration
{
	// These are exactly the immutable Packs absent from the base semantic record
	// set. Reused Pack locations remain only in Manifest and are never rewritten.
	TArray<FAngelscriptEncodedPack> NewPacks;
	FAngelscriptCacheGenerationManifest Manifest;
	FAngelscriptEncodedCacheGenerationManifest EncodedManifest;
	FAngelscriptCacheSemanticDiffResult SemanticDiff;

	void Reset()
	{
		NewPacks.Reset();
		Manifest = {};
		EncodedManifest = {};
		SemanticDiff = {};
	}
};

// Prepares one complete current Generation while physically retaining every
// unchanged record location from BaseGeneration. Both generations must already
// have passed complete Manifest/Pack/record-graph validation. The semantic diff
// is recomputed internally; caller-authored reuse lists are intentionally not an
// input. On every failure OutGeneration is atomically empty.
ANGELSCRIPTRUNTIME_API FAngelscriptCacheIncrementalPreparationResult
PrepareAngelscriptCacheIncrementalGeneration(
	const FAngelscriptValidatedGeneration& BaseGeneration,
	const FAngelscriptValidatedGeneration& CurrentGeneration,
	const FAngelscriptCachePackPolicy& PackPolicy,
	const FAngelscriptCacheReadLimits& Limits,
	IAngelscriptCacheStorageCodec& Codec,
	FAngelscriptCachePreparedIncrementalGeneration& OutGeneration);
