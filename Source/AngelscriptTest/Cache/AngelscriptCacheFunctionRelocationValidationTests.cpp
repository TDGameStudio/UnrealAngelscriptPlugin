#include "Cache/AngelscriptCacheArchive.h"
#include "Cache/AngelscriptCacheCleanCapture.h"
#include "Cache/AngelscriptCacheDecodedRecord.h"
#include "Cache/AngelscriptCacheRemainingRecordTypes.h"
#include "Core/Artifacts/AngelscriptArtifactIdentity.h"

#include "CQTest.h"
#include "Shared/AngelscriptTestFixture.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptCacheFunctionRelocationValidationTests_Private
{
	static FAngelscriptCacheCleanCaptureOptions MakeCaptureOptions()
	{
		FAngelscriptCacheCleanCaptureOptions Options;
		FAngelscriptCompatibilityDescriptor Compatibility;
		Compatibility.CanonicalInputs = {
			TEXT("CacheV2FunctionRelocationValidation"),
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

	static bool ReplaceRecord(
		FAngelscriptCacheCleanModuleArtifacts& Artifacts,
		const FAngelscriptCacheRecordId& OldRecordId,
		FAngelscriptPreparedRecord&& NewRecord)
	{
		for (FAngelscriptPreparedRecord& Record : Artifacts.Records)
		{
			if (Record.RecordId == OldRecordId)
			{
				Record = MoveTemp(NewRecord);
				return true;
			}
		}
		return false;
	}

	static bool BuildPreparedRecord(
		const EAngelscriptCacheRecordKind Kind,
		TArray<uint8>&& Payload,
		FAngelscriptPreparedRecord& OutRecord)
	{
		OutRecord = {};
		OutRecord.CanonicalPayload = MoveTemp(Payload);
		return FAngelscriptCacheRecordArchive::TryBuildRecordId(
			Kind, OutRecord.CanonicalPayload, OutRecord.RecordId).IsSuccess();
	}

	static bool RemoveCallerDeclaredDependencies(
		FAngelscriptCacheCleanModuleArtifacts& Artifacts)
	{
		FAngelscriptCacheReadLimits Limits;
		FAngelscriptCacheReadBudget Budget;
		FAngelscriptCachedFunctionBody CallerBody;
		FAngelscriptCacheRecordId CallerRecordId;
		FAngelscriptCachedModuleSnapshot Snapshot;
		FAngelscriptCacheRecordId SnapshotRecordId;
		bool bFoundCaller = false;
		bool bFoundSnapshot = false;

		for (const FAngelscriptPreparedRecord& Record : Artifacts.Records)
		{
			if (Record.RecordId.Kind != EAngelscriptCacheRecordKind::FunctionBody
				&& Record.RecordId.Kind
					!= EAngelscriptCacheRecordKind::ModuleSnapshot)
			{
				continue;
			}
			TOptional<FAngelscriptDecodedCacheRecordHandle> Decoded;
			if (!FAngelscriptDecodedCacheRecord::TryDecode(
				Record.RecordId,
				Record.CanonicalPayload,
				Limits,
				Budget,
				Decoded).IsSuccess()
				|| !Decoded.IsSet())
			{
				return false;
			}
			if (const FAngelscriptCachedFunctionBody* Body =
				Decoded.GetValue()->TryGetFunctionBody())
			{
				if (!Body->ActualDependencies.IsEmpty())
				{
					if (bFoundCaller)
					{
						return false;
					}
					CallerBody = *Body;
					CallerRecordId = Record.RecordId;
					bFoundCaller = true;
				}
			}
			else if (const FAngelscriptCachedModuleSnapshot* Value =
				Decoded.GetValue()->TryGetModuleSnapshot())
			{
				Snapshot = *Value;
				SnapshotRecordId = Record.RecordId;
				bFoundSnapshot = true;
			}
		}
		if (!bFoundCaller || !bFoundSnapshot
			|| CallerBody.ActualDependencies.Num() != 1)
		{
			return false;
		}

		CallerBody.ActualDependencies.Reset();
		FAngelscriptFunctionInputDescriptor Input;
		Input.SourceDigest = CallerBody.FunctionSourceDigest;
		CallerBody.FunctionInputDigest =
			FAngelscriptArtifactIdentityBuilder::BuildFunctionInputDigest(Input);
		TArray<uint8> CallerPayload;
		if (!FAngelscriptCacheRemainingRecordArchive::SerializeFunctionBody(
			CallerBody, CallerPayload).IsSuccess())
		{
			return false;
		}
		FAngelscriptPreparedRecord CallerRecord;
		if (!BuildPreparedRecord(EAngelscriptCacheRecordKind::FunctionBody,
			MoveTemp(CallerPayload), CallerRecord))
		{
			return false;
		}
		const FAngelscriptCacheRecordId NewCallerRecordId = CallerRecord.RecordId;
		if (!ReplaceRecord(
			Artifacts, CallerRecordId, MoveTemp(CallerRecord)))
		{
			return false;
		}

		FAngelscriptCachedFunctionBodyLink* CallerLink =
			Snapshot.FunctionBodies.FindByPredicate(
				[&CallerBody](const FAngelscriptCachedFunctionBodyLink& Link)
				{
					return Link.FunctionKey
						== CallerBody.Identity.FunctionKey;
				});
		if (CallerLink == nullptr)
		{
			return false;
		}
		CallerLink->RecordId = NewCallerRecordId;
		TArray<uint8> SnapshotPayload;
		if (!FAngelscriptCacheRemainingRecordArchive::SerializeModuleSnapshot(
			Snapshot, SnapshotPayload).IsSuccess())
		{
			return false;
		}
		FAngelscriptPreparedRecord SnapshotRecord;
		if (!BuildPreparedRecord(EAngelscriptCacheRecordKind::ModuleSnapshot,
			MoveTemp(SnapshotPayload), SnapshotRecord))
		{
			return false;
		}
		Artifacts.ModuleSnapshot.RecordId = SnapshotRecord.RecordId;
		return ReplaceRecord(
			Artifacts, SnapshotRecordId, MoveTemp(SnapshotRecord));
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheFunctionRelocationValidationTests,
	"Angelscript.TestModule.Cache.FunctionRelocationValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(RelocationWithoutDeclaredStableDependencyFailsGraphClosed)
	{
		using namespace
			AngelscriptCacheFunctionRelocationValidationTests_Private;
		FAngelscriptTestFixture Fixture(
			*TestRunner, ETestEngineMode::IsolatedFull);
		ASSERT_THAT(IsTrue(Fixture.IsValid()));
		const FString Source = TEXT(R"AS(
enum ERelocationState
{
	Ready = 7,
}

int Callee()
{
	return 41;
}

int Caller()
{
	return Callee() + 1;
}
)AS");
		asIScriptModule* ScriptModule = Fixture.BuildModule(
			"ASCacheV2FunctionRelocationValidation", Source);
		ASSERT_THAT(IsNotNull(ScriptModule));
		TSharedPtr<FAngelscriptModuleDesc> Module =
			Fixture.GetEngine().GetModule(ScriptModule);
		ASSERT_THAT(IsTrue(Module.IsValid()));

		const FAngelscriptCacheCleanCaptureOptions Options =
			MakeCaptureOptions();
		FAngelscriptCacheCleanModuleArtifacts Candidate;
		const FAngelscriptCacheCleanCaptureResult Capture =
			CaptureAngelscriptCleanCompiledModule(
				Module.ToSharedRef(), Options, Candidate);
		ASSERT_THAT(IsTrue(Capture.IsSuccess()));
		ASSERT_THAT(IsTrue(RemoveCallerDeclaredDependencies(Candidate)));

		FAngelscriptCacheCleanModuleArtifacts Output;
		const FAngelscriptCacheCleanCaptureResult Result =
			ValidateAndPromoteAngelscriptCleanCompiledModuleArtifacts(
				Module.ToSharedRef(), Options, MoveTemp(Candidate), Output);
		TestRunner->AddInfo(FString::Printf(
			TEXT("V5.4 relocation dependency rejection: Error=%u GraphRecords=%u OutputRecords=%d Detail=%s"),
			static_cast<uint32>(Result.Error),
			Result.ValidatedGraphRecordCount,
			Output.Records.Num(),
			*Result.Detail));

		ASSERT_THAT(AreEqual(
			EAngelscriptCacheCleanCaptureError::GraphValidationFailed,
			Result.Error));
		ASSERT_THAT(AreEqual(uint32(0), Result.ValidatedGraphRecordCount));
		ASSERT_THAT(IsTrue(Result.Detail.Contains(TEXT("Error=47"))));
		ASSERT_THAT(IsTrue(Output.Records.IsEmpty()));
		ASSERT_THAT(IsTrue(Output.ModuleKey.Hash.IsZero()));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
