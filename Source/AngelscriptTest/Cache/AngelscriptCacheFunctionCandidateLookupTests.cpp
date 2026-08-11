#include "Cache/AngelscriptCacheCleanCapture.h"
#include "Cache/AngelscriptCacheCompilerBridge.h"
#include "Cache/AngelscriptCacheSemanticRecords.h"

#include "CQTest.h"
#include "Shared/AngelscriptTestFixture.h"

#include "as_buildartifact.h"
#include "as_module.h"
#include "as_scriptfunction.h"

#if WITH_ANGELSCRIPT_UNITTESTS

namespace AngelscriptCacheFunctionCandidateLookupTests_Private
{
	static constexpr const char* ModuleName =
		"ASCacheV2FunctionCandidateLookup";
	static constexpr const char* ProducerSource = R"AS(
enum ELookupState
{
	Ready = 7,
}

int ChangedBody()
{
	return 41;
}

int UnchangedBody()
{
	return ChangedBody() + 1;
}
)AS";
	static constexpr const char* ConsumerSource = R"AS(
enum ELookupState
{
	Ready = 7,
}

int ChangedBody()
{
	return 42;
}

int UnchangedBody()
{
	return ChangedBody() + 1;
}
)AS";

	static FAngelscriptCacheCleanCaptureOptions MakeCaptureOptions()
	{
		FAngelscriptCacheCleanCaptureOptions Options;
		FAngelscriptCompatibilityDescriptor Compatibility;
		Compatibility.CanonicalInputs = {
			TEXT("CacheV2FunctionCandidateLookup"),
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

	static FAngelscriptStableFunctionKey BuildCurrentFunctionKey(
		const FAngelscriptStableModuleKey& ModuleKey,
		const asSBuildArtifactInvocation& Invocation)
	{
		FAngelscriptFunctionIdentityDescriptor Identity;
		Identity.OwnerKind = EAngelscriptFunctionOwnerKind::Module;
		Identity.OwnerKey = ModuleKey.Hash;
		Identity.Namespace = UTF8_TO_TCHAR(Invocation.nameSpace.AddressOf());
		Identity.Kind = EAngelscriptArtifactEntityKind::GlobalFunction;
		Identity.CanonicalDeclaration =
			UTF8_TO_TCHAR(Invocation.declaration.AddressOf());
		return FAngelscriptArtifactIdentityBuilder::BuildFunctionKey(Identity);
	}

	static FAngelscriptCachedDataType MakeInt32Type()
	{
		FAngelscriptCachedDataType Type;
		Type.Kind = EAngelscriptCachedDataTypeKind::Primitive;
		Type.Primitive = EAngelscriptCachedPrimitiveType::Int32;
		return Type;
	}

	// This focused adapter represents the same deliberately narrow shape admitted
	// by the two-function clean-capture vertical. It observes the consumer module's
	// declarations; it never reuses the producer graph as current authority.
	static bool BuildCurrentFunctionInterface(
		const asCModule& Module,
		const FAngelscriptStableModuleKey& ModuleKey,
		FAngelscriptCachedModuleInterface& OutInterface)
	{
		OutInterface = {};
		OutInterface.PayloadSchemaVersion =
			FAngelscriptCacheSemanticArchive::ModuleInterfacePayloadSchemaVersion;
		OutInterface.ModuleKey = ModuleKey;
		OutInterface.CanonicalModuleName = UTF8_TO_TCHAR(Module.GetName());
		for (asUINT FunctionIndex = 0;
			FunctionIndex < Module.GetFunctionCount(); ++FunctionIndex)
		{
			asCScriptFunction* Function = static_cast<asCScriptFunction*>(
				Module.GetFunctionByIndex(FunctionIndex));
			if (Function == nullptr || Function->funcType != asFUNC_SCRIPT
				|| Function->objectType != nullptr)
			{
				continue;
			}
			if (Function->GetParamCount() != 0
				|| Function->GetReturnTypeId() != asTYPEID_INT32)
			{
				return false;
			}

			FAngelscriptCachedDeclaration& Declaration =
				OutInterface.Declarations.AddDefaulted_GetRef();
			Declaration.DeclarationKind =
				EAngelscriptCacheDeclarationKind::Function;
			Declaration.EntityKind =
				EAngelscriptArtifactEntityKind::GlobalFunction;
			Declaration.SchemaCoverage =
				EAngelscriptCacheSchemaCoverage::Forbidden;
			Declaration.BodyCoverage =
				EAngelscriptCacheBodyCoverage::Required;
			Declaration.OwnerKind = EAngelscriptFunctionOwnerKind::Module;
			Declaration.OwnerKey = ModuleKey.Hash;
			Declaration.ModuleKey = ModuleKey;
			Declaration.CanonicalNamespace =
				UTF8_TO_TCHAR(Function->GetNamespace());
			Declaration.CanonicalName = UTF8_TO_TCHAR(Function->GetName());
			Declaration.CanonicalDeclaration = UTF8_TO_TCHAR(
				Function->GetDeclaration(false, false, false));
			Declaration.DeclaredType = MakeInt32Type();
			Declaration.Slots.Add({
				EAngelscriptCacheDeclarationSlotKind::Function, FunctionIndex});

			FAngelscriptFunctionIdentityDescriptor Identity;
			Identity.OwnerKind = Declaration.OwnerKind;
			Identity.OwnerKey = Declaration.OwnerKey;
			Identity.Namespace = Declaration.CanonicalNamespace;
			Identity.Kind = Declaration.EntityKind;
			Identity.CanonicalDeclaration =
				Declaration.CanonicalDeclaration;
			Declaration.StableKey =
				FAngelscriptArtifactIdentityBuilder::BuildFunctionKey(
					Identity).Hash;
			if (!FAngelscriptCacheSemanticArchive::ComputeDeclarationHashes(
				Declaration,
				Declaration.SignatureHash,
				Declaration.TraitsHash).IsSuccess())
			{
				return false;
			}
		}
		return !OutInterface.Declarations.IsEmpty();
	}

	struct FLookupObservation final
	{
		FString FunctionName;
		FAngelscriptCacheFunctionCandidateLookupResult Lookup;
		bool bCompilerInvoked = false;
		bool bCompileSucceeded = false;
		int32 CompileCallbackCount = 0;
	};

	struct FLookupContext final
	{
		const FAngelscriptValidatedModuleGraph* Graph = nullptr;
		FAngelscriptCacheCleanCaptureOptions Options;
		FAngelscriptCacheReadLimits Limits;
		FAngelscriptCacheReadBudget* Budget = nullptr;
		FAngelscriptCachedModuleInterface CurrentInterface;
		bool bSupplyCurrentAuthority = true;
		bool bCurrentAuthorityBuildFailed = false;
		TArray<FLookupObservation> Observations;

		FLookupObservation& FindOrAdd(const FString& FunctionName)
		{
			if (FLookupObservation* Existing = Observations.FindByPredicate(
				[&FunctionName](const FLookupObservation& Observation)
				{
					return Observation.FunctionName == FunctionName;
				}))
			{
				return *Existing;
			}
			FLookupObservation& Added = Observations.AddDefaulted_GetRef();
			Added.FunctionName = FunctionName;
			return Added;
		}
	};

	static asEBuildArtifactRestoreResult RestoreFromValidatedGraph(
		const asSBuildArtifactInvocation* Invocation,
		asCScriptFunction* Function,
		void* UserData)
	{
		if (Invocation == nullptr || Function == nullptr || UserData == nullptr)
		{
			return asBUILD_ARTIFACT_RESTORE_REJECTED_CORRUPT;
		}
		FLookupContext& Context = *static_cast<FLookupContext*>(UserData);
		if (Context.Graph == nullptr || Context.Budget == nullptr)
		{
			return asBUILD_ARTIFACT_RESTORE_REJECTED_CORRUPT;
		}

		const FString FunctionName =
			UTF8_TO_TCHAR(Invocation->functionName.AddressOf());
		FLookupObservation& Observation = Context.FindOrAdd(FunctionName);
		if (Context.bSupplyCurrentAuthority
			&& (Function->module == nullptr
				|| !BuildCurrentFunctionInterface(
					*Function->module,
					Context.Graph->GetModuleKey(),
					Context.CurrentInterface)))
		{
			Context.bCurrentAuthorityBuildFailed = true;
			return asBUILD_ARTIFACT_RESTORE_REJECTED_CORRUPT;
		}
		FAngelscriptCacheFunctionInputAuthorities CurrentAuthorities;
		CurrentAuthorities.ModuleInterface = Context.bSupplyCurrentAuthority
			? &Context.CurrentInterface : nullptr;
		Observation.Lookup =
			FAngelscriptCacheCompilerBridge::TryRestoreFunctionFromValidatedGraph(
				*Context.Graph,
				*Invocation,
				*Function,
				BuildCurrentFunctionKey(
					Context.Graph->GetModuleKey(), *Invocation),
				Context.Options.Profile,
				Context.Options.CanonicalCompileOptions,
				CurrentAuthorities,
				Context.Limits,
				*Context.Budget);
		return Observation.Lookup.RestoreResult;
	}

	static void ObserveCompileResult(
		const asSBuildArtifactInvocation* Invocation,
		const asSBuildArtifactCompileResult* Result,
		void* UserData)
	{
		if (Invocation == nullptr || Result == nullptr || UserData == nullptr)
		{
			return;
		}
		FLookupContext& Context = *static_cast<FLookupContext*>(UserData);
		FLookupObservation& Observation = Context.FindOrAdd(
			UTF8_TO_TCHAR(Invocation->functionName.AddressOf()));
		++Observation.CompileCallbackCount;
		Observation.bCompilerInvoked = Result->compilerInvoked;
		Observation.bCompileSucceeded = Result->succeeded;
	}

	static bool ProduceValidatedGraph(
		FAutomationTestBase& Test,
		const FAngelscriptCacheCleanCaptureOptions& Options,
		const FAngelscriptCacheReadLimits& Limits,
		FAngelscriptCacheReadBudget& Budget,
		FAngelscriptValidatedModuleGraph& OutGraph)
	{
		FAngelscriptTestFixture Producer(
			Test, ETestEngineMode::IsolatedFull);
		if (!Producer.IsValid())
		{
			return false;
		}
		asIScriptModule* ScriptModule = Producer.BuildModule(
			ModuleName, FString(ANSI_TO_TCHAR(ProducerSource)));
		if (ScriptModule == nullptr)
		{
			return false;
		}
		TSharedPtr<FAngelscriptModuleDesc> Module =
			Producer.GetEngine().GetModule(ScriptModule);
		if (!Module.IsValid())
		{
			return false;
		}

		FAngelscriptCacheCleanModuleArtifacts Artifacts;
		const FAngelscriptCacheCleanCaptureResult Capture =
			CaptureAngelscriptCleanCompiledModule(
				Module.ToSharedRef(), Options, Artifacts);
		Test.AddInfo(FString::Printf(
			TEXT("V5.4 producer capture: Error=%u Records=%d GraphRecords=%u Detail=%s"),
			static_cast<uint32>(Capture.Error),
			Artifacts.Records.Num(),
			Capture.ValidatedGraphRecordCount,
			*Capture.Detail));
		if (!Capture.IsSuccess())
		{
			return false;
		}

		const FAngelscriptCacheCleanCaptureResult Open =
			OpenAngelscriptValidatedModuleGraphFromCleanArtifacts(
				Module.ToSharedRef(), Options, Artifacts,
				Limits, Budget, OutGraph);
		Test.AddInfo(FString::Printf(
			TEXT("V5.4 producer graph: Error=%u Functions=%d Records=%d Detail=%s"),
			static_cast<uint32>(Open.Error),
			OutGraph.GetFunctionOrdinals().Num(),
			OutGraph.GetReachableRecords().Num(),
			*Open.Detail));
		return Open.IsSuccess();
	}

	static const FLookupObservation* FindObservation(
		const FLookupContext& Context,
		const TCHAR* FunctionName)
	{
		return Context.Observations.FindByPredicate(
			[FunctionName](const FLookupObservation& Observation)
			{
				return Observation.FunctionName == FunctionName;
			});
	}
}

TEST_CLASS_WITH_FLAGS(FAngelscriptCacheFunctionCandidateLookupTests,
	"Angelscript.TestModule.Cache.FunctionCandidateLookup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
{
	TEST_METHOD(IsolatedBodyEditCompilesOnlyChangedFunctionAndRestoresUnchanged)
	{
		using namespace AngelscriptCacheFunctionCandidateLookupTests_Private;
		const FAngelscriptCacheCleanCaptureOptions Options = MakeCaptureOptions();
		FAngelscriptCacheReadLimits Limits;
		FAngelscriptCacheReadBudget Budget;
		FAngelscriptValidatedModuleGraph Graph;
		ASSERT_THAT(IsTrue(ProduceValidatedGraph(
			*TestRunner, Options, Limits, Budget, Graph)));
		ASSERT_THAT(AreEqual(2, Graph.GetFunctionOrdinals().Num()));

		FLookupContext Context;
		Context.Graph = &Graph;
		Context.Options = Options;
		Context.Limits = Limits;
		Context.Budget = &Budget;

		FAngelscriptTestFixture Consumer(
			*TestRunner, ETestEngineMode::IsolatedFull);
		ASSERT_THAT(IsTrue(Consumer.IsValid()));
		asCModule* Module = static_cast<asCModule*>(
			Consumer.GetEngine().GetScriptEngine()->GetModule(
				ModuleName, asGM_ALWAYS_CREATE));
		ASSERT_THAT(IsNotNull(Module));
		Module->SetBuildArtifactRestoreCallback(
			&RestoreFromValidatedGraph, &Context);
		Module->SetBuildArtifactCompileResultCallback(
			&ObserveCompileResult, &Context);
		ASSERT_THAT(IsTrue(Module->AddScriptSection(
			"ASCacheV2FunctionCandidateLookup.as",
			ConsumerSource,
			FCStringAnsi::Strlen(ConsumerSource),
			0) >= 0));
		ASSERT_THAT(IsTrue(Module->Build() >= 0));

		asIScriptFunction* Changed =
			Module->GetFunctionByDecl("int ChangedBody()");
		asIScriptFunction* Unchanged =
			Module->GetFunctionByDecl("int UnchangedBody()");
		ASSERT_THAT(IsNotNull(Changed));
		ASSERT_THAT(IsNotNull(Unchanged));
		int32 ChangedValue = 0;
		int32 UnchangedValue = 0;
		ASSERT_THAT(IsTrue(Consumer.ExecuteInt(*Changed, ChangedValue)));
		ASSERT_THAT(IsTrue(Consumer.ExecuteInt(*Unchanged, UnchangedValue)));

		const FLookupObservation* ChangedObservation =
			FindObservation(Context, TEXT("ChangedBody"));
		const FLookupObservation* UnchangedObservation =
			FindObservation(Context, TEXT("UnchangedBody"));
		ASSERT_THAT(IsNotNull(ChangedObservation));
		ASSERT_THAT(IsNotNull(UnchangedObservation));
		TestRunner->AddInfo(FString::Printf(
			TEXT("V5.4 changed: Status=%u Restore=%u Compiler=%d Value=%d Detail=%s"),
			static_cast<uint32>(ChangedObservation->Lookup.Status),
			static_cast<uint32>(ChangedObservation->Lookup.RestoreResult),
			ChangedObservation->bCompilerInvoked ? 1 : 0,
			ChangedValue,
			*ChangedObservation->Lookup.Detail));
		TestRunner->AddInfo(FString::Printf(
			TEXT("V5.4 unchanged: Status=%u Restore=%u Compiler=%d Value=%d Detail=%s"),
			static_cast<uint32>(UnchangedObservation->Lookup.Status),
			static_cast<uint32>(UnchangedObservation->Lookup.RestoreResult),
			UnchangedObservation->bCompilerInvoked ? 1 : 0,
			UnchangedValue,
			*UnchangedObservation->Lookup.Detail));

		ASSERT_THAT(AreEqual(2, Context.Observations.Num()));
		ASSERT_THAT(IsFalse(Context.bCurrentAuthorityBuildFailed));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheFunctionCandidateLookupStatus::SourceChanged,
			ChangedObservation->Lookup.Status));
		ASSERT_THAT(AreEqual(
			asBUILD_ARTIFACT_RESTORE_MISS,
			ChangedObservation->Lookup.RestoreResult));
		ASSERT_THAT(IsTrue(ChangedObservation->bCompilerInvoked));
		ASSERT_THAT(IsTrue(ChangedObservation->bCompileSucceeded));
		ASSERT_THAT(AreEqual(1, ChangedObservation->CompileCallbackCount));
		ASSERT_THAT(AreEqual(42, ChangedValue));

		ASSERT_THAT(AreEqual(
			EAngelscriptCacheFunctionCandidateLookupStatus::Restored,
			UnchangedObservation->Lookup.Status));
		ASSERT_THAT(AreEqual(
			asBUILD_ARTIFACT_RESTORE_RESTORED,
			UnchangedObservation->Lookup.RestoreResult));
		ASSERT_THAT(IsFalse(UnchangedObservation->bCompilerInvoked));
		ASSERT_THAT(IsTrue(UnchangedObservation->bCompileSucceeded));
		ASSERT_THAT(AreEqual(1, UnchangedObservation->CompileCallbackCount));
		ASSERT_THAT(AreEqual(43, UnchangedValue));
	}

	TEST_METHOD(MissingCurrentCalleeSignatureAuthorityIsTypedMiss)
	{
		using namespace AngelscriptCacheFunctionCandidateLookupTests_Private;
		const FAngelscriptCacheCleanCaptureOptions Options = MakeCaptureOptions();
		FAngelscriptCacheReadLimits Limits;
		FAngelscriptCacheReadBudget Budget;
		FAngelscriptValidatedModuleGraph Graph;
		ASSERT_THAT(IsTrue(ProduceValidatedGraph(
			*TestRunner, Options, Limits, Budget, Graph)));

		FLookupContext Context;
		Context.Graph = &Graph;
		Context.Options = Options;
		Context.Limits = Limits;
		Context.Budget = &Budget;
		Context.bSupplyCurrentAuthority = false;

		FAngelscriptTestFixture Consumer(
			*TestRunner, ETestEngineMode::IsolatedFull);
		ASSERT_THAT(IsTrue(Consumer.IsValid()));
		asCModule* Module = static_cast<asCModule*>(
			Consumer.GetEngine().GetScriptEngine()->GetModule(
				ModuleName, asGM_ALWAYS_CREATE));
		ASSERT_THAT(IsNotNull(Module));
		Module->SetBuildArtifactRestoreCallback(
			&RestoreFromValidatedGraph, &Context);
		Module->SetBuildArtifactCompileResultCallback(
			&ObserveCompileResult, &Context);
		ASSERT_THAT(IsTrue(Module->AddScriptSection(
			"ASCacheV2FunctionCandidateLookup.as",
			ConsumerSource,
			FCStringAnsi::Strlen(ConsumerSource),
			0) >= 0));
		ASSERT_THAT(IsTrue(Module->Build() >= 0));

		const FLookupObservation* ChangedObservation =
			FindObservation(Context, TEXT("ChangedBody"));
		const FLookupObservation* UnchangedObservation =
			FindObservation(Context, TEXT("UnchangedBody"));
		ASSERT_THAT(IsNotNull(ChangedObservation));
		ASSERT_THAT(IsNotNull(UnchangedObservation));
		TestRunner->AddInfo(FString::Printf(
			TEXT("V5.4 missing authority: ChangedStatus=%u UnchangedStatus=%u Restore=%u Compiler=%d Missing=%d Detail=%s"),
			static_cast<uint32>(ChangedObservation->Lookup.Status),
			static_cast<uint32>(UnchangedObservation->Lookup.Status),
			static_cast<uint32>(UnchangedObservation->Lookup.RestoreResult),
			UnchangedObservation->bCompilerInvoked ? 1 : 0,
			UnchangedObservation->Lookup.MissingDependencyOrdinal.IsSet()
				? static_cast<int32>(UnchangedObservation->Lookup.
					MissingDependencyOrdinal.GetValue()) : -1,
			*UnchangedObservation->Lookup.Detail));

		ASSERT_THAT(AreEqual(
			EAngelscriptCacheFunctionCandidateLookupStatus::SourceChanged,
			ChangedObservation->Lookup.Status));
		ASSERT_THAT(AreEqual(
			EAngelscriptCacheFunctionCandidateLookupStatus::DependencyMissing,
			UnchangedObservation->Lookup.Status));
		ASSERT_THAT(AreEqual(
			asBUILD_ARTIFACT_RESTORE_MISS,
			UnchangedObservation->Lookup.RestoreResult));
		ASSERT_THAT(IsTrue(
			UnchangedObservation->Lookup.MissingDependencyOrdinal.IsSet()));
		ASSERT_THAT(AreEqual(uint32(0),
			UnchangedObservation->Lookup.MissingDependencyOrdinal.GetValue()));
		ASSERT_THAT(IsTrue(UnchangedObservation->bCompilerInvoked));
		ASSERT_THAT(IsTrue(UnchangedObservation->bCompileSucceeded));
	}
};

#endif // WITH_ANGELSCRIPT_UNITTESTS
