#pragma once

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

#include "Testing/AngelscriptScriptTestRegistry.h"
#include "Testing/AngelscriptTestSuite.h"

struct FAngelscriptEngine;

/**
 * Persistent UE Automation registrations for reflected script tests.
 *
 * A complex Automation registration has one flag mask for every leaf, so the
 * manager owns one never-recreated bridge for each exact mask ever observed.
 */
class ANGELSCRIPTRUNTIME_API FAngelscriptScriptTestAutomation
{
public:
	static FAngelscriptScriptTestAutomation& Get();

	void Startup();
	void Shutdown();
	void EnsureBridges(
		const FAngelscriptScriptTestRegistrySnapshot& Snapshot);
	void CancelModulesBeforeReload(
		const TSet<FString>& ModuleNames);
	void CancelEngineBeforeShutdown(FAngelscriptEngine* Engine);

	int32 GetBridgeCount() const;
	bool HasBridge(EAutomationTestFlags Flags) const;
	void GetLeavesForMask(
		EAutomationTestFlags Flags,
		TArray<FString>& OutBeautifiedNames,
		TArray<FString>& OutCommands) const;

#if WITH_DEV_AUTOMATION_TESTS
	bool ExecuteBridgeCommandForTesting(
		EAutomationTestFlags Flags,
		const FString& Command,
		TArray<FAutomationExecutionEntry>& OutEntries);
	void EnterSectionForTesting(const FString& Section);
	void LeaveSectionForTesting(const FString& Section);
	bool HasActiveSessionForTesting() const;
	uint64 GetActiveSessionGenerationForTesting() const;
#endif

private:
	class FBridge;

	struct FSectionBinding
	{
		FString Section;
		FDelegateHandle EnterHandle;
		FDelegateHandle LeaveHandle;
	};

	struct FSuiteSession
	{
		FAngelscriptScriptTestId Id;
		uint64 Generation = 0;
		TStrongObjectPtr<UAngelscriptTestSuite> Instance;
		TUniquePtr<FAutomationTestBase> LifecycleResult;
		FAngelscriptEngine* OwningEngine = nullptr;
		bool bSetupFailed = false;
	};

	void EnsureSectionBindings(
		const FAngelscriptScriptTestRegistrySnapshot& Snapshot);
	void EnterSection(const FString& Section);
	void LeaveSection(const FString& Section);
	void CloseActiveSection();
	void AppendSuiteSetupDiagnostics(
		FAutomationTestBase& Result) const;
	bool IsSuiteSetupFailed(
		const FAngelscriptScriptTestId& Id,
		uint64 Generation) const;

	bool bStarted = false;
	TMap<uint64, TSharedPtr<FBridge>> Bridges;
	TMap<FString, FSectionBinding> SectionBindings;
	TOptional<FSuiteSession> ActiveSession;
};
