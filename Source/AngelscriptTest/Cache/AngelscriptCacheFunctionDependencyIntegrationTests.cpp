#include "Cache/AngelscriptCacheCleanCapture.h"
#include "Cache/AngelscriptCacheCompilerBridge.h"
#include "Cache/AngelscriptCacheDecodedRecord.h"

#include "AngelscriptTestMacros.h"
#include "CQTest.h"
#include "Shared/AngelscriptTestFixture.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptCacheFunctionDependencyIntegrationTests_Private
{
	struct FCapturedAuthorities final
	{
		FAngelscriptCachedModuleInterface Interface;
		FAngelscriptCachedModuleState State;
		FAngelscriptCachedFunctionBody FunctionBody;
	};

	static FAngelscriptCacheCleanCaptureOptions MakeCaptureOptions()
	{
		FAngelscriptCacheCleanCaptureOptions Options;
		FAngelscriptCompatibilityDescriptor Compatibility;
		Compatibility.CanonicalInputs = {
			TEXT("CacheV2FunctionDependencyIntegrationTest"),
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
		Options.Context =
			FAngelscriptArtifactIdentityBuilder::BuildContextKey(Context);
		Options.Profile =
			FAngelscriptArtifactIdentityBuilder::BuildArtifactProfileKey(
				Options.Compatibility, Options.Context);
		Options.CanonicalCompileOptions = {
			TEXT("AutomaticImports=false"),
		};
		return Options;
	}

	static const FAngelscriptPreparedRecord* FindRecord(
		const FAngelscriptCacheCleanModuleArtifacts& Artifacts,
		const EAngelscriptCacheRecordKind Kind)
	{
		return Artifacts.Records.FindByPredicate(
			[Kind](const FAngelscriptPreparedRecord& Record)
			{
				return Record.RecordId.Kind == Kind;
			});
	}

	template <typename ValueType, typename AccessorType>
	static bool DecodeRecord(
		const FAngelscriptCacheCleanModuleArtifacts& Artifacts,
		const EAngelscriptCacheRecordKind Kind,
		AccessorType&& Accessor,
		ValueType& OutValue)
	{
		const FAngelscriptPreparedRecord* Record = FindRecord(Artifacts, Kind);
		if (Record == nullptr)
		{
			return false;
		}

		FAngelscriptCacheReadLimits Limits;
		FAngelscriptCacheReadBudget Budget;
		FAngelscriptDecodedCacheRecordBatch Batch(Budget, Limits);
		TOptional<FAngelscriptDecodedCacheRecordHandle> Decoded;
		if (!Batch.TryDecode(
			Record->RecordId, Record->CanonicalPayload, Decoded).IsSuccess()
			|| !Decoded.IsSet())
		{
			return false;
		}

		const ValueType* Value = Accessor(Decoded.GetValue().Get());
		if (Value == nullptr)
		{
			return false;
		}
		OutValue = *Value;
		return true;
	}

	static bool DecodeFunctionBodyByName(
		const FAngelscriptCacheCleanModuleArtifacts& Artifacts,
		const FAngelscriptCachedModuleInterface& Interface,
		const FStringView FunctionName,
		FAngelscriptCachedFunctionBody& OutBody)
	{
		const FAngelscriptCachedDeclaration* Declaration = nullptr;
		int32 DeclarationCount = 0;
		for (const FAngelscriptCachedDeclaration& Candidate
			: Interface.Declarations)
		{
			if (Candidate.DeclarationKind
					== EAngelscriptCacheDeclarationKind::Function
				&& Candidate.CanonicalName == FunctionName)
			{
				Declaration = &Candidate;
				++DeclarationCount;
			}
		}
		if (DeclarationCount != 1 || Declaration == nullptr)
		{
			return false;
		}

		FAngelscriptCacheReadLimits Limits;
		FAngelscriptCacheReadBudget Budget;
		FAngelscriptDecodedCacheRecordBatch Batch(Budget, Limits);
		int32 BodyCount = 0;
		for (const FAngelscriptPreparedRecord& Record : Artifacts.Records)
		{
			if (Record.RecordId.Kind
				!= EAngelscriptCacheRecordKind::FunctionBody)
			{
				continue;
			}
			TOptional<FAngelscriptDecodedCacheRecordHandle> Decoded;
			if (!Batch.TryDecode(
				Record.RecordId, Record.CanonicalPayload, Decoded).IsSuccess()
				|| !Decoded.IsSet())
			{
				return false;
			}
			const FAngelscriptCachedFunctionBody* Body =
				Decoded.GetValue()->TryGetFunctionBody();
			if (Body == nullptr)
			{
				return false;
			}
			if (Body->Identity.FunctionKey.Hash == Declaration->StableKey)
			{
				OutBody = *Body;
				++BodyCount;
			}
		}
		return BodyCount == 1;
	}

	static bool Capture(
		FAutomationTestBase& Test,
		const int32 ConstantValue,
		FCapturedAuthorities& OutCapture)
	{
		FAngelscriptTestFixture Fixture(Test, ETestEngineMode::IsolatedFull);
		if (!Fixture.IsValid())
		{
			Test.AddError(TEXT("Failed to create the function-dependency Engine"));
			return false;
		}

		const FString Source = FString::Printf(TEXT(R"AS(
			class FCachePayload
			{
				int Count;
			}

			const int GCacheAnswer = %d;

			int GetCacheAnswer()
			{
				return GCacheAnswer;
			}
			)AS"), ConstantValue);
		asIScriptModule* ScriptModule = Fixture.BuildModule(
			"ASCacheV2FunctionDependencyIntegration", Source);
		if (ScriptModule == nullptr)
		{
			return false;
		}
		const TSharedPtr<FAngelscriptModuleDesc> Module =
			Fixture.GetEngine().GetModule(ScriptModule);
		if (!Module.IsValid())
		{
			Test.AddError(TEXT("The function-dependency compile lost its module"));
			return false;
		}

		FAngelscriptCacheCleanModuleArtifacts Artifacts;
		const FAngelscriptCacheCleanCaptureResult Result =
			CaptureAngelscriptCleanCompiledModule(
				Module.ToSharedRef(), MakeCaptureOptions(), Artifacts);
		Test.AddInfo(FString::Printf(
			TEXT("Function dependency capture: Value=%d Error=%u Records=%d Graph=%u Detail=%s"),
			ConstantValue,
			static_cast<uint32>(Result.Error),
			Artifacts.Records.Num(),
			Result.ValidatedGraphRecordCount,
			*Result.Detail));
		if (!Result.IsSuccess())
		{
			return false;
		}

		return DecodeRecord(Artifacts,
			EAngelscriptCacheRecordKind::ModuleInterface,
			[](const FAngelscriptDecodedCacheRecord& Record)
			{
				return Record.TryGetModuleInterface();
			},
			OutCapture.Interface)
			&& DecodeRecord(Artifacts,
				EAngelscriptCacheRecordKind::ModuleState,
				[](const FAngelscriptDecodedCacheRecord& Record)
				{
					return Record.TryGetModuleState();
				},
				OutCapture.State)
			&& DecodeFunctionBodyByName(
				Artifacts,
				OutCapture.Interface,
				TEXT("GetCacheAnswer"),
				OutCapture.FunctionBody);
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheFunctionDependencyIntegrationTests,
	"Angelscript.TestModule.Cache.FunctionDependencyIntegration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(CleanCapturePersistsActualHardValueAndChangesOnlyFunctionInput)
	{
		using namespace AngelscriptCacheFunctionDependencyIntegrationTests_Private;
		FCapturedAuthorities Baseline;
		FCapturedAuthorities Changed;
		ASSERT_THAT(IsTrue(Capture(*TestRunner, 41, Baseline)));
		ASSERT_THAT(IsTrue(Capture(*TestRunner, 42, Changed)));

		ASSERT_THAT(AreEqual(1, Baseline.State.HardValues.Num()));
		ASSERT_THAT(AreEqual(1, Changed.State.HardValues.Num()));
		ASSERT_THAT(AreEqual(1, Baseline.FunctionBody.ActualDependencies.Num()));
		ASSERT_THAT(AreEqual(1, Changed.FunctionBody.ActualDependencies.Num()));

		const FAngelscriptCacheSemanticDependency& BaselineDependency =
			Baseline.FunctionBody.ActualDependencies[0];
		const FAngelscriptCacheSemanticDependency& ChangedDependency =
			Changed.FunctionBody.ActualDependencies[0];
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheSemanticDependencyKind::HardValue,
			BaselineDependency.Kind));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheReferenceKind::ScriptGlobal,
			BaselineDependency.Target.Kind));
		ASSERT_THAT(IsTrue(BaselineDependency.Target.StableKey
			== Baseline.State.HardValues[0].Owner.StableKey));
		ASSERT_THAT(IsTrue(BaselineDependency.Target.ExpectedAbi
			== Baseline.State.HardValues[0].Owner.ExpectedAbi));
		ASSERT_THAT(IsTrue(BaselineDependency.ExpectedContentOrValue.IsSet()));
		ASSERT_THAT(IsTrue(
			BaselineDependency.ExpectedContentOrValue.GetValue()
				== Baseline.State.HardValues[0].HardValueHash));

		ASSERT_THAT(IsTrue(BaselineDependency.Target.StableKey
			== ChangedDependency.Target.StableKey));
		ASSERT_THAT(IsFalse(
			BaselineDependency.ExpectedContentOrValue.GetValue()
				== ChangedDependency.ExpectedContentOrValue.GetValue()));
		ASSERT_THAT(IsTrue(Baseline.FunctionBody.FunctionSourceDigest.Hash
			== Changed.FunctionBody.FunctionSourceDigest.Hash));
		ASSERT_THAT(IsFalse(Baseline.FunctionBody.FunctionInputDigest.Hash
			== Changed.FunctionBody.FunctionInputDigest.Hash));

		FAngelscriptCacheFunctionInputAuthorities ChangedAuthorities;
		ChangedAuthorities.ModuleInterface = &Changed.Interface;
		ChangedAuthorities.ModuleState = &Changed.State;
		const FAngelscriptCacheFunctionInputResolution Resolution =
			FAngelscriptCacheCompilerBridge::ResolveCurrentFunctionInput(
				Baseline.FunctionBody,
				Changed.FunctionBody.FunctionSourceDigest,
				ChangedAuthorities);
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheFunctionInputStatus::ResolvedMismatch,
			Resolution.Status));
		ASSERT_THAT(IsTrue(Resolution.CurrentInputDigest.Hash
			== Changed.FunctionBody.FunctionInputDigest.Hash));

		TestRunner->AddInfo(FString::Printf(
			TEXT("Function dependency verified: Source=%s BaselineInput=%s ChangedInput=%s StableGlobal=%s"),
			*Baseline.FunctionBody.FunctionSourceDigest.Hash.ToHexString(),
			*Baseline.FunctionBody.FunctionInputDigest.Hash.ToHexString(),
			*Changed.FunctionBody.FunctionInputDigest.Hash.ToHexString(),
			*BaselineDependency.Target.StableKey.ToHexString()));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
