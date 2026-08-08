#include "Engine/DataTable.h"
#include "Containers/ScriptArray.h"

#include "AngelscriptBinds.h"

#include "Bind_UDataTable_Functions.h"

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
