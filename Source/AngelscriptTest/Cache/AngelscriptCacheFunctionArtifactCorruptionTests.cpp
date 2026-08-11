#include "Cache/AngelscriptCacheCleanCapture.h"
#include "Cache/AngelscriptCacheCompilerBridge.h"
#include "Cache/AngelscriptCacheStableSymbolIdentity.h"

#include "CQTest.h"
#include "Shared/AngelscriptTestFixture.h"

#include "as_buildartifact.h"
#include "as_module.h"
#include "as_restore.h"
#include "as_scriptfunction.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptCacheFunctionArtifactCorruptionTests_Private
{
	static constexpr const char* ModuleName =
		"ASCacheV2FunctionArtifactCorruption";
	static constexpr const char* Source = R"AS(
class FArtifactCorruptionLeaf
{
	int Value = 40;
}

int Answer()
{
	FArtifactCorruptionLeaf Leaf = FArtifactCorruptionLeaf();
	return Leaf.Value + 2;
}
)AS";

	enum class EExecutionMutation : uint8
	{
		UnsupportedVersion,
		UnknownRootTrait,
		InvalidHeapPrefix,
		ImpossibleObjectCount,
		UnknownObjectType,
		InvalidObjectPosition,
		TruncatedRuntimeState,
		TrailingData,
	};

	struct FAttempt final
	{
		EExecutionMutation Mutation = EExecutionMutation::UnsupportedVersion;
		asEBuildArtifactRestoreResult Result =
			asBUILD_ARTIFACT_RESTORE_MISS;
		int32 BeforeWords = -1;
		int32 AfterWords = -1;
		FString Detail;
	};

	struct FRestoreContext final
	{
		FAngelscriptCachedFunctionBody Body;
		FAngelscriptCachedDebugSidecar Debug;
		TArray<uint8> RawVmPayload;
		asSFunctionArtifactValidationDiagnostics BaselineDiagnostics{};
		TArray<FAttempt> Attempts;
		int32 AnswerCallbackCount = 0;
		bool bBaselineValidated = false;
	};

	struct FCompileObservation final
	{
		int32 CallbackCount = 0;
		asEBuildArtifactRestoreResult RestoreResult =
			asBUILD_ARTIFACT_RESTORE_MISS;
		bool bCompilerInvoked = false;
		bool bSucceeded = false;
	};

	class FArtifactReadStream final : public asIBinaryStream
	{
	public:
		explicit FArtifactReadStream(const TConstArrayView<uint8> InBytes)
			: Bytes(InBytes)
		{
		}

		virtual int Read(void* Data, const asUINT Size) override
		{
			if ((Data == nullptr && Size != 0)
				|| Offset > Bytes.Num()
				|| Size > static_cast<asUINT>(Bytes.Num() - Offset))
			{
				return asERROR;
			}
			if (Size != 0)
			{
				FMemory::Memcpy(Data, Bytes.GetData() + Offset, Size);
				Offset += static_cast<int32>(Size);
			}
			return asSUCCESS;
		}

		virtual int Write(const void*, const asUINT) override
		{
			return asNOT_SUPPORTED;
		}

	private:
		TConstArrayView<uint8> Bytes;
		int32 Offset = 0;
	};

	class FArtifactWriteStream final : public asIBinaryStream
	{
	public:
		virtual int Read(void*, const asUINT) override
		{
			return asNOT_SUPPORTED;
		}

		virtual int Write(const void* Data, const asUINT Size) override
		{
			if ((Data == nullptr && Size != 0)
				|| Size > static_cast<asUINT>(MAX_int32 - Bytes.Num()))
			{
				return asOUT_OF_MEMORY;
			}
			if (Size != 0)
			{
				const int32 Offset = Bytes.AddUninitialized(
					static_cast<int32>(Size));
				FMemory::Memcpy(Bytes.GetData() + Offset, Data, Size);
			}
			return asSUCCESS;
		}

		TArray<uint8> Bytes;
	};

	static FAngelscriptCacheCleanCaptureOptions MakeCaptureOptions()
	{
		FAngelscriptCacheCleanCaptureOptions Options;
		FAngelscriptCompatibilityDescriptor Compatibility;
		Compatibility.CanonicalInputs = {
			TEXT("CacheV2FunctionArtifactCorruption"),
			TEXT("VmExecutionCodec=5"),
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

	static bool DecodeRecord(
		const FAngelscriptPreparedRecord& Record,
		TOptional<FAngelscriptDecodedCacheRecordHandle>& OutRecord)
	{
		FAngelscriptCacheReadLimits Limits;
		FAngelscriptCacheReadBudget Budget;
		return FAngelscriptDecodedCacheRecord::TryDecode(
			Record.RecordId,
			Record.CanonicalPayload,
			Limits,
			Budget,
			OutRecord).IsSuccess()
			&& OutRecord.IsSet();
	}

	static bool ProduceAnswerCandidate(
		FAutomationTestBase& Test,
		FAngelscriptCachedFunctionBody& OutBody,
		FAngelscriptCachedDebugSidecar& OutDebug,
		TArray<uint8>& OutRawVmPayload)
	{
		FAngelscriptCacheCleanModuleArtifacts Artifacts;
		FAngelscriptStableFunctionKey AnswerKey;
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
			asCScriptFunction* Answer = static_cast<asCScriptFunction*>(
				ScriptModule->GetFunctionByDecl("int Answer()"));
			TSharedPtr<FAngelscriptModuleDesc> Module =
				Producer.GetEngine().GetModule(ScriptModule);
			if (Answer == nullptr || !Module.IsValid())
			{
				return false;
			}
			FArtifactWriteStream RawStream;
			asCWriter RawWriter(
				static_cast<asCModule*>(ScriptModule),
				&RawStream,
				Answer->engine,
				true);
			if (RawWriter.WriteFunctionArtifact(Answer) < 0
				|| RawStream.Bytes.IsEmpty())
			{
				return false;
			}
			OutRawVmPayload = MoveTemp(RawStream.Bytes);
			if (Answer->scriptData != nullptr)
			{
				for (asUINT DependencyIndex = 0;
					DependencyIndex
						< Answer->scriptData->artifactDependencies.GetLength();
					++DependencyIndex)
				{
					const asSBuildArtifactDependency& Dependency =
						Answer->scriptData->artifactDependencies[DependencyIndex];
					const asCScriptFunction* DependencyFunction =
						Dependency.function;
					const asCTypeInfo* DependencyOwner =
						DependencyFunction != nullptr
							? DependencyFunction->artifactOwnerType : nullptr;
					const asCTypeInfo* DependencyReturn =
						DependencyFunction != nullptr
							? DependencyFunction->returnType.GetTypeInfo() : nullptr;
					const asCTypeInfo* DependencyObject =
						DependencyFunction != nullptr
							? DependencyFunction->objectType : nullptr;
					Test.AddInfo(FString::Printf(
						TEXT("VM corruption dependency[%u]: Kind=%u Ref=%u Function=%s Generated=%d Invocation=%u Params=%u SameModule=%d Owner=%s Return=%s Object=%s"),
						DependencyIndex,
						static_cast<uint32>(Dependency.kind),
						static_cast<uint32>(Dependency.referenceKind),
						DependencyFunction != nullptr
							? UTF8_TO_TCHAR(DependencyFunction->GetDeclaration(
								false, false, false)) : TEXT("<none>"),
						DependencyFunction != nullptr
							&& DependencyFunction->traits.GetTrait(
								asTRAIT_GENERATED_FUNCTION) ? 1 : 0,
						DependencyFunction != nullptr
							? static_cast<uint32>(
								DependencyFunction->artifactInvocationKind) : 0u,
						DependencyFunction != nullptr
							? DependencyFunction->GetParamCount() : 0u,
						DependencyFunction != nullptr
							&& DependencyFunction->module == Answer->module ? 1 : 0,
						DependencyOwner != nullptr
							? UTF8_TO_TCHAR(DependencyOwner->GetName()) : TEXT("<none>"),
						DependencyReturn != nullptr
							? UTF8_TO_TCHAR(DependencyReturn->GetName()) : TEXT("<none>"),
						DependencyObject != nullptr
							? UTF8_TO_TCHAR(DependencyObject->GetName()) : TEXT("<none>")));
				}
			}

			const FAngelscriptCacheCleanCaptureResult Capture =
				CaptureAngelscriptCleanCompiledModule(
					Module.ToSharedRef(), MakeCaptureOptions(), Artifacts);
			Test.AddInfo(FString::Printf(
				TEXT("VM corruption producer: Error=%u Records=%d Graph=%u Detail=%s"),
				static_cast<uint32>(Capture.Error),
				Artifacts.Records.Num(),
				Capture.ValidatedGraphRecordCount,
				*Capture.Detail));
			if (!Capture.IsSuccess()
				|| !FAngelscriptCacheStableSymbolIdentity::TryBuildFunctionKey(
					Artifacts.ModuleKey, *Answer, AnswerKey))
			{
				return false;
			}
		}

		TOptional<FAngelscriptCacheRecordId> DebugRecordId;
		for (const FAngelscriptPreparedRecord& Record : Artifacts.Records)
		{
			if (Record.RecordId.Kind
				!= EAngelscriptCacheRecordKind::FunctionBody)
			{
				continue;
			}
			TOptional<FAngelscriptDecodedCacheRecordHandle> Decoded;
			if (!DecodeRecord(Record, Decoded))
			{
				return false;
			}
			const FAngelscriptCachedFunctionBody* Body =
				Decoded.GetValue()->TryGetFunctionBody();
			if (Body != nullptr && Body->Identity.FunctionKey == AnswerKey)
			{
				OutBody = *Body;
				DebugRecordId = Body->DebugSidecar;
				break;
			}
		}
		if (!DebugRecordId.IsSet())
		{
			return false;
		}
		if (!(FAngelscriptArtifactIdentityBuilder::BuildFunctionContentHash(
			OutRawVmPayload, {}).Execution
			== OutBody.Identity.Content.Execution))
		{
			Test.AddError(TEXT(
				"The independently written raw VM artifact does not match the cached execution identity"));
			return false;
		}

		for (const FAngelscriptPreparedRecord& Record : Artifacts.Records)
		{
			if (!(Record.RecordId == DebugRecordId.GetValue()))
			{
				continue;
			}
			TOptional<FAngelscriptDecodedCacheRecordHandle> Decoded;
			if (!DecodeRecord(Record, Decoded))
			{
				return false;
			}
			const FAngelscriptCachedDebugSidecar* Debug =
				Decoded.GetValue()->TryGetDebugSidecar();
			if (Debug == nullptr)
			{
				return false;
			}
			OutDebug = *Debug;
			return true;
		}
		return false;
	}

	static bool IsAnswer(const asCScriptFunction& Function)
	{
		return Function.objectType == nullptr
			&& Function.name == "Answer";
	}

	static bool IsOffsetInPayload(
		const asUINT Offset,
		const TConstArrayView<uint8> Payload)
	{
		return Offset != asUINT(-1)
			&& Offset < static_cast<asUINT>(Payload.Num());
	}

	static bool MutateExecution(
		TArray<uint8>& Payload,
		const asSFunctionArtifactValidationDiagnostics& Diagnostics,
		const EExecutionMutation Mutation)
	{
		if (Payload.IsEmpty())
		{
			return false;
		}
		switch (Mutation)
		{
		case EExecutionMutation::UnsupportedVersion:
			if (Payload.Num() <= 8)
			{
				return false;
			}
			Payload[8] ^= 0x7f;
			return true;
		case EExecutionMutation::UnknownRootTrait:
			if (!IsOffsetInPayload(Diagnostics.rootTraitsOffset + 3u, Payload))
			{
				return false;
			}
			Payload[Diagnostics.rootTraitsOffset + 3u] |= 0x80u;
			return true;
		case EExecutionMutation::InvalidHeapPrefix:
			if (!IsOffsetInPayload(Diagnostics.objVariablesOnHeapOffset, Payload)
				|| Diagnostics.objectVariableCount >= 63u)
			{
				return false;
			}
			Payload[Diagnostics.objVariablesOnHeapOffset] =
				static_cast<uint8>(Diagnostics.objectVariableCount + 1u);
			return true;
		case EExecutionMutation::ImpossibleObjectCount:
			if (!IsOffsetInPayload(Diagnostics.objectVariableCountOffset, Payload))
			{
				return false;
			}
			Payload[Diagnostics.objectVariableCountOffset] = 63u;
			return true;
		case EExecutionMutation::UnknownObjectType:
			if (!IsOffsetInPayload(
				Diagnostics.firstObjectVariableTypeOffset, Payload))
			{
				return false;
			}
			Payload[Diagnostics.firstObjectVariableTypeOffset] =
				static_cast<uint8>('x');
			return true;
		case EExecutionMutation::InvalidObjectPosition:
			if (!IsOffsetInPayload(
				Diagnostics.firstObjectVariablePositionOffset, Payload))
			{
				return false;
			}
			Payload[Diagnostics.firstObjectVariablePositionOffset] = 0u;
			return true;
		case EExecutionMutation::TruncatedRuntimeState:
			Payload.Pop(EAllowShrinking::No);
			return true;
		case EExecutionMutation::TrailingData:
			Payload.Add(0xee);
			return true;
		default:
			return false;
		}
	}

	static bool ReplaceRawVmPayloadInEnvelope(
		TArray<uint8>& Envelope,
		const TConstArrayView<uint8> OriginalRaw,
		const TConstArrayView<uint8> Replacement)
	{
		if (OriginalRaw.IsEmpty() || Replacement.IsEmpty()
			|| Envelope.Num() < OriginalRaw.Num() + static_cast<int32>(sizeof(uint64)))
		{
			return false;
		}
		const int32 RawOffset = Envelope.Num() - OriginalRaw.Num();
		if (FMemory::Memcmp(
			Envelope.GetData() + RawOffset,
			OriginalRaw.GetData(), OriginalRaw.Num()) != 0)
		{
			return false;
		}
		const int32 LengthOffset = RawOffset - static_cast<int32>(sizeof(uint64));
		Envelope.SetNum(LengthOffset, EAllowShrinking::No);
		const uint64 ReplacementSize = static_cast<uint64>(Replacement.Num());
		for (uint32 Shift = 0; Shift < 64; Shift += 8)
		{
			Envelope.Add(static_cast<uint8>(ReplacementSize >> Shift));
		}
		Envelope.Append(Replacement);
		return true;
	}

	static const TCHAR* MutationName(const EExecutionMutation Mutation)
	{
		switch (Mutation)
		{
		case EExecutionMutation::UnsupportedVersion:
			return TEXT("UnsupportedVersion");
		case EExecutionMutation::UnknownRootTrait:
			return TEXT("UnknownRootTrait");
		case EExecutionMutation::InvalidHeapPrefix:
			return TEXT("InvalidHeapPrefix");
		case EExecutionMutation::ImpossibleObjectCount:
			return TEXT("ImpossibleObjectCount");
		case EExecutionMutation::UnknownObjectType:
			return TEXT("UnknownObjectType");
		case EExecutionMutation::InvalidObjectPosition:
			return TEXT("InvalidObjectPosition");
		case EExecutionMutation::TruncatedRuntimeState:
			return TEXT("TruncatedRuntimeState");
		case EExecutionMutation::TrailingData:
			return TEXT("TrailingData");
		default:
			return TEXT("Invalid");
		}
	}

	static asEBuildArtifactRestoreResult RestoreCorruptionMatrix(
		const asSBuildArtifactInvocation* Invocation,
		asCScriptFunction* Function,
		void* UserData)
	{
		if (Invocation == nullptr || Function == nullptr || UserData == nullptr)
		{
			return asBUILD_ARTIFACT_RESTORE_REJECTED_CORRUPT;
		}
		FRestoreContext& Context = *static_cast<FRestoreContext*>(UserData);
		if (!IsAnswer(*Function))
		{
			return asBUILD_ARTIFACT_RESTORE_MISS;
		}

		++Context.AnswerCallbackCount;
		FArtifactReadStream Stream(Context.RawVmPayload);
		asCReader Reader(Function->module, &Stream, Function->engine);
		Context.bBaselineValidated = Reader.ValidateFunctionArtifact(
			static_cast<asUINT>(Context.RawVmPayload.Num()),
			&Context.BaselineDiagnostics) >= 0;
		if (!Context.bBaselineValidated
			|| Context.BaselineDiagnostics.objectVariableCount == 0)
		{
			return asBUILD_ARTIFACT_RESTORE_REJECTED_CORRUPT;
		}

		for (const EExecutionMutation Mutation : {
			EExecutionMutation::UnsupportedVersion,
			EExecutionMutation::UnknownRootTrait,
			EExecutionMutation::InvalidHeapPrefix,
			EExecutionMutation::ImpossibleObjectCount,
			EExecutionMutation::UnknownObjectType,
			EExecutionMutation::InvalidObjectPosition,
			EExecutionMutation::TruncatedRuntimeState,
			EExecutionMutation::TrailingData})
		{
			FAngelscriptCachedFunctionBody Candidate = Context.Body;
			TArray<uint8> MutatedRaw = Context.RawVmPayload;
			check(MutateExecution(
				MutatedRaw,
				Context.BaselineDiagnostics,
				Mutation));
			check(ReplaceRawVmPayloadInEnvelope(
				Candidate.CanonicalExecutionPayload,
				Context.RawVmPayload,
				MutatedRaw));
			Candidate.Identity.Content.Execution =
				FAngelscriptArtifactIdentityBuilder::BuildFunctionContentHash(
					MutatedRaw, {}).Execution;

			FAttempt& Attempt = Context.Attempts.AddDefaulted_GetRef();
			Attempt.Mutation = Mutation;
			Attempt.BeforeWords = Function->scriptData != nullptr
				? static_cast<int32>(Function->scriptData->byteCode.GetLength())
				: -1;
			FAngelscriptCacheReadLimits Limits;
			FAngelscriptCacheReadBudget Budget;
			Attempt.Result =
				FAngelscriptCacheCompilerBridge::TryRestoreFunctionArtifact(
					*Invocation,
					*Function,
					Candidate,
					Context.Debug,
					Limits,
					Budget,
					Attempt.Detail);
			Attempt.AfterWords = Function->scriptData != nullptr
				? static_cast<int32>(Function->scriptData->byteCode.GetLength())
				: -1;
		}
		return asBUILD_ARTIFACT_RESTORE_REJECTED_CORRUPT;
	}

	static void ObserveCompile(
		const asSBuildArtifactInvocation*,
		const asSBuildArtifactCompileResult* Result,
		void* UserData)
	{
		if (Result == nullptr || Result->function == nullptr
			|| UserData == nullptr || !IsAnswer(*Result->function))
		{
			return;
		}
		FCompileObservation& Observation =
			*static_cast<FCompileObservation*>(UserData);
		++Observation.CallbackCount;
		Observation.RestoreResult = Result->restoreResult;
		Observation.bCompilerInvoked = Result->compilerInvoked;
		Observation.bSucceeded = Result->succeeded;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheFunctionArtifactCorruptionTests,
	"Angelscript.TestModule.Cache.FunctionArtifactCorruption",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(RuntimeStateCorruptionRejectsAtomicallyThenCompiles)
	{
		using namespace AngelscriptCacheFunctionArtifactCorruptionTests_Private;
		FRestoreContext Restore;
		ASSERT_THAT(IsTrue(ProduceAnswerCandidate(
			*TestRunner, Restore.Body, Restore.Debug, Restore.RawVmPayload)));

		FCompileObservation Compile;
		int32 Value = 0;
		{
			FAngelscriptTestFixture Consumer(
				*TestRunner, ETestEngineMode::IsolatedFull);
			ASSERT_THAT(IsTrue(Consumer.IsValid()));
			asCModule* Module = static_cast<asCModule*>(
				Consumer.GetEngine().GetScriptEngine()->GetModule(
					ModuleName, asGM_ALWAYS_CREATE));
			ASSERT_THAT(IsNotNull(Module));
			Module->SetBuildArtifactRestoreCallback(
				&RestoreCorruptionMatrix, &Restore);
			Module->SetBuildArtifactCompileResultCallback(
				&ObserveCompile, &Compile);
			ASSERT_THAT(AreEqual(asSUCCESS, Module->AddScriptSection(
				"FunctionArtifactCorruption.as",
				Source,
				FCStringAnsi::Strlen(Source),
				0)));
			ASSERT_THAT(IsTrue(Module->Build() >= 0));
			asIScriptFunction* Answer =
				Module->GetFunctionByDecl("int Answer()");
			ASSERT_THAT(IsNotNull(Answer));
			ASSERT_THAT(IsTrue(Consumer.ExecuteInt(*Answer, Value)));
		}

		const asSFunctionArtifactValidationDiagnostics& Diagnostics =
			Restore.BaselineDiagnostics;
		TestRunner->AddInfo(FString::Printf(
			TEXT("VM corruption layout: Valid=%d Result=%d Stage=%u Read=%u Expected=%u Error=%u New=%u Runtime=%u Stack=%u Heap=%u Count=%u FirstType=%u FirstPosition=%u ObjectCount=%u"),
			Restore.bBaselineValidated ? 1 : 0,
			Diagnostics.result,
			Diagnostics.stage,
			Diagnostics.bytesRead,
			Diagnostics.expectedSize,
			Diagnostics.hadError ? 1u : 0u,
			Diagnostics.wasNewFunction ? 1u : 0u,
			Diagnostics.runtimeStateOffset,
			Diagnostics.stackNeededOffset,
			Diagnostics.objVariablesOnHeapOffset,
			Diagnostics.objectVariableCountOffset,
			Diagnostics.firstObjectVariableTypeOffset,
			Diagnostics.firstObjectVariablePositionOffset,
			Diagnostics.objectVariableCount));
		ASSERT_THAT(IsTrue(Restore.bBaselineValidated));
		ASSERT_THAT(AreEqual(1, Restore.AnswerCallbackCount));
		ASSERT_THAT(IsTrue(Diagnostics.objectVariableCount > 0));
		ASSERT_THAT(AreEqual(8, Restore.Attempts.Num()));
		for (const FAttempt& Attempt : Restore.Attempts)
		{
			TestRunner->AddInfo(FString::Printf(
				TEXT("VM corruption fallback: Mutation=%s Result=%u Before=%d After=%d Detail=%s"),
				MutationName(Attempt.Mutation),
				static_cast<uint32>(Attempt.Result),
				Attempt.BeforeWords,
				Attempt.AfterWords,
				*Attempt.Detail));
			ASSERT_THAT(AreEqual(
				asBUILD_ARTIFACT_RESTORE_REJECTED_CORRUPT,
				Attempt.Result));
			ASSERT_THAT(AreEqual(0, Attempt.BeforeWords));
			ASSERT_THAT(AreEqual(0, Attempt.AfterWords));
		}
		ASSERT_THAT(AreEqual(1, Compile.CallbackCount));
		ASSERT_THAT(AreEqual(
			asBUILD_ARTIFACT_RESTORE_REJECTED_CORRUPT,
			Compile.RestoreResult));
		ASSERT_THAT(IsTrue(Compile.bCompilerInvoked));
		ASSERT_THAT(IsTrue(Compile.bSucceeded));
		ASSERT_THAT(AreEqual(42, Value));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
