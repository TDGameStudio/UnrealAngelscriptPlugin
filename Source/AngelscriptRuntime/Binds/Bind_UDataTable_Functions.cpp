#include "Bind_UDataTable.h"

#include "AngelscriptEngine.h"

#include "Containers/ScriptArray.h"
#include "Engine/DataTable.h"

#include "StartAngelscriptHeaders.h"
#include "source/as_objecttype.h"
#include "source/as_scriptengine.h"
#include "EndAngelscriptHeaders.h"

namespace
{
	const UScriptStruct* GetStructType(const UDataTable* DataTable, const int TypeId)
	{
		const FAngelscriptTypeUsage Usage = FAngelscriptTypeUsage::FromTypeId(TypeId);
		const UStruct* StructDefinition = Usage.GetUnrealStruct();
		const UScriptStruct* StructType = DataTable->GetRowStruct();

		return StructDefinition != nullptr && StructDefinition == StructType ? StructType : nullptr;
	}

	bool CopyStruct(const UDataTable* DataTable, const FName RowName, void* Destination, const int TypeId)
	{
		if (const void* Row = DataTable->FindRowUnchecked(RowName))
		{
			if (const UScriptStruct* StructType = GetStructType(DataTable, TypeId))
			{
				StructType->CopyScriptStruct(Destination, Row);
				return true;
			}
		}

		return false;
	}

	const UStruct* GetArraySubClass(const UDataTable* DataTable, FScriptArray& OutArray, const int TypeId)
	{
		auto TypeInfo = static_cast<const asCTypeInfo*>(FAngelscriptEngine::Get().Engine->GetTypeInfoById(TypeId));

		if (TypeInfo == nullptr || (TypeInfo->flags & asOBJ_VALUE) == 0)
		{
			FAngelscriptEngine::Throw("OutArray must be a TArray of structs.");
			return nullptr;
		}

		auto ObjectType = static_cast<const asCObjectType*>(TypeInfo);

		if (ObjectType->templateBaseType != FAngelscriptType::GetArrayTemplateTypeInfo())
		{
			FAngelscriptEngine::Throw("OutArray must be a TArray of structs.");
			return nullptr;
		}

		auto SubTypeInfo = static_cast<const asCTypeInfo*>(ObjectType->templateSubTypes[0].GetTypeInfo());

		if (SubTypeInfo == nullptr || (SubTypeInfo->GetFlags() & asOBJ_VALUE) == 0 || SubTypeInfo->plainUserData == 0)
		{
			FAngelscriptEngine::Throw("OutArray must be a TArray of structs.");
			return nullptr;
		}

		auto SubClass = reinterpret_cast<const UStruct*>(SubTypeInfo->plainUserData);

		if (SubClass == nullptr || SubClass != DataTable->GetRowStruct())
		{
			FAngelscriptEngine::Throw("OutArray must be a TArray of structs.");
			return nullptr;
		}

		return SubClass;
	}

	bool IsCategoryHandleValid(const FDataTableCategoryHandle* CategoryHandle)
	{
		return CategoryHandle->DataTable != nullptr
			&& CategoryHandle->ColumnName != NAME_None
			&& CategoryHandle->RowContents != NAME_None;
	}

	using FDataTableMap = TMap<FName, uint8*>;
	using FConstDataTableIterator = FDataTableMap::TConstIterator;

	template <typename CallbackType>
	void ForEachMatchingProperty(const FDataTableCategoryHandle* CategoryHandle, CallbackType&& Callback)
	{
		const UDataTable* DataTable = CategoryHandle->DataTable;

		if (FProperty* Property = DataTable->FindTableProperty(CategoryHandle->ColumnName))
		{
			auto RowContentsAsBinary = static_cast<uint8*>(FMemory_Alloca(Property->GetSize()));
			Property->InitializeValue(RowContentsAsBinary);

			if (Property->ImportText_Direct(*CategoryHandle->RowContents.ToString(), RowContentsAsBinary, nullptr, PPF_None))
			{
				for (FConstDataTableIterator RowIterator = DataTable->GetRowMap().CreateConstIterator(); RowIterator; ++RowIterator)
				{
					const void* Value = Property->ContainerPtrToValuePtr<void>(RowIterator.Value(), 0);
					if (Property->Identical(Value, RowContentsAsBinary, PPF_None))
					{
						Callback(RowIterator);
					}
				}
			}

			Property->DestroyValue(RowContentsAsBinary);
		}
	}

	void AppendAllRows(const UDataTable* DataTable, FScriptArray& OutArray, const UStruct* SubClass)
	{
		const FDataTableMap& RowMap = DataTable->GetRowMap();
		const int32 StartIndex = OutArray.Num();
		const int32 StructureSize = SubClass->GetStructureSize();

		OutArray.Insert(StartIndex, RowMap.Num(), StructureSize, SubClass->GetMinAlignment());

		auto StructType = static_cast<const UScriptStruct*>(SubClass);
		auto Destination = static_cast<uint8*>(OutArray.GetData()) + (StartIndex * StructureSize);

		for (auto RowIterator = RowMap.CreateConstIterator(); RowIterator; ++RowIterator)
		{
			StructType->InitializeStruct(Destination);
			StructType->CopyScriptStruct(Destination, RowIterator.Value());
			Destination += StructureSize;
		}
	}
}

void FAngelscriptUDataTableBinds::AddRow(
	UDataTable* DataTable,
	const FName RowName,
	void* InRowPtr,
	const int InRowTypeId)
{
	if (GetStructType(DataTable, InRowTypeId) != nullptr)
	{
		DataTable->AddRow(RowName, *static_cast<FTableRowBase*>(InRowPtr));
	}
}

bool FAngelscriptUDataTableBinds::FindRow(
	const UDataTable* DataTable,
	const FName RowName,
	void* OutRowPtr,
	const int OutRowTypeId)
{
	return CopyStruct(DataTable, RowName, OutRowPtr, OutRowTypeId);
}

void FAngelscriptUDataTableBinds::GetAllRows(
	const UDataTable* DataTable,
	FScriptArray& OutArray,
	const int TypeId)
{
	if (const UStruct* SubClass = GetArraySubClass(DataTable, OutArray, TypeId))
	{
		AppendAllRows(DataTable, OutArray, SubClass);
	}
}

bool FAngelscriptUDataTableBinds::GetRowFromHandle(
	const FDataTableRowHandle* RowHandle,
	void* OutRowPtr,
	const int OutRowTypeId)
{
	if (RowHandle->DataTable != nullptr && RowHandle->RowName != NAME_None)
	{
		return CopyStruct(RowHandle->DataTable, RowHandle->RowName, OutRowPtr, OutRowTypeId);
	}

	return false;
}

TArray<FName> FAngelscriptUDataTableBinds::GetCategoryRowNames(
	const FDataTableCategoryHandle* CategoryHandle)
{
	TArray<FName> Rows;

	if (IsCategoryHandleValid(CategoryHandle))
	{
		Rows.Reserve(CategoryHandle->DataTable->GetRowMap().Num());
		ForEachMatchingProperty(CategoryHandle, [&Rows](FConstDataTableIterator Iterator)
		{
			Rows.Add(Iterator.Key());
		});
	}

	return Rows;
}

bool FAngelscriptUDataTableBinds::GetCategoryRow(
	const FDataTableCategoryHandle* CategoryHandle,
	const FName RowName,
	void* OutRowPtr,
	const int OutRowTypeId)
{
	return IsCategoryHandleValid(CategoryHandle)
		&& CopyStruct(CategoryHandle->DataTable, RowName, OutRowPtr, OutRowTypeId);
}

void FAngelscriptUDataTableBinds::GetCategoryRows(
	const FDataTableCategoryHandle* CategoryHandle,
	FScriptArray& OutArray,
	const int TypeId)
{
	if (!IsCategoryHandleValid(CategoryHandle))
	{
		return;
	}

	const UDataTable* DataTable = CategoryHandle->DataTable;
	const UStruct* SubClass = GetArraySubClass(DataTable, OutArray, TypeId);
	if (SubClass == nullptr)
	{
		return;
	}

	const FDataTableMap& RowMap = DataTable->GetRowMap();
	TArray<const uint8*> Matches;
	Matches.Reserve(RowMap.Num());
	ForEachMatchingProperty(CategoryHandle, [&Matches](FConstDataTableIterator Iterator)
	{
		Matches.Add(Iterator.Value());
	});

	const int32 StartIndex = OutArray.Num();
	const int32 StructureSize = SubClass->GetStructureSize();
	OutArray.Insert(StartIndex, Matches.Num(), StructureSize, SubClass->GetMinAlignment());

	auto StructType = static_cast<const UScriptStruct*>(SubClass);
	auto Destination = static_cast<uint8*>(OutArray.GetData()) + (StartIndex * StructureSize);

	for (const uint8* Match : Matches)
	{
		StructType->InitializeStruct(Destination);
		StructType->CopyScriptStruct(Destination, Match);
		Destination += StructureSize;
	}
}
