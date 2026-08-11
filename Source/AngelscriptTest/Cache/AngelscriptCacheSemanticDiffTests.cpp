#include "Cache/AngelscriptCacheCleanCapture.h"
#include "Cache/AngelscriptCacheSemanticDiff.h"

#include "CQTest.h"
#include "Shared/AngelscriptTestFixture.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptCacheSemanticDiffTests_Private
{
	static FAngelscriptCacheCleanCaptureOptions MakeCaptureOptions()
	{
		FAngelscriptCacheCleanCaptureOptions Options;
		FAngelscriptCompatibilityDescriptor Compatibility;
		Compatibility.CanonicalInputs = {
			TEXT("CacheV2SemanticDiffTest"),
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

	static FString MakeSource(
		const int32 EnumValue,
		const TCHAR* FunctionName,
		const int32 Answer,
		const bool bShiftDebugLines = false)
	{
		return FString::Printf(TEXT("%s") TEXT(R"AS(
enum ESemanticDiffState
{
	Ready = %d,
}

int %s()
{
	return %d;
}
)AS"),
			bShiftDebugLines ? TEXT("\n\n") : TEXT(""),
			EnumValue,
			FunctionName,
			Answer);
	}

	static bool Capture(
		FAutomationTestBase& Test,
		const FAngelscriptCacheCleanCaptureOptions& Options,
		const FString& Source,
		FAngelscriptCacheCleanModuleArtifacts& OutArtifacts)
	{
		FAngelscriptTestFixture Fixture(Test, ETestEngineMode::IsolatedFull);
		if (!Fixture.IsValid())
		{
			Test.AddError(TEXT("Failed to create semantic-diff isolated engine"));
			return false;
		}

		asIScriptModule* ScriptModule = Fixture.BuildModule(
			"ASCacheV2SemanticDiff", Source);
		if (ScriptModule == nullptr)
		{
			return false;
		}
		const TSharedPtr<FAngelscriptModuleDesc> Module =
			Fixture.GetEngine().GetModule(ScriptModule);
		if (!Module.IsValid())
		{
			Test.AddError(TEXT("Semantic-diff compile lost its module descriptor"));
			return false;
		}

		const FAngelscriptCacheCleanCaptureResult CaptureResult =
			CaptureAngelscriptCleanCompiledModule(
				Module.ToSharedRef(), Options, OutArtifacts);
		Test.AddInfo(FString::Printf(
			TEXT("Semantic-diff capture: Error=%u Records=%d Detail=%s"),
			static_cast<uint32>(CaptureResult.Error),
			OutArtifacts.Records.Num(),
			*CaptureResult.Detail));
		return CaptureResult.IsSuccess();
	}

	class FPreparedPackSource final : public IAngelscriptCachePackSource
	{
	public:
		explicit FPreparedPackSource(
			const TArray<FAngelscriptEncodedPack>& InPacks)
			: Packs(InPacks)
		{
		}

		virtual bool TryGetCompletePack(
			const FAngelscriptHash256& PackId,
			TConstArrayView<uint8>& OutBytes) override
		{
			OutBytes = {};
			for (const FAngelscriptEncodedPack& Pack : Packs)
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
		const TArray<FAngelscriptEncodedPack>& Packs;
	};

	static bool BuildValidatedGeneration(
		FAutomationTestBase& Test,
		const FAngelscriptCacheCleanModuleArtifacts& Artifacts,
		const FAngelscriptCacheCleanCaptureOptions& Options,
		const EAngelscriptCachePackCompressionPolicy Compression,
		FAngelscriptValidatedGeneration& OutGeneration,
		FAngelscriptHash256& OutGenerationId,
		const uint64 TargetRawBytesPerPack =
			FAngelscriptCachePackPolicy::DefaultTargetRawBytesPerPack)
	{
		FAngelscriptCachePackPolicy PackPolicy;
		PackPolicy.CompressionPolicy = Compression;
		PackPolicy.TargetRawBytesPerPack = TargetRawBytesPerPack;
		FAngelscriptUnrealZlibCacheStorageCodec Codec;
		FAngelscriptCachePreparedColdGeneration Prepared;
		const FAngelscriptCacheCleanCaptureResult PrepareResult =
			PrepareAngelscriptCacheColdGeneration(
				Artifacts, Options, PackPolicy, Codec, Prepared);
		if (!PrepareResult.IsSuccess())
		{
			Test.AddError(FString::Printf(
				TEXT("Semantic-diff generation preparation failed: %s"),
				*PrepareResult.Detail));
			return false;
		}

		FPreparedPackSource PackSource(Prepared.Packs);
		FAngelscriptCacheReadLimits Limits;
		FAngelscriptCacheReadBudget Budget;
		TOptional<FAngelscriptValidatedGeneration> Validated;
		const FAngelscriptCacheValidationResult Validation =
			ValidateAngelscriptCacheGeneration(
				Prepared.EncodedManifest.CompleteBytes,
				Prepared.EncodedManifest.ComputedGenerationId,
				PackSource,
				Limits,
				Budget,
				Codec,
				Validated);
		if (!Validation.IsSuccess() || !Validated.IsSet())
		{
			Test.AddError(FString::Printf(
				TEXT("Semantic-diff generation validation failed: Error=%u Stage=%u"),
				static_cast<uint32>(Validation.Error),
				static_cast<uint32>(Validation.Stage)));
			return false;
		}

		OutGenerationId = Prepared.EncodedManifest.ComputedGenerationId;
		OutGeneration = MoveTemp(Validated.GetValue());
		return true;
	}

	static bool CaptureAndValidate(
		FAutomationTestBase& Test,
		const FAngelscriptCacheCleanCaptureOptions& Options,
		const FString& Source,
		FAngelscriptValidatedGeneration& OutGeneration)
	{
		FAngelscriptCacheCleanModuleArtifacts Artifacts;
		if (!Capture(Test, Options, Source, Artifacts))
		{
			return false;
		}
		FAngelscriptHash256 IgnoredGenerationId;
		return BuildValidatedGeneration(
			Test,
			Artifacts,
			Options,
			EAngelscriptCachePackCompressionPolicy::ForceNoneForTest,
			OutGeneration,
			IgnoredGenerationId);
	}

	static const FAngelscriptCacheSemanticModuleDiff* GetSingleModule(
		FAutomationTestBase& Test,
		const FAngelscriptCacheSemanticDiffResult& Diff)
	{
		Test.AddInfo(FString::Printf(
			TEXT("Semantic RecordId diff: Error=%u Source=%u Modules=%d Reused=%d New=%d Retired=%d Detail=%s"),
			static_cast<uint32>(Diff.Error),
			static_cast<uint32>(Diff.SourceIndex.Disposition),
			Diff.Modules.Num(),
			Diff.ReusedRecordIds.Num(),
			Diff.NewRecordIds.Num(),
			Diff.RetiredRecordIds.Num(),
			*Diff.Detail));
		if (Diff.Modules.Num() != 1)
		{
			Test.AddError(FString::Printf(
				TEXT("Expected one semantic module diff, got %d"),
				Diff.Modules.Num()));
			return nullptr;
		}
		const FAngelscriptCacheSemanticModuleDiff& Module = Diff.Modules[0];
		Test.AddInfo(FString::Printf(
			TEXT("Semantic module diff: Snapshot=%u Interface=%u State=%u Types=%d Bodies=%d Debug=%d"),
			static_cast<uint32>(Module.ModuleSnapshot.Disposition),
			static_cast<uint32>(Module.ModuleInterface.Disposition),
			static_cast<uint32>(Module.ModuleState.Disposition),
			Module.TypeSchemas.Num(),
			Module.FunctionBodies.Num(),
			Module.DebugSidecars.Num()));
		return &Diff.Modules[0];
	}

	static int32 CountDisposition(
		const TConstArrayView<FAngelscriptCacheSemanticFunctionChange> Changes,
		const EAngelscriptCacheSemanticChangeDisposition Disposition)
	{
		int32 Count = 0;
		for (const FAngelscriptCacheSemanticFunctionChange& Change : Changes)
		{
			Count += Change.Record.Disposition == Disposition ? 1 : 0;
		}
		return Count;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheSemanticDiffTests,
	"Angelscript.TestModule.Cache.SemanticDiff",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(PhysicalPackDifferencesDoNotCreateSemanticChanges)
	{
		using namespace AngelscriptCacheSemanticDiffTests_Private;
		const FAngelscriptCacheCleanCaptureOptions Options = MakeCaptureOptions();
		FAngelscriptCacheCleanModuleArtifacts Artifacts;
		ASSERT_THAT(IsTrue(Capture(
			*TestRunner, Options, MakeSource(7, TEXT("Answer"), 42), Artifacts)));

		FAngelscriptValidatedGeneration NoneGeneration;
		FAngelscriptValidatedGeneration ShardedGeneration;
		FAngelscriptHash256 NoneGenerationId;
		FAngelscriptHash256 ShardedGenerationId;
		ASSERT_THAT(IsTrue(BuildValidatedGeneration(
			*TestRunner, Artifacts, Options,
			EAngelscriptCachePackCompressionPolicy::ForceNoneForTest,
			NoneGeneration, NoneGenerationId)));
		ASSERT_THAT(IsTrue(BuildValidatedGeneration(
			*TestRunner, Artifacts, Options,
			EAngelscriptCachePackCompressionPolicy::ForceNoneForTest,
			ShardedGeneration, ShardedGenerationId, 1)));
		ASSERT_THAT(IsFalse(NoneGenerationId == ShardedGenerationId,
			TEXT("Different physical Pack grouping must produce distinct generations")));

		const FAngelscriptCacheSemanticDiffResult Diff =
			DiffAngelscriptCacheValidatedGenerations(
				NoneGeneration, ShardedGeneration);
		ASSERT_THAT(IsTrue(Diff.IsSuccess(), *Diff.Detail));
		ASSERT_THAT(IsFalse(Diff.HasSemanticChanges()));
		ASSERT_THAT(AreEqual(7, Diff.ReusedRecordIds.Num()));
		ASSERT_THAT(AreEqual(0, Diff.NewRecordIds.Num()));
		ASSERT_THAT(AreEqual(0, Diff.RetiredRecordIds.Num()));
		const FAngelscriptCacheSemanticModuleDiff* Module =
			GetSingleModule(*TestRunner, Diff);
		ASSERT_THAT(IsNotNull(Module));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheSemanticChangeDisposition::Unchanged,
			Module->ModuleInterface.Disposition));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheSemanticChangeDisposition::Unchanged,
			Module->ModuleState.Disposition));
		ASSERT_THAT(AreEqual(0, Module->TypeSchemas.Num()));
		ASSERT_THAT(AreEqual(0, Module->FunctionBodies.Num()));
		ASSERT_THAT(AreEqual(0, Module->DebugSidecars.Num()));
	}

	TEST_METHOD(BodyOnlyEditKeepsInterfaceTypeStateAndDebugHits)
	{
		using namespace AngelscriptCacheSemanticDiffTests_Private;
		const FAngelscriptCacheCleanCaptureOptions Options = MakeCaptureOptions();
		FAngelscriptValidatedGeneration Before;
		FAngelscriptValidatedGeneration After;
		ASSERT_THAT(IsTrue(CaptureAndValidate(
			*TestRunner, Options, MakeSource(7, TEXT("Answer"), 41), Before)));
		ASSERT_THAT(IsTrue(CaptureAndValidate(
			*TestRunner, Options, MakeSource(7, TEXT("Answer"), 42), After)));

		const FAngelscriptCacheSemanticDiffResult Diff =
			DiffAngelscriptCacheValidatedGenerations(Before, After);
		ASSERT_THAT(IsTrue(Diff.IsSuccess(), *Diff.Detail));
		ASSERT_THAT(IsTrue(Diff.HasSemanticChanges()));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheSemanticChangeDisposition::Modified,
			Diff.SourceIndex.Disposition));
		const FAngelscriptCacheSemanticModuleDiff* Module =
			GetSingleModule(*TestRunner, Diff);
		ASSERT_THAT(IsNotNull(Module));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheSemanticChangeDisposition::Unchanged,
			Module->ModuleInterface.Disposition));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheSemanticChangeDisposition::Unchanged,
			Module->ModuleState.Disposition));
		ASSERT_THAT(AreEqual(0, Module->TypeSchemas.Num()));
		ASSERT_THAT(AreEqual(1, Module->FunctionBodies.Num()));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheSemanticChangeDisposition::Modified,
			Module->FunctionBodies[0].Record.Disposition));
		ASSERT_THAT(AreEqual(0, Module->DebugSidecars.Num()));
		ASSERT_THAT(AreEqual(4, Diff.ReusedRecordIds.Num()));
		ASSERT_THAT(AreEqual(3, Diff.NewRecordIds.Num()));
		ASSERT_THAT(AreEqual(3, Diff.RetiredRecordIds.Num()));
	}

	TEST_METHOD(EnumAuthorityEditIsOneTypeSchemaModification)
	{
		using namespace AngelscriptCacheSemanticDiffTests_Private;
		const FAngelscriptCacheCleanCaptureOptions Options = MakeCaptureOptions();
		FAngelscriptValidatedGeneration Before;
		FAngelscriptValidatedGeneration After;
		ASSERT_THAT(IsTrue(CaptureAndValidate(
			*TestRunner, Options, MakeSource(7, TEXT("Answer"), 42), Before)));
		ASSERT_THAT(IsTrue(CaptureAndValidate(
			*TestRunner, Options, MakeSource(8, TEXT("Answer"), 42), After)));

		const FAngelscriptCacheSemanticDiffResult Diff =
			DiffAngelscriptCacheValidatedGenerations(Before, After);
		ASSERT_THAT(IsTrue(Diff.IsSuccess(), *Diff.Detail));
		const FAngelscriptCacheSemanticModuleDiff* Module =
			GetSingleModule(*TestRunner, Diff);
		ASSERT_THAT(IsNotNull(Module));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheSemanticChangeDisposition::Unchanged,
			Module->ModuleInterface.Disposition));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheSemanticChangeDisposition::Unchanged,
			Module->ModuleState.Disposition));
		ASSERT_THAT(AreEqual(1, Module->TypeSchemas.Num()));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheSemanticChangeDisposition::Modified,
			Module->TypeSchemas[0].Record.Disposition));
		ASSERT_THAT(AreEqual(0, Module->FunctionBodies.Num()));
		ASSERT_THAT(AreEqual(0, Module->DebugSidecars.Num()));
	}

	TEST_METHOD(LineShiftClassifiesDebugSidecarIndependently)
	{
		using namespace AngelscriptCacheSemanticDiffTests_Private;
		const FAngelscriptCacheCleanCaptureOptions Options = MakeCaptureOptions();
		FAngelscriptValidatedGeneration Before;
		FAngelscriptValidatedGeneration After;
		ASSERT_THAT(IsTrue(CaptureAndValidate(
			*TestRunner, Options, MakeSource(7, TEXT("Answer"), 42, false), Before)));
		ASSERT_THAT(IsTrue(CaptureAndValidate(
			*TestRunner, Options, MakeSource(7, TEXT("Answer"), 42, true), After)));

		const FAngelscriptCacheSemanticDiffResult Diff =
			DiffAngelscriptCacheValidatedGenerations(Before, After);
		ASSERT_THAT(IsTrue(Diff.IsSuccess(), *Diff.Detail));
		const FAngelscriptCacheSemanticModuleDiff* Module =
			GetSingleModule(*TestRunner, Diff);
		ASSERT_THAT(IsNotNull(Module));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheSemanticChangeDisposition::Unchanged,
			Module->ModuleInterface.Disposition));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheSemanticChangeDisposition::Unchanged,
			Module->ModuleState.Disposition));
		ASSERT_THAT(AreEqual(0, Module->TypeSchemas.Num()));
		ASSERT_THAT(AreEqual(1, Module->FunctionBodies.Num()));
		ASSERT_THAT(AreEqual(1, Module->DebugSidecars.Num()));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheSemanticChangeDisposition::Modified,
			Module->DebugSidecars[0].Record.Disposition));
	}

	TEST_METHOD(FunctionRenameIsRemovalAndAdditionNotModification)
	{
		using namespace AngelscriptCacheSemanticDiffTests_Private;
		const FAngelscriptCacheCleanCaptureOptions Options = MakeCaptureOptions();
		FAngelscriptValidatedGeneration Before;
		FAngelscriptValidatedGeneration After;
		ASSERT_THAT(IsTrue(CaptureAndValidate(
			*TestRunner, Options, MakeSource(7, TEXT("Answer"), 42), Before)));
		ASSERT_THAT(IsTrue(CaptureAndValidate(
			*TestRunner, Options, MakeSource(7, TEXT("RenamedAnswer"), 42), After)));

		const FAngelscriptCacheSemanticDiffResult Diff =
			DiffAngelscriptCacheValidatedGenerations(Before, After);
		ASSERT_THAT(IsTrue(Diff.IsSuccess(), *Diff.Detail));
		const FAngelscriptCacheSemanticModuleDiff* Module =
			GetSingleModule(*TestRunner, Diff);
		ASSERT_THAT(IsNotNull(Module));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheSemanticChangeDisposition::Modified,
			Module->ModuleInterface.Disposition));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheSemanticChangeDisposition::Unchanged,
			Module->ModuleState.Disposition));
		ASSERT_THAT(AreEqual(2, Module->FunctionBodies.Num()));
		ASSERT_THAT(AreEqual(1, CountDisposition(
			Module->FunctionBodies,
			EAngelscriptCacheSemanticChangeDisposition::Removed)));
		ASSERT_THAT(AreEqual(1, CountDisposition(
			Module->FunctionBodies,
			EAngelscriptCacheSemanticChangeDisposition::Added)));
		ASSERT_THAT(AreEqual(0, CountDisposition(
			Module->FunctionBodies,
			EAngelscriptCacheSemanticChangeDisposition::Modified)));
		ASSERT_THAT(AreEqual(2, Module->DebugSidecars.Num()));
	}

	TEST_METHOD(IncompatibleProfileIsNotMisclassifiedAsSourceEdit)
	{
		using namespace AngelscriptCacheSemanticDiffTests_Private;
		const FAngelscriptCacheCleanCaptureOptions Options = MakeCaptureOptions();
		FAngelscriptValidatedGeneration Generation;
		ASSERT_THAT(IsTrue(CaptureAndValidate(
			*TestRunner, Options, MakeSource(7, TEXT("Answer"), 42), Generation)));
		FAngelscriptValidatedGeneration Incompatible = Generation;
		FBlake3Hash::ByteArray DifferentProfileBytes{};
		DifferentProfileBytes[0] = 1;
		Incompatible.Manifest.Profile.Hash.Value =
			FBlake3Hash(DifferentProfileBytes);

		const FAngelscriptCacheSemanticDiffResult Diff =
			DiffAngelscriptCacheValidatedGenerations(
				Generation, Incompatible);
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheSemanticDiffError::IncompatibleGeneration,
			Diff.Error));
		ASSERT_THAT(IsFalse(Diff.HasSemanticChanges()));
		ASSERT_THAT(AreEqual(0, Diff.Modules.Num()));
		ASSERT_THAT(AreEqual(0, Diff.ReusedRecordIds.Num()));
		ASSERT_THAT(AreEqual(0, Diff.NewRecordIds.Num()));
		ASSERT_THAT(AreEqual(0, Diff.RetiredRecordIds.Num()));
		TestRunner->AddInfo(FString::Printf(
			TEXT("Semantic incompatible rejection: Error=%u Detail=%s"),
			static_cast<uint32>(Diff.Error), *Diff.Detail));
	}

	TEST_METHOD(ForgedValidatedGenerationFailsClosedWithAtomicEmptyDiff)
	{
		using namespace AngelscriptCacheSemanticDiffTests_Private;
		const FAngelscriptCacheCleanCaptureOptions Options = MakeCaptureOptions();
		FAngelscriptValidatedGeneration Generation;
		ASSERT_THAT(IsTrue(CaptureAndValidate(
			*TestRunner, Options, MakeSource(7, TEXT("Answer"), 42), Generation)));
		FAngelscriptValidatedGeneration Forged = Generation;
		int32 SnapshotRecordIndex = INDEX_NONE;
		for (int32 Index = 0; Index < Forged.ReachableRecords.Num(); ++Index)
		{
			if (Forged.ReachableRecords[Index]->GetRecordId().Kind
				== EAngelscriptCacheRecordKind::ModuleSnapshot)
			{
				SnapshotRecordIndex = Index;
				break;
			}
		}
		ASSERT_THAT(IsTrue(SnapshotRecordIndex != INDEX_NONE));
		Forged.ReachableRecords.RemoveAt(SnapshotRecordIndex);

		const FAngelscriptCacheSemanticDiffResult Diff =
			DiffAngelscriptCacheValidatedGenerations(Generation, Forged);
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheSemanticDiffError::MissingRecord,
			Diff.Error));
		ASSERT_THAT(IsFalse(Diff.HasSemanticChanges()));
		ASSERT_THAT(AreEqual(0, Diff.Modules.Num()));
		ASSERT_THAT(AreEqual(0, Diff.ReusedRecordIds.Num()));
		ASSERT_THAT(AreEqual(0, Diff.NewRecordIds.Num()));
		ASSERT_THAT(AreEqual(0, Diff.RetiredRecordIds.Num()));
		ASSERT_THAT(IsFalse(Diff.SourceIndex.PreviousRecordId.IsSet()));
		ASSERT_THAT(IsFalse(Diff.SourceIndex.CurrentRecordId.IsSet()));
		TestRunner->AddInfo(FString::Printf(
			TEXT("Semantic forged-generation rejection: Error=%u Detail=%s"),
			static_cast<uint32>(Diff.Error), *Diff.Detail));
	}
};

#endif
