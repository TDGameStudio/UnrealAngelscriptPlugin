#pragma once

#include "CoreMinimal.h"

#include "Dump/AngelscriptStateSnapshot.h"

enum class EAngelscriptStateDiffChangeType : uint8
{
	Added,
	Removed,
	Changed,
	Unchanged,
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptStateDiffRow
{
	EAngelscriptStateDiffChangeType ChangeType = EAngelscriptStateDiffChangeType::Unchanged;
	FString Category;
	FString Identity;
	FString Field;
	FString BeforeValue;
	FString AfterValue;
	FString Source;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptStateDiff
{
	TArray<FAngelscriptStateDiffRow> Rows;

	int32 CountByChangeType(EAngelscriptStateDiffChangeType ChangeType) const;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptStateDiffBuilder
{
	static FAngelscriptStateDiff Diff(const FAngelscriptStateSnapshot& Before, const FAngelscriptStateSnapshot& After);
	static FString ChangeTypeToString(EAngelscriptStateDiffChangeType ChangeType);
};
