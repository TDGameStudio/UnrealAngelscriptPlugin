#pragma once

#include "CoreMinimal.h"

struct FAngelscriptEngine;

struct ANGELSCRIPTRUNTIME_API FAngelscriptStateSnapshotRow
{
	FString Category;
	FString Identity;
	FString Field;
	FString Value;
	FString ValueKind;
	FString Source;
	FString SortKey;

	FString MakeKey() const;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptStateSnapshot
{
	TArray<FAngelscriptStateSnapshotRow> Rows;
	TSet<FString> RowKeys;

	void AddRow(
		const FString& Category,
		const FString& Identity,
		const FString& Field,
		const FString& Value,
		const FString& ValueKind,
		const FString& Source);
	void SortRows();
	bool AreRowsSorted() const;
	bool HasDuplicateKeys() const;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptStateSnapshotBuilder
{
	static FAngelscriptStateSnapshot Capture(FAngelscriptEngine& Engine);
};
