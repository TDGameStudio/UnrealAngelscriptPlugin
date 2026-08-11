#include "Cache/AngelscriptCacheCompileReuse.h"

#include "Cache/AngelscriptCacheCompilerBridge.h"
#include "Cache/AngelscriptCacheCurrentModuleAuthority.h"
#include "Cache/AngelscriptCacheEnvironment.h"
#include "Cache/AngelscriptCacheService.h"
#include "Cache/AngelscriptCacheStableSymbolIdentity.h"
#include "Cache/AngelscriptFunctionArtifactCodec.h"
#include "Core/AngelscriptEngine.h"
#include "HAL/PlatformTime.h"
#include "Misc/ScopeLock.h"

#include "as_module.h"
#include "as_scriptengine.h"

namespace AngelscriptCacheCompileReuse_Private
{
	static FAngelscriptCacheCompileReusePrepareResult Failure(
		const EAngelscriptCacheCompileReusePrepareError Error,
		FString Detail,
		const TOptional<FAngelscriptCacheValidationResult>& Validation = {})
	{
		FAngelscriptCacheCompileReusePrepareResult Result;
		Result.Error = Error;
		Result.Detail = MoveTemp(Detail);
		Result.Validation = Validation;
		return Result;
	}

	static const FAngelscriptDecodedCacheRecord* FindRecord(
		const FAngelscriptValidatedGeneration& Generation,
		const FAngelscriptCacheRecordId& RecordId)
	{
		for (const FAngelscriptDecodedCacheRecordHandle& Record
			: Generation.ReachableRecords)
		{
			if (Record->GetRecordId() == RecordId)
			{
				return &Record.Get();
			}
		}
		return nullptr;
	}

	static EAngelscriptCacheDecisionOutcome MapOutcome(
		const EAngelscriptCacheFunctionCandidateLookupStatus Status)
	{
		switch (Status)
		{
		case EAngelscriptCacheFunctionCandidateLookupStatus::Restored:
			return EAngelscriptCacheDecisionOutcome::Restored;
		case EAngelscriptCacheFunctionCandidateLookupStatus::RejectedCorrupt:
			return EAngelscriptCacheDecisionOutcome::Rejected;
		case EAngelscriptCacheFunctionCandidateLookupStatus::NotCacheable:
			return EAngelscriptCacheDecisionOutcome::NotCacheable;
		default:
			return EAngelscriptCacheDecisionOutcome::Miss;
		}
	}

	static FString BuildInvocationDetail(
		const asSBuildArtifactInvocation& Invocation,
		const asCScriptFunction& Function,
		const FString& OutcomeDetail)
	{
		return FString::Printf(
			TEXT("InvocationKind=%u Generated=%d Declaration=%s; %s"),
			static_cast<uint32>(Invocation.kind),
			Invocation.isGenerated ? 1 : 0,
			UTF8_TO_TCHAR(Function.GetDeclaration(false, false, false)),
			*OutcomeDetail);
	}

	static bool IsDerivedStaticClassHelper(
		const asSBuildArtifactInvocation& Invocation,
		const asCScriptFunction& Function)
	{
		return Invocation.kind
				== asBUILD_ARTIFACT_INVOCATION_GLOBAL_FUNCTION
			&& Function.GetObjectType() == nullptr
			&& FCStringAnsi::Strcmp(Function.GetName(), "StaticClass") == 0
			&& FCStringAnsi::Strcmp(
				Function.GetDeclaration(false, false, false),
				"UClass StaticClass()") == 0
			&& Function.traits.GetTrait(asTRAIT_GENERATED_FUNCTION);
	}
}

struct FAngelscriptCacheCompileReuseContext::FImpl
{
	struct FModuleState final
	{
		FImpl* Owner = nullptr;
		FAngelscriptStableModuleKey ModuleKey;
		FAngelscriptValidatedModuleGraph Graph;
		FAngelscriptCachedModuleInterface CurrentInterface;
		TArray<FAngelscriptCachedTypeSchema> CurrentTypes;
		FAngelscriptCachedModuleState CurrentState;
		TArray<FAngelscriptCachedFunctionBody> CurrentFunctions;
		TUniquePtr<FAngelscriptCacheEngineEnvironmentResolver> ExternalSymbols;
		FAngelscriptCacheService* Service = nullptr;
		mutable FCriticalSection RestoredDependenciesMutex;
		TMap<FBlake3Hash, TArray<FAngelscriptCacheSemanticDependency>>
			RestoredActualDependencies;
	};

	TUniquePtr<FAngelscriptCacheReadSession> Session;
	FAngelscriptHash256 GenerationId;
	FAngelscriptCacheCleanCaptureOptions Options;
	FAngelscriptCacheReadLimits Limits;
	FAngelscriptCacheReadBudget Budget;
	TArray<TUniquePtr<FModuleState>> Modules;
	mutable FCriticalSection SummaryMutex;
	FAngelscriptCacheFunctionReuseSummary Summary;

	static void RecordLookup(
		FModuleState& State,
		const asSBuildArtifactInvocation& Invocation,
		const asCScriptFunction& Function,
		const FAngelscriptStableFunctionKey& FunctionKey,
		const FAngelscriptCacheFunctionCandidateLookupResult& Lookup,
		const uint64 ElapsedMicroseconds)
	{
		if (State.Owner != nullptr)
		{
			FScopeLock SummaryLock(&State.Owner->SummaryMutex);
			switch (Lookup.Status)
			{
			case EAngelscriptCacheFunctionCandidateLookupStatus::Restored:
				++State.Owner->Summary.RestoredFunctionCount;
				break;
			case EAngelscriptCacheFunctionCandidateLookupStatus::NotCacheable:
				++State.Owner->Summary.NotCacheableCount;
				break;
			case EAngelscriptCacheFunctionCandidateLookupStatus::RejectedCorrupt:
				++State.Owner->Summary.RejectedCorruptCount;
				break;
			default:
				break;
			}
		}
		if (State.Service == nullptr)
		{
			return;
		}
		FAngelscriptCacheDecisionEvent Event;
		Event.Stage = EAngelscriptCacheDecisionStage::FunctionLookup;
		Event.Outcome = AngelscriptCacheCompileReuse_Private::MapOutcome(
			Lookup.Status);
		Event.ReasonDomain =
			EAngelscriptCacheDecisionReasonDomain::FunctionLookup;
		Event.ReasonCode = static_cast<uint32>(Lookup.Status);
		Event.ModuleKeys.Add(State.ModuleKey);
		Event.FunctionKey = FunctionKey;
		Event.ExpectedCoordinate = State.Owner->GenerationId;
		if (!Lookup.CurrentInputDigest.Hash.IsZero())
		{
			Event.CurrentCoordinate = Lookup.CurrentInputDigest.Hash;
		}
		else if (!Lookup.CurrentSourceDigest.Hash.IsZero())
		{
			Event.CurrentCoordinate = Lookup.CurrentSourceDigest.Hash;
		}
		Event.Profile = State.Owner->Options.Profile;
		Event.SourceSnapshot = State.Owner->Session->GetGeneration().
			Manifest.SourceSnapshot;
		Event.ElapsedMicroseconds = ElapsedMicroseconds;
		Event.Detail = AngelscriptCacheCompileReuse_Private::
			BuildInvocationDetail(Invocation, Function, Lookup.Detail);
		State.Service->RecordDecisionEvent(MoveTemp(Event));
	}

	static asEBuildArtifactRestoreResult RestoreFunction(
		const asSBuildArtifactInvocation* Invocation,
		asCScriptFunction* Function,
		void* UserData)
	{
		if (Invocation == nullptr || Function == nullptr || UserData == nullptr)
		{
			return asBUILD_ARTIFACT_RESTORE_REJECTED_CORRUPT;
		}
		FModuleState& State = *static_cast<FModuleState*>(UserData);
		if (State.Owner == nullptr || !State.Owner->Session.IsValid())
		{
			return asBUILD_ARTIFACT_RESTORE_REJECTED_CORRUPT;
		}

		const double StartedSeconds = FPlatformTime::Seconds();
		FAngelscriptStableFunctionKey FunctionKey;
		FString KeyFailure;
		if (!FAngelscriptCacheStableSymbolIdentity::TryBuildFunctionKey(
			State.ModuleKey, *Function, FunctionKey, &KeyFailure))
		{
			FAngelscriptCacheFunctionCandidateLookupResult Lookup;
			Lookup.Status =
				EAngelscriptCacheFunctionCandidateLookupStatus::NotCacheable;
			Lookup.RestoreResult = asBUILD_ARTIFACT_RESTORE_NOT_CACHEABLE;
			Lookup.Detail = MoveTemp(KeyFailure);
			RecordLookup(State, *Invocation, *Function, FunctionKey, Lookup,
				static_cast<uint64>(
				(FPlatformTime::Seconds() - StartedSeconds) * 1000000.0));
			return Lookup.RestoreResult;
		}
		if (AngelscriptCacheCompileReuse_Private::IsDerivedStaticClassHelper(
			*Invocation, *Function))
		{
			FAngelscriptCacheFunctionCandidateLookupResult Lookup;
			Lookup.Status =
				EAngelscriptCacheFunctionCandidateLookupStatus::NotCacheable;
			Lookup.RestoreResult = asBUILD_ARTIFACT_RESTORE_NOT_CACHEABLE;
			Lookup.Detail = TEXT(
				"Derived StaticClass helper is reconstructed from current type/ClassGenerator authority and has no persisted FunctionBody");
			RecordLookup(State, *Invocation, *Function, FunctionKey, Lookup,
				static_cast<uint64>(
					(FPlatformTime::Seconds() - StartedSeconds) * 1000000.0));
			return Lookup.RestoreResult;
		}

		FAngelscriptCacheFunctionInputAuthorities Authorities;
		Authorities.ModuleInterface = &State.CurrentInterface;
		Authorities.TypeSchemas = State.CurrentTypes;
		Authorities.ModuleState = &State.CurrentState;
		Authorities.FunctionBodies = State.CurrentFunctions;
		Authorities.ExternalSymbols = State.ExternalSymbols.Get();
		const FAngelscriptCacheFunctionCandidateLookupResult Lookup =
			FAngelscriptCacheCompilerBridge::TryRestoreFunctionFromValidatedGraph(
				State.Graph,
				*Invocation,
				*Function,
				FunctionKey,
				State.Owner->Options.Profile,
				State.Owner->Options.CanonicalCompileOptions,
				Authorities,
				State.Owner->Limits,
				State.Owner->Budget);
		if (Lookup.Status
			== EAngelscriptCacheFunctionCandidateLookupStatus::Restored)
		{
			FScopeLock Lock(&State.RestoredDependenciesMutex);
			State.RestoredActualDependencies.Add(
				FunctionKey.Hash.Value,
				Lookup.RestoredActualDependencies);
		}
		RecordLookup(State, *Invocation, *Function, FunctionKey, Lookup,
			static_cast<uint64>(
			(FPlatformTime::Seconds() - StartedSeconds) * 1000000.0));
		return Lookup.RestoreResult;
	}

	static void ObserveCompileResult(
		const asSBuildArtifactInvocation* Invocation,
		const asSBuildArtifactCompileResult* Result,
		void* UserData)
	{
		if (Invocation == nullptr || Result == nullptr || UserData == nullptr
			|| Result->function == nullptr || !Result->compilerInvoked)
		{
			return;
		}
		FModuleState& State = *static_cast<FModuleState*>(UserData);
		if (State.Service == nullptr || State.Owner == nullptr)
		{
			return;
		}
		FAngelscriptStableFunctionKey FunctionKey;
		FString KeyFailure;
		if (!FAngelscriptCacheStableSymbolIdentity::TryBuildFunctionKey(
			State.ModuleKey, *Result->function, FunctionKey, &KeyFailure))
		{
			return;
		}
		{
			FScopeLock SummaryLock(&State.Owner->SummaryMutex);
			++State.Owner->Summary.CompiledMissCount;
		}
		FAngelscriptCacheDecisionEvent Event;
		Event.Stage = EAngelscriptCacheDecisionStage::FunctionLookup;
		Event.Outcome = EAngelscriptCacheDecisionOutcome::Compiled;
		Event.ReasonDomain =
			EAngelscriptCacheDecisionReasonDomain::FunctionLookup;
		Event.ReasonCode = static_cast<uint32>(Result->restoreResult);
		Event.ModuleKeys.Add(State.ModuleKey);
		Event.FunctionKey = FunctionKey;
		Event.ExpectedCoordinate = State.Owner->GenerationId;
		Event.Profile = State.Owner->Options.Profile;
		Event.SourceSnapshot = State.Owner->Session->GetGeneration().
			Manifest.SourceSnapshot;
		Event.PrimaryCount = 1;
		const FString OutcomeDetail = Result->succeeded
			? TEXT("Candidate miss compiled successfully")
			: TEXT("Candidate miss compiler invocation failed");
		Event.Detail = AngelscriptCacheCompileReuse_Private::
			BuildInvocationDetail(
				*Invocation, *Result->function, OutcomeDetail);
		State.Service->RecordDecisionEvent(MoveTemp(Event));
	}
};

FAngelscriptCacheCompileReuseContext::FAngelscriptCacheCompileReuseContext()
	: Impl(MakeUnique<FImpl>())
{
}

FAngelscriptCacheCompileReuseContext::~FAngelscriptCacheCompileReuseContext() =
	default;

TUniquePtr<FAngelscriptCacheCompileReuseContext>
FAngelscriptCacheCompileReuseContext::Create(
	TUniquePtr<FAngelscriptCacheReadSession>&& Session,
	const FAngelscriptHash256& GenerationId,
	const FAngelscriptCacheCleanCaptureOptions& CurrentOptions)
{
	if (!Session.IsValid() || GenerationId.IsZero()
		|| Session->GetGenerationId() != GenerationId
		|| CurrentOptions.Profile.Hash.IsZero()
		|| Session->GetGeneration().Manifest.Profile.Hash
			!= CurrentOptions.Profile.Hash)
	{
		return nullptr;
	}
	TUniquePtr<FAngelscriptCacheCompileReuseContext> Context(
		new FAngelscriptCacheCompileReuseContext());
	Context->Impl->Session = MoveTemp(Session);
	Context->Impl->GenerationId = GenerationId;
	Context->Impl->Options = CurrentOptions;
	Context->Impl->Summary.bPresent = true;
	Context->Impl->Summary.CandidateGenerationId = GenerationId;
	return Context;
}

bool FAngelscriptCacheCompileReuseContext::IsValid() const
{
	return Impl.IsValid() && Impl->Session.IsValid()
		&& !Impl->GenerationId.IsZero();
}

FAngelscriptCacheFunctionReuseSummary
FAngelscriptCacheCompileReuseContext::CaptureSummary() const
{
	if (!Impl.IsValid())
	{
		return {};
	}
	FScopeLock SummaryLock(&Impl->SummaryMutex);
	return Impl->Summary;
}

bool FAngelscriptCacheCompileReuseContext::TryCopyActualDependencies(
	const FAngelscriptStableModuleKey& ModuleKey,
	const FAngelscriptStableFunctionKey& FunctionKey,
	TArray<FAngelscriptCacheSemanticDependency>& OutDependencies) const
{
	OutDependencies.Reset();
	if (!IsValid() || ModuleKey.Hash.IsZero() || FunctionKey.Hash.IsZero())
	{
		return false;
	}
	for (const TUniquePtr<FImpl::FModuleState>& State : Impl->Modules)
	{
		if (!State.IsValid() || State->ModuleKey != ModuleKey)
		{
			continue;
		}
		FScopeLock Lock(&State->RestoredDependenciesMutex);
		const TArray<FAngelscriptCacheSemanticDependency>* Dependencies =
			State->RestoredActualDependencies.Find(FunctionKey.Hash.Value);
		if (Dependencies == nullptr)
		{
			return false;
		}
		OutDependencies = *Dependencies;
		return true;
	}
	return false;
}

FAngelscriptCacheCompileReusePrepareResult
FAngelscriptCacheCompileReuseContext::PrepareModule(
	const TSharedRef<FAngelscriptModuleDesc>& Module,
	const FAngelscriptStableModuleKey& ModuleKey,
	const FAngelscriptHash256& CurrentSourceSnapshot,
	FAngelscriptCacheService* Service)
{
	using namespace AngelscriptCacheCompileReuse_Private;
	if (!IsValid() || ModuleKey.Hash.IsZero()
		|| Module->ScriptModule == nullptr
		|| Module->ScriptModule->engine == nullptr)
	{
		return Failure(EAngelscriptCacheCompileReusePrepareError::InvalidInput,
			TEXT("Compile reuse requires a live staged module and selected Generation"));
	}
	const FAngelscriptValidatedGeneration& Generation =
		Impl->Session->GetGeneration();
	if (CurrentSourceSnapshot.IsZero())
	{
		return Failure(
			EAngelscriptCacheCompileReusePrepareError::InvalidInput,
			TEXT("Compile reuse requires a nonzero current SourceSnapshot"));
	}
	if (Impl->Modules.ContainsByPredicate(
		[&ModuleKey](const TUniquePtr<FImpl::FModuleState>& Existing)
		{
			return Existing.IsValid() && Existing->ModuleKey == ModuleKey;
		}))
	{
		return Failure(EAngelscriptCacheCompileReusePrepareError::InvalidInput,
			TEXT("The same module was prepared twice for one compile reuse transaction"));
	}

	const FAngelscriptCacheModuleSnapshotLink* Link =
		Generation.Manifest.ModuleSnapshots.FindByPredicate(
			[&ModuleKey](const FAngelscriptCacheModuleSnapshotLink& Candidate)
			{
				return Candidate.ModuleKey == ModuleKey;
			});
	if (Link == nullptr)
	{
		return Failure(
			EAngelscriptCacheCompileReusePrepareError::CandidateModuleMissing,
			TEXT("The selected Generation has no candidate for the current module"));
	}
	const FAngelscriptDecodedCacheRecord* SourceIndex = FindRecord(
		Generation, Generation.Manifest.SourceIndexRecordId);
	if (SourceIndex == nullptr || SourceIndex->TryGetSourceIndex() == nullptr)
	{
		return Failure(
			EAngelscriptCacheCompileReusePrepareError::GraphValidationFailed,
			TEXT("The selected Generation has no reachable SourceIndex authority"));
	}

	asCScriptEngine& ScriptEngine = *Module->ScriptModule->engine;
	asCModule ValidationModule("__CacheV2CompileReuseValidation", &ScriptEngine);
	FAngelscriptCacheEngineEnvironmentResolver CurrentSymbols(ScriptEngine);
	FAngelscriptCacheEngineLayoutResolver CurrentLayouts(ScriptEngine);
	FAngelscriptFunctionArtifactCodec OpaqueValidator(
		ValidationModule, ScriptEngine);
	FAngelscriptCacheModuleGraphValidationContext GraphContext;
	GraphContext.SelectedProfile = Generation.Manifest.Profile;
	GraphContext.SelectedSourceSnapshot = Generation.Manifest.SourceSnapshot;
	GraphContext.SourceIndex = SourceIndex;
	GraphContext.CurrentSymbols = &CurrentSymbols;
	GraphContext.CurrentLayouts = &CurrentLayouts;
	GraphContext.OpaquePayloads = &OpaqueValidator;

	TUniquePtr<FImpl::FModuleState> State =
		MakeUnique<FImpl::FModuleState>();
	State->Owner = Impl.Get();
	State->ModuleKey = ModuleKey;
	State->Service = Service;
	const FAngelscriptCacheValidationResult GraphResult =
		ValidateModuleSnapshotGraph(
			Link->RecordId,
			Generation.ReachableRecords,
			GraphContext,
			Impl->Limits,
			Impl->Budget,
			State->Graph);
	if (!GraphResult.IsSuccess())
	{
		return Failure(
			EAngelscriptCacheCompileReusePrepareError::GraphValidationFailed,
			FString::Printf(
				TEXT("Compile-reuse graph validation failed: Error=%u Stage=%u Offset=%llu Detail=%s"),
				static_cast<uint32>(GraphResult.Error),
				static_cast<uint32>(GraphResult.Stage),
				GraphResult.ByteOffset,
				*OpaqueValidator.GetLastExecutionFailureDetail()),
			GraphResult);
	}

	const bool bExactSource =
		Generation.Manifest.SourceSnapshot == CurrentSourceSnapshot;
	if (bExactSource)
	{
		bool bHasInterface = false;
		bool bHasState = false;
		for (const FAngelscriptDecodedCacheRecordHandle& Record
			: State->Graph.GetReachableRecords())
		{
			if (const FAngelscriptCachedModuleInterface* Interface =
				Record->TryGetModuleInterface())
			{
				if (bHasInterface)
				{
					return Failure(
						EAngelscriptCacheCompileReusePrepareError::CurrentAuthorityMissing,
						TEXT("The candidate graph contains more than one ModuleInterface"));
				}
				State->CurrentInterface = *Interface;
				bHasInterface = true;
			}
			else if (const FAngelscriptCachedTypeSchema* Type =
				Record->TryGetTypeSchema())
			{
				State->CurrentTypes.Add(*Type);
			}
			else if (const FAngelscriptCachedModuleState* ModuleState =
				Record->TryGetModuleState())
			{
				if (bHasState)
				{
					return Failure(
						EAngelscriptCacheCompileReusePrepareError::CurrentAuthorityMissing,
						TEXT("The candidate graph contains more than one ModuleState"));
				}
				State->CurrentState = *ModuleState;
				bHasState = true;
			}
			else if (const FAngelscriptCachedFunctionBody* Function =
				Record->TryGetFunctionBody())
			{
				State->CurrentFunctions.Add(*Function);
			}
		}
		if (!bHasInterface || !bHasState
			|| State->CurrentInterface.ModuleKey != ModuleKey
			|| State->CurrentState.ModuleKey != ModuleKey)
		{
			return Failure(
				EAngelscriptCacheCompileReusePrepareError::CurrentAuthorityMissing,
				TEXT("The candidate graph omitted its exact-source interface/state authority"));
		}
	}
	else
	{
		FAngelscriptCacheCurrentModuleAuthority CurrentAuthority;
		const FAngelscriptCacheCleanCaptureResult AuthorityResult =
			BuildAngelscriptCacheCurrentModuleAuthority(
				Module, Impl->Options, ModuleKey, CurrentAuthority);
		if (!AuthorityResult.IsSuccess())
		{
			return Failure(
				EAngelscriptCacheCompileReusePrepareError::CurrentAuthorityMissing,
				FString::Printf(
					TEXT("Current pre-compile authority rejected module: Error=%u Detail=%s"),
					static_cast<uint32>(AuthorityResult.Error),
					*AuthorityResult.Detail));
		}
		State->CurrentInterface = MoveTemp(CurrentAuthority.ModuleInterface);
		State->CurrentTypes = MoveTemp(CurrentAuthority.TypeSchemas);
		State->CurrentState = MoveTemp(CurrentAuthority.ModuleState);
		// Function-content authority is populated only after a current function
		// has restored or compiled. Never seed it from a changed Generation.
		State->CurrentFunctions.Reset();
	}
	State->ExternalSymbols =
		MakeUnique<FAngelscriptCacheEngineEnvironmentResolver>(ScriptEngine);

	FImpl::FModuleState* CallbackState = State.Get();
	Impl->Modules.Add(MoveTemp(State));
	{
		FScopeLock SummaryLock(&Impl->SummaryMutex);
		++Impl->Summary.CandidateModuleCount;
	}
	Module->ScriptModule->SetBuildArtifactRestoreCallback(
		&FImpl::RestoreFunction, CallbackState);
	Module->ScriptModule->SetBuildArtifactCompileResultCallback(
		&FImpl::ObserveCompileResult, CallbackState);

	FAngelscriptCacheCompileReusePrepareResult Result;
	Result.Detail = FString::Printf(
		TEXT("Prepared %s module %s with %d graph-validated function candidates"),
		bExactSource ? TEXT("exact-source") : TEXT("changed-source"),
		*Module->ModuleName,
		CallbackState->Graph.GetFunctionOrdinals().Num());
	return Result;
}
