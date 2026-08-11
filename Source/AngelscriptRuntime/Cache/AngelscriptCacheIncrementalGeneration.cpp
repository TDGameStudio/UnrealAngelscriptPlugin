#include "Cache/AngelscriptCacheIncrementalGeneration.h"

namespace AngelscriptCacheIncrementalGeneration_Private
{
	static FAngelscriptCacheIncrementalPreparationResult Failure(
		const EAngelscriptCacheIncrementalPreparationError Error,
		FString&& Detail)
	{
		FAngelscriptCacheIncrementalPreparationResult Result;
		Result.Error = Error;
		Result.Detail = MoveTemp(Detail);
		return Result;
	}

	static FAngelscriptCacheIncrementalPreparationResult ValidationFailure(
		const EAngelscriptCacheIncrementalPreparationError Error,
		const FAngelscriptCacheValidationResult& Validation,
		const TCHAR* Boundary)
	{
		FAngelscriptCacheIncrementalPreparationResult Result = Failure(
			Error,
			FString::Printf(
				TEXT("%s failed: ValidationError=%u Stage=%u Offset=%llu Kind=%u"),
				Boundary,
				static_cast<uint32>(Validation.Error),
				static_cast<uint32>(Validation.Stage),
				static_cast<unsigned long long>(Validation.ByteOffset),
				static_cast<uint32>(Validation.RecordKind)));
		Result.Validation = Validation;
		return Result;
	}

	static int32 CompareRecordIds(
		const FAngelscriptCacheRecordId& Left,
		const FAngelscriptCacheRecordId& Right)
	{
		if (Left < Right)
		{
			return -1;
		}
		if (Right < Left)
		{
			return 1;
		}
		return 0;
	}

	template <typename ValueType, typename ProjectionType>
	static int32 FindRecordId(
		const TArray<ValueType>& SortedValues,
		const FAngelscriptCacheRecordId& RecordId,
		ProjectionType&& Project)
	{
		int32 First = 0;
		int32 Count = SortedValues.Num();
		while (Count > 0)
		{
			const int32 Step = Count / 2;
			const int32 Middle = First + Step;
			if (Project(SortedValues[Middle]) < RecordId)
			{
				First = Middle + 1;
				Count -= Step + 1;
			}
			else
			{
				Count = Step;
			}
		}
		return First < SortedValues.Num()
			&& CompareRecordIds(Project(SortedValues[First]), RecordId) == 0
			? First
			: INDEX_NONE;
	}

	static int32 FindRecordId(
		const TArray<FAngelscriptCacheRecordId>& SortedValues,
		const FAngelscriptCacheRecordId& RecordId)
	{
		return FindRecordId(
			SortedValues,
			RecordId,
			[](const FAngelscriptCacheRecordId& Value)
			{
				return Value;
			});
	}

	static FAngelscriptCachePackLocation LocationFromPack(
		const FAngelscriptEncodedPack& Pack,
		const FAngelscriptCachePackIndexEntry& Entry)
	{
		FAngelscriptCachePackLocation Location;
		Location.PackId = Pack.PackId;
		Location.PackOffset = Entry.PackOffset;
		Location.StoredSize = Entry.StoredSize;
		Location.RawSize = Entry.RawSize;
		Location.Codec = Entry.Codec;
		Location.RawChecksum = Entry.RawChecksum;
		return Location;
	}
}

FAngelscriptCacheIncrementalPreparationResult
PrepareAngelscriptCacheIncrementalGeneration(
	const FAngelscriptValidatedGeneration& BaseGeneration,
	const FAngelscriptValidatedGeneration& CurrentGeneration,
	const FAngelscriptCachePackPolicy& PackPolicy,
	const FAngelscriptCacheReadLimits& Limits,
	IAngelscriptCacheStorageCodec& Codec,
	FAngelscriptCachePreparedIncrementalGeneration& OutGeneration)
{
	using namespace AngelscriptCacheIncrementalGeneration_Private;

	OutGeneration.Reset();
	const FAngelscriptCacheValidationResult BaseManifestValidation =
		ValidateAngelscriptCacheGenerationManifestValue(
			BaseGeneration.Manifest, Limits);
	if (!BaseManifestValidation.IsSuccess())
	{
		return ValidationFailure(
			EAngelscriptCacheIncrementalPreparationError::InvalidBaseManifest,
			BaseManifestValidation,
			TEXT("Base Manifest validation"));
	}

	const FAngelscriptCacheValidationResult CurrentManifestValidation =
		ValidateAngelscriptCacheGenerationManifestValue(
			CurrentGeneration.Manifest, Limits);
	if (!CurrentManifestValidation.IsSuccess())
	{
		return ValidationFailure(
			EAngelscriptCacheIncrementalPreparationError::InvalidCurrentManifest,
			CurrentManifestValidation,
			TEXT("Current Manifest validation"));
	}

	FAngelscriptCacheSemanticDiffResult SemanticDiff =
		DiffAngelscriptCacheValidatedGenerations(
			BaseGeneration, CurrentGeneration);
	if (!SemanticDiff.IsSuccess())
	{
		FAngelscriptCacheIncrementalPreparationResult Result = Failure(
			EAngelscriptCacheIncrementalPreparationError::SemanticDiffFailed,
			FString::Printf(
				TEXT("Semantic Generation diff failed: Error=%u Detail=%s"),
				static_cast<uint32>(SemanticDiff.Error),
				*SemanticDiff.Detail));
		Result.SemanticDiffError = SemanticDiff.Error;
		return Result;
	}

	FAngelscriptCachePreparedIncrementalGeneration Candidate;
	Candidate.SemanticDiff = MoveTemp(SemanticDiff);
	const int32 ExpectedCurrentRecordCount =
		Candidate.SemanticDiff.ReusedRecordIds.Num()
		+ Candidate.SemanticDiff.NewRecordIds.Num();
	if (CurrentGeneration.Manifest.Records.Num() != ExpectedCurrentRecordCount
		|| CurrentGeneration.ReachableRecords.Num() != ExpectedCurrentRecordCount)
	{
		return Failure(
			EAngelscriptCacheIncrementalPreparationError::UnexpectedCurrentRecord,
			FString::Printf(
				TEXT("Current generation record coverage disagrees with semantic diff: Manifest=%d Decoded=%d Expected=%d"),
				CurrentGeneration.Manifest.Records.Num(),
				CurrentGeneration.ReachableRecords.Num(),
				ExpectedCurrentRecordCount));
	}

	TArray<const FAngelscriptDecodedCacheRecord*> CurrentRecords;
	CurrentRecords.Reserve(CurrentGeneration.ReachableRecords.Num());
	for (const FAngelscriptDecodedCacheRecordHandle& Handle :
		CurrentGeneration.ReachableRecords)
	{
		CurrentRecords.Add(&Handle.Get());
	}
	CurrentRecords.Sort([](
		const FAngelscriptDecodedCacheRecord& Left,
		const FAngelscriptDecodedCacheRecord& Right)
	{
		return Left.GetRecordId() < Right.GetRecordId();
	});
	for (int32 Index = 1; Index < CurrentRecords.Num(); ++Index)
	{
		if (CurrentRecords[Index - 1]->GetRecordId()
			== CurrentRecords[Index]->GetRecordId())
		{
			return Failure(
				EAngelscriptCacheIncrementalPreparationError::UnexpectedCurrentRecord,
				TEXT("Current generation contains duplicate decoded RecordIds"));
		}
	}

	TArray<FAngelscriptPreparedRecord> NewRecords;
	NewRecords.Reserve(Candidate.SemanticDiff.NewRecordIds.Num());
	for (const FAngelscriptCacheRecordId& NewRecordId :
		Candidate.SemanticDiff.NewRecordIds)
	{
		const int32 RecordIndex = FindRecordId(
			CurrentRecords,
			NewRecordId,
			[](const FAngelscriptDecodedCacheRecord* Record)
			{
				return Record->GetRecordId();
			});
		if (RecordIndex == INDEX_NONE)
		{
			return Failure(
				EAngelscriptCacheIncrementalPreparationError::MissingCurrentRecord,
				FString::Printf(
					TEXT("New semantic RecordId is absent from current decoded records: Kind=%u Hash=%s"),
					static_cast<uint32>(NewRecordId.Kind),
					*NewRecordId.ContentHash.ToHexString()));
		}

		FAngelscriptPreparedRecord& Prepared = NewRecords.AddDefaulted_GetRef();
		Prepared.RecordId = NewRecordId;
		Prepared.CanonicalPayload.Append(
			CurrentRecords[RecordIndex]->GetCanonicalPayload());
	}

	const FAngelscriptCacheValidationResult PackResult =
		BuildAngelscriptCachePacks(
			NewRecords, PackPolicy, Codec, Candidate.NewPacks);
	if (!PackResult.IsSuccess())
	{
		return ValidationFailure(
			EAngelscriptCacheIncrementalPreparationError::PackBuildFailed,
			PackResult,
			TEXT("Incremental new-record Pack build"));
	}

	TArray<FAngelscriptCacheRecordIndexEntry> NewLocations;
	NewLocations.Reserve(Candidate.SemanticDiff.NewRecordIds.Num());
	for (const FAngelscriptEncodedPack& Pack : Candidate.NewPacks)
	{
		for (const FAngelscriptCachePackIndexEntry& Entry : Pack.Index)
		{
			NewLocations.Add({Entry.RecordId, LocationFromPack(Pack, Entry)});
		}
	}
	NewLocations.Sort([](
		const FAngelscriptCacheRecordIndexEntry& Left,
		const FAngelscriptCacheRecordIndexEntry& Right)
	{
		return Left.RecordId < Right.RecordId;
	});
	if (NewLocations.Num() != Candidate.SemanticDiff.NewRecordIds.Num())
	{
		return Failure(
			EAngelscriptCacheIncrementalPreparationError::NewPackIndexMismatch,
			FString::Printf(
				TEXT("New Pack index coverage mismatch: Indexed=%d Expected=%d"),
				NewLocations.Num(),
				Candidate.SemanticDiff.NewRecordIds.Num()));
	}
	for (int32 Index = 0; Index < NewLocations.Num(); ++Index)
	{
		if (!(NewLocations[Index].RecordId
			== Candidate.SemanticDiff.NewRecordIds[Index]))
		{
			return Failure(
				EAngelscriptCacheIncrementalPreparationError::NewPackIndexMismatch,
				TEXT("New Pack indexes do not exactly match semantic NewRecordIds"));
		}
	}

	Candidate.Manifest = CurrentGeneration.Manifest;
	Candidate.Manifest.Records.Reset(CurrentGeneration.Manifest.Records.Num());
	for (const FAngelscriptCacheRecordIndexEntry& CurrentEntry :
		CurrentGeneration.Manifest.Records)
	{
		const int32 NewIndex = FindRecordId(
			Candidate.SemanticDiff.NewRecordIds, CurrentEntry.RecordId);
		if (NewIndex != INDEX_NONE)
		{
			Candidate.Manifest.Records.Add(NewLocations[NewIndex]);
			continue;
		}

		if (FindRecordId(
			Candidate.SemanticDiff.ReusedRecordIds,
			CurrentEntry.RecordId) == INDEX_NONE)
		{
			return Failure(
				EAngelscriptCacheIncrementalPreparationError::UnexpectedCurrentRecord,
				TEXT("Current Manifest RecordId is neither new nor reused"));
		}
		const int32 BaseIndex = FindRecordId(
			BaseGeneration.Manifest.Records,
			CurrentEntry.RecordId,
			[](const FAngelscriptCacheRecordIndexEntry& Entry)
			{
				return Entry.RecordId;
			});
		if (BaseIndex == INDEX_NONE)
		{
			return Failure(
				EAngelscriptCacheIncrementalPreparationError::MissingBaseLocation,
				FString::Printf(
					TEXT("Reused semantic RecordId has no validated base location: Kind=%u Hash=%s"),
					static_cast<uint32>(CurrentEntry.RecordId.Kind),
					*CurrentEntry.RecordId.ContentHash.ToHexString()));
		}
		Candidate.Manifest.Records.Add(BaseGeneration.Manifest.Records[BaseIndex]);
	}

	const FAngelscriptCacheValidationResult PreparedManifestValidation =
		ValidateAngelscriptCacheGenerationManifestValue(
			Candidate.Manifest, Limits);
	if (!PreparedManifestValidation.IsSuccess())
	{
		return ValidationFailure(
			EAngelscriptCacheIncrementalPreparationError::ManifestBuildFailed,
			PreparedManifestValidation,
			TEXT("Incremental complete Manifest validation"));
	}

	const FAngelscriptCacheValidationResult EncodeResult =
		EncodeAngelscriptCacheGenerationManifest(
			Candidate.Manifest, Candidate.EncodedManifest);
	if (!EncodeResult.IsSuccess())
	{
		return ValidationFailure(
			EAngelscriptCacheIncrementalPreparationError::ManifestBuildFailed,
			EncodeResult,
			TEXT("Incremental complete Manifest encode"));
	}

	FAngelscriptCacheIncrementalPreparationResult Result;
	Result.Detail = FString::Printf(
		TEXT("Prepared incremental Generation %s: Reused=%d New=%d Retired=%d NewPacks=%d CompleteRecords=%d"),
		*Candidate.EncodedManifest.ComputedGenerationId.ToHexString(),
		Candidate.SemanticDiff.ReusedRecordIds.Num(),
		Candidate.SemanticDiff.NewRecordIds.Num(),
		Candidate.SemanticDiff.RetiredRecordIds.Num(),
		Candidate.NewPacks.Num(),
		Candidate.Manifest.Records.Num());
	OutGeneration = MoveTemp(Candidate);
	return Result;
}
