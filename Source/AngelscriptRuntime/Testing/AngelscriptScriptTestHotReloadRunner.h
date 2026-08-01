#pragma once

#include "CoreMinimal.h"
#include "Testing/AngelscriptScriptTestRegistry.h"

struct FAngelscriptEngine;
class FAngelscriptScriptTestExecutionContext;
class FAutomationTestBase;
struct FAngelscriptModuleDesc;
class UAngelscriptTestSuite;

struct ANGELSCRIPTRUNTIME_API FAngelscriptScriptTestRunSummary
{
	int32 Selected = 0;
	int32 Executed = 0;
	int32 Passed = 0;
	int32 Failed = 0;

	bool IsSuccessful() const
	{
		return Selected > 0
			&& Executed == Selected
			&& Passed + Failed == Executed
			&& Failed == 0;
	}
};

/** Run the current reflected registry synchronously in a commandlet. */
ANGELSCRIPTRUNTIME_API bool RunAngelscriptScriptTests(
	FAngelscriptEngine& Engine);

/**
 * Asynchronous automatic-test scheduler used after a successful script hot
 * reload. Pending work contains stable IDs only; suite objects, commands and
 * Worlds remain owned by the active leaf and are never migrated to a newer
 * generation.
 */
class ANGELSCRIPTRUNTIME_API FAngelscriptScriptTestHotReloadRunner
{
public:
	FAngelscriptScriptTestHotReloadRunner();
	~FAngelscriptScriptTestHotReloadRunner();

	void PrepareTests(
		const TArray<TSharedRef<FAngelscriptModuleDesc>>&
			CompiledModules);
	void CancelModulesBeforeReload(
		const TSet<FString>& ModuleNames);
	bool RunTests(FAngelscriptEngine* Engine);
	bool RunCurrentRegistrySynchronously(
		FAngelscriptEngine& Engine,
		FAngelscriptScriptTestRunSummary* OutSummary = nullptr);
	bool ShouldRunTestsOnHotReload() const;

	bool HasWork() const
	{
		return bRunInProgress
			|| ActiveContext.IsValid()
			|| !PendingTests.IsEmpty();
	}

#if WITH_DEV_AUTOMATION_TESTS
	int32 GetPendingCountForTesting() const
	{
		return PendingTests.Num();
	}

	bool HasSuiteSessionForTesting() const
	{
		return SuiteSession.IsValid();
	}

	void SetSuiteSessionClosedObserverForTesting(
		TFunction<void(bool)> Observer)
	{
		SuiteSessionClosedObserverForTesting = MoveTemp(Observer);
	}
#endif

private:
	struct FSuiteSession;

	void CancelOlderWork();
	void CompleteActive();
	bool CompletePreparedRun();
	bool EnsureSuiteSession(
		const FAngelscriptScriptTestId& Id,
		uint64 Generation,
		FAutomationTestBase& Result);
	void CloseSuiteSession();

	TArray<FAngelscriptScriptTestId> PendingTests;
	TSharedPtr<FAngelscriptScriptTestExecutionContext>
		ActiveContext;
	TUniquePtr<FAutomationTestBase> ActiveResult;
	TUniquePtr<FSuiteSession> SuiteSession;
	uint64 PreparedGeneration = 0;
	int32 CompletedSinceGarbageCollection = 0;
	bool bAllPassed = true;
	bool bRunInProgress = false;
#if WITH_DEV_AUTOMATION_TESTS
	TFunction<void(bool)> SuiteSessionClosedObserverForTesting;
#endif
};
