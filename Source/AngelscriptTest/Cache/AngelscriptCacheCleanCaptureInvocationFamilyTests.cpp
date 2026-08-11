#include "Cache/AngelscriptCacheCleanCapture.h"
#include "Cache/AngelscriptCacheDecodedRecord.h"
#include "Cache/AngelscriptCacheEnvironment.h"
#include "Cache/AngelscriptCacheStableSymbolIdentity.h"

#include "CQTest.h"
#include "Shared/AngelscriptTestFixture.h"

#include "as_module.h"
#include "as_restore.h"
#include "as_scriptengine.h"
#include "as_scriptfunction.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptCacheCleanCaptureInvocationFamilyTests_Private
{
	class FArtifactProbeStream final : public asIBinaryStream
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

	static FString DescribeDependencyTarget(
		const asSBuildArtifactDependency& Dependency)
	{
		if (Dependency.type != nullptr)
		{
			return FString::Printf(TEXT("Type:%s"),
				UTF8_TO_TCHAR(Dependency.type->GetName()));
		}
		if (Dependency.function != nullptr)
		{
			return FString::Printf(TEXT("Function:%s"),
				UTF8_TO_TCHAR(Dependency.function->GetDeclaration(
					false, false, false)));
		}
		if (Dependency.globalProperty != nullptr)
		{
			return FString::Printf(TEXT("Global:%s"),
				UTF8_TO_TCHAR(Dependency.globalProperty->name.AddressOf()));
		}
		if (Dependency.objectProperty != nullptr)
		{
			return FString::Printf(TEXT("Property:%s::%s"),
				Dependency.propertyOwnerType != nullptr
					? UTF8_TO_TCHAR(Dependency.propertyOwnerType->GetName())
					: TEXT("<none>"),
				UTF8_TO_TCHAR(Dependency.objectProperty->name.AddressOf()));
		}
		return TEXT("<none>");
	}

	static FString DescribeSymbolUse(const asSFunctionArtifactSymbolUse& Use)
	{
		if (Use.type != nullptr)
		{
			return FString::Printf(TEXT("Type:%s"),
				UTF8_TO_TCHAR(Use.type->GetName()));
		}
		if (Use.function != nullptr)
		{
			return FString::Printf(TEXT("Function:%s"),
				UTF8_TO_TCHAR(Use.function->GetDeclaration(
					false, false, false)));
		}
		if (Use.globalProperty != nullptr)
		{
			return FString::Printf(TEXT("Global:%s"),
				UTF8_TO_TCHAR(Use.globalProperty->name.AddressOf()));
		}
		if (Use.objectProperty != nullptr)
		{
			return FString::Printf(TEXT("Property:%s::%s"),
				Use.propertyOwnerType != nullptr
					? UTF8_TO_TCHAR(Use.propertyOwnerType->GetName())
					: TEXT("<none>"),
				UTF8_TO_TCHAR(Use.objectProperty->name.AddressOf()));
		}
		return TEXT("<none>");
	}

	static FAngelscriptCacheCleanCaptureOptions MakeCaptureOptions()
	{
		FAngelscriptCacheCleanCaptureOptions Options;
		FAngelscriptCompatibilityDescriptor Compatibility;
		Compatibility.CanonicalInputs = {
			TEXT("CacheV2CleanCaptureInvocationFamilies"),
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

	static const TCHAR* KindName(
		const EAngelscriptCachedFunctionInvocationKind Kind)
	{
		switch (Kind)
		{
		case EAngelscriptCachedFunctionInvocationKind::GlobalFunction:
			return TEXT("GlobalFunction");
		case EAngelscriptCachedFunctionInvocationKind::Method:
			return TEXT("Method");
		case EAngelscriptCachedFunctionInvocationKind::Constructor:
			return TEXT("Constructor");
		case EAngelscriptCachedFunctionInvocationKind::Destructor:
			return TEXT("Destructor");
		case EAngelscriptCachedFunctionInvocationKind::Factory:
			return TEXT("Factory");
		case EAngelscriptCachedFunctionInvocationKind::GeneratedDefaultConstructor:
			return TEXT("GeneratedDefaultConstructor");
		case EAngelscriptCachedFunctionInvocationKind::GeneratedDefaultDestructor:
			return TEXT("GeneratedDefaultDestructor");
		case EAngelscriptCachedFunctionInvocationKind::InitDefaults:
			return TEXT("InitDefaults");
		default:
			return TEXT("Unsupported");
		}
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

	static bool ContainsHash(
		const TArray<FAngelscriptHash256>& Values,
		const FAngelscriptHash256& Value)
	{
		return Values.ContainsByPredicate(
			[&Value](const FAngelscriptHash256& Candidate)
			{
				return Candidate == Value;
			});
	}

	static bool CaptureCase(
		FAutomationTestBase& Test,
		const TCHAR* Label,
		const char* ModuleName,
		const FString& Source,
		TSet<EAngelscriptCachedFunctionInvocationKind>& OutKinds)
	{
		FAngelscriptTestFixture Fixture(Test, ETestEngineMode::IsolatedFull);
		if (!Fixture.IsValid())
		{
			return false;
		}
		asIScriptModule* PublicModule = Fixture.BuildModule(ModuleName, Source);
		TSharedPtr<FAngelscriptModuleDesc> Module =
			Fixture.GetEngine().GetModule(PublicModule);
		if (PublicModule == nullptr || !Module.IsValid())
		{
			Test.AddError(FString::Printf(
				TEXT("V5.5 production family %s did not retain its module"), Label));
			return false;
		}

		asCModule* ScriptModule = static_cast<asCModule*>(PublicModule);
		for (asUINT Index = 0;
			Index < ScriptModule->scriptFunctions.GetLength(); ++Index)
		{
			const asCScriptFunction* Function = ScriptModule->scriptFunctions[Index];
			if (Function == nullptr || Function->module != ScriptModule
				|| Function->scriptData == nullptr
				|| Function->artifactInvocationKind
					< asBUILD_ARTIFACT_INVOCATION_GLOBAL_FUNCTION
				|| Function->artifactInvocationKind
					> asBUILD_ARTIFACT_INVOCATION_INIT_DEFAULTS)
			{
				continue;
			}
			Test.AddInfo(FString::Printf(
				TEXT("V5.5 production family source: Case=%s Index=%u Kind=%u Owner=%s Declaration=%s Generated=%d CanonicalSourceBytes=%u"),
				Label,
				Index,
				static_cast<uint32>(Function->artifactInvocationKind),
				Function->artifactOwnerType != nullptr
					? UTF8_TO_TCHAR(Function->artifactOwnerType->GetName())
					: TEXT("<module>"),
				UTF8_TO_TCHAR(Function->GetDeclaration(false, false, false)),
				Function->traits.GetTrait(asTRAIT_GENERATED_FUNCTION) ? 1 : 0,
				Function->scriptData->artifactCanonicalSource.GetLength()));
			for (asUINT DependencyIndex = 0;
				DependencyIndex < Function->scriptData->artifactDependencies.GetLength();
				++DependencyIndex)
			{
				const asSBuildArtifactDependency& Dependency =
					Function->scriptData->artifactDependencies[DependencyIndex];
				Test.AddInfo(FString::Printf(
					TEXT("V5.5 production family dependency: Case=%s Function=%s Index=%u Kind=%u Ref=%u Target=%s"),
					Label,
					UTF8_TO_TCHAR(Function->GetDeclaration(false, false, false)),
					DependencyIndex,
					static_cast<uint32>(Dependency.kind),
					static_cast<uint32>(Dependency.referenceKind),
					*DescribeDependencyTarget(Dependency)));
				if (Dependency.function != nullptr
					&& Dependency.function->module == nullptr)
				{
					FAngelscriptCacheStableReference EnvironmentReference;
					const bool bEnvironmentReference =
						FAngelscriptCacheEnvironmentIdentity::
							TryBuildFunctionReference(
								*Dependency.function, EnvironmentReference);
					Test.AddInfo(FString::Printf(
						TEXT("V5.5 environment function probe: Case=%s Function=%s Dependency=%u FuncType=%u EngineMatch=%d SystemInterface=%d Owner=%s Resolved=%d Key=%s Abi=%s"),
						Label,
						UTF8_TO_TCHAR(Dependency.function->GetDeclaration(
							false, false, false)),
						DependencyIndex,
						static_cast<uint32>(Dependency.function->GetFuncType()),
						Dependency.function->engine == ScriptModule->engine ? 1 : 0,
						Dependency.function->sysFuncIntf != nullptr ? 1 : 0,
						Dependency.function->objectType != nullptr
							? UTF8_TO_TCHAR(
								Dependency.function->objectType->GetName())
							: TEXT("<none>"),
						bEnvironmentReference ? 1 : 0,
						bEnvironmentReference
							? *EnvironmentReference.StableKey.ToHexString()
							: TEXT("<none>"),
						bEnvironmentReference
							? *EnvironmentReference.ExpectedAbi.ToHexString()
							: TEXT("<none>")));
					if (bEnvironmentReference)
					{
						int32 StableCandidateCount = 0;
						int32 SameObjectCandidateCount = 0;
						for (asUINT CandidateIndex = 0;
							CandidateIndex
								< ScriptModule->engine->scriptFunctions.GetLength();
							++CandidateIndex)
						{
							const asCScriptFunction* Candidate =
								ScriptModule->engine->scriptFunctions[CandidateIndex];
							FAngelscriptCacheStableReference CandidateReference;
							if (Candidate == nullptr || Candidate->module != nullptr
								|| !FAngelscriptCacheEnvironmentIdentity::
									TryBuildFunctionReference(
										*Candidate, CandidateReference)
								|| !(CandidateReference.StableKey
									== EnvironmentReference.StableKey))
							{
								continue;
							}
							++StableCandidateCount;
							if (Candidate == Dependency.function)
							{
								++SameObjectCandidateCount;
							}
							Test.AddInfo(FString::Printf(
								TEXT("V5.5 environment function candidate: Case=%s Function=%s RegistryIndex=%u SameObject=%d AbiEqual=%d Owner=%s"),
								Label,
								UTF8_TO_TCHAR(Candidate->GetDeclaration(
									false, false, false)),
								CandidateIndex,
								Candidate == Dependency.function ? 1 : 0,
								CandidateReference.ExpectedAbi
									== EnvironmentReference.ExpectedAbi ? 1 : 0,
								Candidate->objectType != nullptr
									? UTF8_TO_TCHAR(Candidate->objectType->GetName())
									: TEXT("<none>")));
						}
						FAngelscriptCacheEngineEnvironmentResolver CurrentResolver(
							*ScriptModule->engine);
						const TOptional<FAngelscriptCacheCurrentSymbol> Current =
							CurrentResolver.Resolve(
								EAngelscriptCacheReferenceKind::EnvironmentSymbol,
								EnvironmentReference.StableKey);
						Test.AddInfo(FString::Printf(
							TEXT("V5.5 environment function resolver: Case=%s Function=%s StableCandidates=%d SameObjectCandidates=%d Current=%d AbiEqual=%d"),
							Label,
							UTF8_TO_TCHAR(Dependency.function->GetDeclaration(
								false, false, false)),
							StableCandidateCount,
							SameObjectCandidateCount,
							Current.IsSet() ? 1 : 0,
							Current.IsSet()
								&& Current->CurrentAbi
									== EnvironmentReference.ExpectedAbi ? 1 : 0));
					}
				}
			}

			FArtifactProbeStream Probe;
			asCWriter Writer(
				ScriptModule, &Probe, ScriptModule->engine, true);
			asSFunctionArtifactWriteDiagnostics WriteDiagnostics{};
			const int WriteResult = Writer.WriteFunctionArtifact(
				const_cast<asCScriptFunction*>(Function), &WriteDiagnostics);
			if (WriteResult >= 0 && !Probe.Bytes.IsEmpty())
			{
				asCReader Reader(ScriptModule, &Probe, ScriptModule->engine);
				asSFunctionArtifactValidationDiagnostics ReadDiagnostics{};
				const int ReadResult = Reader.ValidateFunctionArtifact(
					static_cast<asUINT>(Probe.Bytes.Num()), &ReadDiagnostics);
				Test.AddInfo(FString::Printf(
					TEXT("V5.5 production family artifact: Case=%s Function=%s Write=%d Read=%d Bytes=%d SymbolUses=%u"),
					Label,
					UTF8_TO_TCHAR(Function->GetDeclaration(false, false, false)),
					WriteResult,
					ReadResult,
					Probe.Bytes.Num(),
					Reader.GetFunctionArtifactSymbolUseCount()));
				for (asUINT UseIndex = 0;
					UseIndex < Reader.GetFunctionArtifactSymbolUseCount(); ++UseIndex)
				{
					const asSFunctionArtifactSymbolUse* Use =
						Reader.GetFunctionArtifactSymbolUse(UseIndex);
					if (Use != nullptr)
					{
						Test.AddInfo(FString::Printf(
							TEXT("V5.5 production family symbol use: Case=%s Function=%s Index=%u Instruction=%u Operand=%u Kind=%u Target=%s"),
							Label,
							UTF8_TO_TCHAR(Function->GetDeclaration(
								false, false, false)),
							UseIndex,
							Use->instructionOrdinal,
							Use->operandSlot,
							static_cast<uint32>(Use->kind),
							*DescribeSymbolUse(*Use)));
					}
				}
			}
		}

		FAngelscriptCacheCleanModuleArtifacts Artifacts;
		const FAngelscriptCacheCleanCaptureResult Capture =
			CaptureAngelscriptCleanCompiledModule(
				Module.ToSharedRef(), MakeCaptureOptions(), Artifacts);
		Test.AddInfo(FString::Printf(
			TEXT("V5.5 production family capture: Case=%s Error=%u Records=%d Graph=%u Detail=%s"),
			Label,
			static_cast<uint32>(Capture.Error),
			Artifacts.Records.Num(),
			Capture.ValidatedGraphRecordCount,
			*Capture.Detail));
		if (!Capture.IsSuccess())
		{
			return false;
		}

		const FAngelscriptCachedModuleInterface* Interface = nullptr;
		const FAngelscriptCachedModuleSnapshot* Snapshot = nullptr;
		TArray<const FAngelscriptCachedFunctionBody*> Bodies;
		TArray<FAngelscriptDecodedCacheRecordHandle> DecodedRecords;
		DecodedRecords.Reserve(Artifacts.Records.Num());
		TArray<FAngelscriptHash256> BodyKeys;
		int32 DebugCount = 0;
		for (const FAngelscriptPreparedRecord& Record : Artifacts.Records)
		{
			TOptional<FAngelscriptDecodedCacheRecordHandle> Decoded;
			if (!DecodeRecord(Record, Decoded))
			{
				Test.AddError(FString::Printf(
					TEXT("V5.5 production family %s could not decode record kind %u"),
					Label, static_cast<uint32>(Record.RecordId.Kind)));
				return false;
			}
			DecodedRecords.Add(Decoded.GetValue());
			const FAngelscriptDecodedCacheRecordHandle& Retained =
				DecodedRecords.Last();
			if (const FAngelscriptCachedModuleInterface* Value =
				Retained->TryGetModuleInterface())
			{
				Interface = Value;
			}
			if (const FAngelscriptCachedModuleSnapshot* Value =
				Retained->TryGetModuleSnapshot())
			{
				Snapshot = Value;
			}
			if (const FAngelscriptCachedFunctionBody* Value =
				Retained->TryGetFunctionBody())
			{
				if (ContainsHash(BodyKeys, Value->Identity.FunctionKey.Hash))
				{
					Test.AddError(FString::Printf(
						TEXT("V5.5 production family %s emitted a duplicate FunctionKey"),
						Label));
					return false;
				}
				BodyKeys.Add(Value->Identity.FunctionKey.Hash);
				Bodies.Add(Value);
				OutKinds.Add(Value->InvocationKind);
				Test.AddInfo(FString::Printf(
					TEXT("V5.5 production family body: Case=%s Kind=%s Dependencies=%d ExecutionBytes=%d Debug=%d"),
					Label,
					KindName(Value->InvocationKind),
					Value->ActualDependencies.Num(),
					Value->CanonicalExecutionPayload.Num(),
					Value->DebugSidecar.IsSet() ? 1 : 0));
			}
			if (Retained->TryGetDebugSidecar() != nullptr)
			{
				++DebugCount;
			}
		}

		if (Interface == nullptr || Snapshot == nullptr || Bodies.IsEmpty()
			|| DebugCount != Bodies.Num()
			|| Snapshot->FunctionBodies.Num() != Bodies.Num())
		{
			Test.AddError(FString::Printf(
				TEXT("V5.5 production family %s graph envelope mismatch: Interface=%d Snapshot=%d Bodies=%d Debug=%d Links=%d"),
				Label,
				Interface != nullptr ? 1 : 0,
				Snapshot != nullptr ? 1 : 0,
				Bodies.Num(),
				DebugCount,
				Snapshot != nullptr ? Snapshot->FunctionBodies.Num() : -1));
			return false;
		}

		for (const FAngelscriptCachedFunctionBody* Body : Bodies)
		{
			const FAngelscriptCachedDeclaration* Declaration =
				Interface->Declarations.FindByPredicate(
					[Body](const FAngelscriptCachedDeclaration& Candidate)
					{
						return Candidate.DeclarationKind
							== EAngelscriptCacheDeclarationKind::Function
							&& Candidate.StableKey
								== Body->Identity.FunctionKey.Hash;
					});
			if (Declaration == nullptr
				|| Declaration->SignatureHash != Body->ExpectedDeclarationAbi)
			{
				Test.AddError(FString::Printf(
					TEXT("V5.5 production family %s body has no exact Function Declaration"),
					Label));
				return false;
			}
		}
		return true;
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheCleanCaptureInvocationFamilyTests,
	"Angelscript.TestModule.Cache.CleanCaptureInvocationFamilies",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(ProductionGraphCarriesEveryStableInvocationFamily)
	{
		using namespace
			AngelscriptCacheCleanCaptureInvocationFamilyTests_Private;

		const TCHAR* GeneratedSource = TEXT(R"AS(
class FCacheGeneratedInvocationOwner
{
	int Value = 3;
	FString Label = "Generated";
	default Value = 5;

	int Read()
	{
		return Value;
	}
}

int RunGeneratedInvocation()
{
	FCacheGeneratedInvocationOwner Owner = FCacheGeneratedInvocationOwner();
	return Owner.Read();
}
)AS");
		const TCHAR* ExplicitSource = TEXT(R"AS(
class FCacheExplicitInvocationOwner
{
	int Value;

	FCacheExplicitInvocationOwner()
	{
		Value = 4;
	}

	~FCacheExplicitInvocationOwner()
	{
		Value = 0;
	}

	int Read()
	{
		return Value;
	}
}

int RunExplicitInvocation()
{
	FCacheExplicitInvocationOwner Owner = FCacheExplicitInvocationOwner();
	return Owner.Read();
}
)AS");

		TSet<EAngelscriptCachedFunctionInvocationKind> Kinds;
		ASSERT_THAT(IsTrue(CaptureCase(
			*TestRunner,
			TEXT("Generated"),
			"ASCacheV2CleanGeneratedFamilies",
			FString(GeneratedSource),
			Kinds)));
		ASSERT_THAT(IsTrue(CaptureCase(
			*TestRunner,
			TEXT("Explicit"),
			"ASCacheV2CleanExplicitFamilies",
			FString(ExplicitSource),
			Kinds)));

		const EAngelscriptCachedFunctionInvocationKind Required[] = {
			EAngelscriptCachedFunctionInvocationKind::GlobalFunction,
			EAngelscriptCachedFunctionInvocationKind::Method,
			EAngelscriptCachedFunctionInvocationKind::Constructor,
			EAngelscriptCachedFunctionInvocationKind::Destructor,
			EAngelscriptCachedFunctionInvocationKind::Factory,
			EAngelscriptCachedFunctionInvocationKind::GeneratedDefaultConstructor,
			EAngelscriptCachedFunctionInvocationKind::GeneratedDefaultDestructor,
			EAngelscriptCachedFunctionInvocationKind::InitDefaults,
		};
		for (const EAngelscriptCachedFunctionInvocationKind Kind : Required)
		{
			TestRunner->AddInfo(FString::Printf(
				TEXT("V5.5 production family coverage: Kind=%s Present=%d"),
				KindName(Kind), Kinds.Contains(Kind) ? 1 : 0));
			ASSERT_THAT(IsTrue(Kinds.Contains(Kind)));
		}
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
