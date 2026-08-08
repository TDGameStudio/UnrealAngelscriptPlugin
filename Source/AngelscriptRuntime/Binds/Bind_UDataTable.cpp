#include "Bind_UDataTable.h"

#include "Engine/DataTable.h"
#include "Containers/ScriptArray.h"

#include "AngelscriptBinds.h"

/**
 * Data-table row mutation, wildcard row access, row handles, and category handles.
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | AngelScript usage signature                                                                          | Purpose / parameter notes                                                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | DataTable.EmptyTable();                                                                              | Removes every row while preserving the table schema.                                                             |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | TArray<FName> Names = DataTable.GetRowNames() const;                                                 | Returns all row names.                                                                                           |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | DataTable.RemoveRow(FName RowName);                                                                  | Removes a named row.                                                                                             |
 * |                                                                                                      | @param RowName Key of the row to remove.                                                                         |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | DataTable.AddRow(FName RowName, const ?&in InRow);                                                   | Copies a wildcard struct value into a named row.                                                                 |
 * |                                                                                                      | @param RowName Key to add or replace.                                                                            |
 * |                                                                                                      | @param InRow Struct value whose exact type must match the table's RowStruct.                                     |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool bFound = DataTable.FindRow(FName RowName, ?&out OutRow) const;                                  | Copies a named row into a wildcard output and reports success.                                                   |
 * |                                                                                                      | @param RowName Key to find.                                                                                      |
 * |                                                                                                      | @param OutRow Struct output whose exact type must match RowStruct.                                               |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | DataTable.GetAllRows(?& OutArray) const;                                                             | Copies every row into a caller-provided typed array.                                                             |
 * |                                                                                                      | @param OutArray TArray whose element type must exactly match RowStruct.                                          |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool bNull = RowHandle.IsNull() const;                                                               | Reports whether the row handle lacks a table or row name.                                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool bEqual = LeftRowHandle == RightRowHandle;                                                       | Compares data-table and row-name identity.                                                                       |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | FString Text = RowHandle.ToDebugString(bool bUseFullPath = false) const;                             | Returns a diagnostic table/row description.                                                                      |
 * |                                                                                                      | @param bUseFullPath Includes the table's full object path when true.                                             |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool bFound = RowHandle.GetRow(?&out OutRow) const;                                                  | Resolves the handle and copies its row into a wildcard output.                                                   |
 * |                                                                                                      | @param OutRow Struct output whose exact type must match the table schema.                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool bNull = CategoryHandle.IsNull() const;                                                          | Reports whether the category handle lacks a table or column selection.                                           |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool bEqual = LeftCategoryHandle == RightCategoryHandle;                                             | Compares table, column, and category identity.                                                                   |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | TArray<FName> Names = CategoryHandle.GetRowNames() const;                                            | Returns row names selected by the category.                                                                      |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | bool bFound = CategoryHandle.GetRow(FName RowName, ?&out OutRow) const;                              | Resolves a selected named row into a wildcard output.                                                            |
 * |                                                                                                      | @param RowName Selected row key.                                                                                 |
 * |                                                                                                      | @param OutRow Struct output whose exact type must match the table schema.                                        |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 * | CategoryHandle.GetRows(?& OutArray) const;                                                           | Copies every selected row into a typed array.                                                                    |
 * |                                                                                                      | @param OutArray TArray whose element type must exactly match the table schema.                                   |
 * +------------------------------------------------------------------------------------------------------+------------------------------------------------------------------------------------------------------------------+
 */

namespace
{
	void BindUDataTableFunctions(FAngelscriptBinds& Binds)
	{
		auto UDataTable_ = Binds.ExistingClassForTarget("UDataTable");

		UDataTable_.Method("void EmptyTable()", METHOD_TRIVIAL(UDataTable, EmptyTable));
		UDataTable_.Method("TArray<FName> GetRowNames() const", METHOD_TRIVIAL(UDataTable, GetRowNames));
		UDataTable_.Method("void RemoveRow(FName RowName)", METHODPR_TRIVIAL(void, UDataTable, RemoveRow, (FName)));
		UDataTable_.Method("void AddRow(FName RowName, const ?&in InRow)", &FAngelscriptUDataTableBinds::AddRow);
		UDataTable_.Method("bool FindRow(FName RowName, ?&out OutRow) const", &FAngelscriptUDataTableBinds::FindRow);
		UDataTable_.Method("void GetAllRows(?& OutArray) const", &FAngelscriptUDataTableBinds::GetAllRows);

		auto FDataTableRowHandle_ = Binds.ExistingClassForTarget("FDataTableRowHandle");

		FDataTableRowHandle_.Method("bool IsNull() const", METHOD_TRIVIAL(FDataTableRowHandle, IsNull));
		FDataTableRowHandle_.Method("bool opEquals(const FDataTableRowHandle& Other) const",
			METHODPR_TRIVIAL(bool, FDataTableRowHandle, operator==, (const FDataTableRowHandle&) const));
		FDataTableRowHandle_.Method("FString ToDebugString(bool bUseFullPath = false) const",
			METHODPR_TRIVIAL(FString, FDataTableRowHandle, ToDebugString, (bool) const));
		FDataTableRowHandle_.Method("bool GetRow(?&out OutRow) const", &FAngelscriptUDataTableBinds::GetRowFromHandle);

		auto FDataTableCategoryHandle_ = Binds.ExistingClassForTarget("FDataTableCategoryHandle");

		FDataTableCategoryHandle_.Method("bool IsNull() const", METHOD_TRIVIAL(FDataTableCategoryHandle, IsNull));
		FDataTableCategoryHandle_.Method("bool opEquals(const FDataTableCategoryHandle& Other) const",
			METHODPR_TRIVIAL(bool, FDataTableCategoryHandle, operator==, (const FDataTableCategoryHandle&) const));
		FDataTableCategoryHandle_.Method("TArray<FName> GetRowNames() const", &FAngelscriptUDataTableBinds::GetCategoryRowNames);
		FDataTableCategoryHandle_.Method("bool GetRow(FName RowName, ?&out OutRow) const", &FAngelscriptUDataTableBinds::GetCategoryRow);
		FDataTableCategoryHandle_.Method("void GetRows(?& OutArray) const", &FAngelscriptUDataTableBinds::GetCategoryRows);
	}
}

AS_FORCE_LINK const FAngelscriptBind Bind_UDataTable(
	TEXT("UDataTable"),
	EAngelscriptBindPhase::ManualBindings,
	&BindUDataTableFunctions);
