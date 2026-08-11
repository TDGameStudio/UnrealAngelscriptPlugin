#include "Cache/AngelscriptCacheCompilerBridge.h"

#include "Cache/AngelscriptFunctionArtifactCodec.h"
#include "Cache/AngelscriptCacheRemainingRecordTypes.h"
#include "Cache/AngelscriptCacheSemanticRecords.h"
#include "Cache/AngelscriptCacheTypeSchema.h"
#include "Core/Artifacts/AngelscriptArtifactIdentity.h"

#include "as_scriptfunction.h"
#include "as_module.h"
#include "as_typeinfo.h"

namespace AngelscriptCacheCompilerBridge_Private
{
	struct FCanonicalDependency final
	{
		FAngelscriptCacheSemanticDependency Dependency;
		TArray<uint8> Bytes;
	};

	static bool BytesLess(
		const TArray<uint8>& Left,
		const TArray<uint8>& Right)
	{
		const int32 Common = FMath::Min(Left.Num(), Right.Num());
		for (int32 Index = 0; Index < Common; ++Index)
		{
			if (Left[Index] != Right[Index])
			{
				return Left[Index] < Right[Index];
			}
		}
		return Left.Num() < Right.Num();
	}

	static bool SameDependencyIdentity(
		const FAngelscriptCacheSemanticDependency& Left,
		const FAngelscriptCacheSemanticDependency& Right)
	{
		return Left.Kind == Right.Kind
			&& Left.Target.Kind == Right.Target.Kind
			&& Left.Target.StableKey == Right.Target.StableKey;
	}

	static TOptional<EAngelscriptArtifactEntityKind> MapInvocationKind(
		const asEBuildArtifactInvocationKind Kind)
	{
		switch (Kind)
		{
		case asBUILD_ARTIFACT_INVOCATION_GLOBAL_FUNCTION:
			return EAngelscriptArtifactEntityKind::GlobalFunction;
		case asBUILD_ARTIFACT_INVOCATION_METHOD:
			return EAngelscriptArtifactEntityKind::Method;
		case asBUILD_ARTIFACT_INVOCATION_CONSTRUCTOR:
			return EAngelscriptArtifactEntityKind::Constructor;
		case asBUILD_ARTIFACT_INVOCATION_DESTRUCTOR:
			return EAngelscriptArtifactEntityKind::Destructor;
		case asBUILD_ARTIFACT_INVOCATION_FACTORY:
			return EAngelscriptArtifactEntityKind::Factory;
		case asBUILD_ARTIFACT_INVOCATION_GENERATED_DEFAULT_CONSTRUCTOR:
			return EAngelscriptArtifactEntityKind::GeneratedDefaultConstructor;
		case asBUILD_ARTIFACT_INVOCATION_GENERATED_DEFAULT_DESTRUCTOR:
			return EAngelscriptArtifactEntityKind::GeneratedDefaultDestructor;
		case asBUILD_ARTIFACT_INVOCATION_INIT_DEFAULTS:
			return EAngelscriptArtifactEntityKind::InitDefaults;
		default:
			return {};
		}
	}

	static const FAngelscriptCachedDeclaration* FindDeclaration(
		const FAngelscriptCachedModuleInterface* Interface,
		const FAngelscriptHash256& StableKey)
	{
		return Interface != nullptr
			? Interface->Declarations.FindByPredicate(
				[&StableKey](const FAngelscriptCachedDeclaration& Declaration)
				{
					return Declaration.StableKey == StableKey;
				})
			: nullptr;
	}

	static TOptional<FAngelscriptCacheCurrentSymbol> ResolveLocalSymbol(
		const FAngelscriptCacheSemanticDependency& Dependency,
		const FAngelscriptCacheFunctionInputAuthorities& Authorities)
	{
		const FAngelscriptHash256& Key = Dependency.Target.StableKey;
		const FAngelscriptCachedDeclaration* Declaration = FindDeclaration(
			Authorities.ModuleInterface, Key);

		switch (Dependency.Kind)
		{
		case EAngelscriptCacheSemanticDependencyKind::ValueLayout:
			if (Dependency.Target.Kind ==
					EAngelscriptCacheReferenceKind::ScriptType
				&& Declaration != nullptr)
			{
				const FAngelscriptCachedTypeSchema* Type =
					Authorities.TypeSchemas.FindByPredicate(
						[&Key](const FAngelscriptCachedTypeSchema& Candidate)
						{
							return Candidate.TypeKey.Hash == Key;
						});
				if (Type != nullptr)
				{
					return FAngelscriptCacheCurrentSymbol{
						Declaration->SignatureHash,
						Type->Layout.TypeLayoutHash,
						{}};
				}
			}
			break;

		case EAngelscriptCacheSemanticDependencyKind::Inheritance:
			if (Dependency.Target.Kind
					== EAngelscriptCacheReferenceKind::ScriptType
				&& Declaration != nullptr)
			{
				return FAngelscriptCacheCurrentSymbol{
					Declaration->SignatureHash, {}, {}};
			}
			break;

		case EAngelscriptCacheSemanticDependencyKind::PropertyLayout:
			if (Declaration == nullptr)
			{
				break;
			}
			for (const FAngelscriptCachedTypeSchema& Type : Authorities.TypeSchemas)
			{
				const FAngelscriptCachedPropertySchema* Property =
					Type.OrderedProperties.FindByPredicate(
						[&Key](const FAngelscriptCachedPropertySchema& Candidate)
						{
							return Candidate.PropertyKey.Hash == Key;
						});
				if (Property != nullptr)
				{
					return FAngelscriptCacheCurrentSymbol{
						Declaration->SignatureHash,
						Property->PropertyLayoutFingerprint,
						{}};
				}
			}
			break;

		case EAngelscriptCacheSemanticDependencyKind::GlobalStorage:
			if (Authorities.ModuleState != nullptr && Declaration != nullptr)
			{
				const FAngelscriptCachedGlobalSchema* Global =
					Authorities.ModuleState->OrderedGlobals.FindByPredicate(
						[&Key](const FAngelscriptCachedGlobalSchema& Candidate)
						{
							return Candidate.GlobalKey.Hash == Key;
						});
				if (Global != nullptr)
				{
					return FAngelscriptCacheCurrentSymbol{
						Declaration->SignatureHash,
						Global->StorageLayoutFingerprint,
						{}};
				}
			}
			break;

		case EAngelscriptCacheSemanticDependencyKind::HardValue:
			if (Authorities.ModuleState != nullptr)
			{
				const FAngelscriptCachedHardValue* HardValue =
					Authorities.ModuleState->HardValues.FindByPredicate(
						[&Dependency](const FAngelscriptCachedHardValue& Candidate)
						{
							return Candidate.Owner.Kind == Dependency.Target.Kind
								&& Candidate.Owner.StableKey
									== Dependency.Target.StableKey;
						});
				if (HardValue != nullptr)
				{
					const FAngelscriptHash256 Abi = Declaration != nullptr
						? Declaration->SignatureHash
						: HardValue->Owner.ExpectedAbi;
					return FAngelscriptCacheCurrentSymbol{
						Abi, HardValue->HardValueHash, {}};
				}
			}
			break;

		case EAngelscriptCacheSemanticDependencyKind::Initializer:
			if (Authorities.ModuleState != nullptr)
			{
				const FAngelscriptCachedInitializerUnit* Initializer =
					Authorities.ModuleState->Initializers.FindByPredicate(
						[&Key](const FAngelscriptCachedInitializerUnit& Candidate)
						{
							return Candidate.InitializerKey.Hash == Key;
						});
				if (Initializer != nullptr)
				{
					const FAngelscriptHash256 Abi = Declaration != nullptr
						? Declaration->SignatureHash
						: Dependency.Target.ExpectedAbi;
					return FAngelscriptCacheCurrentSymbol{
						Abi, Initializer->InitializerExecutionHash, {}};
				}
			}
			break;

		case EAngelscriptCacheSemanticDependencyKind::FunctionContent:
			if (Declaration != nullptr)
			{
				const FAngelscriptCachedFunctionBody* Body =
					Authorities.FunctionBodies.FindByPredicate(
						[&Key](const FAngelscriptCachedFunctionBody& Candidate)
						{
							return Candidate.Identity.FunctionKey.Hash == Key;
						});
				if (Body != nullptr)
				{
					return FAngelscriptCacheCurrentSymbol{
						Declaration->SignatureHash,
						Body->Identity.Content.Execution,
						{}};
				}
			}
			break;

		case EAngelscriptCacheSemanticDependencyKind::Declaration:
		case EAngelscriptCacheSemanticDependencyKind::Signature:
			if (Declaration != nullptr)
			{
				return FAngelscriptCacheCurrentSymbol{
					Declaration->SignatureHash, {}, {}};
			}
			break;

		case EAngelscriptCacheSemanticDependencyKind::Import:
			if (Authorities.ModuleInterface != nullptr
				&& Dependency.Target.Kind ==
					EAngelscriptCacheReferenceKind::ScriptModule
				&& Authorities.ModuleInterface->ModuleKey.Hash == Key)
			{
				return FAngelscriptCacheCurrentSymbol{
					Authorities.ModuleInterface->InterfaceAbi, {}, {}};
			}
			break;

		default:
			break;
		}

		return {};
	}

	static FAngelscriptHash256 BuildCurrentDependencyFingerprint(
		const FAngelscriptCacheSemanticDependency& Dependency,
		const FAngelscriptCacheCurrentSymbol& Current)
	{
		FAngelscriptArtifactCanonicalWriter Writer(
			TEXT("function-dependency-input"));
		Writer.WriteUInt8(static_cast<uint8>(Dependency.Kind));
		Writer.WriteUInt8(static_cast<uint8>(Dependency.Target.Kind));
		Writer.WriteHash(Dependency.Target.StableKey);
		Writer.WriteHash(Current.CurrentAbi);
		const bool bRequiresContent =
			Dependency.ExpectedContentOrValue.IsSet();
		Writer.WriteBool(bRequiresContent);
		if (bRequiresContent)
		{
			Writer.WriteHash(Current.CurrentContentOrValue.GetValue());
		}
		return Writer.FinalizeHash();
	}
}

bool FAngelscriptCacheCompilerBridge::TryBuildFunctionSourceDigest(
	const asSBuildArtifactInvocation& Invocation,
	const TArray<FString>& CanonicalCompileOptions,
	FAngelscriptFunctionSourceDigest& OutDigest,
	FString& OutFailure)
{
	OutDigest = {};
	OutFailure.Reset();
	if (!Invocation.IsCacheable())
	{
		OutFailure = FString::Printf(
			TEXT("Invocation is not cacheable: reason %u"),
			static_cast<uint32>(Invocation.ineligibleReason));
		return false;
	}
	const TOptional<EAngelscriptArtifactEntityKind> Kind =
		AngelscriptCacheCompilerBridge_Private::MapInvocationKind(Invocation.kind);
	if (!Kind.IsSet() || Invocation.canonicalSource.GetLength() == 0)
	{
		OutFailure = TEXT("Invocation kind or canonical source is invalid");
		return false;
	}

	FAngelscriptFunctionSourceDescriptor Descriptor;
	Descriptor.Kind = Kind.GetValue();
	Descriptor.CanonicalSource = UTF8_TO_TCHAR(
		Invocation.canonicalSource.AddressOf());
	Descriptor.CanonicalOptions = CanonicalCompileOptions;
	OutDigest = FAngelscriptArtifactIdentityBuilder::BuildFunctionSourceDigest(
		Descriptor);
	return !OutDigest.Hash.IsZero();
}

FAngelscriptCacheValidationResult
FAngelscriptCacheCompilerBridge::CanonicalizeActualDependencies(
	const TConstArrayView<FAngelscriptCacheSemanticDependency> ObservedDependencies,
	TArray<FAngelscriptCacheSemanticDependency>& OutCanonicalDependencies)
{
	using namespace AngelscriptCacheCompilerBridge_Private;
	OutCanonicalDependencies.Reset();
	TArray<FCanonicalDependency> Canonical;
	Canonical.Reserve(ObservedDependencies.Num());
	for (const FAngelscriptCacheSemanticDependency& Dependency
		: ObservedDependencies)
	{
		FCanonicalDependency& Entry = Canonical.AddDefaulted_GetRef();
		Entry.Dependency = Dependency;
		const FAngelscriptCacheValidationResult Result =
			FAngelscriptCacheSemanticArchive::SerializeSemanticDependency(
				Dependency, Entry.Bytes);
		if (!Result.IsSuccess())
		{
			OutCanonicalDependencies.Reset();
			return Result;
		}
	}
	Canonical.Sort([](const FCanonicalDependency& Left,
		const FCanonicalDependency& Right)
	{
		return BytesLess(Left.Bytes, Right.Bytes);
	});

	for (const FCanonicalDependency& Entry : Canonical)
	{
		if (!OutCanonicalDependencies.IsEmpty())
		{
			const FAngelscriptCacheSemanticDependency& Previous =
				OutCanonicalDependencies.Last();
			if (Previous == Entry.Dependency)
			{
				continue;
			}
			if (SameDependencyIdentity(Previous, Entry.Dependency))
			{
				OutCanonicalDependencies.Reset();
				return FAngelscriptCacheValidationResult(
					EAngelscriptCacheValidationError::ConflictingKey);
			}
		}
		OutCanonicalDependencies.Add(Entry.Dependency);
	}
	return {};
}

FAngelscriptCacheValidationResult
FAngelscriptCacheCompilerBridge::CaptureSuccessfulActualDependencies(
	const asCScriptFunction& Function,
	const IAngelscriptCacheBuildDependencyResolver& Resolver,
	TArray<FAngelscriptCacheSemanticDependency>& OutCanonicalDependencies)
{
	OutCanonicalDependencies.Reset();
	if (Function.scriptData == nullptr)
	{
		return FAngelscriptCacheValidationResult(
			EAngelscriptCacheValidationError::MissingCoverage);
	}
	TArray<FAngelscriptCacheSemanticDependency> Resolved;
	Resolved.Reserve(static_cast<int32>(
		Function.scriptData->artifactDependencies.GetLength()
			+ (Function.artifactOwnerType != nullptr ? 1u : 0u)));

	// The owner of a type-owned function is an intrinsic declaration input and
	// appears in the detached VM artifact's root signature. The compiler body may
	// otherwise report only a property-layout edge, so capture this semantic owner
	// before canonicalizing the observed dependency set instead of inferring it
	// later from opaque artifact bytes.
	if (Function.artifactOwnerType != nullptr)
	{
		asSBuildArtifactDependency OwnerDependency;
		OwnerDependency.kind = asBUILD_ARTIFACT_DEPENDENCY_DECLARATION;
		OwnerDependency.referenceKind = asBUILD_ARTIFACT_REFERENCE_TYPE;
		OwnerDependency.type = Function.artifactOwnerType;
		FAngelscriptCacheSemanticDependency ResolvedOwner;
		if (!Resolver.Resolve(OwnerDependency, ResolvedOwner))
		{
			return FAngelscriptCacheValidationResult(
				EAngelscriptCacheValidationError::CurrentSymbolMissing);
		}
		Resolved.Add(MoveTemp(ResolvedOwner));

		// A value-type method's detached root signature also embeds the
		// concrete owner layout. The compiler body may report only the
		// property-layout edges used by the implementation, so preserve this
		// intrinsic owner-layout input alongside the owner declaration.
		if ((Function.artifactOwnerType->GetFlags() & asOBJ_VALUE) != 0)
		{
			OwnerDependency.kind = asBUILD_ARTIFACT_DEPENDENCY_VALUE_LAYOUT;
			FAngelscriptCacheSemanticDependency ResolvedOwnerLayout;
			if (!Resolver.Resolve(OwnerDependency, ResolvedOwnerLayout))
			{
				return FAngelscriptCacheValidationResult(
					EAngelscriptCacheValidationError::CurrentSymbolMissing);
			}
			Resolved.Add(MoveTemp(ResolvedOwnerLayout));
		}
	}
	for (asUINT Index = 0;
		Index < Function.scriptData->artifactDependencies.GetLength(); ++Index)
	{
		FAngelscriptCacheSemanticDependency Dependency;
		if (!Resolver.Resolve(
			Function.scriptData->artifactDependencies[Index], Dependency))
		{
			OutCanonicalDependencies.Reset();
			return FAngelscriptCacheValidationResult(
				EAngelscriptCacheValidationError::CurrentSymbolMissing);
		}
		Resolved.Add(MoveTemp(Dependency));
	}
	return CanonicalizeActualDependencies(Resolved, OutCanonicalDependencies);
}

FAngelscriptCacheFunctionInputResolution
FAngelscriptCacheCompilerBridge::ResolveCurrentFunctionInput(
	const FAngelscriptCachedFunctionBody& CachedBody,
	const FAngelscriptFunctionSourceDigest& CurrentSourceDigest,
	const FAngelscriptCacheFunctionInputAuthorities& Authorities)
{
	using namespace AngelscriptCacheCompilerBridge_Private;
	FAngelscriptCacheFunctionInputResolution Result;
	if (CurrentSourceDigest.Hash.IsZero()
		|| CachedBody.FunctionSourceDigest.Hash.IsZero())
	{
		return Result;
	}
	if (!(CurrentSourceDigest.Hash == CachedBody.FunctionSourceDigest.Hash))
	{
		Result.Status = EAngelscriptCacheFunctionInputStatus::SourceChanged;
		return Result;
	}

	TArray<FAngelscriptCacheSemanticDependency> CanonicalDependencies;
	if (!CanonicalizeActualDependencies(
		CachedBody.ActualDependencies, CanonicalDependencies).IsSuccess()
		|| CanonicalDependencies.Num() != CachedBody.ActualDependencies.Num())
	{
		return Result;
	}

	FAngelscriptFunctionInputDescriptor Input;
	Input.SourceDigest = CurrentSourceDigest;
	Input.OrderedDependencyFingerprints.Reserve(CanonicalDependencies.Num());
	for (int32 Index = 0; Index < CanonicalDependencies.Num(); ++Index)
	{
		const FAngelscriptCacheSemanticDependency& Dependency =
			CanonicalDependencies[Index];
		TOptional<FAngelscriptCacheCurrentSymbol> Current =
			ResolveLocalSymbol(Dependency, Authorities);
		if (!Current.IsSet() && Authorities.ExternalSymbols != nullptr)
		{
			Current = Authorities.ExternalSymbols->Resolve(
				Dependency.Target.Kind, Dependency.Target.StableKey);
		}
		if (!Current.IsSet() || Current->CurrentAbi.IsZero()
			|| (Dependency.ExpectedContentOrValue.IsSet()
				&& !Current->CurrentContentOrValue.IsSet()))
		{
			Result.Status =
				EAngelscriptCacheFunctionInputStatus::DependencyMissing;
			Result.MissingDependencyOrdinal = static_cast<uint32>(Index);
			return Result;
		}
		Input.OrderedDependencyFingerprints.Add(
			BuildCurrentDependencyFingerprint(Dependency, Current.GetValue()));
	}

	Result.CurrentInputDigest =
		FAngelscriptArtifactIdentityBuilder::BuildFunctionInputDigest(Input);
	Result.Status = Result.CurrentInputDigest.Hash
		== CachedBody.FunctionInputDigest.Hash
		? EAngelscriptCacheFunctionInputStatus::ResolvedMatch
		: EAngelscriptCacheFunctionInputStatus::ResolvedMismatch;
	return Result;
}

FAngelscriptCacheFunctionCandidateLookupResult
FAngelscriptCacheCompilerBridge::TryRestoreFunctionFromValidatedGraph(
	const FAngelscriptValidatedModuleGraph& CandidateGraph,
	const asSBuildArtifactInvocation& Invocation,
	asCScriptFunction& TargetFunction,
	const FAngelscriptStableFunctionKey& CurrentFunctionKey,
	const FAngelscriptArtifactProfileKey& CurrentProfile,
	const TArray<FString>& CanonicalCompileOptions,
	const FAngelscriptCacheFunctionInputAuthorities& CurrentAuthorities,
	const FAngelscriptCacheReadLimits& Limits,
	FAngelscriptCacheReadBudget& Budget)
{
	FAngelscriptCacheFunctionCandidateLookupResult Result;
	if (!Invocation.IsCacheable() || CurrentFunctionKey.Hash.IsZero())
	{
		Result.Status =
			EAngelscriptCacheFunctionCandidateLookupStatus::NotCacheable;
		Result.RestoreResult = asBUILD_ARTIFACT_RESTORE_NOT_CACHEABLE;
		Result.Detail = !Invocation.IsCacheable()
			? FString::Printf(
				TEXT("Invocation is not cacheable: reason %u"),
				static_cast<uint32>(Invocation.ineligibleReason))
			: TEXT("Current stable function key is zero");
		return Result;
	}
	if (CandidateGraph.IsEmpty() || CurrentProfile.Hash.IsZero())
	{
		Result.Status =
			EAngelscriptCacheFunctionCandidateLookupStatus::FunctionKeyMiss;
		Result.Detail = TEXT("Validated candidate graph or current Profile is empty");
		return Result;
	}

	const TConstArrayView<FAngelscriptCacheValidatedFunctionOrdinal> Functions =
		CandidateGraph.GetFunctionOrdinals();
	int32 First = 0;
	int32 Last = Functions.Num();
	while (First < Last)
	{
		const int32 Middle = First + (Last - First) / 2;
		if (Functions[Middle].FunctionKey.Hash < CurrentFunctionKey.Hash)
		{
			First = Middle + 1;
		}
		else
		{
			Last = Middle;
		}
	}
	if (!Functions.IsValidIndex(First)
		|| !(Functions[First].FunctionKey == CurrentFunctionKey))
	{
		Result.Status =
			EAngelscriptCacheFunctionCandidateLookupStatus::FunctionKeyMiss;
		Result.Detail = FString::Printf(
			TEXT("No graph-validated candidate for stable function key %s"),
			*CurrentFunctionKey.Hash.ToHexString());
		return Result;
	}

	const FAngelscriptCacheValidatedFunctionOrdinal& Function = Functions[First];
	const TConstArrayView<FAngelscriptDecodedCacheRecordHandle> Records =
		CandidateGraph.GetReachableRecords();
	if (!Function.BodyRecordOrdinal.IsSet()
		|| !Function.DebugRecordOrdinal.IsSet()
		|| !Records.IsValidIndex(Function.BodyRecordOrdinal.GetValue())
		|| !Records.IsValidIndex(Function.DebugRecordOrdinal.GetValue()))
	{
		Result.Status =
			EAngelscriptCacheFunctionCandidateLookupStatus::NotCacheable;
		Result.RestoreResult = asBUILD_ARTIFACT_RESTORE_NOT_CACHEABLE;
		Result.Detail = TEXT("Candidate has no complete execution/debug body");
		return Result;
	}
	const FAngelscriptCachedFunctionBody* Body =
		Records[Function.BodyRecordOrdinal.GetValue()]->TryGetFunctionBody();
	const FAngelscriptCachedDebugSidecar* Debug =
		Records[Function.DebugRecordOrdinal.GetValue()]->TryGetDebugSidecar();
	if (Body == nullptr || Debug == nullptr
		|| !(Body->Identity.FunctionKey == CurrentFunctionKey))
	{
		Result.Status =
			EAngelscriptCacheFunctionCandidateLookupStatus::RejectedCorrupt;
		Result.RestoreResult = asBUILD_ARTIFACT_RESTORE_REJECTED_CORRUPT;
		Result.Detail = TEXT("Graph function ordinals do not resolve to the expected body/debug records");
		return Result;
	}
	if (!(Body->Identity.Profile.Hash == CurrentProfile.Hash))
	{
		Result.Status =
			EAngelscriptCacheFunctionCandidateLookupStatus::ProfileMismatch;
		Result.Detail = TEXT("Candidate Profile does not match the current compilation Profile");
		return Result;
	}

	FString SourceFailure;
	if (!TryBuildFunctionSourceDigest(
		Invocation,
		CanonicalCompileOptions,
		Result.CurrentSourceDigest,
		SourceFailure))
	{
		Result.Status =
			EAngelscriptCacheFunctionCandidateLookupStatus::NotCacheable;
		Result.RestoreResult = asBUILD_ARTIFACT_RESTORE_NOT_CACHEABLE;
		Result.Detail = MoveTemp(SourceFailure);
		return Result;
	}

	const FAngelscriptCacheFunctionInputResolution Input =
		ResolveCurrentFunctionInput(
			*Body,
			Result.CurrentSourceDigest,
			CurrentAuthorities);
	Result.CurrentInputDigest = Input.CurrentInputDigest;
	Result.MissingDependencyOrdinal = Input.MissingDependencyOrdinal;
	switch (Input.Status)
	{
	case EAngelscriptCacheFunctionInputStatus::SourceChanged:
		Result.Status =
			EAngelscriptCacheFunctionCandidateLookupStatus::SourceChanged;
		Result.Detail = TEXT("Current function source digest differs from the candidate");
		return Result;
	case EAngelscriptCacheFunctionInputStatus::DependencyMissing:
		Result.Status =
			EAngelscriptCacheFunctionCandidateLookupStatus::DependencyMissing;
		Result.Detail = FString::Printf(
			TEXT("Current authority is missing dependency ordinal %d"),
			Input.MissingDependencyOrdinal.IsSet()
				? static_cast<int32>(Input.MissingDependencyOrdinal.GetValue()) : -1);
		return Result;
	case EAngelscriptCacheFunctionInputStatus::ResolvedMismatch:
		Result.Status =
			EAngelscriptCacheFunctionCandidateLookupStatus::InputChanged;
		Result.Detail = TEXT("Current function input digest differs from the candidate");
		return Result;
	case EAngelscriptCacheFunctionInputStatus::ResolvedMatch:
		break;
	default:
		Result.Status =
			EAngelscriptCacheFunctionCandidateLookupStatus::RejectedCorrupt;
		Result.RestoreResult = asBUILD_ARTIFACT_RESTORE_REJECTED_CORRUPT;
		Result.Detail = TEXT("Candidate function input could not be resolved deterministically");
		return Result;
	}

	Result.RestoreResult = TryRestoreFunctionArtifact(
		Invocation,
		TargetFunction,
		*Body,
		*Debug,
		Limits,
		Budget,
		Result.Detail);
	if (Result.RestoreResult == asBUILD_ARTIFACT_RESTORE_RESTORED)
	{
		Result.Status =
			EAngelscriptCacheFunctionCandidateLookupStatus::Restored;
		Result.RestoredActualDependencies = Body->ActualDependencies;
	}
	else if (Result.RestoreResult
		== asBUILD_ARTIFACT_RESTORE_NOT_CACHEABLE)
	{
		Result.Status =
			EAngelscriptCacheFunctionCandidateLookupStatus::NotCacheable;
	}
	else if (Result.RestoreResult
		== asBUILD_ARTIFACT_RESTORE_REJECTED_CORRUPT)
	{
		Result.Status =
			EAngelscriptCacheFunctionCandidateLookupStatus::RejectedCorrupt;
	}
	else
	{
		Result.Status =
			EAngelscriptCacheFunctionCandidateLookupStatus::FunctionKeyMiss;
	}
	return Result;
}

asEBuildArtifactRestoreResult
FAngelscriptCacheCompilerBridge::TryRestoreFunctionArtifact(
	const asSBuildArtifactInvocation& Invocation,
	asCScriptFunction& TargetFunction,
	const FAngelscriptCachedFunctionBody& CachedBody,
	const FAngelscriptCachedDebugSidecar& CachedDebug,
	const FAngelscriptCacheReadLimits& Limits,
	FAngelscriptCacheReadBudget& Budget,
	FString& OutDetail)
{
	OutDetail.Reset();
	if (!Invocation.IsCacheable())
	{
		OutDetail = FString::Printf(
			TEXT("Invocation is not cacheable: reason %u"),
			static_cast<uint32>(Invocation.ineligibleReason));
		return asBUILD_ARTIFACT_RESTORE_NOT_CACHEABLE;
	}
	if (TargetFunction.module == nullptr || TargetFunction.engine == nullptr)
	{
		OutDetail = TEXT("Target function has no module or engine");
		return asBUILD_ARTIFACT_RESTORE_REJECTED_CORRUPT;
	}
	if (CachedBody.CanonicalExecutionPayload.IsEmpty()
		|| CachedDebug.CanonicalDebugPayload.IsEmpty()
		|| CachedBody.VmExecutionCodecVersion
			!= FAngelscriptFunctionArtifactCodec::ExecutionCodecVersion
		|| CachedDebug.VmDebugCodecVersion
			!= FAngelscriptFunctionArtifactCodec::DebugCodecVersion
		|| static_cast<uint8>(CachedBody.InvocationKind)
			!= static_cast<uint8>(Invocation.kind)
		|| !(CachedBody.Identity.FunctionKey
			== CachedDebug.FunctionKey)
		|| !(CachedBody.Identity.Profile.Hash
			== CachedDebug.Profile.Hash)
		|| !(CachedBody.Identity.Content.Debug
			== CachedDebug.DebugHash))
	{
		OutDetail = TEXT("Candidate identity, invocation kind or codec is inconsistent");
		return asBUILD_ARTIFACT_RESTORE_REJECTED_CORRUPT;
	}

	const FAngelscriptFunctionContentHash RecomputedDebugContent =
		FAngelscriptArtifactIdentityBuilder::BuildFunctionContentHash(
			{},
			CachedDebug.CanonicalDebugPayload);
	if (!(RecomputedDebugContent.Debug == CachedBody.Identity.Content.Debug))
	{
		OutDetail = TEXT("Candidate debug content hash mismatch");
		return asBUILD_ARTIFACT_RESTORE_REJECTED_CORRUPT;
	}

	FAngelscriptFunctionArtifactCodec Codec(
		*TargetFunction.module, *TargetFunction.engine);
	const FAngelscriptCacheValidationResult Restore =
		Codec.RestoreFunctionIntoExisting(
			CachedBody.CanonicalExecutionPayload,
			CachedDebug.CanonicalDebugPayload,
			CachedBody.ModuleKey,
			CachedBody.ActualDependencies,
			CachedBody.Identity.Content.Execution,
			Limits,
			Budget,
			TargetFunction);
	if (!Restore.IsSuccess())
	{
		OutDetail = Codec.GetLastExecutionFailureDetail();
		if (OutDetail.IsEmpty())
		{
			OutDetail = FString::Printf(
				TEXT("Artifact restore failed: Error=%u Offset=%llu"),
				static_cast<uint32>(Restore.Error),
				Restore.ByteOffset);
		}
		return asBUILD_ARTIFACT_RESTORE_REJECTED_CORRUPT;
	}
	OutDetail = FString::Printf(
		TEXT("Restored complete function artifact to current FunctionId %d"),
		TargetFunction.id);
	return asBUILD_ARTIFACT_RESTORE_RESTORED;
}
