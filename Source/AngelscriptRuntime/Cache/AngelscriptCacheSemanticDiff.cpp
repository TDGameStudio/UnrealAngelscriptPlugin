#include "Cache/AngelscriptCacheSemanticDiff.h"

namespace AngelscriptCacheSemanticDiff_Private
{
	struct FTypeEntry
	{
		FAngelscriptStableTypeKey Key;
		FAngelscriptCacheRecordId RecordId;
	};

	struct FFunctionEntry
	{
		FAngelscriptStableFunctionKey Key;
		FAngelscriptCacheRecordId BodyRecordId;
		TOptional<FAngelscriptCacheRecordId> DebugRecordId;
	};

	struct FModuleEntry
	{
		FAngelscriptStableModuleKey Key;
		FAngelscriptCacheRecordId SnapshotRecordId;
		FAngelscriptCacheRecordId InterfaceRecordId;
		FAngelscriptCacheRecordId StateRecordId;
		TArray<FTypeEntry> Types;
		TArray<FFunctionEntry> Functions;
	};

	struct FGenerationView
	{
		FAngelscriptCacheRecordId SourceIndexRecordId;
		TArray<FAngelscriptCacheRecordId> RecordIds;
		TArray<FModuleEntry> Modules;
	};

	static bool Fail(
		FAngelscriptCacheSemanticDiffResult& OutResult,
		const EAngelscriptCacheSemanticDiffError Error,
		FString Detail)
	{
		OutResult.Error = Error;
		OutResult.Detail = MoveTemp(Detail);
		return false;
	}

	static const FAngelscriptDecodedCacheRecord* FindRecord(
		const TConstArrayView<const FAngelscriptDecodedCacheRecord*> Records,
		const FAngelscriptCacheRecordId& RecordId)
	{
		int32 First = 0;
		int32 Count = Records.Num();
		while (Count > 0)
		{
			const int32 Step = Count / 2;
			const int32 Index = First + Step;
			if (Records[Index]->GetRecordId() < RecordId)
			{
				First = Index + 1;
				Count -= Step + 1;
			}
			else
			{
				Count = Step;
			}
		}
		return First < Records.Num()
			&& Records[First]->GetRecordId() == RecordId
			? Records[First]
			: nullptr;
	}

	static bool BuildGenerationView(
		const FAngelscriptValidatedGeneration& Generation,
		FGenerationView& OutView,
		FAngelscriptCacheSemanticDiffResult& OutResult)
	{
		OutView = {};
		if (Generation.Manifest.SourceIndexRecordId.Kind
			!= EAngelscriptCacheRecordKind::SourceIndex)
		{
			return Fail(OutResult,
				EAngelscriptCacheSemanticDiffError::WrongRecordKind,
				TEXT("Validated generation SourceIndex root has the wrong record kind"));
		}

		TArray<const FAngelscriptDecodedCacheRecord*> Records;
		Records.Reserve(Generation.ReachableRecords.Num());
		for (const FAngelscriptDecodedCacheRecordHandle& Handle
			: Generation.ReachableRecords)
		{
			Records.Add(&Handle.Get());
		}
		Records.Sort([](
			const FAngelscriptDecodedCacheRecord& A,
			const FAngelscriptDecodedCacheRecord& B)
			{
				return A.GetRecordId() < B.GetRecordId();
			});
		for (int32 Index = 1; Index < Records.Num(); ++Index)
		{
			if (Records[Index - 1]->GetRecordId()
				== Records[Index]->GetRecordId())
			{
				return Fail(OutResult,
					EAngelscriptCacheSemanticDiffError::InvalidValidatedGeneration,
					TEXT("Validated generation contains a duplicate reachable RecordId"));
			}
		}

		const FAngelscriptDecodedCacheRecord* SourceRecord = FindRecord(
			Records, Generation.Manifest.SourceIndexRecordId);
		if (SourceRecord == nullptr)
		{
			return Fail(OutResult,
				EAngelscriptCacheSemanticDiffError::MissingRecord,
				TEXT("Validated generation is missing its SourceIndex root"));
		}
		const FAngelscriptCachedSourceIndex* SourceIndex =
			SourceRecord->TryGetSourceIndex();
		if (SourceIndex == nullptr)
		{
			return Fail(OutResult,
				EAngelscriptCacheSemanticDiffError::WrongRecordKind,
				TEXT("Validated generation SourceIndex root does not decode as SourceIndex"));
		}
		if (!(SourceIndex->SourceSnapshot == Generation.Manifest.SourceSnapshot))
		{
			return Fail(OutResult,
				EAngelscriptCacheSemanticDiffError::EmbeddedOwnerMismatch,
				TEXT("Validated generation SourceIndex snapshot disagrees with its Manifest"));
		}

		OutView.SourceIndexRecordId = Generation.Manifest.SourceIndexRecordId;
		OutView.RecordIds.Reserve(Records.Num());
		for (const FAngelscriptDecodedCacheRecord* Record : Records)
		{
			OutView.RecordIds.Add(Record->GetRecordId());
		}

		OutView.Modules.Reserve(Generation.Manifest.ModuleSnapshots.Num());
		for (const FAngelscriptCacheModuleSnapshotLink& Root
			: Generation.Manifest.ModuleSnapshots)
		{
			if (Root.RecordId.Kind
				!= EAngelscriptCacheRecordKind::ModuleSnapshot)
			{
				return Fail(OutResult,
					EAngelscriptCacheSemanticDiffError::WrongRecordKind,
					TEXT("Validated generation has a non-ModuleSnapshot module root"));
			}
			const FAngelscriptDecodedCacheRecord* SnapshotRecord =
				FindRecord(Records, Root.RecordId);
			if (SnapshotRecord == nullptr)
			{
				return Fail(OutResult,
					EAngelscriptCacheSemanticDiffError::MissingRecord,
					TEXT("Validated generation is missing a ModuleSnapshot root record"));
			}
			const FAngelscriptCachedModuleSnapshot* Snapshot =
				SnapshotRecord->TryGetModuleSnapshot();
			if (Snapshot == nullptr)
			{
				return Fail(OutResult,
					EAngelscriptCacheSemanticDiffError::WrongRecordKind,
					TEXT("ModuleSnapshot root does not decode as ModuleSnapshot"));
			}
			if (Root.ModuleKey != Snapshot->ModuleKey)
			{
				return Fail(OutResult,
					EAngelscriptCacheSemanticDiffError::EmbeddedOwnerMismatch,
					TEXT("ModuleSnapshot embedded ModuleKey disagrees with its Manifest root"));
			}

			FModuleEntry& Module = OutView.Modules.AddDefaulted_GetRef();
			Module.Key = Root.ModuleKey;
			Module.SnapshotRecordId = Root.RecordId;
			Module.InterfaceRecordId = Snapshot->ModuleInterface.RecordId;
			Module.StateRecordId = Snapshot->ModuleState.RecordId;

			if (Snapshot->ModuleInterface.ModuleKey != Module.Key
				|| Snapshot->ModuleState.ModuleKey != Module.Key)
			{
				return Fail(OutResult,
					EAngelscriptCacheSemanticDiffError::EmbeddedOwnerMismatch,
					TEXT("ModuleSnapshot interface/state links disagree with their module owner"));
			}
			if (Module.InterfaceRecordId.Kind
					!= EAngelscriptCacheRecordKind::ModuleInterface
				|| Module.StateRecordId.Kind
					!= EAngelscriptCacheRecordKind::ModuleState)
			{
				return Fail(OutResult,
					EAngelscriptCacheSemanticDiffError::WrongRecordKind,
					TEXT("ModuleSnapshot interface/state link has the wrong record kind"));
			}

			const FAngelscriptDecodedCacheRecord* InterfaceRecord =
				FindRecord(Records, Module.InterfaceRecordId);
			const FAngelscriptDecodedCacheRecord* StateRecord =
				FindRecord(Records, Module.StateRecordId);
			if (InterfaceRecord == nullptr || StateRecord == nullptr)
			{
				return Fail(OutResult,
					EAngelscriptCacheSemanticDiffError::MissingRecord,
					TEXT("ModuleSnapshot is missing its interface or state record"));
			}
			const FAngelscriptCachedModuleInterface* Interface =
				InterfaceRecord->TryGetModuleInterface();
			const FAngelscriptCachedModuleState* State =
				StateRecord->TryGetModuleState();
			if (Interface == nullptr || State == nullptr)
			{
				return Fail(OutResult,
					EAngelscriptCacheSemanticDiffError::WrongRecordKind,
					TEXT("ModuleSnapshot interface or state record decoded as the wrong kind"));
			}
			if (Interface->ModuleKey != Module.Key
				|| State->ModuleKey != Module.Key)
			{
				return Fail(OutResult,
					EAngelscriptCacheSemanticDiffError::EmbeddedOwnerMismatch,
					TEXT("Module interface or state embedded ModuleKey disagrees with its root"));
			}

			Module.Types.Reserve(Snapshot->TypeSchemas.Num());
			for (const FAngelscriptCachedTypeSchemaLink& Link
				: Snapshot->TypeSchemas)
			{
				if (Link.RecordId.Kind
					!= EAngelscriptCacheRecordKind::TypeSchema)
				{
					return Fail(OutResult,
						EAngelscriptCacheSemanticDiffError::WrongRecordKind,
						TEXT("ModuleSnapshot TypeSchema link has the wrong record kind"));
				}
				const FAngelscriptDecodedCacheRecord* TypeRecord =
					FindRecord(Records, Link.RecordId);
				if (TypeRecord == nullptr)
				{
					return Fail(OutResult,
						EAngelscriptCacheSemanticDiffError::MissingRecord,
						TEXT("ModuleSnapshot is missing a linked TypeSchema"));
				}
				const FAngelscriptCachedTypeSchema* TypeSchema =
					TypeRecord->TryGetTypeSchema();
				if (TypeSchema == nullptr)
				{
					return Fail(OutResult,
						EAngelscriptCacheSemanticDiffError::WrongRecordKind,
						TEXT("Linked TypeSchema record decoded as the wrong kind"));
				}
				if (TypeSchema->ModuleKey != Module.Key
					|| TypeSchema->TypeKey != Link.TypeKey)
				{
					return Fail(OutResult,
						EAngelscriptCacheSemanticDiffError::EmbeddedOwnerMismatch,
						TEXT("Linked TypeSchema embedded owner disagrees with its link"));
				}
				Module.Types.Add({Link.TypeKey, Link.RecordId});
			}
			Module.Types.Sort([](const FTypeEntry& A, const FTypeEntry& B)
				{
					return A.Key.Hash < B.Key.Hash;
				});
			for (int32 Index = 1; Index < Module.Types.Num(); ++Index)
			{
				if (Module.Types[Index - 1].Key
					== Module.Types[Index].Key)
				{
					return Fail(OutResult,
						EAngelscriptCacheSemanticDiffError::DuplicateSemanticOwner,
						TEXT("ModuleSnapshot contains a duplicate TypeKey"));
				}
			}

			Module.Functions.Reserve(Snapshot->FunctionBodies.Num());
			for (const FAngelscriptCachedFunctionBodyLink& Link
				: Snapshot->FunctionBodies)
			{
				if (Link.RecordId.Kind
					!= EAngelscriptCacheRecordKind::FunctionBody)
				{
					return Fail(OutResult,
						EAngelscriptCacheSemanticDiffError::WrongRecordKind,
						TEXT("ModuleSnapshot FunctionBody link has the wrong record kind"));
				}
				const FAngelscriptDecodedCacheRecord* FunctionRecord =
					FindRecord(Records, Link.RecordId);
				if (FunctionRecord == nullptr)
				{
					return Fail(OutResult,
						EAngelscriptCacheSemanticDiffError::MissingRecord,
						TEXT("ModuleSnapshot is missing a linked FunctionBody"));
				}
				const FAngelscriptCachedFunctionBody* FunctionBody =
					FunctionRecord->TryGetFunctionBody();
				if (FunctionBody == nullptr)
				{
					return Fail(OutResult,
						EAngelscriptCacheSemanticDiffError::WrongRecordKind,
						TEXT("Linked FunctionBody record decoded as the wrong kind"));
				}
				if (FunctionBody->ModuleKey != Module.Key
					|| FunctionBody->Identity.FunctionKey != Link.FunctionKey)
				{
					return Fail(OutResult,
						EAngelscriptCacheSemanticDiffError::EmbeddedOwnerMismatch,
						TEXT("Linked FunctionBody embedded owner disagrees with its link"));
				}

				FFunctionEntry& Function =
					Module.Functions.AddDefaulted_GetRef();
				Function.Key = Link.FunctionKey;
				Function.BodyRecordId = Link.RecordId;
				Function.DebugRecordId = FunctionBody->DebugSidecar;
				if (Function.DebugRecordId.IsSet())
				{
					if (Function.DebugRecordId->Kind
						!= EAngelscriptCacheRecordKind::DebugSidecar)
					{
						return Fail(OutResult,
							EAngelscriptCacheSemanticDiffError::WrongRecordKind,
							TEXT("FunctionBody DebugSidecar link has the wrong record kind"));
					}
					const FAngelscriptDecodedCacheRecord* DebugRecord =
						FindRecord(Records, Function.DebugRecordId.GetValue());
					if (DebugRecord == nullptr)
					{
						return Fail(OutResult,
							EAngelscriptCacheSemanticDiffError::MissingRecord,
							TEXT("FunctionBody is missing its linked DebugSidecar"));
					}
					const FAngelscriptCachedDebugSidecar* DebugSidecar =
						DebugRecord->TryGetDebugSidecar();
					if (DebugSidecar == nullptr)
					{
						return Fail(OutResult,
							EAngelscriptCacheSemanticDiffError::WrongRecordKind,
							TEXT("Linked DebugSidecar record decoded as the wrong kind"));
					}
					if (DebugSidecar->FunctionKey != Function.Key)
					{
						return Fail(OutResult,
							EAngelscriptCacheSemanticDiffError::EmbeddedOwnerMismatch,
							TEXT("Linked DebugSidecar FunctionKey disagrees with its owner"));
					}
				}
			}
			Module.Functions.Sort([](
				const FFunctionEntry& A,
				const FFunctionEntry& B)
				{
					return A.Key.Hash < B.Key.Hash;
				});
			for (int32 Index = 1; Index < Module.Functions.Num(); ++Index)
			{
				if (Module.Functions[Index - 1].Key
					== Module.Functions[Index].Key)
				{
					return Fail(OutResult,
						EAngelscriptCacheSemanticDiffError::DuplicateSemanticOwner,
						TEXT("ModuleSnapshot contains a duplicate StableFunctionKey"));
				}
			}
		}

		OutView.Modules.Sort([](const FModuleEntry& A, const FModuleEntry& B)
			{
				return A.Key.Hash < B.Key.Hash;
			});
		for (int32 Index = 1; Index < OutView.Modules.Num(); ++Index)
		{
			if (OutView.Modules[Index - 1].Key
				== OutView.Modules[Index].Key)
			{
				return Fail(OutResult,
					EAngelscriptCacheSemanticDiffError::DuplicateSemanticOwner,
					TEXT("Validated generation contains a duplicate ModuleKey root"));
			}
		}
		return true;
	}

	static FAngelscriptCacheSemanticRecordChange MakeRecordChange(
		const FAngelscriptCacheRecordId* Previous,
		const FAngelscriptCacheRecordId* Current)
	{
		FAngelscriptCacheSemanticRecordChange Change;
		if (Previous != nullptr)
		{
			Change.PreviousRecordId = *Previous;
		}
		if (Current != nullptr)
		{
			Change.CurrentRecordId = *Current;
		}
		if (Previous == nullptr)
		{
			Change.Disposition =
				EAngelscriptCacheSemanticChangeDisposition::Added;
		}
		else if (Current == nullptr)
		{
			Change.Disposition =
				EAngelscriptCacheSemanticChangeDisposition::Removed;
		}
		else if (!(*Previous == *Current))
		{
			Change.Disposition =
				EAngelscriptCacheSemanticChangeDisposition::Modified;
		}
		return Change;
	}

	static FAngelscriptCacheSemanticRecordChange MakeOptionalRecordChange(
		const TOptional<FAngelscriptCacheRecordId>& Previous,
		const TOptional<FAngelscriptCacheRecordId>& Current)
	{
		return MakeRecordChange(
			Previous.IsSet() ? &Previous.GetValue() : nullptr,
			Current.IsSet() ? &Current.GetValue() : nullptr);
	}

	static void AppendAllTypeChanges(
		const TConstArrayView<FTypeEntry> Types,
		const EAngelscriptCacheSemanticChangeDisposition Disposition,
		TArray<FAngelscriptCacheSemanticTypeChange>& OutChanges)
	{
		for (const FTypeEntry& Type : Types)
		{
			FAngelscriptCacheSemanticTypeChange& Change =
				OutChanges.AddDefaulted_GetRef();
			Change.TypeKey = Type.Key;
			Change.Record = Disposition
				== EAngelscriptCacheSemanticChangeDisposition::Added
				? MakeRecordChange(nullptr, &Type.RecordId)
				: MakeRecordChange(&Type.RecordId, nullptr);
		}
	}

	static void AppendAllFunctionChanges(
		const TConstArrayView<FFunctionEntry> Functions,
		const EAngelscriptCacheSemanticChangeDisposition Disposition,
		TArray<FAngelscriptCacheSemanticFunctionChange>& OutBodyChanges,
		TArray<FAngelscriptCacheSemanticFunctionChange>& OutDebugChanges)
	{
		for (const FFunctionEntry& Function : Functions)
		{
			FAngelscriptCacheSemanticFunctionChange& BodyChange =
				OutBodyChanges.AddDefaulted_GetRef();
			BodyChange.FunctionKey = Function.Key;
			BodyChange.Record = Disposition
				== EAngelscriptCacheSemanticChangeDisposition::Added
				? MakeRecordChange(nullptr, &Function.BodyRecordId)
				: MakeRecordChange(&Function.BodyRecordId, nullptr);

			if (Function.DebugRecordId.IsSet())
			{
				FAngelscriptCacheSemanticFunctionChange& DebugChange =
					OutDebugChanges.AddDefaulted_GetRef();
				DebugChange.FunctionKey = Function.Key;
				DebugChange.Record = Disposition
					== EAngelscriptCacheSemanticChangeDisposition::Added
					? MakeRecordChange(
						nullptr, &Function.DebugRecordId.GetValue())
					: MakeRecordChange(
						&Function.DebugRecordId.GetValue(), nullptr);
			}
		}
	}

	static void DiffTypes(
		const TConstArrayView<FTypeEntry> Previous,
		const TConstArrayView<FTypeEntry> Current,
		TArray<FAngelscriptCacheSemanticTypeChange>& OutChanges)
	{
		int32 PreviousIndex = 0;
		int32 CurrentIndex = 0;
		while (PreviousIndex < Previous.Num() || CurrentIndex < Current.Num())
		{
			const bool bPreviousOnly = CurrentIndex >= Current.Num()
				|| (PreviousIndex < Previous.Num()
					&& Previous[PreviousIndex].Key.Hash
						< Current[CurrentIndex].Key.Hash);
			const bool bCurrentOnly = PreviousIndex >= Previous.Num()
				|| (CurrentIndex < Current.Num()
					&& Current[CurrentIndex].Key.Hash
						< Previous[PreviousIndex].Key.Hash);
			if (bPreviousOnly)
			{
				AppendAllTypeChanges(
					TConstArrayView<FTypeEntry>(&Previous[PreviousIndex], 1),
					EAngelscriptCacheSemanticChangeDisposition::Removed,
					OutChanges);
				++PreviousIndex;
			}
			else if (bCurrentOnly)
			{
				AppendAllTypeChanges(
					TConstArrayView<FTypeEntry>(&Current[CurrentIndex], 1),
					EAngelscriptCacheSemanticChangeDisposition::Added,
					OutChanges);
				++CurrentIndex;
			}
			else
			{
				const FTypeEntry& Before = Previous[PreviousIndex++];
				const FTypeEntry& After = Current[CurrentIndex++];
				if (!(Before.RecordId == After.RecordId))
				{
					FAngelscriptCacheSemanticTypeChange& Change =
						OutChanges.AddDefaulted_GetRef();
					Change.TypeKey = Before.Key;
					Change.Record = MakeRecordChange(
						&Before.RecordId, &After.RecordId);
				}
			}
		}
	}

	static void DiffFunctions(
		const TConstArrayView<FFunctionEntry> Previous,
		const TConstArrayView<FFunctionEntry> Current,
		TArray<FAngelscriptCacheSemanticFunctionChange>& OutBodyChanges,
		TArray<FAngelscriptCacheSemanticFunctionChange>& OutDebugChanges)
	{
		int32 PreviousIndex = 0;
		int32 CurrentIndex = 0;
		while (PreviousIndex < Previous.Num() || CurrentIndex < Current.Num())
		{
			const bool bPreviousOnly = CurrentIndex >= Current.Num()
				|| (PreviousIndex < Previous.Num()
					&& Previous[PreviousIndex].Key.Hash
						< Current[CurrentIndex].Key.Hash);
			const bool bCurrentOnly = PreviousIndex >= Previous.Num()
				|| (CurrentIndex < Current.Num()
					&& Current[CurrentIndex].Key.Hash
						< Previous[PreviousIndex].Key.Hash);
			if (bPreviousOnly)
			{
				AppendAllFunctionChanges(
					TConstArrayView<FFunctionEntry>(&Previous[PreviousIndex], 1),
					EAngelscriptCacheSemanticChangeDisposition::Removed,
					OutBodyChanges, OutDebugChanges);
				++PreviousIndex;
			}
			else if (bCurrentOnly)
			{
				AppendAllFunctionChanges(
					TConstArrayView<FFunctionEntry>(&Current[CurrentIndex], 1),
					EAngelscriptCacheSemanticChangeDisposition::Added,
					OutBodyChanges, OutDebugChanges);
				++CurrentIndex;
			}
			else
			{
				const FFunctionEntry& Before = Previous[PreviousIndex++];
				const FFunctionEntry& After = Current[CurrentIndex++];
				if (!(Before.BodyRecordId == After.BodyRecordId))
				{
					FAngelscriptCacheSemanticFunctionChange& Change =
						OutBodyChanges.AddDefaulted_GetRef();
					Change.FunctionKey = Before.Key;
					Change.Record = MakeRecordChange(
						&Before.BodyRecordId, &After.BodyRecordId);
				}
				const FAngelscriptCacheSemanticRecordChange DebugChange =
					MakeOptionalRecordChange(
						Before.DebugRecordId, After.DebugRecordId);
				if (DebugChange.Disposition
					!= EAngelscriptCacheSemanticChangeDisposition::Unchanged)
				{
					FAngelscriptCacheSemanticFunctionChange& Change =
						OutDebugChanges.AddDefaulted_GetRef();
					Change.FunctionKey = Before.Key;
					Change.Record = DebugChange;
				}
			}
		}
	}

	static void DiffRecordSets(
		const TConstArrayView<FAngelscriptCacheRecordId> Previous,
		const TConstArrayView<FAngelscriptCacheRecordId> Current,
		FAngelscriptCacheSemanticDiffResult& OutResult)
	{
		int32 PreviousIndex = 0;
		int32 CurrentIndex = 0;
		while (PreviousIndex < Previous.Num() || CurrentIndex < Current.Num())
		{
			if (CurrentIndex >= Current.Num()
				|| (PreviousIndex < Previous.Num()
					&& Previous[PreviousIndex] < Current[CurrentIndex]))
			{
				OutResult.RetiredRecordIds.Add(Previous[PreviousIndex++]);
			}
			else if (PreviousIndex >= Previous.Num()
				|| Current[CurrentIndex] < Previous[PreviousIndex])
			{
				OutResult.NewRecordIds.Add(Current[CurrentIndex++]);
			}
			else
			{
				OutResult.ReusedRecordIds.Add(Previous[PreviousIndex]);
				++PreviousIndex;
				++CurrentIndex;
			}
		}
	}

	static void AddWholeModule(
		const FModuleEntry& Module,
		const EAngelscriptCacheSemanticChangeDisposition Disposition,
		FAngelscriptCacheSemanticDiffResult& OutResult)
	{
		FAngelscriptCacheSemanticModuleDiff& Diff =
			OutResult.Modules.AddDefaulted_GetRef();
		Diff.ModuleKey = Module.Key;
		if (Disposition == EAngelscriptCacheSemanticChangeDisposition::Added)
		{
			Diff.ModuleSnapshot = MakeRecordChange(
				nullptr, &Module.SnapshotRecordId);
			Diff.ModuleInterface = MakeRecordChange(
				nullptr, &Module.InterfaceRecordId);
			Diff.ModuleState = MakeRecordChange(
				nullptr, &Module.StateRecordId);
		}
		else
		{
			Diff.ModuleSnapshot = MakeRecordChange(
				&Module.SnapshotRecordId, nullptr);
			Diff.ModuleInterface = MakeRecordChange(
				&Module.InterfaceRecordId, nullptr);
			Diff.ModuleState = MakeRecordChange(
				&Module.StateRecordId, nullptr);
		}
		AppendAllTypeChanges(Module.Types, Disposition, Diff.TypeSchemas);
		AppendAllFunctionChanges(Module.Functions, Disposition,
			Diff.FunctionBodies, Diff.DebugSidecars);
	}
}

bool FAngelscriptCacheSemanticModuleDiff::HasSemanticChanges() const
{
	return ModuleSnapshot.Disposition
			!= EAngelscriptCacheSemanticChangeDisposition::Unchanged
		|| ModuleInterface.Disposition
			!= EAngelscriptCacheSemanticChangeDisposition::Unchanged
		|| ModuleState.Disposition
			!= EAngelscriptCacheSemanticChangeDisposition::Unchanged
		|| !TypeSchemas.IsEmpty()
		|| !FunctionBodies.IsEmpty()
		|| !DebugSidecars.IsEmpty();
}

bool FAngelscriptCacheSemanticDiffResult::HasSemanticChanges() const
{
	if (!IsSuccess())
	{
		return false;
	}
	if (SourceIndex.Disposition
		!= EAngelscriptCacheSemanticChangeDisposition::Unchanged)
	{
		return true;
	}
	for (const FAngelscriptCacheSemanticModuleDiff& Module : Modules)
	{
		if (Module.HasSemanticChanges())
		{
			return true;
		}
	}
	return false;
}

FAngelscriptCacheSemanticDiffResult
DiffAngelscriptCacheValidatedGenerations(
	const FAngelscriptValidatedGeneration& Previous,
	const FAngelscriptValidatedGeneration& Current)
{
	using namespace AngelscriptCacheSemanticDiff_Private;

	FAngelscriptCacheSemanticDiffResult Result;
	if (!(Previous.Manifest.Compatibility.Hash
			== Current.Manifest.Compatibility.Hash)
		|| !(Previous.Manifest.Context.Hash == Current.Manifest.Context.Hash)
		|| !(Previous.Manifest.Profile.Hash == Current.Manifest.Profile.Hash))
	{
		Fail(Result,
			EAngelscriptCacheSemanticDiffError::IncompatibleGeneration,
			TEXT("Semantic generations use different Compatibility, Context or ArtifactProfile coordinates"));
		return Result;
	}

	FGenerationView PreviousView;
	FGenerationView CurrentView;
	if (!BuildGenerationView(Previous, PreviousView, Result)
		|| !BuildGenerationView(Current, CurrentView, Result))
	{
		Result.SourceIndex = {};
		Result.Modules.Reset();
		Result.ReusedRecordIds.Reset();
		Result.NewRecordIds.Reset();
		Result.RetiredRecordIds.Reset();
		return Result;
	}

	Result.SourceIndex = MakeRecordChange(
		&PreviousView.SourceIndexRecordId,
		&CurrentView.SourceIndexRecordId);
	DiffRecordSets(PreviousView.RecordIds, CurrentView.RecordIds, Result);

	int32 PreviousIndex = 0;
	int32 CurrentIndex = 0;
	while (PreviousIndex < PreviousView.Modules.Num()
		|| CurrentIndex < CurrentView.Modules.Num())
	{
		const bool bPreviousOnly =
			CurrentIndex >= CurrentView.Modules.Num()
			|| (PreviousIndex < PreviousView.Modules.Num()
				&& PreviousView.Modules[PreviousIndex].Key.Hash
					< CurrentView.Modules[CurrentIndex].Key.Hash);
		const bool bCurrentOnly =
			PreviousIndex >= PreviousView.Modules.Num()
			|| (CurrentIndex < CurrentView.Modules.Num()
				&& CurrentView.Modules[CurrentIndex].Key.Hash
					< PreviousView.Modules[PreviousIndex].Key.Hash);
		if (bPreviousOnly)
		{
			AddWholeModule(
				PreviousView.Modules[PreviousIndex++],
				EAngelscriptCacheSemanticChangeDisposition::Removed,
				Result);
		}
		else if (bCurrentOnly)
		{
			AddWholeModule(
				CurrentView.Modules[CurrentIndex++],
				EAngelscriptCacheSemanticChangeDisposition::Added,
				Result);
		}
		else
		{
			const FModuleEntry& Before =
				PreviousView.Modules[PreviousIndex++];
			const FModuleEntry& After =
				CurrentView.Modules[CurrentIndex++];
			FAngelscriptCacheSemanticModuleDiff& Diff =
				Result.Modules.AddDefaulted_GetRef();
			Diff.ModuleKey = Before.Key;
			Diff.ModuleSnapshot = MakeRecordChange(
				&Before.SnapshotRecordId, &After.SnapshotRecordId);
			Diff.ModuleInterface = MakeRecordChange(
				&Before.InterfaceRecordId, &After.InterfaceRecordId);
			Diff.ModuleState = MakeRecordChange(
				&Before.StateRecordId, &After.StateRecordId);
			DiffTypes(Before.Types, After.Types, Diff.TypeSchemas);
			DiffFunctions(Before.Functions, After.Functions,
				Diff.FunctionBodies, Diff.DebugSidecars);
		}
	}

	Result.Detail = FString::Printf(
		TEXT("Compared semantic RecordIds: Modules=%d Reused=%d New=%d Retired=%d"),
		Result.Modules.Num(),
		Result.ReusedRecordIds.Num(),
		Result.NewRecordIds.Num(),
		Result.RetiredRecordIds.Num());
	return Result;
}
