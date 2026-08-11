#pragma once

#include "Cache/AngelscriptCacheManifestPack.h"

enum class EAngelscriptCacheSemanticChangeDisposition : uint8
{
	Unchanged = 0,
	Added = 1,
	Removed = 2,
	Modified = 3,
};

enum class EAngelscriptCacheSemanticDiffError : uint8
{
	None = 0,
	IncompatibleGeneration = 1,
	InvalidValidatedGeneration = 2,
	MissingRecord = 3,
	WrongRecordKind = 4,
	DuplicateSemanticOwner = 5,
	EmbeddedOwnerMismatch = 6,
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheSemanticRecordChange
{
	EAngelscriptCacheSemanticChangeDisposition Disposition =
		EAngelscriptCacheSemanticChangeDisposition::Unchanged;
	TOptional<FAngelscriptCacheRecordId> PreviousRecordId;
	TOptional<FAngelscriptCacheRecordId> CurrentRecordId;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheSemanticTypeChange
{
	FAngelscriptStableTypeKey TypeKey;
	FAngelscriptCacheSemanticRecordChange Record;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheSemanticFunctionChange
{
	FAngelscriptStableFunctionKey FunctionKey;
	FAngelscriptCacheSemanticRecordChange Record;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheSemanticModuleDiff
{
	FAngelscriptStableModuleKey ModuleKey;
	FAngelscriptCacheSemanticRecordChange ModuleSnapshot;
	FAngelscriptCacheSemanticRecordChange ModuleInterface;
	FAngelscriptCacheSemanticRecordChange ModuleState;
	TArray<FAngelscriptCacheSemanticTypeChange> TypeSchemas;
	TArray<FAngelscriptCacheSemanticFunctionChange> FunctionBodies;
	TArray<FAngelscriptCacheSemanticFunctionChange> DebugSidecars;

	bool HasSemanticChanges() const;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptCacheSemanticDiffResult
{
	EAngelscriptCacheSemanticDiffError Error =
		EAngelscriptCacheSemanticDiffError::None;
	FString Detail;
	FAngelscriptCacheSemanticRecordChange SourceIndex;
	TArray<FAngelscriptCacheSemanticModuleDiff> Modules;
	TArray<FAngelscriptCacheRecordId> ReusedRecordIds;
	TArray<FAngelscriptCacheRecordId> NewRecordIds;
	TArray<FAngelscriptCacheRecordId> RetiredRecordIds;

	bool IsSuccess() const
	{
		return Error == EAngelscriptCacheSemanticDiffError::None;
	}

	bool HasSemanticChanges() const;
};

ANGELSCRIPTRUNTIME_API FAngelscriptCacheSemanticDiffResult
DiffAngelscriptCacheValidatedGenerations(
	const FAngelscriptValidatedGeneration& Previous,
	const FAngelscriptValidatedGeneration& Current);
