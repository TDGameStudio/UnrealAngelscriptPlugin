#include "Cache/AngelscriptCacheCleanCapture.h"
#include "Cache/AngelscriptCacheDecodedRecord.h"

#include "CQTest.h"
#include "Shared/AngelscriptTestFixture.h"

#include "as_objecttype.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptCacheCleanOracleMutationTests_Private
{
	struct FCleanObservation
	{
		FAngelscriptCacheRecordId SourceIndexRecordId;
		FAngelscriptCacheRecordId ModuleInterfaceRecordId;
		FAngelscriptCacheRecordId TypeSchemaRecordId;
		FAngelscriptCacheRecordId ModuleStateRecordId;
		FAngelscriptCacheRecordId FunctionBodyRecordId;
		FAngelscriptCacheRecordId DebugSidecarRecordId;
		FAngelscriptCacheRecordId ModuleSnapshotRecordId;
		FAngelscriptCachedModuleInterface Interface;
		FAngelscriptCachedTypeSchema TypeSchema;
		FAngelscriptCachedModuleState ModuleState;
		FAngelscriptCachedFunctionBody FunctionBody;
		FAngelscriptCachedDebugSidecar DebugSidecar;
		FAngelscriptCachedModuleSnapshot ModuleSnapshot;
	};

	static FAngelscriptCacheCleanCaptureOptions MakeCaptureOptions()
	{
		FAngelscriptCacheCleanCaptureOptions Options;
		FAngelscriptCompatibilityDescriptor Compatibility;
		Compatibility.CanonicalInputs = {
			TEXT("CacheV2CleanOracleMutationTest"),
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

	static bool CaptureSource(
		FAutomationTestBase& Test,
		const char* ModuleName,
		const FString& Source,
		FAngelscriptCacheCleanModuleArtifacts& OutArtifacts,
		FAngelscriptCacheCleanCaptureResult& OutCapture)
	{
		OutArtifacts.Reset();
		OutCapture = {};
		FAngelscriptTestFixture Fixture(Test, ETestEngineMode::IsolatedFull);
		if (!Fixture.IsValid())
		{
			Test.AddError(TEXT("Failed to create the clean-oracle isolated Engine"));
			return false;
		}

		asIScriptModule* ScriptModule = Fixture.BuildModule(ModuleName, Source);
		if (ScriptModule == nullptr)
		{
			return false;
		}
		const TSharedPtr<FAngelscriptModuleDesc> Module =
			Fixture.GetEngine().GetModule(ScriptModule);
		if (!Module.IsValid())
		{
			Test.AddError(TEXT("The clean-oracle compile lost its module descriptor"));
			return false;
		}
		Test.AddInfo(FString::Printf(
			TEXT("Clean-oracle compiled shape: DescClasses=%d DescEnums=%d DescDelegates=%d VmObjects=%u VmEnums=%u VmGlobals=%u VmGlobalFunctions=%u"),
			Module->Classes.Num(),
			Module->Enums.Num(),
			Module->Delegates.Num(),
			ScriptModule->GetObjectTypeCount(),
			ScriptModule->GetEnumCount(),
			ScriptModule->GetGlobalVarCount(),
			ScriptModule->GetFunctionCount()));
		for (asUINT Index = 0; Index < ScriptModule->GetGlobalVarCount(); ++Index)
		{
			Test.AddInfo(FString::Printf(
				TEXT("Clean-oracle VM global[%u]: %s"),
				Index,
				UTF8_TO_TCHAR(ScriptModule->GetGlobalVarDeclaration(Index, true))));
		}
		for (asUINT Index = 0; Index < ScriptModule->GetFunctionCount(); ++Index)
		{
			asIScriptFunction* Function = ScriptModule->GetFunctionByIndex(Index);
			Test.AddInfo(FString::Printf(
				TEXT("Clean-oracle VM function[%u]: %s type=%u"),
				Index,
				Function != nullptr
					? UTF8_TO_TCHAR(Function->GetDeclaration(true, false, false))
					: TEXT("<null>"),
				Function != nullptr
					? static_cast<uint32>(Function->GetFuncType()) : MAX_uint32));
		}
		for (const TSharedRef<FAngelscriptClassDesc>& Class : Module->Classes)
		{
			const asCObjectType* ScriptType =
				static_cast<const asCObjectType*>(Class->ScriptType);
			Test.AddInfo(FString::Printf(
				TEXT("Clean-oracle class: Name=%s Namespace=%s Super=%s CodeSuper=%s SuperIsCode=%d IsStruct=%d StaticGlobal=%s VmSize=%d VmAlign=%d BaseBoundary=%d DescProperties=%d VmProperties=%u"),
				*Class->ClassName,
				Class->Namespace.IsSet() ? **Class->Namespace : TEXT(""),
				*Class->SuperClass,
				Class->CodeSuperClass != nullptr
					? *Class->CodeSuperClass->GetPathName() : TEXT("<none>"),
				Class->bSuperIsCodeClass ? 1 : 0,
				Class->bIsStruct ? 1 : 0,
				*Class->StaticClassGlobalVariableName,
				ScriptType != nullptr ? ScriptType->size : -1,
				ScriptType != nullptr ? ScriptType->alignment : -1,
				ScriptType != nullptr ? ScriptType->basePropertyOffset : -1,
				Class->Properties.Num(),
				ScriptType != nullptr ? ScriptType->GetPropertyCount() : 0));
			for (const TSharedRef<FAngelscriptPropertyDesc>& Property
				: Class->Properties)
			{
				Test.AddInfo(FString::Printf(
					TEXT("Clean-oracle property: Name=%s LiteralType=%s ScriptIndex=%d ScriptOffset=%llu Private=%d Protected=%d UnrealProperty=%d"),
					*Property->PropertyName,
					*Property->LiteralType,
					Property->ScriptPropertyIndex,
					static_cast<uint64>(Property->ScriptPropertyOffset),
					Property->bIsPrivate ? 1 : 0,
					Property->bIsProtected ? 1 : 0,
					Property->bHasUnrealProperty ? 1 : 0));
			}
		}

		OutCapture = CaptureAngelscriptCleanCompiledModule(
			Module.ToSharedRef(), MakeCaptureOptions(), OutArtifacts);
		Test.AddInfo(FString::Printf(
			TEXT("Clean-oracle capture: Module=%s Error=%u Records=%d GraphRecords=%u Detail=%s"),
			*Module->ModuleName,
			static_cast<uint32>(OutCapture.Error),
			OutArtifacts.Records.Num(),
			OutCapture.ValidatedGraphRecordCount,
			*OutCapture.Detail));
		return true;
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

	static int32 CountRecords(
		const FAngelscriptCacheCleanModuleArtifacts& Artifacts,
		const EAngelscriptCacheRecordKind Kind)
	{
		int32 Count = 0;
		for (const FAngelscriptPreparedRecord& Record : Artifacts.Records)
		{
			if (Record.RecordId.Kind == Kind)
			{
				++Count;
			}
		}
		return Count;
	}

	static bool DecodeRecord(
		FAutomationTestBase& Test,
		FAngelscriptDecodedCacheRecordBatch& Batch,
		const FAngelscriptPreparedRecord* Prepared,
		TOptional<FAngelscriptDecodedCacheRecordHandle>& OutDecoded)
	{
		OutDecoded.Reset();
		if (Prepared == nullptr)
		{
			Test.AddError(TEXT("The requested prepared Cache V2 record is absent"));
			return false;
		}
		const FAngelscriptCacheValidationResult Result = Batch.TryDecode(
			Prepared->RecordId, Prepared->CanonicalPayload, OutDecoded);
		if (!Result.IsSuccess() || !OutDecoded.IsSet())
		{
			Test.AddError(FString::Printf(
				TEXT("The production Cache V2 record did not decode: Kind=%u Error=%u Stage=%u Offset=%llu"),
				static_cast<uint32>(Prepared->RecordId.Kind),
				static_cast<uint32>(Result.Error),
				static_cast<uint32>(Result.Stage),
				Result.ByteOffset));
			return false;
		}
		return true;
	}

	static bool ObserveArtifacts(
		FAutomationTestBase& Test,
		const FAngelscriptCacheCleanModuleArtifacts& Artifacts,
		const FStringView TargetFunctionName,
		FCleanObservation& OutObservation)
	{
		const FAngelscriptPreparedRecord* Source = FindRecord(
			Artifacts, EAngelscriptCacheRecordKind::SourceIndex);
		const FAngelscriptPreparedRecord* Interface = FindRecord(
			Artifacts, EAngelscriptCacheRecordKind::ModuleInterface);
		const FAngelscriptPreparedRecord* Type = FindRecord(
			Artifacts, EAngelscriptCacheRecordKind::TypeSchema);
		const FAngelscriptPreparedRecord* State = FindRecord(
			Artifacts, EAngelscriptCacheRecordKind::ModuleState);
		const FAngelscriptPreparedRecord* Snapshot = FindRecord(
			Artifacts, EAngelscriptCacheRecordKind::ModuleSnapshot);
		if (Source == nullptr || Interface == nullptr || Type == nullptr
			|| State == nullptr || Snapshot == nullptr)
		{
			Test.AddError(TEXT("The clean-oracle output is missing a required singleton record"));
			return false;
		}

		FAngelscriptCacheReadLimits Limits;
		FAngelscriptCacheReadBudget Budget;
		FAngelscriptDecodedCacheRecordBatch Batch(Budget, Limits);
		TOptional<FAngelscriptDecodedCacheRecordHandle> DecodedInterface;
		TOptional<FAngelscriptDecodedCacheRecordHandle> DecodedType;
		TOptional<FAngelscriptDecodedCacheRecordHandle> DecodedState;
		TOptional<FAngelscriptDecodedCacheRecordHandle> DecodedSnapshot;
		if (!DecodeRecord(Test, Batch, Interface, DecodedInterface)
			|| !DecodeRecord(Test, Batch, Type, DecodedType)
			|| !DecodeRecord(Test, Batch, State, DecodedState)
			|| !DecodeRecord(Test, Batch, Snapshot, DecodedSnapshot))
		{
			return false;
		}

		const FAngelscriptCachedModuleInterface* DecodedInterfaceValue =
			DecodedInterface.GetValue()->TryGetModuleInterface();
		const FAngelscriptCachedTypeSchema* DecodedTypeValue =
			DecodedType.GetValue()->TryGetTypeSchema();
		const FAngelscriptCachedModuleState* DecodedStateValue =
			DecodedState.GetValue()->TryGetModuleState();
		const FAngelscriptCachedModuleSnapshot* DecodedSnapshotValue =
			DecodedSnapshot.GetValue()->TryGetModuleSnapshot();
		if (DecodedInterfaceValue == nullptr || DecodedTypeValue == nullptr
			|| DecodedStateValue == nullptr || DecodedSnapshotValue == nullptr)
		{
			Test.AddError(TEXT("A required clean-oracle record decoded as the wrong semantic kind"));
			return false;
		}

		const FAngelscriptCachedDeclaration* TargetDeclaration = nullptr;
		int32 TargetDeclarationCount = 0;
		for (const FAngelscriptCachedDeclaration& Declaration
			: DecodedInterfaceValue->Declarations)
		{
			if (Declaration.DeclarationKind
					== EAngelscriptCacheDeclarationKind::Function
				&& Declaration.CanonicalName == TargetFunctionName)
			{
				TargetDeclaration = &Declaration;
				++TargetDeclarationCount;
			}
		}
		if (TargetDeclarationCount != 1 || TargetDeclaration == nullptr)
		{
			Test.AddError(FString::Printf(
				TEXT("The clean-oracle expected one target function named %.*s, found %d"),
				TargetFunctionName.Len(), TargetFunctionName.GetData(),
				TargetDeclarationCount));
			return false;
		}

		int32 FunctionMatchCount = 0;
		int32 DebugMatchCount = 0;
		for (const FAngelscriptPreparedRecord& Record : Artifacts.Records)
		{
			if (Record.RecordId.Kind != EAngelscriptCacheRecordKind::FunctionBody
				&& Record.RecordId.Kind
					!= EAngelscriptCacheRecordKind::DebugSidecar)
			{
				continue;
			}
			TOptional<FAngelscriptDecodedCacheRecordHandle> Decoded;
			if (!DecodeRecord(Test, Batch, &Record, Decoded))
			{
				return false;
			}
			if (const FAngelscriptCachedFunctionBody* FunctionValue =
				Decoded.GetValue()->TryGetFunctionBody())
			{
				if (FunctionValue->Identity.FunctionKey.Hash
					== TargetDeclaration->StableKey)
				{
					++FunctionMatchCount;
					OutObservation.FunctionBodyRecordId = Record.RecordId;
					OutObservation.FunctionBody = *FunctionValue;
				}
			}
			else if (const FAngelscriptCachedDebugSidecar* DebugValue =
				Decoded.GetValue()->TryGetDebugSidecar())
			{
				if (DebugValue->FunctionKey.Hash
					== TargetDeclaration->StableKey)
				{
					++DebugMatchCount;
					OutObservation.DebugSidecarRecordId = Record.RecordId;
					OutObservation.DebugSidecar = *DebugValue;
				}
			}
		}
		if (FunctionMatchCount != 1 || DebugMatchCount != 1)
		{
			Test.AddError(FString::Printf(
				TEXT("The clean-oracle target %.*s resolved to %d bodies and %d debug sidecars"),
				TargetFunctionName.Len(), TargetFunctionName.GetData(),
				FunctionMatchCount, DebugMatchCount));
			return false;
		}

		OutObservation.SourceIndexRecordId = Source->RecordId;
		OutObservation.ModuleInterfaceRecordId = Interface->RecordId;
		OutObservation.TypeSchemaRecordId = Type->RecordId;
		OutObservation.ModuleStateRecordId = State->RecordId;
		OutObservation.ModuleSnapshotRecordId = Snapshot->RecordId;
		OutObservation.Interface = *DecodedInterfaceValue;
		OutObservation.TypeSchema = *DecodedTypeValue;
		OutObservation.ModuleState = *DecodedStateValue;
		OutObservation.ModuleSnapshot = *DecodedSnapshotValue;
		return true;
	}

	static FString MakeSource(
		const FString& PropertyLine = TEXT("int Count;"),
		const FString& GlobalName = TEXT("GCacheAnswer"),
		const int32 GlobalValue = 41,
		const FString& FunctionName = TEXT("GetCacheAnswer"),
		const int32 ReturnValue = 7,
		const FString& FunctionPrefix = FString())
	{
		return FString::Printf(TEXT(R"AS(
class FCachePayload
{
	%s
}

const int %s = %d;

int %s()
{
	%sreturn %d;
}
)AS"),
			*PropertyLine,
			*GlobalName,
			GlobalValue,
			*FunctionName,
			*FunctionPrefix,
			ReturnValue);
	}

	static bool CaptureObservation(
		FAutomationTestBase& Test,
		const char* ModuleName,
		const FString& Source,
		const FString& Label,
		FCleanObservation& OutObservation,
		const FStringView TargetFunctionName = TEXT("GetCacheAnswer"))
	{
		FAngelscriptCacheCleanModuleArtifacts Artifacts;
		FAngelscriptCacheCleanCaptureResult Capture;
		if (!CaptureSource(
			Test, ModuleName, Source, Artifacts, Capture) || !Capture.IsSuccess())
		{
			Test.AddError(FString::Printf(
				TEXT("Clean-oracle %s capture failed: Error=%u Detail=%s"),
				*Label, static_cast<uint32>(Capture.Error), *Capture.Detail));
			return false;
		}
		if (!ObserveArtifacts(
			Test, Artifacts, TargetFunctionName, OutObservation))
		{
			return false;
		}
		Test.AddInfo(FString::Printf(
			TEXT("Clean-oracle %s records: Source=%s Interface=%s Type=%s State=%s Function=%s Debug=%s Snapshot=%s"),
			*Label,
			*OutObservation.SourceIndexRecordId.ContentHash.ToHexString(),
			*OutObservation.ModuleInterfaceRecordId.ContentHash.ToHexString(),
			*OutObservation.TypeSchemaRecordId.ContentHash.ToHexString(),
			*OutObservation.ModuleStateRecordId.ContentHash.ToHexString(),
			*OutObservation.FunctionBodyRecordId.ContentHash.ToHexString(),
			*OutObservation.DebugSidecarRecordId.ContentHash.ToHexString(),
			*OutObservation.ModuleSnapshotRecordId.ContentHash.ToHexString()));
		Test.AddInfo(FString::Printf(
			TEXT("Clean-oracle %s function coordinates: Key=%s Abi=%s Source=%s Input=%s Execution=%s Debug=%s"),
			*Label,
			*OutObservation.FunctionBody.Identity.FunctionKey.Hash.ToHexString(),
			*OutObservation.FunctionBody.ExpectedDeclarationAbi.ToHexString(),
			*OutObservation.FunctionBody.FunctionSourceDigest.Hash.ToHexString(),
			*OutObservation.FunctionBody.FunctionInputDigest.Hash.ToHexString(),
			*OutObservation.FunctionBody.Identity.Content.Execution.ToHexString(),
			*OutObservation.DebugSidecar.DebugHash.ToHexString()));
		return true;
	}

	static const FAngelscriptCachedDeclaration* FindDeclaration(
		const FAngelscriptCachedModuleInterface& Interface,
		const EAngelscriptCacheDeclarationKind Kind)
	{
		return Interface.Declarations.FindByPredicate(
			[Kind](const FAngelscriptCachedDeclaration& Declaration)
			{
				return Declaration.DeclarationKind == Kind;
			});
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheCleanOracleMutationTests,
	"Angelscript.TestModule.Cache.CleanOracleMutation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
public:
	TEST_METHOD(IndependentUnchangedCompilesAreRecordIdentical)
	{
		using namespace AngelscriptCacheCleanOracleMutationTests_Private;
		FCleanObservation Baseline;
		FCleanObservation Current;
		const FString Source = MakeSource();
		ASSERT_THAT(IsTrue(CaptureObservation(*TestRunner,
			"ASCacheV2CleanOracleUnchanged", Source, TEXT("unchanged-baseline"),
			Baseline)));
		ASSERT_THAT(IsTrue(CaptureObservation(*TestRunner,
			"ASCacheV2CleanOracleUnchanged", Source, TEXT("unchanged-current"),
			Current)));
		ASSERT_THAT(IsTrue(Baseline.SourceIndexRecordId
			== Current.SourceIndexRecordId));
		ASSERT_THAT(IsTrue(Baseline.ModuleInterfaceRecordId
			== Current.ModuleInterfaceRecordId));
		ASSERT_THAT(IsTrue(Baseline.TypeSchemaRecordId
			== Current.TypeSchemaRecordId));
		ASSERT_THAT(IsTrue(Baseline.ModuleStateRecordId
			== Current.ModuleStateRecordId));
		ASSERT_THAT(IsTrue(Baseline.FunctionBodyRecordId
			== Current.FunctionBodyRecordId));
		ASSERT_THAT(IsTrue(Baseline.DebugSidecarRecordId
			== Current.DebugSidecarRecordId));
		ASSERT_THAT(IsTrue(Baseline.ModuleSnapshotRecordId
			== Current.ModuleSnapshotRecordId));
	}

	TEST_METHOD(BodyOnlyMutationKeepsDeclarationAndLayoutCoordinates)
	{
		using namespace AngelscriptCacheCleanOracleMutationTests_Private;
		FCleanObservation Baseline;
		FCleanObservation Current;
		ASSERT_THAT(IsTrue(CaptureObservation(*TestRunner,
			"ASCacheV2CleanOracleBody", MakeSource(), TEXT("body-baseline"),
			Baseline)));
		ASSERT_THAT(IsTrue(CaptureObservation(*TestRunner,
			"ASCacheV2CleanOracleBody",
			MakeSource(TEXT("int Count;"), TEXT("GCacheAnswer"), 41,
				TEXT("GetCacheAnswer"), 8),
			TEXT("body-current"), Current)));
		ASSERT_THAT(IsFalse(Baseline.SourceIndexRecordId
			== Current.SourceIndexRecordId));
		ASSERT_THAT(IsTrue(Baseline.ModuleInterfaceRecordId
			== Current.ModuleInterfaceRecordId));
		ASSERT_THAT(IsTrue(Baseline.TypeSchemaRecordId
			== Current.TypeSchemaRecordId));
		ASSERT_THAT(IsTrue(Baseline.ModuleStateRecordId
			== Current.ModuleStateRecordId));
		ASSERT_THAT(IsTrue(Baseline.DebugSidecarRecordId
			== Current.DebugSidecarRecordId));
		ASSERT_THAT(IsFalse(Baseline.FunctionBodyRecordId
			== Current.FunctionBodyRecordId));
		ASSERT_THAT(IsFalse(Baseline.ModuleSnapshotRecordId
			== Current.ModuleSnapshotRecordId));
		ASSERT_THAT(IsTrue(Baseline.FunctionBody.Identity.FunctionKey
			== Current.FunctionBody.Identity.FunctionKey));
		ASSERT_THAT(IsTrue(Baseline.FunctionBody.ExpectedDeclarationAbi
			== Current.FunctionBody.ExpectedDeclarationAbi));
		ASSERT_THAT(IsFalse(Baseline.FunctionBody.FunctionSourceDigest.Hash
			== Current.FunctionBody.FunctionSourceDigest.Hash));
		ASSERT_THAT(IsFalse(Baseline.FunctionBody.FunctionInputDigest.Hash
			== Current.FunctionBody.FunctionInputDigest.Hash));
		ASSERT_THAT(IsFalse(Baseline.FunctionBody.Identity.Content.Execution
			== Current.FunctionBody.Identity.Content.Execution));
		ASSERT_THAT(IsTrue(Baseline.DebugSidecar.DebugHash
			== Current.DebugSidecar.DebugHash));
	}

	TEST_METHOD(SignatureRenameRekeysOnlyTheFunctionDeclarationFamily)
	{
		using namespace AngelscriptCacheCleanOracleMutationTests_Private;
		FCleanObservation Baseline;
		FCleanObservation Current;
		ASSERT_THAT(IsTrue(CaptureObservation(*TestRunner,
			"ASCacheV2CleanOracleSignature", MakeSource(),
			TEXT("signature-baseline"), Baseline)));
		ASSERT_THAT(IsTrue(CaptureObservation(*TestRunner,
			"ASCacheV2CleanOracleSignature",
			MakeSource(TEXT("int Count;"), TEXT("GCacheAnswer"), 41,
				TEXT("GetCacheValue")),
			TEXT("signature-current"), Current, TEXT("GetCacheValue"))));
		ASSERT_THAT(IsFalse(Baseline.SourceIndexRecordId
			== Current.SourceIndexRecordId));
		ASSERT_THAT(IsFalse(Baseline.ModuleInterfaceRecordId
			== Current.ModuleInterfaceRecordId));
		ASSERT_THAT(IsTrue(Baseline.TypeSchemaRecordId
			== Current.TypeSchemaRecordId));
		ASSERT_THAT(IsTrue(Baseline.ModuleStateRecordId
			== Current.ModuleStateRecordId));
		ASSERT_THAT(IsFalse(Baseline.FunctionBody.Identity.FunctionKey
			== Current.FunctionBody.Identity.FunctionKey));
		ASSERT_THAT(IsFalse(Baseline.FunctionBodyRecordId
			== Current.FunctionBodyRecordId));
		ASSERT_THAT(IsFalse(Baseline.DebugSidecarRecordId
			== Current.DebugSidecarRecordId));
		ASSERT_THAT(IsFalse(Baseline.ModuleSnapshotRecordId
			== Current.ModuleSnapshotRecordId));
	}

	TEST_METHOD(ClassPropertyMutationChangesOnlyTypeAndInterfaceSemantics)
	{
		using namespace AngelscriptCacheCleanOracleMutationTests_Private;
		FCleanObservation Baseline;
		FCleanObservation Current;
		ASSERT_THAT(IsTrue(CaptureObservation(*TestRunner,
			"ASCacheV2CleanOracleLayout", MakeSource(), TEXT("layout-baseline"),
			Baseline)));
		ASSERT_THAT(IsTrue(CaptureObservation(*TestRunner,
			"ASCacheV2CleanOracleLayout",
			MakeSource(TEXT("int Count; int Extra; int Third;")),
			TEXT("layout-current"), Current)));
		ASSERT_THAT(IsFalse(Baseline.SourceIndexRecordId
			== Current.SourceIndexRecordId));
		ASSERT_THAT(IsFalse(Baseline.ModuleInterfaceRecordId
			== Current.ModuleInterfaceRecordId));
		ASSERT_THAT(IsFalse(Baseline.TypeSchemaRecordId
			== Current.TypeSchemaRecordId));
		ASSERT_THAT(IsTrue(Baseline.ModuleStateRecordId
			== Current.ModuleStateRecordId));
		ASSERT_THAT(IsTrue(Baseline.FunctionBodyRecordId
			== Current.FunctionBodyRecordId));
		ASSERT_THAT(IsTrue(Baseline.DebugSidecarRecordId
			== Current.DebugSidecarRecordId));
		ASSERT_THAT(IsFalse(Baseline.ModuleSnapshotRecordId
			== Current.ModuleSnapshotRecordId));
		ASSERT_THAT(IsTrue(Baseline.TypeSchema.TypeKey
			== Current.TypeSchema.TypeKey));
		ASSERT_THAT(IsFalse(Baseline.TypeSchema.Layout.TypeLayoutHash
			== Current.TypeSchema.Layout.TypeLayoutHash));
		ASSERT_THAT(AreEqual(uint64(56), Baseline.TypeSchema.Layout.SemanticSize));
		ASSERT_THAT(AreEqual(uint64(64), Current.TypeSchema.Layout.SemanticSize));
		ASSERT_THAT(IsTrue(Baseline.FunctionBody.Identity.Content.Execution
			== Current.FunctionBody.Identity.Content.Execution));
	}

	TEST_METHOD(GlobalRenameChangesStorageIdentityWithoutTouchingTypeOrFunction)
	{
		using namespace AngelscriptCacheCleanOracleMutationTests_Private;
		FCleanObservation Baseline;
		FCleanObservation Current;
		ASSERT_THAT(IsTrue(CaptureObservation(*TestRunner,
			"ASCacheV2CleanOracleGlobal", MakeSource(), TEXT("global-baseline"),
			Baseline)));
		ASSERT_THAT(IsTrue(CaptureObservation(*TestRunner,
			"ASCacheV2CleanOracleGlobal",
			MakeSource(TEXT("int Count;"), TEXT("GCacheResult")),
			TEXT("global-current"), Current)));
		ASSERT_THAT(IsFalse(Baseline.SourceIndexRecordId
			== Current.SourceIndexRecordId));
		ASSERT_THAT(IsFalse(Baseline.ModuleInterfaceRecordId
			== Current.ModuleInterfaceRecordId));
		ASSERT_THAT(IsTrue(Baseline.TypeSchemaRecordId
			== Current.TypeSchemaRecordId));
		ASSERT_THAT(IsFalse(Baseline.ModuleStateRecordId
			== Current.ModuleStateRecordId));
		ASSERT_THAT(IsTrue(Baseline.FunctionBodyRecordId
			== Current.FunctionBodyRecordId));
		ASSERT_THAT(IsTrue(Baseline.DebugSidecarRecordId
			== Current.DebugSidecarRecordId));
		ASSERT_THAT(IsFalse(Baseline.ModuleSnapshotRecordId
			== Current.ModuleSnapshotRecordId));
		ASSERT_THAT(IsFalse(Baseline.ModuleState.OrderedGlobals[0].GlobalKey
			== Current.ModuleState.OrderedGlobals[0].GlobalKey));
		ASSERT_THAT(IsFalse(
			Baseline.ModuleState.OrderedGlobals[0].StorageLayoutFingerprint
			== Current.ModuleState.OrderedGlobals[0].StorageLayoutFingerprint));
	}

	TEST_METHOD(PureConstantValueMutationKeepsDeclarationAndStorageLayout)
	{
		using namespace AngelscriptCacheCleanOracleMutationTests_Private;
		FCleanObservation Baseline;
		FCleanObservation Current;
		ASSERT_THAT(IsTrue(CaptureObservation(*TestRunner,
			"ASCacheV2CleanOracleInitializer", MakeSource(),
			TEXT("initializer-baseline"), Baseline)));
		ASSERT_THAT(IsTrue(CaptureObservation(*TestRunner,
			"ASCacheV2CleanOracleInitializer",
			MakeSource(TEXT("int Count;"), TEXT("GCacheAnswer"), 42),
			TEXT("initializer-current"), Current)));
		ASSERT_THAT(IsFalse(Baseline.SourceIndexRecordId
			== Current.SourceIndexRecordId));
		ASSERT_THAT(IsTrue(Baseline.ModuleInterfaceRecordId
			== Current.ModuleInterfaceRecordId));
		ASSERT_THAT(IsTrue(Baseline.TypeSchemaRecordId
			== Current.TypeSchemaRecordId));
		ASSERT_THAT(IsFalse(Baseline.ModuleStateRecordId
			== Current.ModuleStateRecordId));
		ASSERT_THAT(IsTrue(Baseline.FunctionBodyRecordId
			== Current.FunctionBodyRecordId));
		ASSERT_THAT(IsTrue(Baseline.DebugSidecarRecordId
			== Current.DebugSidecarRecordId));
		ASSERT_THAT(IsFalse(Baseline.ModuleSnapshotRecordId
			== Current.ModuleSnapshotRecordId));
		ASSERT_THAT(IsTrue(Baseline.ModuleState.OrderedGlobals[0].GlobalKey
			== Current.ModuleState.OrderedGlobals[0].GlobalKey));
		ASSERT_THAT(IsTrue(
			Baseline.ModuleState.OrderedGlobals[0].StorageLayoutFingerprint
			== Current.ModuleState.OrderedGlobals[0].StorageLayoutFingerprint));
		ASSERT_THAT(IsFalse(Baseline.ModuleState.HardValues[0].HardValueHash
			== Current.ModuleState.HardValues[0].HardValueHash));
	}

	TEST_METHOD(DebugOnlyMutationKeepsExecutionAndStaticJitCoordinates)
	{
		using namespace AngelscriptCacheCleanOracleMutationTests_Private;
		FCleanObservation Baseline;
		FCleanObservation Current;
		ASSERT_THAT(IsTrue(CaptureObservation(*TestRunner,
			"ASCacheV2CleanOracleDebug", MakeSource(), TEXT("debug-baseline"),
			Baseline)));
		ASSERT_THAT(IsTrue(CaptureObservation(*TestRunner,
			"ASCacheV2CleanOracleDebug",
			MakeSource(TEXT("int Count;"), TEXT("GCacheAnswer"), 41,
				TEXT("GetCacheAnswer"), 7,
				TEXT("// debug-only line shift\n\t")),
			TEXT("debug-current"), Current)));
		ASSERT_THAT(IsFalse(Baseline.SourceIndexRecordId
			== Current.SourceIndexRecordId));
		ASSERT_THAT(IsTrue(Baseline.ModuleInterfaceRecordId
			== Current.ModuleInterfaceRecordId));
		ASSERT_THAT(IsTrue(Baseline.TypeSchemaRecordId
			== Current.TypeSchemaRecordId));
		ASSERT_THAT(IsTrue(Baseline.ModuleStateRecordId
			== Current.ModuleStateRecordId));
		ASSERT_THAT(IsFalse(Baseline.DebugSidecarRecordId
			== Current.DebugSidecarRecordId));
		ASSERT_THAT(IsFalse(Baseline.FunctionBodyRecordId
			== Current.FunctionBodyRecordId));
		ASSERT_THAT(IsFalse(Baseline.ModuleSnapshotRecordId
			== Current.ModuleSnapshotRecordId));
		ASSERT_THAT(IsTrue(Baseline.FunctionBody.Identity.FunctionKey
			== Current.FunctionBody.Identity.FunctionKey));
		ASSERT_THAT(IsTrue(Baseline.FunctionBody.ExpectedDeclarationAbi
			== Current.FunctionBody.ExpectedDeclarationAbi));
		ASSERT_THAT(IsTrue(Baseline.FunctionBody.FunctionSourceDigest.Hash
			== Current.FunctionBody.FunctionSourceDigest.Hash));
		ASSERT_THAT(IsTrue(Baseline.FunctionBody.FunctionInputDigest.Hash
			== Current.FunctionBody.FunctionInputDigest.Hash));
		ASSERT_THAT(IsTrue(Baseline.FunctionBody.Identity.Content.Execution
			== Current.FunctionBody.Identity.Content.Execution));
		ASSERT_THAT(IsFalse(Baseline.DebugSidecar.DebugHash
			== Current.DebugSidecar.DebugHash));
	}

	TEST_METHOD(ClassPropertyAndGlobalInitializerEnterProductionCapture)
	{
		using namespace AngelscriptCacheCleanOracleMutationTests_Private;

		const FString Source = TEXT(R"AS(
class FCachePayload
{
	int Count;
}

const int GCacheAnswer = 41;

int GetCacheAnswer()
{
	return 7;
}
)AS");

		FAngelscriptCacheCleanModuleArtifacts Artifacts;
		FAngelscriptCacheCleanCaptureResult Capture;
		ASSERT_THAT(IsTrue(CaptureSource(
			*TestRunner,
			"ASCacheV2CleanOracleStructuralState",
			Source,
			Artifacts,
			Capture)));
		ASSERT_THAT(IsTrue(Capture.IsSuccess(),
			TEXT("A normal class/property/global-initializer compile must reach the production cache producer")));
		ASSERT_THAT(AreEqual(
			static_cast<uint32>(Artifacts.Records.Num()),
			Capture.ValidatedGraphRecordCount));
		for (const EAngelscriptCacheRecordKind Kind : {
			EAngelscriptCacheRecordKind::SourceIndex,
			EAngelscriptCacheRecordKind::ModuleInterface,
			EAngelscriptCacheRecordKind::TypeSchema,
			EAngelscriptCacheRecordKind::ModuleState,
			EAngelscriptCacheRecordKind::ModuleSnapshot})
		{
			ASSERT_THAT(AreEqual(int32(1), CountRecords(Artifacts, Kind)));
		}
		const int32 FunctionBodyCount = CountRecords(
			Artifacts, EAngelscriptCacheRecordKind::FunctionBody);
		const int32 DebugSidecarCount = CountRecords(
			Artifacts, EAngelscriptCacheRecordKind::DebugSidecar);
		ASSERT_THAT(IsTrue(FunctionBodyCount > 0));
		ASSERT_THAT(AreEqual(FunctionBodyCount, DebugSidecarCount));

		FCleanObservation TargetObservation;
		ASSERT_THAT(IsTrue(ObserveArtifacts(
			*TestRunner, Artifacts, TEXT("GetCacheAnswer"),
			TargetObservation)));
		const FAngelscriptCachedModuleInterface* Interface =
			&TargetObservation.Interface;
		const FAngelscriptCachedTypeSchema* Schema =
			&TargetObservation.TypeSchema;
		const FAngelscriptCachedModuleState* State =
			&TargetObservation.ModuleState;
		const FAngelscriptCachedFunctionBody* Body =
			&TargetObservation.FunctionBody;

		ASSERT_THAT(IsTrue(Interface->Declarations.Num() >= 4));
		const FAngelscriptCachedDeclaration* GlobalDeclaration = nullptr;
		const FAngelscriptCachedDeclaration* FunctionDeclaration = nullptr;
		int32 GlobalDeclarationCount = 0;
		int32 FunctionDeclarationCount = 0;
		for (const FAngelscriptCachedDeclaration& Declaration
			: Interface->Declarations)
		{
			if (Declaration.DeclarationKind
					== EAngelscriptCacheDeclarationKind::Global
				&& Declaration.CanonicalName == TEXT("GCacheAnswer"))
			{
				GlobalDeclaration = &Declaration;
				++GlobalDeclarationCount;
			}
			else if (Declaration.DeclarationKind
					== EAngelscriptCacheDeclarationKind::Function
				&& Declaration.CanonicalName == TEXT("GetCacheAnswer"))
			{
				FunctionDeclaration = &Declaration;
				++FunctionDeclarationCount;
			}
			if (Declaration.DeclarationKind
				== EAngelscriptCacheDeclarationKind::Global)
			{
				ASSERT_THAT(IsFalse(Declaration.CanonicalName
					== TEXT("__StaticType_FCachePayload")));
			}
		}
		ASSERT_THAT(AreEqual(1, GlobalDeclarationCount));
		ASSERT_THAT(AreEqual(1, FunctionDeclarationCount));
		ASSERT_THAT(IsNotNull(GlobalDeclaration));
		ASSERT_THAT(IsNotNull(FunctionDeclaration));
		ASSERT_THAT(AreEqual(
			FString(TEXT("GCacheAnswer")), GlobalDeclaration->CanonicalName));
		ASSERT_THAT(AreEqual(
			FString(TEXT("GetCacheAnswer")), FunctionDeclaration->CanonicalName));

		ASSERT_THAT(AreEqual(
			EAngelscriptCachedTypeKind::Class, Schema->TypeKind));
		ASSERT_THAT(AreEqual(
			FString(TEXT("FCachePayload")), Schema->CanonicalName));
		ASSERT_THAT(AreEqual(uint64(56), Schema->Layout.SemanticSize));
		ASSERT_THAT(AreEqual(uint32(8), Schema->Layout.SemanticAlignment));
		ASSERT_THAT(AreEqual(uint32(48), Schema->Layout.BasePropertyBoundary));
		ASSERT_THAT(AreEqual(int32(1), Schema->OrderedProperties.Num()));
		const FAngelscriptCachedPropertySchema& Property =
			Schema->OrderedProperties[0];
		ASSERT_THAT(AreEqual(FString(TEXT("Count")), Property.CanonicalName));
		ASSERT_THAT(AreEqual(uint32(48), Property.SemanticByteOffset));
		ASSERT_THAT(AreEqual(uint32(4), Property.SemanticStorageSize));
		ASSERT_THAT(AreEqual(uint32(4), Property.SemanticStorageAlignment));
		ASSERT_THAT(IsTrue(Schema->Reflection.StaticClassGlobalName.IsSet()));
		ASSERT_THAT(AreEqual(
			FString(TEXT("__StaticType_FCachePayload")),
			Schema->Reflection.StaticClassGlobalName.GetValue()));

		ASSERT_THAT(AreEqual(int32(1), State->OrderedGlobals.Num()));
		ASSERT_THAT(AreEqual(int32(1), State->HardValues.Num()));
		const FAngelscriptCachedGlobalSchema& Global = State->OrderedGlobals[0];
		const FAngelscriptCachedHardValue& HardValue = State->HardValues[0];
		ASSERT_THAT(AreEqual(
			EAngelscriptCachedGlobalInitializationKind::PureConstant,
			Global.InitializationKind));
		ASSERT_THAT(AreEqual(
			EAngelscriptCachedHardValueKind::GlobalConstant,
			HardValue.HardValueKind));
		ASSERT_THAT(IsTrue(
			HardValue.Owner.StableKey == GlobalDeclaration->StableKey));
		ASSERT_THAT(IsTrue(
			HardValue.Owner.ExpectedAbi == GlobalDeclaration->SignatureHash));
		ASSERT_THAT(IsTrue(HardValue.CanonicalValue.IsSet()));
		ASSERT_THAT(AreEqual(
			EAngelscriptCachedCanonicalValueKind::SignedInteger,
			HardValue.CanonicalValue->ValueKind));
		ASSERT_THAT(AreEqual(
			int32(4), HardValue.CanonicalValue->FixedWidthValueBytes.Num()));
		ASSERT_THAT(AreEqual(
			uint8(41), HardValue.CanonicalValue->FixedWidthValueBytes[0]));
		ASSERT_THAT(AreEqual(uint8(0),
			HardValue.CanonicalValue->FixedWidthValueBytes[1]));
		ASSERT_THAT(AreEqual(uint8(0),
			HardValue.CanonicalValue->FixedWidthValueBytes[2]));
		ASSERT_THAT(AreEqual(uint8(0),
			HardValue.CanonicalValue->FixedWidthValueBytes[3]));
		ASSERT_THAT(IsTrue(
			Body->Identity.FunctionKey.Hash == FunctionDeclaration->StableKey));
	}
};

#endif
