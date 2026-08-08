#include "Testing/AngelscriptBindExecutionObservation.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Core/AngelscriptEngine.h"

namespace
{
	constexpr int32 BindPhaseCount = static_cast<int32>(EAngelscriptBindPhase::Finalization) + 1;

	FCriticalSection GAngelscriptBindExecutionObservationMutex;
	FAngelscriptBindExecutionSnapshot GAngelscriptBindExecutionSnapshot;
	double GBindScriptTypesStartTime = 0.0;
	double GCallbackExecutionStartTime = 0.0;
	double GProviderStartTime = 0.0;
	int32 GActiveProviderIndex = INDEX_NONE;
	uint64 GNextExecutionEpoch = 0;
	bool GObservationPassActive = false;

	const TCHAR* GetPhaseName(const EAngelscriptBindPhase Phase)
	{
		switch (Phase)
		{
		case EAngelscriptBindPhase::TypeDeclarations:
			return TEXT("TypeDeclarations");
		case EAngelscriptBindPhase::TypeInfrastructure:
			return TEXT("TypeInfrastructure");
		case EAngelscriptBindPhase::ManualBindings:
			return TEXT("ManualBindings");
		case EAngelscriptBindPhase::GeneratedBindings:
			return TEXT("GeneratedBindings");
		case EAngelscriptBindPhase::ReflectionBindings:
			return TEXT("ReflectionBindings");
		case EAngelscriptBindPhase::PostReflectionBindings:
			return TEXT("PostReflectionBindings");
		case EAngelscriptBindPhase::Finalization:
			return TEXT("Finalization");
		default:
			return TEXT("Invalid");
		}
	}

	void ResetExecutionFields(FAngelscriptBindExecutionSnapshot& Snapshot)
	{
		Snapshot.EngineIdentity = 0;
		Snapshot.ExecutionEpoch = 0;
		Snapshot.ExecutedBindNames.Reset();
		Snapshot.ProviderRecords.Reset();
		Snapshot.PhaseTotals.Init(FAngelscriptBindPhaseExecutionTotal(), BindPhaseCount);
		Snapshot.bExecutionAborted = false;
		Snapshot.FirstFailureDiagnostic.Reset();
		Snapshot.PublicationEligibility = EAngelscriptBindPublicationEligibility::Pending;
		Snapshot.bPublicationResultRecorded = false;
		Snapshot.bPublished = false;
		Snapshot.CallbackExecutionDurationSeconds = 0.0;
	}
}

void FAngelscriptBindExecutionObservation::Reset()
{
	FScopeLock Lock(&GAngelscriptBindExecutionObservationMutex);
	GAngelscriptBindExecutionSnapshot = FAngelscriptBindExecutionSnapshot();
	GAngelscriptBindExecutionSnapshot.CollectionPhaseProviderCounts.Init(0, BindPhaseCount);
	GAngelscriptBindExecutionSnapshot.PhaseTotals.Init(FAngelscriptBindPhaseExecutionTotal(), BindPhaseCount);
	GBindScriptTypesStartTime = 0.0;
	GCallbackExecutionStartTime = 0.0;
	GProviderStartTime = 0.0;
	GActiveProviderIndex = INDEX_NONE;
	GObservationPassActive = false;
}

FAngelscriptBindExecutionSnapshot FAngelscriptBindExecutionObservation::GetLastSnapshot()
{
	FScopeLock Lock(&GAngelscriptBindExecutionObservationMutex);
	return GAngelscriptBindExecutionSnapshot;
}

int32 FAngelscriptBindExecutionObservation::GetInvocationCount()
{
	FScopeLock Lock(&GAngelscriptBindExecutionObservationMutex);
	return GAngelscriptBindExecutionSnapshot.InvocationCount;
}

void FAngelscriptBindExecutionObservation::RecordCollectionFinalization(
	const bool bSucceeded,
	const TConstArrayView<int32> PhaseProviderCounts,
	const TConstArrayView<FName> ProviderOrder,
	const double DurationSeconds,
	const FString& Diagnostic)
{
	FScopeLock Lock(&GAngelscriptBindExecutionObservationMutex);
	++GAngelscriptBindExecutionSnapshot.CollectionFinalizationCount;
	GAngelscriptBindExecutionSnapshot.bCollectionFinalized = bSucceeded;
	GAngelscriptBindExecutionSnapshot.FinalizedProviderCount = ProviderOrder.Num();
	GAngelscriptBindExecutionSnapshot.CollectionPhaseProviderCounts.Reset(PhaseProviderCounts.Num());
	GAngelscriptBindExecutionSnapshot.CollectionPhaseProviderCounts.Append(PhaseProviderCounts);
	GAngelscriptBindExecutionSnapshot.FinalizedProviderOrder.Reset(ProviderOrder.Num());
	GAngelscriptBindExecutionSnapshot.FinalizedProviderOrder.Append(ProviderOrder);
	GAngelscriptBindExecutionSnapshot.CollectionFinalizationDurationSeconds = DurationSeconds;
	GAngelscriptBindExecutionSnapshot.CollectionFinalizationDiagnostic = Diagnostic;
}

void FAngelscriptBindExecutionObservation::RecordLateRegistration(const FString& Diagnostic)
{
	FScopeLock Lock(&GAngelscriptBindExecutionObservationMutex);
	if (GAngelscriptBindExecutionSnapshot.LateRegistrationDiagnostic.IsEmpty())
	{
		GAngelscriptBindExecutionSnapshot.LateRegistrationDiagnostic = Diagnostic;
	}
}

void FAngelscriptBindExecutionObservation::BeginBindScriptTypesTiming()
{
	FScopeLock Lock(&GAngelscriptBindExecutionObservationMutex);
	GBindScriptTypesStartTime = FPlatformTime::Seconds();
	GAngelscriptBindExecutionSnapshot.BindScriptTypesDurationSeconds = 0.0;
}

void FAngelscriptBindExecutionObservation::EndBindScriptTypesTiming()
{
	FScopeLock Lock(&GAngelscriptBindExecutionObservationMutex);
	if (GBindScriptTypesStartTime > 0.0)
	{
		GAngelscriptBindExecutionSnapshot.BindScriptTypesDurationSeconds =
			FPlatformTime::Seconds() - GBindScriptTypesStartTime;
		GBindScriptTypesStartTime = 0.0;
	}
}

void FAngelscriptBindExecutionObservation::BeginObservationPass(const void* EngineIdentity)
{
	FScopeLock Lock(&GAngelscriptBindExecutionObservationMutex);
	++GAngelscriptBindExecutionSnapshot.InvocationCount;
	ResetExecutionFields(GAngelscriptBindExecutionSnapshot);
	GAngelscriptBindExecutionSnapshot.EngineIdentity = reinterpret_cast<UPTRINT>(EngineIdentity);
	GAngelscriptBindExecutionSnapshot.ExecutionEpoch = ++GNextExecutionEpoch;
	GCallbackExecutionStartTime = FPlatformTime::Seconds();
	GProviderStartTime = 0.0;
	GActiveProviderIndex = INDEX_NONE;
	GObservationPassActive = true;
}

void FAngelscriptBindExecutionObservation::BeginProvider(
	const FName OwnerModule,
	const FName BindName,
	const EAngelscriptBindPhase Phase,
	const ANSICHAR* SourceFile,
	const int32 SourceLine)
{
	FScopeLock Lock(&GAngelscriptBindExecutionObservationMutex);
	if (!GObservationPassActive || GActiveProviderIndex != INDEX_NONE)
	{
		return;
	}

	FAngelscriptBindProviderExecutionRecord& Record =
		GAngelscriptBindExecutionSnapshot.ProviderRecords.AddDefaulted_GetRef();
	Record.EngineIdentity = GAngelscriptBindExecutionSnapshot.EngineIdentity;
	Record.ExecutionEpoch = GAngelscriptBindExecutionSnapshot.ExecutionEpoch;
	Record.OwnerModule = OwnerModule;
	Record.BindName = BindName;
	Record.Phase = Phase;
	Record.SourceFile = SourceFile != nullptr ? ANSI_TO_TCHAR(SourceFile) : FString();
	Record.SourceLine = SourceLine;
	GActiveProviderIndex = GAngelscriptBindExecutionSnapshot.ProviderRecords.Num() - 1;
	GProviderStartTime = FPlatformTime::Seconds();
	GAngelscriptBindExecutionSnapshot.ExecutedBindNames.Add(BindName);

	const int32 PhaseIndex = static_cast<int32>(Phase);
	if (GAngelscriptBindExecutionSnapshot.PhaseTotals.IsValidIndex(PhaseIndex))
	{
		++GAngelscriptBindExecutionSnapshot.PhaseTotals[PhaseIndex].AttemptedCount;
	}
}

void FAngelscriptBindExecutionObservation::EndProvider(
	const bool bSucceeded,
	const FString& FailureDiagnostic)
{
	FScopeLock Lock(&GAngelscriptBindExecutionObservationMutex);
	if (!GObservationPassActive
		|| !GAngelscriptBindExecutionSnapshot.ProviderRecords.IsValidIndex(GActiveProviderIndex))
	{
		return;
	}

	FAngelscriptBindProviderExecutionRecord& Record =
		GAngelscriptBindExecutionSnapshot.ProviderRecords[GActiveProviderIndex];
	Record.DurationSeconds = GProviderStartTime > 0.0
		? FPlatformTime::Seconds() - GProviderStartTime
		: 0.0;
	Record.Status = bSucceeded
		? EAngelscriptBindProviderStatus::Succeeded
		: EAngelscriptBindProviderStatus::Failed;
	Record.FailureDiagnostic = FailureDiagnostic;

	const int32 PhaseIndex = static_cast<int32>(Record.Phase);
	if (GAngelscriptBindExecutionSnapshot.PhaseTotals.IsValidIndex(PhaseIndex))
	{
		FAngelscriptBindPhaseExecutionTotal& PhaseTotal =
			GAngelscriptBindExecutionSnapshot.PhaseTotals[PhaseIndex];
		PhaseTotal.DurationSeconds += Record.DurationSeconds;
		if (bSucceeded)
		{
			++PhaseTotal.SucceededCount;
		}
		else
		{
			++PhaseTotal.FailedCount;
		}
	}

	if (!bSucceeded)
	{
		GAngelscriptBindExecutionSnapshot.bExecutionAborted = true;
		if (GAngelscriptBindExecutionSnapshot.FirstFailureDiagnostic.IsEmpty())
		{
			GAngelscriptBindExecutionSnapshot.FirstFailureDiagnostic = FailureDiagnostic;
		}
	}
	GProviderStartTime = 0.0;
	GActiveProviderIndex = INDEX_NONE;
}

void FAngelscriptBindExecutionObservation::RecordExecutedBind(const FName BindName)
{
	FScopeLock Lock(&GAngelscriptBindExecutionObservationMutex);
	if (GObservationPassActive)
	{
		GAngelscriptBindExecutionSnapshot.ExecutedBindNames.Add(BindName);
	}
}

void FAngelscriptBindExecutionObservation::EndObservationPass(
	const bool bExecutionSucceeded,
	const FString& FailureDiagnostic)
{
	FScopeLock Lock(&GAngelscriptBindExecutionObservationMutex);
	if (!GObservationPassActive)
	{
		return;
	}

	if (GCallbackExecutionStartTime > 0.0)
	{
		GAngelscriptBindExecutionSnapshot.CallbackExecutionDurationSeconds =
			FPlatformTime::Seconds() - GCallbackExecutionStartTime;
	}
	if (!bExecutionSucceeded)
	{
		GAngelscriptBindExecutionSnapshot.bExecutionAborted = true;
		if (GAngelscriptBindExecutionSnapshot.FirstFailureDiagnostic.IsEmpty())
		{
			GAngelscriptBindExecutionSnapshot.FirstFailureDiagnostic = FailureDiagnostic;
		}
	}
	GAngelscriptBindExecutionSnapshot.PublicationEligibility =
		bExecutionSucceeded && !GAngelscriptBindExecutionSnapshot.bExecutionAborted
			? EAngelscriptBindPublicationEligibility::Eligible
			: EAngelscriptBindPublicationEligibility::Blocked;
	GCallbackExecutionStartTime = 0.0;
	GProviderStartTime = 0.0;
	GActiveProviderIndex = INDEX_NONE;
	GObservationPassActive = false;
}

void FAngelscriptBindExecutionObservation::RecordPublicationResult(
	const void* EngineIdentity,
	const bool bPublished)
{
	FScopeLock Lock(&GAngelscriptBindExecutionObservationMutex);
	if (GAngelscriptBindExecutionSnapshot.EngineIdentity != reinterpret_cast<UPTRINT>(EngineIdentity))
	{
		return;
	}
	GAngelscriptBindExecutionSnapshot.bPublicationResultRecorded = true;
	GAngelscriptBindExecutionSnapshot.bPublished = bPublished;

#if AS_PRINT_STATS
	UE_LOG(
		Angelscript,
		Display,
		TEXT("AS_BIND_CALLBACK_SUMMARY engine=%llu epoch=%llu published=%d callbacks=%d total_ms=%.3f"),
		static_cast<uint64>(GAngelscriptBindExecutionSnapshot.EngineIdentity),
		GAngelscriptBindExecutionSnapshot.ExecutionEpoch,
		bPublished ? 1 : 0,
		GAngelscriptBindExecutionSnapshot.ProviderRecords.Num(),
		GAngelscriptBindExecutionSnapshot.CallbackExecutionDurationSeconds * 1000.0);
	for (int32 PhaseIndex = 0; PhaseIndex < GAngelscriptBindExecutionSnapshot.PhaseTotals.Num(); ++PhaseIndex)
	{
		const FAngelscriptBindPhaseExecutionTotal& Total =
			GAngelscriptBindExecutionSnapshot.PhaseTotals[PhaseIndex];
		UE_LOG(
			Angelscript,
			Display,
			TEXT("AS_BIND_PHASE_TOTAL engine=%llu epoch=%llu phase=%s attempted=%d succeeded=%d failed=%d duration_ms=%.3f"),
			static_cast<uint64>(GAngelscriptBindExecutionSnapshot.EngineIdentity),
			GAngelscriptBindExecutionSnapshot.ExecutionEpoch,
			GetPhaseName(static_cast<EAngelscriptBindPhase>(PhaseIndex)),
			Total.AttemptedCount,
			Total.SucceededCount,
			Total.FailedCount,
			Total.DurationSeconds * 1000.0);
	}
	for (const FString& Line : BuildTopCallbackLogLines(GAngelscriptBindExecutionSnapshot, 10))
	{
		UE_LOG(Angelscript, Display, TEXT("%s"), *Line);
	}
#endif
}

TArray<FString> FAngelscriptBindExecutionObservation::BuildTopCallbackLogLines(
	const FAngelscriptBindExecutionSnapshot& Snapshot,
	const int32 TopCount)
{
	TArray<int32> Indices;
	Indices.Reserve(Snapshot.ProviderRecords.Num());
	for (int32 Index = 0; Index < Snapshot.ProviderRecords.Num(); ++Index)
	{
		Indices.Add(Index);
	}
	Indices.StableSort([&Snapshot](const int32 LeftIndex, const int32 RightIndex)
	{
		return Snapshot.ProviderRecords[LeftIndex].DurationSeconds
			> Snapshot.ProviderRecords[RightIndex].DurationSeconds;
	});

	const int32 LineCount = FMath::Clamp(TopCount, 0, Indices.Num());
	TArray<FString> Lines;
	Lines.Reserve(LineCount);
	for (int32 Rank = 0; Rank < LineCount; ++Rank)
	{
		const FAngelscriptBindProviderExecutionRecord& Record =
			Snapshot.ProviderRecords[Indices[Rank]];
		Lines.Add(FString::Printf(
			TEXT("AS_BIND_CALLBACK_TOP rank=%d engine=%llu epoch=%llu owner=%s bind=%s phase=%s status=%s duration_ms=%.3f"),
			Rank + 1,
			static_cast<uint64>(Record.EngineIdentity),
			Record.ExecutionEpoch,
			*Record.OwnerModule.ToString(),
			*Record.BindName.ToString(),
			GetPhaseName(Record.Phase),
			Record.Status == EAngelscriptBindProviderStatus::Succeeded ? TEXT("Succeeded") : TEXT("Failed"),
			Record.DurationSeconds * 1000.0));
	}
	return Lines;
}

#endif
