#pragma once

#include "Cache/AngelscriptCacheSemanticDiff.h"

enum class EAngelscriptCacheDependencyPropagationError : uint8
{
	None = 0,
	InvalidManifest = 1,
	InvalidValidatedGeneration = 2,
	DuplicateSemanticAuthority = 3,
};

enum class EAngelscriptCacheDependencyMissReason : uint8
{
	TargetUnresolved = 1,
	AbiMismatch = 2,
	ContentUnavailable = 3,
	ContentMismatch = 4,
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheDependencyMiss
{
	EAngelscriptCacheDependencyMissReason Reason =
		EAngelscriptCacheDependencyMissReason::TargetUnresolved;
	EAngelscriptCacheRecordKind OwnerRecordKind =
		static_cast<EAngelscriptCacheRecordKind>(0);
	FAngelscriptHash256 OwnerStableKey;
	uint32 OwnerOrdinal = 0;
	uint32 DependencyOrdinal = 0;
	EAngelscriptCacheSemanticDependencyKind DependencyKind =
		EAngelscriptCacheSemanticDependencyKind::Invalid;
	EAngelscriptCacheReferenceKind TargetKind =
		EAngelscriptCacheReferenceKind::Invalid;
	FAngelscriptHash256 TargetStableKey;
	FAngelscriptHash256 ExpectedAbi;
	TOptional<FAngelscriptHash256> CurrentAbi;
	TOptional<FAngelscriptHash256> ExpectedContentOrValue;
	TOptional<FAngelscriptHash256> CurrentContentOrValue;

	friend bool operator==(
		const FAngelscriptCacheDependencyMiss& Left,
		const FAngelscriptCacheDependencyMiss& Right)
	{
		return Left.Reason == Right.Reason
			&& Left.OwnerRecordKind == Right.OwnerRecordKind
			&& Left.OwnerStableKey == Right.OwnerStableKey
			&& Left.OwnerOrdinal == Right.OwnerOrdinal
			&& Left.DependencyOrdinal == Right.DependencyOrdinal
			&& Left.DependencyKind == Right.DependencyKind
			&& Left.TargetKind == Right.TargetKind
			&& Left.TargetStableKey == Right.TargetStableKey
			&& Left.ExpectedAbi == Right.ExpectedAbi
			&& Left.CurrentAbi == Right.CurrentAbi
			&& Left.ExpectedContentOrValue == Right.ExpectedContentOrValue
			&& Left.CurrentContentOrValue == Right.CurrentContentOrValue;
	}

	friend bool operator!=(
		const FAngelscriptCacheDependencyMiss& Left,
		const FAngelscriptCacheDependencyMiss& Right)
	{
		return !(Left == Right);
	}
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheDependentModule
{
	FAngelscriptStableModuleKey ModuleKey;
	TArray<FAngelscriptCacheDependencyMiss> Reasons;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheDependentRecompileWave
{
	TArray<FAngelscriptCacheDependentModule> Modules;

	void Reset()
	{
		Modules.Empty();
	}

	bool IsEmpty() const
	{
		return Modules.IsEmpty();
	}
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheDependencyPropagationResult
{
	EAngelscriptCacheDependencyPropagationError Error =
		EAngelscriptCacheDependencyPropagationError::None;
	EAngelscriptCacheSemanticDiffError SemanticDiffError =
		EAngelscriptCacheSemanticDiffError::None;
	TOptional<FAngelscriptCacheValidationResult> Validation;
	FString Detail;

	bool IsSuccess() const
	{
		return Error == EAngelscriptCacheDependencyPropagationError::None;
	}
};

// Plans one conservative fixed-point wave. It never compiles, publishes or
// mutates an Engine. The caller overlays successful forced-clean module records
// and invokes this again until the returned wave is empty.
ANGELSCRIPTRUNTIME_API FAngelscriptCacheDependencyPropagationResult
PlanAngelscriptCacheDependentRecompileWave(
	const FAngelscriptValidatedGeneration& CandidateGeneration,
	const FAngelscriptCacheReadLimits& Limits,
	const IAngelscriptCacheCurrentSymbolResolver* ExternalCurrentSymbols,
	FAngelscriptCacheDependentRecompileWave& OutWave);
