#include "Cache/AngelscriptCacheCleanCapture.h"
#include "Cache/AngelscriptCacheDecodedRecord.h"
#include "Cache/AngelscriptCacheIncrementalGeneration.h"
#include "Cache/AngelscriptCacheSemanticDiff.h"
#include "Cache/AngelscriptCacheStore.h"

#include "CQTest.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Shared/AngelscriptTestFixture.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptCacheIncrementalGenerationTests_Private
{
	struct FSemanticCoordinates final
	{
		FAngelscriptStableModuleKey ModuleKey;
		FAngelscriptStableTypeKey TypeKey;
		FAngelscriptStableGlobalKey GlobalKey;
		FAngelscriptStableFunctionKey FunctionKey;
		FAngelscriptHash256 DeclarationAbi;
		FAngelscriptHash256 FunctionSource;
		FAngelscriptHash256 FunctionInput;
		FAngelscriptHash256 Execution;
		FAngelscriptHash256 Debug;
	};

	struct FCandidate final
	{
		FAngelscriptCacheCleanModuleArtifacts Artifacts;
		FAngelscriptCachePreparedColdGeneration Prepared;
		FAngelscriptValidatedGeneration Validated;
		FSemanticCoordinates Coordinates;
	};

	class FPreparedPackSource final : public IAngelscriptCachePackSource
	{
	public:
		FPreparedPackSource(
			const TConstArrayView<FAngelscriptEncodedPack> InOldPacks,
			const TConstArrayView<FAngelscriptEncodedPack> InNewPacks)
			: OldPacks(InOldPacks)
			, NewPacks(InNewPacks)
		{
		}

		virtual bool TryGetCompletePack(
			const FAngelscriptHash256& PackId,
			TConstArrayView<uint8>& OutBytes) override
		{
			OutBytes = {};
			for (const FAngelscriptEncodedPack& Pack : NewPacks)
			{
				if (Pack.PackId == PackId)
				{
					OutBytes = Pack.Bytes;
					return true;
				}
			}
			for (const FAngelscriptEncodedPack& Pack : OldPacks)
			{
				if (Pack.PackId == PackId)
				{
					OutBytes = Pack.Bytes;
					return true;
				}
			}
			return false;
		}

	private:
		TConstArrayView<FAngelscriptEncodedPack> OldPacks;
		TConstArrayView<FAngelscriptEncodedPack> NewPacks;
	};

	class FScopedDiskRoot final
	{
	public:
		FScopedDiskRoot()
		{
			Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(
				FPaths::ProjectSavedDir(),
				TEXT("Automation/AngelscriptCacheIncrementalGeneration"),
				FGuid::NewGuid().ToString(EGuidFormats::Digits)));
			FPaths::NormalizeDirectoryName(Root);
			check(Root.Contains(
				TEXT("/Saved/Automation/AngelscriptCacheIncrementalGeneration/")));
			check(IFileManager::Get().MakeDirectory(*Root, true));
		}

		~FScopedDiskRoot()
		{
			if (Root.Contains(
				TEXT("/Saved/Automation/AngelscriptCacheIncrementalGeneration/")))
			{
				IFileManager::Get().DeleteDirectory(*Root, false, true);
			}
		}

		FString Root;
	};

	static FAngelscriptCacheCleanCaptureOptions MakeCaptureOptions()
	{
		FAngelscriptCacheCleanCaptureOptions Options;
		FAngelscriptCompatibilityDescriptor Compatibility;
		Compatibility.CanonicalInputs = {
			TEXT("CacheV2IncrementalGenerationTest"),
			TEXT("VmExecutionCodec=2"),
		};
		Options.Compatibility =
			FAngelscriptArtifactIdentityBuilder::BuildCompatibilityKey(
				Compatibility);

		FAngelscriptContextDescriptor Context;
		Context.CanonicalInputs = {
			TEXT("SourceMount=Game"),
			TEXT("DebugSidecar=Enabled"),
		};
		Options.Context = FAngelscriptArtifactIdentityBuilder::BuildContextKey(
			Context);
		Options.Profile =
			FAngelscriptArtifactIdentityBuilder::BuildArtifactProfileKey(
				Options.Compatibility, Options.Context);
		Options.CanonicalCompileOptions = {
			TEXT("AutomaticImports=false"),
		};
		return Options;
	}

	static FString MakeSource(const int32 Answer)
	{
		return FString::Printf(TEXT(R"AS(
class FIncrementalPayload
{
	int Count;
}

const int GIncrementalAnswer = 41;

int GetIncrementalAnswer()
{
	return %d;
}
)AS"), Answer);
	}

	static bool LocationsEqual(
		const FAngelscriptCachePackLocation& Left,
		const FAngelscriptCachePackLocation& Right)
	{
		return Left.PackId == Right.PackId
			&& Left.PackOffset == Right.PackOffset
			&& Left.StoredSize == Right.StoredSize
			&& Left.RawSize == Right.RawSize
			&& Left.Codec == Right.Codec
			&& Left.RawChecksum == Right.RawChecksum;
	}

	static const FAngelscriptCacheRecordIndexEntry* FindEntry(
		const FAngelscriptCacheGenerationManifest& Manifest,
		const FAngelscriptCacheRecordId& RecordId)
	{
		return Manifest.Records.FindByPredicate(
			[&](const FAngelscriptCacheRecordIndexEntry& Entry)
			{
				return Entry.RecordId == RecordId;
			});
	}

	static bool ContainsRecordId(
		const TConstArrayView<FAngelscriptCacheRecordId> Values,
		const FAngelscriptCacheRecordId& RecordId)
	{
		for (const FAngelscriptCacheRecordId& Value : Values)
		{
			if (Value == RecordId)
			{
				return true;
			}
		}
		return false;
	}

	static const TCHAR* RecordKindName(
		const EAngelscriptCacheRecordKind Kind)
	{
		switch (Kind)
		{
		case EAngelscriptCacheRecordKind::SourceIndex:
			return TEXT("SourceIndex");
		case EAngelscriptCacheRecordKind::ModuleInterface:
			return TEXT("ModuleInterface");
		case EAngelscriptCacheRecordKind::TypeSchema:
			return TEXT("TypeSchema");
		case EAngelscriptCacheRecordKind::ModuleState:
			return TEXT("ModuleState");
		case EAngelscriptCacheRecordKind::FunctionBody:
			return TEXT("FunctionBody");
		case EAngelscriptCacheRecordKind::DebugSidecar:
			return TEXT("DebugSidecar");
		case EAngelscriptCacheRecordKind::ModuleSnapshot:
			return TEXT("ModuleSnapshot");
		default:
			return TEXT("Invalid");
		}
	}

	static void LogRecordIds(
		FAutomationTestBase& Test,
		const TCHAR* Disposition,
		const TConstArrayView<FAngelscriptCacheRecordId> RecordIds)
	{
		for (int32 Index = 0; Index < RecordIds.Num(); ++Index)
		{
			const FAngelscriptCacheRecordId& RecordId = RecordIds[Index];
			Test.AddInfo(FString::Printf(
				TEXT("Incremental %s[%d]: Kind=%s(%u) RecordId=%s"),
				Disposition,
				Index,
				RecordKindName(RecordId.Kind),
				static_cast<uint32>(RecordId.Kind),
				*RecordId.ContentHash.ToHexString()));
		}
	}

	static bool ObserveSemanticCoordinates(
		FAutomationTestBase& Test,
		FCandidate& Candidate)
	{
		Candidate.Coordinates = {};
		Candidate.Coordinates.ModuleKey = Candidate.Artifacts.ModuleKey;
		const FAngelscriptCachedModuleInterface* Interface = nullptr;
		int32 InterfaceCount = 0;
		int32 TypeCount = 0;
		int32 StateCount = 0;
		int32 FunctionCount = 0;
		int32 DebugCount = 0;
		for (const FAngelscriptDecodedCacheRecordHandle& Handle :
			Candidate.Validated.ReachableRecords)
		{
			const FAngelscriptDecodedCacheRecord& Record = Handle.Get();
			if (const FAngelscriptCachedModuleInterface* Value =
				Record.TryGetModuleInterface())
			{
				++InterfaceCount;
				Interface = Value;
			}
			if (const FAngelscriptCachedTypeSchema* Type =
				Record.TryGetTypeSchema())
			{
				++TypeCount;
				Candidate.Coordinates.TypeKey = Type->TypeKey;
			}
			if (const FAngelscriptCachedModuleState* State =
				Record.TryGetModuleState())
			{
				++StateCount;
				if (State->OrderedGlobals.Num() == 1)
				{
					Candidate.Coordinates.GlobalKey =
						State->OrderedGlobals[0].GlobalKey;
				}
			}
			if (const FAngelscriptCachedFunctionBody* Function =
				Record.TryGetFunctionBody())
			{
				++FunctionCount;
			}
			if (const FAngelscriptCachedDebugSidecar* Debug =
				Record.TryGetDebugSidecar())
			{
				++DebugCount;
			}
		}

		const FAngelscriptCachedDeclaration* TargetDeclaration = nullptr;
		int32 TargetDeclarationCount = 0;
		if (Interface != nullptr)
		{
			for (const FAngelscriptCachedDeclaration& Declaration
				: Interface->Declarations)
			{
				if (Declaration.DeclarationKind
						== EAngelscriptCacheDeclarationKind::Function
					&& Declaration.CanonicalName
						== TEXT("GetIncrementalAnswer"))
				{
					TargetDeclaration = &Declaration;
					++TargetDeclarationCount;
				}
			}
		}
		int32 TargetFunctionCount = 0;
		int32 TargetDebugCount = 0;
		if (TargetDeclaration != nullptr)
		{
			for (const FAngelscriptDecodedCacheRecordHandle& Handle
				: Candidate.Validated.ReachableRecords)
			{
				const FAngelscriptDecodedCacheRecord& Record = Handle.Get();
				if (const FAngelscriptCachedFunctionBody* Function =
					Record.TryGetFunctionBody())
				{
					if (Function->Identity.FunctionKey.Hash
						== TargetDeclaration->StableKey)
					{
						++TargetFunctionCount;
						Candidate.Coordinates.FunctionKey =
							Function->Identity.FunctionKey;
						Candidate.Coordinates.DeclarationAbi =
							Function->ExpectedDeclarationAbi;
						Candidate.Coordinates.FunctionSource =
							Function->FunctionSourceDigest.Hash;
						Candidate.Coordinates.FunctionInput =
							Function->FunctionInputDigest.Hash;
						Candidate.Coordinates.Execution =
							Function->Identity.Content.Execution;
					}
				}
				if (const FAngelscriptCachedDebugSidecar* Debug =
					Record.TryGetDebugSidecar())
				{
					if (Debug->FunctionKey.Hash == TargetDeclaration->StableKey)
					{
						++TargetDebugCount;
						Candidate.Coordinates.Debug = Debug->DebugHash;
					}
				}
			}
		}

		if (InterfaceCount != 1 || TypeCount != 1 || StateCount != 1
			|| FunctionCount < 1 || DebugCount != FunctionCount
			|| TargetDeclarationCount != 1 || TargetFunctionCount != 1
			|| TargetDebugCount != 1
			|| Candidate.Coordinates.ModuleKey.Hash.IsZero()
			|| Candidate.Coordinates.TypeKey.Hash.IsZero()
			|| Candidate.Coordinates.GlobalKey.Hash.IsZero()
			|| Candidate.Coordinates.FunctionKey.Hash.IsZero()
			|| Candidate.Coordinates.DeclarationAbi.IsZero()
			|| Candidate.Coordinates.FunctionSource.IsZero()
			|| Candidate.Coordinates.FunctionInput.IsZero()
			|| Candidate.Coordinates.Execution.IsZero()
			|| Candidate.Coordinates.Debug.IsZero())
		{
			Test.AddError(FString::Printf(
				TEXT("Production incremental candidate lacks exact semantic-coordinate coverage: Interface=%d Type=%d State=%d Function=%d Debug=%d TargetDeclaration=%d TargetFunction=%d TargetDebug=%d"),
				InterfaceCount, TypeCount, StateCount, FunctionCount, DebugCount,
				TargetDeclarationCount, TargetFunctionCount, TargetDebugCount));
			return false;
		}
		return true;
	}

	static bool BuildCandidate(
		FAutomationTestBase& Test,
		const FAngelscriptCacheCleanCaptureOptions& Options,
		const int32 Answer,
		const uint64 TargetRawBytesPerPack,
		FCandidate& OutCandidate)
	{
		OutCandidate = {};
		FAngelscriptTestFixture Fixture(Test, ETestEngineMode::IsolatedFull);
		if (!Fixture.IsValid())
		{
			Test.AddError(TEXT("Failed to create incremental-generation isolated engine"));
			return false;
		}

		asIScriptModule* ScriptModule = Fixture.BuildModule(
			"ASCacheV2IncrementalGeneration", MakeSource(Answer));
		if (ScriptModule == nullptr)
		{
			return false;
		}
		const TSharedPtr<FAngelscriptModuleDesc> Module =
			Fixture.GetEngine().GetModule(ScriptModule);
		if (!Module.IsValid())
		{
			Test.AddError(TEXT("Incremental-generation compile lost its module descriptor"));
			return false;
		}

		const FAngelscriptCacheCleanCaptureResult Capture =
			CaptureAngelscriptCleanCompiledModule(
				Module.ToSharedRef(), Options, OutCandidate.Artifacts);
		if (!Capture.IsSuccess())
		{
			Test.AddError(FString::Printf(
				TEXT("Incremental-generation capture failed: Error=%u Detail=%s"),
				static_cast<uint32>(Capture.Error), *Capture.Detail));
			return false;
		}

		FAngelscriptCachePackPolicy PackPolicy;
		PackPolicy.TargetRawBytesPerPack = TargetRawBytesPerPack;
		PackPolicy.CompressionPolicy =
			EAngelscriptCachePackCompressionPolicy::ForceNoneForTest;
		FAngelscriptUnrealZlibCacheStorageCodec Codec;
		const FAngelscriptCacheCleanCaptureResult Prepare =
			PrepareAngelscriptCacheColdGeneration(
				OutCandidate.Artifacts,
				Options,
				PackPolicy,
				Codec,
				OutCandidate.Prepared);
		if (!Prepare.IsSuccess())
		{
			Test.AddError(FString::Printf(
				TEXT("Incremental-generation full candidate preparation failed: Error=%u Detail=%s"),
				static_cast<uint32>(Prepare.Error), *Prepare.Detail));
			return false;
		}

		FPreparedPackSource Packs(OutCandidate.Prepared.Packs, {});
		FAngelscriptCacheReadBudget Budget;
		TOptional<FAngelscriptValidatedGeneration> Validated;
		const FAngelscriptCacheValidationResult Validation =
			ValidateAngelscriptCacheGeneration(
				OutCandidate.Prepared.EncodedManifest.CompleteBytes,
				OutCandidate.Prepared.EncodedManifest.ComputedGenerationId,
				Packs,
				FAngelscriptCacheReadLimits{},
				Budget,
				Codec,
				Validated);
		if (!Validation.IsSuccess() || !Validated.IsSet())
		{
			Test.AddError(FString::Printf(
				TEXT("Incremental-generation full candidate validation failed: Error=%u Stage=%u"),
				static_cast<uint32>(Validation.Error),
				static_cast<uint32>(Validation.Stage)));
			return false;
		}

		OutCandidate.Validated = MoveTemp(Validated.GetValue());
		if (!ObserveSemanticCoordinates(Test, OutCandidate))
		{
			return false;
		}
		Test.AddInfo(FString::Printf(
			TEXT("Full clean candidate: Answer=%d Records=%d Packs=%d Generation=%s"),
			Answer,
			OutCandidate.Validated.ReachableRecords.Num(),
			OutCandidate.Prepared.Packs.Num(),
			*OutCandidate.Prepared.EncodedManifest.ComputedGenerationId.ToHexString()));
		Test.AddInfo(FString::Printf(
			TEXT("Full clean owners: Module=%s Type=%s Global=%s Function=%s"),
			*OutCandidate.Coordinates.ModuleKey.Hash.ToHexString(),
			*OutCandidate.Coordinates.TypeKey.Hash.ToHexString(),
			*OutCandidate.Coordinates.GlobalKey.Hash.ToHexString(),
			*OutCandidate.Coordinates.FunctionKey.Hash.ToHexString()));
		Test.AddInfo(FString::Printf(
			TEXT("Full clean function coordinates: Abi=%s Source=%s Input=%s Execution=%s Debug=%s"),
			*OutCandidate.Coordinates.DeclarationAbi.ToHexString(),
			*OutCandidate.Coordinates.FunctionSource.ToHexString(),
			*OutCandidate.Coordinates.FunctionInput.ToHexString(),
			*OutCandidate.Coordinates.Execution.ToHexString(),
			*OutCandidate.Coordinates.Debug.ToHexString()));
		return true;
	}

	static bool PrepareIncremental(
		FAutomationTestBase& Test,
		const FAngelscriptValidatedGeneration& Base,
		const FAngelscriptValidatedGeneration& Current,
		FAngelscriptCachePreparedIncrementalGeneration& OutPrepared,
		FAngelscriptCacheIncrementalPreparationResult& OutResult)
	{
		FAngelscriptCachePackPolicy PackPolicy;
		PackPolicy.CompressionPolicy =
			EAngelscriptCachePackCompressionPolicy::ForceNoneForTest;
		FAngelscriptUnrealZlibCacheStorageCodec Codec;
		OutResult = PrepareAngelscriptCacheIncrementalGeneration(
			Base,
			Current,
			PackPolicy,
			FAngelscriptCacheReadLimits{},
			Codec,
			OutPrepared);
		Test.AddInfo(FString::Printf(
			TEXT("Incremental prepare: Error=%u DiffError=%u Reused=%d New=%d Retired=%d NewPacks=%d ManifestRecords=%d Generation=%s Detail=%s"),
			static_cast<uint32>(OutResult.Error),
			static_cast<uint32>(OutResult.SemanticDiffError),
			OutPrepared.SemanticDiff.ReusedRecordIds.Num(),
			OutPrepared.SemanticDiff.NewRecordIds.Num(),
			OutPrepared.SemanticDiff.RetiredRecordIds.Num(),
			OutPrepared.NewPacks.Num(),
			OutPrepared.Manifest.Records.Num(),
			*OutPrepared.EncodedManifest.ComputedGenerationId.ToHexString(),
			*OutResult.Detail));
		if (OutResult.IsSuccess())
		{
			LogRecordIds(Test, TEXT("reused"),
				OutPrepared.SemanticDiff.ReusedRecordIds);
			LogRecordIds(Test, TEXT("new"),
				OutPrepared.SemanticDiff.NewRecordIds);
			LogRecordIds(Test, TEXT("retired"),
				OutPrepared.SemanticDiff.RetiredRecordIds);
		}
		return OutResult.IsSuccess();
	}

	static bool ValidateMixedGeneration(
		FAutomationTestBase& Test,
		const FCandidate& Base,
		const FAngelscriptCachePreparedIncrementalGeneration& Incremental,
		FAngelscriptValidatedGeneration& OutValidated)
	{
		FPreparedPackSource Packs(Base.Prepared.Packs, Incremental.NewPacks);
		FAngelscriptCacheReadBudget Budget;
		FAngelscriptUnrealZlibCacheStorageCodec Codec;
		TOptional<FAngelscriptValidatedGeneration> Validated;
		const FAngelscriptCacheValidationResult Result =
			ValidateAngelscriptCacheGeneration(
				Incremental.EncodedManifest.CompleteBytes,
				Incremental.EncodedManifest.ComputedGenerationId,
				Packs,
				FAngelscriptCacheReadLimits{},
				Budget,
				Codec,
				Validated);
		Test.AddInfo(FString::Printf(
			TEXT("Mixed generation validation: Error=%u Stage=%u Decoded=%d"),
			static_cast<uint32>(Result.Error),
			static_cast<uint32>(Result.Stage),
			Validated.IsSet() ? Validated->ReachableRecords.Num() : 0));
		if (!Result.IsSuccess() || !Validated.IsSet())
		{
			return false;
		}
		OutValidated = MoveTemp(Validated.GetValue());
		return true;
	}

	static FAngelscriptCacheReadSelection MakeSelection(
		const FAngelscriptCacheGenerationManifest& Manifest)
	{
		FAngelscriptCacheReadSelection Selection;
		Selection.Compatibility = Manifest.Compatibility;
		Selection.Context = Manifest.Context;
		Selection.Profile = Manifest.Profile;
		Selection.SourceSnapshot = Manifest.SourceSnapshot;
		Selection.bAllowPendingColdStart = false;
		return Selection;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheIncrementalGenerationTests,
	"Angelscript.TestModule.Cache.IncrementalGeneration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(BodyOnlyEditWritesOnlyNewRecordsAndRetainsOldLocations)
	{
		using namespace AngelscriptCacheIncrementalGenerationTests_Private;
		const FAngelscriptCacheCleanCaptureOptions Options = MakeCaptureOptions();
		FCandidate Base;
		FCandidate Current;
		ASSERT_THAT(IsTrue(BuildCandidate(
			*TestRunner, Options, 41,
			FAngelscriptCachePackPolicy::DefaultTargetRawBytesPerPack, Base)));
		ASSERT_THAT(IsTrue(BuildCandidate(
			*TestRunner, Options, 42,
			FAngelscriptCachePackPolicy::DefaultTargetRawBytesPerPack, Current)));

		FAngelscriptCachePreparedIncrementalGeneration Incremental;
		FAngelscriptCacheIncrementalPreparationResult Result;
		ASSERT_THAT(IsTrue(PrepareIncremental(
			*TestRunner, Base.Validated, Current.Validated, Incremental, Result)));
		const int32 CompleteRecordCount =
			Current.Validated.ReachableRecords.Num();
		ASSERT_THAT(AreEqual(
			CompleteRecordCount - 3,
			Incremental.SemanticDiff.ReusedRecordIds.Num()));
		ASSERT_THAT(AreEqual(3, Incremental.SemanticDiff.NewRecordIds.Num()));
		ASSERT_THAT(AreEqual(3, Incremental.SemanticDiff.RetiredRecordIds.Num()));
		ASSERT_THAT(AreEqual(
			CompleteRecordCount, Incremental.Manifest.Records.Num()));
		ASSERT_THAT(IsTrue(Base.Coordinates.ModuleKey
			== Current.Coordinates.ModuleKey));
		ASSERT_THAT(IsTrue(Base.Coordinates.TypeKey
			== Current.Coordinates.TypeKey));
		ASSERT_THAT(IsTrue(Base.Coordinates.GlobalKey
			== Current.Coordinates.GlobalKey));
		ASSERT_THAT(IsTrue(Base.Coordinates.FunctionKey
			== Current.Coordinates.FunctionKey));
		ASSERT_THAT(IsTrue(Base.Coordinates.DeclarationAbi
			== Current.Coordinates.DeclarationAbi));
		ASSERT_THAT(IsFalse(Base.Coordinates.FunctionSource
			== Current.Coordinates.FunctionSource));
		ASSERT_THAT(IsFalse(Base.Coordinates.FunctionInput
			== Current.Coordinates.FunctionInput));
		ASSERT_THAT(IsFalse(Base.Coordinates.Execution
			== Current.Coordinates.Execution));
		ASSERT_THAT(IsTrue(Base.Coordinates.Debug
			== Current.Coordinates.Debug));

		int32 NewPackRecordCount = 0;
		for (const FAngelscriptEncodedPack& Pack : Incremental.NewPacks)
		{
			for (const FAngelscriptCachePackIndexEntry& Entry : Pack.Index)
			{
				++NewPackRecordCount;
				ASSERT_THAT(IsTrue(ContainsRecordId(
					Incremental.SemanticDiff.NewRecordIds, Entry.RecordId)));
			}
		}
		ASSERT_THAT(AreEqual(3, NewPackRecordCount));

		for (const FAngelscriptCacheRecordId& Reused :
			Incremental.SemanticDiff.ReusedRecordIds)
		{
			const FAngelscriptCacheRecordIndexEntry* OldEntry =
				FindEntry(Base.Prepared.Manifest, Reused);
			const FAngelscriptCacheRecordIndexEntry* NewEntry =
				FindEntry(Incremental.Manifest, Reused);
			ASSERT_THAT(IsNotNull(OldEntry));
			ASSERT_THAT(IsNotNull(NewEntry));
			ASSERT_THAT(IsTrue(LocationsEqual(
				OldEntry->Location, NewEntry->Location)));
		}
		for (const FAngelscriptCacheRecordId& Retired :
			Incremental.SemanticDiff.RetiredRecordIds)
		{
			ASSERT_THAT(IsNull(FindEntry(Incremental.Manifest, Retired)));
		}

		FAngelscriptValidatedGeneration Mixed;
		ASSERT_THAT(IsTrue(ValidateMixedGeneration(
			*TestRunner, Base, Incremental, Mixed)));
		const FAngelscriptCacheSemanticDiffResult Equivalent =
			DiffAngelscriptCacheValidatedGenerations(Current.Validated, Mixed);
		ASSERT_THAT(IsTrue(Equivalent.IsSuccess()));
		ASSERT_THAT(IsFalse(Equivalent.HasSemanticChanges()));
		ASSERT_THAT(AreEqual(
			CompleteRecordCount, Equivalent.ReusedRecordIds.Num()));
		ASSERT_THAT(IsFalse(Incremental.EncodedManifest.ComputedGenerationId
			== Current.Prepared.EncodedManifest.ComputedGenerationId));
		ASSERT_THAT(IsFalse(Incremental.EncodedManifest.ComputedGenerationId
			== Base.Prepared.EncodedManifest.ComputedGenerationId));
	}

	TEST_METHOD(UnchangedCandidateWithDifferentShardingIsWriteFree)
	{
		using namespace AngelscriptCacheIncrementalGenerationTests_Private;
		const FAngelscriptCacheCleanCaptureOptions Options = MakeCaptureOptions();
		FCandidate Base;
		FCandidate Regrouped;
		ASSERT_THAT(IsTrue(BuildCandidate(
			*TestRunner, Options, 41,
			FAngelscriptCachePackPolicy::DefaultTargetRawBytesPerPack, Base)));
		ASSERT_THAT(IsTrue(BuildCandidate(
			*TestRunner, Options, 41, 1, Regrouped)));
		ASSERT_THAT(IsFalse(Base.Prepared.EncodedManifest.ComputedGenerationId
			== Regrouped.Prepared.EncodedManifest.ComputedGenerationId));

		FAngelscriptCachePreparedIncrementalGeneration Incremental;
		FAngelscriptCacheIncrementalPreparationResult Result;
		ASSERT_THAT(IsTrue(PrepareIncremental(
			*TestRunner, Base.Validated, Regrouped.Validated, Incremental, Result)));
		ASSERT_THAT(AreEqual(
			Base.Validated.ReachableRecords.Num(),
			Incremental.SemanticDiff.ReusedRecordIds.Num()));
		ASSERT_THAT(AreEqual(0, Incremental.SemanticDiff.NewRecordIds.Num()));
		ASSERT_THAT(AreEqual(0, Incremental.SemanticDiff.RetiredRecordIds.Num()));
		ASSERT_THAT(AreEqual(0, Incremental.NewPacks.Num()));
		ASSERT_THAT(IsTrue(Incremental.EncodedManifest.ComputedGenerationId
			== Base.Prepared.EncodedManifest.ComputedGenerationId));
		ASSERT_THAT(AreEqual(
			Base.Prepared.EncodedManifest.CompleteBytes,
			Incremental.EncodedManifest.CompleteBytes));
	}

	TEST_METHOD(RealStorePublishesMixedGenerationAndKeepsPinnedBaseReadable)
	{
		using namespace AngelscriptCacheIncrementalGenerationTests_Private;
		const FAngelscriptCacheCleanCaptureOptions Options = MakeCaptureOptions();
		FCandidate Base;
		FCandidate Current;
		ASSERT_THAT(IsTrue(BuildCandidate(
			*TestRunner, Options, 41,
			FAngelscriptCachePackPolicy::DefaultTargetRawBytesPerPack, Base)));
		ASSERT_THAT(IsTrue(BuildCandidate(
			*TestRunner, Options, 42,
			FAngelscriptCachePackPolicy::DefaultTargetRawBytesPerPack, Current)));

		FScopedDiskRoot Disk;
		TUniquePtr<IAngelscriptCacheAtomicFileOps> FileOps =
			CreateAngelscriptCacheAtomicFileOps();
		TUniquePtr<IAngelscriptCacheNamespaceLockOps> LockOps =
			CreateAngelscriptCacheNamespaceLockOps();
		ASSERT_THAT(IsNotNull(FileOps.Get()));
		ASSERT_THAT(IsNotNull(LockOps.Get()));

		FAngelscriptCacheStorePaths Paths;
		ASSERT_THAT(IsTrue(BuildAngelscriptCacheStorePaths(
			Disk.Root / TEXT("CacheV2"),
			Options.Compatibility,
			Options.Context,
			*FileOps,
			Paths).IsSuccess()));
		const TOptional<FAngelscriptCacheWriterToken> BaseToken =
			FAngelscriptCacheWriterToken::TryParse(
				TEXT("8401-00112233445566778899aabbccddeeff"));
		const TOptional<FAngelscriptCacheWriterToken> IncrementalToken =
			FAngelscriptCacheWriterToken::TryParse(
				TEXT("8402-fedcba9876543210fedcba9876543210"));
		ASSERT_THAT(IsTrue(BaseToken.IsSet()));
		ASSERT_THAT(IsTrue(IncrementalToken.IsSet()));

		FAngelscriptCacheReadLimits Limits;
		FAngelscriptUnrealZlibCacheStorageCodec Codec;
		FAngelscriptCacheReadBudget BasePublicationBudget;
		const FAngelscriptCacheStoreResult BasePublication =
			PublishAngelscriptCacheGeneration(
				Paths,
				EAngelscriptCachePublicationDisposition::Current,
				TOptional<FAngelscriptHash256>{},
				Base.Prepared.Packs,
				Base.Prepared.Manifest,
				Base.Prepared.EncodedManifest,
				BaseToken.GetValue(),
				Limits,
				BasePublicationBudget,
				FPlatformTime::Seconds() + 5.0,
				[]() { return false; },
				Codec,
				*LockOps,
				*FileOps);
		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreError::None, BasePublication.Error));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreCommitState::CurrentCommitted,
			BasePublication.CommitState));

		TArray<TArray<uint8>> BasePackBytesBefore;
		for (const FAngelscriptEncodedPack& Pack : Base.Prepared.Packs)
		{
			TArray<uint8>& Bytes = BasePackBytesBefore.AddDefaulted_GetRef();
			ASSERT_THAT(IsTrue(FileOps->ReopenReadAll(
				Paths.BuildPackPath(Pack.PackId),
				Limits.MaxPackBytes,
				Bytes).IsSuccess()));
		}

		TUniquePtr<FAngelscriptCacheReadSession> PinnedBase;
		const FAngelscriptCacheStoreResult BaseOpen =
			OpenBestAngelscriptCacheReadSession(
				Paths,
				MakeSelection(Base.Prepared.Manifest),
				Limits,
				FPlatformTime::Seconds() + 5.0,
				[]() { return false; },
				Codec,
				*LockOps,
				*FileOps,
				PinnedBase);
		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreError::None, BaseOpen.Error));
		ASSERT_THAT(IsNotNull(PinnedBase.Get()));

		FAngelscriptCachePreparedIncrementalGeneration Incremental;
		FAngelscriptCacheIncrementalPreparationResult PrepareResult;
		ASSERT_THAT(IsTrue(PrepareIncremental(
			*TestRunner,
			PinnedBase->GetGeneration(),
			Current.Validated,
			Incremental,
			PrepareResult)));

		FAngelscriptCacheReadBudget IncrementalPublicationBudget;
		const FAngelscriptCacheStoreResult IncrementalPublication =
			PublishAngelscriptCacheGeneration(
				Paths,
				EAngelscriptCachePublicationDisposition::Current,
				PinnedBase->GetGenerationId(),
				Incremental.NewPacks,
				Incremental.Manifest,
				Incremental.EncodedManifest,
				IncrementalToken.GetValue(),
				Limits,
				IncrementalPublicationBudget,
				FPlatformTime::Seconds() + 5.0,
				[]() { return false; },
				Codec,
				*LockOps,
				*FileOps);
		TestRunner->AddInfo(FString::Printf(
			TEXT("Incremental Store publication: Error=%u Stage=%u Commit=%u Before=%s After=%s"),
			static_cast<uint32>(IncrementalPublication.Error),
			static_cast<uint32>(IncrementalPublication.Stage),
			static_cast<uint32>(IncrementalPublication.CommitState),
			IncrementalPublication.GenerationBefore.IsSet()
				? *IncrementalPublication.GenerationBefore->ToHexString()
				: TEXT("none"),
			IncrementalPublication.GenerationAfter.IsSet()
				? *IncrementalPublication.GenerationAfter->ToHexString()
				: TEXT("none")));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreError::None, IncrementalPublication.Error));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheStoreCommitState::CurrentCommitted,
			IncrementalPublication.CommitState));

		TUniquePtr<FAngelscriptCacheReadSession> ReopenedCurrent;
		const FAngelscriptCacheStoreResult CurrentOpen =
			OpenBestAngelscriptCacheReadSession(
				Paths,
				MakeSelection(Incremental.Manifest),
				Limits,
				FPlatformTime::Seconds() + 5.0,
				[]() { return false; },
				Codec,
				*LockOps,
				*FileOps,
				ReopenedCurrent);
		ASSERT_THAT(AreEqual(EAngelscriptCacheStoreError::None, CurrentOpen.Error));
		ASSERT_THAT(IsNotNull(ReopenedCurrent.Get()));
		ASSERT_THAT(IsTrue(ReopenedCurrent->GetGenerationId()
			== Incremental.EncodedManifest.ComputedGenerationId));
		const FAngelscriptCacheSemanticDiffResult CurrentEquivalent =
			DiffAngelscriptCacheValidatedGenerations(
				Current.Validated, ReopenedCurrent->GetGeneration());
		ASSERT_THAT(IsTrue(CurrentEquivalent.IsSuccess()));
		ASSERT_THAT(IsFalse(CurrentEquivalent.HasSemanticChanges()));

		ASSERT_THAT(IsTrue(PinnedBase->GetGenerationId()
			== Base.Prepared.EncodedManifest.ComputedGenerationId));
		ASSERT_THAT(AreEqual(
			Base.Validated.ReachableRecords.Num(),
			PinnedBase->GetGeneration().ReachableRecords.Num()));
		for (int32 Index = 0; Index < Base.Prepared.Packs.Num(); ++Index)
		{
			TArray<uint8> BytesAfter;
			ASSERT_THAT(IsTrue(FileOps->ReopenReadAll(
				Paths.BuildPackPath(Base.Prepared.Packs[Index].PackId),
				Limits.MaxPackBytes,
				BytesAfter).IsSuccess()));
			ASSERT_THAT(AreEqual(BasePackBytesBefore[Index], BytesAfter));
		}
	}

	TEST_METHOD(ForgedBaseManifestFailsWithAtomicEmptyOutput)
	{
		using namespace AngelscriptCacheIncrementalGenerationTests_Private;
		const FAngelscriptCacheCleanCaptureOptions Options = MakeCaptureOptions();
		FCandidate Base;
		ASSERT_THAT(IsTrue(BuildCandidate(
			*TestRunner, Options, 41,
			FAngelscriptCachePackPolicy::DefaultTargetRawBytesPerPack, Base)));

		FAngelscriptValidatedGeneration Forged = Base.Validated;
		ASSERT_THAT(IsTrue(!Forged.Manifest.Records.IsEmpty()));
		Forged.Manifest.Records[0].Location.PackId = {};
		FAngelscriptCachePreparedIncrementalGeneration Incremental;
		Incremental.NewPacks.AddDefaulted();
		Incremental.Manifest.Records.AddDefaulted();
		Incremental.EncodedManifest.CompleteBytes.Add(0xff);
		FAngelscriptCacheIncrementalPreparationResult Result;
		ASSERT_THAT(IsFalse(PrepareIncremental(
			*TestRunner, Forged, Base.Validated, Incremental, Result)));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheIncrementalPreparationError::InvalidBaseManifest,
			Result.Error));
		ASSERT_THAT(AreEqual(0, Incremental.NewPacks.Num()));
		ASSERT_THAT(AreEqual(0, Incremental.Manifest.Records.Num()));
		ASSERT_THAT(AreEqual(0, Incremental.EncodedManifest.CompleteBytes.Num()));
		ASSERT_THAT(AreEqual(0, Incremental.SemanticDiff.ReusedRecordIds.Num()));
		ASSERT_THAT(AreEqual(0, Incremental.SemanticDiff.NewRecordIds.Num()));
		ASSERT_THAT(AreEqual(0, Incremental.SemanticDiff.RetiredRecordIds.Num()));
	}
};

#endif
