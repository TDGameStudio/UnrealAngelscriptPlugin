#include "Dump/AngelscriptStateDiff.h"

int32 FAngelscriptStateDiff::CountByChangeType(const EAngelscriptStateDiffChangeType ChangeType) const
{
	int32 Count = 0;
	for (const FAngelscriptStateDiffRow& Row : Rows)
	{
		if (Row.ChangeType == ChangeType)
		{
			++Count;
		}
	}
	return Count;
}

FAngelscriptStateDiff FAngelscriptStateDiffBuilder::Diff(const FAngelscriptStateSnapshot& Before, const FAngelscriptStateSnapshot& After)
{
	TMap<FString, const FAngelscriptStateSnapshotRow*> BeforeByKey;
	TMap<FString, const FAngelscriptStateSnapshotRow*> AfterByKey;
	BeforeByKey.Reserve(Before.Rows.Num());
	AfterByKey.Reserve(After.Rows.Num());

	for (const FAngelscriptStateSnapshotRow& Row : Before.Rows)
	{
		BeforeByKey.Add(Row.MakeKey(), &Row);
	}
	for (const FAngelscriptStateSnapshotRow& Row : After.Rows)
	{
		AfterByKey.Add(Row.MakeKey(), &Row);
	}

	FAngelscriptStateDiff Result;
	Result.Rows.Reserve(FMath::Abs(After.Rows.Num() - Before.Rows.Num()) + 64);
	for (const FAngelscriptStateSnapshotRow& BeforeRow : Before.Rows)
	{
		const FAngelscriptStateSnapshotRow* const* AfterRowPtr = AfterByKey.Find(BeforeRow.MakeKey());
		const FAngelscriptStateSnapshotRow* AfterRow = AfterRowPtr != nullptr ? *AfterRowPtr : nullptr;

		FAngelscriptStateDiffRow DiffRow;
		if (AfterRow == nullptr)
		{
			DiffRow.ChangeType = EAngelscriptStateDiffChangeType::Removed;
			DiffRow.Category = BeforeRow.Category;
			DiffRow.Identity = BeforeRow.Identity;
			DiffRow.Field = BeforeRow.Field;
			DiffRow.BeforeValue = BeforeRow.Value;
			DiffRow.Source = BeforeRow.Source;
		}
		else if (BeforeRow.Value != AfterRow->Value)
		{
			DiffRow.ChangeType = EAngelscriptStateDiffChangeType::Changed;
			DiffRow.Category = AfterRow->Category;
			DiffRow.Identity = AfterRow->Identity;
			DiffRow.Field = AfterRow->Field;
			DiffRow.BeforeValue = BeforeRow.Value;
			DiffRow.AfterValue = AfterRow->Value;
			DiffRow.Source = AfterRow->Source;
		}
		else
		{
			continue;
		}

		Result.Rows.Add(MoveTemp(DiffRow));
	}

	for (const FAngelscriptStateSnapshotRow& AfterRow : After.Rows)
	{
		if (BeforeByKey.Contains(AfterRow.MakeKey()))
		{
			continue;
		}

		FAngelscriptStateDiffRow DiffRow;
		DiffRow.ChangeType = EAngelscriptStateDiffChangeType::Added;
		DiffRow.Category = AfterRow.Category;
		DiffRow.Identity = AfterRow.Identity;
		DiffRow.Field = AfterRow.Field;
		DiffRow.AfterValue = AfterRow.Value;
		DiffRow.Source = AfterRow.Source;
		Result.Rows.Add(MoveTemp(DiffRow));
	}

	Result.Rows.Sort(
		[](const FAngelscriptStateDiffRow& Left, const FAngelscriptStateDiffRow& Right)
		{
			if (Left.Category != Right.Category)
			{
				return Left.Category < Right.Category;
			}
			if (Left.Identity != Right.Identity)
			{
				return Left.Identity < Right.Identity;
			}
			if (Left.Field != Right.Field)
			{
				return Left.Field < Right.Field;
			}
			return static_cast<uint8>(Left.ChangeType) < static_cast<uint8>(Right.ChangeType);
		});

	return Result;
}

FString FAngelscriptStateDiffBuilder::ChangeTypeToString(const EAngelscriptStateDiffChangeType ChangeType)
{
	switch (ChangeType)
	{
	case EAngelscriptStateDiffChangeType::Added:
		return TEXT("Added");
	case EAngelscriptStateDiffChangeType::Removed:
		return TEXT("Removed");
	case EAngelscriptStateDiffChangeType::Changed:
		return TEXT("Changed");
	case EAngelscriptStateDiffChangeType::Unchanged:
	default:
		return TEXT("Unchanged");
	}
}
