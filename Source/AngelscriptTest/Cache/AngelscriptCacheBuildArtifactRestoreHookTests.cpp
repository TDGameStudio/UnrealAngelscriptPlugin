#include "Cache/AngelscriptCacheCleanCapture.h"
#include "Cache/AngelscriptCacheCompilerBridge.h"

#include "CQTest.h"
#include "Shared/AngelscriptTestFixture.h"

#include "as_buildartifact.h"
#include "as_module.h"
#include "as_scriptfunction.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptCacheBuildArtifactRestoreHookTests_Private
{
	static constexpr const char* ModuleName =
		"ASCacheV2BuildArtifactRestoreHook";
	static constexpr const char* Source = R"AS(
enum ERestoreHookState
{
	Ready = 7,
}

int Answer()
{
	int Local = 40;
	return Local + 2;
}
)AS";

	enum class ECandidateMode : uint8
	{
		Valid,
		Miss,
		CorruptExecution,
		CorruptDebugWithMatchingHash,
		DebugLocalCountMismatch,
		DebugLocalPositionOutOfRange,
		DebugLocalMalformedUtf8,
		DebugTrailingData,
		StatusOnlyRestored,
	};

	struct FRestoreContext final
	{
		ECandidateMode Mode = ECandidateMode::Valid;
		FAngelscriptCachedFunctionBody Body;
		FAngelscriptCachedDebugSidecar Debug;
		FAngelscriptCacheReadLimits Limits;
		FAngelscriptCacheReadBudget Budget;
		FString Detail;
		int32 CallbackCount = 0;
		int32 ByteCodeWordsBefore = -1;
		int32 ByteCodeWordsAfter = -1;
		asEBuildArtifactRestoreResult LastResult =
			asBUILD_ARTIFACT_RESTORE_MISS;
	};

	struct FCompileObservation final
	{
		int32 CallbackCount = 0;
		int32 CompileResult = asERROR;
		asEBuildArtifactRestoreResult RestoreResult =
			asBUILD_ARTIFACT_RESTORE_MISS;
		bool bCompilerInvoked = false;
		bool bSucceeded = false;
	};

	static FAngelscriptCacheCleanCaptureOptions MakeCaptureOptions()
	{
		FAngelscriptCacheCleanCaptureOptions Options;
		FAngelscriptCompatibilityDescriptor Compatibility;
		Compatibility.CanonicalInputs = {
			TEXT("CacheV2BuildArtifactRestoreHook"),
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

	static bool DecodeCandidate(
		const FAngelscriptCacheCleanModuleArtifacts& Artifacts,
		FAngelscriptCachedFunctionBody& OutBody,
		FAngelscriptCachedDebugSidecar& OutDebug)
	{
		const FAngelscriptPreparedRecord* BodyRecord = FindRecord(
			Artifacts, EAngelscriptCacheRecordKind::FunctionBody);
		const FAngelscriptPreparedRecord* DebugRecord = FindRecord(
			Artifacts, EAngelscriptCacheRecordKind::DebugSidecar);
		if (BodyRecord == nullptr || DebugRecord == nullptr)
		{
			return false;
		}

		FAngelscriptCacheReadLimits Limits;
		FAngelscriptCacheReadBudget Budget;
		TOptional<FAngelscriptDecodedCacheRecordHandle> DecodedBody;
		TOptional<FAngelscriptDecodedCacheRecordHandle> DecodedDebug;
		if (!FAngelscriptDecodedCacheRecord::TryDecode(
				BodyRecord->RecordId,
				BodyRecord->CanonicalPayload,
				Limits,
				Budget,
				DecodedBody).IsSuccess()
			|| !DecodedBody.IsSet()
			|| !FAngelscriptDecodedCacheRecord::TryDecode(
				DebugRecord->RecordId,
				DebugRecord->CanonicalPayload,
				Limits,
				Budget,
				DecodedDebug).IsSuccess()
			|| !DecodedDebug.IsSet())
		{
			return false;
		}

		const FAngelscriptCachedFunctionBody* Body =
			DecodedBody.GetValue()->TryGetFunctionBody();
		const FAngelscriptCachedDebugSidecar* Debug =
			DecodedDebug.GetValue()->TryGetDebugSidecar();
		if (Body == nullptr || Debug == nullptr)
		{
			return false;
		}
		OutBody = *Body;
		OutDebug = *Debug;
		return true;
	}

	static bool ProduceCandidate(
		FAutomationTestBase& Test,
		FAngelscriptCachedFunctionBody& OutBody,
		FAngelscriptCachedDebugSidecar& OutDebug,
		int32& OutProducerFunctionId)
	{
		OutProducerFunctionId = -1;
		FAngelscriptCacheCleanModuleArtifacts Artifacts;
		{
			FAngelscriptTestFixture Producer(
				Test, ETestEngineMode::IsolatedFull);
			if (!Producer.IsValid())
			{
				return false;
			}
			asIScriptModule* ScriptModule = Producer.BuildModule(
				ModuleName, FString(ANSI_TO_TCHAR(Source)));
			if (ScriptModule == nullptr)
			{
				return false;
			}
			asIScriptFunction* Answer =
				ScriptModule->GetFunctionByDecl("int Answer()");
			if (Answer == nullptr)
			{
				return false;
			}
			OutProducerFunctionId = Answer->GetId();

			TSharedPtr<FAngelscriptModuleDesc> Module =
				Producer.GetEngine().GetModule(ScriptModule);
			if (!Module.IsValid())
			{
				return false;
			}
			const FAngelscriptCacheCleanCaptureResult Capture =
				CaptureAngelscriptCleanCompiledModule(
					Module.ToSharedRef(), MakeCaptureOptions(), Artifacts);
			Test.AddInfo(FString::Printf(
				TEXT("V5.3 producer capture: Error=%u Records=%d GraphRecords=%u Detail=%s"),
				static_cast<uint32>(Capture.Error),
				Artifacts.Records.Num(),
				Capture.ValidatedGraphRecordCount,
				*Capture.Detail));
			if (!Capture.IsSuccess())
			{
				return false;
			}
		}
		return DecodeCandidate(Artifacts, OutBody, OutDebug);
	}

	static asEBuildArtifactRestoreResult RestoreCandidate(
		const asSBuildArtifactInvocation* Invocation,
		asCScriptFunction* Function,
		void* UserData)
	{
		if (Invocation == nullptr || Function == nullptr || UserData == nullptr)
		{
			return asBUILD_ARTIFACT_RESTORE_REJECTED_CORRUPT;
		}
		FRestoreContext& Context = *static_cast<FRestoreContext*>(UserData);
		++Context.CallbackCount;
		Context.ByteCodeWordsBefore = Function->scriptData != nullptr
			? static_cast<int32>(Function->scriptData->byteCode.GetLength())
			: -1;
		if (Context.Mode == ECandidateMode::Miss)
		{
			Context.LastResult = asBUILD_ARTIFACT_RESTORE_MISS;
			Context.ByteCodeWordsAfter = Context.ByteCodeWordsBefore;
			return Context.LastResult;
		}
		if (Context.Mode == ECandidateMode::StatusOnlyRestored)
		{
			Context.LastResult = asBUILD_ARTIFACT_RESTORE_RESTORED;
			Context.ByteCodeWordsAfter = Context.ByteCodeWordsBefore;
			return Context.LastResult;
		}

		FAngelscriptCachedFunctionBody Candidate = Context.Body;
		FAngelscriptCachedDebugSidecar CandidateDebug = Context.Debug;
		if (Context.Mode == ECandidateMode::CorruptExecution
			&& !Candidate.CanonicalExecutionPayload.IsEmpty())
		{
			Candidate.CanonicalExecutionPayload[0] ^= 0xff;
		}
		const bool bMutateDebug =
			Context.Mode == ECandidateMode::CorruptDebugWithMatchingHash
			|| Context.Mode == ECandidateMode::DebugLocalCountMismatch
			|| Context.Mode == ECandidateMode::DebugLocalPositionOutOfRange
			|| Context.Mode == ECandidateMode::DebugLocalMalformedUtf8
			|| Context.Mode == ECandidateMode::DebugTrailingData;
		if (bMutateDebug && !CandidateDebug.CanonicalDebugPayload.IsEmpty())
		{
			TArray<uint8>& Payload = CandidateDebug.CanonicalDebugPayload;
			if (Context.Mode == ECandidateMode::CorruptDebugWithMatchingHash)
			{
				Payload[0] ^= 0xff;
			}
			else if (Context.Mode == ECandidateMode::DebugTrailingData)
			{
				Payload.Add(0xee);
			}
			else
			{
				// DebugSidecar v2 ends with the ordinal-aligned explicit-local
				// table. This fixture deliberately has exactly one named local:
				// [count=1][name length=5]["Local"][declared position].
				constexpr int32 LocalNameLength = 5;
				constexpr int32 LocalRowBytes =
					sizeof(uint32) * 3 + LocalNameLength;
				const int32 LocalCountOffset = Payload.Num() - LocalRowBytes;
				check(LocalCountOffset >= 0);
				auto ReadUInt32 = [&Payload](const int32 Offset)
				{
					return static_cast<uint32>(Payload[Offset])
						| static_cast<uint32>(Payload[Offset + 1]) << 8u
						| static_cast<uint32>(Payload[Offset + 2]) << 16u
						| static_cast<uint32>(Payload[Offset + 3]) << 24u;
				};
				auto WriteUInt32 = [&Payload](
					const int32 Offset, const uint32 Value)
				{
					Payload[Offset] = static_cast<uint8>(Value);
					Payload[Offset + 1] = static_cast<uint8>(Value >> 8u);
					Payload[Offset + 2] = static_cast<uint8>(Value >> 16u);
					Payload[Offset + 3] = static_cast<uint8>(Value >> 24u);
				};
				check(ReadUInt32(LocalCountOffset) == 1u);
				check(ReadUInt32(LocalCountOffset + sizeof(uint32))
					== LocalNameLength);
				const int32 LocalNameOffset =
					LocalCountOffset + sizeof(uint32) * 2;
				check(FMemory::Memcmp(
					Payload.GetData() + LocalNameOffset,
					"Local", LocalNameLength) == 0);
				const int32 LocalPositionOffset =
					LocalNameOffset + LocalNameLength;

				if (Context.Mode == ECandidateMode::DebugLocalCountMismatch)
				{
					WriteUInt32(LocalCountOffset, 0u);
					Payload.SetNum(LocalCountOffset + sizeof(uint32),
						EAllowShrinking::No);
				}
				else if (Context.Mode
					== ECandidateMode::DebugLocalPositionOutOfRange)
				{
					WriteUInt32(LocalPositionOffset, MAX_int32);
				}
				else
				{
					check(Context.Mode
						== ECandidateMode::DebugLocalMalformedUtf8);
					Payload[LocalNameOffset] = 0xc0;
				}
			}
			const FAngelscriptFunctionContentHash Recomputed =
				FAngelscriptArtifactIdentityBuilder::BuildFunctionContentHash(
					Candidate.CanonicalExecutionPayload,
					CandidateDebug.CanonicalDebugPayload);
			Candidate.Identity.Content = Recomputed;
			CandidateDebug.DebugHash = Recomputed.Debug;
		}
		Context.LastResult =
			FAngelscriptCacheCompilerBridge::TryRestoreFunctionArtifact(
				*Invocation,
				*Function,
				Candidate,
				CandidateDebug,
				Context.Limits,
				Context.Budget,
				Context.Detail);
		Context.ByteCodeWordsAfter = Function->scriptData != nullptr
			? static_cast<int32>(Function->scriptData->byteCode.GetLength())
			: -1;
		return Context.LastResult;
	}

	static void ObserveCompileResult(
		const asSBuildArtifactInvocation*,
		const asSBuildArtifactCompileResult* Result,
		void* UserData)
	{
		if (Result == nullptr || UserData == nullptr)
		{
			return;
		}
		FCompileObservation& Observation =
			*static_cast<FCompileObservation*>(UserData);
		++Observation.CallbackCount;
		Observation.CompileResult = Result->compileResult;
		Observation.RestoreResult = Result->restoreResult;
		Observation.bCompilerInvoked = Result->compilerInvoked;
		Observation.bSucceeded = Result->succeeded;
	}

	static bool BuildConsumer(
		FAutomationTestBase& Test,
		const FAngelscriptCachedFunctionBody& Body,
		const FAngelscriptCachedDebugSidecar& Debug,
		const ECandidateMode Mode,
		FRestoreContext& OutRestore,
		FCompileObservation& OutCompile,
		int32& OutFunctionId,
		int32& OutValue)
	{
		OutRestore.Mode = Mode;
		OutRestore.Body = Body;
		OutRestore.Debug = Debug;
		OutRestore.Detail.Reset();
		OutRestore.CallbackCount = 0;
		OutRestore.ByteCodeWordsBefore = -1;
		OutRestore.ByteCodeWordsAfter = -1;
		OutCompile = {};
		OutFunctionId = -1;
		OutValue = 0;

		FAngelscriptTestFixture Consumer(
			Test, ETestEngineMode::IsolatedFull);
		if (!Consumer.IsValid())
		{
			return false;
		}
		asIScriptEngine* Engine = Consumer.GetEngine().GetScriptEngine();
		asIScriptModule* Padding = Engine->GetModule(
			"ASCacheV2RestoreHookPadding", asGM_ALWAYS_CREATE);
		const char* PaddingSource = "int Padding() { return 1; }";
		if (Padding == nullptr
			|| Padding->AddScriptSection(
				"Padding.as", PaddingSource, FCStringAnsi::Strlen(PaddingSource), 0)
				< 0
			|| Padding->Build() < 0)
		{
			return false;
		}

		asCModule* Module = static_cast<asCModule*>(Engine->GetModule(
			ModuleName, asGM_ALWAYS_CREATE));
		if (Module == nullptr)
		{
			return false;
		}
		Module->SetBuildArtifactRestoreCallback(
			&RestoreCandidate, &OutRestore);
		Module->SetBuildArtifactCompileResultCallback(
			&ObserveCompileResult, &OutCompile);
		if (Module->AddScriptSection(
				"ASCacheV2BuildArtifactRestoreHook.as",
				Source,
				FCStringAnsi::Strlen(Source),
				0) < 0
			|| Module->Build() < 0)
		{
			return false;
		}

		asIScriptFunction* Answer = Module->GetFunctionByDecl("int Answer()");
		if (Answer == nullptr)
		{
			return false;
		}
		OutFunctionId = Answer->GetId();
		return Consumer.ExecuteInt(*Answer, OutValue);
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheBuildArtifactRestoreHookTests,
	"Angelscript.TestModule.Cache.BuildArtifactRestoreHook",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ValidCandidateRestoresBeforeCompilerAndKeepsCurrentFunctionId)
	{
		using namespace AngelscriptCacheBuildArtifactRestoreHookTests_Private;
		FAngelscriptCachedFunctionBody Body;
		FAngelscriptCachedDebugSidecar Debug;
		int32 ProducerFunctionId = -1;
		ASSERT_THAT(IsTrue(ProduceCandidate(
			*TestRunner, Body, Debug, ProducerFunctionId)));

		FRestoreContext Restore;
		FCompileObservation Compile;
		int32 ConsumerFunctionId = -1;
		int32 Value = 0;
		ASSERT_THAT(IsTrue(BuildConsumer(
			*TestRunner,
			Body,
			Debug,
			ECandidateMode::Valid,
			Restore,
			Compile,
			ConsumerFunctionId,
			Value)));

		TestRunner->AddInfo(FString::Printf(
			TEXT("V5.3 restored: HookCalls=%d CompilerCalls=%d Result=%u BeforeWords=%d AfterWords=%d ProducerId=%d ConsumerId=%d Value=%d Detail=%s"),
			Restore.CallbackCount,
			Compile.bCompilerInvoked ? 1 : 0,
			static_cast<uint32>(Compile.RestoreResult),
			Restore.ByteCodeWordsBefore,
			Restore.ByteCodeWordsAfter,
			ProducerFunctionId,
			ConsumerFunctionId,
			Value,
			*Restore.Detail));
		ASSERT_THAT(AreEqual(1, Restore.CallbackCount));
		ASSERT_THAT(AreEqual(1, Compile.CallbackCount));
		ASSERT_THAT(AreEqual(
			asBUILD_ARTIFACT_RESTORE_RESTORED, Compile.RestoreResult));
		ASSERT_THAT(IsFalse(Compile.bCompilerInvoked));
		ASSERT_THAT(IsTrue(Compile.bSucceeded));
		ASSERT_THAT(AreEqual(0, Restore.ByteCodeWordsBefore));
		ASSERT_THAT(IsTrue(Restore.ByteCodeWordsAfter > 0));
		ASSERT_THAT(IsTrue(ProducerFunctionId != ConsumerFunctionId));
		ASSERT_THAT(AreEqual(42, Value));
	}

	TEST_METHOD(MissAndCorruptCandidateCompileNormallyWithoutPartialMutation)
	{
		using namespace AngelscriptCacheBuildArtifactRestoreHookTests_Private;
		FAngelscriptCachedFunctionBody Body;
		FAngelscriptCachedDebugSidecar Debug;
		int32 ProducerFunctionId = -1;
		ASSERT_THAT(IsTrue(ProduceCandidate(
			*TestRunner, Body, Debug, ProducerFunctionId)));

		for (const ECandidateMode Mode : {
			ECandidateMode::Miss,
			ECandidateMode::CorruptExecution,
			ECandidateMode::CorruptDebugWithMatchingHash,
			ECandidateMode::DebugLocalCountMismatch,
			ECandidateMode::DebugLocalPositionOutOfRange,
			ECandidateMode::DebugLocalMalformedUtf8,
			ECandidateMode::DebugTrailingData,
			ECandidateMode::StatusOnlyRestored})
		{
			FRestoreContext Restore;
			FCompileObservation Compile;
			int32 FunctionId = -1;
			int32 Value = 0;
			ASSERT_THAT(IsTrue(BuildConsumer(
				*TestRunner,
				Body,
				Debug,
				Mode,
				Restore,
				Compile,
				FunctionId,
				Value)));
			const asEBuildArtifactRestoreResult Expected =
				Mode == ECandidateMode::Miss
				? asBUILD_ARTIFACT_RESTORE_MISS
				: asBUILD_ARTIFACT_RESTORE_REJECTED_CORRUPT;
			TestRunner->AddInfo(FString::Printf(
				TEXT("V5.3 fallback: Mode=%u HookCalls=%d CompilerCalls=%d Result=%u BeforeWords=%d AfterHookWords=%d FunctionId=%d Value=%d Detail=%s"),
				static_cast<uint32>(Mode),
				Restore.CallbackCount,
				Compile.bCompilerInvoked ? 1 : 0,
				static_cast<uint32>(Compile.RestoreResult),
				Restore.ByteCodeWordsBefore,
				Restore.ByteCodeWordsAfter,
				FunctionId,
				Value,
				*Restore.Detail));
			ASSERT_THAT(AreEqual(1, Restore.CallbackCount));
			ASSERT_THAT(AreEqual(Expected, Compile.RestoreResult));
			ASSERT_THAT(IsTrue(Compile.bCompilerInvoked));
			ASSERT_THAT(IsTrue(Compile.bSucceeded));
			ASSERT_THAT(AreEqual(0, Restore.ByteCodeWordsBefore));
			ASSERT_THAT(AreEqual(0, Restore.ByteCodeWordsAfter));
			ASSERT_THAT(AreEqual(42, Value));
		}
	}

	TEST_METHOD(UnstablePublicSingleIsNotCacheableWithoutLookup)
	{
		using namespace AngelscriptCacheBuildArtifactRestoreHookTests_Private;
		FAngelscriptTestFixture Fixture(
			*TestRunner, ETestEngineMode::IsolatedFull);
		ASSERT_THAT(IsTrue(Fixture.IsValid()));
		asCModule* Module = static_cast<asCModule*>(
			Fixture.GetEngine().GetScriptEngine()->GetModule(
				"ASCacheV2RestoreHookPublicSingle", asGM_ALWAYS_CREATE));
		ASSERT_THAT(IsNotNull(Module));

		FRestoreContext Restore;
		FCompileObservation Compile;
		Module->SetBuildArtifactRestoreCallback(&RestoreCandidate, &Restore);
		Module->SetBuildArtifactCompileResultCallback(
			&ObserveCompileResult, &Compile);
		asIScriptFunction* Function = nullptr;
		ASSERT_THAT(AreEqual(asSUCCESS, Module->CompileFunction(
			"DebugSnippet.as",
			"int DebugSnippet() { return 11; }",
			0,
			0,
			&Function)));
		ASSERT_THAT(IsNotNull(Function));
		Function->Release();

		TestRunner->AddInfo(FString::Printf(
			TEXT("V5.3 not-cacheable: HookCalls=%d CompilerCalls=%d Result=%u"),
			Restore.CallbackCount,
			Compile.bCompilerInvoked ? 1 : 0,
			static_cast<uint32>(Compile.RestoreResult)));
		ASSERT_THAT(AreEqual(0, Restore.CallbackCount));
		ASSERT_THAT(AreEqual(1, Compile.CallbackCount));
		ASSERT_THAT(AreEqual(
			asBUILD_ARTIFACT_RESTORE_NOT_CACHEABLE,
			Compile.RestoreResult));
		ASSERT_THAT(IsTrue(Compile.bCompilerInvoked));
		ASSERT_THAT(IsTrue(Compile.bSucceeded));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
