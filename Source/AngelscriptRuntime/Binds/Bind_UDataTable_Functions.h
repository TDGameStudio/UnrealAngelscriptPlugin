#pragma once

#include "CoreMinimal.h"

class UDataTable;
struct FDataTableCategoryHandle;
struct FDataTableRowHandle;
class FScriptArray;

struct FAngelscriptUDataTableBinds
{
	static void AddRow(UDataTable* DataTable, FName RowName, void* InRowPtr, int InRowTypeId);
	static bool FindRow(const UDataTable* DataTable, FName RowName, void* OutRowPtr, int OutRowTypeId);
	static void GetAllRows(const UDataTable* DataTable, FScriptArray& OutArray, int TypeId);

	static bool GetRowFromHandle(
		const FDataTableRowHandle* RowHandle,
		void* OutRowPtr,
		int OutRowTypeId);

	static TArray<FName> GetCategoryRowNames(const FDataTableCategoryHandle* CategoryHandle);
	static bool GetCategoryRow(
		const FDataTableCategoryHandle* CategoryHandle,
		FName RowName,
		void* OutRowPtr,
		int OutRowTypeId);
	static void GetCategoryRows(
		const FDataTableCategoryHandle* CategoryHandle,
		FScriptArray& OutArray,
		int TypeId);
};
