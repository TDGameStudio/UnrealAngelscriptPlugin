#include "Cache/AngelscriptCacheArchive.h"
#include "Cache/AngelscriptCacheCleanCapture.h"
#include "Cache/AngelscriptCacheCompilerBridge.h"
#include "Cache/AngelscriptCacheDecodedRecord.h"
#include "Cache/AngelscriptCacheEnvironment.h"
#include "Cache/AngelscriptCacheRemainingRecordTypes.h"
#include "Cache/AngelscriptFunctionArtifactCodec.h"
#include "Core/Artifacts/AngelscriptArtifactIdentity.h"

#include "CQTest.h"
#include "Shared/AngelscriptTestFixture.h"

#include "as_module.h"
#include "as_objecttype.h"
#include "as_property.h"
#include "as_restore.h"
#include "as_scriptengine.h"
#include "as_scriptfunction.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptCacheSymbolDependencyValidationTests_Private
{
	class FArtifactStream final : public asIBinaryStream
	{
	public:
		virtual int Read(void* Data, const asUINT Size) override
		{
			if ((Data == nullptr && Size != 0)
				|| ReadOffset > Bytes.Num()
				|| Size > static_cast<asUINT>(Bytes.Num() - ReadOffset))
			{
				return asERROR;
			}
			if (Size != 0)
			{
				FMemory::Memcpy(Data, Bytes.GetData() + ReadOffset, Size);
				ReadOffset += static_cast<int32>(Size);
			}
			return asSUCCESS;
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
		int32 ReadOffset = 0;
	};

	class FAcceptingChargeSink final
		: public IAngelscriptCacheCandidateChargeSink
	{
	public:
		virtual EAngelscriptCacheCandidateChargeResult TryExtend(uint64) override
		{
			return EAngelscriptCacheCandidateChargeResult::Success;
		}
	};

	static FAngelscriptHash256 MakeCoordinateHash(
		const FStringView Domain,
		const FStringView Value)
	{
		FAngelscriptArtifactCanonicalWriter Writer(Domain);
		Writer.WriteString(Value);
		return Writer.FinalizeHash();
	}

	static TOptional<FAngelscriptStableModuleKey> MakeRawModuleKey(
		const FStringView LogicalPath,
		const FStringView ModuleName)
	{
		return FAngelscriptArtifactIdentityBuilder::TryBuildModuleKey(
			TEXT("/Angelscript/Game"), LogicalPath, ModuleName);
	}

	static bool BuildRawFunctionArtifact(
		FAngelscriptTestFixture& Fixture,
		const char* ModuleName,
		const char* SourceSection,
		const char* Source,
		const char* FunctionDeclaration,
		asCModule*& OutModule,
		asCScriptFunction*& OutFunction,
		TArray<uint8>& OutPayload)
	{
		OutModule = nullptr;
		OutFunction = nullptr;
		OutPayload.Reset();
		asCScriptEngine* Engine = static_cast<asCScriptEngine*>(
			Fixture.GetEngine().GetScriptEngine());
		if (Engine == nullptr)
		{
			return false;
		}
		OutModule = static_cast<asCModule*>(Engine->GetModule(
			ModuleName, asGM_ALWAYS_CREATE));
		if (OutModule == nullptr
			|| OutModule->AddScriptSection(
				SourceSection, Source, FCStringAnsi::Strlen(Source), 0) < 0
			|| OutModule->Build() < 0)
		{
			return false;
		}
		OutFunction = static_cast<asCScriptFunction*>(
			OutModule->GetFunctionByDecl(FunctionDeclaration));
		if (OutFunction == nullptr || OutFunction->scriptData == nullptr)
		{
			return false;
		}

		FArtifactStream Stream;
		asCWriter Writer(OutModule, &Stream, Engine, true);
		if (Writer.WriteFunctionArtifact(OutFunction) < 0
			|| Stream.Bytes.IsEmpty())
		{
			return false;
		}
		OutPayload = MoveTemp(Stream.Bytes);
		return true;
	}

	static FAngelscriptCacheValidationResult ValidateRawArtifact(
		asCModule& Module,
		asCScriptEngine& Engine,
		const FAngelscriptStableModuleKey& ModuleKey,
		const TConstArrayView<uint8> Payload,
		const TConstArrayView<FAngelscriptCacheSemanticDependency> Dependencies,
		FAngelscriptCacheOpaquePayloadSummary& OutSummary,
		FString& OutDetail)
	{
		FAngelscriptFunctionArtifactCodec Codec(Module, Engine);
		TArray<uint8> Envelope;
		FAngelscriptHash256 ExecutionHash;
		const FAngelscriptCacheValidationResult EncodeResult =
			Codec.EncodeExecutionArtifact(
				Payload, ModuleKey, Dependencies, Envelope, ExecutionHash);
		if (!EncodeResult.IsSuccess())
		{
			OutSummary = {};
			OutDetail = Codec.GetLastExecutionFailureDetail();
			return EncodeResult;
		}
		FAngelscriptCacheOpaquePayloadValidationRequest Request;
		Request.Kind = EAngelscriptCacheOpaquePayloadKind::FunctionExecution;
		Request.CodecVersion =
			FAngelscriptFunctionArtifactCodec::ExecutionCodecVersion;
		Request.ModuleKey = ModuleKey;
		Request.OwnerKey = MakeCoordinateHash(
			TEXT("cache-v2-v55-raw-artifact-owner"),
			TEXT("RawFunctionArtifact"));
		Request.CanonicalPayload = Envelope;
		Request.DeclaredDependencies = Dependencies;
		FAngelscriptCacheReadLimits Limits;
		FAngelscriptCacheReadBudget Budget;
		FAcceptingChargeSink ChargeSink;
		const FAngelscriptCacheValidationResult Result = Codec.Validate(
			Request, Limits, Budget, ChargeSink, OutSummary);
		OutDetail = Codec.GetLastExecutionFailureDetail();
		return Result;
	}

	static FAngelscriptCacheCleanCaptureOptions MakeCaptureOptions()
	{
		FAngelscriptCacheCleanCaptureOptions Options;
		FAngelscriptCompatibilityDescriptor Compatibility;
		Compatibility.CanonicalInputs = {
			TEXT("CacheV2SymbolDependencyValidation"),
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

	struct FDecodedAuthorities final
	{
		FAngelscriptCachedModuleInterface Interface;
		FAngelscriptCachedTypeSchema TypeSchema;
		FAngelscriptCachedModuleState State;
		TArray<FAngelscriptCachedFunctionBody> Bodies;
		TArray<FAngelscriptCacheRecordId> BodyRecordIds;
		FAngelscriptCachedModuleSnapshot Snapshot;
		FAngelscriptCacheRecordId SnapshotRecordId;
		bool bHasInterface = false;
		bool bHasTypeSchema = false;
		bool bHasState = false;
		bool bHasSnapshot = false;
	};

	static bool DecodeAuthorities(
		const FAngelscriptCacheCleanModuleArtifacts& Artifacts,
		FDecodedAuthorities& Out)
	{
		Out = {};
		FAngelscriptCacheReadLimits Limits;
		FAngelscriptCacheReadBudget Budget;
		for (const FAngelscriptPreparedRecord& Record : Artifacts.Records)
		{
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

			if (const FAngelscriptCachedModuleInterface* Interface =
				Decoded.GetValue()->TryGetModuleInterface())
			{
				if (Out.bHasInterface)
				{
					return false;
				}
				Out.Interface = *Interface;
				Out.bHasInterface = true;
			}
			else if (const FAngelscriptCachedTypeSchema* TypeSchema =
				Decoded.GetValue()->TryGetTypeSchema())
			{
				if (Out.bHasTypeSchema)
				{
					return false;
				}
				Out.TypeSchema = *TypeSchema;
				Out.bHasTypeSchema = true;
			}
			else if (const FAngelscriptCachedModuleState* State =
				Decoded.GetValue()->TryGetModuleState())
			{
				if (Out.bHasState)
				{
					return false;
				}
				Out.State = *State;
				Out.bHasState = true;
			}
			else if (const FAngelscriptCachedFunctionBody* FunctionBody =
				Decoded.GetValue()->TryGetFunctionBody())
			{
				Out.Bodies.Add(*FunctionBody);
				Out.BodyRecordIds.Add(Record.RecordId);
			}
			else if (const FAngelscriptCachedModuleSnapshot* Snapshot =
				Decoded.GetValue()->TryGetModuleSnapshot())
			{
				if (Out.bHasSnapshot)
				{
					return false;
				}
				Out.Snapshot = *Snapshot;
				Out.SnapshotRecordId = Record.RecordId;
				Out.bHasSnapshot = true;
			}
		}
		return Out.bHasInterface && Out.bHasTypeSchema && Out.bHasState
			&& Out.bHasSnapshot && Out.Bodies.Num() == Out.BodyRecordIds.Num();
	}

	static int32 FindFunctionBodyIndex(
		const FDecodedAuthorities& Decoded,
		const FStringView FunctionName)
	{
		const FAngelscriptCachedDeclaration* Declaration = nullptr;
		int32 DeclarationCount = 0;
		for (const FAngelscriptCachedDeclaration& Candidate
			: Decoded.Interface.Declarations)
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
			return INDEX_NONE;
		}

		int32 BodyIndex = INDEX_NONE;
		for (int32 CandidateIndex = 0;
			CandidateIndex < Decoded.Bodies.Num(); ++CandidateIndex)
		{
			if (Decoded.Bodies[CandidateIndex].Identity.FunctionKey.Hash
				== Declaration->StableKey)
			{
				if (BodyIndex != INDEX_NONE)
				{
					return INDEX_NONE;
				}
				BodyIndex = CandidateIndex;
			}
		}
		return BodyIndex;
	}

	static bool RemoveDependencyAndRepairDerivedInput(
		FAngelscriptCacheCleanModuleArtifacts& Artifacts,
		const FStringView TargetFunctionName,
		const EAngelscriptCacheSemanticDependencyKind DependencyKind,
		int32& OutOriginalDependencyCount,
		const IAngelscriptCacheCurrentSymbolResolver* ExternalSymbols = nullptr)
	{
		OutOriginalDependencyCount = 0;
		FDecodedAuthorities Decoded;
		if (!DecodeAuthorities(Artifacts, Decoded))
		{
			return false;
		}

		const int32 BodyIndex = FindFunctionBodyIndex(
			Decoded, TargetFunctionName);
		const int32 DependencyIndex = BodyIndex != INDEX_NONE
			? Decoded.Bodies[BodyIndex].ActualDependencies.IndexOfByPredicate(
				[DependencyKind](
					const FAngelscriptCacheSemanticDependency& Dependency)
				{
					return Dependency.Kind == DependencyKind;
				})
			: INDEX_NONE;
		if (BodyIndex == INDEX_NONE || DependencyIndex == INDEX_NONE)
		{
			return false;
		}

		FAngelscriptCachedFunctionBody& Body = Decoded.Bodies[BodyIndex];
		OutOriginalDependencyCount = Body.ActualDependencies.Num();
		Body.ActualDependencies.RemoveAt(DependencyIndex);

		FAngelscriptCacheFunctionInputAuthorities Authorities;
		Authorities.ModuleInterface = &Decoded.Interface;
		Authorities.TypeSchemas = TConstArrayView<FAngelscriptCachedTypeSchema>(
			&Decoded.TypeSchema, 1);
		Authorities.ModuleState = &Decoded.State;
		Authorities.FunctionBodies = Decoded.Bodies;
		Authorities.ExternalSymbols = ExternalSymbols;
		const FAngelscriptCacheFunctionInputResolution Resolution =
			FAngelscriptCacheCompilerBridge::ResolveCurrentFunctionInput(
				Body, Body.FunctionSourceDigest, Authorities);
		if ((Resolution.Status
				!= EAngelscriptCacheFunctionInputStatus::ResolvedMatch
			&& Resolution.Status
				!= EAngelscriptCacheFunctionInputStatus::ResolvedMismatch)
			|| Resolution.CurrentInputDigest.Hash.IsZero())
		{
			return false;
		}
		Body.FunctionInputDigest = Resolution.CurrentInputDigest;

		TArray<uint8> BodyPayload;
		if (!FAngelscriptCacheRemainingRecordArchive::SerializeFunctionBody(
			Body, BodyPayload).IsSuccess())
		{
			return false;
		}
		FAngelscriptPreparedRecord BodyRecord;
		if (!BuildPreparedRecord(EAngelscriptCacheRecordKind::FunctionBody,
			MoveTemp(BodyPayload), BodyRecord))
		{
			return false;
		}
		const FAngelscriptCacheRecordId NewBodyRecordId = BodyRecord.RecordId;
		if (!ReplaceRecord(Artifacts, Decoded.BodyRecordIds[BodyIndex],
			MoveTemp(BodyRecord)))
		{
			return false;
		}

		FAngelscriptCachedFunctionBodyLink* Link =
			Decoded.Snapshot.FunctionBodies.FindByPredicate(
				[&Body](const FAngelscriptCachedFunctionBodyLink& Candidate)
				{
					return Candidate.FunctionKey == Body.Identity.FunctionKey;
				});
		if (Link == nullptr)
		{
			return false;
		}
		Link->RecordId = NewBodyRecordId;

		TArray<uint8> SnapshotPayload;
		if (!FAngelscriptCacheRemainingRecordArchive::SerializeModuleSnapshot(
			Decoded.Snapshot, SnapshotPayload).IsSuccess())
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
		return ReplaceRecord(Artifacts, Decoded.SnapshotRecordId,
			MoveTemp(SnapshotRecord));
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheSymbolDependencyValidationTests,
	"Angelscript.TestModule.Cache.SymbolDependencyValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(PropertyRelocationWithoutDeclaredDependencyFailsGraphClosed)
	{
		using namespace AngelscriptCacheSymbolDependencyValidationTests_Private;
		FAngelscriptTestFixture Fixture(
			*TestRunner, ETestEngineMode::IsolatedFull);
		ASSERT_THAT(IsTrue(Fixture.IsValid()));
		const FString Source = TEXT(R"AS(
class FCacheDependencyPayload
{
	int Count;
}

const int GCacheDependencyAnswer = 41;

int ReadCacheDependency()
{
	FCacheDependencyPayload Payload;
	return Payload.Count + GCacheDependencyAnswer;
}
)AS");
		asIScriptModule* ScriptModule = Fixture.BuildModule(
			"ASCacheV2SymbolDependencyValidation", Source);
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
		TestRunner->AddInfo(FString::Printf(
			TEXT("V5.5 symbol dependency baseline: Error=%u Records=%d Graph=%u Detail=%s"),
			static_cast<uint32>(Capture.Error), Candidate.Records.Num(),
			Capture.ValidatedGraphRecordCount, *Capture.Detail));
		ASSERT_THAT(IsTrue(Capture.IsSuccess()));

		int32 OriginalDependencyCount = 0;
		ASSERT_THAT(IsTrue(RemoveDependencyAndRepairDerivedInput(
			Candidate,
			TEXT("ReadCacheDependency"),
			EAngelscriptCacheSemanticDependencyKind::PropertyLayout,
			OriginalDependencyCount)));
		ASSERT_THAT(IsTrue(OriginalDependencyCount > 1));

		FAngelscriptCacheCleanModuleArtifacts Output;
		const FAngelscriptCacheCleanCaptureResult Result =
			ValidateAndPromoteAngelscriptCleanCompiledModuleArtifacts(
				Module.ToSharedRef(), Options, MoveTemp(Candidate), Output);
		TestRunner->AddInfo(FString::Printf(
			TEXT("V5.5 missing property relocation dependency: Error=%u Graph=%u OutputRecords=%d OriginalDependencies=%d Detail=%s"),
			static_cast<uint32>(Result.Error),
			Result.ValidatedGraphRecordCount,
			Output.Records.Num(), OriginalDependencyCount, *Result.Detail));

		ASSERT_THAT(AreEqual(
			EAngelscriptCacheCleanCaptureError::GraphValidationFailed,
			Result.Error));
		ASSERT_THAT(AreEqual(uint32(0), Result.ValidatedGraphRecordCount));
		ASSERT_THAT(IsTrue(Result.Detail.Contains(TEXT("Error=47"))));
		ASSERT_THAT(IsTrue(Output.Records.IsEmpty()));
		ASSERT_THAT(IsTrue(Output.ModuleKey.Hash.IsZero()));
	}

	TEST_METHOD(TypeDeclarationRelocationWithoutDeclaredDependencyFailsGraphClosed)
	{
		using namespace AngelscriptCacheSymbolDependencyValidationTests_Private;
		FAngelscriptTestFixture Fixture(
			*TestRunner, ETestEngineMode::IsolatedFull);
		ASSERT_THAT(IsTrue(Fixture.IsValid()));
		const FString Source = TEXT(R"AS(
class FCacheTypeDependencyPayload
{
	int Count;
}

const int GCacheTypeDependencyAnswer = 41;

int ReadCacheTypeDependency()
{
	FCacheTypeDependencyPayload Payload;
	return Payload.Count + GCacheTypeDependencyAnswer;
}
)AS");
		asIScriptModule* ScriptModule = Fixture.BuildModule(
			"ASCacheV2TypeDependencyValidation", Source);
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
		TestRunner->AddInfo(FString::Printf(
			TEXT("V5.5 type dependency baseline: Error=%u Records=%d Graph=%u Detail=%s"),
			static_cast<uint32>(Capture.Error), Candidate.Records.Num(),
			Capture.ValidatedGraphRecordCount, *Capture.Detail));
		ASSERT_THAT(IsTrue(Capture.IsSuccess()));

		int32 OriginalDependencyCount = 0;
		ASSERT_THAT(IsTrue(RemoveDependencyAndRepairDerivedInput(
			Candidate,
			TEXT("ReadCacheTypeDependency"),
			EAngelscriptCacheSemanticDependencyKind::Declaration,
			OriginalDependencyCount)));
		ASSERT_THAT(IsTrue(OriginalDependencyCount > 1));

		FAngelscriptCacheCleanModuleArtifacts Output;
		const FAngelscriptCacheCleanCaptureResult Result =
			ValidateAndPromoteAngelscriptCleanCompiledModuleArtifacts(
				Module.ToSharedRef(), Options, MoveTemp(Candidate), Output);
		TestRunner->AddInfo(FString::Printf(
			TEXT("V5.5 missing type declaration dependency: Error=%u Graph=%u OutputRecords=%d OriginalDependencies=%d Detail=%s"),
			static_cast<uint32>(Result.Error),
			Result.ValidatedGraphRecordCount,
			Output.Records.Num(), OriginalDependencyCount, *Result.Detail));

		ASSERT_THAT(AreEqual(
			EAngelscriptCacheCleanCaptureError::GraphValidationFailed,
			Result.Error));
		ASSERT_THAT(AreEqual(uint32(0), Result.ValidatedGraphRecordCount));
		ASSERT_THAT(IsTrue(Result.Detail.Contains(TEXT("Error=47"))));
		ASSERT_THAT(IsTrue(Output.Records.IsEmpty()));
		ASSERT_THAT(IsTrue(Output.ModuleKey.Hash.IsZero()));
	}

	TEST_METHOD(FunctionRelocationWithoutDeclaredDependencyFailsGraphClosed)
	{
		using namespace AngelscriptCacheSymbolDependencyValidationTests_Private;
		FAngelscriptTestFixture Fixture(
			*TestRunner, ETestEngineMode::IsolatedFull);
		ASSERT_THAT(IsTrue(Fixture.IsValid()));
		const FString Source = TEXT(R"AS(
enum ECacheFunctionDependencyMarker
{
	Ready = 1,
}

int ProvideCacheFunctionDependency()
{
	return 41;
}

int ReadCacheFunctionDependency()
{
	return ProvideCacheFunctionDependency() + 1;
}
)AS");
		asIScriptModule* ScriptModule = Fixture.BuildModule(
			"ASCacheV2FunctionDependencyValidation", Source);
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
		TestRunner->AddInfo(FString::Printf(
			TEXT("V5.5 function dependency baseline: Error=%u Records=%d Graph=%u Detail=%s"),
			static_cast<uint32>(Capture.Error), Candidate.Records.Num(),
			Capture.ValidatedGraphRecordCount, *Capture.Detail));
		ASSERT_THAT(IsTrue(Capture.IsSuccess()));

		int32 OriginalDependencyCount = 0;
		ASSERT_THAT(IsTrue(RemoveDependencyAndRepairDerivedInput(
			Candidate,
			TEXT("ReadCacheFunctionDependency"),
			EAngelscriptCacheSemanticDependencyKind::Signature,
			OriginalDependencyCount)));
		ASSERT_THAT(IsTrue(OriginalDependencyCount > 0));

		FAngelscriptCacheCleanModuleArtifacts Output;
		const FAngelscriptCacheCleanCaptureResult Result =
			ValidateAndPromoteAngelscriptCleanCompiledModuleArtifacts(
				Module.ToSharedRef(), Options, MoveTemp(Candidate), Output);
		TestRunner->AddInfo(FString::Printf(
			TEXT("V5.5 missing function signature dependency: Error=%u Graph=%u OutputRecords=%d OriginalDependencies=%d Detail=%s"),
			static_cast<uint32>(Result.Error),
			Result.ValidatedGraphRecordCount,
			Output.Records.Num(), OriginalDependencyCount, *Result.Detail));

		ASSERT_THAT(AreEqual(
			EAngelscriptCacheCleanCaptureError::GraphValidationFailed,
			Result.Error));
		ASSERT_THAT(AreEqual(uint32(0), Result.ValidatedGraphRecordCount));
		ASSERT_THAT(IsTrue(Result.Detail.Contains(TEXT("Error=47"))));
		ASSERT_THAT(IsTrue(Output.Records.IsEmpty()));
		ASSERT_THAT(IsTrue(Output.ModuleKey.Hash.IsZero()));
	}

	TEST_METHOD(GeneratedStaticClassStorageArtifactFailsClosedWithoutPublicGlobalDependency)
	{
		using namespace AngelscriptCacheSymbolDependencyValidationTests_Private;
		FAngelscriptTestFixture Fixture(
			*TestRunner, ETestEngineMode::IsolatedFull);
		ASSERT_THAT(IsTrue(Fixture.IsValid()));
		const FString Source = TEXT(R"AS(
class FCacheGeneratedStaticStorage
{
	int Count;
}

const int GCacheGeneratedStaticAnswer = 41;

int ReadGeneratedStaticAnswer()
{
	return GCacheGeneratedStaticAnswer;
}
)AS");
		static constexpr const char* ModuleName =
			"ASCacheV2GeneratedStaticStorageValidation";
		asIScriptModule* PublicModule = Fixture.BuildModule(ModuleName, Source);
		ASSERT_THAT(IsNotNull(PublicModule));
		asCModule* Module = static_cast<asCModule*>(PublicModule);
		asCScriptFunction* StaticClassFunction = nullptr;
		for (asUINT Index = 0;
			Index < Module->globalFunctionList.GetLength(); ++Index)
		{
			asCScriptFunction* Candidate = Module->globalFunctionList[Index];
			if (Candidate != nullptr
				&& Candidate->name == "StaticClass"
				&& Candidate->traits.GetTrait(asTRAIT_GENERATED_FUNCTION))
			{
				ASSERT_THAT(IsNull(StaticClassFunction));
				StaticClassFunction = Candidate;
			}
		}
		ASSERT_THAT(IsNotNull(StaticClassFunction));
		asCGlobalProperty* Global = nullptr;
		for (asUINT Index = 0;
			Index < Module->scriptGlobalsList.GetLength(); ++Index)
		{
			asCGlobalProperty* Candidate = Module->scriptGlobalsList[Index];
			if (Candidate != nullptr
				&& Candidate->name == "__StaticType_FCacheGeneratedStaticStorage")
			{
				ASSERT_THAT(IsNull(Global));
				Global = Candidate;
			}
		}
		ASSERT_THAT(IsNotNull(Global));

		FArtifactStream Stream;
		asSFunctionArtifactWriteDiagnostics Diagnostics{};
		asCWriter Writer(Module, &Stream, Module->engine, true);
		ASSERT_THAT(AreEqual(asSUCCESS,
			Writer.WriteFunctionArtifact(StaticClassFunction, &Diagnostics)));
		ASSERT_THAT(IsFalse(Stream.Bytes.IsEmpty()));
		ASSERT_THAT(AreEqual(asUINT(1),
			Diagnostics.usedGlobalPropertyCount));
		ASSERT_THAT(IsTrue(StaticClassFunction->scriptData != nullptr));
		bool bObservedGeneratedGlobalStorage = false;
		for (asUINT Index = 0; Index < StaticClassFunction->scriptData->
			artifactDependencies.GetLength(); ++Index)
		{
			const asSBuildArtifactDependency& Dependency =
				StaticClassFunction->scriptData->artifactDependencies[Index];
			bObservedGeneratedGlobalStorage |= Dependency.kind
					== asBUILD_ARTIFACT_DEPENDENCY_GLOBAL_STORAGE
				&& Dependency.referenceKind
					== asBUILD_ARTIFACT_REFERENCE_GLOBAL
				&& Dependency.globalProperty == Global;
		}
		ASSERT_THAT(IsTrue(bObservedGeneratedGlobalStorage));

		const TOptional<FAngelscriptStableModuleKey> ModuleKey =
			MakeRawModuleKey(
				TEXT("GeneratedStaticStorageValidation.as"),
				ANSI_TO_TCHAR(ModuleName));
		ASSERT_THAT(IsTrue(ModuleKey.IsSet()));
		FAngelscriptCacheOpaquePayloadSummary RejectedSummary;
		FString RejectedDetail;
		const FAngelscriptCacheValidationResult Rejected = ValidateRawArtifact(
			*Module, *Module->engine, ModuleKey.GetValue(), Stream.Bytes, {},
			RejectedSummary, RejectedDetail);
		TestRunner->AddInfo(FString::Printf(
			TEXT("V5.5 generated StaticClass storage artifact: WriterResult=%d Globals=%u Bytes=%d ValidationError=%u Relocations=%d Detail=%s"),
			Diagnostics.result, Diagnostics.usedGlobalPropertyCount,
			Stream.Bytes.Num(), static_cast<uint32>(Rejected.Error),
			RejectedSummary.OrderedRelocations.Num(), *RejectedDetail));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheValidationError::RelocationDependencyMismatch,
			Rejected.Error));
		ASSERT_THAT(IsTrue(RejectedSummary.OrderedRelocations.IsEmpty()));
		ASSERT_THAT(IsTrue(RejectedDetail.Contains(
			TEXT("Function artifact symbol use"))));
	}

	TEST_METHOD(GeneratedStaticClassCallUsesOwningTypeDeclarationAuthority)
	{
		using namespace AngelscriptCacheSymbolDependencyValidationTests_Private;
		FAngelscriptTestFixture Fixture(
			*TestRunner, ETestEngineMode::IsolatedFull);
		ASSERT_THAT(IsTrue(Fixture.IsValid()));
		const FString Source = TEXT(R"AS(
class FCacheGeneratedStaticCall
{
	int Count;
}

const int GCacheGeneratedStaticCallAnswer = 41;

int ReadGeneratedStaticClassCall()
{
	return FCacheGeneratedStaticCall::StaticClass() != nullptr
		? GCacheGeneratedStaticCallAnswer : 0;
}
)AS");
		asIScriptModule* ScriptModule = Fixture.BuildModule(
			"ASCacheV2GeneratedStaticCallValidation", Source);
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
		TestRunner->AddInfo(FString::Printf(
			TEXT("V5.5 generated StaticClass call baseline: Error=%u Records=%d Graph=%u Detail=%s"),
			static_cast<uint32>(Capture.Error), Candidate.Records.Num(),
			Capture.ValidatedGraphRecordCount, *Capture.Detail));
		ASSERT_THAT(IsTrue(Capture.IsSuccess()));
		FDecodedAuthorities Decoded;
		ASSERT_THAT(IsTrue(DecodeAuthorities(Candidate, Decoded)));
		const int32 TargetBodyIndex = FindFunctionBodyIndex(
			Decoded, TEXT("ReadGeneratedStaticClassCall"));
		ASSERT_THAT(IsTrue(TargetBodyIndex != INDEX_NONE));
		int32 EnvironmentDependencyCount = 0;
		for (const FAngelscriptCacheSemanticDependency& Dependency
			: Decoded.Bodies[TargetBodyIndex].ActualDependencies)
		{
			if (Dependency.Kind
					== EAngelscriptCacheSemanticDependencyKind::EnvironmentAbi
				&& Dependency.Target.Kind
					== EAngelscriptCacheReferenceKind::EnvironmentSymbol)
			{
				++EnvironmentDependencyCount;
			}
		}
		ASSERT_THAT(AreEqual(1, EnvironmentDependencyCount));

		asCScriptEngine* ScriptEngine = static_cast<asCScriptEngine*>(
			Fixture.GetEngine().GetScriptEngine());
		ASSERT_THAT(IsNotNull(ScriptEngine));
		FAngelscriptCacheEngineEnvironmentResolver EnvironmentSymbols(
			*ScriptEngine);
		FAngelscriptCacheCleanModuleArtifacts MissingEnvironment = Candidate;
		int32 EnvironmentOriginalDependencyCount = 0;
		ASSERT_THAT(IsTrue(RemoveDependencyAndRepairDerivedInput(
			MissingEnvironment,
			TEXT("ReadGeneratedStaticClassCall"),
			EAngelscriptCacheSemanticDependencyKind::EnvironmentAbi,
			EnvironmentOriginalDependencyCount,
			&EnvironmentSymbols)));
		FAngelscriptCacheCleanModuleArtifacts MissingEnvironmentOutput;
		const FAngelscriptCacheCleanCaptureResult MissingEnvironmentResult =
			ValidateAndPromoteAngelscriptCleanCompiledModuleArtifacts(
				Module.ToSharedRef(), Options, MoveTemp(MissingEnvironment),
				MissingEnvironmentOutput);
		TestRunner->AddInfo(FString::Printf(
			TEXT("V5.5 missing generated StaticClass environment dependency: Error=%u Graph=%u OutputRecords=%d OriginalDependencies=%d Detail=%s"),
			static_cast<uint32>(MissingEnvironmentResult.Error),
			MissingEnvironmentResult.ValidatedGraphRecordCount,
			MissingEnvironmentOutput.Records.Num(),
			EnvironmentOriginalDependencyCount,
			*MissingEnvironmentResult.Detail));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheCleanCaptureError::GraphValidationFailed,
			MissingEnvironmentResult.Error));
		ASSERT_THAT(AreEqual(uint32(0),
			MissingEnvironmentResult.ValidatedGraphRecordCount));
		ASSERT_THAT(IsTrue(MissingEnvironmentResult.Detail.Contains(
			TEXT("Error=47"))));
		ASSERT_THAT(IsTrue(MissingEnvironmentOutput.Records.IsEmpty()));
		ASSERT_THAT(IsTrue(MissingEnvironmentOutput.ModuleKey.Hash.IsZero()));

		int32 OriginalDependencyCount = 0;
		ASSERT_THAT(IsTrue(RemoveDependencyAndRepairDerivedInput(
			Candidate,
			TEXT("ReadGeneratedStaticClassCall"),
			EAngelscriptCacheSemanticDependencyKind::Declaration,
			OriginalDependencyCount,
			&EnvironmentSymbols)));
		ASSERT_THAT(IsTrue(OriginalDependencyCount > 1));

		FAngelscriptCacheCleanModuleArtifacts Output;
		const FAngelscriptCacheCleanCaptureResult Result =
			ValidateAndPromoteAngelscriptCleanCompiledModuleArtifacts(
				Module.ToSharedRef(), Options, MoveTemp(Candidate), Output);
		TestRunner->AddInfo(FString::Printf(
			TEXT("V5.5 missing generated StaticClass owning-type dependency: Error=%u Graph=%u OutputRecords=%d OriginalDependencies=%d Detail=%s"),
			static_cast<uint32>(Result.Error),
			Result.ValidatedGraphRecordCount,
			Output.Records.Num(), OriginalDependencyCount, *Result.Detail));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheCleanCaptureError::GraphValidationFailed,
			Result.Error));
		ASSERT_THAT(AreEqual(uint32(0), Result.ValidatedGraphRecordCount));
		ASSERT_THAT(IsTrue(Result.Detail.Contains(TEXT("Error=47"))));
		ASSERT_THAT(IsTrue(Output.Records.IsEmpty()));
		ASSERT_THAT(IsTrue(Output.ModuleKey.Hash.IsZero()));
	}

	TEST_METHOD(ValueLayoutRelocationRequiresExactStableDependency)
	{
		using namespace AngelscriptCacheSymbolDependencyValidationTests_Private;
		FAngelscriptTestFixture Fixture(
			*TestRunner, ETestEngineMode::IsolatedFull);
		ASSERT_THAT(IsTrue(Fixture.IsValid()));
		static constexpr const char* ModuleName =
			"ASCacheV2ValueLayoutSymbolValidation";
		static constexpr const char* Source = R"AS(
struct FCacheValueLayout
{
	int Number;
}

int ReadCacheValueLayout(FCacheValueLayout Value)
{
	return Value.Number;
}
)AS";
		asCModule* Module = nullptr;
		asCScriptFunction* Function = nullptr;
		TArray<uint8> Payload;
		ASSERT_THAT(IsTrue(BuildRawFunctionArtifact(
			Fixture, ModuleName, "ValueLayoutSymbolValidation.as", Source,
			"int ReadCacheValueLayout(FCacheValueLayout)",
			Module, Function, Payload)));
		ASSERT_THAT(IsNotNull(Module));
		ASSERT_THAT(IsNotNull(Function));
		ASSERT_THAT(AreEqual(asUINT(1), Module->GetObjectTypeCount()));
		asCObjectType* ValueType = static_cast<asCObjectType*>(
			Module->GetObjectTypeByIndex(0));
		ASSERT_THAT(IsNotNull(ValueType));
		ASSERT_THAT(AreEqual(asUINT(1), ValueType->localProperties.GetLength()));
		asCObjectProperty* Property = ValueType->localProperties[0];
		ASSERT_THAT(IsNotNull(Property));

		const TOptional<FAngelscriptStableModuleKey> ModuleKey =
			MakeRawModuleKey(
				TEXT("ValueLayoutSymbolValidation.as"),
				ANSI_TO_TCHAR(ModuleName));
		ASSERT_THAT(IsTrue(ModuleKey.IsSet()));
		FAngelscriptTypeIdentityDescriptor TypeIdentity;
		TypeIdentity.ModuleKey = ModuleKey.GetValue();
		TypeIdentity.Namespace = UTF8_TO_TCHAR(ValueType->GetNamespace());
		TypeIdentity.Kind = EAngelscriptArtifactEntityKind::Struct;
		TypeIdentity.CanonicalDeclaration = TEXT("struct FCacheValueLayout");
		const FAngelscriptStableTypeKey TypeKey =
			FAngelscriptArtifactIdentityBuilder::BuildTypeKey(TypeIdentity);
		ASSERT_THAT(IsFalse(TypeKey.Hash.IsZero()));
		FAngelscriptPropertyIdentityDescriptor PropertyIdentity;
		PropertyIdentity.OwnerTypeKey = TypeKey;
		PropertyIdentity.Kind = EAngelscriptArtifactEntityKind::Property;
		PropertyIdentity.Name = UTF8_TO_TCHAR(Property->name.AddressOf());
		PropertyIdentity.CanonicalType = UTF8_TO_TCHAR(
			Property->type.Format(ValueType->nameSpace, false, false).AddressOf());
		const FAngelscriptStablePropertyKey PropertyKey =
			FAngelscriptArtifactIdentityBuilder::BuildPropertyKey(PropertyIdentity);
		ASSERT_THAT(IsFalse(PropertyKey.Hash.IsZero()));

		FAngelscriptCacheSemanticDependency ValueLayoutDependency;
		ValueLayoutDependency.Kind =
			EAngelscriptCacheSemanticDependencyKind::ValueLayout;
		ValueLayoutDependency.Target = {
			EAngelscriptCacheReferenceKind::ScriptType,
			TypeKey.Hash,
			MakeCoordinateHash(
				TEXT("cache-v2-v55-value-type-abi"),
				TypeIdentity.CanonicalDeclaration),
		};
		ValueLayoutDependency.ExpectedContentOrValue = MakeCoordinateHash(
			TEXT("cache-v2-v55-value-layout"),
			TypeIdentity.CanonicalDeclaration);
		FAngelscriptCacheSemanticDependency PropertyDependency;
		PropertyDependency.Kind =
			EAngelscriptCacheSemanticDependencyKind::PropertyLayout;
		PropertyDependency.Target = {
			EAngelscriptCacheReferenceKind::ScriptProperty,
			PropertyKey.Hash,
			MakeCoordinateHash(
				TEXT("cache-v2-v55-value-property-abi"),
				PropertyIdentity.Name),
		};
		PropertyDependency.ExpectedContentOrValue = MakeCoordinateHash(
			TEXT("cache-v2-v55-value-property-layout"),
			PropertyIdentity.Name);
		const TArray<FAngelscriptCacheSemanticDependency> Dependencies{
			ValueLayoutDependency,
			PropertyDependency,
		};

		asCScriptEngine* Engine = static_cast<asCScriptEngine*>(
			Fixture.GetEngine().GetScriptEngine());
		ASSERT_THAT(IsNotNull(Engine));
		FAngelscriptCacheOpaquePayloadSummary BaselineSummary;
		FString BaselineDetail;
		const FAngelscriptCacheValidationResult Baseline = ValidateRawArtifact(
			*Module, *Engine, ModuleKey.GetValue(), Payload, Dependencies,
			BaselineSummary, BaselineDetail);
		TestRunner->AddInfo(FString::Printf(
			TEXT("V5.5 value-layout symbol baseline: Error=%u Relocations=%d Bytes=%d Detail=%s"),
			static_cast<uint32>(Baseline.Error),
			BaselineSummary.OrderedRelocations.Num(), Payload.Num(),
			*BaselineDetail));
		ASSERT_THAT(IsTrue(Baseline.IsSuccess()));
		ASSERT_THAT(IsTrue(BaselineSummary.OrderedRelocations.ContainsByPredicate(
			[&TypeKey](const FAngelscriptCacheRelocationUse& Relocation)
			{
				return Relocation.DependencyKind
						== EAngelscriptCacheSemanticDependencyKind::ValueLayout
					&& Relocation.ReferenceKind
						== EAngelscriptCacheReferenceKind::ScriptType
					&& Relocation.StableKey == TypeKey.Hash;
			})));

		const TArray<FAngelscriptCacheSemanticDependency> MissingValueLayout{
			PropertyDependency};
		FAngelscriptCacheOpaquePayloadSummary MissingSummary;
		FString MissingDetail;
		const FAngelscriptCacheValidationResult Missing = ValidateRawArtifact(
			*Module, *Engine, ModuleKey.GetValue(), Payload, MissingValueLayout,
			MissingSummary, MissingDetail);
		TestRunner->AddInfo(FString::Printf(
			TEXT("V5.5 missing value-layout dependency: Error=%u Relocations=%d Detail=%s"),
			static_cast<uint32>(Missing.Error),
			MissingSummary.OrderedRelocations.Num(), *MissingDetail));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheValidationError::RelocationDependencyMismatch,
			Missing.Error));
		ASSERT_THAT(IsTrue(MissingSummary.OrderedRelocations.IsEmpty()));
		ASSERT_THAT(IsTrue(MissingDetail.Contains(TEXT("expects dependency"))));
		ASSERT_THAT(IsTrue(MissingDetail.Contains(TEXT("but declared 1"))));
		ASSERT_THAT(IsTrue(MissingDetail.Contains(TypeKey.Hash.ToHexString())));
		ASSERT_THAT(IsTrue(MissingDetail.Contains(PropertyKey.Hash.ToHexString())));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
