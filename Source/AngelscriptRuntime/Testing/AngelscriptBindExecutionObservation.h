#pragma once

#include "CoreMinimal.h"
#include "Core/AngelscriptBinds.h"

#if WITH_DEV_AUTOMATION_TESTS

enum class EAngelscriptBindProviderStatus : uint8
{
	Succeeded,
	Failed,
};

enum class EAngelscriptBindPublicationEligibility : uint8
{
	Pending,
	Eligible,
	Blocked,
};

struct FAngelscriptBindPhaseExecutionTotal
{
	int32 AttemptedCount = 0;
	int32 SucceededCount = 0;
	int32 FailedCount = 0;
	double DurationSeconds = 0.0;
};

struct FAngelscriptBindProviderExecutionRecord
{
	UPTRINT EngineIdentity = 0;
	uint64 ExecutionEpoch = 0;
	FName OwnerModule;
	FName BindName;
	EAngelscriptBindPhase Phase = EAngelscriptBindPhase::ExplicitBindings;
	FString SourceFile;
	int32 SourceLine = 0;
	EAngelscriptBindProviderStatus Status = EAngelscriptBindProviderStatus::Succeeded;
	double DurationSeconds = 0.0;
	FString FailureDiagnostic;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptBindExecutionSnapshot
{
	int32 CollectionFinalizationCount = 0;
	bool bCollectionFinalized = false;
	int32 FinalizedProviderCount = 0;
	TArray<int32> CollectionPhaseProviderCounts;
	TArray<FName> FinalizedProviderOrder;
	double CollectionFinalizationDurationSeconds = 0.0;
	FString CollectionFinalizationDiagnostic;
	FString LateRegistrationDiagnostic;

	int32 InvocationCount = 0;
	UPTRINT EngineIdentity = 0;
	uint64 ExecutionEpoch = 0;
	TArray<FName> ExecutedBindNames;
	TArray<FAngelscriptBindProviderExecutionRecord> ProviderRecords;
	TArray<FAngelscriptBindPhaseExecutionTotal> PhaseTotals;
	bool bExecutionAborted = false;
	FString FirstFailureDiagnostic;
	EAngelscriptBindPublicationEligibility PublicationEligibility =
		EAngelscriptBindPublicationEligibility::Pending;
	bool bPublicationResultRecorded = false;
	bool bPublished = false;
	double BindScriptTypesDurationSeconds = 0.0;
	double CallbackExecutionDurationSeconds = 0.0;
};

struct ANGELSCRIPTRUNTIME_API FAngelscriptBindExecutionObservation
{
	static void Reset();
	static FAngelscriptBindExecutionSnapshot GetLastSnapshot();
	static int32 GetInvocationCount();

	static void RecordCollectionFinalization(
		bool bSucceeded,
		TConstArrayView<int32> PhaseProviderCounts,
		TConstArrayView<FName> ProviderOrder,
		double DurationSeconds,
		const FString& Diagnostic);
	static void RecordLateRegistration(const FString& Diagnostic);

	static void BeginBindScriptTypesTiming();
	static void EndBindScriptTypesTiming();
	static void BeginObservationPass(const void* EngineIdentity = nullptr);
	static void BeginProvider(
		FName OwnerModule,
		FName BindName,
		EAngelscriptBindPhase Phase,
		const ANSICHAR* SourceFile,
		int32 SourceLine);
	static void EndProvider(bool bSucceeded, const FString& FailureDiagnostic = FString());
	static void RecordExecutedBind(FName BindName);
	static void EndObservationPass(
		bool bExecutionSucceeded = true,
		const FString& FailureDiagnostic = FString());
	static void RecordPublicationResult(const void* EngineIdentity, bool bPublished);

	static TArray<FString> BuildTopCallbackLogLines(
		const FAngelscriptBindExecutionSnapshot& Snapshot,
		int32 TopCount = 10);
};

#endif
